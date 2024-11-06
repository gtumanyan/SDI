#include "BaseUtil.h"
#include "FileUtil.h"
#include "ScopedWin.h"
#include "WinUtil.h"

#include "Log.h"

TempStr GetLastErrorStrTemp(DWORD err) {
    if (err == 0) {
        err = GetLastError();
    }
    if (err == 0) {
        return str::DupTemp("");
    }
    if (err == ERROR_INTERNET_EXTENDED_ERROR) {
        char buf[4096]{};
        DWORD bufSize = dimof(buf) - 1;
        // TODO: ignoring a case where buffer is too small. 4 kB should be enough for everybody
        InternetGetLastResponseInfoA(&err, buf, &bufSize);
        buf[4095] = 0;
        return str::DupTemp(buf);
    }
    char* msgBuf = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    DWORD ferr = FormatMessageA(flags, nullptr, err, lang, (LPSTR)&msgBuf, 0, nullptr);
    if (!ferr || !msgBuf) {
        return str::DupTemp("");
    }
    auto res = str::DupTemp(msgBuf);
    LocalFree(msgBuf);
    return res;
}

void LogLastError(DWORD err) {
    TempStr msg = GetLastErrorStrTemp(err);
    if (msg == nullptr) {
        msg = (TempStr) "";
    }
    str::TrimWSInPlace(msg, str::TrimOpt::Both);
    logf("LogLastError: 0x%x (%d) '%s'\n", (int)err, (int)err, msg);
}

void DbgOutLastError(DWORD err) {
    TempStr msg = GetLastErrorStrTemp(err);
    OutputDebugStringA(msg);
}

TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing) {
    if (createIfMissing) {
        csidl = csidl | CSIDL_FLAG_CREATE;
    }
    WCHAR path[MAX_PATH]{};
    HRESULT res = SHGetFolderPathW(nullptr, csidl, nullptr, 0, path);
    if (S_OK != res) {
        return nullptr;
    }
    return ToUtf8Temp(path);
}

// Return the full exe path of my own executable
TempStr GetSelfExePathTemp() {
    WCHAR buf[MAX_PATH]{};
    DWORD nSize = dimof(buf) - 1;
    auto h = GetInstance();
    DWORD res = GetModuleFileNameW(h, buf, nSize);
    if (res < nSize) {
        return ToUtf8Temp(buf);
    }
    nSize = res + 2;
    WCHAR* buf2 = Allocator::AllocArray<WCHAR>(nullptr, (size_t)nSize);
    res = GetModuleFileNameW(h, buf, nSize);
    ReportIf(res < nSize);
    return ToUtf8Temp(buf2);
}

TempWStr GetSelfExePathWTemp()
{
		WCHAR buf[MAX_PATH]{};
		std::wstring long_path;
		auto h = GetInstance();
		DWORD nSize = dimof(buf) - 1;
		auto res = GetModuleFileNameW(h, buf, nSize);
		if (res < nSize)
		{
				return buf;
		}
		nSize = res + 2;
		WCHAR* buf2 = Allocator::AllocArray<WCHAR>(nullptr, (size_t)nSize);
		res = GetModuleFileNameW(h, buf, nSize);
		ReportIf(res < nSize);
		return buf2;
}

// Return directory where our executable is located
TempStr GetSelfExeDirTemp() {
    TempStr path = GetSelfExePathTemp();
    return path::GetDirTemp(path);
}
// cmdLine must contain quoted exe path as first argument
HANDLE LaunchProcessInDir(const char* cmdLine, const char* currDir, DWORD flags) {
    PROCESS_INFORMATION pi = {nullptr};
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    // CreateProcess() might modify cmd line argument, so make a copy
    // in case caller provides a read-only string
    WCHAR* cmdLineW = ToWStrTemp(cmdLine);
    WCHAR* dirW = ToWStrTemp(currDir);
    if (!CreateProcessW(nullptr, cmdLineW, nullptr, nullptr, FALSE, flags, nullptr, dirW, &si, &pi)) {
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

bool CreateProcessHelper(const char* exe, const char* args) {
    if (!args) {
        args = "";
    }
    TempStr cmd = str::FormatTemp("\"%s\" %s", exe, args);
    AutoCloseHandle process = LaunchProcessInDir(cmd);
    return process != nullptr;
}
bool IsValidHandle(HANDLE h) {
    return !(h == nullptr || h == INVALID_HANDLE_VALUE);
}

// cf. http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// A convenient way to grab the same value as HINSTANCE passed to WinMain
HINSTANCE GetInstance() {
    return (HINSTANCE)&__ImageBase;
}

