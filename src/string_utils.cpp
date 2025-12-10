#include <shlwapi.h>

#include "string_utils.hpp"

#include "SDI.h"
#include "system.h"

//{ Strings

void WStringShort::sprintf(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    vsprintf(format, args);
    va_end(args);
}

void WStringShort::vsprintf(const wchar_t* format, va_list args)
{
    // Get required size
    va_list args_copy;
    va_copy(args_copy, args);
    int required = System._vscwprintf_dll(format, args_copy);
    va_end(args_copy);

    if (required < 0) {
        str_.clear();
        return;
    }

    // Resize and format directly into the string buffer
    str_.resize(static_cast<size_t>(required));
    (void)vswprintf_s(str_.data(), str_.size() + 1, format, args);
}

void WStringShort::append(const wchar_t* str)
{
    if (str) {
        str_ += str;
    }
}

void WStringShort::strcpy(const wchar_t* Str)
{
    str_ = (Str != nullptr) ? Str : L"";
}

void strsub(wchar_t *str,const wchar_t *pattern,const wchar_t *rep)
{
		wchar_t *s;

		s=StrStrIW(str,pattern);
		if(s)
		{
				wchar_t buf[MAX_PATH];
				wcscpy(buf,s);
				wcscpy(s,rep);
				wcscpy(s+wcslen(rep),buf+wcslen(pattern));
		}
}

void strtoupper(const char *s1,size_t len)
{
		char *s=const_cast<char *>(s1);
		while(len--)
		{
				*s=static_cast<char>(toupper(*s));
				s++;
		}
}

void strtolower(const char *s1,size_t len)
{
		char *s=const_cast<char *>(s1);
		if(len)
		while(len--)
		{
				*s=static_cast<char>(tolower(*s));
				s++;
		}
}

size_t unicode2ansi(const unsigned char *s,char *out,size_t DestSize)
{
    const wchar_t *wide_s = reinterpret_cast<const wchar_t*>(
        s + (s[0] == 0xFF ? 2 : 0)  // Skip BOM
    );

    size_t ret = WideCharToMultiByte(
        CP_ACP, 0, wide_s, static_cast<int>(DestSize / 2 - 1),
        out, static_cast<int>(DestSize), nullptr, nullptr
    );

    if (DestSize > 0)
        out[DestSize - 1] = '\0';

    return ret;
}

int _wtoi_my(const wchar_t *str)
{
		int val;
		swscanf(str,L"%d",&val);
		return val;
}
//}
