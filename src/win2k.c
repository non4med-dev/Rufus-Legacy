#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shlobj.h>
#include <limits.h>
#include <stdlib.h>
#include "win2k.h"

#undef AttachConsole
#undef DecodePointer
#undef EncodePointer
#undef GetModuleHandleExA
#undef GetModuleHandleExW
#undef GetNativeSystemInfo
#undef InitializeSListHead
#undef IsWow64Process
#undef SetThreadUILanguage
#undef SHParseDisplayName

#pragma pack(push, 2)
typedef struct {
	WORD Reserved;
	WORD Type;
	WORD Count;
} W2K_GROUP_ICON_DIRECTORY;

typedef struct {
	BYTE Width;
	BYTE Height;
	BYTE ColorCount;
	BYTE Reserved;
	WORD Planes;
	WORD BitCount;
	DWORD BytesInResource;
	WORD ResourceId;
} W2K_GROUP_ICON_ENTRY;
#pragma pack(pop)

// Resolve optional kernel exports without adding Windows 2000 loader dependencies
static FARPROC W2K_GetKernelProc(const char* name)
{
	HMODULE module = GetModuleHandleA("kernel32.dll");
	return (module == NULL) ? NULL : GetProcAddress(module, name);
}

// Select an icon group entry by size and, when requested, by alpha depth
static const BYTE* W2K_GetIconResource(HINSTANCE hInstance, int resourceId,
	int width, int height, BOOL requireAlpha, DWORD* iconSize)
{
	HRSRC groupResource, iconResource;
	HGLOBAL groupHandle, iconHandle;
	const W2K_GROUP_ICON_DIRECTORY* directory;
	const W2K_GROUP_ICON_ENTRY* entries;
	const BYTE* iconData = NULL;
	DWORD groupSize;
	int i, best = -1, bestDistance = INT_MAX, bestBitCount = -1;

	if (iconSize == NULL)
		return NULL;
	*iconSize = 0;
	groupResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_GROUP_ICON);
	if (groupResource == NULL)
		return NULL;
	groupSize = SizeofResource(hInstance, groupResource);
	groupHandle = LoadResource(hInstance, groupResource);
	directory = (const W2K_GROUP_ICON_DIRECTORY*)LockResource(groupHandle);
	if ((directory == NULL) || (directory->Type != 1) || (directory->Count == 0) ||
		(groupSize < sizeof(*directory) + directory->Count * sizeof(*entries)))
		return NULL;

	entries = (const W2K_GROUP_ICON_ENTRY*)(directory + 1);
	for (i = 0; i < directory->Count; i++) {
		int entryWidth = (entries[i].Width == 0) ? 256 : entries[i].Width;
		int entryHeight = (entries[i].Height == 0) ? 256 : entries[i].Height;
		int distance = abs(entryWidth - width) + abs(entryHeight - height);
		if (requireAlpha && (entries[i].BitCount != 32))
			continue;
		if ((distance < bestDistance) ||
			((distance == bestDistance) && (entries[i].BitCount > bestBitCount))) {
			best = i;
			bestDistance = distance;
			bestBitCount = entries[i].BitCount;
		}
	}
	if (best < 0)
		return NULL;

	iconResource = FindResource(hInstance, MAKEINTRESOURCE(entries[best].ResourceId), RT_ICON);
	if (iconResource == NULL)
		return NULL;
	*iconSize = SizeofResource(hInstance, iconResource);
	iconHandle = LoadResource(hInstance, iconResource);
	iconData = (const BYTE*)LockResource(iconHandle);
	return ((*iconSize == 0) ? NULL : iconData);
}

// Load the best legacy-compatible frame from a native icon resource
HICON W2K_LoadIconResource(HINSTANCE hInstance, int resourceId, int width, int height)
{
	DWORD iconSize;
	const BYTE* iconData = W2K_GetIconResource(hInstance, resourceId,
		width, height, FALSE, &iconSize);
	HICON icon = (iconData == NULL) ? NULL : CreateIconFromResourceEx((PBYTE)iconData,
		iconSize, TRUE, 0x00030000, width, height, LR_DEFAULTCOLOR);

	if (icon != NULL)
		return icon;
	return (HICON)LoadImageA(hInstance, MAKEINTRESOURCEA(resourceId), IMAGE_ICON,
		width, height, LR_DEFAULTCOLOR);
}

// Precompose a 32-bit alpha frame because Windows 2000 image lists discard alpha
HICON W2K_LoadAlphaIconResource(HINSTANCE hInstance, int resourceId,
	int width, int height, COLORREF background)
{
	BITMAPINFO bitmapInfo = { 0 };
	const BITMAPINFOHEADER* sourceHeader;
	BYTE *colorBits = NULL, *maskBits = NULL;
	const BYTE* sourceBits;
	DWORD iconSize, colorSize, maskSize, maskStride;
	HDC hDC = NULL;
	HBITMAP colorBitmap = NULL, maskBitmap = NULL;
	HICON icon = NULL;
	ICONINFO iconInfo = { 0 };
	int i, pixelCount, sourceWidth, sourceHeight;
	BYTE alpha, red, green, blue;

	sourceBits = W2K_GetIconResource(hInstance, resourceId, width, height, TRUE, &iconSize);
	if ((sourceBits == NULL) || (iconSize < sizeof(BITMAPINFOHEADER)))
		goto fallback;
	sourceHeader = (const BITMAPINFOHEADER*)(const void*)sourceBits;
	sourceWidth = sourceHeader->biWidth;
	sourceHeight = abs(sourceHeader->biHeight) / 2;
	if ((sourceHeader->biSize < sizeof(BITMAPINFOHEADER)) ||
		(sourceHeader->biSize > iconSize) || (sourceHeader->biPlanes != 1) ||
		(sourceHeader->biBitCount != 32) || (sourceHeader->biCompression != BI_RGB) ||
		(sourceWidth != width) || (sourceHeight != height))
		goto fallback;
	pixelCount = width * height;
	colorSize = pixelCount * 4;
	if (sourceHeader->biSize + colorSize > iconSize)
		goto fallback;
	sourceBits += sourceHeader->biSize;

	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = width;
	bitmapInfo.bmiHeader.biHeight = height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	hDC = GetDC(NULL);
	colorBitmap = CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS,
		(void**)&colorBits, NULL, 0);
	ReleaseDC(NULL, hDC);
	hDC = NULL;
	if ((colorBitmap == NULL) || (colorBits == NULL))
		goto out;

	maskStride = ((width + 15) / 16) * 2;
	maskSize = maskStride * height;
	maskBits = (BYTE*)calloc(maskSize, 1);
	if (maskBits == NULL)
		goto out;
	red = GetRValue(background);
	green = GetGValue(background);
	blue = GetBValue(background);
	for (i = 0; i < pixelCount; i++) {
		alpha = sourceBits[4 * i + 3];
		if (alpha == 0) {
			// Reverse mask rows so they align with the bottom-up colour bitmap. (port)
			maskBits[(height - 1 - i / width) * maskStride + (i % width) / 8] |=
				(BYTE)(0x80 >> ((i % width) & 7));
			colorBits[4 * i + 0] = 0;
			colorBits[4 * i + 1] = 0;
			colorBits[4 * i + 2] = 0;
		} else {
			colorBits[4 * i + 0] = (BYTE)((sourceBits[4 * i + 0] * alpha +
				blue * (255 - alpha) + 127) / 255);
			colorBits[4 * i + 1] = (BYTE)((sourceBits[4 * i + 1] * alpha +
				green * (255 - alpha) + 127) / 255);
			colorBits[4 * i + 2] = (BYTE)((sourceBits[4 * i + 2] * alpha +
				red * (255 - alpha) + 127) / 255);
		}
		colorBits[4 * i + 3] = 0;
	}
	maskBitmap = CreateBitmap(width, height, 1, 1, maskBits);
	if (maskBitmap == NULL)
		goto out;
	iconInfo.fIcon = TRUE;
	iconInfo.hbmColor = colorBitmap;
	iconInfo.hbmMask = maskBitmap;
	icon = CreateIconIndirect(&iconInfo);

out:
	free(maskBits);
	if (maskBitmap != NULL)
		DeleteObject(maskBitmap);
	if (colorBitmap != NULL)
		DeleteObject(colorBitmap);
	if (icon != NULL)
		return icon;

fallback:
	return W2K_LoadIconResource(hInstance, resourceId, width, height);
}

static BOOL CALLBACK W2K_RestoreComboBoxDropHeight(HWND hCtrl, LPARAM unused)
{
	char className[16];
	RECT rect;
	int itemHeight, selectionHeight, dropHeight;

	UNREFERENCED_PARAMETER(unused);
	if ((GetClassNameA(hCtrl, className, ARRAYSIZE(className)) == 0) ||
		(lstrcmpiA(className, "ComboBox") != 0))
		return TRUE;
	itemHeight = (int)SendMessage(hCtrl, CB_GETITEMHEIGHT, 0, 0);
	selectionHeight = (int)SendMessage(hCtrl, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
	// An empty combo box may only report its selection-field height. (port)
	if ((itemHeight <= 0) && (selectionHeight > 0))
		itemHeight = selectionHeight;
	if ((itemHeight <= 0) || (selectionHeight <= 0))
		return TRUE;
	GetWindowRect(hCtrl, &rect);
	MapWindowPoints(NULL, GetParent(hCtrl), (POINT*)&rect, 2);
	dropHeight = selectionHeight + 8 * itemHeight + 6;
	// Restore room for eight items after Rufus resizes the combo box. (port)
	SetWindowPos(hCtrl, NULL, rect.left, rect.top, rect.right - rect.left, dropHeight,
		SWP_NOZORDER | SWP_NOACTIVATE);
	return TRUE;
}

void W2K_RestoreComboBoxDropHeights(HWND hDlg)
{
	EnumChildWindows(hDlg, W2K_RestoreComboBoxDropHeight, 0);
}

BOOL WINAPI W2K_AttachConsole(DWORD dwProcessId)
{
	typedef BOOL(WINAPI* Fn)(DWORD);
	Fn fn = (Fn)W2K_GetKernelProc("AttachConsole");
	if (fn != NULL)
		return fn(dwProcessId);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

PVOID WINAPI W2K_DecodePointer(PVOID Ptr)
{
	typedef PVOID(WINAPI* Fn)(PVOID);
	Fn fn = (Fn)W2K_GetKernelProc("DecodePointer");
	// Pointer encoding is an identity operation when the native API is unavailable. (port)
	return (fn == NULL) ? Ptr : fn(Ptr);
}

PVOID WINAPI W2K_EncodePointer(PVOID Ptr)
{
	typedef PVOID(WINAPI* Fn)(PVOID);
	Fn fn = (Fn)W2K_GetKernelProc("EncodePointer");
	// Match the identity fallback used by the decoder. (port)
	return (fn == NULL) ? Ptr : fn(Ptr);
}

static BOOL W2K_GetModuleFromAddress(const void* address, HMODULE* module)
{
	MEMORY_BASIC_INFORMATION mbi;
	if ((VirtualQuery(address, &mbi, sizeof(mbi)) == 0) || (mbi.AllocationBase == NULL)) {
		SetLastError(ERROR_MOD_NOT_FOUND);
		return FALSE;
	}
	*module = (HMODULE)mbi.AllocationBase;
	return TRUE;
}

BOOL WINAPI W2K_GetModuleHandleExA(DWORD dwFlags, LPCSTR lpModuleName, HMODULE* phModule)
{
	typedef BOOL(WINAPI* Fn)(DWORD, LPCSTR, HMODULE*);
	Fn fn = (Fn)W2K_GetKernelProc("GetModuleHandleExA");
	HMODULE module;
	char path[MAX_PATH];

	if (fn != NULL)
		return fn(dwFlags, lpModuleName, phModule);
	if ((dwFlags & ~(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) || (phModule == NULL) ||
		((dwFlags & GET_MODULE_HANDLE_EX_FLAG_PIN) &&
		(dwFlags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT))) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (dwFlags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS) {
		if (!W2K_GetModuleFromAddress(lpModuleName, &module))
			return FALSE;
	} else {
		module = GetModuleHandleA(lpModuleName);
		if (module == NULL)
			return FALSE;
	}
	if ((dwFlags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) == 0) {
		if ((GetModuleFileNameA(module, path, ARRAYSIZE(path)) == 0) ||
			((module = LoadLibraryA(path)) == NULL))
			return FALSE;
	}
	*phModule = module;
	return TRUE;
}

BOOL WINAPI W2K_GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule)
{
	typedef BOOL(WINAPI* Fn)(DWORD, LPCWSTR, HMODULE*);
	Fn fn = (Fn)W2K_GetKernelProc("GetModuleHandleExW");
	HMODULE module;
	wchar_t path[MAX_PATH];

	if (fn != NULL)
		return fn(dwFlags, lpModuleName, phModule);
	if ((dwFlags & ~(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) || (phModule == NULL) ||
		((dwFlags & GET_MODULE_HANDLE_EX_FLAG_PIN) &&
		(dwFlags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT))) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (dwFlags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS) {
		if (!W2K_GetModuleFromAddress(lpModuleName, &module))
			return FALSE;
	} else {
		module = GetModuleHandleW(lpModuleName);
		if (module == NULL)
			return FALSE;
	}
	if ((dwFlags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) == 0) {
		if ((GetModuleFileNameW(module, path, ARRAYSIZE(path)) == 0) ||
			((module = LoadLibraryW(path)) == NULL))
			return FALSE;
	}
	*phModule = module;
	return TRUE;
}

VOID WINAPI W2K_GetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	typedef VOID(WINAPI* Fn)(LPSYSTEM_INFO);
	Fn fn = (Fn)W2K_GetKernelProc("GetNativeSystemInfo");
	if (fn != NULL)
		fn(lpSystemInfo);
	else
		// Windows 2000 cannot run under WOW64, so GetSystemInfo is already native. (port)
		GetSystemInfo(lpSystemInfo);
}

VOID WINAPI W2K_InitializeSListHead(PSLIST_HEADER ListHead)
{
	typedef VOID(WINAPI* Fn)(PSLIST_HEADER);
	Fn fn = (Fn)W2K_GetKernelProc("InitializeSListHead");
	if (fn != NULL)
		fn(ListHead);
	else if (ListHead != NULL)
		// Initialize an empty Windows 2000 SLIST header to zero. (port)
		ZeroMemory(ListHead, sizeof(*ListHead));
}

BOOL WINAPI W2K_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process)
{
	typedef BOOL(WINAPI* Fn)(HANDLE, PBOOL);
	Fn fn = (Fn)W2K_GetKernelProc("IsWow64Process");
	if (fn != NULL)
		return fn(hProcess, Wow64Process);
	if (Wow64Process == NULL) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	// Windows 2000 has no WOW64 execution environment. (port)
	*Wow64Process = FALSE;
	return TRUE;
}

LANGID WINAPI W2K_SetThreadUILanguage(LANGID LangId)
{
	typedef LANGID(WINAPI* Fn)(LANGID);
	Fn fn = (Fn)W2K_GetKernelProc("SetThreadUILanguage");
	LANGID previous;
	if (fn != NULL)
		return fn(LangId);
	previous = LANGIDFROMLCID(GetThreadLocale());
	// Use SetThreadLocale as the Windows 2000 UI-language fallback. (port)
	return SetThreadLocale(MAKELCID(LangId, SORT_DEFAULT)) ? previous : 0;
}

HRESULT WINAPI W2K_SHParseDisplayName(PCWSTR pszName, IBindCtx* pbc,
	PIDLIST_ABSOLUTE* ppidl, SFGAOF sfgaoIn, SFGAOF* psfgaoOut)
{
	typedef HRESULT(WINAPI* Fn)(PCWSTR, IBindCtx*, PIDLIST_ABSOLUTE*, SFGAOF, SFGAOF*);
	HMODULE shell = GetModuleHandleA("shell32.dll");
	Fn fn = (shell == NULL) ? NULL : (Fn)GetProcAddress(shell, "SHParseDisplayName");
	IShellFolder* desktop = NULL;
	ULONG eaten = 0;
	SFGAOF attributes = sfgaoIn;
	HRESULT hr;

	if (fn != NULL)
		return fn(pszName, pbc, ppidl, sfgaoIn, psfgaoOut);
	if ((pszName == NULL) || (ppidl == NULL))
		return E_INVALIDARG;
	*ppidl = NULL;
	hr = SHGetDesktopFolder(&desktop);
	if (FAILED(hr))
		return hr;
	// Parse the path through the desktop folder when SHParseDisplayName is unavailable. (port)
	hr = IShellFolder_ParseDisplayName(desktop, NULL, pbc, (LPWSTR)pszName,
		&eaten, (LPITEMIDLIST*)ppidl, (psfgaoOut == NULL) ? NULL : &attributes);
	IShellFolder_Release(desktop);
	if (psfgaoOut != NULL)
		*psfgaoOut = attributes;
	return hr;
}

void* __imp_W2K_AttachConsole = (void*)W2K_AttachConsole;
void* __imp_W2K_GetNativeSystemInfo = (void*)W2K_GetNativeSystemInfo;
void* __imp_W2K_IsWow64Process = (void*)W2K_IsWow64Process;
void* __imp_W2K_SetThreadUILanguage = (void*)W2K_SetThreadUILanguage;
void* __imp_W2K_SHParseDisplayName = (void*)W2K_SHParseDisplayName;

#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:__imp__AttachConsole@4=___imp_W2K_AttachConsole")
#pragma comment(linker, "/alternatename:__imp__GetNativeSystemInfo@4=___imp_W2K_GetNativeSystemInfo")
#pragma comment(linker, "/alternatename:__imp__IsWow64Process@8=___imp_W2K_IsWow64Process")
#pragma comment(linker, "/alternatename:__imp__SetThreadUILanguage@4=___imp_W2K_SetThreadUILanguage")
#pragma comment(linker, "/alternatename:__imp__SHParseDisplayName@20=___imp_W2K_SHParseDisplayName")
#endif
