/*
This file is part of Snappy Driver Installer.

Snappy Driver Installer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License or (at your option) any later version.

Snappy Driver Installer is distributed in the hope that it will be useful
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Snappy Driver Installer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>

#include "enum.h"

#pragma once

#define ubprintf(...) do { safe_sprintf(&ubuffer[ubuffer_pos], UBUFFER_SIZE - ubuffer_pos - 4, __VA_ARGS__); \
	ubuffer_pos = strlen(ubuffer); ubuffer[ubuffer_pos++] = '\r'; ubuffer[ubuffer_pos++] = '\n'; \
	ubuffer[ubuffer_pos] = 0; } while(0)

//{ Event
class Event
{
public:
    virtual ~Event(){}
    virtual void wait()=0;
    virtual bool isRaised()=0;
    virtual void raise()=0;
    virtual void reset()=0;
};
Event *CreateEventWr(bool manual=false);
//}

//{ Thread - Modern C++26 implementation using std::jthread
#include <thread>
#include <functional>
#include <memory>

typedef unsigned ( __stdcall *threadCallback)(void *arg);

class Thread
{
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    std::jthread m_thread;

public:
    Thread() = default;
    ~Thread() = default;

    // Move operations
    Thread(Thread&&) noexcept = default;
    Thread& operator=(Thread&&) noexcept = default;

    void start(threadCallback callback, void* arg)
    {
        m_thread = std::jthread([callback, arg]() -> void {
            callback(arg);
        });
    }

    void join()
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    [[nodiscard]] bool joinable() const noexcept
    {
        return m_thread.joinable();
    }

    // Request stop for cooperative cancellation (C++20 feature)
    bool request_stop() noexcept
    {
        return m_thread.request_stop();
    }

    [[nodiscard]] std::stop_token get_stop_token() const noexcept
    {
        return m_thread.get_stop_token();
    }
};
//}

//{ RECT
struct RECT_WR
{
    long left;
    long top;
    long right;
    long bottom;
};
//}

void get_resource(int id,void **data,size_t *size);
void mkdir_r(const wchar_t *path);
void StrFormatSize(long long val,wchar_t *buf,int len);
void ShowHelp();

typedef int (__cdecl *WINAPI5_vscwprintf)(const wchar_t * __restrict___Format,va_list _ArgList);

//{ System
class SystemImp
{
    HINSTANCE hinstLib=nullptr;
    WINAPI5_vscwprintf _vscwprintf_func=nullptr;

public:
    SystemImp();
    ~SystemImp();

    static bool IsLangInstalled(int group);
    static unsigned GetTickCountWr();

//    int canWrite(const wchar_t *path);
    static bool canWriteFile(const wchar_t *path,const wchar_t *mode);
    static int canWriteDirectory(const wchar_t *path);
    static int run_command(const wchar_t* file,const wchar_t* cmd,int show,int wait);
    static int run_command32(const wchar_t* file,const wchar_t* cmd,int show,int wait);
    static void run_controlpanel(const wchar_t* cmd);
    void benchmark();

    static void deletefile(const wchar_t *filename);
    static BOOL FileAvailable(const wchar_t *path, int numRetries, int waitTime);
    static bool FileExists(const wchar_t *filename);
    static bool FileExists2(const wchar_t *spec);
    static bool DirectoryExists(const wchar_t *spec);
    static __int64 FileSize(const wchar_t *filename);
    static __int64 DirectorySize(const std::wstring directory);
    static std::wstring ExpandEnvVar(std::wstring source);
    static bool ChooseDir(wchar_t *path,const wchar_t *title);
    static bool ChooseFile(wchar_t *filename,const wchar_t *strlist,const wchar_t *ext);
    static void CreateDir(const wchar_t *filename);
    static void fileDelSpec(wchar_t *filename);
    static int DriveNumber(const wchar_t *filename);

    static int UnregisterClass_log(const wchar_t *lpClassName,const wchar_t *func,const wchar_t *obj);
    int _vscwprintf_dll(const wchar_t * _Format,va_list _ArgList);
    static std::string wtoa (const std::wstring& wstr);
    static std::wstring AppPathW();
    static std::string AppPathS();
    //int FindLatestExeVersion(int bit=32);
    static bool SystemProtectionEnabled(State *state);
    static int GetRestorePointCreationFrequency();
    static void SetRestorePointCreationFrequency(int freq);
    bool CreateRestorePoint(std::wstring desc);
    static int getver(const char *s);
    static int getcurver(const char *s);
    static bool GetNonPresentDevices();
};
extern SystemImp System;
//}

//{ FileMonitor
class Filemon
{
public:
    virtual ~Filemon(){}
};

typedef void (*FileChangeCallback)(const wchar_t *szFile,int Action,int lParam);
Filemon *CreateFilemon(const wchar_t *szDirectory,int subdirs,FileChangeCallback callback);
extern int monitor_pause;
//}

typedef BOOL (WINAPI *LPFN_Wow64DisableWow64FsRedirection)(PVOID *OldValue);
typedef BOOL (WINAPI *LPFN_Wow64RevertWow64FsRedirection)(PVOID OldValue);