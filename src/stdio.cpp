#include <windowsx.h>
#include <assert.h>

#include "SDI.h"
#include "system.h"
#include "resource.h"
#include "msapi_utf8.h"
#include "localization.h"

#define FACILITY_WIM            322

/*
 * Globals
 */
HWND hStatus;
size_t ubuffer_pos = 0;
char ubuffer[UBUFFER_SIZE];	// Buffer for ubpushf() messages we don't log right away

void uprintf(const char *format, ...)
{
	static char buf[4096];
	char* p = buf;
	wchar_t* wbuf;
	va_list args;
	int n;

	va_start(args, format);
	n = safe_vsnprintf(p, sizeof(buf)-3, format, args); // buf-3 is room for CR/LF/NUL
	va_end(args);

	p += (n < 0)?sizeof(buf)-3:n;

	while((p>buf) && (isspaceU(p[-1])))
		*--p = '\0';

	*p++ = '\r';
	*p++ = '\n';
	*p   = '\0';

	wbuf = utf8_to_wchar(buf);
	// Send output to Windows debug facility
	// coverity[dont_call]
	OutputDebugStringW(wbuf);
	if ((hLog != NULL) && (hLog != INVALID_HANDLE_VALUE)) {
		// Send output to our log Window
		Edit_SetSel(hLog, MAX_LOG_SIZE, MAX_LOG_SIZE);
		Edit_ReplaceSel(hLog, wbuf);
		// Make sure the message scrolls into view
		Edit_Scroll(hLog, Edit_GetLineCount(hLog), 0);
	}
	free(wbuf);
}

void uprintfs(const char* str)
{
	wchar_t* wstr;
	wstr = utf8_to_wchar(str);
	// coverity[dont_call]
	OutputDebugStringW(wstr);
	if ((hLog != NULL) && (hLog != INVALID_HANDLE_VALUE)) {
		Edit_SetSel(hLog, MAX_LOG_SIZE, MAX_LOG_SIZE);
		Edit_ReplaceSel(hLog, wstr);
		Edit_Scroll(hLog, Edit_GetLineCount(hLog), 0);
	}
	free(wstr);
}

// Convert a Windows error to human readable string
// One really has to wonder why the hell FormatMessage() was designed not to
// handle FORMAT_MESSAGE_FROM_HMODULE automatically according to the facility...
const char *WindowsErrorString(void)
{
	static char err_string[256] = { 0 };

	DWORD size, presize;
	DWORD error_code, _error_code, format_error;
	LCID locale;
	HANDLE hModule = NULL;

	error_code = GetLastError();
	_error_code = error_code;
	// Set thread locale to en-US when in this function.
	// This is because kernel32!FormatMessage is documented to try the following order when dwLanguageId==0:
	// - Language neutral
	// - Thread locale
	// - User default locale
	// - System default locale
	// - English US
	// Some Windows localisations do not provide English MUI resources for specific error codes.
	// So with thread locale set to en-US, FormatMessage will try neutral, then en-US, then fall back to localised string.
	locale = GetThreadLocale();
	SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT));
retry:
	// Check for specific facility error codes
	switch (HRESULT_FACILITY(_error_code)) {
	case FACILITY_NULL:
		// Special case for internet related errors, that don't actually have a facility
		// set but still require a hModule into wininet to display the messages.
		if ((_error_code >= INTERNET_ERROR_BASE) && (_error_code <= INTERNET_ERROR_LAST))
			hModule = GetModuleHandleA("wininet.dll");
		break;
	case FACILITY_ITF:
		hModule = GetModuleHandleA("vdsutil.dll");
		break;
	case FACILITY_WIM:
		hModule = GetModuleHandleA("wimgapi.dll");
		break;
	default:
		break;
	}
	static_sprintf(err_string, "[0x%08lX] ", error_code);
	presize = (DWORD)strlen(err_string);

	// coverity[var_deref_model]
	size = FormatMessageU(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
		((hModule != NULL) ? FORMAT_MESSAGE_FROM_HMODULE : 0), hModule,
		_error_code, 0,
		&err_string[presize], (DWORD)(sizeof(err_string) - strlen(err_string)), NULL);
	if (size == 0) {
		format_error = GetLastError();
		switch (format_error) {
		case ERROR_SUCCESS:
			static_sprintf(err_string, "[0x%08lX] (No Windows Error String)", _error_code);
			break;
		case ERROR_MR_MID_NOT_FOUND:
		case ERROR_MUI_FILE_NOT_FOUND:
		case ERROR_MUI_FILE_NOT_LOADED:
			// We might be trying with the wrong facility. Remove it and try again.
			if (HRESULT_FACILITY(_error_code) != FACILITY_NULL) {
				_error_code = HRESULT_CODE(_error_code);
				goto retry;
			}
			static_sprintf(err_string, "[0x%08lX] (NB: This system was unable to provide a descriptive error message)", error_code);
			break;
		default:
			static_sprintf(err_string, "[0x%08lX] (FormatMessage error code 0x%08lX)", error_code, format_error);
			break;
		}
	} else {
		// Microsoft may suffix CRLF to error messages, which we need to remove...
		assert(presize > 2);
		size += presize - 2;
		// Cannot underflow if the above assert passed since our first char is neither of the following
		while ((err_string[size] == 0x0D) || (err_string[size] == 0x0A) || (err_string[size] == 0x20))
			err_string[size--] = 0;
	}

	SetThreadLocale(locale);	// Set the original thread locale on exit
	SetLastError(error_code);	// Make sure we don't change the errorcode on exit
	return err_string;
}
// Find upper power of 2
static __inline uint16_t upo2(uint16_t v)
{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v++;
		return v;
}

// Convert a size to human readable
char* SizeToHumanReadable(uint64_t size, BOOL copy_to_log, BOOL fake_units)
{
	int suffix;
	static char str_size[32];
	const char* dir = ((right_to_left_mode) && (!copy_to_log)) ? LEFT_TO_RIGHT_MARK : "";
	double hr_size = (double)size;
	double t;
	uint16_t i_size;
	const char **_msg_table = copy_to_log ? default_msg_table : msg_table;
	const double divider = fake_units ? 1000.0 : 1024.0;

	for (suffix = 0; suffix < MAX_SIZE_SUFFIXES - 1; suffix++) {
		if (hr_size < divider)
			break;
		hr_size /= divider;
	}
	if (suffix == 0) {
		static_sprintf(str_size, "%s%d%s %s", dir, (int)hr_size, dir, _msg_table[MSG_020 - MSG_000]);
	} else if (fake_units) {
		if (hr_size < 8) {
			static_sprintf(str_size, (fabs((hr_size * 10.0) - (floor(hr_size + 0.5) * 10.0)) < 0.5) ?
				"%0.0f%s":"%0.1f%s", hr_size, _msg_table[MSG_020 + suffix - MSG_000]);
		} else {
			t = (double)upo2((uint16_t)hr_size);
			i_size = (uint16_t)((fabs(1.0f - (hr_size / t)) < 0.05f) ? t : hr_size);
			static_sprintf(str_size, "%s%d%s %s", dir, i_size, dir, _msg_table[MSG_020 + suffix - MSG_000]);
		}
	} else {
		static_sprintf(str_size, (hr_size * 10.0 - (floor(hr_size) * 10.0)) < 0.5?
			"%s%0.0f%s %s":"%s%0.1f%s %s", dir, hr_size, dir, _msg_table[MSG_020 + suffix - MSG_000]);
	}
	return str_size;
}
