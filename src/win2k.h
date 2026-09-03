#pragma once

#include <windows.h>
#include <shlobj.h>

#ifdef __cplusplus
extern "C" {
#endif

extern BOOL WINAPI W2K_AttachConsole(DWORD dwProcessId);
extern PVOID WINAPI W2K_DecodePointer(PVOID Ptr);
extern PVOID WINAPI W2K_EncodePointer(PVOID Ptr);
extern BOOL WINAPI W2K_GetModuleHandleExA(DWORD dwFlags, LPCSTR lpModuleName, HMODULE* phModule);
extern BOOL WINAPI W2K_GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule);
extern VOID WINAPI W2K_GetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo);
extern VOID WINAPI W2K_InitializeSListHead(PSLIST_HEADER ListHead);
extern BOOL WINAPI W2K_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process);
extern LANGID WINAPI W2K_SetThreadUILanguage(LANGID LangId);
extern HRESULT WINAPI W2K_SHParseDisplayName(PCWSTR pszName, IBindCtx* pbc,
	PIDLIST_ABSOLUTE* ppidl, SFGAOF sfgaoIn, SFGAOF* psfgaoOut);

extern HICON W2K_LoadIconResource(HINSTANCE hInstance, int resourceId, int width, int height);
extern HICON W2K_LoadAlphaIconResource(HINSTANCE hInstance, int resourceId,
	int width, int height, COLORREF background);
extern void W2K_RestoreComboBoxDropHeights(HWND hDlg);

#define AttachConsole W2K_AttachConsole
#define DecodePointer W2K_DecodePointer
#define EncodePointer W2K_EncodePointer
#define GetModuleHandleExA W2K_GetModuleHandleExA
#define GetModuleHandleExW W2K_GetModuleHandleExW
#define GetNativeSystemInfo W2K_GetNativeSystemInfo
#define InitializeSListHead W2K_InitializeSListHead
#define IsWow64Process W2K_IsWow64Process
#define SetThreadUILanguage W2K_SetThreadUILanguage
#define SHParseDisplayName W2K_SHParseDisplayName

#ifdef __cplusplus
}
#endif
