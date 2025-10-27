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

#include <windows.h>
#include <SRRestorePtAPI.h> // for RestorePoint

#include "SDI.h"
#include "cli.h"
#include "Settings.h"
#include "gui.h"
#include "system.h"
#include "install.h"
#include "manager.h"
#include "matcher.h"
#include "resource.h"
#include "shellapi.h"
#include "theme.h"
#include "update.h"

typedef BOOL (WINAPI *PFN_SETRESTOREPTW)(PRESTOREPOINTINFOW pRestorePtSpec,PSTATEMGRSTATUS pSMgrStatus);
#include "device.h"

// Depend on Win32API
#include "enum.h"

// Autoclicker
#define AUTOCLICKER_CONFIRM
#define NUM_CLICKDATA 7
struct wnddata_t
{
		// Main wnd
		int wnd_wx,wnd_wy;
		int cln_wx,cln_wy;

		// Install button
		int btn_x, btn_y;
		int btn_wx,btn_wy;
};

class Autoclicker_t
{
		static const wnddata_t clicktbl[NUM_CLICKDATA];
		static volatile int clicker_flag;

private:
		void calcwnddata(wnddata_t *w,HWND hwnd);
		int cmpclickdata(int *a,int *b);
		static int CALLBACK EnumWindowsProc(HWND hwnd,LPARAM lParam);

public:
		void setflag(int v){clicker_flag=v;}
		void wndclicker(int mode);
		static unsigned int __stdcall thread_clicker(void *arg);
};

//{ Global vars
long long ar_total,ar_proceed;
int instflag;
size_t itembar_act;
int needreboot=0;
wchar_t extractdir[MAX_PATH + 1];

volatile int Autoclicker_t::clicker_flag;
Autoclicker_t Autoclicker;

long long totalinstalltime,totalextracttime;
long long installtime,extracttime;
//}

//{ Installer
void _7z_total(long long i)
{
		ar_proceed=0;
		ar_total=i;
}

int _7z_setcomplete(long long i)
{
		if(Settings.statemode==STATEMODE_EXIT)return S_OK;
		if(installmode==MODE_STOPPING)
		{
				uprintf("MODE_STOPPING\n");
				return E_ABORT;
		}
		if(manager_g->items_list.empty())return S_OK;
		if(!manager_g->items_list[itembar_act].checked)
		{
				uprintf("stop:itembar_act %d\n",itembar_act);
				return E_ABORT;
		}

		ar_proceed=i;
		//log("PR %d/%d\n",ar_proceed,ar_total);
		manager_g->items_list[itembar_act].updatecur();
		manager_g->updateoverall();
		MainWindow.redrawfield();
		return S_OK;
}

void driver_install(wchar_t *hwid,const wchar_t *inf,int *ret,int *needrb)
{
		WStringShort cmd;
		WStringShort buf;
		void *install64bin;
		ThreadAbs *thr=CreateThread();
		size_t size;
		FILE *f;

		*ret=1;*needrb=1;
		cmd.sprintf(L"%s\\install64.exe",extractdir);
		if(!System.FileExists(cmd.Get()))
		{
				mkdir_r(extractdir);
				uprintf("Dir: (%S)\n",extractdir);
				f=_wfopen(cmd.Get(),L"wb");
				if(f)
				{
						uprintf("Created '%S'\n",cmd.Get());
						get_resource(IDR_INSTALL64,&install64bin,&size);
						fwrite(install64bin,1,size,f);
						fclose(f);
				}
				else
						uprintf("Unable to create '%S': %s",cmd.Get(), WindowsErrorString());
		}

		Autoclicker.setflag(1);
		//const char* logFilePath = ToUtf8(Settings.log_dir, CP_UTF8);
  //  WriteCurrentLogToFile(logFilePath);
		thr->start(&Autoclicker_t::thread_clicker,nullptr);
		{
				if(Settings.flags&FLAG_DISABLEINSTALL)
						Sleep(2000);
				else
						*ret=UpdateDriverForPlugAndPlayDevices(MainWindow.hMain,hwid,inf,INSTALLFLAG_FORCE,needrb);
		}

		if(!*ret)*ret=GetLastError();
		if((Settings.flags&FLAG_DISABLEINSTALL)==0)
		if((unsigned)*ret==0xE0000235||manager_g->matcher->getState()->getArchitecture())//ERROR_IN_WOW64
		{
				buf.sprintf(L"\"%s\" \"%s\"",hwid,inf);
				cmd.sprintf(L"%s\\install64.exe",extractdir);
				uprintf("'%S %S'\n",cmd.Get(),buf.Get());
				*ret=System.run_command(cmd.Get(),buf.Get(),SW_HIDE,1);
				if((*ret&0x7FFFFFFF)==1)
				{
						*needrb=(*ret&0x80000000)?1:0;
						*ret&=~0x80000000;
				}
		}
		Autoclicker.setflag(0);

		thr->join();
		delete thr;
		if(*ret==1)SaveHWID(hwid);
}

void removeextrainfs(wchar_t *inf)
{
		wchar_t buf[MAX_PATH];
		wchar_t *s=inf;
		HANDLE hFind;
		WIN32_FIND_DATA FindFileData;

		while(wcschr(s,L'\\'))s=wcschr(s,L'\\')+1;

		wcscpy(buf,inf);
		wcscpy(buf+(s-inf),L"*.inF");
		hFind=FindFirstFile(buf,&FindFileData);
		if(hFind!=INVALID_HANDLE_VALUE)
		do
		if(!(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
		{
				wcscpy(buf+(s-inf),FindFileData.cFileName);
				if(!StrStrIW(inf,FindFileData.cFileName))
						uprintf("deleting %S (%d)\n",buf,DeleteFile(buf));
				else
						uprintf("keeping  %S\n",buf);
		}
		while(FindNextFile(hFind,&FindFileData)!=0);
}

unsigned int __stdcall Manager::thread_install(void *arg)
{
		UNREFERENCED_PARAMETER(arg);

		itembar_t *itembar,*itembar1;
    wchar_t cmd[MAX_PATH];
		wchar_t hwid[86];
		wchar_t inf[MAX_PATH];
		wchar_t buf[MAX_PATH];
		size_t i,j;
		RESTOREPOINTINFOW pRestorePtSpec;
		STATEMGRSTATUS pSMgrStatus = { };
		HINSTANCE hinstLib=nullptr;
		PFN_SETRESTOREPTW WIN5f_SRSetRestorePointW = NULL;
		int failed=0,installed=0;

		if(CRITICAL_SECTION_ACTIVE)EnterCriticalSection(&sync);

		// Prepare extract dir
		uprintf("extractdir='%S'\n",extractdir);

		installmode=MODE_INSTALLING;
		manager_g->items_list[SLOT_EXTRACTING].install_status=
				(instflag&INSTALLDRIVERS)?STR_INST_INSTALLING:STR_EXTR_EXTRACTING;
		manager_g->items_list[SLOT_EXTRACTING].isactive=1;
		manager_g->setpos();

		ClickVisiter cv{ID_REBOOT,CHECKBOX::GET};
		wPanels->Accept(cv);

		if(isRebootDesired())Settings.flags|=FLAG_AUTOINSTALL;

		// Download driverpacks
#ifdef USE_TORRENT
		if((Settings.flags&(FLAG_SCRIPTMODE|FLAG_UPDATESOK)) ||
				(!(Settings.flags&FLAG_SCRIPTMODE)))
		{
				unsigned downdrivers=0;
				itembar=&manager_g->items_list[RES_SLOTS];
				// find which items are selected
				// check if the associated driver pack is set to DRIVERPACK_TYPE_UPDATE
				// and set it's priority in the torrent
				for(i=RES_SLOTS;i<manager_g->items_list.size()&&installmode==MODE_INSTALLING;i++,itembar++)
						if(itembar->checked&&itembar->isactive&&itembar->hwidmatch&&itembar->hwidmatch->getdrp_packontorrent())
				{
						if(!Updater->isTorrentReady())
						{
								uprintf("Waiting for torrent");
								for(j=0;j<200;j++)
								{
										uprintf("*");
										if(Updater->isTorrentReady())
										{
												uprintf("DONE\n");
												break;
										}
										Sleep(100);
								}
								if(!Updater->isTorrentReady())break;
								uprintf("\n");
						}
						Updater->SetFilePriority(itembar->hwidmatch->getdrp_packname(),libtorrent::low_priority);
						downdrivers++;
				}
				// if any DRIVERPACK_TYPE_UPDATE items are selected in the torrent
				// then do the download
				if(downdrivers)
				{
						Updater->resumeDownloading();
						uprintf("{{{{{{{{\n");
						while(installmode&&!Updater->isUpdateCompleted())
						{
								Sleep(500);
						}
						uprintf("{}}}}}}}}}\n");
				}
				if(installmode==MODE_STOPPING)
				{
						itembar->install_status=STR_INST_STOPPING;
						manager_g->items_list[SLOT_EXTRACTING].install_status=STR_INST_STOPPING;
						manager_g->selectnone();
				}
		}
#endif

		// Restore point
		bool restorePointSelected=manager_g->items_list[SLOT_RESTORE_POINT].checked;
		bool restorePointSucceeded=false;
		if(restorePointSelected)
		{
				// get the current state of restore points
				int restorePointFrequency=System.GetRestorePointCreationFrequency();
				// set to always create a restore point
				System.SetRestorePointCreationFrequency(0);
				// System Restore client dll
				hinstLib=LoadLibrary(L"SrClient.dll");
				if(hinstLib!=NULL)
						WIN5f_SRSetRestorePointW=(PFN_SETRESTOREPTW)GetProcAddress(hinstLib,"SRSetRestorePointW");

				if(hinstLib&&WIN5f_SRSetRestorePointW)
				{
						manager_g->items_list[SLOT_RESTORE_POINT].percent=500;
						manager_g->items_list[SLOT_RESTORE_POINT].install_status=STR_REST_CREATING;
						itembar_act=SLOT_RESTORE_POINT;
						MainWindow.redrawfield();

						memset(&pRestorePtSpec,0,sizeof(RESTOREPOINTINFOW));
						pRestorePtSpec.dwEventType=BEGIN_SYSTEM_CHANGE;
						pRestorePtSpec.dwRestorePtType=DEVICE_DRIVER_INSTALL;
						wcscpy(pRestorePtSpec.szDescription,L"Installed drivers");
						if(CRITICAL_SECTION_ACTIVE)LeaveCriticalSection(&sync);
						if(Settings.flags&FLAG_DISABLEINSTALL)
						{
								Sleep(2000);
								restorePointSucceeded=true;
						}
						else
						{
								restorePointSucceeded=WIN5f_SRSetRestorePointW(&pRestorePtSpec,&pSMgrStatus);
								uprintf("rt rest point{ %d(%d)\n",(int)restorePointSucceeded,pSMgrStatus.nStatus);
						}
						if(CRITICAL_SECTION_ACTIVE)EnterCriticalSection(&sync);

						manager_g->items_list[SLOT_RESTORE_POINT].percent=1000;
						if(restorePointSucceeded)
						{
								manager_g->items_list[SLOT_RESTORE_POINT].install_status=STR_REST_CREATED;
						}else if(pSMgrStatus.nStatus==ERROR_SERVICE_DISABLED)
						{
								manager_g->items_list[SLOT_RESTORE_POINT].install_status=STR_RESTOREPOINTS_DISABLED;
								uprintf("ERROR in thread_install: Failed to create restore point. Restore points disabled.\n");
						}else
						{
								manager_g->items_list[SLOT_RESTORE_POINT].install_status=STR_REST_FAILED;
								uprintf("ERROR in thread_install: Failed to create restore point\n");
						}
				}
				else
				{
						manager_g->items_list[SLOT_RESTORE_POINT].install_status=STR_REST_FAILED;
						uprintf("ERROR in thread_install: Failed to create restore point %d\n",hinstLib);
				}
				MainWindow.redrawfield();
				if(hinstLib)FreeLibrary(hinstLib);
				// return it to the state we found it in
				System.SetRestorePointCreationFrequency(restorePointFrequency);
				//
				manager_g->set_rstpnt(0);
				manager_g->items_list[SLOT_RESTORE_POINT].percent=0;
		}
		totalextracttime=totalinstalltime=0;
		wsprintf(buf,L"%ws\\SetupAPI.dev.log",manager_g->matcher->getState()->textas.getw(manager_g->matcher->getState()->getWindir()));
		_wremove(buf);

		// if restore point was selected and failed then abort at this point
		if((restorePointSelected&&restorePointSucceeded)||(!restorePointSelected)||(Settings.flags&FLAG_NOSTOP))
		{

		goaround:
				itembar=&manager_g->items_list[0];
				for(i=0;i<manager_g->items_list.size()&&installmode==MODE_INSTALLING;i++,itembar++)
						if(i>=RES_SLOTS&&itembar->checked&&itembar->isactive&&itembar->hwidmatch)
				{
						int unpacked=0;
						int limits[7];
						Hwidmatch *hwidmatch=itembar->hwidmatch;

						memset(limits,0,sizeof(limits));
						itembar_act=i;
						ar_proceed=0;
						uprintf("Installing $%04d\n",i);
						hwidmatch->print_hr();
            wsprintf(cmd,L"%s\\%S",extractdir,hwidmatch->getdrp_infpath());

						manager_g->animstart=System.GetTickCountWr();
						MainWindow.offset_target=(itembar->curpos>>16);
						SetTimer(MainWindow.hMain,1,1000/60,nullptr);

						// Extract
						extracttime=System.GetTickCountWr();
						wsprintf(inf,L"%s\\%S%S",
										unpacked?hwidmatch->getdrp_packpath():extractdir,
										hwidmatch->getdrp_infpath(),
										hwidmatch->getdrp_infname());
						uprintf("%S\n",hwidmatch->getdrp_packname());
						if(System.FileExists(inf))
						{
								uprintf("Already unpacked(%S)\n",inf);
								_7z_total(100);
								_7z_setcomplete(100);
								MainWindow.redrawfield();
						}
						else
						if(wcsstr(hwidmatch->getdrp_packname(),L"unpacked.7z"))
						{
								uprintf("'%S' Unpacked\n",hwidmatch->getdrp_packpath());
								unpacked=1;
								_7z_total(100);
								_7z_setcomplete(100);
								MainWindow.redrawfield();
						}
						else
						{
                wsprintf(cmd,L"app -e \"%s\\%s\" -o\"%s\"",hwidmatch->getdrp_packpath(),hwidmatch->getdrp_packname(),
                        extractdir);

								itembar1=itembar;
								for(j=i;j<manager_g->items_list.size();j++,itembar1++)
										if(itembar1->checked&&
											 !wcscmp(hwidmatch->getdrp_packpath(),itembar1->hwidmatch->getdrp_packpath())&&
											 !wcscmp(hwidmatch->getdrp_packname(),itembar1->hwidmatch->getdrp_packname()))
								{
										wsprintf(buf,L" \"%S\"",itembar1->hwidmatch->getdrp_infpath());
                    if(!wcsstr(cmd,buf))wcscat(cmd,buf);
								}
								uprintf("Extracting '%S'\n",cmd);
								itembar->install_status=(instflag&INSTALLDRIVERS)?STR_INST_EXTRACT:STR_EXTR_EXTRACTING;
								MainWindow.redrawfield();

								// attempt extraction
								int tries=0;
                int r=0;

								// verify the file is available
                wchar_t full_arch_name[MAX_PATH];
                wsprintf(full_arch_name,L"%s\\%s",hwidmatch->getdrp_packpath(),hwidmatch->getdrp_packname());
                bool FileOk=System.FileAvailable(full_arch_name,20,5);
								if(!FileOk)
								{
										uprintf("Error: %S not found. Download failed or network or storage are not accessible.\n", full_arch_name);
										itembar->checked=0;
										itembar->install_status=STR_INST_FAILED;
										failed++;
								}
								else
								{
										do
								{
										if(!itembar->checked||installmode!=MODE_INSTALLING||tries>60)break;
										if(CRITICAL_SECTION_ACTIVE) LeaveCriticalSection(&sync);
                    r=Extract7z(cmd);
										if(CRITICAL_SECTION_ACTIVE)EnterCriticalSection(&sync);
										itembar=&manager_g->items_list[itembar_act];
										if(!FileOk)
										{
												uprintf("Error, 7Zip unknown fatal error.\n");
												uprintf("Error, checking for driverpack availability...");
												// if the 'Drivers' path exists
												if(System.FileExists(hwidmatch->getdrp_packpath()))break;
												uprintf("Waiting for DriverPacks to become available.");
												do
												{
														uprintf(".");
														Sleep(1000);
														tries++;
														if(!itembar->checked||installmode!=MODE_INSTALLING||tries>60)break;
												}while(!System.FileExists(hwidmatch->getdrp_packpath())&&!hwidmatch->getdrp_packontorrent());
												uprintf("OK\n");
										}
										}while(FileOk&&!hwidmatch->getdrp_packontorrent());
								}

								if(installmode==MODE_STOPPING)
								{
										manager_g->items_list[SLOT_EXTRACTING].install_status=STR_INST_STOPPING;
										itembar->install_status=STR_INST_STOPPING;
								}
								//if(!itembar->checked)manager_g->items_list[SLOT_EXTRACTING].install_status=STR_INST_STOPPING;
								//itembar->percent=manager_g->items_list[SLOT_EMPTY].percent;
								hwidmatch=itembar->hwidmatch;
								totalextracttime+=extracttime=System.GetTickCountWr()-extracttime;
								uprintf("Ret %d, %ld secs\n",FileOk,extracttime/1000);
								if(FileOk&&itembar->install_status!=STR_INST_STOPPING)
								{
										itembar->install_status=STR_EXTR_FAILED;
										itembar->val1=FileOk;
										itembar->checked=0;
										uprintf("ERROR: extraction failed\n");
								}
						}

						if(instflag&OPENFOLDER&&itembar->checked)
								itembar->install_status=STR_EXTR_OK;

						// Install driver
						if(instflag&INSTALLDRIVERS&&itembar->checked)
						{
								int needrb=0,ret=1;
								wsprintf(inf,L"%s\\%S%S",
											 unpacked?hwidmatch->getdrp_packpath():extractdir,
											 hwidmatch->getdrp_infpath(),
											 hwidmatch->getdrp_infname());
								wsprintf(hwid,L"%S",hwidmatch->getdrp_drvHWID());
								uprintf("Install32 '%S','%S'\n",hwid,inf);
								itembar->install_status=STR_INST_INSTALL;
								MainWindow.redrawfield();

								installtime=System.GetTickCountWr();
								if(CRITICAL_SECTION_ACTIVE)LeaveCriticalSection(&sync);
								if(installmode==MODE_INSTALLING)
										driver_install(hwid,inf,&ret,&needrb);
								else
										ret=1;
								if(CRITICAL_SECTION_ACTIVE)EnterCriticalSection(&sync);
								totalinstalltime+=installtime=System.GetTickCountWr()-installtime;
								itembar=&manager_g->items_list[itembar_act];

								if(ret==1)installed++;else failed++;
                                uprintf("Ret %d(0x%X),", ret, ret);
                                uprintf("%s,%ld secs\n\n", needrb ? "rb" : "norb", installtime / 1000);
								if(installmode==MODE_STOPPING)
								{
										itembar->install_status=STR_INST_STOPPING;
										manager_g->items_list[SLOT_EXTRACTING].install_status=STR_INST_STOPPING;
										manager_g->selectnone();
								}
								else
								{
										if(ret==1)
												itembar->install_status=needrb?STR_INST_REBOOT:STR_INST_OK;
										else
										{
												manager_g->expand(i,EXPAND_MODE::EXPAND);
												itembar->install_status=STR_INST_FAILED;
												itembar->val1=ret;
												uprintf("ERROR: installation failed\n");
										}

										if(needrb)needreboot=1;
								}
						}
						if(!unpacked&&(Settings.flags&FLAG_DELEXTRAINFS))removeextrainfs(inf);
						if(instflag&INSTALLDRIVERS)itembar->percent=0;
						itembar->checked=0;
						MainWindow.redrawmainwnd();
				}
				if(installmode==MODE_INSTALLING)
				{
						itembar=&manager_g->items_list[RES_SLOTS];
						for(j=RES_SLOTS;j<manager_g->items_list.size();j++,itembar++)
								if(itembar->checked)
										goto goaround;
				}

		} // if restorePointSucceeded

		// Installation completed by this point
		wsprintf(buf,L"%ws\\SetupAPI.dev.log",manager_g->matcher->getState()->textas.getw(manager_g->matcher->getState()->getWindir()));
		wsprintf(cmd,L"%s\\%ssetupAPI.log",Settings.log_dir);
		if(!(Settings.flags&FLAG_NOLOGFILE)) CopyFile(buf,cmd,0);

		if(instflag&OPENFOLDER)
		{
				wchar_t *p=extractdir+wcslen(extractdir);
				while(*(--p)!='\\');
				*p=0;
				uprintf("%S\n",extractdir);
				ShellExecute(nullptr,L"explore",extractdir,nullptr,nullptr,SW_SHOW);
				manager_g->items_list[SLOT_EXTRACTING].isactive=0;
				manager_g->clear();
				manager_g->setpos();
		}
		if(instflag&INSTALLDRIVERS&&(Settings.flags&FLAG_KEEPTEMPFILES)==0)
		{
				wsprintf(buf,L" /c rd /s /q \"%s\"",extractdir);
        System.run_command(L"cmd",buf,SW_HIDE,1);
		}

		manager_g->items_list[SLOT_EXTRACTING].percent=0;
		if(installmode==MODE_STOPPING)
		{
				Settings.flags&=~FLAG_AUTOINSTALL;
				installmode=MODE_NONE;
		}
		if(installmode==MODE_INSTALLING)
		{
				manager_g->items_list[SLOT_EXTRACTING].install_status=
						needreboot?STR_INST_COMPLITED_RB:STR_INST_COMPLITED;
				installmode=MODE_SCANNING;

				MainWindow.ShowProgressInTaskbar(false);
				FLASHWINFO fi;
				fi.cbSize=sizeof(FLASHWINFO);
				fi.hwnd=MainWindow.hMain;
				fi.dwFlags=FLASHW_ALL|FLASHW_TIMERNOFG;
				fi.uCount=1;
				fi.dwTimeout=0;
				FlashWindowEx(&fi);
		}
		itembar_act=0;
		uprintf("Extraction: %ld secs\n",totalextracttime/1000);
		uprintf("Installation: %ld secs\n",totalinstalltime/1000);
		ret_global=installed+(failed<<16);
		if(needreboot)ret_global|=0x40<<24;
		if(CRITICAL_SECTION_ACTIVE)LeaveCriticalSection(&sync);
		MainWindow.ShowProgressInTaskbar(false);
		invalidate(INVALIDATE_DEVICES);
		MainWindow.redrawmainwnd();

		installupdate_exitflag=1;
		installupdate_event->raise();

		return 0;
}
//}

//{ Autoclicker
const wnddata_t Autoclicker_t::clicktbl[NUM_CLICKDATA]=
{
		// Windows XP (normal)
		{
				396,-1,
				390,283,
#ifdef AUTOCLICKER_CONFIRM
				107,249,// continue
#else
				245,249,  // stop
#endif
				132,23
		},
		// Windows XP (rare)
		{
				396,-1,
				390,283,
#ifdef AUTOCLICKER_CONFIRM
				164,249,// continue
#else
				275,249,  // stop
#endif
				105,23
		},
		// Windows 7 and Windows 8.1 (normal)
		{
				500,270,
				500,270,
#ifdef AUTOCLICKER_CONFIRM
				47,139,  // continue
				448,87   // continue
#else
				47,67,     // stop
				448,72     // stop
#endif
		},
		// Windows 7 and Windows 8.1 (rare)
		{
				500,280,
				500,280,
#ifdef AUTOCLICKER_CONFIRM
				47,149,  // continue
				448,87   // continue
#else
				47,77,     // stop
				448,72     // stop
#endif
		},
		// Windows 7 and Windows 8.1 (rare)
		{
				500,230,
				500,230,
#ifdef AUTOCLICKER_CONFIRM
				47,120,  // continue
				448,66   // continue
#else
				47,67,     // stop
				448,53     // stop
#endif
		},
		// Windows 7 and Windows 8.1 (rare)
		{
				500,244,
				500,244,
#ifdef AUTOCLICKER_CONFIRM
				47,126,  // continue
				448,74   // continue
#else
				47,67,     // stop
				448,59     // stop
#endif
		},
		{
				-1,212,
				-1,212,
#ifdef AUTOCLICKER_CONFIRM
				-1,118,   // continue
				94,23     // continue
#else
				-1,118,   // stop
				129,23    // stop
#endif
		}
};

void Autoclicker_t::calcwnddata(wnddata_t *w,HWND hwnd)
{
		WINDOWINFO pwi,pwb;
		HWND parent=GetParent(hwnd);
		pwb.cbSize=pwi.cbSize=sizeof(WINDOWINFO);

		GetWindowInfo(parent,&pwi);
		w->wnd_wx=pwi.rcWindow.right-pwi.rcWindow.left;
		w->wnd_wy=pwi.rcWindow.bottom-pwi.rcWindow.top;
		w->cln_wx=pwi.rcClient.right-pwi.rcClient.left;
		w->cln_wy=pwi.rcClient.bottom-pwi.rcClient.top;

		GetWindowInfo(hwnd,&pwb);
		w->btn_x =pwb.rcClient.left-pwi.rcClient.left;
		w->btn_y =pwb.rcClient.top-pwi.rcClient.top;
		w->btn_wx=pwb.rcClient.right-pwb.rcClient.left;
		w->btn_wy=pwb.rcClient.bottom-pwb.rcClient.top;
}

int Autoclicker_t::cmpclickdata(int *a,int *b)
{
		int i;

		for(i=0;i<8;i++,a++,b++)
		if(*a!=*b&&*b!=-1)return 0;

		return 1;
}

BOOL CALLBACK Autoclicker_t::EnumWindowsProc(HWND hwnd,LPARAM lParam)
{
	WCHAR buf[MAX_CLASS_NAME] = { L'\0' };
	WINDOWINFO pwi;
		wnddata_t w;
		int i;

		pwi.cbSize=sizeof(WINDOWINFO);
		GetWindowInfo(hwnd,&pwi);

		if(lParam&2)
		{
				GetWindowText(hwnd,buf, _countof(buf));
				uprintf("Window %06X,%06X '%S'\n",hwnd,GetParent(hwnd),buf);
				GetClassName(hwnd,buf, _countof(buf));
				uprintf("Class: '%S'\n",buf);
				RealGetWindowClass(hwnd,buf, _countof(buf));
				uprintf("RealClass: '%S'\n",buf);
				uprintfs("\n");
		}

		if((lParam&1)==1)
		{
				Autoclicker.calcwnddata(&w,hwnd);
				if(lParam&2)
				{
						uprintf("* MainWindow (%d,%d) (%d,%d)\n",w.wnd_wx,w.wnd_wy,w.cln_wx,w.cln_wy);
						uprintf("* Child (%d,%d,%d,%d)\n",w.btn_x,w.btn_y,w.btn_wx,w.btn_wy);
						uprintfs("\n");
				}

				if((lParam&2)==0)for(i=0;i<NUM_CLICKDATA;i++)
						if(Autoclicker.cmpclickdata((int *)&w,(int *)&clicktbl[i]))
				{
						SwitchToThisWindow(hwnd,0);
						GetWindowInfo(GetParent(hwnd),&pwi);
						SetCursorPos(pwi.rcClient.left+w.btn_x+w.btn_wx/2,pwi.rcClient.top+w.btn_y+w.btn_wy/2);
						Sleep(500);
						if(IsWindow(hwnd))
						{
								GetWindowInfo(hwnd,&pwi);
								Autoclicker.calcwnddata(&w,hwnd);

								if(Autoclicker.cmpclickdata((int *)&w,(int *)&clicktbl[i]))
								{
										GetWindowInfo(GetParent(hwnd),&pwi);
										SetCursorPos(pwi.rcClient.left+w.btn_x+w.btn_wx/2,pwi.rcClient.top+w.btn_y+w.btn_wy/2);
										int x=w.btn_x+w.btn_wx/2;
										int y=w.btn_y+w.btn_wy/2;
										int pos=(y<<16)|x;
										SendMessage(GetParent(hwnd),WM_LBUTTONDOWN,0,pos);
										SendMessage(GetParent(hwnd),WM_LBUTTONUP,  0,pos);
										SetActiveWindow(hwnd);
										SendMessage(hwnd,BM_CLICK,0,0);
										uprintf("Autoclicker fired\n");
								}
						}
				}
		}

		if((lParam&1)==0)
				EnumChildWindows(hwnd,EnumWindowsProc,lParam|1);

		return 1;
}

void Autoclicker_t::wndclicker(int mode)
{
		EnumChildWindows(GetDesktopWindow(),EnumWindowsProc,mode);
}

void save_wndinfo()
{
		Autoclicker.wndclicker(2);
}

unsigned int __stdcall Autoclicker_t::thread_clicker(void *arg)
{
		UNREFERENCED_PARAMETER(arg);

		while(clicker_flag)
		{
				EnumChildWindows(GetDesktopWindow(),EnumWindowsProc,0);
				Sleep(100);
		}
		return 0;
}
//}
