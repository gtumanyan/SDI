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

#include "com_header.h"
#include "common.h"
#include "logging.h"
#include "settings.h"
#include "system.h"
#include "manager.h"
#include "matcher.h"
#include "commdlg.h"
#include "shellapi.h"
#include "tchar.h"

#include <windows.h>
#include <process.h>
#ifdef _MSC_VER
#include <errno.h>
#include <commdlg.h>
#include <direct.h>
#include <shellapi.h>
#else
#undef _INC_SHLWAPI
#endif
#include <setupapi.h>       // for SHELLEXECUTEINFO
#include <shlwapi.h>        // for PathFileExists
#include <shlobj.h>         // for SHBrowseForFolder
#include <sapi.h>

// Depend on Win32API
#include "main.h"

#include "system_imp.h"
#include <SRRestorePtAPI.h> // for RestorePoint
typedef int (WINAPI *WINAPI5t_SRSetRestorePointW)(PRESTOREPOINTINFOW pRestorePtSpec,PSTATEMGRSTATUS pSMgrStatus);

SystemImp System;
int monitor_pause=0;
HFONT CLIHelp_Font;

static BOOL CALLBACK EnumLanguageGroupsProc(
    LGRPID LanguageGroup,
    LPTSTR lpLanguageGroupString,
    LPTSTR lpLanguageGroupNameString,
    DWORD dwFlags,
    LONG_PTR  lParam
    )
{
    UNREFERENCED_PARAMETER(lpLanguageGroupString);
    UNREFERENCED_PARAMETER(lpLanguageGroupNameString);
    UNREFERENCED_PARAMETER(dwFlags);

    LGRPID* plLang=(LGRPID*)(lParam);
    //Log.print_con("lang %d,%ws,%ws,%d\n",LanguageGroup,lpLanguageGroupString,lpLanguageGroupNameString,dwFlags);
    if(*plLang==LanguageGroup)
    {
        *plLang=0;
        return false;
    }
    return true;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd,UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	// If the BFFM_INITIALIZED message is received
	// set the path to the start path.
    UNREFERENCED_PARAMETER(lParam);
    if((uMsg==BFFM_INITIALIZED)&&(lpData!=0))
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	return 0; // The function should always return 0.
}

bool SystemImp::IsLangInstalled(int group)
{
    LONG lLang=group;
    EnumSystemLanguageGroups(EnumLanguageGroupsProc,LGRPID_INSTALLED,(LONG_PTR)&lLang);
    return lLang==0;
}

unsigned SystemImp::GetTickCountWr()
{
    return GetTickCount();
}

SystemImp::SystemImp()
{
}

SystemImp::~SystemImp()
{
}

bool SystemImp::ChooseDir(wchar_t *path,const wchar_t *title)
{
    BROWSEINFO lpbi;
    memset(&lpbi,0,sizeof(BROWSEINFO));
    lpbi.hwndOwner=MainWindow.hMain;
    lpbi.pszDisplayName=path;
    lpbi.lpszTitle=title;
    lpbi.lParam=(LPARAM)path;
    lpbi.lpfn=BrowseCallbackProc;
    lpbi.ulFlags=BIF_NEWDIALOGSTYLE|BIF_EDITBOX;

    LPITEMIDLIST list=SHBrowseForFolder(&lpbi);
    if(list)
    {
        SHGetPathFromIDList(list,path);
        return true;
    }
    return false;
}

bool SystemImp::ChooseFile(wchar_t *filename,const wchar_t *strlist,const wchar_t *ext)
{
    OPENFILENAME ofn;
    memset(&ofn,0,sizeof(OPENFILENAME));
    ofn.lStructSize=sizeof(OPENFILENAME);
    ofn.hwndOwner  =MainWindow.hMain;
    ofn.lpstrFilter=strlist;
    ofn.nMaxFile   =BUFLEN;
    ofn.lpstrDefExt=ext;
    ofn.lpstrFile  =filename;
    ofn.Flags      =OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    std::wstring initPath=System.AppPathW();
    ofn.lpstrInitialDir=initPath.c_str();

    if(GetOpenFileName(&ofn))return true;
    return false;
}
void get_resource(int id,void **data,size_t *size)
{
    HRSRC myResource=FindResource(nullptr,MAKEINTRESOURCE(id),(wchar_t *)RESFILE);
    if(!myResource)
    {
        Log.print_err("ERROR in get_resource(): failed FindResource(%d)\n",id);
        *size=0;
        *data=nullptr;
        return;
    }
    *size=SizeofResource(nullptr,myResource);
    *data=LoadResource(nullptr,myResource);
}
void StrFormatSize(long long val,wchar_t *buf,int len)
{
    StrFormatByteSizeW(val,buf,len);
}

void mkdir_r(const wchar_t *path)
{
    // force create the directory path
    // by attempting to create each component of the path
    // one at a time

    // invalid path - 'C:'
    if(path[1]==L':'&&path[2]==0)return;

    // if it exists there's nothing to do
    if(System.DirectoryExists(path))return;

    wchar_t buf[BUFLEN];
    wcscpy(buf,path);
    wchar_t *p=buf;

    // unc path - skip past server name
    if((wcslen(buf)>2)&&(buf[0]==L'\\')&&(buf[1]==L'\\'))
    {
        p++;p++;
        p=wcschr(p,L'\\');
        p++;
    }

    // work through the path one directory at a time
    // by searching for the backslash delimiter
    while((p==wcschr(p,L'\\')))
    {
        *p=0;
        if(_wmkdir(buf)<0&&errno!=EEXIST&&wcslen(buf)>2)
        {
            Log.print_err("ERROR in mkdir_r(): failed _wmkdir(%S,%d). Write protected?\n",buf,errno);
            return;
        }
        *p=L'\\';
        p++;
    }
    // final directory component
    if(_wmkdir(buf)<0&&errno!=EEXIST&&wcslen(buf)>2)
        Log.print_err("ERROR in mkdir_r(): failed _wmkdir(%S,%d). Write protected?\n",buf,errno);
}

void SystemImp::UnregisterClass_log(const wchar_t *lpClassName,const wchar_t *func,const wchar_t *obj)
{
    if(!UnregisterClass(lpClassName,ghInst))
        Log.print_err("ERROR in %S(): failed UnregisterClass(%S)\n",func,obj);
}

bool SystemImp::FileAvailable(const wchar_t *path, int numRetries, int waitTime)
{
    // this repeatedly tests for existence of the given file
    // for the given number of retries
    bool FileOk;
    int retries=0;

    FileOk=System.FileExists(path);
    while(!FileOk)
    {
        retries++;
        if(retries>numRetries)break;
        Log.print_con("Waiting: %S\n", path);
        Sleep(waitTime*1000);
        FileOk=System.FileExists(path);
    }

    return(FileOk);
}

bool SystemImp::FileExists(const wchar_t *path)
{
    return PathFileExists(path)!=0;
}

bool SystemImp::FileExists2(const wchar_t *spec)
{
    bool ret=FALSE;
    WStringShort buf;
    buf.sprintf(L"%s",spec);
    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind=FindFirstFileW(buf.Get(),&FindFileData);
    if(hFind!=INVALID_HANDLE_VALUE)
    {
        if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)
            ret=TRUE;
        else

        while(FindNextFileW(hFind,&FindFileData)!=0)
        {
            if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)
            {
                ret=TRUE;
                break;
            }
        }
        FindClose(hFind);
    }

    return ret;
}

bool SystemImp::DirectoryExists(const wchar_t *spec)
{
    DWORD ftyp=GetFileAttributesW(spec);
    if(ftyp!=INVALID_FILE_ATTRIBUTES)
    {
        if((ftyp&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
            return true;
        else
            return false;
    }
    else
    {
        DWORD error=GetLastError();
        if(error&(ERROR_PATH_NOT_FOUND|ERROR_FILE_NOT_FOUND|ERROR_INVALID_NAME|ERROR_BAD_NETPATH))
            return false;
        else
            return true;
    }
}

__int64 SystemImp::FileSize(const wchar_t *filename)
{
    __int64 ret=0;
    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind=FindFirstFileW(filename,&FindFileData);
    if(hFind==INVALID_HANDLE_VALUE)return ret;

    do
    {
        ULARGE_INTEGER ul;
        ul.HighPart=FindFileData.nFileSizeHigh;
        ul.LowPart=FindFileData.nFileSizeLow;
        ret+=ul.QuadPart;
    }
    while(FindNextFile(hFind,&FindFileData)!=0);
    FindClose(hFind);

    return ret;
}

__int64 SystemImp::DirectorySize(const std::wstring directory)
{
    // assumes a full path and no file spec
    std::wstring cwd(directory);
    // add missing backslash
    if(cwd.back()!=L'\\')
        cwd+=L'\\';
    std::wstring spec(cwd);
    spec+=L"*.*";
    __int64 ret=0;

    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind=FindFirstFileW(spec.c_str(),&FindFileData);
    if(hFind==INVALID_HANDLE_VALUE)return ret;

    do
    {
        if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
        {
            if((_wcsicmp(FindFileData.cFileName,L".")!=0)&&(_wcsicmp(FindFileData.cFileName,L"..")!=0))
            {
                spec=cwd+FindFileData.cFileName;
                ret=ret+DirectorySize(spec);
            }
        }
        else
        {
            ULARGE_INTEGER ul;
            ul.HighPart=FindFileData.nFileSizeHigh;
            ul.LowPart=FindFileData.nFileSizeLow;
            ret+=ul.QuadPart;
        }
    }
    while(FindNextFile(hFind,&FindFileData)!=0);
    FindClose(hFind);

    return ret;
}

int SystemImp::DriveNumber(const wchar_t *filename)
{
    return PathGetDriveNumberW(filename);
}

std::wstring SystemImp::ExpandEnvVar(std::wstring source)
{
    wchar_t d[BUFLEN];
    *d=0;
    ExpandEnvironmentStringsW(source.c_str(),d,BUFLEN);
    return d;
}
/*
int SystemImp::canWrite(const wchar_t *path)
{
    DWORD flagsv=0;
    DWORD lasterror;
    wchar_t drive[4];

    wcscpy(drive,L"C:\\");

    if(path&&wcslen(path)>1&&path[1]==':')
    {
        drive[0]=path[0];
        if(!GetVolumeInformation(drive,nullptr,0,nullptr,nullptr,&flagsv,nullptr,0))
        {
            lasterror=GetLastError();
            Log.print_err("Error: canWrite : GetVolumeInformation(1) failed with error %d\n",lasterror);
            if(lasterror==3)Log.print_err("Error: Path not found: %S\n",path);
        }
    }
    else
        if(!GetVolumeInformation(nullptr,nullptr,0,nullptr,nullptr,&flagsv,nullptr,0))
        {
            lasterror=GetLastError();
            Log.print_err("Error: canWrite : GetVolumeInformation(2) failed with error %d\n",lasterror);
            if(lasterror==3)Log.print_err("Error: Path not found: %S\n",path);
        }

    return (flagsv&FILE_READ_ONLY_VOLUME)?0:1;
}
*/
bool SystemImp::canWriteFile(const wchar_t *path,const wchar_t *mode)
{
    // test if the given file name can be written or updated

    FILE *stream;
    errno_t err;
    DWORD flagsv=0;
    DWORD lasterror;
    wchar_t drive[4];

    // full local path given
    if(path&&wcslen(path)>1&&path[1]==':')
    {
        wcscpy(drive,L"C:\\");
        drive[0]=path[0];
        if(!GetVolumeInformation(drive,nullptr,0,nullptr,nullptr,&flagsv,nullptr,0))
        {
            lasterror=GetLastError();
            Log.print_err("Error: canWriteFile : GetVolumeInformation(1) failed with error %d\n",lasterror);
            if(lasterror==3)Log.print_err("Error: Path not found: %S\n",path);
        }
    }
    // test if the file can be opened for the required mode
    else
    {
        err = _wfopen_s(&stream, path, mode);
        if (err)
        {
            Log.print_err("Error %d The file %S was not opened\n",err,path);
            return(false);
        }
        // Close stream if it isn't NULL
        if (stream)
        {
            err = fclose(stream);
            if (err)
            {
                Log.print_err("The file %S was not closed\n,path");
            }
            return(true);
        }
    }
    return bool (flagsv&FILE_READ_ONLY_VOLUME)?0:1;
}

int SystemImp::canWriteDirectory(const wchar_t *path)
{
    // test if the given directory is writeable

    DWORD flagsv=0;
    DWORD lasterror;
    wchar_t drive[4];

    // attempt to force create it first
    mkdir_r(path);

    // full local path given
    if(path&&wcslen(path)>1&&path[1]==':')
    {
        wcscpy(drive,L"C:\\");
        drive[0]=path[0];
        if(!GetVolumeInformation(drive,nullptr,0,nullptr,nullptr,&flagsv,nullptr,0))
        {
            lasterror=GetLastError();
            Log.print_err("Error: canWriteDirectory : GetVolumeInformation(1) failed with error %d\n",lasterror);
            if(lasterror==3)Log.print_err("Error: Path not found: %S\n",path);
        }
    }
    // test if i can create a temporary file in the given directory
    else
    {
        wchar_t tmpFile[MAX_PATH];
        // the function opens and closes the temp file
        flagsv=(GetTempFileName(path,L"SDIO",0,tmpFile));
        // if temp file was successfully created then I should delete it
        if(flagsv)
            DeleteFile(tmpFile);
        return(flagsv);
    }
    return (flagsv&FILE_READ_ONLY_VOLUME)?0:1;
}

void SystemImp::CreateDir(const wchar_t *filename)
{
    CreateDirectory(filename,nullptr);
}

void SystemImp::deletefile(const wchar_t *filename)
{
    DeleteFileW(filename);
}

void SystemImp::fileDelSpec(wchar_t *filename)
{
    PathRemoveFileSpec(filename);
}

int SystemImp::run_command(const wchar_t* file,const wchar_t* cmd,int show,int wait)
{
    DWORD ret;

    SHELLEXECUTEINFO ShExecInfo;
    memset(&ShExecInfo,0,sizeof(SHELLEXECUTEINFO));
    ShExecInfo.cbSize=sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask=SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.lpFile=file;
    ShExecInfo.lpParameters=cmd;
    ShExecInfo.nShow=show;

    Log.print_con("Run(%S,%S,%d,%d)\n",file,cmd,show,wait);
    // is file "open"
    if(!wcscmp(file,L"open"))
        ShellExecute(nullptr,L"open",cmd,nullptr,nullptr,SW_SHOWNORMAL);
    else
        ShellExecuteEx(&ShExecInfo);

    if(!wait)return 0;
    WaitForSingleObject(ShExecInfo.hProcess,INFINITE);
    GetExitCodeProcess(ShExecInfo.hProcess,&ret);
    return ret;
}

int SystemImp::run_command32(const wchar_t* file,const wchar_t* cmd,int show,int wait)
{
    // dynamic binding to kernel32 is good
    PVOID OldValue=NULL;
    LPFN_Wow64DisableWow64FsRedirection pfnWow64DisableWowFsRedirection=(LPFN_Wow64DisableWow64FsRedirection)GetProcAddress(GetModuleHandle(_T("kernel32")),"Wow64DisableWow64FsRedirection");
    LPFN_Wow64RevertWow64FsRedirection pfnWow64RevertWowFsRedirection=(LPFN_Wow64RevertWow64FsRedirection)GetProcAddress(GetModuleHandle(_T("kernel32")),"Wow64RevertWow64FsRedirection");
    bool b=false;
    if(pfnWow64DisableWowFsRedirection && pfnWow64RevertWowFsRedirection)
        b=pfnWow64DisableWowFsRedirection(&OldValue);

    std::wstring f=ExpandEnvVar(file);
    int ret=System.run_command(f.c_str(),cmd,show,wait);

    if(b && pfnWow64DisableWowFsRedirection && pfnWow64RevertWowFsRedirection)
        pfnWow64RevertWowFsRedirection(OldValue);
    return ret;
}

void SystemImp::run_controlpanel(const wchar_t* cmd)
{
    PVOID OldValue=NULL;
    LPFN_Wow64DisableWow64FsRedirection pfnWow64DisableWowFsRedirection=(LPFN_Wow64DisableWow64FsRedirection)GetProcAddress(GetModuleHandle(_T("kernel32")),"Wow64DisableWow64FsRedirection");
    LPFN_Wow64RevertWow64FsRedirection pfnWow64RevertWowFsRedirection=(LPFN_Wow64RevertWow64FsRedirection)GetProcAddress(GetModuleHandle(_T("kernel32")),"Wow64RevertWow64FsRedirection");
    bool b=false;
    if(pfnWow64DisableWowFsRedirection && pfnWow64RevertWowFsRedirection)
        b=pfnWow64DisableWowFsRedirection(&OldValue);

    std::wstring cp=System.ExpandEnvVar(L"%windir%\\System32\\control.exe");
    System.run_command(cp.c_str(),cmd,SW_NORMAL,0);

    if(b && pfnWow64DisableWowFsRedirection && pfnWow64RevertWowFsRedirection)
        pfnWow64RevertWowFsRedirection(OldValue);
}

int SystemImp::_vscwprintf_dll(const wchar_t * _Format,va_list _ArgList)
{
    if(hinstLib==NULL)
    {
        hinstLib=LoadLibrary(L"msvcrt.dll");
        if(hinstLib!=NULL)_vscwprintf_func=(WINAPI5_vscwprintf)GetProcAddress(hinstLib,"_vscwprintf");
    }
    return _vscwprintf_func?_vscwprintf_func(_Format,_ArgList):1024*4;
}

std::string SystemImp::wtoa (const std::wstring& wstr)
{
   return (std::string(wstr.begin(), wstr.end()));
}

std::wstring SystemImp::AppPathW()
{
    std::wstring path;
    std::wstring long_path;
    path.resize(MAX_PATH, 0);
    auto path_size(GetModuleFileNameW(nullptr, &path.front(), MAX_PATH));
    path.resize(path_size);

    int length=GetLongPathNameW(path.c_str(),nullptr,0);
    wchar_t* buffer = new wchar_t[length];
    length = GetLongPathNameW(path.c_str(), buffer, length);
    long_path=std::wstring(buffer);
    long_path=long_path.substr(0,long_path.find_last_of(L"\\/"));

    return long_path;
}

std::string SystemImp::AppPathS()
{
    std::wstring path=AppPathW();
    return wtoa(path);
}

int SystemImp::FindLatestExeVersion(int bit)
{
    int ver=VERSION_REV;
    std::wstring spec;
    if(bit==32)spec=AppPathW()+L"\\SDI*.exe";
    else if(bit==64)spec=AppPathW()+L"\\SDI_x64*.exe";
    else return 0;

    WIN32_FIND_DATA fd;
    HANDLE hFind=::FindFirstFile(spec.c_str(), &fd);
    if(hFind!=INVALID_HANDLE_VALUE)
    {
        do
        {
            if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
            {
                std::wstring v=fd.cFileName;
                int vi=0;
                if(bit==32)vi=_wtoi(v.substr(6,3).c_str());
                else if(bit==64)vi=_wtoi(v.substr(10,3).c_str());
                if(vi>ver)ver=vi;
            }
        }while(::FindNextFile(hFind,&fd));
        ::FindClose(hFind);
    }
    return ver;
}

int SystemImp::getver(const char *s)
{
    while(*s)
    {
        if(*s=='_'&&s[1]>='0'&&s[1]<='9')
            return atoi(s+1);

        s++;
    }
    return 0;
}

int SystemImp::getcurver(const char *ptr)
{
    // find the latest version of the given
    // driver pack in my list
    WStringShort bffw;

    bffw.sprintf(L"%S",ptr);
    wchar_t *s=bffw.GetV();
    while(*s)
    {
        if(*s=='_'&&s[1]>='0'&&s[1]<='9')
        {
            *s=0;
            s=const_cast<wchar_t *>(manager_g->matcher->finddrp(bffw.Get()));
            if(!s)return 0;
            while(*s)
            {
                if(*s==L'_'&&s[1]>=L'0'&&s[1]<=L'9')
                    return _wtoi_my(s+1);

                s++;
            }
            return 0;
        }
        s++;
    }
    return 0;
}

bool SystemImp::SystemProtectionEnabled(State *state)
{
    // windows version
    int vMajor, vMinor;
    state->getWinVer(&vMajor,&vMinor);
    // windows less than XP not supported
    if(vMajor<5)return false;

    // this reads the 64 bit registry from either 32-bit or 64-bit application
    // if i can't find it or read it i'll assume yes
    bool ret=true;
    DWORD err;
    HKEY hkey;
    err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore",0,KEY_READ|KEY_WOW64_64KEY,&hkey);
    if(err==ERROR_SUCCESS)
    {
        DWORD dwData;
        DWORD cbData=sizeof(DWORD);
        DWORD dwType=REG_DWORD;
        // windows XP
        if(vMajor==5)
        {
            err=RegQueryValueEx(hkey,L"DisableSR",nullptr,&dwType,(LPBYTE)&dwData,&cbData);
            if(err==ERROR_SUCCESS)ret=dwData==0;
            else if(err==ERROR_FILE_NOT_FOUND)ret=false;
            else Log.print_err("ERROR in SystemProtectionEnabled(): error in RegQueryValueEx %d\n",err);
        }
        // every other version
        else
        {
            err=RegQueryValueEx(hkey,L"RPSessionInterval",nullptr,&dwType,(LPBYTE)&dwData,&cbData);
            if(err==ERROR_SUCCESS)ret=dwData==1;
            else if(err==ERROR_FILE_NOT_FOUND)ret=false;
            else Log.print_err("ERROR in SystemProtectionEnabled(): error in RegQueryValueEx %d\n",err);
        }

        RegCloseKey(hkey);
    }
    else Log.print_err("ERROR in SystemProtectionEnabled(): error in RegOpenKeyEx %d\n",err);
    return ret;
}

int SystemImp::GetRestorePointCreationFrequency()
{
    // this reads the 64 bit registry from either 32-bit or 64-bit application
    // -2 error opening key, -1 value not found
    int ret=-2;
    HKEY hkey;
    DWORD err;
    err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore",0,KEY_READ|KEY_WOW64_64KEY,&hkey);
    if(err==ERROR_SUCCESS)
    {
        DWORD dwData;
        DWORD cbData=sizeof(DWORD);
        DWORD dwType=REG_DWORD;
        err=RegQueryValueEx(hkey,L"SystemRestorePointCreationFrequency",nullptr,&dwType,(LPBYTE)&dwData,&cbData);
        if(err==ERROR_SUCCESS)ret=dwData;
        else ret=-1;
        RegCloseKey(hkey);
    }
    else Log.print_err("ERROR in GetRestorePointCreationFrequency(): error in RegOpenKeyEx %d\n",err);
    return ret;
}

void SystemImp::SetRestorePointCreationFrequency(int freq)
{
    // this reads the 64 bit registry from either 32-bit or 64-bit application
    if(freq==-2)return;
    HKEY hkey;
    DWORD err;
    err=RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore",0,KEY_WRITE|KEY_WOW64_64KEY,&hkey);
    if(err==ERROR_SUCCESS)
    {
        DWORD dwData=freq;
        DWORD cbData=sizeof(DWORD);
        DWORD dwType=REG_DWORD;
        // -1 means delete the value, otherwise set the value
        if(freq==-1)
            RegDeleteValue(hkey,L"SystemRestorePointCreationFrequency");
        else
            RegSetValueEx(hkey,L"SystemRestorePointCreationFrequency",0,dwType,(LPBYTE)&dwData,cbData);
        RegCloseKey(hkey);
    }
    else Log.print_err("ERROR in SetRestorePointCreationFrequency(): error in RegOpenKeyEx %d\n",err);
}

bool SystemImp::CreateRestorePoint(std::wstring desc)
{
    RESTOREPOINTINFOW pRestorePtSpec;
    STATEMGRSTATUS pSMgrStatus;
    WINAPI5t_SRSetRestorePointW WIN5f_SRSetRestorePointW;
    bool restorePointSucceeded=false;

        // get the current state of restore points
        int restorePointFrequency=System.GetRestorePointCreationFrequency();
        // set to always create a restore point
        System.SetRestorePointCreationFrequency(0);
        hinstLib=LoadLibrary(L"SrClient.dll");
        if(hinstLib!=NULL)
            WIN5f_SRSetRestorePointW=(WINAPI5t_SRSetRestorePointW)GetProcAddress(hinstLib,"SRSetRestorePointW");

        if(hinstLib&&WIN5f_SRSetRestorePointW)
        {
            memset(&pRestorePtSpec,0,sizeof(RESTOREPOINTINFOW));
            pRestorePtSpec.dwEventType=BEGIN_SYSTEM_CHANGE;
            pRestorePtSpec.dwRestorePtType=DEVICE_DRIVER_INSTALL;
            wcscpy(pRestorePtSpec.szDescription,desc.c_str());
            if(Settings.flags&FLAG_DISABLEINSTALL)
            {
                Sleep(2000);
                restorePointSucceeded=true;
            }
            else
            {
                restorePointSucceeded=WIN5f_SRSetRestorePointW(&pRestorePtSpec,&pSMgrStatus);
                Log.print_debug("Restore Point: %d (%d)\n",(int)restorePointSucceeded,pSMgrStatus.nStatus);
            }
            // return it to the state we found it in
            System.SetRestorePointCreationFrequency(restorePointFrequency);

            if(!restorePointSucceeded)
            {
                if(pSMgrStatus.nStatus==ERROR_SERVICE_DISABLED)
                    Log.print_err("ERROR: CreateRestorePoint : Failed to create restore point. Restore points disabled.\n");
                else
                    Log.print_err("ERROR: CreateRestorePoint : Failed to create restore point.\n");
            }
        }
        else
            Log.print_err("ERROR: CreateRestorePoint : Failed to create restore point %d\n",hinstLib);

        if(hinstLib)FreeLibrary(hinstLib);
    return restorePointSucceeded;
}

bool SystemImp::GetNonPresentDevices()
{
    wchar_t buf[BUFLEN];
    buf[0]=0;
    DWORD ret;
    ret=GetEnvironmentVariable(L"DEVMGR_SHOW_NONPRESENT_DEVICES",buf,BUFLEN);
    if(ret==0)return false;
    ret=_wtoi(buf);
    return (ret);
}

//{ FileMonitor
FilemonImp::FilemonImp(const wchar_t *szDirectory, int subdirs_, FileChangeCallback callback_)
{
	wcscpy(data.dir,szDirectory);

	data.hDir=CreateFile(szDirectory,FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
	                            nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr);

	if(data.hDir!=INVALID_HANDLE_VALUE)
	{
		data.ol.hEvent    = CreateEvent(nullptr,TRUE,FALSE,nullptr);
		data.notifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_FILE_NAME;
		data.callback     = callback_;
		data.subdirs      = subdirs_;

		if(refresh(data))
		{
			return;
		}
		else
		{
			CloseHandle(data.ol.hEvent);
			CloseHandle(data.hDir);
			data.hDir=INVALID_HANDLE_VALUE;
		}
	}
}

int FilemonImp::refresh(FilemonDataPOD &data1)
{
	return ReadDirectoryChangesW(data1.hDir,data1.buffer,sizeof(data1.buffer),data1.subdirs,
	                      data1.notifyFilter,nullptr,&data1.ol,monitor_callback);
}

void CALLBACK FilemonImp::monitor_callback(DWORD dwErrorCode,DWORD dwNumberOfBytesTransfered,LPOVERLAPPED lpOverlapped)
{
    UNREFERENCED_PARAMETER(dwNumberOfBytesTransfered);

	wchar_t szFile[MAX_PATH];
	PFILE_NOTIFY_INFORMATION pNotify;
	FilemonDataPOD *pMonitor=reinterpret_cast<FilemonDataPOD*>(lpOverlapped);

	if(dwErrorCode==ERROR_SUCCESS)
	{
        size_t offset=0;
        do
		{
			pNotify=(PFILE_NOTIFY_INFORMATION)&pMonitor->buffer[offset];
			offset+=pNotify->NextEntryOffset;

            wcsncpy(szFile,pNotify->FileName,pNotify->FileNameLength/sizeof(wchar_t)+1);

			if(!monitor_pause)
			{
                FILE *f;
                wchar_t buf[BUFLEN];
                int m=0,flag;
                size_t sz=0;

                errno=0;
                wsprintf(buf,L"%ws\\%ws",pMonitor->dir,szFile);
                Log.print_con("{\n  changed'%S'\n",buf);
                f=_wfsopen(buf,L"rb",0x10); //deny read/write mode
                if(f)m=2;
                if(!f)
                {
                    f=_wfopen(buf,L"rb");
                    if(f)m=1;
                }
                if(f)
                {
                    _fseeki64(f,0,SEEK_END);
                    sz=static_cast<size_t>(_ftelli64(f));
                    fclose(f);
                }
                /*
                    m0: failed to open
                    m1: opened(normal)
                    m2: opened(exclusive)
                */
                switch(pNotify->Action)
                {
                    case 1: // Added
                        flag=errno?0:1;
                        break;

                    case 2: // Removed
                        flag=1;
                        break;

                    case 3: // Modifed
                        flag=errno?0:1;
                        break;

                    case 4: // Renamed(old name)
                        flag=0;
                        break;

                    case 5: // Renamed(new name)
                        flag=1;
                        break;

                    default:
                        flag=0;
                }

                Log.print_con("  %c a(%d),m(%d),err(%02d),size(%9d)\n",flag?'+':'-',pNotify->Action,m,errno,sz);

                if(flag)pMonitor->callback(szFile,pNotify->Action,(int)pMonitor->lParam);
                Log.print_con("}\n\n");
			}
		}while(pNotify->NextEntryOffset!=0);
	}
	if(!pMonitor->fStop)refresh(*pMonitor);
}

FilemonImp::~FilemonImp()
{
	if(data.hDir!=INVALID_HANDLE_VALUE)
    {
        data.fStop=TRUE;
        CancelIo(data.hDir);
        if(!HasOverlappedIoCompleted(&data.ol))SleepEx(5,TRUE);
        CloseHandle(data.ol.hEvent);
        CloseHandle(data.hDir);
    }
}
//}

#ifdef BENCH_MODE
void SystemImp::benchmark()
{
    char buf[BUFLEN];
    wchar_t bufw[BUFLEN];
    long long i,tm1,tm2;
    int a;

    // wsprintfA is faster but doesn't support %f
    tm1=GetTickCountWr();
    for(i=0;i<1024*1024;i++)wsprintfA(buf,"Test str %ws",L"wchar str");
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024;i++)sprintf(buf,"Test str %ws",L"wchar str");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c wsprintfA \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c sprintf   \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024;i++)wsprintfW(bufw,L"Test str %s",L"wchar str");
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024;i++)swprintf(bufw,L"Test str %s",L"wchar str");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c wsprintfW \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c swprintf  \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)_strcmpi("Test str %ws","wchar str");
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)strcmpi("Test str %ws","wchar str");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c _strcmpi \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c strcmpi  \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)strcasecmp("Test str %ws","wchar str");
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)strcmpi("Test str %ws","wchar str");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c strcasecmp \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c strcmpi  \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)StrCmpIW(L"Test str %ws",L"wchar str");
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)wcsicmp(L"Test str %ws",L"wchar str");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c StrCmpIW \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c wcsicmp  \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)StrCmpIA("Test str %ws","wchar str");
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)strcmpi("Test str %ws","wchar str");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c StrCmpIA \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c strcmpi  \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)StrCmpW(L"Test str %ws",bufw); // StrCmpW == lstrcmp
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)wcscmp(L"Test str %ws",bufw);
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c StrCmpW \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c wcscmp  \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)a=StrCmpA("Test str %ws",buf);
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)a=strcmp("Test str %ws",buf);
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c StrCmpA \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c strcmp \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)StrStrA("Test str %ws",buf);
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)strstr("Test str %ws","Test str %ws");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c StrStrA \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c strstr \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)StrStrW(L"Test str %ws",bufw);
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)wcsstr(L"Test str %ws",bufw);
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c StrStrW \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c wcsstr  \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5*2;i++)lstrlenA(buf);
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5*2;i++)strlen(buf);
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c lstrlenA \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c strlen   \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5*2;i++)lstrlenW(bufw);
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5*2;i++)wcslen(bufw);
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c lstrlenW \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c wcslen   \t%ld\n\n",tm1>tm2?'+':' ',tm2);

    tm1=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)StrCpyA(buf,"Test str %ws");
    tm1=GetTickCountWr()-tm1;
    tm2=GetTickCountWr();
    for(i=0;i<1024*1024*5;i++)strcpy(buf,"Test str %ws");
    tm2=GetTickCountWr()-tm2;
    Log.print_con("%c StrCpyA \t%ld\n",tm1<tm2?'+':' ',tm1);
    Log.print_con("%c strcpy \t%ld\n\n",tm1>tm2?'+':' ',tm2);
}
#endif
//}

//{ Virus detection
void viruscheck(const wchar_t *szFile,int action,int lParam)
{
    UNREFERENCED_PARAMETER(szFile);
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(lParam);

    int type;
    int update=0;

    if(Settings.flags&FLAG_NOVIRUSALERTS)return;
    type=GetDriveType(nullptr);

    // autorun.inf
    if(type!=DRIVE_CDROM)
    {
        if(System.FileExists(L"\\autorun.inf"))
        {
            FILE *f;
            f=_wfopen(L"\\autorun.inf",L"rb");
            if(f)
            {
                char buf[BUFLEN];
                fread(buf,BUFLEN,1,f);
                fclose(f);
                buf[BUFLEN-1]=0;
                if(!StrStrIA(buf,"[NOT_A_VIRUS]")&&StrStrIA(buf,"open"))
                    manager_g->itembar_setactive(SLOT_VIRUS_AUTORUN,update=1);
            }
            else
                Log.print_con("NOTE: cannot open autorun.inf [error: %d]\n",errno);
        }
    }

    // RECYCLER
    if(type==DRIVE_REMOVABLE)
        if(System.FileExists(L"\\RECYCLER")&&!System.FileExists(L"\\RECYCLER\\not_a_virus.txt"))
            manager_g->itembar_setactive(SLOT_VIRUS_RECYCLER,update=1);

    // Hidden folders
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind=FindFirstFile(L"\\*.*",&FindFileData);
    if(type==DRIVE_REMOVABLE)
    while(FindNextFile(hFind,&FindFileData)!=0)
    {
        if(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
        {
            if(lstrcmp(FindFileData.cFileName,L"..")==0)continue;
            if(lstrcmpi(FindFileData.cFileName,L"System Volume Information")==0)continue;

            if(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN)
            {
                WStringShort bufw;
                bufw.sprintf(L"\\%ws\\not_a_virus.txt",FindFileData.cFileName);
                if(System.FileExists(bufw.Get()))continue;
                Log.print_con("VIRUS_WARNING: hidden folder '%S'\n",FindFileData.cFileName);
                manager_g->itembar_setactive(SLOT_VIRUS_HIDDEN,update=1);
            }
        }
    }
    FindClose(hFind);
    if(update)
    {
        manager_g->setpos();
        SetTimer(MainWindow.hMain,1,1000/60,nullptr);
    }
}
//}

static BOOL CALLBACK ShowHelpProcedure(HWND hwnd,UINT Message,WPARAM wParam,LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    HWND hEditBox;
    LPCSTR s;
    size_t sz;

    switch(Message)
    {
    case WM_INITDIALOG:
        SetWindowTextW(hwnd,L"Command Line options help");
        hEditBox=GetDlgItem(hwnd,0);
        SetWindowTextW(hEditBox,L"Command Line options");
        //ShowWindow(hEditBox,SW_HIDE);
        hEditBox=GetDlgItem(hwnd,IDOK);
        ShowWindow(hEditBox,SW_HIDE);
        hEditBox=GetDlgItem(hwnd,IDCANCEL);
        SetWindowTextW(hEditBox,L"Close");

        get_resource(IDR_CLI_HELP,(void **)&s,&sz);
        hEditBox=GetDlgItem(hwnd,IDC_EDIT1);

        SendMessage(hEditBox,WM_SETFONT,(WPARAM)CLIHelp_Font,0);

        SetWindowTextA(hEditBox,s);
        SendMessage(hEditBox,EM_SETREADONLY,1,0);

        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
            EndDialog(hwnd,IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hwnd,IDCANCEL);
            return TRUE;

        default:
            break;
        }
        break;

    case WM_CTLCOLORSTATIC:
        hEditBox=GetDlgItem(hwnd,IDC_EDIT1);
        if((HWND)lParam==hEditBox)
        {
            HDC hdcStatic=(HDC)wParam;
            SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetStockObject(HOLLOW_BRUSH);
        }
        else return TRUE;

    default:
        break;
    }
    return false;
}

void ShowHelp()
{
    CLIHelp_Font=CreateFont(-12,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH,L"Consolas");

    DialogBox(ghInst,MAKEINTRESOURCE(IDD_LICENSE),nullptr,(DLGPROC)ShowHelpProcedure);
    DeleteObject(CLIHelp_Font);
}
