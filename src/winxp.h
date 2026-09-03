#pragma once

#include <windows.h>
#include <shlobj.h>

// Keep Windows 2000 import wrappers separate from the XP API replacements
#include "win2k.h"

#ifdef __cplusplus
extern "C" {
#endif

// Kernel32 replacements
extern ULONGLONG XP_GetTickCount64(VOID);
extern BOOL      XP_GetVolumeInformationByHandleW(HANDLE hFile, LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
extern LANGID    XP_GetThreadUILanguage(VOID);
extern BOOL      XP_CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped);
extern BOOL      XP_CancelSynchronousIo(HANDLE hThread);
extern BOOLEAN   XP_CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);
extern DWORD     XP_GetFinalPathNameByHandleW(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
extern BOOL      XP_GetProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion, DWORD dwSpMinorVersion, PDWORD pdwReturnedProductType);
extern INT       XP_LCIDToLocaleName(LCID Locale, LPWSTR lpName, INT cchName, DWORD dwFlags);
extern BOOL      XP_QueryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD pdwSize);
extern BOOL      XP_SetDefaultDllDirectories(DWORD DirectoryFlags);
// Query logical DPI through an API that is available on XP. (port)
extern UINT      XP_GetDpiForWindow(HWND hWnd);

// Shell32 replacements
extern HRESULT   XP_SHCreateItemFromParsingName(PCWSTR pszPath, IBindCtx* pbc, REFIID riid, void** ppv);
extern HRESULT   XP_SHGetKnownFolderPath(REFKNOWNFOLDERID rid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);

// Redirect post-XP APIs to the compatibility implementations
#define GetTickCount64                  XP_GetTickCount64
#define GetVolumeInformationByHandleW   XP_GetVolumeInformationByHandleW
#define GetThreadUILanguage             XP_GetThreadUILanguage
#define CancelIoEx                      XP_CancelIoEx
#define CancelSynchronousIo             XP_CancelSynchronousIo
#define CreateSymbolicLinkW             XP_CreateSymbolicLinkW
#define GetFinalPathNameByHandleW       XP_GetFinalPathNameByHandleW
#define GetProductInfo                  XP_GetProductInfo
#define LCIDToLocaleName                XP_LCIDToLocaleName
#define QueryFullProcessImageNameW      XP_QueryFullProcessImageNameW
#define SetDefaultDllDirectories        XP_SetDefaultDllDirectories

#define SHCreateItemFromParsingName     XP_SHCreateItemFromParsingName
#define SHGetKnownFolderPath            XP_SHGetKnownFolderPath

#ifdef __cplusplus
}
#endif
