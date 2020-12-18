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

#include "com_header.h"
#include "common.h"
#include "logging.h"
#include "system.h"
#include "settings.h"
#include "cli.h"

#include <cstring>

#define INSTALLEDVENFILENAMEDEFPATH L"%temp%\\SDI2\\InstalledID.txt"

// Structures
struct CommandLineParam_t
{
    bool ShowHelp;
    bool SaveInstalledHWD;
    wchar_t SaveInstalledFileName[BUFLEN];
    bool HWIDInstalled;
    wchar_t HWIDSTR[BUFLEN];
};
CommandLineParam_t CLIParam;

static void ExpandPath(wchar_t *Apath)
{
    std::wstring infoBuf;
    infoBuf=System.ExpandEnvVar(std::wstring(Apath));
    wcscpy(Apath,infoBuf.c_str());
}

void SaveHWID(wchar_t *hwid)
{
    if(CLIParam.SaveInstalledHWD)
    {
        FILE *f=_wfopen(CLIParam.SaveInstalledFileName,L"a+");
        if(!f)
        {
            Log.print_err("Failed to create '%S'\n",CLIParam.SaveInstalledFileName);
            return;
        }
        fwprintf(f,L"%s",hwid);
        fwprintf(f,L"\n");
        fclose(f);
    }
}

void Parse_save_installed_id_swith(const wchar_t *ParamStr)
{
    size_t tmpLen=wcslen(SAVE_INSTALLED_ID_DEF);

    if(wcslen(ParamStr)>tmpLen)
    {
        if(ParamStr[tmpLen]==L':')
            wcscpy(CLIParam.SaveInstalledFileName,ParamStr+tmpLen+1);
        else if(ParamStr[tmpLen]==L' ')
            wcscpy(CLIParam.SaveInstalledFileName,INSTALLEDVENFILENAMEDEFPATH);
        else return;
    }
    else
        wcscpy(CLIParam.SaveInstalledFileName,INSTALLEDVENFILENAMEDEFPATH);

    CLIParam.SaveInstalledHWD=true;
}

void Parse_HWID_installed_swith(const wchar_t *ParamStr)
{
    size_t tmpLen=wcslen(HWIDINSTALLED_DEF);
    if(wcslen(ParamStr)<(tmpLen+17)) //-HWIDInstalled:VEN_xxxx&DEV_xxxx
    {
        Log.print_err("invalid parameter %S\n",ParamStr);
        ret_global=24;//ERROR_BAD_LENGTH;
        Settings.statemode=STATEMODE_EXIT;
        return;
    }
    else
    {
        WStringShort buf;
        buf.append(ParamStr+tmpLen);
        const wchar_t *chB;

        chB=wcsrchr(buf.Get(),'=');
        if(chB==NULL)
            wcscpy(CLIParam.SaveInstalledFileName,INSTALLEDVENFILENAMEDEFPATH);
        else
        {
            tmpLen=chB-buf.Get()+1;
            wcscpy(CLIParam.SaveInstalledFileName,buf.Get()+tmpLen);
            (buf.GetV())[tmpLen-1]=0;
        }
        wcscpy(CLIParam.HWIDSTR,buf.Get());
        CLIParam.HWIDInstalled=true;
    }
}

void init_CLIParam()
{
    memset(&CLIParam,0,sizeof(CLIParam));
}

void RUN_CLI()
{
    if(CLIParam.SaveInstalledHWD)
    {
        ExpandPath(CLIParam.SaveInstalledFileName);
        WStringShort buf;
        buf.append(CLIParam.SaveInstalledFileName);
        System.fileDelSpec(buf.GetV());
        System.CreateDir(buf.Get());
        System.deletefile(CLIParam.SaveInstalledFileName);
    }
    else
        if(CLIParam.HWIDInstalled)
        {
            ExpandPath(CLIParam.SaveInstalledFileName);
            FILE *f;
            f=_wfopen(CLIParam.SaveInstalledFileName,L"rt");
            if(!f)Log.print_err("Failed to open '%S'\n",CLIParam.SaveInstalledFileName);
            else
            {
                wchar_t buf[BUFLEN];
                while(fgetws(buf,sizeof(buf)/2,f))
                {
                    //Log.print_con("'%S'\n", buf);
                    if(wcsstr(buf,CLIParam.HWIDSTR)!=NULL)
                    {
                        ret_global=1;
                        break;
                    }
                }
                fclose(f);
            }
            Settings.flags|=FLAG_AUTOCLOSE|FLAG_NOGUI;
            Settings.statemode=STATEMODE_EXIT;
        }
}
