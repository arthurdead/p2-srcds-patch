#include "filesystem.h"

#define protected public
#include "appframework/IAppSystemGroup.h"
#undef protected

bool CAppSystemGroup::AddSystems( AppSystemInfo_t *pSystemList )
{
	while ( pSystemList->m_pModuleName[0] )
	{
		AppModule_t module = LoadModule( pSystemList->m_pModuleName );
		IAppSystem *pSystem = AddSystem( module, pSystemList->m_pInterfaceName );
		if ( !pSystem )
		{
			Warning( "Unable to load interface %s from %s, requested from EXE.\n", pSystemList->m_pInterfaceName, pSystemList->m_pModuleName );
			return false;
		}
		++pSystemList;
	}

	return true;
}

IAppSystem *CAppSystemGroup::AddSystem( AppModule_t module, const char *pInterfaceName )
{
	if (module == APP_MODULE_INVALID)
		return NULL;

	int nFoundIndex = m_SystemDict.Find( pInterfaceName );
	if ( nFoundIndex != m_SystemDict.InvalidIndex() )
	{
		Warning("AppFramework : Attempted to add two systems with the same interface name %s!\n", pInterfaceName );
		return m_Systems[ m_SystemDict[nFoundIndex] ];
	}

	Assert( (module >= 0) && (module < m_Modules.Count()) );
	CreateInterfaceFn pFactory = m_Modules[module].m_pModule ? Sys_GetFactory( m_Modules[module].m_pModule ) : m_Modules[module].m_Factory;

	int retval;
	void *pSystem = pFactory( pInterfaceName, &retval );
	if ((retval != IFACE_OK) || (!pSystem))
	{
		Warning("AppFramework : Unable to create system %s!\n", pInterfaceName );
		return NULL;
	}

	IAppSystem *pAppSystem = static_cast<IAppSystem*>(pSystem);
	
	int sysIndex = m_Systems.AddToTail( pAppSystem );

	MEM_ALLOC_CREDIT();
	m_SystemDict.Insert( pInterfaceName, sysIndex );
	return pAppSystem;
}

AppModule_t CAppSystemGroup::LoadModule( const char *pDLLName )
{
	int nLen = Q_strlen( pDLLName ) + 1;
	char *pModuleName = (char*)stackalloc( nLen );
	Q_StripExtension( pDLLName, pModuleName, nLen );

	for ( int i = m_Modules.Count(); --i >= 0; ) 
	{
		if ( m_Modules[i].m_pModuleName )
		{
			if ( !Q_stricmp( pModuleName, m_Modules[i].m_pModuleName ) )
				return i;
		}
	}

	CSysModule *pSysModule = LoadModuleDLL( pDLLName );
	if (!pSysModule)
	{
#ifdef _X360
		Warning("AppFramework : Unable to load module %s! (err #%d)\n", pDLLName, GetLastError() );
#else
		Warning("AppFramework : Unable to load module %s!\n", pDLLName );
#endif
		return APP_MODULE_INVALID;
	}

	int nIndex = m_Modules.AddToTail();
	m_Modules[nIndex].m_pModule = pSysModule;
	m_Modules[nIndex].m_Factory = 0;
	m_Modules[nIndex].m_pModuleName = (char*)malloc( nLen );
	Q_strncpy( m_Modules[nIndex].m_pModuleName, pModuleName, nLen );

	return nIndex;
}

void *CAppSystemGroup::FindSystem( const char *pSystemName )
{
	unsigned short i = m_SystemDict.Find( pSystemName );
	if (i != m_SystemDict.InvalidIndex())
		return m_Systems[m_SystemDict[i]];

 	for ( i = 0; i < m_Systems.Count(); ++i )
	{
		void *pInterface = m_Systems[i]->QueryInterface( pSystemName );
		if (pInterface)
			return pInterface;
	}

	int nExternalCount = m_NonAppSystemFactories.Count();
	for ( i = 0; i < nExternalCount; ++i )
	{
		void *pInterface = m_NonAppSystemFactories[i]( pSystemName, NULL );
		if (pInterface)
			return pInterface;
	}

	if ( m_pParentAppSystem )
	{
		void* pInterface = m_pParentAppSystem->FindSystem( pSystemName );
		if ( pInterface )
			return pInterface;
	}

	return NULL;
}