/*
This file is part of Snappy Driver Installer Origin.

Snappy Driver Installer Origin is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License or (at your option) any later version.

Snappy Driver Installer Origin is distributed in the hope that it will be useful
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Snappy Driver Installer Origin.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>
#include "windows.h"
#ifndef SYSTEM_H
#define SYSTEM_H
#include "enum.h"

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

//{ ThreadAbs
typedef unsigned ( __stdcall *threadCallback)(void *arg);

class ThreadAbs
{
public:
    virtual ~ThreadAbs(){}
    virtual void start(threadCallback callback,void *arg)=0;
    virtual void join()=0;
};
ThreadAbs *CreateThread();
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

typedef int (__cdecl *WINAPI5_vscwprintf)(const wchar_t * __restrict___Format, va_list _ArgList);

//{ System
class SystemImp
{
    HINSTANCE hinstLib=nullptr;
    WINAPI5_vscwprintf _vscwprintf_func=nullptr;

public:
    SystemImp();
    ~SystemImp();

    bool IsLangInstalled(int group);
    unsigned GetTickCountWr();

    int canWrite(const wchar_t *path);
    int run_command(const wchar_t* file,const wchar_t* cmd,int show,int wait);
    int run_command32(const wchar_t* file,const wchar_t* cmd,int show,int wait);
    void run_controlpanel(const wchar_t* cmd);
    void benchmark();

    void deletefile(const wchar_t *filename);
    bool FileExists(const wchar_t *filename);
    bool FileExists2(const wchar_t *spec);
    bool DirectoryExists(const wchar_t *spec);
    __int64 FileSize(const wchar_t *filename);
    __int64 DirectorySize(const std::wstring directory);
    std::wstring ExpandEnvVar(std::wstring source);
    bool ChooseDir(wchar_t *path,const wchar_t *title);
    bool ChooseFile(wchar_t *filename,const wchar_t *strlist,const wchar_t *ext);
    void CreateDir(const wchar_t *filename);
    void fileDelSpec(wchar_t *filename);
	int DriveNumber(const wchar_t *filename);

    void UnregisterClass_log(const wchar_t *lpClassName,const wchar_t *func,const wchar_t *obj);
    int _vscwprintf_dll(const wchar_t * _Format,va_list _ArgList);
    std::string wtoa (const std::wstring& wstr);
    std::wstring AppPathW();
    std::string AppPathS();
    int FindLatestExeVersion(int bit=32);
    bool SystemProtectionEnabled(State *state);
    int GetRestorePointCreationFrequency();
    void SetRestorePointCreationFrequency(int freq);
    bool CreateRestorePoint(std::wstring desc);
    int getver(const char *s);
    int getcurver(const char *s);
    bool GetNonPresentDevices();
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

#endif
