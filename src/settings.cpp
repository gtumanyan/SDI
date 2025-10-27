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

#include "SDI.h"
#include "logging.h"
#include "system.h"
#include "Settings.h"
#include "cli.h"
#include "update.h"
#include "install.h"
#include "theme.h"

#include <windows.h>
#include <Shlwapi.h>
#include <shellapi.h>       // for CommandLineToArgvW

#include "enum.h"

int volatile installmode=MODE_NONE;
int invaidate_set;
uint8_t num_cores;
int ret_global=0;

Settings_t::Settings_t()
{
    *curlang=0;
    wcscpy(curtheme,  L"(default)");
    wcscpy(logO_dir,  L"Logs");

    wcscpy(drp_dir,   L"Drivers");
    wcscpy(output_dir,L"indexes\\txt");
    *drpext_dir=0;
    wcscpy(index_dir, L"indexes");
    wcscpy(data_dir,  L"Tools");
    *log_dir=0;

    wcscpy(state_file,L"untitled.snp");
    *finish=0;
    *finish_upd=0;
    *finish_rb=0;

    flags=COLLECTION_USE_LZMA;
    statemode=STATEMODE_REAL;
    expertmode=0;
    hintdelay=500;
    license=0;
    scale=256;
    savedscale=scale;
    wndwx=0,wndwy=0;wndsc=1;
    filters=
        (1<<ID_SHOW_MISSING)+
        (1<<ID_SHOW_NEWER)+
        (1<<ID_SHOW_BETTER)+
        (1<<ID_SHOW_NF_MISSING)+
        (1<<ID_SHOW_ONE);
    virtual_os_version=0;
    virtual_arch_type=0;
}

bool Settings_t::argstr(const wchar_t *s,const wchar_t *cmp,wchar_t *d)
{
    if(StrStrIW(s,cmp)) {wcscpy(d,s+wcslen(cmp));return true;}
    return false;
}

bool Settings_t::argint(const wchar_t *s,const wchar_t *cmp,int *d)
{
    if(StrStrIW(s,cmp)){*d=_wtoi_my(s+wcslen(cmp));return true;}
    return false;
}

bool Settings_t::argopt(const wchar_t *s,const wchar_t *cmp,int *d)
{
    if(StrStrIW(s,cmp)){*d=1;return true;}
    return false;
}

bool Settings_t::argflg(const wchar_t *s,const wchar_t *cmp,int f)
{
    if(!_wcsicmp(s,cmp)){ flags|=f;return true; }
    return false;
}

// Если в комстроке указан действительный путь, установить драйвера из этой папки
bool Settings_t::install(WCHAR* cmdLine)
{
    if (PathFileExists(cmdLine)) {
        wcscpy_s(drpext_dir, cmdLine);
        flags |= FLAG_AUTOCLOSE |
            FLAG_AUTOINSTALL | FLAG_NORESTOREPOINT | FLAG_DPINSTMODE |//FLAG_DISABLEINSTALL|
            FLAG_PRESERVECFG;
        return true;
    }
    else return false;
}

void Settings_t::parse(const WCHAR* cmdLine,size_t ind) {
    WinVersions winVersions;

    uprintf("Args:[%S]\n",cmdLine);



    int nArgs;

    WCHAR** argsArr=CommandLineToArgvW(cmdLine,&nArgs);
    for(size_t i=ind;i<static_cast<size_t>(nArgs);i++)
    {
        wchar_t *pr=argsArr[i];


        if(pr[0]=='/') pr[0]='-';

        if(argstr(pr,L"-drp_dir:",       drp_dir))	continue;
        if(argstr(pr,L"-index_dir:",     index_dir))continue;
        if(argstr(pr,L"-output_dir:",    output_dir))continue;
        if(argstr(pr,L"-data_dir:",      data_dir))continue;
        if(argstr(pr,L"-log_dir:",       logO_dir))continue;

        if(argstr(pr,L"-finish_cmd:",    finish))continue;
        if(argstr(pr,L"-finishrb_cmd:",  finish_rb))continue;
        if(argstr(pr,L"-finish_upd_cmd:",finish_upd))continue;

        if(argstr(pr,L"-lang:",          curlang))continue;
        if(argstr(pr,L"-theme:",         curtheme))continue;
        if(argint(pr,L"-hintdelay:",     &hintdelay))continue;
        if(argint(pr,L"-license:",       &license))continue;
        if(argint(pr,L"-scale:",         &scale))continue;
        if(argint(pr,L"-wndwx:",         &wndwx))continue;
        if(argint(pr,L"-wndwy:",         &wndwy))continue;
        if(argint(pr,L"-wndsc:",         &wndsc))continue;
        if(argint(pr,L"-filters:",       &filters))continue;

        if(argint(pr,L"-port:",          &Updater->port))continue;
        if(argint(pr,L"-downlimit:",     &Updater->downlimit))continue;
        if(argint(pr,L"-uplimit:",       &Updater->uplimit))continue;
        if(argint(pr,L"-connections:",   &Updater->connections))continue;
        if(argint(pr,L"-activetorrent:", &Updater->activetorrent))continue;

        if(argopt(pr,L"-expertmode",     &expertmode))continue;
        if(argflg(pr,L"-showconsole",    FLAG_SHOWCONSOLE))continue;
        if(argflg(pr,L"-norestorepnt",   FLAG_NORESTOREPOINT))continue;
        if(argflg(pr,L"-novirusalerts",  FLAG_NOVIRUSALERTS))continue;
        if(argflg(pr,L"-preservecfg",    FLAG_PRESERVECFG))continue;
        if(argflg(pr,L"-hideBOOSTY",     FLAG_HIDEBOOSTY))continue;

        if(argflg(pr,L"-showdrpnames1",  FLAG_SHOWDRPNAMES1))continue;
        if(argflg(pr,L"-showdrpnames2",  FLAG_SHOWDRPNAMES2))continue;
        if(argflg(pr,L"-oldstyle",       FLAG_OLDSTYLE))continue;

        if(argflg(pr,L"-checkupdates",   FLAG_CHECKUPDATES))continue;
        if(argflg(pr,L"-onlyupdates",    FLAG_ONLYUPDATES))continue;

        if(!_wcsicmp(pr,L"-7z"))
        {
            WStringShort cmd;
            cmd.sprintf(L"7za.exe %s",StrStrIW(cmdLine,L"-7z")+4);
            uprintf("Executing '%S'\n",cmd.Get());
            registerall();
            ret_global=Extract7z(cmd.Get());
            uprintf("Ret: %d\n",ret_global);
            statemode=STATEMODE_EXIT;
            break;
        }

        if(!_wcsicmp(pr,L"-PATH"))
        {
            wcscpy(drpext_dir,argsArr[++i]);
            flags|=FLAG_AUTOCLOSE|
                FLAG_AUTOINSTALL|FLAG_NORESTOREPOINT|FLAG_DPINSTMODE|//FLAG_DISABLEINSTALL|
                FLAG_PRESERVECFG;
            continue;
        }
        if(!_wcsicmp(pr,L"-install")&&nArgs-i==3)
        {
            wchar_t buf[MAX_PATH*2 + 15];
            uprintf("Install '%S' '%S'\n",argsArr[i+1],argsArr[i+2]);
            GetEnvironmentVariable(L"TEMP",buf,MAX_PATH);
            wsprintf(extractdir,L"%s\\SDI",buf);
            installmode=MODE_INSTALLING;
            driver_install(argsArr[i+1],argsArr[i+2],&ret_global,&needreboot);
            uprintf("Ret: %X,%d\n",ret_global,needreboot);
            if(needreboot)ret_global|=0x80000000;
            wsprintf(buf,L" /c rd /s /q \"%s\"",extractdir);
            System.run_command(L"cmd",buf,SW_HIDE,1);
            statemode=STATEMODE_EXIT;
            break;
        }
        if(!_wcsicmp(pr,L"-?"))
        {
            ShowHelp();
            //Settings.flags|=FLAG_AUTOCLOSE|FLAG_NOGUI;
            Settings.statemode=STATEMODE_EXIT;
            break;
        }

        if(argflg(pr,L"-filtersp",       FLAG_FILTERSP)){ flags&=~COLLECTION_USE_LZMA;continue; }
        if(argflg(pr,L"-reindex",        COLLECTION_FORCE_REINDEXING))continue;
        if(argflg(pr,L"-index_hr",       COLLECTION_PRINT_INDEX))continue;
        if(argflg(pr,L"-nogui",          FLAG_NOGUI))continue;
        if(argflg(pr,L"-autoinstall",    FLAG_AUTOINSTALL))continue;
        if(argflg(pr,L"-autoclose",      FLAG_AUTOCLOSE))continue;
        if(argflg(pr,L"-autoupdate",     FLAG_AUTOUPDATE))continue;
        if(argflg(pr,L"-nostop",         FLAG_NOSTOP))continue;

        if(argstr(pr,L"-extractdir:",    extractdir)){ flags|=FLAG_EXTRACTONLY;continue; }
        if(argflg(pr,L"-keepunpackedindex",FLAG_KEEPUNPACKINDEX))continue;
        if(argflg(pr,L"-keeptempfiles",  FLAG_KEEPTEMPFILES))continue;
        if(argflg(pr,L"-disableinstall", FLAG_DISABLEINSTALL))continue;
        if(argflg(pr,L"-failsafe",       FLAG_FAILSAFE))continue;
        if(argflg(pr,L"-delextrainfs",   FLAG_DELEXTRAINFS))continue;

        if(argstr(pr,L"-ls:",            state_file)){ statemode=STATEMODE_EMUL;;continue; }
        if(argflg(pr,L"-verbose:",       gReducedLogging=false))continue;
        if(argflg(pr,L"-nologfile",      FLAG_NOLOGFILE))continue;
        if(argflg(pr,L"-nosnapshot",     FLAG_NOSNAPSHOT))continue;
        if(argflg(pr,L"-nostamp",        FLAG_NOSTAMP))continue;
        if(argstr(pr,L"-getdevicelist:", device_list_filename))continue;

        if(argflg(pr,L"-a:32",           0)){ virtual_arch_type=32;continue; }
        if(argflg(pr,L"-a:64",           0)){ virtual_arch_type=64;continue; }
        if(argint(pr,L"-v:",             &virtual_os_version))
        {
            virtual_os_version=winVersions.GetVersionIndex(virtual_os_version,false)+ID_OS_ITEMS;
            continue;
        }
        if( StrStrIW(pr,SAVE_INSTALLED_ID_DEF))
            Parse_save_installed_id_swith(pr);
        else if( StrStrIW(pr,HWIDINSTALLED_DEF))
            Parse_HWID_installed_swith(pr);
        else if( StrStrIW(pr,GFG_DEF))
            continue;


        else uprintf("Unknown argument '%S'\n",pr);
        if(statemode==STATEMODE_EXIT)break;
    }

    Settings.savedscale=Settings.scale;
    ExpandEnvironmentStrings(logO_dir,log_dir,MAX_PATH);
    LocalFree(argsArr);
    if(statemode==STATEMODE_EXIT)return;
}

void Settings_t::save()
{
    if(flags&FLAG_PRESERVECFG)return;
    if(!System.canWriteFile(L"SDI2.cfg",L"wt"))
    {
        uprintf("ERROR in settings_save(): Write-protected,'SDI2.cfg'\n");
        return;
    }
    FILE *f=_wfopen(L"SDI2.cfg",L"wt");
    if(!f)return;
    fwprintf(f,L"\"-drp_dir:%ws\"\n\"-index_dir:%ws\"\n\"-output_dir:%ws\"\n"
              L"\"-data_dir:%ws\"\n\"-log_dir:%ws\"\n\n"
              L"\"-finish_cmd:%ws\"\n\"-finishrb_cmd:%ws\"\n\"-finish_upd_cmd:%ws\"\n\n"
              L"\"-lang:%ws\"\n\"-theme:%ws\"\n-hintdelay:%d\n-license:%d\n"
              L"-wndwx:%d\n-wndwy:%d\n-wndsc:%d\n-scale:%d\n-filters:%d\n\n"
              L"-port:%d\n-downlimit:%d\n-uplimit:%d\n-connections:%d\n\n",
            drp_dir,index_dir,output_dir,
            data_dir,logO_dir,
            finish,finish_rb,finish_upd,
            STR(STR_LANG_ID),curtheme,hintdelay,license?1:0,wndwx,wndwy,wndsc,autosized?savedscale:scale,filters,
            Updater->port,Updater->downlimit,Updater->uplimit,Updater->connections);

    if(expertmode)fwprintf(f,L"-expertmode ");
    if(flags&FLAG_SHOWCONSOLE)fwprintf(f,L"-showconsole ");
    if(flags&FLAG_NORESTOREPOINT)fwprintf(f,L"-norestorepnt ");
    if(flags&FLAG_NOVIRUSALERTS)fwprintf(f,L"-novirusalerts ");
    if(flags&FLAG_HIDEBOOSTY)fwprintf(f,L"-hideBOOSTY ");

    if(flags&FLAG_SHOWDRPNAMES1)fwprintf(f,L"-showdrpnames1 ");
    if(flags&FLAG_SHOWDRPNAMES2)fwprintf(f,L"-showdrpnames2 ");
    if(flags&FLAG_OLDSTYLE)fwprintf(f,L"-oldstyle ");

    if(flags&FLAG_CHECKUPDATES)fwprintf(f,L"-checkupdates ");
    if(flags&FLAG_ONLYUPDATES)fwprintf(f,L"-onlyupdates ");
    if(flags&FLAG_KEEPUNPACKINDEX)fwprintf(f,L"-keepunpackedindex ");
    fclose(f);
}

void Settings_t::loginfo()
{
    if(!gReducedLogging)
    {
        uprintfs("Settings\n");
        uprintf("  drp_dir='%S'\n",drp_dir);
        uprintf("  index_dir='%S'\n",index_dir);
        uprintf("  output_dir='%S'\n",output_dir);
        uprintf("  data_dir='%S'\n",data_dir);
        uprintf("  log_dir='%S'\n",log_dir);
        uprintf("  extractdir='%S'\n",extractdir);
        uprintf("  lang=%S\n",curlang);
        uprintf("  theme=%S\n",curtheme);
        uprintf("  scale=%d\n",scale);
        uprintf("  expertmode=%d\n",expertmode);
        uprintf("  filters=%d\n",filters);
        uprintf("  autoinstall=%d\n",(flags&FLAG_AUTOINSTALL)?1:0);
        uprintf("  autoclose=%d\n",(flags&FLAG_AUTOCLOSE)?1:0);
        uprintf("  failsafe=%d\n",(flags&FLAG_FAILSAFE)?1:0);
        uprintf("  delextrainfs=%d\n",(flags&FLAG_DELEXTRAINFS)?1:0);
        uprintf("  checkupdates=%d\n",(flags&FLAG_CHECKUPDATES)?1:0);
        uprintf("  norestorepnt=%d\n",(flags&FLAG_NORESTOREPOINT)?1:0);
        uprintf("  disableinstall=%d\n",(flags&FLAG_DISABLEINSTALL)?1:0);
        uprintf("  nostop=%d\n",(flags&FLAG_NOSTOP)?1:0);
        uprintfs("\n");

        if(statemode==STATEMODE_EMUL)uprintf("Virtual system system config '%S'\n",state_file);
        if(virtual_arch_type)uprintf("Virtual Windows version: %d-bit\n",virtual_arch_type);
        if(virtual_os_version)uprintf("Virtual Windows version: %d.%d\n",virtual_os_version/10,virtual_os_version%10);
        uprintfs("\n");
    }
}

bool Settings_t::load(const wchar_t *filename) {
		wchar_t buf[1024];

    if(!loadCFGFile(filename,buf)) return false;
    parse(buf,0);
    return true;
}

wchar_t *Settings_t::ltrim(wchar_t *s)
{
    while(iswspace(*s)) s++;
    return s;
}

bool Settings_t::loadCFGFile(const wchar_t *FileName,wchar_t *DestStr)
{
    FILE *f;
    wchar_t Buff[MAX_PATH];

    *DestStr=0;

    ExpandEnvironmentStringsW(FileName,Buff,MAX_PATH);
    uprintf("Loading %S\n",Buff);
    f=_wfopen(Buff,L"rt");
    if(!f)
    {
        uprintf("Failed to load %S\n",Buff);
        return false;
    }

    while(fgetws(Buff,sizeof(Buff)/2,f))
    {
        wcscpy(Buff,ltrim(Buff));       //  trim spaces
        if(*Buff=='#')continue;         // comments
        if(*Buff==';')continue;         // comments
        if(*Buff=='/')*Buff='-';        // replace / with -
        if(::wcsstr(Buff,L"-?"))continue; // ignore -?
        if(Buff[wcslen(Buff)-1]=='\n')Buff[wcslen(Buff)-1]='\0';
        if(!*Buff)continue;

        wcscat(wcscat(DestStr,Buff),L" ");
    }
    fclose(f);
    return true;
}

bool Settings_t::load_cfg_switch(const wchar_t *cmdParams)
{
    wchar_t **argv;
    int argc;

    size_t len=wcslen(GFG_DEF);
    argv=CommandLineToArgvW(cmdParams,&argc);
    for(int i=1;i<argc;i++)
    {
        wchar_t *pr=argv[i];
        if(pr[0]=='/')pr[0]='-';
        if(StrStrIW(pr,GFG_DEF))
        {
            if(load(pr+len))
            {
                flags|=FLAG_PRESERVECFG;
                return true;
            }
        }
    }
    return false;
}
