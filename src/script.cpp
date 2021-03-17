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

#include "script.h"
#include "logging.h"

#include <windows.h>
#include <setupapi.h>       // for CommandLineToArgvW
#include <iostream>
#include "settings.h"
#include "system.h"
#include "shellapi.h"

#include "matcher.h"
#include "indexing.h"
#include "enum.h"
#include "main.h"
#include "model.h"
#include "update.h"
#include "install.h"
#include <iostream>

extern Event *deviceupdate_event;
extern volatile int deviceupdate_exitflag;
extern int bundle_display;
extern int bundle_shadow;
extern Manager manager_v[2];

Bundle bundle[2];
extern wchar_t extractdir[BUFLEN];

Script::Script()
{
    //ctor
}

Script::~Script()
{
    //dtor
}

bool Script::cmdArgIsPresent()
{
    int argc;
    wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(size_t i=1;i<static_cast<size_t>(argc);i++)
    {
        wchar_t *pr=argv[i];
        if(pr[0]=='/')pr[0]='-';
        if(StrStrIW(pr,L"-script"))
            return true;
    }
    return false;
}

bool Script::loadscript()
{
    bool ret=false;
    FILE *f;
    wchar_t Buff[BUFLEN]=L"";

    // get command line
    int argc;
    wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(size_t i=1;i<static_cast<size_t>(argc);i++)
    {
        wchar_t *pr=argv[i];
        if(pr[0]=='/')pr[0]='-';

        if(StrStrIW(pr,L"-script"))
        {
            // get file script file name
            wcscpy(Buff, &pr[8]);
            if(wcslen(Buff)==0)
            {
                std::cout << "Command line argument missing.\n";
                std::cout << "Usage: SDI -script:file.txt\n";
                break;
            }

            // %0 is always the script file name
            wchar_t w[BUFLEN]=L"";
            ExpandEnvironmentStringsW(Buff, w, BUFLEN);
            parameters.clear();
            parameters.push_back(w);
        }
        else if(parameters.size()>0)
            parameters.push_back(pr);
    }

    // load the script
    if(parameters.size()>0)
    {
        // open the script file
        f=_wfopen(parameters[0].c_str(),L"rt");
        if(!f)
            std::wcout << L"Failed to open " << parameters[0].c_str() << L"\n";
        else
        {
            while(fgetws(Buff,sizeof(Buff)/2,f))
            {
                wcscpy(Buff,ltrim(Buff));       // trim spaces
                if(*Buff=='#')continue;         // comments
                if(*Buff==';')continue;         // comments
                // add the line to my list
                if(wcslen(Buff)>0)
                {
                    // trim the line feed
                    if(Buff[wcslen(Buff)-1]==L'\n')
                        Buff[wcslen(Buff)-1]=L'\0';
                    std::wstring w(Buff);
                    doParameters(w);
                    ScriptText.push_back(w);
                }
            }
            fclose(f);
            ret=true;
        }
    }

    return ret;
}

bool Script::runscript()
{
    if(ScriptText.size()==0)return false;

    Settings.flags=FLAG_NOGUI|FLAG_NOLOGFILE|FLAG_NOSNAPSHOT|
                   FLAG_PRESERVECFG|FLAG_SCRIPTMODE|COLLECTION_USE_LZMA;
    Settings.filters=0;
    std::wstring logdir(Settings.logO_dir);
    int LastExitCode=0;
    bool NeedReboot=false;
    Log.set_verbose(0);
    Updater=CreateUpdater();
    int torrentport=Updater->torrentport;

    // get the system drive
    wchar_t systemDrive[BUFLEN]={0};
    GetEnvironmentVariable(L"SystemDrive",systemDrive,BUFLEN);
    wcscat(systemDrive,L"\\temp");

    // temp directory
    wchar_t buf[BUFLEN];
    GetEnvironmentVariable(L"TEMP",buf,BUFLEN);

    // if the TEMP environment variable is not set then use the system drive
    if(wcslen(buf)==0)
        wcscpy(buf,systemDrive);

    GetEnvironmentVariable(L"TEMP",buf,BUFLEN);
    wsprintf(extractdir,L"%s\\SDI",buf);

    // iterate the script text lines
    for(std::vector<std::wstring>::iterator txt = ScriptText.begin(); txt != ScriptText.end(); ++txt)
    {
        std::wstring w=*txt;

        //extract the command and arguments
        std::vector<std::wstring>args=split(w, L' ');

        // extract all arguments as a single string
        std::wstring allargs;
        size_t p=w.find(' ');
        if(p!=std::string::npos)
        {
            allargs=w.substr(p+1,std::string::npos);
        }

        // process the command
        if(args.size()>0)
        {
            Log.print_debug("Command: %S\n",args[0].c_str());

            if(StrStrIW(args[0].c_str(),L"init"))
            {
                bool r=false;
                if(args.size()>1)
                {
                    r=StrStrIW(args[1].c_str(),L"reindex");
                    Log.print_debug("Argument: %S\n",args[1].c_str());
                }
                if(r)Settings.flags|=COLLECTION_FORCE_REINDEXING;
                else Settings.flags&=~COLLECTION_FORCE_REINDEXING;
                manager_v[0].init(bundle[bundle_display].getMatcher());
                manager_v[1].init(bundle[bundle_display].getMatcher());
                bundle[bundle_display].bundle_prep();
                // following triggers deviceupdate_event->raise
                invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_INDEXES|INVALIDATE_MANAGER);
                LastExitCode=Bundle::thread_loadall(&bundle);
                Settings.flags&=~COLLECTION_FORCE_REINDEXING;
            }
            else if(_wcsicmp(args[0].c_str(),L"activetorrent")==0)
            {
                if(args.size()>1)
                {
                    Updater_t::activetorrent=std::stoi(args[1]);
                    Log.print_debug("Argument: %d\n",Updater_t::activetorrent);
                    #ifdef USE_TORRENT
                    delete Updater;
                    Updater=CreateUpdater();
                    #endif // USE_TORRENT
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: activetorrent : Missing argument\n");
                }
            }
            else if(StrStrIW(args[0].c_str(),L"select"))
            {
                int filter=0;
                std::vector<std::wstring> drpfilter;
                for(size_t i=1;i<args.size();i++)
                {
                    std::wstring s(args[i]);
                    // trim spaces
                    s.erase(std::remove_if(s.begin(), s.end(), &std::isspace), s.end());  // not1 deprecated in C++17 (removed in C++20)
                    if(_wcsicmp(s.c_str(),L"missing")==0)filter|=2;
                    else if(_wcsicmp(s.c_str(),L"newer")==0)filter|=4;
                    else if(_wcsicmp(s.c_str(),L"current")==0)filter|=8;
                    else if(_wcsicmp(s.c_str(),L"older")==0)filter|=16;
                    else if(_wcsicmp(s.c_str(),L"better")==0)filter|=32;
                    else if(_wcsicmp(s.c_str(),L"worse")==0)filter|=64;
                    else if(s.length()>0)drpfilter.push_back(s);
                }
                if((filter>0)||(drpfilter.size()>0))
                {
                    Log.print_con("Select: filter:%d, drpfilter:%d\n",filter,drpfilter.size());
                    selectNone();
                    Settings.filters=filter;
                    manager_g->filter(filter,&drpfilter);
                    selectAll();
                    LastExitCode=0;
                    Log.print_con("%d drivers selected\n",manager_g->selected());
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: select : Missing argument\n");
                }
            }
            else if(StrStrIW(args[0].c_str(),L"checkupdates"))
            {
                LastExitCode=Updater->scriptInitUpdates(torrentport);
            }
            else if(_wcsicmp(args[0].c_str(),L"get")==0)
            {
                if(args.size()>1)
                {
                    if(_wcsicmp(args[1].c_str(),L"app")==0)
                    {
                        LastExitCode=Updater->scriptDownloadApp();
                        if(!LastExitCode)Log.print_con("Application downloaded successfully\n");
                        else Log.print_con("Application download failed\n");
                    }
                    else if(_wcsicmp(args[1].c_str(),L"indexes")==0)
                    {
                        LastExitCode=Updater->scriptDownloadIndexes();
                        if(!LastExitCode)Log.print_con("Indexes downloaded successfully\n");
                        else Log.print_con("Indexes download failed\n");
                    }
                    else if(_wcsicmp(args[1].c_str(),L"driverpacks")==0)
                    {
                        if(args.size()>2)
                        {
                            if(_wcsicmp(args[2].c_str(),L"all")==0||
                               _wcsicmp(args[2].c_str(),L"missing")==0||
                               _wcsicmp(args[2].c_str(),L"updates")==0||
                               _wcsicmp(args[2].c_str(),L"selected")==0)
                            {
                                Log.print_debug("Argument: %S\n",args[2].c_str());
                                LastExitCode=Updater->scriptDownloadDrivers(args[2]);
                                if(!LastExitCode)Log.print_con("Driver packs downloaded successfully\n");
                                else Log.print_con("Driver packs download failed\n");
                            }
                            else
                            {
                                LastExitCode=1;
                                Log.print_err("Error: get driverpacks : invalid argument\n");
                            }
                        }
                        else
                        {
                            LastExitCode=1;
                            Log.print_err("Error: get driverpacks : missing argument\n");
                        }
                    }
                    else if(_wcsicmp(args[1].c_str(),L"everything")==0)
                    {
                        LastExitCode=Updater->scriptDownloadEverything();
                        if(!LastExitCode)Log.print_con("Everything downloaded successfully\n");
                        else Log.print_con("Download failed\n");
                    }
                    else
                    {
                        LastExitCode=1;
                        Log.print_err("Error: get : Invalid argument\n");
                    }
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: get : Missing argument\n");
                }
            }
            else if(_wcsicmp(args[0].c_str(),L"install")==0)
            {
                LastExitCode=Updater->scriptInstall();
                NeedReboot=(ret_global>>24)&0x40;
                unsigned int failed=(ret_global>>16)&255;
                unsigned int installed=ret_global&65535;
                Log.print_debug("Installer return code: %d, ",ret_global);
                Log.print_con("%d drivers installed, %d drivers failed.\n",installed,failed);
            }
            else if(_wcsicmp(args[0].c_str(),L"snapshot")==0)
            {
                Settings.flags&=~FLAG_NOSNAPSHOT;
                Log.gen_timestamp();
                wchar_t filename[BUFLEN];
                if(args.size()>1)
                    wsprintf(filename, L"%S", args[1].c_str());
                else
                    wsprintf(filename,L"%s\\%sstate.snp",logdir.c_str(),Log.getTimestamp());
                Log.print_debug("Argument: %S\n",filename);
                State *state=manager_g->matcher->getState();
                LastExitCode=state->save(filename);
                Log.print_con("Snapshot %S\n",LastExitCode?L"failed":L"succeeded");
                Settings.flags|=FLAG_NOSNAPSHOT;
            }
            else if(_wcsicmp(args[0].c_str(),L"loadsnapshot")==0)
            {
                // this must be placed immediately before the init command
                if(args.size()>1)
                {
                    Log.print_debug("Argument: %S\n",args[1].c_str());
                    if(System.FileExists2(args[1].c_str()))
                    {
                        wcscpy(Settings.state_file,args[1].c_str());
                        Settings.statemode=STATEMODE_EMUL;
                    }
                    else
                    {
                        Log.print_err("Error: loadsnapshot : File not found.");
                        LastExitCode=1;
                    }
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: loadsnapshot : Missing argument\n");
                }
            }
            else if(_wcsicmp(args[0].c_str(),L"unloadsnapshot")==0)
            {
                // this must be placed immediately before the init command
                Settings.statemode=STATEMODE_REAL;
            }
            else if(StrStrIW(args[0].c_str(),L"restorepoint"))
            {
                std::wstring desc=L"Snappy Driver Installer";
                if(args.size()>1)
                {
                    desc=L"";
                    for(std::vector<std::wstring>::size_type i = 1; i != args.size(); i++)
                    {
                        desc.append(args[i].c_str());
                        desc.append(L" ");
                    }
                }
                Log.print_debug("Argument: %S\n",desc.c_str());
                LastExitCode=!System.CreateRestorePoint(desc);
                Log.print_con("Restore point %S: %S\n",LastExitCode?L"failed":L"succeeded", desc.c_str());
            }
            else if(StrStrIW(args[0].c_str(),L"writedevicelist"))
            {
                if(args.size()>1)
                {
                    Log.print_debug("Argument: %S\n",args[1].c_str());
                    wchar_t arg[BUFLEN];
                    wcscpy(arg,args[1].c_str());
                    LastExitCode=manager_g->matcher->write_device_list(arg);
                    Log.print_con("Write Device List %S\n",LastExitCode?L"failed":L"succeeded");
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: writedevicelist : Missing argument\n");
                }
            }
            else if(StrStrIW(args[0].c_str(),L"logdir"))
            {
                if(allargs.length()>0)
                {
                    logdir=allargs;
                    Log.print_debug("Argument: %S\n",allargs.c_str());
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: logdir : Missing argument\n");
                }
            }
            else if(StrStrIW(args[0].c_str(),L"drpdir"))
            {
                if(allargs.length()>0)
                {
                    Log.print_debug("Argument: %S\n",allargs.c_str());
                    wcscpy(Settings.drp_dir,allargs.c_str());
                    invalidate(INVALIDATE_INDEXES|INVALIDATE_MANAGER);
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: drpdir : Missing argument\n");
                }
            }
            else if(StrStrIW(args[0].c_str(),L"indexdir"))
            {
                if(allargs.length()>0)
                {
                    Log.print_debug("Argument: %S\n",allargs.c_str());
                    wcscpy(Settings.index_dir,allargs.c_str());
                    invalidate(INVALIDATE_INDEXES|INVALIDATE_MANAGER);
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: indexdir : Missing argument\n");
                }
            }
            else if (StrStrIW(args[0].c_str(),L"extractdir"))
            {
                if(allargs.length()>0)
                {
                    Log.print_debug("Argument: %S\n",allargs.c_str());
                    wcscpy(extractdir,allargs.c_str());
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: extractdir : Missing argument\n");
                }
            }
            else if(StrStrIW(args[0].c_str(),L"torrentport"))
            {
                if(args.size()>1)
                {
                    torrentport=std::stoi(args[1]);
                    Log.print_debug("Argument: %d\n",torrentport);
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: torrentport : Missing argument\n");
                }
            }
            else if(_wcsicmp(args[0].c_str(),L"keeptempfiles")==0)
            {
                if(args.size()>1)
                {
                    if(StrStrIW(args[1].c_str(),L"on"))
                        Settings.flags|=FLAG_KEEPTEMPFILES;
                    else if(StrStrIW(args[1].c_str(),L"off"))
                        Settings.flags&=~FLAG_KEEPTEMPFILES;
                    else
                        Log.print_err("Error: keeptempfiles : Invalid argument");
                }
                Log.print_debug("Keep Temp Files is %S\n",Settings.flags&FLAG_KEEPTEMPFILES?L"on":L"off");
            }
            else if(StrStrIW(args[0].c_str(),L"echo"))
            {
                int v=Log.get_verbose();
                Log.set_verbose(v|LOG_VERBOSE_LOG_CON);
                Log.print_con("%S\n",w.c_str());
                Log.set_verbose(v);
            }
            else if(StrStrIW(args[0].c_str(),L"debug"))
            {
                int v=Log.get_verbose();
                if(args.size()>1)
                {
                    if(StrStrIW(args[1].c_str(),L"on"))
                        v|=LOG_VERBOSE_DEBUG;
                    else if(StrStrIW(args[1].c_str(),L"off"))
                        v&=~LOG_VERBOSE_DEBUG;
                    Log.set_verbose(v);
                }
                Log.print_debug("Debug is %S\n",v&LOG_VERBOSE_DEBUG?L"on":L"off");
            }
            else if(StrStrIW(args[0].c_str(),L"logging"))
            {
                if(args.size()>1)
                {
                    if(StrStrIW(args[1].c_str(),L"on"))
                    {
                        Settings.flags&=~FLAG_NOLOGFILE;
                        wchar_t arg[BUFLEN];
                        wcscpy(arg,logdir.c_str());
                        Log.start(arg);
                    }
                    else if(StrStrIW(args[1].c_str(),L"off"))
                    {
                        Settings.flags|=FLAG_NOLOGFILE;
                        Log.stop();
                    }
                }
                Log.print_debug("Logging is %S\n",Settings.flags&FLAG_NOLOGFILE?L"off":L"on");
            }
            else if(StrStrIW(args[0].c_str(),L"cmd"))
            {
                if(args.size()>1)
                {
                    std::wstring cmd=L" /c ";
                    for(std::vector<std::wstring>::size_type i = 1; i != args.size(); i++)
                        cmd.append(args[i]+L" ");
                    LastExitCode=System.run_command(L"cmd",cmd.c_str(),SW_SHOWNORMAL,1);
                }
                else
                {
                    LastExitCode=1;
                    Log.print_err("Error: cmd : Missing argument\n");
                }
            }
            else if(StrStrIW(args[0].c_str(),L"verbose"))
            {
                if(args.size()>1)
                {
                    int v=std::stoi(args[1]);
                    Log.set_verbose(v);
                }
                Log.print_debug("Verbose is %d\n",Log.get_verbose());
            }
            else if(StrStrIW(args[0].c_str(),L"enableinstall"))
            {
                if(args.size()>1)
                {
                    if(StrStrIW(args[1].c_str(),L"on"))
                        Settings.flags&=~FLAG_DISABLEINSTALL;
                    else if(StrStrIW(args[1].c_str(),L"off"))
                        Settings.flags|=FLAG_DISABLEINSTALL;
                }
                Log.print_debug("Install is %S\n",Settings.flags&FLAG_DISABLEINSTALL?L"off":L"on");
            }
            else if(StrStrIW(args[0].c_str(),L"pause"))
            {
                system("pause");
            }
            else if(_wcsicmp(args[0].c_str(),L"reboot")==0)
            {
                bool doReboot=true;
                if((args.size()>1)&&(_wcsicmp(args[1].c_str(),L"ifneeded")==0))
                    doReboot=NeedReboot;
                if(doReboot)
                {
                    wcscpy(buf,L" /c Shutdown.exe -r -t 5");
                    LastExitCode=System.run_command(L"cmd",buf,SW_HIDE,0);
                }
            }
            else if(_wcsicmp(args[0].c_str(),L"runlatest")==0)
            {
                std::wstring cmd;
                for(std::vector<std::wstring>::size_type i = 1; i != args.size(); i++)
                    cmd.append(L" "+args[i]);
                RunLatest(cmd);
            }
            else if(StrStrIW(args[0].c_str(),L"onerror"))
            {
                if(LastExitCode)
                {
                    if(args.size()>1)
                    {
                        if(StrStrIW(args[1].c_str(),L"end"))
                        {
                            Log.print_err("Last operation failed, ending\n");
                            break;
                        }
                        else if(_wcsicmp(args[1].c_str(),L"goto")==0)
                        {
                            if(args.size()>2)
                            {
                                std::wstring label=args[2];
                                Log.print_debug("Argument: %S\n",label.c_str());
                                p=label.find(L":");
                                if(p==std::string::npos)
                                    label.insert(0, L":");
                                for(std::vector<std::wstring>::iterator txt2 = ScriptText.begin(); txt2 != ScriptText.end(); ++txt2)
                                {
                                    std::wstring w2=*txt2;
                                    if(_wcsicmp(w2.c_str(),label.c_str())==0)
                                    {
                                        // this will put the for loop pointer to the line with the label
                                        // the for loop will then naturally iterate to the following line
                                        txt=txt2;
                                        break;
                                    }
                                }
                            }
                            else
                                Log.print_err("Error: goto : Missing argument\n");

                        }
                    }
                    else
                        Log.print_err("Error: onerror : Missing argument\n");
                }
            }
            else if(_wcsicmp(args[0].c_str(),L"goto")==0)
            {
                if(args.size()>1)
                {
                    std::wstring label=args[1];
                    Log.print_debug("Argument: %S\n",label.c_str());
                    p=label.find(L":");
                    if(p==std::string::npos)
                        label.insert(0, L":");
                    for(std::vector<std::wstring>::iterator txt2 = ScriptText.begin(); txt2 != ScriptText.end(); ++txt2)
                    {
                        std::wstring w2=*txt2;
                        if(_wcsicmp(w2.c_str(),label.c_str())==0)
                        {
                            // this will put the for loop pointer to the line with the label
                            // the for loop will then naturally iterate to the following line
                            txt=txt2;
                            break;
                        }
                    }
                }
                else
                    Log.print_err("Error: goto : Missing argument\n");
            }
            else if(StrStrIW(args[0].c_str(),L"end"))
            {
               break;
            }
            else
            {
                // label
                std::wstring label=args[0];
                p=label.find(L":");
                if(p!=0)
                {
                    LastExitCode=1;
                    Log.print_con("Invalid command\n");
                }
            }
        }
    }

    return true;
}

wchar_t *Script::ltrim(wchar_t *s)
{
    while(iswspace(*s)) s++;
    return s;
}

void Script::selectNone()
{
    manager_g->filter(126);
    manager_g->itembar_setactive(SLOT_RESTORE_POINT,0);
    manager_g->selectnone();
}

void Script::selectAll()
{
    manager_g->itembar_setactive(SLOT_RESTORE_POINT,0);
    manager_g->selectall();
}

int Script::updatesInitialised()
{
    int ret=!(Settings.flags&FLAG_UPDATESOK);
    if(ret)Log.print_err("Error: get : Updates not initialised");
    return ret;
}

void Script::doParameters(std::wstring &cmd)
{
    for(size_t i=0;i<=9;i++)
    {
        std::wstring rp=L"%";
        rp.append(std::to_wstring(i));
        size_t p=cmd.find(rp);
        if(p!=std::string::npos)
        {
            if(i<parameters.size())
                cmd.replace(p,2,parameters[i]);
            else
                cmd.replace(p,2,L"");
        }
    }
}

void Script::RunLatest(std::wstring args)
{
    // this would be run after an application update
    // and should be followed by an "end"

    // get the correct executable
    std::wstring cmd;
    #ifdef _WIN64
        cmd=L"SDI_x64"+std::to_wstring(System.FindLatestExeVersion(64))+L".exe";
    #else
        cmd=L"SDI"+std::to_wstring(System.FindLatestExeVersion(32))+L".exe";
    #endif // _WIN64

    if(!System.FileExists2(cmd.c_str()))
    {
        Log.print_err("File not found: %S\n",cmd.c_str());
        return;
    }

    cmd+=args;
    Log.print_con("Cmd: %S\n",cmd.c_str());

    STARTUPINFO si;
    ZeroMemory(&si,sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi,sizeof(pi));

    CreateProcess(nullptr,(LPWSTR)cmd.c_str(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP|CREATE_NEW_CONSOLE,nullptr,nullptr,&si,&pi);
}
