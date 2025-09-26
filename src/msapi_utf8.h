#include <windows.h>
#include <shellapi.h>

#pragma once
#if defined(_MSC_VER)
// disable VS2012 Code Analysis warnings that are intentional
#pragma warning(disable: 6387)	// Don't care about bad params
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define wchar_to_utf8_no_alloc(wsrc, dest, dest_size) \
	WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, dest, (int)(dest_size), NULL, NULL)
#define utf8_to_wchar_no_alloc(src, wdest, wdest_size) \
	MultiByteToWideChar(CP_UTF8, 0, src, -1, wdest, (int)(wdest_size))

// Never ever use isdigit() or isspace(), etc. on UTF-8 strings!
// These calls take an int and char is signed so MS compilers will produce an assert error on anything that's > 0x80
#define isspaceU(c) isspace((unsigned char)(c))

#define sfree(p) do {if (p != NULL) {free((void*)(p)); p = NULL;}} while(0)
#define wconvert(p)     wchar_t* w ## p = utf8_to_wchar(p)
#define walloc(p, size) wchar_t* w ## p = (p == NULL)?NULL:(wchar_t*)calloc(size, sizeof(wchar_t))
#define wfree(p) sfree(w ## p)

/*
 * Converts an UTF-16 string to UTF8 (allocates returned string)
 * Returns NULL on error
 */
static __inline char* wchar_to_utf8(const wchar_t* wstr)
{
	int size = 0;
	char* str = NULL;

	if (wstr == NULL)
		return NULL;

	// Convert the empty string too
	if (wstr[0] == 0)
		return (char*)calloc(1, 1);

	// Find out the size we need to allocate for our converted string
	size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (size <= 1)	// An empty string would be size 1
		return NULL;

	if ((str = (char*)calloc(size, 1)) == NULL)
		return NULL;

	if (wchar_to_utf8_no_alloc(wstr, str, size) != size) {
		sfree(str);
		return NULL;
	}

	return str;
}

/*
 * Converts an UTF8 string to UTF-16 (allocates returned string)
 * Returns NULL on error
 */
static __inline wchar_t* utf8_to_wchar(const char* str)
{
	int size = 0;
	wchar_t* wstr = NULL;

	if (str == NULL)
		return NULL;

	// Convert the empty string too
	if (str[0] == 0)
		return (wchar_t*)calloc(1, sizeof(wchar_t));

	// Find out the size we need to allocate for our converted string
	size = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	if (size <= 1)	// An empty string would be size 1
		return NULL;

	if ((wstr = (wchar_t*)calloc(size, sizeof(wchar_t))) == NULL)
		return NULL;

	if (utf8_to_wchar_no_alloc(str, wstr, size) != size) {
		sfree(wstr);
		return NULL;
	}
	return wstr;
}


static __inline DWORD FormatMessageU(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
									 DWORD dwLanguageId, char* lpBuffer, DWORD nSize, va_list *Arguments)
{
	DWORD ret = 0, err = ERROR_INVALID_DATA;
	// Exclude support for the FORMAT_MESSAGE_ALLOCATE_BUFFER special case.
	if (dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	walloc(lpBuffer, nSize);
	if (wlpBuffer == NULL) {
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return 0;
	}
	ret = FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId, wlpBuffer, nSize, Arguments);
	err = GetLastError();
	if ((ret != 0) && ((ret = wchar_to_utf8_no_alloc(wlpBuffer, lpBuffer, nSize)) == 0)) {
		err = GetLastError();
		ret = 0;
	}
	// Coverity doesn't realize that we filtered out the FORMAT_MESSAGE_ALLOCATE_BUFFER case
	// coverity[leaked_storage]
	wfree(lpBuffer);
	SetLastError(err);
	return ret;
}

// SendMessage, with LPARAM as UTF-8 string
static __inline LRESULT SendMessageLU(HWND hWnd, UINT Msg, WPARAM wParam, const char* lParam)
{
	LRESULT ret = FALSE;
	DWORD err = ERROR_INVALID_DATA;
	wconvert(lParam);
	ret = SendMessageW(hWnd, Msg, wParam, (LPARAM)wlParam);
	err = GetLastError();
	wfree(lParam);
	SetLastError(err);
	return ret;
}
static __inline BOOL SetWindowTextU(HWND hWnd, const char* lpString)
{
	BOOL ret = FALSE;
	DWORD err = ERROR_INVALID_DATA;
	wconvert(lpString);
	ret = SetWindowTextW(hWnd, wlpString);
	err = GetLastError();
	wfree(lpString);
	SetLastError(err);
	return ret;
}

static __inline int GetWindowTextLengthU(HWND hWnd)
{
	int ret = 0;
	DWORD err = ERROR_INVALID_DATA;
	wchar_t* wbuf = NULL;
	char* buf = NULL;

	ret = GetWindowTextLengthW(hWnd);
	err = GetLastError();
	if (ret == 0) goto out;
	wbuf = (wchar_t* )calloc(ret, sizeof(wchar_t));
	err = GetLastError();
	if (wbuf == NULL) {
		err = ERROR_OUTOFMEMORY; ret = 0; goto out;
	}
	ret = GetWindowTextW(hWnd, wbuf, ret);
	err = GetLastError();
	if (ret == 0) goto out;
	buf = wchar_to_utf8(wbuf);
	err = GetLastError();
	if (buf == NULL) {
		err = ERROR_OUTOFMEMORY; ret = 0; goto out;
	}
	ret = (int)strlen(buf) + 2;	// GetDlgItemText seems to add a character
	err = GetLastError();
out:
	sfree(wbuf);
	sfree(buf);
	SetLastError(err);
	return ret;
}

static __inline UINT GetDlgItemTextU(HWND hDlg, int nIDDlgItem, char* lpString, int nMaxCount)
{
	UINT ret = 0;
	DWORD err = ERROR_INVALID_DATA;
	// coverity[returned_null]
	walloc(lpString, nMaxCount);
	ret = GetDlgItemTextW(hDlg, nIDDlgItem, wlpString, nMaxCount);
	err = GetLastError();
	if ((ret != 0) && ((ret = wchar_to_utf8_no_alloc(wlpString, lpString, nMaxCount)) == 0)) {
		err = GetLastError();
	}
	wfree(lpString);
	SetLastError(err);
	return ret;
}

static __inline HANDLE CreateFileU(const char* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
								   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
								   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE ret = INVALID_HANDLE_VALUE;
	DWORD err = ERROR_INVALID_DATA;
	wconvert(lpFileName);
	ret = CreateFileW(wlpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
		dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	err = GetLastError();
	wfree(lpFileName);
	SetLastError(err);
	return ret;
}

static __inline BOOL DeleteFileU(const char* lpFileName)
{
	BOOL ret = FALSE;
	DWORD err = ERROR_INVALID_DATA;
	wconvert(lpFileName);
	ret = DeleteFileW(wlpFileName);
	err = GetLastError();
	wfree(lpFileName);
	SetLastError(err);
	return ret;
}

// This one is tricky since we can't blindly convert a
// UTF-16 position to a UTF-8 one. So we do it manually.
static __inline const char* PathFindFileNameU(const char* szPath)
{
	size_t i;
	if (szPath == NULL)
		return NULL;
	for (i = strlen(szPath); i != 0; i--) {
		if ((szPath[i] == '/') || (szPath[i] == '\\')) {
			i++;
			break;
		}
	}
	return &szPath[i];
}


#ifdef __cplusplus
}
#endif
