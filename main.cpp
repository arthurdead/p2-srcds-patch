#include <Windows.h>
#include <stdio.h>
#include <pathcch.h>
#include <shlwapi.h>
#include "detours/detours.h"
#include "memoryutils.h"

#include "iserverplugin.h"
#include "materialsystem/imaterialsystem.h"
#include "vphysics_interface.h"
#include "istudiorender.h"
#ifdef DEDICATED_DLL
class CAppSystemGroup;
#include "engine_hlds_api.h"
#endif
#include "filesystem.h"
#include "eiface.h"
#include "game/server/iplayerinfo.h"
#ifdef LAUNCHER_DLL
#include "vgui/IPanel.h"
#include "IVGuiModule.h"
#endif

#define protected public
#include "appframework/IAppSystemGroup.h"
#undef protected

#pragma comment(lib, "Pathcch.lib")
#pragma comment(lib, "Shlwapi.lib")

#if (!defined LAUNCHER_DLL && !defined DEDICATED_DLL)
#error "netiher dll defined"
#endif

#if (defined LAUNCHER_DLL && defined DEDICATED_DLL)
#error "both dll defined"
#endif

const char *GetP2Root()
{
	const char *p2root = CommandLine()->ParmValue("-p2root", "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Portal 2");
	return p2root;
}

void AddBinDir()
{
	wchar_t exefile[MAX_PATH]{L'\0'};
	GetModuleFileNameW(nullptr, exefile, ARRAYSIZE(exefile));

	PathCchRemoveFileSpec(exefile, ARRAYSIZE(exefile));

	wcscat_s(exefile, ARRAYSIZE(exefile), L"\\bin");

	//Msg("[SRCDS-PATCH] Added %S as bin dir\n", exefile);
	AddDllDirectory(exefile);

	const char *p2root = GetP2Root();

	MultiByteToWideChar(CP_UTF8, 0, p2root, strlen(p2root)+1, exefile, ARRAYSIZE(exefile));

	wcscat_s(exefile, ARRAYSIZE(exefile), L"\\bin");

	//Msg("[SRCDS-PATCH] Added %S as bin dir\n", exefile);
	AddDllDirectory(exefile);
}

using Main_t = int (*)(HINSTANCE, HINSTANCE, const char *, int);
int SharedMain(Main_t actualmain, HINSTANCE hInstance, HINSTANCE hPrevInstance, const char *lpCmdLine, int nCmdShow);
bool g_bDedicated = false;

#ifdef LAUNCHER_DLL
EXTERN_C DLL_EXPORT int __cdecl LauncherMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, const char *lpCmdLine, int nCmdShow)
{
	AddBinDir();

	HMODULE actualdll = LoadLibraryExW(L"bin\\launcher_original", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	if(!actualdll) {
		return 42;
	}

	Main_t actualmain = (Main_t)GetProcAddress(actualdll, "LauncherMain");

	g_bDedicated = false;

	return SharedMain(actualmain, hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
#endif

#ifdef DEDICATED_DLL
EXTERN_C DLL_EXPORT int __cdecl DedicatedMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, const char *lpCmdLine, int nCmdShow)
{
	AddBinDir();

	HMODULE actualdll = LoadLibraryExW(L"bin\\dedicated_original", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	if(!actualdll) {
		return 42;
	}

	Main_t actualmain = (Main_t)GetProcAddress(actualdll, "DedicatedMain");

	g_bDedicated = true;

	return SharedMain(actualmain, hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
#endif

HANDLE dllInstance = NULL;
BOOL WINAPI DllMain(HANDLE hInst, ULONG ulInit, LPVOID lpReserved)
{
	dllInstance = hInst;

	if(DetourIsHelperProcess()) {
		return TRUE;
	}

	if(ulInit == DLL_PROCESS_ATTACH) {
		DetourRestoreAfterWith();
	}

	return TRUE;
}

bool PatchFuncs(bool dedicated);

int SharedMain(Main_t actualmain, HINSTANCE hInstance, HINSTANCE hPrevInstance, const char *lpCmdLine, int nCmdShow)
{
	LoadLibraryExW(L"engine", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"inputsystem", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"materialsystem", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"studiorender", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"soundemittersystem", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"vphysics", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"vgui2", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"vscript", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"datacache", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"localize", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"steam", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"filesystem_stdio", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	LoadLibraryExW(L"scenefilecache", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

	//if(!g_bDedicated) {
	#ifdef LAUNCHER_DLL
		LoadLibraryExW(L"valve_avi", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		LoadLibraryExW(L"vguimatsurface", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		LoadLibraryExW(L"shaderapidx9", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		LoadLibraryExW(L"serverbrowser", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	#endif
	//} else {
	#ifdef DEDICATED_DLL
		LoadLibraryExW(L"shaderapiempty", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	#endif
	//}

	if(!PatchFuncs(g_bDedicated)) {
		return 1;
	}

	CommandLine()->CreateCmdLine(lpCmdLine);

	return actualmain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

template <typename T>
void *to_void(T arg)
{
	union
	{
		T u1;
		void *u2;
	};
	u1 = arg;
	return u2;
}

template <typename T>
T to_member(void *arg)
{
	union
	{
		T u1;
		void *u2;
	};
	u2 = arg;
	return u1;
}

#ifdef DEDICATED_DLL
using Error_t = decltype(Error) *;
Error_t TrueError = Error;

void FakeError(const tchar *pMsgFormat, ...)
{
	tchar msg[2048]{'\0'};

	va_list args{nullptr};
	va_start(args, pMsgFormat);
	int len = vsnprintf_s(msg, ARRAYSIZE(msg), _TRUNCATE, pMsgFormat, args);
	va_end(args);

	msg[len] = u'\n';

	Warning(msg);
}

class CSteam3Client
{
public:
	void Activate();
	using Activate_t = decltype(&CSteam3Client::Activate);

	void RunFrame();
	using RunFrame_t = decltype(&CSteam3Client::RunFrame);

	static Activate_t s_ActivateFunc;
	static RunFrame_t s_RunFrameFunc;
};

CSteam3Client::Activate_t CSteam3Client::s_ActivateFunc = nullptr;
CSteam3Client::RunFrame_t CSteam3Client::s_RunFrameFunc = nullptr;

using Steam3Client_t = CSteam3Client &(*)();
Steam3Client_t RealSteam3Client = nullptr;
CSteam3Client &Steam3Client()
{
	return RealSteam3Client();
}

void CSteam3Client::Activate()
{
	(this->*s_ActivateFunc)();
}

void CSteam3Client::RunFrame()
{
	(this->*s_RunFrameFunc)();
}

using _Host_RunFrame_t = void (*)(float);
_Host_RunFrame_t Real_Host_RunFrame = nullptr;
void Fake_Host_RunFrame(float time)
{
	Steam3Client().RunFrame();
	Real_Host_RunFrame(time);
}

void *FileSystemFactory(const char *pName, int *pReturnCode)
{
	if(pReturnCode)
		*pReturnCode = IFACE_OK;
	return g_pFullFileSystem;
}

void MountDependencies(int iAppId, CUtlVector<unsigned int> &depList)
{

}

class CSys
{
public:
	bool LoadModules(CAppSystemGroup *pAppSystemGroup);
	using LoadModules_t = decltype(&CSys::LoadModules);

	static LoadModules_t Real_LoadModules;
};

CSys::LoadModules_t CSys::Real_LoadModules = nullptr;

bool CSys::LoadModules(CAppSystemGroup *pAppSystemGroup)
{
	bool res{(this->*Real_LoadModules)(pAppSystemGroup)};
	if(!res)
		return false;

	AppSystemInfo_t appSystems[]{
		{"inputsystem.dll", "InputStackSystemVersion001"},
		{"scenefilecache.dll", "SceneFileCache002"},
		{"datacache.dll", "MDLCache004"},

		{"", ""},
	};

	if(!pAppSystemGroup->AddSystems(appSystems))
		return false;

	return res;
}
#endif

using GetModuleFileNameA_t = decltype(GetModuleFileNameA) *;
GetModuleFileNameA_t TrueGetModuleFileNameA = GetModuleFileNameA;

DWORD FakeGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
	DWORD len = TrueGetModuleFileNameA(hModule, lpFilename, nSize);
	return len;
}

class CCommandLine
{
public:
	void CreateCmdLine(const char *commandline);
	using CreateCmdLine_t = decltype(&CCommandLine::CreateCmdLine);

	static CreateCmdLine_t Real_CreateCmdLine;
};

CCommandLine::CreateCmdLine_t CCommandLine::Real_CreateCmdLine = nullptr;

void CCommandLine::CreateCmdLine(const char *commandline)
{
	//erase "\"-"
	(this->*Real_CreateCmdLine)(commandline);
}

class CServerPlugin
{
public:
	void LoadPlugins();
	using LoadPlugins_t = decltype(&CServerPlugin::LoadPlugins);

	static LoadPlugins_t Real_LoadPlugins;
};

CServerPlugin::LoadPlugins_t CServerPlugin::Real_LoadPlugins = nullptr;

void CServerPlugin::LoadPlugins()
{
	(this->*Real_LoadPlugins)();

	char dllname[MAX_PATH]{'\0'};
	GetModuleFileNameA((HMODULE)dllInstance, dllname, ARRAYSIZE(dllname));

	CreateInterfaceFn engienfac = Sys_GetFactory("engine");
	void *tmp = LookupSignature(engienfac, "\x55\x8B\xEC\x53\x56\x57\x68\x90\x00\x00\x00");
	using LoadPlugin_t = bool (CServerPlugin::*)(const char *);
	(this->*to_member<LoadPlugin_t>(tmp))(dllname);
}

template <typename T1, typename T2>
void HookFunc(T1 real, T2 fake)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	void *ptr1 = to_void(real);
	void *ptr2 = to_void(fake);
	DetourAttach(&(PVOID &)ptr1, ptr2);
	DetourTransactionCommit();
}

template <typename T1, typename T2, typename T3>
void HookFunc2(T1 real, T2 fake, T3 &realreal)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	void *ptr1 = to_void(real);
	void *ptr2 = to_void(fake);
	void *ptr3 = to_void(realreal);
	DetourAttachEx(&(PVOID &)ptr1, ptr2, &(PDETOUR_TRAMPOLINE &)ptr3, nullptr, nullptr);
	realreal = to_member<T3>(ptr3);
	DetourTransactionCommit();
}

bool PatchFuncs(bool dedicated)
{
	g_bDedicated = dedicated;

	//HookFunc2(TrueGetModuleFileNameA, FakeGetModuleFileNameA, TrueGetModuleFileNameA);

	CreateInterfaceFn engienfac = Sys_GetFactory("engine");
	void *tmp = NULL;
	//if(dedicated) {
#ifdef DEDICATED_DLL
		CreateInterfaceFn dedifac = Sys_GetFactory("dedicated_original");

		tmp = LookupSignature(engienfac, "\x55\x8B\xEC\x83\xEC\x40\x8B\x0D\x2A\x2A\x2A\x2A");
		Real_Host_RunFrame = (_Host_RunFrame_t)tmp;
		HookFunc2(tmp, Fake_Host_RunFrame, Real_Host_RunFrame);

		HookFunc(LookupSignature(dedifac, "\x55\x8B\xEC\x56\xFF\x15\x2A\x2A\x2A\x2A"), FileSystemFactory);
		HookFunc(LookupSignature(dedifac, "\x55\x8B\xEC\xB8\x60\x22\x00\x00"), MountDependencies);

		tmp = LookupSignature(dedifac, "\x55\x8B\xEC\x83\xEC\x68\xB8\x2A\x2A\x2A\x2A");
		CSys::Real_LoadModules = (CSys::LoadModules_t)to_member<CSys::LoadModules_t>(tmp);
		HookFunc2(tmp, &CSys::LoadModules, CSys::Real_LoadModules);

		HookFunc(TrueError, FakeError);

		void *_Host_RunFrame_Client = LookupSignature(engienfac, "\x55\x8B\xEC\xFF\x15\x2A\x2A\x2A\x2A\xDD\x1D\x2A\x2A\x2A\x2A");
		void *SetStartupInfo = LookupSignature(engienfac, "\x55\x8B\xEC\x51\x56\x57\x8B\x3D\x2A\x2A\x2A\x2A\x8B\xF1");

		RealSteam3Client = (Steam3Client_t)GetFuncAtOffset(SetStartupInfo, 0xB5);
		CSteam3Client::s_ActivateFunc = (CSteam3Client::Activate_t)to_member<CSteam3Client::Activate_t>(GetFuncAtOffset(SetStartupInfo, 0xBC));
		CSteam3Client::s_RunFrameFunc = (CSteam3Client::RunFrame_t)to_member<CSteam3Client::RunFrame_t>(GetFuncAtOffset(_Host_RunFrame_Client, 0x5F));
#endif
	//}

	//tmp = nullptr;
	//CCommandLine::Real_CreateCmdLine = (CCommandLine::CreateCmdLine_t)to_member<CSys::CreateCmdLine_t>(tmp);
	//HookFunc2(tmp, &CCommandLine::CreateCmdLine, CCommandLine::Real_CreateCmdLine);

	CreateInterfaceFn filesys = Sys_GetFactory("filesystem_stdio");

	g_pFullFileSystem = (IFileSystem *)filesys(FILESYSTEM_INTERFACE_VERSION, NULL);

	//if(dedicated) {
#ifdef DEDICATED_DLL
		Steam3Client().Activate();

		HMODULE dll = GetModuleHandleW(L"engine");
		char base_dir[MAX_PATH]{'\0'};
		GetModuleFileNameA(dll, base_dir, ARRAYSIZE(base_dir));
		PathRemoveFileSpecA(base_dir);
		PathRemoveFileSpecA(base_dir);
		strcat_s(base_dir, ARRAYSIZE(base_dir), "\\");

		char game_dir[MAX_PATH]{'\0'};
		strcpy_s(game_dir, ARRAYSIZE(game_dir), base_dir);
		strcat_s(game_dir, ARRAYSIZE(game_dir), "portal2");

		char mod_dir[MAX_PATH]{'\0'};
		const char *game = CommandLine()->ParmValue("-game", "portal2");
		if(V_IsAbsolutePath(game)) {
			strcpy_s(mod_dir, ARRAYSIZE(mod_dir), game);
		} else {
			strcpy_s(mod_dir, ARRAYSIZE(mod_dir), base_dir);
			strcat_s(mod_dir, ARRAYSIZE(mod_dir), game);
		}

		//Msg("[SRCDS-PATCH] Added %s to GAME searchpath\n", game_dir);
		g_pFullFileSystem->AddSearchPath(game_dir, "GAME", PATH_ADD_TO_TAIL);

		//Msg("[SRCDS-PATCH] Added %s to GAME searchpath\n", mod_dir);
		g_pFullFileSystem->AddSearchPath(mod_dir, "GAME", PATH_ADD_TO_TAIL);
#endif
	//}

	tmp = LookupSignature(engienfac, "\x55\x8B\xEC\x51\x56\x8B\xF1\x57\x8D\x4E\x04");
	CServerPlugin::Real_LoadPlugins = (CServerPlugin::LoadPlugins_t)to_member<CServerPlugin::LoadPlugins_t>(tmp);
	HookFunc2(tmp, &CServerPlugin::LoadPlugins, CServerPlugin::Real_LoadPlugins);

	return true;
}

class CEmptyPlugin : public IServerPluginCallbacks
{
public:
	bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override;
	void Unload() override {}
	void Pause() override {}
	void UnPause() override {}
	const char *GetPluginDescription() override { return ""; }
	void LevelInit(const char *pMapName) override {}
	void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax) override {}
	void GameFrame(bool simulating) override;
	void LevelShutdown() override {}
	void ClientActive(edict_t *pEntity) override {}
	void ClientFullyConnect(edict_t *pEntity) override {}
	void ClientDisconnect(edict_t *pEntity) override {}
	void ClientPutInServer(edict_t *pEntity, const char *playername) override {}
	void SetCommandClient(int index) override {}
	void ClientSettingsChanged(edict_t *pEdict) override {}
	PLUGIN_RESULT ClientConnect(bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen) override { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT ClientCommand(edict_t *pEntity, const CCommand &args) override { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT NetworkIDValidated(const char *pszUserName, const char *pszNetworkID) override { return PLUGIN_CONTINUE; }
	void OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue) override {}
	void OnEdictAllocated(edict_t *edict) override {}
	void OnEdictFreed(const edict_t *edict) override {}
};

ConVar *mp_dev_wait_for_other_player = nullptr;
ConVar *sv_portal_players = nullptr;
IVEngineServer *engine = NULL;
CGlobalVars *gpGlobals = NULL;

void dev_wait_change(IConVar *var, const char *pOldValue, float flOldValue)
{
	ConVar *convar = (ConVar *)var;
	if(convar->GetBool()) {
		convar->SetValue(false);
	}
}

void portals_players(IConVar *var, const char *pOldValue, float flOldValue)
{
	ConVar *convar = (ConVar *)var;
	int clients = gpGlobals->maxClients;
	if(convar->GetInt() < clients) {
		convar->SetValue(clients);
	}
}

class CServerClients
{
public:
	void GetPlayerLimits(int &min, int &max, int &def)
	{
		min = 3;
		max = 33;
		def = max;
		sv_portal_players->SetValue(max);
	}
};

#ifdef LAUNCHER_DLL
FnCommandCallback_t oldopenserverbrowser{nullptr};
vgui::IPanel *vguipanel = NULL;
IVGuiModule *vguimodule = NULL;
KeyValues *SetCustomScheme = NULL;

void newopenserverbrowser(const CCommand &args)
{
	oldopenserverbrowser(args);

	vgui::VPANEL vpanel = vguimodule->GetPanel();

	vguipanel->SendMessage(vpanel, SetCustomScheme, vpanel);
}
#endif

class CCvar
{
public:
	static FnCommandCallback_t GetCallback(ConCommand *cmd)
	{
		return cmd->m_fnCommandCallback;
	}

	static void SetCallback(ConCommand *cmd, FnCommandCallback_t callback)
	{
		cmd->m_fnCommandCallback = callback;
	}
};

FnCommandCallback_t oldmaxplayers{nullptr};

void newmaxplayers(const CCommand &args)
{
	int clients = -1;

	if(args.ArgC () >= 2) {
		clients = Q_atoi(args[1]);
		if(clients <= 3) {
			return;
		}
	}

	if(clients != -1) {
		sv_portal_players->SetValue(clients);
	}

	oldmaxplayers(args);
}

ConVar *mp_gamemode = NULL;
ConCommand *retry = NULL;

using CL_Retry_t = void (*)();
CL_Retry_t CL_Retry = NULL;

void newretry(const CCommand &args)
{
	CL_Retry();
}

bool CEmptyPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	ConnectTier1Libraries( &interfaceFactory, 1 );

	engine = (IVEngineServer*)interfaceFactory(INTERFACEVERSION_VENGINESERVER, NULL);

	IPlayerInfoManager *playerinfomanager = (IPlayerInfoManager *)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER,NULL);
	gpGlobals = playerinfomanager->GetGlobalVars();

	mp_gamemode = new ConVar("mp_gamemode", "coop", 
		FCVAR_REPLICATED|FCVAR_DEVELOPMENTONLY|
	#ifdef LAUNCHER_DLL
		FCVAR_CLIENTDLL
	#else
		FCVAR_GAMEDLL
	#endif
		, "Current game mode, acceptable values are coop, realism, versus, survival, scavenge and holdout; changed using map command, eg: map mapname versus"
	);

	retry = new ConCommand("retry", newretry, "Retry connection to last server.", FCVAR_DONTRECORD | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENTCMD_CAN_EXECUTE);

	ConVar_Register( 0 );

	CommandLine()->AppendParm("-allowspectators", "");

	mp_dev_wait_for_other_player = g_pCVar->FindVar("mp_dev_wait_for_other_player");
	mp_dev_wait_for_other_player->InstallChangeCallback(dev_wait_change);
	mp_dev_wait_for_other_player->SetValue(false);

	sv_portal_players = g_pCVar->FindVar("sv_portal_players");
	sv_portal_players->InstallChangeCallback(portals_players);
	sv_portal_players->SetValue(33);

	IServerGameClients *gameclients = (IServerGameClients *)gameServerFactory(INTERFACEVERSION_SERVERGAMECLIENTS,NULL);
	void *tmp = (*(void ***)gameclients)[0];
	HookFunc(tmp, &CServerClients::GetPlayerLimits);

	tmp = LookupSignature(interfaceFactory, "\x55\x8B\xEC\x8B\x45\x08\x83\xEC\x08\x83\x38\x02");
	unsigned char *buffer = (unsigned char *)tmp;
	unsigned long old;
	VirtualProtect(buffer, 4, PAGE_EXECUTE_READWRITE, &old);
	for(int i = 0xBC; i <= 0xD6; i++) {
		buffer[i] = 0x90;
	}

	tmp = LookupSignature(interfaceFactory, "\x55\x8B\xEC\x83\xEC\x14\x57\xE8\x64\x4E\x00\x00\x2A\x2A\x2A\x2A\x2A");
	CL_Retry = (CL_Retry_t)tmp;

	ConCommand *maxplayers = g_pCVar->FindCommand("maxplayers");
	oldmaxplayers = CCvar::GetCallback(maxplayers);
	CCvar::SetCallback(maxplayers, newmaxplayers);

	//if(!engine->IsDedicatedServer()) {
	#ifdef LAUNCHER_DLL
		CreateInterfaceFn browserfac = Sys_GetFactory("serverbrowser");
		CreateInterfaceFn vgui2fac = Sys_GetFactory("vgui2");

		vguimodule = (IVGuiModule *)browserfac("VGuiModuleServerBrowser001", NULL);
		vguipanel = (vgui::IPanel *)vgui2fac(VGUI_PANEL_INTERFACE_VERSION, NULL);

		SetCustomScheme = new KeyValues("SetCustomScheme");
		SetCustomScheme->SetString("SchemeName", "SourceScheme");

		ConCommand *openserverbrowser = g_pCVar->FindCommand("openserverbrowser");
		oldopenserverbrowser = CCvar::GetCallback(openserverbrowser);
		CCvar::SetCallback(openserverbrowser, newopenserverbrowser);
	#endif
	//}

	return true;
}

void CEmptyPlugin::GameFrame(bool simulating)
{
	
}

CEmptyPlugin g_EmptyPlugin{};
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEmptyPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_EmptyPlugin);