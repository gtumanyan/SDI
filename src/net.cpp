#include <netlistmgr.h>
#include <assert.h>

#include "SDI.h"
#include "resource.h"
#include "msapi_utf8.h"

/* Maximum download chunk size, in bytes */
#define DOWNLOAD_BUFFER_SIZE    (10*KB)

DWORD DownloadStatus;
DWORD ErrorStatus = 0;
static DWORD error_code;
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
	// If the URL is followed by a query, remove that part
	// Make sure we detect escaped queries too
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
static HINTERNET GetInternetSession(const char* user_agent, BOOL bRetry)
{
	int i;
	char default_agent[64];
	BOOL decodingSupport = TRUE;
	VARIANT_BOOL InternetConnection = VARIANT_FALSE;
	DWORD dwFlags, dwTimeout = NET_SESSION_TIMEOUT, dwProtocolSupport = HTTP_PROTOCOL_FLAG_HTTP2;
	HINTERNET hSession = NULL;
	HRESULT hr = S_FALSE;
	INetworkListManager* pNetworkListManager;
	// Create a NetworkListManager Instance to check the network connection
	IGNORE_RETVAL(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
	hr = CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL,
		IID_INetworkListManager, (LPVOID*)&pNetworkListManager);
	if (hr == S_OK) {
		for (i = 0; i <= WRITE_RETRIES; i++) {
			hr = pNetworkListManager->get_IsConnectedToInternet(&InternetConnection);
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
	if (InternetConnection == VARIANT_FALSE) {
		SetLastError(ERROR_INTERNET_DISCONNECTED);
		goto out;
	}
	static_sprintf(default_agent, APPNAME "/%d.%d.%d (Windows NT %lu.%lu%s)",
		SDI_version[0], SDI_version[1], SDI_version[2],
		WindowsVersion.Major, WindowsVersion.Minor, is_WOW64() ? "; WOW64" : "");
	hSession = InternetOpenA((user_agent == NULL) ? default_agent : user_agent,
		INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	// Set the timeouts
	InternetSetOptionA(hSession, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&dwTimeout, sizeof(dwTimeout));
	InternetSetOptionA(hSession, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&dwTimeout, sizeof(dwTimeout));
	InternetSetOptionA(hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&dwTimeout, sizeof(dwTimeout));
	// Enable gzip and deflate decoding schemes
	InternetSetOptionA(hSession, INTERNET_OPTION_HTTP_DECODING, (LPVOID)&decodingSupport, sizeof(decodingSupport));
	// Enable HTTP/2 protocol support
	InternetSetOptionA(hSession, INTERNET_OPTION_ENABLE_HTTP_PROTOCOL, (LPVOID)&dwProtocolSupport, sizeof(dwProtocolSupport));

out:
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
		const char* accept_types[] = { "*/*\0", NULL };
		const char* short_name;
		unsigned char buf[DOWNLOAD_BUFFER_SIZE];
		char hostname[64], urlpath[128], strsize[32];
		BOOL r = FALSE;
		DWORD dwSize, dwWritten, dwDownloaded;
		HANDLE hFile = INVALID_HANDLE_VALUE;
		HINTERNET hSession = NULL, hConnection = NULL, hRequest = NULL;
		URL_COMPONENTSA UrlParts = { sizeof(URL_COMPONENTSA), NULL, 1, (INTERNET_SCHEME)0,
			hostname, sizeof(hostname), 0, NULL, 1, urlpath, sizeof(urlpath), NULL, 1 };
		uint64_t size = 0, total_size = 0;

		ErrorStatus = 0;
		DownloadStatus = 404;
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

		if ((!InternetCrackUrlA(url, (DWORD)safe_strlen(url), 0, &UrlParts))
				|| (UrlParts.lpszHostName == NULL) || (UrlParts.lpszUrlPath == NULL)) {
				uprintf("Unable to decode URL: %s", WindowsErrorString());
				goto out;
		}
		hostname[sizeof(hostname) - 1] = 0;

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

		// If we are querying the GitHub API, we need to enable raw content
		if (strstr(url, "api.github.com") != NULL && !HttpAddRequestHeadersA(hRequest,
				"Accept: application/vnd.github.v3.raw", (DWORD)-1, HTTP_ADDREQ_FLAG_ADD)) {
				uprintf("Unable to enable raw content from GitHub API: %s", WindowsErrorString());
				goto out;
		}
		// Must use "Accept-Encoding: identity" to get the file size
		// This is needed for GitHub as the Microsoft HTTP APIs can't seem to read content-length for
		// compressed content from GitHub, and using "identity" disables compression.
		HttpSendRequestA(hRequest, "Accept-Encoding: identity", -1L, NULL, 0);

		// Get the file size
		dwSize = sizeof(DownloadStatus);
		HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, (LPVOID)&DownloadStatus, &dwSize, NULL);
		if (DownloadStatus != 200) {
				error_code = ERROR_INTERNET_ITEM_NOT_FOUND;
				SetLastError(SDI_ERROR(error_code));
				uprintf("%s '%s': %d", (DownloadStatus == 404) ? "File not found" : "Unable to access file", url, DownloadStatus);
				goto out;
		}
		dwSize = sizeof(strsize);
		if (!HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH, (LPVOID)strsize, &dwSize, NULL)) {
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
				*buffer = new BYTE[total_size + 2]();
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
				ErrorStatus = SDI_ERROR(ERROR_WRITE_FAULT);
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
