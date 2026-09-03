#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <inttypes.h>
#include <stdio.h>
#include <wchar.h>
#include "winxp.h"

// Convert native status values when the optional ntdll helper is available
static DWORD XP_NtStatusToDosError(NTSTATUS status)
{
	static DWORD(WINAPI * pfnRtlNtStatusToDosError)(NTSTATUS) = NULL;
	if (pfnRtlNtStatusToDosError == NULL) {
		HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
		if (hNtDll != NULL)
			pfnRtlNtStatusToDosError = (DWORD(WINAPI*)(NTSTATUS))GetProcAddress(hNtDll, "RtlNtStatusToDosError");
	}
	return (pfnRtlNtStatusToDosError == NULL) ?
		ERROR_GEN_FAILURE : pfnRtlNtStatusToDosError(status);
}

BOOL WINAPI XP_InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
{
	(void)Flags;
	InitializeCriticalSection(lpCriticalSection);
	if (dwSpinCount > 0)
		SetCriticalSectionSpinCount(lpCriticalSection, dwSpinCount);
	return TRUE;
}

int WINAPI XP_CompareStringEx(
	LPCWSTR lpLocaleName, DWORD dwCmpFlags,
	LPCWCH lpString1, int cchCount1,
	LPCWCH lpString2, int cchCount2,
	LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved, LPARAM lParam)
{
	(void)lpLocaleName; (void)lpVersionInformation; (void)lpReserved; (void)lParam;
	return CompareStringW(LOCALE_USER_DEFAULT, dwCmpFlags, lpString1, cchCount1, lpString2, cchCount2);
}

int WINAPI XP_GetLocaleInfoEx(LPCWSTR lpLocaleName, LCTYPE LCType, LPWSTR lpLCData, int cchData)
{
	(void)lpLocaleName;
	return GetLocaleInfoW(LOCALE_USER_DEFAULT, LCType, lpLCData, cchData);
}

int WINAPI XP_LCMapStringEx(
	LPCWSTR lpLocaleName, DWORD dwMapFlags,
	LPCWSTR lpSrcStr, int cchSrc,
	LPWSTR lpDestStr, int cchDest,
	LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved, LPARAM lParam)
{
	(void)lpLocaleName; (void)lpVersionInformation; (void)lpReserved; (void)lParam;
	return LCMapStringW(LOCALE_USER_DEFAULT, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
}

void* __imp_InitializeCriticalSectionEx = (void*)XP_InitializeCriticalSectionEx;
void* __imp_CompareStringEx = (void*)XP_CompareStringEx;
void* __imp_GetLocaleInfoEx = (void*)XP_GetLocaleInfoEx;
void* __imp_LCMapStringEx = (void*)XP_LCMapStringEx;

#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:__imp__InitializeCriticalSectionEx@12=___imp_InitializeCriticalSectionEx")
#pragma comment(linker, "/alternatename:__imp__CompareStringEx@36=___imp_CompareStringEx")
#pragma comment(linker, "/alternatename:__imp__GetLocaleInfoEx@16=___imp_GetLocaleInfoEx")
#pragma comment(linker, "/alternatename:__imp__LCMapStringEx@36=___imp_LCMapStringEx")
#endif

#undef GetTickCount64
#undef GetVolumeInformationByHandleW
#undef GetThreadUILanguage
#undef CancelIoEx
#undef CancelSynchronousIo
#undef CreateSymbolicLinkW
#undef GetFinalPathNameByHandleW
#undef GetProductInfo
#undef LCIDToLocaleName
#undef QueryFullProcessImageNameW
#undef SetDefaultDllDirectories
#undef SHCreateItemFromParsingName
#undef SHGetKnownFolderPath

typedef ULONGLONG(WINAPI* PFN_GetTickCount64)(VOID);

ULONGLONG XP_GetTickCount64(VOID)
{
	static PFN_GetTickCount64 pfnGetTickCount64 = NULL;
	static BOOL resolved = FALSE;
	static volatile LONG fallback_init = 0;
	static CRITICAL_SECTION fallback_lock;
	static DWORD fallback_last = 0;
	static ULONGLONG fallback_high = 0;
	DWORD tick;
	ULONGLONG result;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) {
			pfnGetTickCount64 = (PFN_GetTickCount64)GetProcAddress(hKernel, "GetTickCount64");
		}
		resolved = TRUE;
	}

	if (pfnGetTickCount64) {
		return pfnGetTickCount64();
	}

	if (InterlockedCompareExchange(&fallback_init, 1, 0) == 0) {
		InitializeCriticalSection(&fallback_lock);
		fallback_last = GetTickCount();
		InterlockedExchange(&fallback_init, 2);
	} else {
		while (InterlockedCompareExchange(&fallback_init, 2, 2) != 2)
			Sleep(0);
	}

	EnterCriticalSection(&fallback_lock);
	tick = GetTickCount();
	if (tick < fallback_last)
		fallback_high += (1ULL << 32);
	fallback_last = tick;
	result = fallback_high | tick;
	LeaveCriticalSection(&fallback_lock);

	return result;
}

typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	} DUMMYUNIONNAME;
	ULONG_PTR Information;
} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;

typedef struct _FILE_FS_VOLUME_INFORMATION {
	LARGE_INTEGER VolumeCreationTime;
	ULONG VolumeSerialNumber;
	ULONG VolumeLabelLength;
	BOOLEAN SupportsObjects;
	WCHAR VolumeLabel[1];
} FILE_FS_VOLUME_INFORMATION;

typedef struct _FILE_FS_ATTRIBUTE_INFORMATION {
	ULONG FileSystemAttributes;
	LONG MaximumComponentNameLength;
	ULONG FileSystemNameLength;
	WCHAR FileSystemName[1];
} FILE_FS_ATTRIBUTE_INFORMATION;

typedef BOOL(WINAPI* PFN_GetVolumeInformationByHandleW)(
	HANDLE, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD);

typedef NTSTATUS(WINAPI* PFN_NtQueryVolumeInformationFile)(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FsInformation,
	ULONG Length,
	ULONG FsInformationClass
	);

BOOL XP_GetVolumeInformationByHandleW(
	HANDLE hFile,
	LPWSTR lpVolumeNameBuffer,
	DWORD nVolumeNameSize,
	LPDWORD lpVolumeSerialNumber,
	LPDWORD lpMaximumComponentLength,
	LPDWORD lpFileSystemFlags,
	LPWSTR lpFileSystemNameBuffer,
	DWORD nFileSystemNameSize
)
{
	static PFN_GetVolumeInformationByHandleW pfnGetVolumeInformationByHandleW = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) {
			pfnGetVolumeInformationByHandleW = (PFN_GetVolumeInformationByHandleW)GetProcAddress(hKernel, "GetVolumeInformationByHandleW");
		}
		resolved = TRUE;
	}

	if (pfnGetVolumeInformationByHandleW) {
		return pfnGetVolumeInformationByHandleW(
			hFile, lpVolumeNameBuffer, nVolumeNameSize,
			lpVolumeSerialNumber, lpMaximumComponentLength,
			lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize
		);
	}

	HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
	if (!hNtDll) return FALSE;

	PFN_NtQueryVolumeInformationFile pfnNtQueryVolumeInformationFile =
		(PFN_NtQueryVolumeInformationFile)GetProcAddress(hNtDll, "NtQueryVolumeInformationFile");

	if (!pfnNtQueryVolumeInformationFile) return FALSE;

	IO_STATUS_BLOCK iosb;
	NTSTATUS status;

	if (lpVolumeNameBuffer || lpVolumeSerialNumber) {
		BYTE volInfoBuf[sizeof(FILE_FS_VOLUME_INFORMATION) + MAX_PATH * sizeof(WCHAR)];
		status = pfnNtQueryVolumeInformationFile(hFile, &iosb, volInfoBuf, sizeof(volInfoBuf), 1);

		if (status >= 0) {
			FILE_FS_VOLUME_INFORMATION* pVolInfo = (FILE_FS_VOLUME_INFORMATION*)volInfoBuf;
			if (lpVolumeSerialNumber) {
				*lpVolumeSerialNumber = pVolInfo->VolumeSerialNumber;
			}
			if (lpVolumeNameBuffer && nVolumeNameSize > 0) {
				DWORD charsToCopy = pVolInfo->VolumeLabelLength / sizeof(WCHAR);
				if (charsToCopy >= nVolumeNameSize) charsToCopy = nVolumeNameSize - 1;
				wcsncpy(lpVolumeNameBuffer, pVolInfo->VolumeLabel, charsToCopy);
				lpVolumeNameBuffer[charsToCopy] = L'\0';
			}
		}
		else {
			SetLastError(XP_NtStatusToDosError(status));
			return FALSE;
		}
	}

	if (lpMaximumComponentLength || lpFileSystemFlags || lpFileSystemNameBuffer) {
		BYTE attrInfoBuf[sizeof(FILE_FS_ATTRIBUTE_INFORMATION) + MAX_PATH * sizeof(WCHAR)];
		status = pfnNtQueryVolumeInformationFile(hFile, &iosb, attrInfoBuf, sizeof(attrInfoBuf), 5);

		if (status >= 0) {
			FILE_FS_ATTRIBUTE_INFORMATION* pAttrInfo = (FILE_FS_ATTRIBUTE_INFORMATION*)attrInfoBuf;
			if (lpMaximumComponentLength) {
				*lpMaximumComponentLength = pAttrInfo->MaximumComponentNameLength;
			}
			if (lpFileSystemFlags) {
				*lpFileSystemFlags = pAttrInfo->FileSystemAttributes;
			}
			if (lpFileSystemNameBuffer && nFileSystemNameSize > 0) {
				DWORD charsToCopy = pAttrInfo->FileSystemNameLength / sizeof(WCHAR);
				if (charsToCopy >= nFileSystemNameSize) charsToCopy = nFileSystemNameSize - 1;
				wcsncpy(lpFileSystemNameBuffer, pAttrInfo->FileSystemName, charsToCopy);
				lpFileSystemNameBuffer[charsToCopy] = L'\0';
			}
		}
		else {
			SetLastError(XP_NtStatusToDosError(status));
			return FALSE;
		}
	}

	return TRUE;
}

typedef LANGID(WINAPI* PFN_GetThreadUILanguage)(VOID);

LANGID XP_GetThreadUILanguage(VOID)
{
	static PFN_GetThreadUILanguage pfnGetThreadUILanguage = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) pfnGetThreadUILanguage = (PFN_GetThreadUILanguage)GetProcAddress(hKernel, "GetThreadUILanguage");
		resolved = TRUE;
	}

	if (pfnGetThreadUILanguage) {
		return pfnGetThreadUILanguage();
	}

	return GetUserDefaultUILanguage();
}

typedef BOOL(WINAPI* PFN_CancelIoEx)(HANDLE, LPOVERLAPPED);

BOOL XP_CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped)
{
	static PFN_CancelIoEx pfnCancelIoEx = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) pfnCancelIoEx = (PFN_CancelIoEx)GetProcAddress(hKernel, "CancelIoEx");
		resolved = TRUE;
	}

	if (pfnCancelIoEx) {
		return pfnCancelIoEx(hFile, lpOverlapped);
	}

	return CancelIo(hFile);
}

typedef BOOL(WINAPI* PFN_CancelSynchronousIo)(HANDLE);

BOOL XP_CancelSynchronousIo(HANDLE hThread)
{
	static PFN_CancelSynchronousIo pfnCancelSynchronousIo = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) pfnCancelSynchronousIo = (PFN_CancelSynchronousIo)GetProcAddress(hKernel, "CancelSynchronousIo");
		resolved = TRUE;
	}

	if (pfnCancelSynchronousIo) {
		return pfnCancelSynchronousIo(hThread);
	}

	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}

typedef BOOLEAN(WINAPI* PFN_CreateSymbolicLinkW)(LPCWSTR, LPCWSTR, DWORD);

BOOLEAN XP_CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags)
{
	static PFN_CreateSymbolicLinkW pfnCreateSymbolicLinkW = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) {
			pfnCreateSymbolicLinkW = (PFN_CreateSymbolicLinkW)GetProcAddress(hKernel, "CreateSymbolicLinkW");
		}
		resolved = TRUE;
	}

	if (pfnCreateSymbolicLinkW) {
		return pfnCreateSymbolicLinkW(lpSymlinkFileName, lpTargetFileName, dwFlags);
	}

	if ((dwFlags & SYMBOLIC_LINK_FLAG_DIRECTORY) == 0) {
		wchar_t combined[2 * MAX_PATH], resolved[2 * MAX_PATH];
		const wchar_t* separator;
		size_t directory_len;

		// Replace a relative file symlink with an NTFS hard link on XP
		if ((lpTargetFileName[0] == L'\\') ||
			((lpTargetFileName[0] != 0) && (lpTargetFileName[1] == L':'))) {
			if (CreateHardLinkW(lpSymlinkFileName, lpTargetFileName, NULL))
				return TRUE;
		} else {
			separator = wcsrchr(lpSymlinkFileName, L'\\');
			if (separator == NULL)
				separator = wcsrchr(lpSymlinkFileName, L'/');
			if (separator != NULL) {
				directory_len = (size_t)(separator - lpSymlinkFileName + 1);
				if ((directory_len + wcslen(lpTargetFileName) + 1 < ARRAYSIZE(combined)) &&
					(directory_len < ARRAYSIZE(combined))) {
					memcpy(combined, lpSymlinkFileName, directory_len * sizeof(wchar_t));
					wcscpy(&combined[directory_len], lpTargetFileName);
					if ((GetFullPathNameW(combined, ARRAYSIZE(resolved), resolved, NULL) > 0) &&
						CreateHardLinkW(lpSymlinkFileName, resolved, NULL))
						return TRUE;
				}
			}
		}
	}

	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}

typedef DWORD(WINAPI* PFN_GetFinalPathNameByHandleW)(HANDLE, LPWSTR, DWORD, DWORD);

typedef struct _FILE_NAME_INFORMATION {
	ULONG FileNameLength;
	WCHAR FileName[1];
} FILE_NAME_INFORMATION, * PFILE_NAME_INFORMATION;

typedef NTSTATUS(WINAPI* PFN_NtQueryInformationFile)(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FileInformation,
	ULONG Length,
	ULONG FileInformationClass
	);

DWORD XP_GetFinalPathNameByHandleW(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags)
{
	static PFN_GetFinalPathNameByHandleW pfnGetFinalPathNameByHandleW = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) {
			pfnGetFinalPathNameByHandleW = (PFN_GetFinalPathNameByHandleW)GetProcAddress(hKernel, "GetFinalPathNameByHandleW");
		}
		resolved = TRUE;
	}

	if (pfnGetFinalPathNameByHandleW) {
		return pfnGetFinalPathNameByHandleW(hFile, lpszFilePath, cchFilePath, dwFlags);
	}

	// Accept opened-name semantics returned by NtQueryInformationFile
	if ((dwFlags & ~FILE_NAME_OPENED) != VOLUME_NAME_DOS) {
		SetLastError(ERROR_NOT_SUPPORTED);
		return 0;
	}

	HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
	if (!hNtDll) return 0;

	PFN_NtQueryInformationFile pfnNtQueryInformationFile =
		(PFN_NtQueryInformationFile)GetProcAddress(hNtDll, "NtQueryInformationFile");
	if (!pfnNtQueryInformationFile) return 0;

	BYTE buffer[sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR)];
	IO_STATUS_BLOCK iosb;
	NTSTATUS status = pfnNtQueryInformationFile(hFile, &iosb, buffer, sizeof(buffer), 9);

	if (status < 0) {
		SetLastError(XP_NtStatusToDosError(status));
		return 0;
	}

	PFILE_NAME_INFORMATION pNameInfo = (PFILE_NAME_INFORMATION)buffer;
	if (pNameInfo->FileNameLength == 0) {
		return 0;
	}

	WCHAR szNtPath[MAX_PATH];
	DWORD cchNtPath = pNameInfo->FileNameLength / sizeof(WCHAR);
	if (cchNtPath >= MAX_PATH) cchNtPath = MAX_PATH - 1;
	wcsncpy(szNtPath, pNameInfo->FileName, cchNtPath);
	szNtPath[cchNtPath] = L'\0';

	WCHAR szDrive[3] = L"A:";
	WCHAR szDosTarget[MAX_PATH];

	for (WCHAR c = L'A'; c <= L'Z'; c++) {
		szDrive[0] = c;
		if (QueryDosDeviceW(szDrive, szDosTarget, MAX_PATH)) {
			size_t cchDosTarget = wcslen(szDosTarget);
			if (_wcsnicmp(szNtPath, szDosTarget, cchDosTarget) == 0) {
				WCHAR szResolved[MAX_PATH] = { 0 };
				_snwprintf(szResolved, MAX_PATH - 1, L"\\\\?\\%c:%s", c, szNtPath + cchDosTarget);
				szResolved[MAX_PATH - 1] = L'\0';

				DWORD cchResolved = wcslen(szResolved);
				if (cchResolved < cchFilePath) {
					wcscpy(lpszFilePath, szResolved);
					return cchResolved;
				}
				return cchResolved + 1;
			}
		}
	}

	if (cchNtPath < cchFilePath) {
		wcscpy(lpszFilePath, szNtPath);
		return cchNtPath;
	}
	return cchNtPath + 1;
}

typedef BOOL(WINAPI* PFN_GetProductInfo)(DWORD, DWORD, DWORD, DWORD, PDWORD);

BOOL XP_GetProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion, DWORD dwSpMinorVersion, PDWORD pdwReturnedProductType)
{
	static PFN_GetProductInfo pfnGetProductInfo = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) pfnGetProductInfo = (PFN_GetProductInfo)GetProcAddress(hKernel, "GetProductInfo");
		resolved = TRUE;
	}

	if (pfnGetProductInfo) {
		return pfnGetProductInfo(dwOSMajorVersion, dwOSMinorVersion, dwSpMajorVersion, dwSpMinorVersion, pdwReturnedProductType);
	}

	if (pdwReturnedProductType) {
		*pdwReturnedProductType = 0;
	}
	return TRUE;
}

typedef INT(WINAPI* PFN_LCIDToLocaleName)(LCID, LPWSTR, INT, DWORD);

INT XP_LCIDToLocaleName(LCID Locale, LPWSTR lpName, INT cchName, DWORD dwFlags)
{
	static PFN_LCIDToLocaleName pfnLCIDToLocaleName = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) pfnLCIDToLocaleName = (PFN_LCIDToLocaleName)GetProcAddress(hKernel, "LCIDToLocaleName");
		resolved = TRUE;
	}

	if (pfnLCIDToLocaleName) {
		return pfnLCIDToLocaleName(Locale, lpName, cchName, dwFlags);
	}

	if (!lpName || cchName <= 0) return 0;

	WCHAR lang[9] = { 0 }, cty[9] = { 0 };
	if (GetLocaleInfoW(Locale, LOCALE_SISO639LANGNAME, lang, 9) > 0 &&
		GetLocaleInfoW(Locale, LOCALE_SISO3166CTRYNAME, cty, 9) > 0) {
		return _snwprintf(lpName, cchName, L"%s-%s", lang, cty);
	}

	return 0;
}

typedef BOOL(WINAPI* PFN_QueryFullProcessImageNameW)(HANDLE, DWORD, LPWSTR, PDWORD);

BOOL XP_QueryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD pdwSize)
{
	static PFN_QueryFullProcessImageNameW pfnQueryFullProcessImageNameW = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) pfnQueryFullProcessImageNameW = (PFN_QueryFullProcessImageNameW)GetProcAddress(hKernel, "QueryFullProcessImageNameW");
		resolved = TRUE;
	}

	if (pfnQueryFullProcessImageNameW) {
		return pfnQueryFullProcessImageNameW(hProcess, dwFlags, lpExeName, pdwSize);
	}

	typedef DWORD(WINAPI* PFN_GetModuleFileNameExW)(HANDLE, HMODULE, LPWSTR, DWORD);
	HMODULE hPsapi = LoadLibraryA("psapi.dll");
	if (hPsapi) {
		PFN_GetModuleFileNameExW pfnGetModuleFileNameExW =
			(PFN_GetModuleFileNameExW)GetProcAddress(hPsapi, "GetModuleFileNameExW");
		if (pfnGetModuleFileNameExW && pdwSize) {
			DWORD ret = pfnGetModuleFileNameExW(hProcess, NULL, lpExeName, *pdwSize);
			if (ret > 0) {
				*pdwSize = ret;
				// Release the dynamically loaded PSAPI module before returning
				FreeLibrary(hPsapi);
				return TRUE;
			}
		}
		FreeLibrary(hPsapi);
	}

	return FALSE;
}

typedef BOOL(WINAPI* PFN_SetDefaultDllDirectories)(DWORD);

BOOL XP_SetDefaultDllDirectories(DWORD DirectoryFlags)
{
	static PFN_SetDefaultDllDirectories pfnSetDefaultDllDirectories = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");
		if (hKernel) pfnSetDefaultDllDirectories = (PFN_SetDefaultDllDirectories)GetProcAddress(hKernel, "SetDefaultDllDirectories");
		resolved = TRUE;
	}

	if (pfnSetDefaultDllDirectories) {
		return pfnSetDefaultDllDirectories(DirectoryFlags);
	}

	return TRUE;
}

typedef HRESULT(WINAPI* PFN_SHCreateItemFromParsingName)(PCWSTR, IBindCtx*, REFIID, void**);

HRESULT XP_SHCreateItemFromParsingName(PCWSTR pszPath, IBindCtx* pbc, REFIID riid, void** ppv)
{
	static PFN_SHCreateItemFromParsingName pfnSHCreateItemFromParsingName = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hShell = GetModuleHandleA("shell32.dll");
		if (hShell) pfnSHCreateItemFromParsingName = (PFN_SHCreateItemFromParsingName)GetProcAddress(hShell, "SHCreateItemFromParsingName");
		resolved = TRUE;
	}

	if (pfnSHCreateItemFromParsingName) {
		return pfnSHCreateItemFromParsingName(pszPath, pbc, riid, ppv);
	}

	PIDLIST_ABSOLUTE pidl = NULL;
	HRESULT hr = SHParseDisplayName(pszPath, pbc, &pidl, 0, NULL);
	if (SUCCEEDED(hr)) {
		typedef HRESULT(WINAPI* PFN_SHCreateShellItem)(LPCITEMIDLIST, IShellFolder*, LPCITEMIDLIST, IShellItem**);

		HMODULE hShell = GetModuleHandleA("shell32.dll");
		PFN_SHCreateShellItem pfnSHCreateShellItem = NULL;

		if (hShell) {
			pfnSHCreateShellItem = (PFN_SHCreateShellItem)GetProcAddress(hShell, "SHCreateShellItem");
		}

		if (pfnSHCreateShellItem) {
			hr = pfnSHCreateShellItem(NULL, NULL, pidl, (IShellItem**)ppv);
		}
		else {
			hr = E_NOTIMPL;
		}

		CoTaskMemFree(pidl);
	}
	return hr;
}

typedef HRESULT(WINAPI* PFN_SHGetKnownFolderPath)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);

UINT XP_GetDpiForWindow(HWND hWnd)
{
	typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
	static PFN_GetDpiForWindow pfnGetDpiForWindow = NULL;
	static BOOL resolved = FALSE;
	HDC hDC;
	UINT dpi = 96;

	// Use GetDpiForWindow dynamically when a newer system provides it
	if (!resolved) {
		HMODULE hUser = GetModuleHandleA("user32.dll");
		if (hUser != NULL)
			pfnGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(hUser, "GetDpiForWindow");
		resolved = TRUE;
	}
	if (pfnGetDpiForWindow != NULL)
		return pfnGetDpiForWindow(hWnd);

	// Read XP logical DPI from LOGPIXELSX because its icon metric is not DPI-aware
	hDC = GetDC(hWnd);
	if (hDC != NULL) {
		dpi = (UINT)GetDeviceCaps(hDC, LOGPIXELSX);
		ReleaseDC(hWnd, hDC);
	}
	return (dpi == 0) ? 96 : dpi;
}

HRESULT XP_SHGetKnownFolderPath(REFKNOWNFOLDERID rid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath)
{
	static PFN_SHGetKnownFolderPath pfnSHGetKnownFolderPath = NULL;
	static BOOL resolved = FALSE;

	if (!resolved) {
		HMODULE hShell = GetModuleHandleA("shell32.dll");
		if (hShell) pfnSHGetKnownFolderPath = (PFN_SHGetKnownFolderPath)GetProcAddress(hShell, "SHGetKnownFolderPath");
		resolved = TRUE;
	}

	if (pfnSHGetKnownFolderPath) {
		return pfnSHGetKnownFolderPath(rid, dwFlags, hToken, ppszPath);
	}

	if (!ppszPath) return E_POINTER;
	*ppszPath = NULL;

	int csidl = -1;
	if (IsEqualGUID(rid, &FOLDERID_Downloads))        csidl = CSIDL_PERSONAL;
	else if (IsEqualGUID(rid, &FOLDERID_Documents))    csidl = CSIDL_PERSONAL;
	else if (IsEqualGUID(rid, &FOLDERID_Desktop))      csidl = CSIDL_DESKTOP;
	else if (IsEqualGUID(rid, &FOLDERID_ProgramFiles)) csidl = CSIDL_PROGRAM_FILES;
	// Map post-XP known-folder IDs to their legacy AppData CSIDLs
	else if (IsEqualGUID(rid, &FOLDERID_LocalAppData)) csidl = CSIDL_LOCAL_APPDATA;
	else if (IsEqualGUID(rid, &FOLDERID_RoamingAppData)) csidl = CSIDL_APPDATA;

	if (csidl == -1) return E_NOTIMPL;

	WCHAR szPath[MAX_PATH];
	HRESULT hr = SHGetFolderPathW(NULL, csidl, hToken, 0, szPath);
	if (SUCCEEDED(hr)) {
		SIZE_T bytes = (wcslen(szPath) + 1) * sizeof(WCHAR);
		*ppszPath = (PWSTR)CoTaskMemAlloc(bytes);
		if (*ppszPath) {
			memcpy(*ppszPath, szPath, bytes);
		}
		else {
			hr = E_OUTOFMEMORY;
		}
	}
	return hr;
}
