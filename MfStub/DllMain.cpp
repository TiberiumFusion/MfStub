// This proxy is intended for and specifically made to be used with WMP 11 on Windows 7, but it may work for other programs which use Media Foundation.
// Since different versions of Windows have different versions of mf.dll with different exports that need forwarding, each built DLL is platform-specific.

#if _DEBUG
#define _BREAK_AT_RUNTIME 1
#endif


// Windows version
#if MFSTUB_TARGET_NT60
	#define WINVER 0x0600
	#define _WIN32_WINNT 0x0600
#elif MFSTUB_TARGET_NT61
	#define WINVER 0x0601
	#define _WIN32_WINNT 0x0601
#elif MFSTUB_TARGET_NT63
	#define WINVER 0x0603
	#define _WIN32_WINNT 0x0603
#elif MFSTUB_TARGET_NTBC
	#define WINVER 0x0A00
	#define _WIN32_WINNT 0x0A00
#endif

#include <Windows.h>
#include <intsafe.h>
#include <strsafe.h>

// MF headers
#if MFSTUB_TARGET_NT60
	#include "mfidl_60.h"
#elif MFSTUB_TARGET_NT61
	#include "mfidl_61.h"
#elif MFSTUB_TARGET_NT63
	#include "mfidl_63.h"
#elif MFSTUB_TARGET_NTBC
	#include "mfidl_BC.h"
#endif

#include "ini.h"

#include "resource.h"



// ____________________________________________________________________________________________________
// 
//     Helpers
// ____________________________________________________________________________________________________
//

#define STREQ(a, b) (strcmp(a, b) == 0)

static HRESULT mbstr2wcstr(LPCSTR in, LPWSTR* out)
{
	int bufSize = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
	if (bufSize == 0)
		return E_FAIL;
	
	LPWSTR string = (LPWSTR)malloc(bufSize);
	MultiByteToWideChar(CP_UTF8, 0, in, -1, string, bufSize);
	if (bufSize == 0)
		return E_FAIL;

	*out = string;

	return S_OK;
}



// ____________________________________________________________________________________________________
// 
//     Entry
// ____________________________________________________________________________________________________
//

HINSTANCE DllInstance = NULL;

BOOL ProcessAttach();
BOOL ProcessDetach();

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD dwReason, PVOID pvReserved)
{
    UNREFERENCED_PARAMETER(pvReserved);

    switch (dwReason)
    {
		case DLL_PROCESS_ATTACH:
			DllInstance = hInstDLL;
			return ProcessAttach();

		case DLL_PROCESS_DETACH:
			return ProcessDetach();
    }

    return TRUE;
}



// ____________________________________________________________________________________________________
// 
//     Config
// ____________________________________________________________________________________________________
//

LPCSTR DefaultConfigIniText = NULL;
static HRESULT LoadDefaultIniConfigDocumentContent()
{
	if (DllInstance == NULL)
		return E_FAIL;

	HRSRC res = FindResource(DllInstance, MAKEINTRESOURCE(INI_CONFIG_DEFAULT_DOCUMENT), RT_RCDATA);
	if (res == NULL)
		return E_FAIL;

	HGLOBAL resHandle = LoadResource(DllInstance, res);
	if (resHandle == NULL)
		return E_FAIL;

	DWORD resDataLen = SizeofResource(DllInstance, res);
	if (resDataLen == 0)
		return E_FAIL;

	DefaultConfigIniText = (LPCSTR)LockResource(resHandle);
	if (DefaultConfigIniText == NULL)
		return E_FAIL;

	return S_OK;
}


//
// On-disk file
//

LPCWSTR ConfigPath = L"mfstub.ini";
FILETIME LastConfigReadFileWriteTime = { 0, 0 };

HRESULT GetFullConfigFilePath(LPWSTR path, DWORD pathLen)
{
	if (DllInstance == NULL)
		return E_FAIL;

	// Look for the config file next to ourself
	DWORD moduleNameReadChars = GetModuleFileNameW(DllInstance, path, pathLen); // e.g. "C:\Program Files\Windows Media Player\mf.dll"
	if (moduleNameReadChars == 0)
		return E_FAIL;

	DWORD p = moduleNameReadChars - 1;
	while (p > 1)
	{
		if (path[p] == L'\\')
		{
			path[p + 1] = L'\0'; // chop off file name to leave directory plus trailing slash, e.g. "C:\Program Files\Windows Media Player\"
			break;
		}
		p--;
	}

	if (FAILED( StringCchCatW(path, pathLen, ConfigPath) )) // e.g. "C:\Program Files\Windows Media Player\mfstub.ini"
		return E_FAIL;

	return S_OK;
}

HANDLE GetConfigFileReadHandle()
{
	WCHAR configFullPath[MAX_PATH];
	if (FAILED( GetFullConfigFilePath(configFullPath, MAX_PATH) ))
		return NULL;

	HANDLE hConfigFile = CreateFileW(configFullPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hConfigFile == INVALID_HANDLE_VALUE)
		return NULL;
	
	return hConfigFile;
}

HRESULT GetConfigFileLastModifiedTime(HANDLE hConfigFile, LPFILETIME filetime)
{
	bool localHandle = false;
	if (hConfigFile == NULL)
	{
		HANDLE hConfigFile = GetConfigFileReadHandle();
		if (hConfigFile == NULL)
			return E_FAIL;

		localHandle = true;
	}

	if (GetFileTime(hConfigFile, NULL, NULL, filetime) == 0)
	{
		if (localHandle)
			CloseHandle(hConfigFile);
		return E_FAIL;
	}

	if (localHandle)
		CloseHandle(hConfigFile);

	return S_OK;
}


//
// Parsed config options (with defaults here matching default on-disk config file)
//

enum MfStubConfig_PMPInitModeEnum
{
	MfStubConfig_PMPInitMode_Default, // implicit value for falling through to the default
	MfStubConfig_PMPInitMode_UnprotectedProcessPMP, // default
	MfStubConfig_PMPInitMode_ForceNonPMP,
	MfStubConfig_PMPInitMode_Count
};

struct MfStubConfig
{
	// Main
	bool Passthrough;
	LPWSTR OriginalMfDllName;
	MfStubConfig_PMPInitModeEnum PMPInitMode;
	bool WatchConfigFile;

	// Debug
	bool AlertSessionCreation;
};

MfStubConfig _MfStubConfig;

void ApplyDefaultConfigValues()
{	
	_MfStubConfig.Passthrough = false; // value matches default config document
	_MfStubConfig.OriginalMfDllName = L"m_.dll"; // ditto
	_MfStubConfig.PMPInitMode = MfStubConfig_PMPInitMode_UnprotectedProcessPMP; // etc
	_MfStubConfig.WatchConfigFile = false;
	_MfStubConfig.AlertSessionCreation = false;
}


//
// Init and loader
//

static int ParseConfigIniItem(void* user, LPCSTR section, LPCSTR name, LPCSTR value)
{
	#define IS_TRUE_BOOL(v) (_stricmp(v, "true") == 0 || _stricmp(v, "1") == 0)
	#define IS_NONEMPTY_STRING(v) (v != NULL && strlen(v) > 0)

	if (STREQ(section, "Main"))
	{
		if (STREQ(name, "Passthrough"))
		{
			_MfStubConfig.Passthrough = IS_TRUE_BOOL(value);
			return 1; // indicates success?
		}
		else if (STREQ(name, "OriginalMfDllName") && IS_NONEMPTY_STRING(value))
		{
			if (mbstr2wcstr(value, &_MfStubConfig.OriginalMfDllName))
				return 1;
		}
		else if (STREQ(name, "PMPInitMode") && IS_NONEMPTY_STRING(value))
		{
			if (STREQ(value, "ForceNonPMP"))
				_MfStubConfig.PMPInitMode = MfStubConfig_PMPInitMode_ForceNonPMP;
			else if (STREQ(value, "UnprotectedProcessPMP"))
				_MfStubConfig.PMPInitMode = MfStubConfig_PMPInitMode_UnprotectedProcessPMP;
			else
				return 0; // indicates non-parsed entry?

			return 1;
		}
		else if (STREQ(name, "WatchConfigFile"))
		{
			_MfStubConfig.WatchConfigFile = IS_TRUE_BOOL(value);
			return 1;
		}
	}

	else if (STREQ(section, "Debug"))
	{
		if (STREQ(name, "AlertSessionCreation"))
		{
			_MfStubConfig.AlertSessionCreation = IS_TRUE_BOOL(value);
			return 1;
		}
	}

	return 0;
}

HRESULT UpdateConfigFromFile(HANDLE hConfigFile)
{
	// Update tracked write time
	GetConfigFileLastModifiedTime(hConfigFile, &LastConfigReadFileWriteTime);

	// Stat file size
	LARGE_INTEGER configFileSizeStat;
	if (GetFileSizeEx(hConfigFile, &configFileSizeStat) == 0)
		return E_FAIL;
	if (configFileSizeStat.QuadPart <= 0 || configFileSizeStat.QuadPart > 16 * 1024) // abort in case of a potentially dangerously large file, since a valid config should fit well within 16KB
		return E_ABORT;

	// Read all bytes
	LONGLONG configFileSize = configFileSizeStat.QuadPart + 1ll;
	size_t configFileSizeSizeT;
	if (FAILED( LongLongToSizeT(configFileSize, &configFileSizeSizeT) ))
		return E_FAIL;

	LPSTR configFileBuffer = (LPSTR)malloc(configFileSizeSizeT);
	if (configFileBuffer == NULL)
		return E_FAIL;

	DWORD bytesRead;
	if (!ReadFile(hConfigFile, configFileBuffer, configFileSize, &bytesRead, NULL))
	{
		free(configFileBuffer);
		return E_FAIL;
	}
	if (bytesRead == 0)
	{
		free(configFileBuffer);
		return E_FAIL;
	}

	configFileBuffer[configFileSize - 1] = '\0'; // ensure null terminator

	// Parse config
	BOOL configParseSuccess = (ini_parse_string(configFileBuffer, ParseConfigIniItem, NULL) < 0);
	free(configFileBuffer);

	if (!configParseSuccess)
		return E_FAIL;

	return S_OK;
}

HRESULT LoadOrInitConfig()
{
	// Always init with sane config defaults first
	ApplyDefaultConfigValues();

	// Then try to load user config from disk
	WCHAR configFullPath[MAX_PATH];
	if (FAILED( GetFullConfigFilePath(configFullPath, MAX_PATH) ))
		return E_FAIL;

	bool fileExists = false;
	HANDLE hConfigFile = CreateFileW(configFullPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hConfigFile == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_ACCESS_DENIED)
		{
			// Since the config path will most likely be inside WMP's program files folder, this failure will be a common occurrence due to wmplayer.exe typically not running as an admin
			// In which we'll try again with a read-only handle instead
			hConfigFile = CreateFileW(configFullPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hConfigFile == INVALID_HANDLE_VALUE)
				return E_FAIL;

			fileExists = true;
		}
		else
			return E_FAIL;
	}
	else
		fileExists = (GetLastError() == ERROR_ALREADY_EXISTS);

	HRESULT finalHr = S_OK;
	if (fileExists)
	{
		// Read in the config from the disk
		if (FAILED( UpdateConfigFromFile(hConfigFile)) )
			finalHr = E_FAIL;
	}
	else
	{
		// Try to deploy a default config file
		if (DefaultConfigIniText == NULL)
		{
			if (SUCCEEDED( LoadDefaultIniConfigDocumentContent() ))
			{
				DWORD numBytesWritten;
				WriteFile(hConfigFile, DefaultConfigIniText, strlen(DefaultConfigIniText), &numBytesWritten, NULL);
			}
		}
	}
	
	CloseHandle(hConfigFile);

	return finalHr;
}

HRESULT UpdateConfigFromFileIfModifiedOnDisk()
{
	HANDLE hConfigFile = GetConfigFileReadHandle();
	if (hConfigFile == NULL)
		return E_FAIL;

	FILETIME writeTime;
	if (FAILED( GetConfigFileLastModifiedTime(hConfigFile, &writeTime) ))
	{
		CloseHandle(hConfigFile);
		return E_FAIL;
	}

	if (   (LastConfigReadFileWriteTime.dwLowDateTime == 0 && LastConfigReadFileWriteTime.dwHighDateTime == 0)
		|| CompareFileTime(&writeTime, &LastConfigReadFileWriteTime) == 1) // file was written to since we last read it
	{
		// Update our config values from the file (LastConfigReadFileWriteTime will be updated as well)
		if (FAILED( UpdateConfigFromFile(hConfigFile) ))
		{
			CloseHandle(hConfigFile);
			return E_FAIL;
		}
	}

	CloseHandle(hConfigFile);
	return S_OK;
}



// ____________________________________________________________________________________________________
// 
//     Main
// ____________________________________________________________________________________________________
//

#define C_DLL_EXPORT             extern "C" __declspec(dllexport)
#define CALLING_CONVENTION       __stdcall


LPCWSTR AlertDialogTitle = L"mf.dll Stub";

bool AlreadyShownMfDllMissingMessageBox = false;


//
// Original methods
//

typedef HRESULT (__stdcall *MFCreateMediaSession_type)(IMFAttributes* pConfiguration, __out IMFMediaSession** ppMediaSession);
typedef HRESULT (__stdcall *MFCreatePMPMediaSession_type)(DWORD dwCreationFlags, IMFAttributes* pConfiguration, __out IMFMediaSession** ppMediaSession, __out_opt IMFActivate** ppEnablerActivate);


//
// Rewrites
//

// Unnecessary
// No need to rewrite the non-PMP session init method
/*
C_DLL_EXPORT HRESULT __stdcall MFCreateMediaSession(IMFAttributes* pConfiguration, __out IMFMediaSession** ppMediaSession)
{
	//return E_FAIL; // crashes wmp
	//return S_OK; // crashes wmp

	//DebugBreak();

	HMODULE hModuleOriginalMfDll = GetModuleHandle(Config_OriginalMfDllName);
	MFCreateMediaSession_type orig_MFCreateMediaSession = (MFCreateMediaSession_type)GetProcAddress(hModuleOriginalMfDll, "MFCreateMediaSession");

	if (orig_MFCreateMediaSession)
        return orig_MFCreateMediaSession(pConfiguration, ppMediaSession);
    else
        return E_FAIL;
}
*/

// In my experience, WMP 11 always uses protected sessions to play MP3 and WMA files. It seems that PMP is always used regardless of the user's MF codec preference for the file type (e.g. Microsoft decoder, LAV Filters decoder, etc).
// On the other hand, WMP 11 does NOT use protected sessions to play FLAC files. Presumably this is also true for other media types it does not understand out of the box (which require a third party MF codec or DirectShow codec to be installed).
// My best theory to explain this is that WMP 11 only wants PMP on file types for which a Microsoft MF codec exists - regardless of the user's codec preferences for that file type. All other file types get non-PMP sessions.
C_DLL_EXPORT HRESULT __stdcall MFCreatePMPMediaSession(DWORD dwCreationFlags, IMFAttributes* pConfiguration, __out IMFMediaSession** ppMediaSession, __out_opt IMFActivate** ppEnablerActivate)
{
	//return E_FAIL; // crashes wmp
	//return S_OK; // crashes wmp

	#if _BREAK_AT_RUNTIME
	DebugBreak();
	#endif

	// Update config from on-disk file
	if (_MfStubConfig.WatchConfigFile)
	{
		UpdateConfigFromFileIfModifiedOnDisk();
	}

	// Find the original mf.dll module
	// It should be loaded already due to the forwarded imports, so we don't need to LoadLibrary() it
	HMODULE hModuleOriginalMfDll = GetModuleHandle(_MfStubConfig.OriginalMfDllName);
	if (hModuleOriginalMfDll == NULL)
	{
		if (!AlreadyShownMfDllMissingMessageBox)
		{
			// Inform user that the necessasry DLL was not found, instead of failing silently
			LPCWSTR baseMessage = L"The original mf.dll could not be found.\nPath: \"%s\"\n\nIf you are using the mfstub.ini config file, check the \"OriginalMfDllName\" path and ensure it is valid.\n\nBecause the DLL was not found, the application will probably crash now.";
			LPWSTR formattedMessage = (LPWSTR)malloc( lstrlenW(baseMessage) + lstrlenW(_MfStubConfig.OriginalMfDllName) );
			wsprintf(formattedMessage, baseMessage, _MfStubConfig.OriginalMfDllName);

			MessageBoxW(
				NULL,
				formattedMessage,
				L"mf.dll Stub Error",
				MB_ICONERROR
			);
			
			AlreadyShownMfDllMissingMessageBox = true;

			free(formattedMessage);
		}

		return E_FAIL;
	}

	MFCreatePMPMediaSession_type orig_MFCreatePMPMediaSession;
	MFCreateMediaSession_type orig_MFCreateMediaSession;

	// Choose the right method for creating the MF session
	if (_MfStubConfig.Passthrough)
	{
		// Call the original method as-is with the original parameters, as if it had been forwarded
		if (_MfStubConfig.AlertSessionCreation)
			MessageBoxW(NULL, L"Caller wants PMP session\nCreating PMP session as desired (passthrough)", AlertDialogTitle, MB_ICONINFORMATION);

		orig_MFCreatePMPMediaSession = (MFCreatePMPMediaSession_type)GetProcAddress(hModuleOriginalMfDll, "MFCreatePMPMediaSession");

		if (orig_MFCreatePMPMediaSession)
			return orig_MFCreatePMPMediaSession(dwCreationFlags, pConfiguration, ppMediaSession, ppEnablerActivate);
		else
			return E_FAIL;
	}
	else
	{
		switch (_MfStubConfig.PMPInitMode)
		{
			case MfStubConfig_PMPInitMode_ForceNonPMP:
				// Instead of doing what the caller wants (create a PMP session with their desired parameters), we'll create a normal (non-PMP) session instead
				if (_MfStubConfig.AlertSessionCreation)
					MessageBoxW(NULL, L"Caller wants PMP session\nCreating normal session instead", AlertDialogTitle, MB_ICONINFORMATION);

				orig_MFCreateMediaSession = (MFCreateMediaSession_type)GetProcAddress(hModuleOriginalMfDll, "MFCreateMediaSession");

				if (orig_MFCreateMediaSession)
					return orig_MFCreateMediaSession(pConfiguration, ppMediaSession);
				else
					return E_FAIL;

				break;

			case MfStubConfig_PMPInitMode_UnprotectedProcessPMP:
			case MfStubConfig_PMPInitMode_Default:
				// We'll create a PMP session, but we will ensure it is unprotected
				if (_MfStubConfig.AlertSessionCreation)
					MessageBoxW(NULL, L"Caller wants PMP session\nCreating unprotected PMP session", AlertDialogTitle, MB_ICONINFORMATION);

				orig_MFCreatePMPMediaSession = (MFCreatePMPMediaSession_type)GetProcAddress(hModuleOriginalMfDll, "MFCreatePMPMediaSession");

				if (orig_MFCreatePMPMediaSession)
					return orig_MFCreatePMPMediaSession(MFPMPSESSION_UNPROTECTED_PROCESS, pConfiguration, ppMediaSession, ppEnablerActivate);
				else
					return E_FAIL;

				break;
		}
	}
}


//
// Init
//

BOOL ProcessAttach()
{
	#if _BREAK_AT_RUNTIME
	DebugBreak();
	#endif

	LoadOrInitConfig();

    return TRUE;
}

BOOL ProcessDetach()
{
    return TRUE;
}
