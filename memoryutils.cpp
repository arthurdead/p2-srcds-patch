#include "memoryutils.h"
#include <Windows.h>
#include <stdio.h>

static size_t UTIL_DecodeHexString(unsigned char *buffer, size_t maxlength, char *hexstr)
{
	size_t written{0};
	size_t length{strlen(hexstr)};

	for(size_t i{0}; i < length; i++)
	{
		if(written >= maxlength)
			break;

		buffer[written++] = hexstr[i];

		if(hexstr[i] == '\\' && hexstr[i + 1] == 'x')
		{
			if(i + 3 >= length)
				continue;

			char s_byte[3]{
				hexstr[i + 2],
				hexstr[i + 3],
				'\0',
			};
			int r_byte{0};
			sscanf_s(s_byte, "%x", &r_byte);
			buffer[written - 1] = static_cast<unsigned char>(r_byte);
			i += 3;
		}
	}

	return written;
}

struct DynLibInfo final
{
	public:
		void *baseAddress{nullptr};
		size_t memorySize{0};
};

static bool GetLibraryInfo(void *libPtr, DynLibInfo &lib)
{
	if(!libPtr)
		return false;

	MEMORY_BASIC_INFORMATION info{};
	if(!VirtualQuery(libPtr, &info, sizeof(MEMORY_BASIC_INFORMATION)))
		return false;

	unsigned long baseAddr{reinterpret_cast<unsigned long>(info.AllocationBase)};

	IMAGE_DOS_HEADER *dos{reinterpret_cast<IMAGE_DOS_HEADER *>(baseAddr)};
	IMAGE_NT_HEADERS *pe{reinterpret_cast<IMAGE_NT_HEADERS *>(baseAddr + dos->e_lfanew)};
	IMAGE_FILE_HEADER *file{&pe->FileHeader};
	IMAGE_OPTIONAL_HEADER *opt{&pe->OptionalHeader};

	if(dos->e_magic != IMAGE_DOS_SIGNATURE || pe->Signature != IMAGE_NT_SIGNATURE || opt->Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		return false;

	#ifdef _WIN64
	if(file->Machine != IMAGE_FILE_MACHINE_AMD64)
		return false;
	#else
	if(file->Machine != IMAGE_FILE_MACHINE_I386)
		return false;
	#endif

	if((file->Characteristics & IMAGE_FILE_DLL) == 0)
		return false;

	lib.memorySize = opt->SizeOfImage;
	lib.baseAddress = reinterpret_cast<void *>(baseAddr);

	return true;
}

static void *FindPattern(void *libPtr, char *pattern, size_t len)
{
	DynLibInfo lib{};
	if(!GetLibraryInfo(libPtr, lib))
		return nullptr;

	char *ptr{reinterpret_cast<char *>(lib.baseAddress)};
	char *end{ptr + lib.memorySize - len};
	bool found{false};
	while(ptr < end) {
		found = true;
		for(size_t i{0}; i < len; i++) {
			if(pattern[i] != '\x2A' && pattern[i] != ptr[i]) {
				found = false;
				break;
			}
		}
		if(found)
			return ptr;
		ptr++;
	}

	return nullptr;
}

void *LookupSignature(void *address, char *signature)
{
	if(signature[0] == '@') {
		MEMORY_BASIC_INFORMATION mem{};
		if(VirtualQuery(address, &mem, sizeof(mem)))
			return GetProcAddress(reinterpret_cast<HMODULE>(mem.AllocationBase), &signature[1]);
	}

	unsigned char sig[511]{'\0'};
	size_t bytes{UTIL_DecodeHexString(sig, ARRAYSIZE(sig), signature)};
	if(bytes >= 1)
		return FindPattern(address, reinterpret_cast<char *>(sig), bytes);

	return nullptr;
}

void *GetFuncAtOffset(void *ptr, int offset)
{
	int relative{*reinterpret_cast<int *>(reinterpret_cast<unsigned long>(ptr) + (offset + 1))};
	return reinterpret_cast<void *>(reinterpret_cast<unsigned long>(ptr) + (offset + 1) + 4 + relative);
}