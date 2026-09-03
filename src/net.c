/*
 * Rufus: The Reliable USB Formatting Utility
 * Networking functionality (web file download, check for update, etc.)
 * Copyright © 2012-2025 Pete Batard <pete@akeo.ie>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Memory leaks detection - define _CRTDBG_MAP_ALLOC as preprocessor macro */
#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <windows.h>
// Temporary workaround for MinGW32 delay-loading
// See https://github.com/pbatard/rufus/pull/2513
#if defined(__MINGW32__)
#undef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT __attribute__((visibility("hidden")))
#endif
#include <wininet.h>
#include <netlistmgr.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>
#include <virtdisk.h>

#include "rufus.h"
#include "winxp.h"
#include "missing.h"
#include "resource.h"
#include "msapi_utf8.h"
#include "localization.h"
#include "bled/bled.h"
#include "dbx/dbx_info.h"

#include "settings.h"

/* Maximum download chunk size in bytes (xp experiments) */
#define DOWNLOAD_BUFFER_SIZE    (10*KB)
/* Default delay between update checks */
#define DEFAULT_UPDATE_INTERVAL (24*3600)
#define WININET_TLS10_FLAG 0x00000080
#define WININET_TLS11_FLAG 0x00000200
#define WININET_TLS12_FLAG 0x00000800
#define WININET_TLS13_FLAG 0x00002000
static BOOL tls_restart_required;
#ifndef INTERNET_OPEN_TYPE_DIRECT
#define INTERNET_OPEN_TYPE_DIRECT               1
#endif
#ifndef INTERNET_OPTION_SECURE_PROTOCOLS
#define INTERNET_OPTION_SECURE_PROTOCOLS        31
#endif

static BOOL tls_restart_required;

static BOOL ReadTlsDword(HKEY root, const char* path, const char* name, DWORD* value)
{
	HKEY hKey;
	DWORD type = 0, size = sizeof(*value);
	LONG status;

	status = RegOpenKeyExA(root, path, 0, KEY_READ, &hKey);
	if (status != ERROR_SUCCESS)
		return FALSE;

	status = RegQueryValueExA(hKey, name, NULL, &type, (LPBYTE)value, &size);
	RegCloseKey(hKey);

	return (status == ERROR_SUCCESS) &&
		(type == REG_DWORD) &&
		(size == sizeof(*value));
}

static DWORD GetConfiguredSecureProtocols(void)
{
	DWORD protocols;
	DWORD value;
	const char* internet_settings =
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
	const char* policy_settings =
		"SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
	const char* tls12_client =
		"SYSTEM\\CurrentControlSet\\Control\\SecurityProviders\\SCHANNEL\\Protocols\\TLS 1.2\\Client";

	/*
	 * Group Policy overrides the user's Internet Options.
	 */
	if (ReadTlsDword(HKEY_LOCAL_MACHINE, policy_settings,
		"SecureProtocols", &protocols)) {
	}
	else if (ReadTlsDword(HKEY_CURRENT_USER, internet_settings,
		"SecureProtocols", &protocols)) {
	}
	else if (ReadTlsDword(HKEY_LOCAL_MACHINE, internet_settings,
		"SecureProtocols", &protocols)) {
	}
	else {
		if (WindowsVersion.Version >= WINDOWS_VISTA)
			protocols = WININET_TLS10_FLAG;
		else
			protocols = WININET_TLS10_FLAG |
			WININET_TLS11_FLAG |
			WININET_TLS12_FLAG;
	}

	if ((protocols & WININET_TLS12_FLAG) &&
		ReadTlsDword(HKEY_LOCAL_MACHINE, tls12_client,
			"Enabled", &value) &&
		(value == 0)) {
		protocols &= ~WININET_TLS12_FLAG;
	}

	return protocols;
}

// Fido support and checks for Windows 7 and (experimentally) Vista
// Vista is allowed to run fido under specific circumstances, AND...
// Support is purely based on me injecting myself with hopium that
// someone will bother looking into the SSL connection errors

static BOOL IsDotNet45OrNewerInstalled(void)
{
	HKEY hKey;
	DWORD dwRelease = 0, dwSize = sizeof(DWORD);
	BOOL bInstalled = FALSE;

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
		"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full",
		0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueExA(hKey, "Release", NULL, NULL, (LPBYTE)&dwRelease, &dwSize) == ERROR_SUCCESS) {
			if (dwRelease >= 378389)
				bInstalled = TRUE;
		}
		RegCloseKey(hKey);
	}

	return bInstalled;
}

static BOOL IsWMF4OrNewerInstalled(void)
{
	HKEY hKey;
	char version_str[32] = { 0 };
	DWORD dwSize = sizeof(version_str);
	BOOL bInstalled = FALSE;

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
		"SOFTWARE\\Microsoft\\PowerShell\\3\\PowerShellEngine",
		0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueExA(hKey, "PowerShellVersion", NULL, NULL, (LPBYTE)version_str, &dwSize) == ERROR_SUCCESS) {
			int major_ver = atoi(version_str);
			if (major_ver >= 4)
				bInstalled = TRUE;
		}
		RegCloseKey(hKey);
	}

	return bInstalled;
}

DWORD DownloadStatus;
BYTE* fido_script = NULL;
HANDLE update_check_thread = NULL;

extern loc_cmd* selected_locale;
extern HANDLE dialog_handle;
extern BOOL is_x86_64;
extern USHORT NativeMachine;
static DWORD error_code, fido_len = 0;
static BOOL force_update_check = FALSE;
static const char* request_headers[2] = { "Accept-Encoding: none", "Accept-Encoding: gzip, deflate" };
extern const char* efi_archname[ARCH_MAX];

#if defined(__MINGW32__)
#define INetworkListManager_get_IsConnectedToInternet INetworkListManager_IsConnectedToInternet
#endif

static char* GetShortName(const char* url)
{
	static char short_name[128];
	char *p;
	size_t i, len = safe_strlen(url);
	if (len < 5)
		return NULL;

	for (i = len - 2; i > 0; i--) {
		if (url[i] == '/') {
			i++;
			break;
		}
	}
	memset(short_name, 0, sizeof(short_name));
	static_strcpy(short_name, &url[i]);
	p = strstr(short_name, "%3F");
	if (p != NULL)
		*p = 0;
	p = strstr(short_name, "%3f");
	if (p != NULL)
		*p = 0;
	for (i = 0; i < strlen(short_name); i++) {
		if ((short_name[i] == '?') || (short_name[i] == '#')) {
			short_name[i] = 0;
			break;
		}
	}
	return short_name;
}

static __inline BOOL is_WOW64(void)
{
	BOOL ret = FALSE;
	IsWow64Process(GetCurrentProcess(), &ret);
	return ret;
}

// Open an Internet session
// Lots of bullshit
// My testing shows that without TLS 1.2, networking fails
// So let's do something stupid, and not innitialize WinINet unless it's enabled
static HINTERNET GetInternetSession(const char* user_agent, BOOL bRetry)
{
	DWORD dwProtocols = GetConfiguredSecureProtocols() &
		(WININET_TLS10_FLAG | WININET_TLS11_FLAG | WININET_TLS12_FLAG | WININET_TLS13_FLAG);
	int i;
	char default_agent[64];
	BOOL decodingSupport = TRUE;
	VARIANT_BOOL InternetConnection = VARIANT_FALSE;
	DWORD dwFlags, dwTimeout = NET_SESSION_TIMEOUT, dwProtocolSupport = HTTP_PROTOCOL_FLAG_HTTP2;
	HINTERNET hSession = NULL;
	HRESULT hr = S_FALSE;
	INetworkListManager* pNetworkListManager;
	if (tls_restart_required || !(dwProtocols & WININET_TLS12_FLAG)) {
		SetLastError(ERROR_INTERNET_SECURITY_CHANNEL_ERROR);
		return NULL;
	}
	// Create a NetworkListManager Instance to check the network connection
	IGNORE_RETVAL(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
	hr = CoCreateInstance(&CLSID_NetworkListManager, NULL, CLSCTX_ALL,
		&IID_INetworkListManager, (LPVOID*)&pNetworkListManager);
	if (hr == S_OK) {
		for (i = 0; i <= WRITE_RETRIES; i++) {
			hr = INetworkListManager_get_IsConnectedToInternet(pNetworkListManager, &InternetConnection);
			// INetworkListManager may fail with ERROR_SERVICE_DEPENDENCY_FAIL if the DHCP service
			// is not running, in which case we must fall back to using InternetGetConnectedState().
			// See https://github.com/pbatard/rufus/issues/1801.
			if (hr == HRESULT_FROM_WIN32(ERROR_SERVICE_DEPENDENCY_FAIL)) {
				InternetConnection = InternetGetConnectedState(&dwFlags, 0) ? VARIANT_TRUE : VARIANT_FALSE;
				break;
			}
			if (hr == S_OK || !bRetry)
				break;
			Sleep(1000);
		}
	}
	/* VPN Fix for Windows 7 */
	if (InternetConnection == VARIANT_FALSE) {
		if (!InternetGetConnectedState(&dwFlags, 0)) {
			// Ignore the disconnect check and attempt connection through active adapter (VPN)
			uprintf("Network manager reported offline, attempting connection anyway...");
		}
	}
	static_sprintf(default_agent, APPLICATION_NAME "/%d.%d.%d (Windows NT %lu.%lu%s)",
		rufus_version[0], rufus_version[1], rufus_version[2],
		WindowsVersion.Major, WindowsVersion.Minor, is_WOW64() ? "; WOW64" : "");
	hSession = InternetOpenA((user_agent == NULL) ? default_agent : user_agent,
		INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hSession == NULL)
		return NULL;
	if (!InternetSetOptionA(hSession, INTERNET_OPTION_SECURE_PROTOCOLS, &dwProtocols, sizeof(dwProtocols)) &&
		(dwProtocols & WININET_TLS13_FLAG)) {
		dwProtocols &= ~WININET_TLS13_FLAG;
		InternetSetOptionA(hSession, INTERNET_OPTION_SECURE_PROTOCOLS, &dwProtocols, sizeof(dwProtocols));
	}
	// Set the timeouts
	InternetSetOptionA(hSession, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&dwTimeout, sizeof(dwTimeout));
	InternetSetOptionA(hSession, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&dwTimeout, sizeof(dwTimeout));
	InternetSetOptionA(hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&dwTimeout, sizeof(dwTimeout));
	// Enable gzip and deflate decoding schemes
	InternetSetOptionA(hSession, INTERNET_OPTION_HTTP_DECODING, (LPVOID)&decodingSupport, sizeof(decodingSupport));
	// Enable HTTP/2 protocol support
	InternetSetOptionA(hSession, INTERNET_OPTION_ENABLE_HTTP_PROTOCOL, (LPVOID)&dwProtocolSupport, sizeof(dwProtocolSupport));
	return hSession;
}

/*
 * Download a file or fill a buffer from an URL
 * Mostly taken from http://support.microsoft.com/kb/234913
 * If file is NULL, a buffer is allocated for the download (that needs to be freed by the caller)
 * If hProgressDialog is not NULL, this function will send INIT and EXIT messages
 * to the dialog in question, with WPARAM being set to nonzero for EXIT on success
 * and also attempt to indicate progress using an IDC_PROGRESS control
 * Note that when a buffer is used, the actual size of the buffer is two more than its reported
 * size (with the extra bytes set to 0) to accommodate for calls that need NUL-terminated data.
 */
uint64_t DownloadToFileOrBufferEx(const char* url, const char* file, const char* user_agent,
	BYTE** buffer, HWND hProgressDialog, BOOL bTaskBarProgress)
{
	const char* accept_types[] = {"*/*\0", NULL};
	const char* short_name;
	unsigned char buf[DOWNLOAD_BUFFER_SIZE];
	char hostname[64], urlpath[128], strsize[32];
	BOOL r = FALSE, use_github_api;
	DWORD dwSize, dwWritten, dwDownloaded;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	HINTERNET hSession = NULL, hConnection = NULL, hRequest = NULL;
	URL_COMPONENTSA UrlParts = { sizeof(URL_COMPONENTSA), NULL, 1, (INTERNET_SCHEME)0,
		hostname, sizeof(hostname), 0, NULL, 1, urlpath, sizeof(urlpath), NULL, 1 };
	uint64_t size = 0, total_size = 0;

	ErrorStatus = 0;
	DownloadStatus = 404;
	// Force-fail networking and use local dbx instead (port)
	if (WindowsVersion.Version < WINDOWS_VISTA) {
		if (buffer != NULL)
			*buffer = NULL;
		SetLastError(ERROR_NOT_SUPPORTED);
		return 0;
	}
	if (hProgressDialog != NULL)
		UpdateProgressWithInfoInit(hProgressDialog, FALSE);

	assert(url != NULL);
	if (buffer != NULL)
		*buffer = NULL;

	short_name = (file != NULL) ? PathFindFileNameU(file) : PathFindFileNameU(url);

	if (hProgressDialog != NULL) {
		PrintInfo(5000, MSG_085, short_name);
		uprintf("Downloading %s", url);
	}

	if ( (!InternetCrackUrlA(url, (DWORD)safe_strlen(url), 0, &UrlParts))
	  || (UrlParts.lpszHostName == NULL) || (UrlParts.lpszUrlPath == NULL)) {
		uprintf("Unable to decode URL: %s", WindowsErrorString());
		goto out;
	}
	hostname[sizeof(hostname)-1] = 0;

	hSession = GetInternetSession(user_agent, TRUE);
	if (hSession == NULL) {
		uprintf("Could not open Internet session: %s", WindowsErrorString());
		goto out;
	}

	hConnection = InternetConnectA(hSession, UrlParts.lpszHostName, UrlParts.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)NULL);
	if (hConnection == NULL) {
		uprintf("Could not connect to server %s:%d: %s", UrlParts.lpszHostName, UrlParts.nPort, WindowsErrorString());
		goto out;
	}

	hRequest = HttpOpenRequestA(hConnection, "GET", UrlParts.lpszUrlPath, NULL, NULL, accept_types,
		INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
		INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_HYPERLINK |
		((UrlParts.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0), (DWORD_PTR)NULL);
	if (hRequest == NULL) {
		uprintf("Could not open URL %s: %s", url, WindowsErrorString());
		goto out;
	}

	// If we are querying the GitHub API, we need to enable raw content and
	// set 'Accept-Encoding' to 'none' to get the data length.
	use_github_api = (strstr(url, "api.github.com") != NULL);
	if (use_github_api && !HttpAddRequestHeadersA(hRequest, "Accept: application/vnd.github.v3.raw",
		(DWORD)-1, HTTP_ADDREQ_FLAG_ADD)) {
		uprintf("Unable to enable raw content from GitHub API: %s", WindowsErrorString());
		goto out;
	}
	if (!HttpSendRequestA(hRequest, request_headers[use_github_api ? 0 : 1], -1L, NULL, 0)) {
		uprintf("Unable to send request: %s", WindowsErrorString());
		goto out;
	}

	// Get the file size
	dwSize = sizeof(DownloadStatus);
	HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, (LPVOID)&DownloadStatus, &dwSize, NULL);
	if (DownloadStatus != 200) {
		error_code = ERROR_INTERNET_ITEM_NOT_FOUND;
		SetLastError(RUFUS_ERROR(error_code));
		uprintf("%s '%s': %d", (DownloadStatus == 404) ? "File not found" : "Unable to access file", url, DownloadStatus);
		goto out;
	}
	dwSize = sizeof(strsize);
	if (!HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH, (LPVOID)strsize, &dwSize, NULL)) {
		// Note: The following line
		// len = DownloadToFileOrBuffer(RUFUS_URL "/sbat_level.txt", NULL, (BYTE**)&sbat_level_txt, NULL, FALSE);
		// Causes the error down below on Windows 7 and Vista. I dont know why it fails and I'm too lazy to find care
		// sbat has a hardcoded fallback and uses dbx files anyways
		uprintf("Unable to retrieve file length: %s", WindowsErrorString());
		goto out;
	}
	total_size = strtoull(strsize, NULL, 10);
	if (hProgressDialog != NULL) {
		char msg[128];
		uprintf("File length: %s", SizeToHumanReadable(total_size, FALSE, FALSE));
		if (right_to_left_mode)
			static_sprintf(msg, "(%s) %s", SizeToHumanReadable(total_size, FALSE, FALSE), GetShortName(url));
		else
			static_sprintf(msg, "%s (%s)", GetShortName(url), SizeToHumanReadable(total_size, FALSE, FALSE));
		PrintStatus(5000, MSG_085, msg);
	}

	if (file != NULL) {
		hFile = CreateFileU(file, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			uprintf("Unable to create file '%s': %s", short_name, WindowsErrorString());
			goto out;
		}
	} else {
		if (buffer == NULL) {
			uprintf("No buffer pointer provided for download");
			goto out;
		}
		// Allocate one extra byte, so that caller can rely on NUL-terminated text if needed
		*buffer = calloc((size_t)total_size + 2, 1);
		if (*buffer == NULL) {
			uprintf("Could not allocate buffer for download");
			goto out;
		}
	}

	// Keep checking for data until there is nothing left.
	while (1) {
		// User may have cancelled the download
		if (IS_ERROR(ErrorStatus))
			goto out;
		if (!InternetReadFile(hRequest, buf, sizeof(buf), &dwDownloaded) || (dwDownloaded == 0))
			break;
		if (hProgressDialog != NULL)
			UpdateProgressWithInfo(OP_NOOP, MSG_241, size, total_size);
		if (file != NULL) {
			if (!WriteFile(hFile, buf, dwDownloaded, &dwWritten, NULL)) {
				uprintf("Error writing file '%s': %s", short_name, WindowsErrorString());
				goto out;
			} else if (dwDownloaded != dwWritten) {
				uprintf("Error writing file '%s': Only %d/%d bytes written", short_name, dwWritten, dwDownloaded);
				goto out;
			}
		} else {
			memcpy(&(*buffer)[size], buf, dwDownloaded);
		}
		size += dwDownloaded;
	}

	if (size != total_size) {
		uprintf("Could not download complete file - read: %lld bytes, expected: %lld bytes", size, total_size);
		ErrorStatus = RUFUS_ERROR(ERROR_WRITE_FAULT);
		goto out;
	} else {
		DownloadStatus = 200;
		r = TRUE;
		if (hProgressDialog != NULL) {
			UpdateProgressWithInfo(OP_NOOP, MSG_241, total_size, total_size);
			uprintf("Successfully downloaded '%s'", short_name);
		}
	}

out:
	error_code = GetLastError();
	if (hFile != INVALID_HANDLE_VALUE) {
		// Force a flush - May help with the PKI API trying to process downloaded updates too early...
		FlushFileBuffers(hFile);
		CloseHandle(hFile);
	}
	if (!r) {
		if (file != NULL)
			DeleteFileU(file);
		if (buffer != NULL)
			safe_free(*buffer);
	}
	if (hRequest)
		InternetCloseHandle(hRequest);
	if (hConnection)
		InternetCloseHandle(hConnection);
	if (hSession)
		InternetCloseHandle(hSession);

	SetLastError(error_code);
	return r ? size : 0;
}

// Download and validate a signed file. The file must have a corresponding '.sig' on the server.
DWORD DownloadSignedFile(const char* url, const char* file, HWND hProgressDialog, BOOL bPromptOnError)
{
	char* url_sig = NULL;
	BYTE *buf = NULL, *sig = NULL;
	DWORD buf_len = 0, sig_len = 0;
	DWORD ret = 0;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	assert(url != NULL);

	url_sig = malloc(strlen(url) + 5);
	if (url_sig == NULL) {
		uprintf("Could not allocate signature URL");
		goto out;
	}
	strcpy(url_sig, url);
	strcat(url_sig, ".sig");

	buf_len = (DWORD)DownloadToFileOrBuffer(url, NULL, &buf, hProgressDialog, FALSE);
	if (buf_len == 0)
		goto out;
	sig_len = (DWORD)DownloadToFileOrBuffer(url_sig, NULL, &sig, NULL, FALSE);
	if ((sig_len != RSA_SIGNATURE_SIZE) || (!ValidateOpensslSignature(buf, buf_len, sig, sig_len))) {
		uprintf("FATAL: Download signature is invalid ✗");
		DownloadStatus = 403;	// Forbidden
		ErrorStatus = RUFUS_ERROR(APPERR(ERROR_BAD_SIGNATURE));
		SendMessage(GetDlgItem(hProgressDialog, IDC_PROGRESS), PBM_SETSTATE, (WPARAM)PBST_ERROR, 0);
		SetTaskbarProgressState(TASKBAR_ERROR);
		goto out;
	}

	uprintf("Download signature is valid ✓");
	DownloadStatus = 206;	// Partial content
	hFile = CreateFileU(file, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		uprintf("Unable to create file '%s': %s", PathFindFileNameU(file), WindowsErrorString());
		goto out;
	}
	if (!WriteFile(hFile, buf, buf_len, &ret, NULL)) {
		uprintf("Error writing file '%s': %s", PathFindFileNameU(file), WindowsErrorString());
		ret = 0;
		goto out;
	} else if (ret != buf_len) {
		uprintf("Error writing file '%s': Only %d/%d bytes written", PathFindFileNameU(file), ret, buf_len);
		ret = 0;
		goto out;
	}
	DownloadStatus = 200;	// Full content

out:
	if (hProgressDialog != NULL)
		SendMessage(hProgressDialog, UM_PROGRESS_EXIT, (WPARAM)ret, 0);
	if ((bPromptOnError) && (DownloadStatus != 200)) {
		PrintInfo(0, MSG_242);
		SetLastError(error_code);
		MessageBoxExU(hMainDialog, IS_ERROR(ErrorStatus) ? StrError(ErrorStatus, FALSE) : WindowsErrorString(),
			lmprintf(MSG_044), MB_OK | MB_ICONERROR | MB_IS_RTL, selected_langid);
	}
	safe_closehandle(hFile);
	free(url_sig);
	free(buf);
	free(sig);
	return ret;
}

/* Threaded download */
typedef struct {
	const char* url;
	const char* file;
	HWND hProgressDialog;
	BOOL bPromptOnError;
} DownloadSignedFileThreadArgs;

static DWORD WINAPI DownloadSignedFileThread(LPVOID param)
{
	DownloadSignedFileThreadArgs* args = (DownloadSignedFileThreadArgs*)param;
	ExitThread(DownloadSignedFile(args->url, args->file, args->hProgressDialog, args->bPromptOnError));
}

HANDLE DownloadSignedFileThreaded(const char* url, const char* file, HWND hProgressDialog, BOOL bPromptOnError)
{
	static DownloadSignedFileThreadArgs args;
	args.url = url;
	args.file = file;
	args.hProgressDialog = hProgressDialog;
	args.bPromptOnError = bPromptOnError;
	return CreateThread(NULL, 0, DownloadSignedFileThread, &args, 0, NULL);
}

static __inline uint64_t to_uint64_t(uint16_t x[3]) {
	int i;
	uint64_t ret = 0;
	for (i = 0; i < 3; i++)
		ret = (ret << 16) + x[i];
	return ret;
}

BOOL UseLocalDbx(int arch)
{
	char reg_name[32], path[MAX_PATH];
	if (WindowsVersion.Version < WINDOWS_VISTA) {
		static_sprintf(path, "%s\\%s\\dbx_%s.bin", app_data_dir, FILES_DIR, efi_archname[arch]);
		return _accessU(path, 0) != -1;
	}
	static_sprintf(reg_name, "DBXTimestamp_%s", efi_archname[arch]);
	return (uint64_t)ReadSetting64(reg_name) > dbx_info[arch - 1].timestamp;
}

static void CheckForDBXUpdates(int verbose)
{
	int i, r;
	char reg_name[32], timestamp_url[256], path[MAX_PATH];
	char *p, *c, *rep, *buf = NULL;
	struct tm t = { 0 };
	uint64_t size, timestamp;
	BOOL already_prompted = FALSE;

	for (i = 0; i < ARRAYSIZE(dbx_info); i++) {
		// Get the epoch of the last commit
		timestamp = 0;
		static_strcpy(timestamp_url, dbx_info[i].url);
		p = strstr(timestamp_url, "contents/");
		if (p == NULL)
			continue;
		*p = 0;
		rep = replace_char(&p[9], '/', "%2F");
		static_strcat(timestamp_url, "commits?path=");
		static_strcat(timestamp_url, rep);
		free(rep);
		static_strcat(timestamp_url, "&page=1&per_page=1");
		vuprintf("Querying %s for DBX update timestamp", timestamp_url);
		size = DownloadToFileOrBuffer(timestamp_url, NULL, (BYTE**)&buf, NULL, FALSE);
		if (size == 0)
			continue;
		// Assumes that the GitHub JSON commit dates are of the form:
		// "date":[ ]*"2025-02-24T20:20:22Z"
		p = strstr(buf, "\"date\":");
		if (p == NULL) {
			safe_free(buf);
			continue;
		}
		c = &p[7];
		while (*c == ' ' || *c == '"')
			c++;
		p = c;
		while (*c != '"' && *c != '\0')
			c++;
		*c = 0;
		// "Thank you, X3J11 ANSI committee, for introducing the well thought through 'struct tm'", said ABSOLUTELY NOONE ever!
		r = sscanf(p, "%d-%d-%dT%d:%d:%dZ", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
		safe_free(buf);
		if (r != 6)
			continue;
		t.tm_year -= 1900;
		t.tm_mon -= 1;
		timestamp = _mktime64(&t);
		vuprintf("DBX update timestamp is %" PRId64, timestamp);
		static_sprintf(reg_name, "DBXTimestamp_%s", efi_archname[i + 1]);
		// Check if we have an external DBX that is newer than embedded/last downloaded
		if (timestamp <= MAX(dbx_info[i].timestamp, (uint64_t)ReadSetting64(reg_name)))
			continue;
		if (!already_prompted) {
			r = MessageBoxExU(hMainDialog, lmprintf(MSG_354), lmprintf(MSG_353),
				MB_YESNO | MB_ICONWARNING | MB_IS_RTL, selected_langid);
			already_prompted = TRUE;
			if (r != IDYES)
				break;
			static_sprintf(path, "%s\\%s", app_data_dir, FILES_DIR);
			if ((_mkdirU(path) != 0) && (errno != EEXIST)) {
				uprintf("Warning: Could not create DBX update directory '%s'", path);
				break;
			}
		}
		static_sprintf(path, "%s\\%s\\dbx_%s.bin", app_data_dir, FILES_DIR, efi_archname[i + 1]);
		if (DownloadToFileOrBuffer(dbx_info[i].url, path, NULL, NULL, FALSE) != 0) {
			WriteSetting64(reg_name, timestamp);
			uprintf("Saved %s as 'dbx_%s.bin'", dbx_info[i].url, efi_archname[i + 1]);
		} else
			uprintf("Warning: Failed to download %s", dbx_info[i].url);
	}
}

/*
 * Background thread to check for updates (including UEFI DBX updates)
 */
static DWORD WINAPI CheckForUpdatesThread(LPVOID param)
{
	BOOL releases_only = TRUE, found_new_version = FALSE;
	int status = 0;
	const char* server_url = RUFUS_URL "/";
	int i, j, k, max_channel, verbose = 0, verpos[4];
	static const char* channel[] = { "release", "beta", "test" };		// release channel
	const char* accept_types[] = { "*/*\0", NULL };
	char* buf = NULL;
	// Changed urlpath from 128 to 1024, same as in IsDownloadable()
	char agent[64], hostname[64], urlpath[1024], sigpath[256];
	DWORD dwSize, dwDownloaded, dwTotalSize, dwStatus;
	BYTE *sig = NULL;
	HINTERNET hSession = NULL, hConnection = NULL, hRequest = NULL;
	URL_COMPONENTSA UrlParts = { sizeof(URL_COMPONENTSA), NULL, 1, (INTERNET_SCHEME)0,
		hostname, sizeof(hostname), 0, NULL, 1, urlpath, sizeof(urlpath), NULL, 1 };
	SYSTEMTIME ServerTime, LocalTime;
	FILETIME FileTime;
	int64_t local_time = 0, reg_time, server_time, update_interval;
	verbose = ReadSetting32(SETTING_VERBOSE_UPDATES);
	// Without this the FileDialog will produce error 0x8001010E when compiled for Vista or later
	IGNORE_RETVAL(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
	// Unless the update was forced, wait a while before performing the update check
	if (!force_update_check) {
		// It would of course be a lot nicer to use a timer and wake the thread, but my
		// development time is limited and this is FASTER to implement.
		do {
			for (i = 0; ( i < 30) && (!force_update_check); i++)
				Sleep(500);
		} while ((!force_update_check) && ((op_in_progress || (dialog_showing > 0))));
		if (!force_update_check) {
			if ((ReadSetting32(SETTING_UPDATE_INTERVAL) == -1)) {
				vuprintf("Check for updates disabled, as per settings.");
				goto out;
			}
			reg_time = ReadSetting64(SETTING_LAST_UPDATE);
			update_interval = (int64_t)ReadSetting32(SETTING_UPDATE_INTERVAL);
			if (update_interval == 0) {
				WriteSetting32(SETTING_UPDATE_INTERVAL, DEFAULT_UPDATE_INTERVAL);
				update_interval = DEFAULT_UPDATE_INTERVAL;
			}
			GetSystemTime(&LocalTime);
			if (!SystemTimeToFileTime(&LocalTime, &FileTime))
				goto out;
			local_time = ((((int64_t)FileTime.dwHighDateTime) << 32) + FileTime.dwLowDateTime) / 10000000;
			vvuprintf("Local time: %" PRId64, local_time);
			if (local_time < reg_time + update_interval) {
				vuprintf("Next update check in %" PRId64 " seconds.", reg_time + update_interval - local_time);
				goto out;
			}
		}
	}

	// Perform the DBX Update check
	PrintInfoDebug(3000, MSG_352);
	CheckForDBXUpdates(verbose);

	PrintInfoDebug(3000, MSG_243);
	status++;	// 1

	if (!InternetCrackUrlA(server_url, (DWORD)safe_strlen(server_url), 0, &UrlParts))
		goto out;
	hostname[sizeof(hostname)-1] = 0;

	static_sprintf(agent, APPLICATION_NAME "/%d.%d.%d (Windows NT %lu.%lu%s)",
		rufus_version[0], rufus_version[1], rufus_version[2],
		WindowsVersion.Major, WindowsVersion.Minor, is_WOW64() ? "; WOW64" : "");
	hSession = GetInternetSession(NULL, FALSE);
	if (hSession == NULL)
		goto out;
	hConnection = InternetConnectA(hSession, UrlParts.lpszHostName, UrlParts.nPort,
		NULL, NULL, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)NULL);
	if (hConnection == NULL)
		goto out;

	status++;	// 2
	// BETAs are only made available when the application arch is x86_64
	if (is_x86_64)
		releases_only = !ReadSettingBool(SETTING_INCLUDE_BETAS);

	// Test releases get their own distribution channel (and also force beta checks)
#if defined(TEST)
	max_channel = (int)ARRAYSIZE(channel);
#else
	max_channel = releases_only ? 1 : (int)ARRAYSIZE(channel) - 1;
#endif
	vuprintf("Using %s for the update check", RUFUS_URL);
	for (k = 0; (k < max_channel) && (!found_new_version); k++) {
		// Get the arch name and convert it lowercase
		char* archname = strdup(GetArchName(WindowsVersion.Arch));
		safe_strtolower(archname);
		// Free any previous buffers we might have used
		safe_free(buf);
		safe_free(sig);
		uprintf("Checking %s channel...", channel[k]);
		// At this stage we can query the server for various update version files.
		// We first try to lookup for "<appname>_<os_arch>_<os_version_major>_<os_version_minor>.ver"
		// and then remove each of the <os_> components until we find our match. For instance, we may first
		// look for rufus_win_x64_6.2.ver (Win8 x64) but only get a match for rufus_win_x64_6.ver (Vista x64 or later)
		// This allows sunsetting OS versions (eg XP) or providing different downloads for different archs/groups.
		// Note that for BETAs, we only catter for x64 regardless of the OS arch.
		static_sprintf(urlpath, "%s%s%s_win_%s_%lu.%lu.ver", APPLICATION_NAME, (k == 0) ? "": "_",
			(k == 0) ? "" : channel[k], archname, WindowsVersion.Major, WindowsVersion.Minor);
		safe_free(archname);
		vuprintf("Base update check: %s", urlpath);
		for (i = 0, j = (int)safe_strlen(urlpath) - 5; (j > 0) && (i < ARRAYSIZE(verpos)); j--) {
			if ((urlpath[j] == '.') || (urlpath[j] == '_')) {
				verpos[i++] = j;
			}
		}
		assert(i == ARRAYSIZE(verpos));

		UrlParts.lpszUrlPath = urlpath;
		UrlParts.dwUrlPathLength = sizeof(urlpath);
		for (i = 0; i < ARRAYSIZE(verpos); i++) {
			vvuprintf("Trying %s", UrlParts.lpszUrlPath);
			hRequest = HttpOpenRequestA(hConnection, "GET", UrlParts.lpszUrlPath, NULL, NULL, accept_types,
				INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
				INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_HYPERLINK |
				((UrlParts.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0), (DWORD_PTR)NULL);
			if ((hRequest == NULL) || (!HttpSendRequestA(hRequest,
				request_headers[1], -1L, NULL, 0))) {
				uprintf("Unable to send request: %s", WindowsErrorString());
				goto out;
			}

			// Ensure that we get a text file
			dwSize = sizeof(dwStatus);
			dwStatus = 404;
			HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER, (LPVOID)&dwStatus, &dwSize, NULL);
			if (dwStatus == 200)
				break;
			InternetCloseHandle(hRequest);
			hRequest = NULL;
			safe_strcpy(&urlpath[verpos[i]], 5, ".ver");
		}
		if (dwStatus != 200) {
			vuprintf("Could not find a %s version file on server %s", channel[k], server_url);
			if ((releases_only) || (k + 1 >= ARRAYSIZE(channel)))
				goto out;
			continue;
		}
		vuprintf("Found match for %s on server %s", urlpath, server_url);

		// We also get a date from the web server, which we'll use to avoid out of sync check,
		// in case some set their clock way into the future and back.
		// On the other hand, if local clock is set way back in the past, we will never check.
		dwSize = sizeof(ServerTime);
		// If we can't get a date we can trust, don't bother...
		if ( (!HttpQueryInfoA(hRequest, HTTP_QUERY_DATE|HTTP_QUERY_FLAG_SYSTEMTIME, (LPVOID)&ServerTime, &dwSize, NULL))
			|| (!SystemTimeToFileTime(&ServerTime, &FileTime)) )
			goto out;
		server_time = ((((int64_t)FileTime.dwHighDateTime) << 32) + FileTime.dwLowDateTime) / 10000000;
		vvuprintf("Server time: %" PRId64, server_time);
		// Always store the server response time - the only clock we trust!
		WriteSetting64(SETTING_LAST_UPDATE, server_time);
		// Might as well let the user know
		if (!force_update_check) {
			if ((local_time > server_time + 600) || (local_time < server_time - 600)) {
				uprintf("IMPORTANT: Your local clock is more than 10 minutes in the %s. Unless you fix this, "
					APPLICATION_NAME " may not be able to check for updates...",
					(local_time > server_time + 600)?"future":"past");
			}
		}

		dwSize = sizeof(dwTotalSize);
		if (!HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER, (LPVOID)&dwTotalSize, &dwSize, NULL))
			goto out;

		// Make sure the file is NUL terminated
		buf = (char*)calloc(dwTotalSize + 1, 1);
		if (buf == NULL)
			goto out;
		// This is a version file - we should be able to gulp it down in one go
		if (!InternetReadFile(hRequest, buf, dwTotalSize, &dwDownloaded) || (dwDownloaded != dwTotalSize))
			goto out;
		vuprintf("Successfully downloaded version file (%d bytes)", dwTotalSize);

		// Now download the signature file
		static_sprintf(sigpath, "%s/%s.sig", server_url, urlpath);
		dwDownloaded = (DWORD)DownloadToFileOrBuffer(sigpath, NULL, &sig, NULL, FALSE);
		if ((dwDownloaded != RSA_SIGNATURE_SIZE) || (!ValidateOpensslSignature(buf, dwTotalSize, sig, dwDownloaded))) {
			uprintf("FATAL: Version signature is invalid ✗");
			goto out;
		}
		vuprintf("Version signature is valid ✓");

		status++;
		parse_update(buf, dwTotalSize + 1);

		vuprintf("UPDATE DATA:");
		vuprintf("  version: %d.%d.%d (%s)", update.version[0], update.version[1], update.version[2], channel[k]);
		vuprintf("  platform_min: %d.%d", update.platform_min[0], update.platform_min[1]);
		vuprintf("  url: %s", update.download_url);

		found_new_version = ((to_uint64_t(update.version) > to_uint64_t(rufus_version)) || (force_update))
			&& ((WindowsVersion.Major > update.platform_min[0])
				|| ((WindowsVersion.Major == update.platform_min[0]) && (WindowsVersion.Minor >= update.platform_min[1])));
		uprintf("N%sew %s version found%c", found_new_version ? "" : "o n", channel[k], found_new_version ? '!' : '.');
	}

out:
	safe_free(buf);
	safe_free(sig);
	if (hRequest)
		InternetCloseHandle(hRequest);
	if (hConnection)
		InternetCloseHandle(hConnection);
	if (hSession)
		InternetCloseHandle(hSession);
	switch (status) {
	case 1:
		PrintInfoDebug(3000, MSG_244);
		break;
	case 2:
		// MSG_245 -> MSG_247, updates are disabled anyways, so show no updates available for aesthetics lol (port)
		PrintInfoDebug(3000, MSG_247);
		break;
	case 3:
	case 4:
		PrintInfo(3000, found_new_version ? MSG_246 : MSG_247);
	default:
		break;
	}
	// Start the new download after cleanup
	if (found_new_version) {
		// User may have started an operation while we were checking
		while ((!force_update_check) && (op_in_progress || (dialog_showing > 0))) {
			Sleep(15000);
		}
		DownloadNewVersion();
	} else if (force_update_check) {
		PostMessage(hMainDialog, UM_NO_UPDATE, 0, 0);
	}
	force_update_check = FALSE;
	update_check_thread = NULL;
	CoUninitialize();
	ExitThread(0);
}

/*
 * Initiate a check for updates. If force is true, ignore the wait period
 */
BOOL CheckForUpdates(BOOL force)
{
	// Disable networking on NT5 (port)
	if (WindowsVersion.Version < WINDOWS_VISTA)
		return FALSE;
	force_update_check = force;
	if (update_check_thread != NULL)
		return FALSE;

	update_check_thread = CreateThread(NULL, 0, CheckForUpdatesThread, NULL, 0, NULL);
	if (update_check_thread == NULL) {
		uprintf("Unable to start update check thread");
		return FALSE;
	}
	return TRUE;
}

/*
 * Download an ISO through Fido
 */
static DWORD WINAPI DownloadISOThread(LPVOID param)
{
	BOOL is_vista = (WindowsVersion.Version == WINDOWS_VISTA);
	BOOL is_seven = (WindowsVersion.Version == WINDOWS_7);
	char locale_str[1024], cmdline[sizeof(locale_str) + 512], pipe[MAX_GUID_STRING_LENGTH + 16] = "\\\\.\\pipe\\";
	char powershell_path[MAX_PATH], icon_path[MAX_PATH] = { 0 }, script_path[MAX_PATH] = { 0 };
	char found_pwsh[MAX_PATH] = { 0 };
	char *url = NULL, sig_url[128];
	const char* selected_powershell = NULL;
	uint64_t uncompressed_size;
	int64_t size = -1;
	BYTE *compressed = NULL, *sig = NULL;
	HANDLE hFile, hPipe;
	DWORD dwExitCode = 99, dwCompressedSize, dwSize, dwAvail, dwPipeSize = 4096;
	LARGE_INTEGER patch_pos;
	char *version_line, *version_eol;
	GUID guid;

	dialog_showing++;
	IGNORE_RETVAL(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
	// Disable Fido on NT5 (port)
	if (WindowsVersion.Version < WINDOWS_VISTA)
		goto out;

	// Use a GUID as random unique string, else ill-intentioned security "researchers"
	// may either spam our pipe or replace our script to fool antivirus solutions into
	// thinking that Rufus is doing something malicious...
	IGNORE_RETVAL(CoCreateGuid(&guid));
	// coverity[fixed_size_dest]
	strcpy(&pipe[9], GuidToString(&guid, TRUE));
	static_sprintf(icon_path, "%s%s.ico", temp_dir, APPLICATION_NAME);
	ExtractAppIcon(icon_path, TRUE);

//#define FORCE_URL "https://github.com/pbatard/rufus/raw/master/res/loc/test/windows_to_go.iso"
//#define FORCE_URL "https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/debian-9.8.0-amd64-netinst.iso"
#if !defined(FORCE_URL)
#if defined(RUFUS_TEST)
	IGNORE_RETVAL(hFile);
	IGNORE_RETVAL(sig_url);
	IGNORE_RETVAL(dwCompressedSize);
	IGNORE_RETVAL(uncompressed_size);
	// In test mode, just use our local script
	static_strcpy(script_path, "D:\\Projects\\Fido\\Fido.ps1");
#else
	// If we don't have the script, download it
	if (fido_script == NULL) {
		dwCompressedSize = (DWORD)DownloadToFileOrBuffer(fido_url, NULL, &compressed, hMainDialog, FALSE);
		if (dwCompressedSize == 0)
			goto out;
		static_sprintf(sig_url, "%s.sig", fido_url);
		dwSize = (DWORD)DownloadToFileOrBuffer(sig_url, NULL, &sig, NULL, FALSE);
		if ((dwSize != RSA_SIGNATURE_SIZE) || (!ValidateOpensslSignature(compressed, dwCompressedSize, sig, dwSize))) {
			uprintf("FATAL: Download signature is invalid ✗");
			ErrorStatus = RUFUS_ERROR(APPERR(ERROR_BAD_SIGNATURE));
			SendMessage(hProgress, PBM_SETSTATE, (WPARAM)PBST_ERROR, 0);
			SetTaskbarProgressState(TASKBAR_ERROR);
			safe_free(compressed);
			free(sig);
			goto out;
		}
		free(sig);
		uprintf("Download signature is valid ✓");
		uncompressed_size = *((uint64_t*)&compressed[5]);
		if ((uncompressed_size < 1 * MB) && (bled_init(0, uprintf, NULL, NULL, NULL, NULL, &ErrorStatus) >= 0)) {
			fido_script = malloc((size_t)uncompressed_size);
			size = bled_uncompress_from_buffer_to_buffer(compressed, dwCompressedSize, fido_script, (size_t)uncompressed_size, BLED_COMPRESSION_LZMA);
			bled_exit();
		}
		safe_free(compressed);
		if (size != uncompressed_size) {
			uprintf("FATAL: Could not uncompressed download script");
			safe_free(fido_script);
			ErrorStatus = RUFUS_ERROR(ERROR_INVALID_DATA);
			SendMessage(hProgress, PBM_SETSTATE, (WPARAM)PBST_ERROR, 0);
			SetTaskbarProgressState(TASKBAR_ERROR);
			goto out;
		}
		fido_len = (DWORD)size;

		SendMessage(hProgress, PBM_SETSTATE, (WPARAM)PBST_NORMAL, 0);
		SetTaskbarProgressState(TASKBAR_NORMAL);
		SetTaskbarProgressValue(0, MAX_PROGRESS);
		SendMessage(hProgress, PBM_SETPOS, 0, 0);
	}
	PrintInfo(0, MSG_148);

	assert((fido_script != NULL) && (fido_len != 0));

	static_sprintf(script_path, "%s%s.ps1", temp_dir, GuidToString(&guid, TRUE));
	hFile = CreateFileU(script_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		uprintf("Unable to create download script '%s': %s", script_path, WindowsErrorString());
		goto out;
	}
	if ((!WriteFile(hFile, fido_script, fido_len, &dwSize, NULL)) || (dwSize != fido_len)) {
		uprintf("Unable to write download script '%s': %s", script_path, WindowsErrorString());
		goto out;
	}
	// Why oh why does PowerShell refuse to open read-only files that haven't been closed?
	// Because of this limitation, we can't use LockFileEx() on the file we create...
	safe_closehandle(hFile);
	if (ValidateSignature(INVALID_HANDLE_VALUE, script_path) != NO_ERROR) {
		uprintf("FATAL: Script signature is invalid ✗");
		ErrorStatus = RUFUS_ERROR(APPERR(ERROR_BAD_SIGNATURE));
		SendMessage(hProgress, PBM_SETSTATE, (WPARAM)PBST_ERROR, 0);
		SetTaskbarProgressState(TASKBAR_ERROR);
		goto out;
	}
	uprintf("Script signature is valid ✓");

	// Fido patch signature bypass (idk if its still relevant, this codebase is bullshit) (port)
	if (is_vista || is_seven) {
		version_line = strstr((char*)fido_script, "$winver =");
		version_eol = (version_line == NULL) ? NULL : strchr(version_line, '\n');
		if ((version_eol != NULL) && (version_eol - version_line >= 17)) {
			hFile = CreateFileU(script_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			patch_pos.QuadPart = version_line - (char*)fido_script;
			if ((hFile == INVALID_HANDLE_VALUE) || !SetFilePointerEx(hFile, patch_pos, NULL, FILE_BEGIN) ||
				!WriteFile(hFile, "$winver = 10.0; #", 17, &dwSize, NULL) || (dwSize != 17)) {
				uprintf("Could not prepare Fido for PowerShell 7: %s", WindowsErrorString());
				safe_closehandle(hFile);
				goto out;
			}
			safe_closehandle(hFile);
			duprintf("Adjusted Fido version check for PowerShell 7");
		}
	}
	SetFileAttributesU(script_path, FILE_ATTRIBUTE_READONLY);
#endif
	static_sprintf(powershell_path, "%s\\WindowsPowerShell\\v1.0\\powershell.exe", system_dir);
	static_sprintf(locale_str, "%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s",
		selected_locale->txt[0], lmprintf(MSG_135), lmprintf(MSG_136), lmprintf(MSG_137),
		lmprintf(MSG_138), lmprintf(MSG_139), lmprintf(MSG_040), lmprintf(MSG_140), lmprintf(MSG_141),
		lmprintf(MSG_006), lmprintf(MSG_007), lmprintf(MSG_042), lmprintf(MSG_142), lmprintf(MSG_143),
		lmprintf(MSG_144), lmprintf(MSG_145), lmprintf(MSG_146), lmprintf(MSG_199));

	hPipe = CreateNamedPipeA(pipe, PIPE_ACCESS_INBOUND,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
		dwPipeSize, dwPipeSize, 0, NULL);
	if (hPipe == INVALID_HANDLE_VALUE) {
		uprintf("Could not create pipe '%s': %s", pipe, WindowsErrorString());
		goto out;
	}

	// External Powershell for Fido on 7 and Vista (port)
	selected_powershell = (WindowsVersion.Version >= WINDOWS_8) ? powershell_path : NULL;
	if (selected_powershell == NULL) {
		if (is_vista) {
			HKEY hKey;
			BOOL update_found = FALSE;

			// All rollups that include TLS 1.2
			const char* valid_kbs[] = {
				"KB4019276", "KB4056564", "KB4103725", "KB4284826", "KB4338815", "KB4343899",
				"KB4458010", "KB4462926", "KB4467695", "KB4471320", "KB4480964", "KB4487018",
				"KB4489880", "KB4493467", "KB4499175", "KB4503269", "KB4507452", "KB4512491",
				"KB4516033", "KB4520009", "KB4525234", "KB4530692", "KB4534303", "KB4537810",
				"KB4541506", "KB4550951", "KB4556836", "KB4561670", "KB4565536", "KB4571723",
				"KB4577064", "KB4580346", "KB4586807", "KB4592498", "KB4598275", "KB4601360",
				"KB5000844", "KB5001339", "KB5003192", "KB5003661", "KB5004299", "KB5005030",
				"KB5005607", "KB5006669", "KB5007206", "KB5008244", "KB5009586", "KB5010384",
				"KB5011534", "KB5012632", "KB5014006", "KB5014739", "KB5015861", "KB5016686",
				"KB5017305", "KB5018413", "KB5020019", "KB5021289", "KB5022340", "KB5022874",
				"KB5023759", "KB5025279", "KB5026362", "KB5027279", "KB5028222", "KB5029318",
				"KB5030271", "KB5031416", "KB5032254", "KB5033422", "KB5034173", "KB5034831",
				"KB5035942", "KB5036966", "KB5037778", "KB5039260", "KB5040497", "KB5041838",
				"KB5043051", "KB5044277", "KB5046613", "KB5048685", "KB5050009", "KB5051979",
				"KB5053603", "KB5055609", "KB5058429", "KB5061198", "KB5061026", "KB5062624",
				"KB5063888", "KB5065508", "KB5066874", "KB5068906", "KB5071504", "KB5073697"
			};

			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\Packages", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
				RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\Packages", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
			{
				char pkg_name[MAX_PATH];
				DWORD pkg_index = 0, pkg_len = MAX_PATH;

				while (pkg_len = MAX_PATH, RegEnumKeyExA(hKey, pkg_index++, pkg_name, &pkg_len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
					for (size_t i = 0; i < ARRAYSIZE(valid_kbs); i++) {
						if (strstr(pkg_name, valid_kbs[i]) != NULL) {
							update_found = TRUE;
							break;
						}
					}
					if (update_found) break;
				}
				RegCloseKey(hKey);
			}

			if (!update_found) {
				for (size_t i = 0; i < ARRAYSIZE(valid_kbs); i++) {
					char hotfix_path[256];
					static_sprintf(hotfix_path, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix\\%s", valid_kbs[i]);

					if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, hotfix_path, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
					{
						update_found = TRUE;
						RegCloseKey(hKey);
						break;
					}
				}
			}

			if (!update_found) {
				int response = MessageBoxA(NULL,
					"Windows Vista requires a May 2018 or later security update to support secure TLS 1.2 connections.\n\n"
					"Would you like to open your browser to download the required update from the Microsoft Update Catalog?",
					"Security Update Required",
					MB_YESNO | MB_ICONEXCLAMATION);

				if (response == IDYES) {
					ShellExecuteA(NULL, "open",
						"https://www.catalog.update.microsoft.com/Search.aspx?q=KB4056564",
						NULL, NULL, SW_SHOWNORMAL);
				}
				goto out;
			}

		}

		const char* pwsh_candidates[] = {
			"C:\\Program Files\\PowerShell\\7\\pwsh.exe",
			"C:\\Program Files (x86)\\PowerShell\\7\\pwsh.exe",
			"C:\\Program Files\\PowerShell\\7-preview\\pwsh.exe",
		};

		char reg_subkey[MAX_PATH] = { 0 };
		HKEY hParentKey, hSubKey;
		DWORD subkey_index = 0, subkey_len = MAX_PATH, val_size = sizeof(found_pwsh);

		for (size_t k = 0; k < ARRAYSIZE(pwsh_candidates); k++) {
			if (PathFileExistsA(pwsh_candidates[k])) {
				selected_powershell = pwsh_candidates[k];
				break;
			}
		}

		if (selected_powershell == NULL) {
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\PowerShellCore\\InstalledVersions", 0, KEY_READ, &hParentKey) == ERROR_SUCCESS) {
				while (RegEnumKeyExA(hParentKey, subkey_index++, reg_subkey, &subkey_len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
					subkey_len = sizeof(reg_subkey);
					val_size = sizeof(found_pwsh);
					char full_subkey_path[MAX_PATH];
					static_sprintf(full_subkey_path, "SOFTWARE\\Microsoft\\PowerShellCore\\InstalledVersions\\%s", reg_subkey);
					if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, full_subkey_path, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
						if (RegQueryValueExA(hSubKey, "InstallDir", NULL, NULL, (LPBYTE)found_pwsh, &val_size) == ERROR_SUCCESS) {
							PathCombineA(found_pwsh, found_pwsh, "pwsh.exe");
							if (PathFileExistsA(found_pwsh)) {
								selected_powershell = found_pwsh;
								RegCloseKey(hSubKey);
								break;
							}
						}
						RegCloseKey(hSubKey);
					}
				}
				RegCloseKey(hParentKey);
			}
		}

		if (selected_powershell == NULL) {
			if (SearchPathA(NULL, "pwsh.exe", NULL, MAX_PATH, found_pwsh, NULL) > 0)
				selected_powershell = found_pwsh;
		}

		if ((selected_powershell == NULL) || (is_vista && !PathFileExistsA(selected_powershell))) {
			if (!IsDotNet45OrNewerInstalled()) {
				int response;
				if (is_vista) {
					response = MessageBoxA(NULL,
						"Powershell 7 requires .NET Framework 4.6 or newer to run, but it isn't detected on your system.\n\n"
						"Would you like to open your browser to download .NET Framework 4.6?",
						".NET Framework 4.6 Required",
						MB_YESNO | MB_ICONEXCLAMATION);

					if (response == IDYES) {
						ShellExecuteA(NULL, "open",
							"https://www.microsoft.com/en-us/download/details.aspx?id=48130",
							NULL, NULL, SW_SHOWNORMAL);
					}
				}
				else {
					response = MessageBoxA(NULL,
						"Windows Management Framework (WMF) requires .NET Framework 4.5.2 or newer to be installed, but it isn't detected on your system.\n\n"
						"Would you like to open your browser to download .NET Framework 4.8?",
						".NET Framework 4.5.2+ Required",
						MB_YESNO | MB_ICONEXCLAMATION);

					if (response == IDYES) {
						ShellExecuteA(NULL, "open",
							"https://dotnet.microsoft.com/en-us/download/dotnet-framework/net48",
							NULL, NULL, SW_SHOWNORMAL);
					}
				}
				goto out;
			}

			if (!is_vista && !IsWMF4OrNewerInstalled()) {
				int response = MessageBoxA(NULL,
					"PowerShell needs Windows Management Framework (WMF) 4.0 or newer to run, but your system version is out of date.\n\n"
					"Would you like to open your browser to download WMF 5.1?",
					"WMF 4.0+ Required",
					MB_YESNO | MB_ICONEXCLAMATION);

				if (response == IDYES) {
					ShellExecuteA(NULL, "open",
						"https://www.microsoft.com/en-us/download/details.aspx?id=54616",
						NULL, NULL, SW_SHOWNORMAL);
				}
				goto out;
			}

			int response;
			if (is_vista) {
				response = MessageBoxA(NULL,
					"PowerShell 7 is required to run Fido!\n\n"
					"Windows Vista Extended Kernel, Ver. 10192022 (recommended) or newer is required.\n\n"
					"Would you like to visit forum.legacydev.org to learn how to install Powershell 7 on Windows Vista?",
					"PowerShell 7 Required",
					MB_YESNO | MB_ICONEXCLAMATION);
			} else if (is_seven) {
				response = MessageBoxA(NULL,
					"PowerShell 7 is required to run Fido!\n\n"
					"Would you like to open your browser to download PowerShell 7.2.24?",
					"PowerShell 7 Required",
					MB_YESNO | MB_ICONEXCLAMATION);
			}
			else {
				response = MessageBoxA(NULL,
				"PowerShell 7 is required to run Fido!\n\n"
					"Would you like to open your browser to download PowerShell 7?",
					"PowerShell 7 Required",
					MB_YESNO | MB_ICONEXCLAMATION);
			}

			if (response == IDYES) {
				if (is_vista) {
					ShellExecuteA(NULL, "open",
						"https://forum.legacydev.org/viewtopic.php?t=231",
						NULL, NULL, SW_SHOWNORMAL);
				} else if (is_seven) {
					ShellExecuteA(NULL, "open",
						"https://github.com/PowerShell/PowerShell/releases/download/v7.2.24/PowerShell-7.2.24-win-x64.msi",
						NULL, NULL, SW_SHOWNORMAL);
				} else {
					ShellExecuteA(NULL, "open",
						"https://github.com/PowerShell/powershell/releases/latest",
						NULL, NULL, SW_SHOWNORMAL);
				}
			}

			goto out;
		}
	}

	if (is_vista) {
		// Written with Powershell 7.2.2 running through the extended kernel in mind
		// Credits go to TSNH https://forum.legacydev.org/viewtopic.php?t=231
		static_sprintf(cmdline,
			"cmd.exe /c pwsh.exe -Command \""
			"$host.UI.RawUI.WindowTitle = 'PowerShell 7'; "
			"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
			"if ((Get-ExecutionPolicy) -ne 'AllSigned') { Set-ExecutionPolicy -Scope Process Bypass }; "
			"& '%s' -PipeName '%s' -LocData '%s' -Icon '%s' -AppTitle '%s' -PlatformArch '%s'\"",
			script_path, &pipe[9], locale_str, icon_path, lmprintf(MSG_149), GetArchName(NativeMachine)
		);
	} else {
		static_sprintf(cmdline, "\"%s\" -NonInteractive -Sta -NoProfile -ExecutionPolicy Bypass "
			"-Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; & '%s' -PipeName '%s' -LocData '%s' -Icon '%s' -AppTitle '%s' -PlatformArch '%s'\"",
			selected_powershell, script_path, &pipe[9], locale_str, icon_path, lmprintf(MSG_149), GetArchName(NativeMachine));
	}
	ErrorStatus = 0;
	dwExitCode = RunCommand(cmdline, app_data_dir, TRUE);
	uprintf("Exited download script with code: %d", dwExitCode);
	if ((dwExitCode == 0) && PeekNamedPipe(hPipe, NULL, dwPipeSize, NULL, &dwAvail, NULL) && (dwAvail != 0)) {
		url = malloc(dwAvail + 1);
		dwSize = 0;
		if ((url != NULL) && ReadFile(hPipe, url, dwAvail, &dwSize, NULL) && (dwSize > 4)) {
#else
	{	{	url = strdup(FORCE_URL);
			dwSize = (DWORD)strlen(FORCE_URL);
#endif
			IMG_SAVE img_save = { 0 };
// WTF is wrong with Microsoft's static analyzer reporting a potential buffer overflow here?!?
#if defined(_MSC_VER)
#pragma warning(disable: 6386)
#endif
			url[min(dwSize, dwAvail)] = 0;
#if defined(_MSC_VER)
#pragma warning(default: 6386)
#endif
			EXT_DECL(img_ext, GetShortName(url), __VA_GROUP__("*.iso"), __VA_GROUP__(lmprintf(MSG_036)));
			img_save.Type = VIRTUAL_STORAGE_TYPE_DEVICE_ISO;
			img_save.ImagePath = FileDialog(TRUE, NULL, &img_ext, NULL);
			if (img_save.ImagePath == NULL) {
				goto out;
			}
			// Download the ISO and report errors if any
			SendMessage(hMainDialog, UM_PROGRESS_INIT, 0, 0);
			ErrorStatus = 0;
			SendMessage(hMainDialog, UM_TIMER_START, 0, 0);
			if (DownloadToFileOrBuffer(url, img_save.ImagePath, NULL, hMainDialog, TRUE) == 0) {
				SendMessage(hMainDialog, UM_PROGRESS_EXIT, 0, 0);
				if (SCODE_CODE(ErrorStatus) == ERROR_CANCELLED) {
					uprintf("Download cancelled by user");
					Notification(MSG_INFO, NULL, NULL, lmprintf(MSG_211), lmprintf(MSG_041));
					PrintInfo(0, MSG_211);
				} else {
					Notification(MSG_ERROR, NULL, NULL, lmprintf(MSG_194, GetShortName(url)), lmprintf(MSG_043, WindowsErrorString()));
					PrintInfo(0, MSG_212);
				}
			} else {
				// Download was successful => Select and scan the ISO
				image_path = safe_strdup(img_save.ImagePath);
				PostMessage(hMainDialog, UM_SELECT_ISO, 0, 0);
			}
			safe_free(img_save.ImagePath);
		}
	}

out:
	if (icon_path[0] != 0)
		DeleteFileU(icon_path);
#if !defined(RUFUS_TEST)
	if (script_path[0] != 0) {
		SetFileAttributesU(script_path, FILE_ATTRIBUTE_NORMAL);
		DeleteFileU(script_path);
	}
#endif
	free(url);
	SendMessage(hMainDialog, UM_ENABLE_CONTROLS, 0, 0);
	dialog_showing--;
	CoUninitialize();
	ExitThread(dwExitCode);
}

BOOL DownloadISO()
{
	// Do not expose network-backed ISO downloads before Windows Vista. (port)
	if (WindowsVersion.Version < WINDOWS_VISTA)
		return FALSE;
	if (CreateThread(NULL, 0, DownloadISOThread, NULL, 0, NULL) == NULL) {
		uprintf("Unable to start Windows ISO download thread");
		ErrorStatus = RUFUS_ERROR(APPERR(ERROR_CANT_START_THREAD));
		SendMessage(hMainDialog, UM_ENABLE_CONTROLS, 0, 0);
		return FALSE;
	}
	return TRUE;
}

BOOL IsDownloadable(const char* url)
{
	DWORD dwSize, dwTotalSize = 0;
	const char* accept_types[] = { "*/*\0", NULL };
	// Changed urlpath from 128 to 1024, modern microsoft download URLs reach 400 + characters in some cases
	char hostname[64], urlpath[1024];
	HINTERNET hSession = NULL, hConnection = NULL, hRequest = NULL;
	URL_COMPONENTSA UrlParts = { sizeof(URL_COMPONENTSA), NULL, 1, (INTERNET_SCHEME)0,
		hostname, sizeof(hostname), 0, NULL, 1, urlpath, sizeof(urlpath), NULL, 1 };

	// Do not probe remote URLs before Windows Vista. (port)
	if ((WindowsVersion.Version < WINDOWS_VISTA) || (url == NULL))
		return FALSE;

	ErrorStatus = 0;
	DownloadStatus = 404;

	if ((!InternetCrackUrlA(url, (DWORD)safe_strlen(url), 0, &UrlParts))
		|| (UrlParts.lpszHostName == NULL) || (UrlParts.lpszUrlPath == NULL))
		goto out;
	hostname[sizeof(hostname) - 1] = 0;

	// Open an Internet session
	hSession = GetInternetSession(NULL, FALSE);
	if (hSession == NULL)
		goto out;

	hConnection = InternetConnectA(hSession, UrlParts.lpszHostName, UrlParts.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)NULL);
	if (hConnection == NULL)
		goto out;

	hRequest = HttpOpenRequestA(hConnection, "GET", UrlParts.lpszUrlPath, NULL, NULL, accept_types,
		INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
		INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_HYPERLINK |
		((UrlParts.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0), (DWORD_PTR)NULL);
	if (hRequest == NULL)
		goto out;

	if (!HttpSendRequestA(hRequest, request_headers[1], -1L, NULL, 0))
		goto out;

	// Get the file size
	dwSize = sizeof(DownloadStatus);
	HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, (LPVOID)&DownloadStatus, &dwSize, NULL);
	if (DownloadStatus != 200)
		goto out;
	dwSize = sizeof(dwTotalSize);
	HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, (LPVOID)&dwTotalSize, &dwSize, NULL);

out:
	if (hRequest)
		InternetCloseHandle(hRequest);
	if (hConnection)
		InternetCloseHandle(hConnection);
	if (hSession)
		InternetCloseHandle(hSession);

	return (dwTotalSize > 0);
}
