#pragma once

#include <string>
#include <cstdarg>
#include <span>

//----------------------------------------------------------------------------

// Modern C++23 wrapper around std::wstring maintaining legacy API compatibility
class WStringShort
{
private:
    std::wstring str_;
    
public:
    WStringShort([[maybe_unused]] bool debug_ = false) {}
    
    void sprintf(const wchar_t* format, ...);
    void vsprintf(const wchar_t* format, va_list args);
    void append(const wchar_t* str);
    void strcpy(const wchar_t* Str);
    
    wchar_t* GetV() { return str_.data(); }
    const wchar_t* Get() const { return str_.c_str(); }
    size_t Length() const { return str_.size(); }
    
    // Allow implicit conversion to const wchar_t* for compatibility
    operator const wchar_t*() const { return str_.c_str(); }
};

// Type aliases for backward compatibility
using WString = WStringShort;
using WString_dyn = WStringShort;

//----------------------------------------------------------------------------
// String conversion utilities

// C++23 modern function: converts UTF-16LE encoded data to ANSI string
// Handles optional BOM (0xFF 0xFE) automatically
[[nodiscard]] std::string unicode_to_ansi(std::span<const unsigned char> input);
