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

#include <algorithm>
#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <cfgmgr32.h>
#include <dwmapi.h>

//#include "VersionEx.h"		//moved to SDI.h for registry.h
#include "Version.h"
#include "SDI.h"
#include "logging.h"
#include "system.h"
#include "msapi_utf8.h"
#include "resource.h"
#include "Settings.h"
#include "darkmode.h"

#include "7zip.h"
#include "cli.h"
#include "indexing.h"
#include "Lzma86.h"
#include "manager.h"
#include "install.h"    // non-portable
#include "gui.h"
#include "draw.h"   // non-portable
#include "theme.h"
#include "update.h"
#include "enum.h"   // non-portable
#include "usbwizard.h"

#include "model.h"
#include "script.h"

#include "wizards.h"

static BOOL log_displayed = FALSE;
static HWND hStart = NULL;

/*
 * Globals
 */
OPENED_LIBRARIES_VARS;
HINSTANCE hMainInstance;
HWND hMainDialog, hMultiToolbar, hSaveToolbar, hHashToolbar, hAdvancedDeviceToolbar, hAdvancedFormatToolbar, hUpdatesDlg = NULL;
uint16_t SDI_version[3];
HWND hLog = NULL, hLogDialog = NULL, hProgress = NULL;
BOOL debug = FALSE;
BOOL right_to_left_mode = FALSE;
int dialog_showing = 0;
char user_dir[MAX_PATH], cur_dir[MAX_PATH];
Manager manager_v[2];
Manager *manager_g=&manager_v[0];
USBWizard *USBWiz;

volatile int installupdate_exitflag=0;
Event *installupdate_event;

volatile int deviceupdate_exitflag=0;
Event *deviceupdate_event;
CRITICAL_SECTION sync;
bool CRITICAL_SECTION_ACTIVE=false;
int manager_active=0;
int bundle_display=1;
int bundle_shadow=0;
bool emptydrp;
WinVersions winVersions;
HMENU pSysMenu,ToolsMenu,UpdatesMenu;
int pSysMenuCount=0;
TORRENT_SELECTION_MODE TorrentSelectionMode=TSM_NONE;

// drag/drop in elevated processess
// https://helgeklein.com/blog/2010/03/how-to-enable-drag-and-drop-for-an-elevated-mfc-application-on-vistawindows-7/
typedef BOOL (WINAPI *PFN_CHANGEWINDOWMESSAGEFILTER)(UINT,DWORD);
HMODULE hModuleUser32=GetModuleHandle(TEXT("user32.dll"));
PFN_CHANGEWINDOWMESSAGEFILTER pfnChangeWindowMessageFilter=(PFN_CHANGEWINDOWMESSAGEFILTER)GetProcAddress(hModuleUser32,"ChangeWindowMessageFilter");

//}

//{ Objects
Popup_t *Popup=nullptr;
MainWindow_t MainWindow;
Settings_t Settings;
//}

// Callback for the log window
BOOL CALLBACK LogCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HFONT hf = NULL;
	HDC hDC;
	LONG lfHeight;
	LONG_PTR style;
	DWORD log_size;
	char *log_buffer = NULL, *filepath;
	EXT_DECL(log_ext, "SDI.log", __VA_GROUP__("*.log"), __VA_GROUP__("SDI log"));
	switch (message) {
	case WM_INITDIALOG:
		hLog = GetDlgItem(hDlg, IDC_LOG_EDIT);

		// Increase the size of our log textbox to MAX_LOG_SIZE (unsigned word)
		PostMessage(hLog, EM_LIMITTEXT, MAX_LOG_SIZE , 0);
		if (hf == NULL) {
			// Set the font to Unicode so that we can display anything
			hDC = GetDC(NULL);
			lfHeight = -MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72);
			safe_release_dc(NULL, hDC);
			hf = CreateFontA(lfHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, 0, "Consolas");
		}
		SendDlgItemMessageA(hDlg, IDC_LOG_EDIT, WM_SETFONT, (WPARAM)hf, TRUE);
		// Set 'Close Log' as the selected button
		SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDCANCEL), TRUE);

		// Suppress any inherited RTL flags from our edit control's style. Otherwise
		// the displayed text becomes a mess due to Windows trying to interpret
		// dots, parenthesis, columns and so on in an RTL context...
		// We also take this opportunity to fix the scroll bar and text alignment.
		style = GetWindowLongPtr(hLog, GWL_EXSTYLE);
		style &= ~(WS_EX_RTLREADING | WS_EX_RIGHT | WS_EX_LEFTSCROLLBAR);
		SetWindowLongPtr(hLog, GWL_EXSTYLE, style);
		style = GetWindowLongPtr(hLog, GWL_STYLE);
		style &= ~(ES_RIGHT);
		SetWindowLongPtr(hLog, GWL_STYLE, style);
		break;
	case WM_NCDESTROY:
		safe_delete_object(hf);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			ShowWindow(hDlg, SW_HIDE);
			log_displayed = FALSE;
			// Set focus to the Cancel button on the main dialog
			// This avoids intempestive tooltip display from the log toolbar button
			SendMessage(hMainDialog, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hMainDialog, IDCANCEL), TRUE);
			return TRUE;
		case IDC_LOG_CLEAR:
			SetWindowTextA(hLog, "");
			return TRUE;
		case IDC_LOG_SAVE:
			log_size = GetWindowTextLengthU(hLog);
			if (log_size <= 0)
				break;
			log_buffer = (char*)malloc(log_size);
			if (log_buffer != NULL) {
				log_size = GetDlgItemTextU(hDlg, IDC_LOG_EDIT, log_buffer, log_size);
				if (log_size != 0) {
					log_size--;	// remove NUL terminator
					filepath =  FileDialog(TRUE, user_dir, &log_ext, NULL);
					if (filepath != NULL)
						FileIO(FILE_IO_WRITE, filepath, &log_buffer, &log_size);
					safe_free(filepath);
				}
				safe_free(log_buffer);
			}
			break;
		}
		break;
	case WM_CLOSE:
		ShowWindow(hDlg, SW_HIDE);
		log_displayed = FALSE;
		// Set focus to the Cancel button on the main dialog
		// This avoids intempestive tooltip display from the log toolbar button
		SendMessage(hMainDialog, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hMainDialog, IDCANCEL), TRUE);
		return TRUE;
	case UM_RESIZE_BUTTONS:
		// Resize our buttons for low scaling factors
		ResizeButtonHeight(hDlg, IDCANCEL);
		ResizeButtonHeight(hDlg, IDC_LOG_SAVE);
		ResizeButtonHeight(hDlg, IDC_LOG_CLEAR);
		return TRUE;
	}
	return FALSE;
}

/*
 * Application Entrypoint
 */
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
		BOOL attached_console = FALSE;

		// Save instance of the application for further reference
		hMainInstance = hInstance;

		//Timers.start(time_total);

		// Determine number of CPU cores ("Logical Processors")
		SYSTEM_INFO siSysInfo;
		GetSystemInfo(&siSysInfo);
		num_cores=siSysInfo.dwNumberOfProcessors;

    // scripting
    if(Script::cmdArgIsPresent())
    {
        Script script;
        if(script.loadscript())
        {
            deviceupdate_event=CreateEventWr();
            script.runscript();
            delete deviceupdate_event;
        }
        return 0;
    }

    // Check if the mouse present
    if(!GetSystemMetrics(SM_MOUSEPRESENT))MainWindow.kbpanel=KB_FIELD;

    // Load settings
    init_CLIParam();
    if(!Settings.load_cfg_switch(GetCommandLineW()))
        Settings.load(L"SDI2.cfg");

		if (!Settings.install(lpCmdLine))
				Settings.parse(GetCommandLineW(),1);

    RUN_CLI();

    // Close the app if the work is done
    if(Settings.statemode==STATEMODE_EXIT)
    {
        return ret_global;
    }

    // force create the directories
    mkdir_r(Settings.drp_dir);
    mkdir_r(Settings.index_dir);
    mkdir_r(Settings.output_dir);

    // Load text
    vLang=CreateVaultLang(language,STR_NM,IDR_LANG);
    vTheme=CreateVaultTheme(theme,THEME_NM,IDR_THEME);

		// Allocate resources
    Bundle bundle[2];
    manager_v[0].init(bundle[bundle_display].getMatcher());
    manager_v[1].init(bundle[bundle_display].getMatcher());
    deviceupdate_event=CreateEventWr();

    // Start device/driver scan
    bundle[bundle_display].bundle_prep();
    invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_INDICES|INVALIDATE_MANAGER);
    ThreadAbs *thr=CreateThread();
    thr->start(&Bundle::thread_loadall,&bundle[0]);

    // Check updates
    #ifdef USE_TORRENT
    Updater=CreateUpdater();
    TorrentSelectionMode=TSM_AUTO;
    #endif

    // Start folder monitors
    Filemon *mon_drp=CreateFilemon(Settings.drp_dir,1,drp_callback);

    // MAIN GUI LOOP
    MainWindow.MainLoop(nShowCmd);

	// Wait till the device scan thread is finished
    if(MainWindow.hMain)deviceupdate_exitflag=1;
    deviceupdate_event->raise();
    thr->join();
    delete thr;
    delete deviceupdate_event;

    // Stop libtorrent
    #ifdef USE_TORRENT
    delete Updater;
    #endif

    // Save settings
    Settings.save();

    // Free allocated resources
    delete vLang;
    delete vTheme;

    // Stop folder monitors
    delete mon_drp;

    // Bring the console window back
    ShowWindow(GetConsoleWindow(),SW_SHOWNOACTIVATE);

		if (attached_console) {
				SetWindowPos(GetConsoleWindow(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
				FreeConsole();
		}
		CoUninitialize();
		//safe_closehandle(mutex);	# TODO
		uprintf("*** " APPLICATION_NAME " exit ***\n");
#ifdef _CRTDBG_MAP_ALLOC
		_CrtDumpMemoryLeaks();
#endif
    // Exit
    return ret_global;
}

void MainWindow_t::AddMenuItem(HMENU parent,UINT mask,UINT id,UINT type,UINT state,HMENU hSubMenu,wchar_t* typedata)
{
    MENUITEMINFO mi;
    mi.cbSize=sizeof(MENUITEMINFO);
    mi.fMask=mask;
    mi.wID=id;
    mi.fType=type;
    mi.fState=state;
    mi.dwTypeData=typedata;
    mi.hSubMenu=hSubMenu;

    if (parent != NULL)
        InsertMenuItem(parent, 0, TRUE, &mi);
}

void MainWindow_t::ModifyMenuItem(HMENU parent, UINT mask, UINT id, UINT state, wchar_t* typedata)
{
    if (parent==NULL)return;

    MENUITEMINFO mi;
    memset(&mi, 0, sizeof(MENUITEMINFO));
    mi.cbSize=sizeof(MENUITEMINFO);
    mi.fMask=mask;

    if(GetMenuItemInfo(parent, id, false, &mi))
    {
        mi.fType=0;
        mi.fState=state;
        mi.dwTypeData=typedata;
        SetMenuItemInfo(parent, id, false, &mi);
    }
}

void MainWindow_t::LoadMenuItems()
{
    if(!pSysMenu)
    {
        // get the initial number of items before I mess with it
        pSysMenu=GetSystemMenu(hMain,FALSE);
        pSysMenuCount=GetMenuItemCount(pSysMenu);
    }

    // remove all my menu entries
    int menucount=GetMenuItemCount(pSysMenu);
    while(menucount>pSysMenuCount)
    {
        DeleteMenu(pSysMenu,0,MF_BYPOSITION);
        menucount=GetMenuItemCount(pSysMenu);
    }

    // the tools menu - reverse order
    ToolsMenu=CreatePopupMenu();
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_DEVICEPRNT,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_DEVICEPRNT));
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_SYSCONTROL,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_SYSCONTROL));
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_SYSREST,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_SYSREST));
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_SYSPROT,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_SYSPROT));
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_SYSPROPS_ADV,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYSPROPS_ADV));
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_SYSPROPS,0,0,nullptr,const_cast<wchar_t *>STR(STR_REST_SYSPROPS));
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_DEVICEMNG,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYS_DEVICEMNG));
    AddMenuItem(ToolsMenu,MIIM_STRING|MIIM_ID,ID_COMPMNG,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_COMPMNG));

    // the updates sub-menu - reverse order
    UpdatesMenu=CreatePopupMenu();
    AddMenuItem(UpdatesMenu,MIIM_STRING|MIIM_ID,IDM_UPDATES_DRIVERS,0,0,nullptr,const_cast<wchar_t *>STR(STR_UPDATES_DRIVERS));
    AddMenuItem(UpdatesMenu,MIIM_STRING|MIIM_ID,IDM_UPDATES_SDI,0,0,nullptr,const_cast<wchar_t *>STR(STR_UPDATES_SDI));
    AddMenuItem(UpdatesMenu,MIIM_FTYPE,0,MFT_SEPARATOR,0,nullptr,const_cast<wchar_t *>(L""));
    AddMenuItem(UpdatesMenu,MIIM_STRING|MIIM_ID|MIIM_STATE,IDM_SEED,0,MFS_DISABLED,nullptr,const_cast<wchar_t *>STR(STR_SYST_START_SEED));

    // add options to the system menu - reverse order
    AddMenuItem(pSysMenu,MIIM_FTYPE,0,MFT_SEPARATOR,0,nullptr,const_cast<wchar_t *>(L""));
    #ifndef NDEBUG
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_WELCOME,0,0,nullptr,const_cast<wchar_t *>(L"Welcome"));
    #endif // NDEBUG
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_LICENSE,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_LICENSE));
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_ABOUT,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_ABOUT));
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_TRANSLATE,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_TRANSLATE));
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_USBWIZARD,0,0,nullptr,const_cast<wchar_t *>STR(STR_SYST_USBWIZARD));
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_DRVDIR,0,0,nullptr,const_cast<wchar_t *>STR(STR_DRVDIR));
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_OPENLOGS,0,0,nullptr,const_cast<wchar_t *>STR(STR_OPENLOGS));
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID|MIIM_SUBMENU,IDM_TOOLS,0,0,ToolsMenu,const_cast<wchar_t *>STR(STR_TOOLS));
    AddMenuItem(pSysMenu,MIIM_STRING|MIIM_ID|MIIM_SUBMENU,IDM_UPDATES,0,0,UpdatesMenu,const_cast<wchar_t *>STR(STR_UPDATES));
}

void MainWindow_t::MainLoop(int nShowCmd) {
    if((Settings.flags&FLAG_NOGUI)&&(Settings.flags&FLAG_AUTOINSTALL)==0)return;

    // Register classMain
    WNDCLASSEX wcex;
    memset(&wcex,0,sizeof(WNDCLASSEX));
    wcex.cbSize=         sizeof(WNDCLASSEX);
    wcex.lpfnWndProc=    WndProcMainCallback;
    wcex.hInstance=      hMainInstance;
    wcex.hIcon=          LoadIcon(hMainInstance,MAKEINTRESOURCE(IDR_MAINWND));
    wcex.hCursor=        LoadCursor(nullptr,IDC_ARROW);
    wcex.lpszClassName=  classMain;
		// For the extended translucent frame to be visible, we need black background.
		wcex.hbrBackground=  (HBRUSH)GetStockObject(BLACK_BRUSH);
    if(!RegisterClassEx(&wcex))
    {
				uprintf("ERROR in gui(): failed to register '%S' class\n",wcex.lpszClassName);
        return;
    }

    // Register classPopup
    wcex.lpfnWndProc=PopupProcedure;
    wcex.lpszClassName=classPopup;
    wcex.hIcon=nullptr;
    if(!RegisterClassEx(&wcex))
    {
				uprintf("ERROR in gui(): failed to register '%S' class\n",wcex.lpszClassName);
        System.UnregisterClass_log(classMain,L"gui",L"classMain");
        return;
    }

    // Register classField
    wcex.lpfnWndProc=WndProcFieldCallback;
    wcex.lpszClassName=classField;
    if(!RegisterClassEx(&wcex))
    {
		uprintf("ERROR in gui(): failed to register '%S' class\n",wcex.lpszClassName);
        System.UnregisterClass_log(classMain,L"gui",L"classMain");
        System.UnregisterClass_log(classPopup,L"gui",L"classPopup");
        return;
    }

    // Main windows

	hMain = CreateWindowEx(WS_EX_LAYERED,classMain, VERSION_FILEVERSION_LONG,
                        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                        CW_USEDEFAULT,CW_USEDEFAULT,D(MAINWND_WX),D(MAINWND_WY),
                        nullptr,nullptr,hMainInstance,nullptr);
    if(!hMain)
    {
				uprintf("ERROR in gui(): failed to create '%S' window\n",classMain);
        return;
    }

		// license dialog
		//if(!Settings.license)
		//		DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_LICENSE),nullptr, LicenseCallback);

    // Enable updates notifications
    if(Settings.license==2)
    {
        /*int f;
        f=lang_enum(hLang,L"langs",manager_g->matcher->state->locale);
        vvuprintf("lang %d\n",f);
        lang_set(f);*/

        //if(MessageBox(0,STR(STR_UPD_DIALOG_MSG),STR(STR_UPD_DIALOG_TITLE),MB_YESNO|MB_ICONQUESTION)==IDYES)
        {
            Settings.flags|=FLAG_CHECKUPDATES;
            #ifdef USE_TORRENT
            Updater->checkUpdates();
            #endif
            invalidate(INVALIDATE_MANAGER);
        }
    }

    if(Settings.license)
    {
        //time_test=System.GetTickCountWr()-time_total;log_times();
        ShowWindow(hMain,(Settings.flags&FLAG_NOGUI)?SW_HIDE:nShowCmd);
        int done=0;
        while(!done)
        {
            while(WAIT_IO_COMPLETION==MsgWaitForMultipleObjectsEx(0,nullptr,INFINITE,QS_ALLINPUT,MWMO_ALERTABLE));

            MSG msg;
            while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE))
            {
                if(msg.message==WM_QUIT)
                {
                    done=TRUE;
                    break;
                }else
                if(msg.message==WM_KEYDOWN)
                {
                    if(!(msg.lParam&(1<<30)))
                    {
                        if(msg.wParam==VK_CONTROL||msg.wParam==VK_SPACE)
                        {
                            POINT p;
                            GetCursorPos(&p);
                            SetCursorPos(p.x+1,p.y);
                            SetCursorPos(p.x,p.y);
                        }
                        if(msg.wParam==VK_CONTROL)ctrl_down=1;
                        if(msg.wParam==VK_SPACE)  space_down=1;
                        if(msg.wParam==VK_SHIFT||msg.wParam==VK_LSHIFT||msg.wParam==VK_RSHIFT)  space_down=shift_down=1;
                    }
                    if(msg.wParam==VK_SPACE&&kbpanel)
                    {
                        if(kbpanel==KB_FIELD)
                        {
                            SendMessage(hwndFrame,WM_LBUTTONDOWN,0,0);
                            SendMessage(hwndFrame,WM_LBUTTONUP,0,0);
                        }
                        else
                        {
                            SendMessage(hMain,WM_LBUTTONDOWN,0,0);
                            SendMessage(hMain,WM_LBUTTONUP,0,0);
                        }
                    }
                    if((msg.wParam==VK_LEFT||msg.wParam==VK_RIGHT)&&kbpanel==KB_INSTALL)
                    {
                        arrowsAdvance(msg.wParam==VK_LEFT?-1:1);
                    }
                    if((msg.wParam==VK_LEFT)&&kbpanel==KB_FIELD)
                    {
                        size_t index;
                        int nop;
                        manager_g->hitscan(0,0,&index,&nop);
                        manager_g->expand(index,EXPAND_MODE::COLLAPSE);
                    }
                    if((msg.wParam==VK_RIGHT)&&kbpanel==KB_FIELD)
                    {
                        size_t index;
                        int nop;
                        manager_g->hitscan(0,0,&index,&nop);
                        manager_g->expand(index,EXPAND_MODE::EXPAND);
                    }
                    if(msg.wParam==VK_UP)arrowsAdvance(-1);else
                    if(msg.wParam==VK_DOWN)arrowsAdvance(1);

                    if(msg.wParam==VK_TAB&&shift_down)
                    {
                        tabadvance(-1);
                    }
                    if(msg.wParam==VK_TAB&&!shift_down)
                    {
                        tabadvance(1);
                    }
                }else
                if(msg.message==WM_KEYUP)
                {
                    if(msg.wParam==VK_CONTROL||msg.wParam==VK_SPACE)
                    {
                        Popup->drawpopup(0,0,FLOATING_NONE,0,0,hwndFrame);
                    }
                    if(msg.wParam==VK_CONTROL)ctrl_down=0;
                    if(msg.wParam==VK_SPACE)  space_down=0;
                    if(msg.wParam==VK_SHIFT||msg.wParam==VK_LSHIFT||msg.wParam==VK_RSHIFT)  space_down=shift_down=0;
                }

                if(!(msg.message==WM_SYSKEYDOWN&&msg.wParam==VK_MENU))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }

    // UnregisterClass will fail if a window is still in use
    // result is application can't shut down
    if( (System.UnregisterClass_log(classMain,L"gui",L"classMain") or
         System.UnregisterClass_log(classPopup,L"gui",L"classPopup") or
         System.UnregisterClass_log(classField,L"gui",L"classField")) ) {
             // the ugly way to end the process
            _Exit(0);
         }
}
//}

//{ Subroutes
void drp_callback(const wchar_t *szFile,int action,int lParam)
{
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(lParam);

    if(StrStrIW(szFile,L".7z")&&Updater->isPaused())invalidate(INVALIDATE_INDICES);
}

const wchar_t MainWindow_t::classMain[]= L"classSDIMain";
const wchar_t MainWindow_t::classField[]=L"classSDIField";
const wchar_t MainWindow_t::classPopup[]=L"classSDIPopup";
MainWindow_t::MainWindow_t()
{
    hFont=wFont::Create();
    hLang=nullptr;
    hTheme=nullptr;

    mousex=-1;
    mousey=-1;
    mousedown=MOUSE_NONE;
    kbpanel=KB_NONE;
}

MainWindow_t::~MainWindow_t()
{
    delete hFont;
    delete hLang;
    delete hTheme;
}

struct TData
{
		HWND pages[4];
		HWND tab;
} data;

static BOOL CALLBACK DialogPage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
		UNREFERENCED_PARAMETER(hwnd);
		UNREFERENCED_PARAMETER(wp);
		UNREFERENCED_PARAMETER(lp);

		switch(msg)
		{
				case WM_COMMAND:
						switch(wp)
						{
								case IDD_P1_ZOOMR:
										SendMessage(GetDlgItem(data.pages[0],IDD_P1_ZOOMI),TBM_SETPOS,1,-256);
										break;
								case IDD_P1_DRV1:
								case IDD_P1_DRV2:
								case IDD_P1_DRV3:
										{
												Settings.flags&=~(FLAG_SHOWDRPNAMES1|FLAG_SHOWDRPNAMES2);
												if(SendMessage(GetDlgItem(data.pages[0],IDD_P1_DRV2),BM_GETCHECK,BST_CHECKED,0))Settings.flags|=FLAG_SHOWDRPNAMES1;
												if(SendMessage(GetDlgItem(data.pages[0],IDD_P1_DRV3),BM_GETCHECK,BST_CHECKED,0))Settings.flags|=FLAG_SHOWDRPNAMES2;
												manager_g->filter(Settings.filters);
												PostMessage(MainWindow.hMain,WM_UPDATETHEME,0,0);
										}
								default:
										break;
						}

				case WM_HSCROLL:
						{
								int n=-SendMessage(GetDlgItem(data.pages[0],IDD_P1_ZOOMI),TBM_GETPOS,0,0);
								if(n!=Settings.scale)
								{
										Settings.savedscale=Settings.scale=n;
										PostMessage(MainWindow.hMain,WM_UPDATETHEME,0,0);
								}
						}
						break;

				default:
						break;
		}

		return FALSE;
}

static void OnSelChange()
{
		int sel=TabCtrl_GetCurSel(data.tab);
		ShowWindow(data.pages[0],(sel==0)?SW_SHOW:SW_HIDE);
		ShowWindow(data.pages[1],(sel==1)?SW_SHOW:SW_HIDE);
		ShowWindow(data.pages[2],(sel==2)?SW_SHOW:SW_HIDE);
		ShowWindow(data.pages[3],(sel==3)?SW_SHOW:SW_HIDE);
}

static BOOL CALLBACK EnumChildProcMirror(HWND hWnd, LPARAM lParam)
{
		UNREFERENCED_PARAMETER(lParam);

		// Make sure window is valid
		if (hWnd && IsWindow(hWnd))
		{
				// exclude two specific controls
				int id=GetDlgCtrlID(hWnd);
				if(id==IDD_P1_ZOOMS||id==IDD_P1_ZOOMB)return TRUE;
				// Get the class name for the control
				wchar_t szClassName[MAXCHAR];
				GetClassName(hWnd, szClassName, MAXCHAR);
				// mirror the control
				if(wcscmp(szClassName,L"Edit")==0)
						setMirroringEdit(hWnd);
				else
						setMirroring(hWnd);
		}

		return TRUE;
}

/*
 * License callback
 */
INT_PTR CALLBACK LicenseCallback(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
		WINDOWPOS* wpos;
		static HBRUSH background_brush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
		HWND hEditBox;
		RECT rect;
		LPCSTR s;
		size_t sz;
		switch (Message) {
		case WM_INITDIALOG:
				get_resource(IDR_LICENSE, (void**)&s, &sz);
				hEditBox = GetDlgItem(hwnd, IDC_LICENSE_TEXT);
				SetWindowTextA(hEditBox, s);
				SendMessage(hEditBox, EM_SETREADONLY, 1, 0);
				// only show decline button on startup
				if (GetParent(hwnd))
				{
						ShowWindow(GetDlgItem(hwnd, IDCANCEL), SW_HIDE);
						SetFocus(GetDlgItem(hwnd, IDOK));
				}
				return TRUE;

		case WM_COMMAND:
				switch (LOWORD(wParam)) {
				case IDOK:
						Settings.license = 2;
						EndDialog(hwnd, IDOK);
						return TRUE;

				case IDCANCEL:
						if (!GetParent(hwnd))Settings.license = 0;
						EndDialog(hwnd, IDCANCEL);
						return TRUE;

				default:
						break;
				}
				break;
		case WM_WINDOWPOSCHANGED:
				wpos = (WINDOWPOS*)lParam;
				{
						int r = SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
						if (r && wpos->cy - rect.bottom > 0)
						{
								int sz1 = rect.bottom - 20 - wpos->cy;
								wpos->y = 10;
								wpos->cy = rect.bottom - 20;
								MoveWindow(hwnd, wpos->x, wpos->y, wpos->cx, wpos->cy, 1);

								GetRelativeCtrlRect(GetDlgItem(hwnd, IDC_LICENSE_TEXT), &rect);
								rect.bottom += sz1;
								MoveWindow(GetDlgItem(hwnd, IDC_LICENSE_TEXT), rect.left, rect.top, rect.right, rect.bottom, 1);

								GetRelativeCtrlRect(GetDlgItem(hwnd, IDOK), &rect);
								rect.top += sz1;
								MoveWindow(GetDlgItem(hwnd, IDOK), rect.left, rect.top, rect.right, rect.bottom, 1);

								GetRelativeCtrlRect(GetDlgItem(hwnd, IDCANCEL), &rect);
								rect.top += sz1;
								MoveWindow(GetDlgItem(hwnd, IDCANCEL), rect.left, rect.top, rect.right, rect.bottom, 1);
						}
				}
				return TRUE;
		case WM_CTLCOLORSTATIC:
				hEditBox = GetDlgItem(hwnd, IDC_LICENSE_TEXT);
				if ((HWND)lParam == hEditBox)
				{
						HDC hdcStatic = (HDC)wParam;
						SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
						SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
						return (LRESULT)GetStockObject(HOLLOW_BRUSH);
				}
				else
				{
						HDC hdcStatic = (HDC)wParam;
						SetBkMode(hdcStatic, TRANSPARENT);
						return (INT_PTR)background_brush;
				}
		case WM_CTLCOLORDLG:
				return (INT_PTR)background_brush;

		default:
				break;
		}
		return (INT_PTR)FALSE;
}

static BOOL CALLBACK SettingsDialog(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
		wchar_t num[32];

		switch (msg)
		{
				case WM_INITDIALOG:

						// save current window state
						RECT rect;
						GetWindowRect(MainWindow.hMain,&rect);
						Settings.wndwx=rect.right-rect.left;
						Settings.wndwy=rect.bottom-rect.top;

						data.pages[0]=CreateDialog(hMainInstance,MAKEINTRESOURCE(IDD_VIEWSETTINGS),hwnd,(DLGPROC)DialogPage);
						data.pages[1]=CreateDialog(hMainInstance,MAKEINTRESOURCE(IDD_UPDATESSETTINGS),hwnd,(DLGPROC)DialogPage);
						data.pages[2]=CreateDialog(hMainInstance,MAKEINTRESOURCE(IDD_PATHSETTINGS),hwnd,(DLGPROC)DialogPage);
						data.pages[3]=CreateDialog(hMainInstance,MAKEINTRESOURCE(IDD_ADVANCEDSETTINGS),hwnd,(DLGPROC)DialogPage);

						data.tab=GetDlgItem(hwnd,IDC_TAB1);
						if(data.tab)
						{
								TCITEM tci;
								tci.mask = TCIF_TEXT;
								tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_VIEW_TAB));
								if(TabCtrl_InsertItem(data.tab, 0, &tci)==-1) uprintf("ERROR in winMain(): failed to insert page in tab control.\n");
								tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_UPDATES_TAB));
								if(TabCtrl_InsertItem(data.tab, 1, &tci)==-1) uprintf("ERROR in winMain(): failed to insert page in tab control.\n");
								tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_PATH_TAB));
								if(TabCtrl_InsertItem(data.tab, 2, &tci)==-1) uprintf("ERROR in winMain(): failed to insert page in tab control.\n");
								tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_ADVANCED_TAB));
								if(TabCtrl_InsertItem(data.tab, 3, &tci)==-1) uprintf("ERROR in winMain(): failed to insert page in tab control.\n");

								RECT rc;
								GetWindowRect(data.tab,&rc);
								POINT offset;
								offset.x=0;
								offset.y=0;
								ScreenToClient(hwnd,&offset);
								OffsetRect(&rc,offset.x,offset.y);

								rc.top+=30;
								rc.left+=3;
								rc.right-=3;
								rc.bottom-=3;
								SetWindowPos(data.pages[0],nullptr,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_HIDEWINDOW);
								SetWindowPos(data.pages[1],nullptr,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_HIDEWINDOW);
								SetWindowPos(data.pages[2],nullptr,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_HIDEWINDOW);
								SetWindowPos(data.pages[3],nullptr,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_HIDEWINDOW);

								// Strings
								SetWindowText(hwnd,STR(STR_OPTION_TITLE));

								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_DRV),STR(STR_OPTION_DRPNAMES));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_DRV1),STR(STR_OPTION_HIDE_NAMES));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_DRV2),STR(STR_OPTION_SHOW_RIGHT));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_DRV3),STR(STR_OPTION_SHOW_ABOVE));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_ZOOMG),STR(STR_OPTION_SCALLING));

								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_ZOOML),STR(STR_OPTION_SCALLING_H));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_ZOOMS),STR(STR_OPTION_SCALLING_SML));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_ZOOMB),STR(STR_OPTION_SCALLING_BIG));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_ZOOMR),STR(STR_OPTION_SCALLING_RST));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_HINTG),STR(STR_OPTION_HINT));
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_HINTL),STR(STR_OPTION_HINT_LABEL));

								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_TOR),STR(STR_OPTION_TORRENT));
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_PORT),STR(STR_OPTION_PORT));
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_CON),STR(STR_OPTION_MAX_CON));
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_DOWN),STR(STR_OPTION_MAX_DOWNLOAD));
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_UP),STR(STR_OPTION_MAX_UPLOAD));
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_UPD),STR(STR_OPTION_CHECKUPDATES));
								SetWindowText(GetDlgItem(data.pages[1],IDONLYUPDATE),STR(STR_UPD_ONLYUPDATES));
								SetWindowText(GetDlgItem(data.pages[1],IDPREALLOCATE),STR(STR_UPD_PREALLOCATE));

								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR1),STR(STR_OPTION_DIR_DRIVERS));
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR2),STR(STR_OPTION_DIR_INDICES));
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR3),STR(STR_OPTION_DIR_INDICESH));
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR4),STR(STR_OPTION_DIR_DATA));
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR5),STR(STR_OPTION_DIR_LOGS));

								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMDG),STR(STR_OPTION_CMD));
								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMDL),STR(STR_OPTION_CMD_LABEL));
								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD1),STR(STR_OPTION_CMD_FINISH));
								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD2),STR(STR_OPTION_CMD_FINISHRB));
								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD3),STR(STR_OPTION_CMD_FINISHDN));
								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CONSL),STR(STR_OPTION_CONSOLE));

								// Set data
								WStringShort str;

								int r;
								switch(Settings.flags&(FLAG_SHOWDRPNAMES1|FLAG_SHOWDRPNAMES2))
								{
										case FLAG_SHOWDRPNAMES1:r=IDD_P1_DRV2;break;
										case FLAG_SHOWDRPNAMES2:r=IDD_P1_DRV3;break;
//                    case 0:r=2;break;
										default:r=IDD_P1_DRV1;break;
								}
								SendMessage(GetDlgItem(data.pages[0],r),BM_SETCHECK,BST_CHECKED,0);

								// windows doesn't normally show a focus rect until it first receives
								// keyboard input but i like to see where the keyboard focus is
								PostMessage(hwnd, WM_UPDATEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
								// set the keyboard focus to the selected radio button
								SetFocus(GetDlgItem(data.pages[0],r));

								SendMessage(GetDlgItem(data.pages[0],IDD_P1_ZOOMI),TBM_SETRANGE,1,MAKELONG(-350,-150));
                                //                SendMessage(GetDlgItem(data.pages[0],IDD_P1_ZOOMI),TBM_SETRANGE,1,MAKELONG(150,350));
								SendMessage(GetDlgItem(data.pages[0],IDD_P1_ZOOMI),TBM_SETPOS,1,-Settings.scale);
								str.sprintf(L"%d",Settings.hintdelay);
								SetWindowText(GetDlgItem(data.pages[0],IDD_P1_HINTE),str.Get());

								str.sprintf(L"%d",Updater->port);
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_PORTE),str.Get());
								str.sprintf(L"%d",Updater->connections);
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_CONE),str.Get());
								str.sprintf(L"%d",Updater->downlimit);
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_DOWNE),str.Get());
								str.sprintf(L"%d",Updater->uplimit);
								SetWindowText(GetDlgItem(data.pages[1],IDD_P2_UPE),str.Get());
								if(!(Settings.flags&FLAG_CHECKUPDATES))SendMessage(GetDlgItem(data.pages[1],IDD_P2_UPD),BM_SETCHECK,BST_CHECKED,0);
								if(Settings.flags&FLAG_ONLYUPDATES)SendMessage(GetDlgItem(data.pages[1],IDONLYUPDATE),BM_SETCHECK,BST_CHECKED,0);

								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR1E),Settings.drp_dir);
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR2E),Settings.index_dir);
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR3E),Settings.output_dir);
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR4E),Settings.data_dir);
								SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR5E),Settings.logO_dir);

								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD1E),Settings.finish);
								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD2E),Settings.finish_rb);
								SetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD3E),Settings.finish_upd);
								if(Settings.flags&FLAG_SHOWCONSOLE)SendMessage(GetDlgItem(data.pages[3],IDD_P4_CONSL),BM_SETCHECK,BST_CHECKED,0);

								SetWindowText(GetDlgItem(hwnd,IDOK),STR(STR_UPD_BTN_OK));
								SetWindowText(GetDlgItem(hwnd,IDCANCEL),STR(STR_UPD_BTN_CANCEL));

								OnSelChange();

								if (right_to_left_mode)
								{
										setMirroring(hwnd);
										// iterate all controls on the dialog
										EnumChildWindows(hwnd, EnumChildProcMirror, 0);
										// can't find a nice way to do these two so i'll just swap the text
										SetWindowText(GetDlgItem(data.pages[0],IDD_P1_ZOOMS),STR(STR_OPTION_SCALLING_BIG));
										SetWindowText(GetDlgItem(data.pages[0],IDD_P1_ZOOMB),STR(STR_OPTION_SCALLING_SML));
								}

						}
						break;

				case WM_NOTIFY:
						switch(((LPNMHDR)lp)->code)
						{
								case TCN_SELCHANGE:
										OnSelChange();
										break;

								default:
										break;
						}
						break;

				case WM_HSCROLL:
						uprintf("asd");
						break;

				case WM_COMMAND:
						switch(wp)
						{
								case IDOK:
										{
												int n=-SendMessage(GetDlgItem(data.pages[0],IDD_P1_ZOOMI),TBM_GETPOS,0,0);
												if(n!=Settings.scale)
												{
														Settings.savedscale=Settings.scale=n;
														PostMessage(MainWindow.hMain,WM_UPDATETHEME,0,0);
												}
				}
										Settings.flags&=~(FLAG_SHOWDRPNAMES1|FLAG_SHOWDRPNAMES2);
										if(SendMessage(GetDlgItem(data.pages[0],IDD_P1_DRV2),BM_GETCHECK,BST_CHECKED,0))Settings.flags|=FLAG_SHOWDRPNAMES1;
										if(SendMessage(GetDlgItem(data.pages[0],IDD_P1_DRV3),BM_GETCHECK,BST_CHECKED,0))Settings.flags|=FLAG_SHOWDRPNAMES2;
										manager_g->filter(Settings.filters);
										manager_g->setpos();
										MainWindow.redrawfield();

										GetWindowText(GetDlgItem(data.pages[0],IDD_P1_HINTE),num,32);
										Settings.hintdelay=_wtoi_my(num);

										GetWindowText(GetDlgItem(data.pages[1],IDD_P2_PORTE),num,32);
										Updater->port=_wtoi_my(num);
										GetWindowText(GetDlgItem(data.pages[1],IDD_P2_CONE),num,32);
										Updater->connections=_wtoi_my(num);
										GetWindowText(GetDlgItem(data.pages[1],IDD_P2_DOWNE),num,32);
										Updater->downlimit=_wtoi_my(num);
										GetWindowText(GetDlgItem(data.pages[1],IDD_P2_UPE),num,32);
										Updater->uplimit=_wtoi_my(num);
										Updater->set_torrent_params();

										if(!SendMessage(GetDlgItem(data.pages[1],IDD_P2_UPD),BM_GETCHECK,0,0))
												Settings.flags|=FLAG_CHECKUPDATES;
										else
												Settings.flags&=~FLAG_CHECKUPDATES;

										if(SendMessage(GetDlgItem(data.pages[1],IDONLYUPDATE),BM_GETCHECK,0,0))
												Settings.flags|=FLAG_ONLYUPDATES;
										else
												Settings.flags&=~FLAG_ONLYUPDATES;

										GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR1E),Settings.drp_dir,MAX_PATH);
										GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR2E),Settings.index_dir, MAX_PATH);
										GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR3E),Settings.output_dir, MAX_PATH);
										GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR4E),Settings.data_dir, MAX_PATH);
										GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR5E),Settings.logO_dir, MAX_PATH);

										GetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD1E),Settings.finish,RESTART_MAX_CMD_LINE);
										GetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD2E),Settings.finish_rb, RESTART_MAX_CMD_LINE);
										GetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD3E),Settings.finish_upd, RESTART_MAX_CMD_LINE);

										if(SendMessage(GetDlgItem(data.pages[3],IDD_P4_CONSL),BM_GETCHECK,0,0))
										{
												Settings.flags|=FLAG_SHOWCONSOLE;
												// Display the log Window
												log_displayed = !log_displayed;
												// Must come last for the log window to get focus
												ShowWindow(hLogDialog, log_displayed ? SW_SHOW : SW_HIDE);
										}
										else
										{
												Settings.flags&=~FLAG_SHOWCONSOLE;
												log_displayed = FALSE;
										}

										EndDialog(hwnd,wp);
										break;

								case IDCANCEL:
										EndDialog(hwnd,wp);
										break;

								default:
										break;
						}
				default:
						break;
		}

		return FALSE;
}



void MainWindow_t::snapshot()
{
    if(System.ChooseFile(Settings.state_file,STR(STR_OPENSNAPSHOT),L"snp"))
    {
        Settings.statemode=STATEMODE_EMUL;
        invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
    }
}

void MainWindow_t::extractto()
{
    wchar_t dir[MAX_PATH];
    std::wstring path=System.AppPathW();
    wcscpy(dir,path.c_str());

    if(System.ChooseDir(dir,STR(STR_EXTRACTFOLDER)))
    {
        int argc;
        wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        WStringShort buf;
        buf.sprintf(L"%s\\drv.exe",dir);
        if(!CopyFile(argv[0],buf.Get(),0))
            uprintf("ERROR in extractto(): failed CopyFile(%S,%S)\n",argv[0],buf.Get());
        LocalFree(argv);

        wcscat(dir,L"\\drivers");
        wcscpy(extractdir,dir);
        manager_g->install(OPENFOLDER);
    }
}

void MainWindow_t::selectDrpDir()
{
    if(System.ChooseDir(Settings.drpext_dir,STR(STR_DRVDIR)))
    {
        invalidate(INVALIDATE_INDICES|INVALIDATE_MANAGER);
    }
}

void invalidate(int v)
{
    invaidate_set|=v;
    deviceupdate_event->raise();
}
//}

//{ Scrollbar
void MainWindow_t::setscrollrange(int y)
{
    RECT rect;
    GetClientRect(hwndFrame,&rect);

    SCROLLINFO si;
    si.cbSize=sizeof(si);
    si.fMask =SIF_RANGE|SIF_PAGE;
    si.nMin  =0;
    si.nMax  =y;
    si.nPage =rect.bottom;
    scrollvisible=rect.bottom>y;
    SetScrollInfo(hwndFrame,SB_VERT,&si,TRUE);
}

int MainWindow_t::getscrollpos()
{
    if(!hwndFrame)
    {
        uprintfs("ERROR in getscrollpos(): hwndFrame is 0\n");
        return 0;
    }

    SCROLLINFO si;
    si.cbSize=sizeof(si);
    si.fMask=SIF_POS;
    si.nPos=0;
    GetScrollInfo(hwndFrame,SB_VERT,&si);
    return si.nPos;
}

void MainWindow_t::setscrollpos(int pos)
{
    if(!hwndFrame)
    {
        uprintfs("ERROR in setscrollpos(): hwndFrame is 0\n");
        return;
    }

    SCROLLINFO si;
    si.cbSize=sizeof(si);
    si.fMask=SIF_POS;
    si.nPos=pos;
    SetScrollInfo(hwndFrame,SB_VERT,&si,TRUE);
}
//}

//{ Misc

void escapeAmpUrl(wchar_t *buf,const wchar_t *source)
{
    while(*source)
    {
        *buf=*source;
        if(*buf==L'&')
        {
            *buf++=L'%';
            *buf++=L'2';
            *buf=L'6';
        }
        if(*buf==L'\\')
        {
            *buf++=L'%';
            *buf++=L'5';
            *buf=L'C';
        }
        buf++;source++;
    }
    *buf=0;
}

void escapeAmp(wchar_t *buf,const wchar_t *source)
{
    while(*source)
    {
        *buf=*source;
        if(*buf==L'&')*(++buf)=L'&';
        buf++;source++;
    }
    *buf=0;
}
//}

//{ GUI Helpers
HWND CreateWindowMF(const wchar_t *type,const wchar_t *name,HWND hwnd,intptr_t id,DWORD f)
{
		HINSTANCE h = GetModuleHandle(nullptr);
		return CreateWindow(type,name,WS_CHILD|WS_VISIBLE|f,0,0,0,0,hwnd,(HMENU)(id),h,NULL);
}

void setMirroringEdit(HWND hwnd)
{
		setMirroring(hwnd);

		// reposition edit controls for right-to-left
		if(right_to_left_mode)
		{
				RECT p,r;
				GetWindowRect(GetParent(hwnd),&p);
				GetWindowRect(hwnd,&r);
				MapWindowPoints(HWND_DESKTOP,GetParent(hwnd),(LPPOINT)&r, 2);
				int w=r.right-r.left;
				int h=r.bottom-r.top;
				r.left=p.right-p.left-r.left-w;
				MoveWindow(hwnd,r.left,r.top,w,h,TRUE);
		}
}

void MainWindow_t::redrawfield()
{
		if(Settings.flags&FLAG_NOGUI)return;
		if(!hwndFrame)
		{
				uprintf("ERROR in redrawfield(): hwndFrame is 0\n");
				return;
		}
		InvalidateRect(hwndFrame,nullptr,0);
}

void MainWindow_t::redrawmainwnd()
{
		if(Settings.flags&FLAG_NOGUI)return;
		if(!hMain)
		{
				uprintf("ERROR in redrawmainwnd(): hMain is 0\n");
				return;
		}
		InvalidateRect(hMain,nullptr,0);
}

void checktimer(const wchar_t* str, long long t, int uMsg)
{
		if (System.GetTickCountWr() - t > 20 && debug)
				uprintf("GUI lag in %S[%X]: %ld\n", str, uMsg, System.GetTickCountWr() - t);
}

void MainWindow_t::ShowProgressInTaskbar(bool show,long long complited,long long total)
{
		int hres;
		ITaskbarList3 *pTL;
		static const IID my_CLSID_TaskbarList={0x56fdf344,0xfd6d,0x11d0,{0x95,0x8a,0x00,0x60,0x97,0xc9,0xa0,0x90}};

		CoInitializeEx(nullptr,COINIT_MULTITHREADED);
		hres=CoCreateInstance(my_CLSID_TaskbarList,nullptr,CLSCTX_ALL,IID_ITaskbarList3,(LPVOID*)&pTL);
		if(FAILED(hres))
		{
				CoUninitialize();
				//printf("FAILED to create IID_ITaskbarList3 object. Error code = 0x%X\n",hres);
				return;
		}
		//printf("%d,%d\n",flags,complited);
		pTL->SetProgressValue(hMain,complited,total);
		pTL->SetProgressState(hMain,show?TBPF_NORMAL:TBPF_NOPROGRESS);
		pTL->Release();
		CoUninitialize();
}

void MainWindow_t::DownloadedTorrent(int TorrentResults)
{
    // a torrent has just been downloaded

    // update the menu items
    ModifyMenuItem(pSysMenu,MIIM_STATE,IDM_SEED,MFS_ENABLED,nullptr);
    UpdateTorrentItems(Updater->activetorrent);

	// get driver count, index count, command line count
    wchar_t spec1[MAX_PATH];
    wchar_t spec2[MAX_PATH];
    wcscpy(spec1,Settings.drp_dir);wcscat(spec1,L"\\*.*");
    wcscpy(spec2,Settings.index_dir);wcscat(spec2,L"\\*.*");
    int argc;
    CommandLineToArgvW(GetCommandLineW(),&argc);

    // torrent results
    int NewVersion=TorrentResults>>8;
    //int LatestExeVersion=System.FindLatestExeVersion();
    int DriverPacksAvailable=TorrentResults&0xFF;

    if(TorrentSelectionMode==TSM_AUTO)
	{
        // just finished downloading the first torrent after startup
        // if there are no drivers and no indices
        // and no command line then show the welcome screen
        if(!System.FileExists2(spec1)&&!System.FileExists2(spec2)&&(argc<2))
        {
            TorrentSelectionMode=TSM_NONE;
            DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_WELCOME), MainWindow.hMain,(DLGPROC)WelcomeCallback);
        }
        // otherwise if there are updates on the current torrent then stop switching
        //else if((NewVersion>LatestExeVersion)||(DriverPacksAvailable>0))
				else if(DriverPacksAvailable>0)
            TorrentSelectionMode=TSM_NONE;
        // no updates on this torrent so try the next one then stop
        else if(Updater->activetorrent==1)
        {
            TorrentSelectionMode=TSM_NONE;
            ResetUpdater(2);
        }
	}
 }

void MainWindow_t::ResetUpdater(int activetorrent)
{
    #ifdef USE_TORRENT
    // update the menu items
    ModifyMenuItem(pSysMenu,MIIM_STRING|MIIM_STATE,IDM_SEED,MFS_DISABLED,const_cast<wchar_t *>STR(STR_SYST_START_SEED));
    UpdateTorrentItems(0);
    Settings.flags|=FLAG_CHECKUPDATES;

    delete Updater;
    if(activetorrent>0)
        Updater_t::activetorrent=activetorrent;
    Updater=CreateUpdater();
    Updater->checkUpdates();
    UpdateTorrentItems(Updater_t::activetorrent);

    #endif // USE_TORRENT
}

void MainWindow_t::UpdateTorrentItems(int activetorrent)
{
    switch (activetorrent)
    {
        case 0:
            ModifyMenuItem(UpdatesMenu,MIIM_STATE|MIIM_ID,IDM_UPDATES_SDI,MFS_UNCHECKED,nullptr);
            ModifyMenuItem(UpdatesMenu,MIIM_STATE|MIIM_ID,IDM_UPDATES_DRIVERS,MFS_UNCHECKED,nullptr);
            break;
        case 1:
            ModifyMenuItem(UpdatesMenu,MIIM_STATE|MIIM_ID,IDM_UPDATES_SDI,MFS_CHECKED,nullptr);
            break;
        case 2:
            ModifyMenuItem(UpdatesMenu,MIIM_STATE|MIIM_ID,IDM_UPDATES_DRIVERS,MFS_CHECKED,nullptr);
            break;
        default:
            break;
    }
}

void MainWindow_t::tabadvance(int v)
{
    if(v>0)
        wPanels->NextPanel();
    else
        wPanels->PrevPanel();

    if(kbpanel==KB_LANG)
        hLang->Focus();
    else if(kbpanel==KB_THEME)
        hTheme->Focus();
    else
        SetFocus(hMain);

    HoverVisiter hv{0,0};
    wPanels->Accept(hv);

    redrawfield();
}

extern int setaa;
void MainWindow_t::arrowsAdvance(int v)
{
    if(!kbpanel)return;

    if(v>0)
        wPanels->NextItem();
    else
        wPanels->PrevItem();

    if(kbpanel==KB_INSTALL)
    {
        kbinstall+=v;
        if(kbinstall<0)kbinstall=2;
        if(kbinstall>2)kbinstall=0;

        HoverVisiter hv{0,0};
        wPanels->Accept(hv);
        redrawmainwnd();
        return;
    }
    if(kbpanel==KB_FIELD)
    {
        kbfield+=v;
        setaa=1;
        redrawfield();
        return;
    }

    HoverVisiter hv{0,0};
    wPanels->Accept(hv);
    redrawmainwnd();
}
//}


//{ Txt
size_t Txt::strcpy(const char *str)
{
		size_t r=text.size();
		text.insert(text.end(),str,str+strlen(str)+1);
		return r;
}

size_t Txt::strcpyw(const wchar_t *str)
{
		size_t r=text.size();
		text.insert(text.end(),reinterpret_cast<const char *>(str),reinterpret_cast<const char *>(str+wcslen(str)+1));
		return r;
}

size_t Txt::t_memcpy(const char *mem,size_t sz)
{
		size_t r=text.size();
		text.insert(text.end(),mem,mem+sz);
		return r;
}

size_t Txt::t_memcpyz(const char *mem,size_t sz)
{
		size_t r=text.size();
		text.insert(text.end(),mem,mem+sz);
		text.insert(text.end(),0);
		return r;
}

size_t Txt::memcpyz_dup(const char *mem,size_t sz)
{
		std::string str(mem,sz);
		auto it=dub.find(str);

		if(it==dub.end())
		{
				size_t r=text.size();
				text.insert(text.end(),mem,mem+sz);
				text.insert(text.end(),0);

				dub.insert({std::move(str),r});
				return r;
		}
		else
		{
				return it->second;
		}
}

size_t Txt::alloc(size_t sz)
{
		size_t r=text.size();
		text.resize(r+sz);
		return r;
}

Txt::Txt()
{
		reset(2);
		text[0]=text[1]=0;
}

void Txt::reset(size_t sz)
{
		text.resize(sz);
		text.reserve(1024*1024*2); //TODO
}

void Txt::shrink()
{
		//log("Text_usage %d/%d\n",text.size(),text.capacity());
		text.shrink_to_fit();
}
//}

//{ Hashtable
unsigned Hashtable::gethashcode(const char *s, size_t sz)
{
		int h=5381;

		while(sz--)
		{
				int ch=*s++;
				h=((h<<5)+h)^ch;
		}
		return h;
}

void Hashtable::reset(size_t size1)
{
		size=(int)size1;
		if(!size)size=1;
		items.resize(size);
		items.reserve(size*sizeof(int));
		memset(items.data(),0,size*sizeof(Hashitem));
}

char *Hashtable::savedata(char *p)
{
		memcpy(p,&size,sizeof(int));p+=sizeof(int);
		p=items.savedata(p);
		return p;
}

char *Hashtable::loaddata(char *p)
{
		memcpy(&size,p,sizeof(int));p+=sizeof(int);
		items.resize(size);
		return items.loaddata(p);
}

/*
next
			0: free
		 -1: used,next is free
	1..x : used,next is used
*/
void Hashtable::additem(int key,int value)
{
		int curi=gethashcode((char *)&key,sizeof(int))%size;
		Hashitem *cur=&items[curi];

		int previ=-1;
		if(cur->next!=0)
		do
		{
				cur=&items[curi];
				previ=curi;
		}
		while((curi=cur->next)>0);

		if(cur->next==-1)
		{
				items.emplace_back(Hashitem());
				cur=&items.back();
				curi=(int)(items.size()-1);
		}

		cur->key=key;
		cur->value=value;
		cur->next=-1;
		if(previ>=0)(&items[previ])->next=curi;
}

int Hashtable::find(int key,int *isfound)
{
		if(!size)
		{
				*isfound=0;
				return 0;
		}

		int curi=gethashcode((char *)&key,sizeof(int))%size;
		Hashitem *cur=&items[curi];

		if(cur->next<0)
		{
				if(key==cur->key)
				{
						findnext_v=cur->next;
						findstr=key;
						*isfound=1;
						return cur->value;
				}
		}

		if(cur->next==0)
		{
				*isfound=0;
				return 0;
		}

		do
		{
				cur=&items[curi];
				if(key==cur->key)
				{
						findnext_v=cur->next;
						findstr=key;
						*isfound=1;
						return cur->value;
				}
		} while((curi=cur->next)>0);

		*isfound=0;
		return 0;
}

int Hashtable::findnext(int *isfound)
{
		Hashitem *cur;
		int curi=findnext_v;

		*isfound=0;
		if(curi<=0)return 0;

		cur=&items[curi];
		do
		{
				cur=&items[curi];
				if(cur->key==findstr)
				{
						findnext_v=cur->next;
						*isfound=1;
						return cur->value;
				}
		} while((curi=cur->next)>0);
		return 0;
}
//}

LRESULT MainWindow_t::WndProcCommon(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);

    RECT rect;
    short x,y;

    x=LOWORD(lParam);
    y=HIWORD(lParam);
    switch(uMsg)
    {
        case WM_MOUSELEAVE:
            Popup->onLeave();
            break;

        case WM_MOUSEHOVER:
            Popup->onHover();
            break;

        case WM_ACTIVATE:
            InvalidateRect(hwnd,nullptr,0);
            break;

        case WM_MOUSEMOVE:
            if(mousedown==MOUSE_CLICK||mousedown==MOUSE_MOVE)
            {
                GetWindowRect(hMain,&rect);
                if(mousedown==MOUSE_MOVE||abs(mousex-x)>2||abs(mousey-y)>2)
                {
                    mousedown=MOUSE_MOVE;
                    MoveWindow(hMain,rect.left+(x-mousex)*(right_to_left_mode ?-1:1),rect.top+y-mousey,
                               rect.right-rect.left,rect.bottom-rect.top,1);
                }
            }
            return 1;

        case WM_LBUTTONDOWN:
            if(kbpanel&&x&&y)
            {
                kbpanel=KB_NONE;
                redrawmainwnd();
            }
            SetFocus(hMain);
            if(!IsZoomed(hMain))
            {
                mousex=x;
                mousey=y;
                mousedown=MOUSE_CLICK;
                SetCapture(hwnd);
            }
            break;

        case WM_CANCELMODE:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            mousex=-1;
            mousey=-1;
            SetCursor(LoadCursor(nullptr,IDC_ARROW));
            ReleaseCapture();
            mouseclick=uMsg==WM_LBUTTONUP&&mousedown!=MOUSE_MOVE?1:0;
            mousedown=MOUSE_NONE;
            return 1;

        default:
            return 1;
    }
    return 0;
}

LRESULT CALLBACK MainWindow_t::WndProcMainCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
		return MainWindow.MainCallback(hwnd,uMsg,wParam,lParam);
}

LRESULT MainWindow_t::MainCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
		static BOOL first_log_display = TRUE;
		WINDOWPLACEMENT wndp;
		wndp.length=sizeof(WINDOWPLACEMENT);
		POINT Point;
		RECT rect, rc, DialogRect, DesktopRect;
		short x,y;

		int i, nWidth, nHeight, offset;
	int f;
		int wp;
		//long long timer=System.GetTickCountWr();

		x=LOWORD(lParam);
		y=HIWORD(lParam);

		if(WndProcCommon(hwnd,uMsg,wParam,lParam))
		switch(uMsg) {

				case WM_CREATE:
						// Canvas
						canvasMain=Canvas::Create();
						hMain=hwnd;

						// Field
						hwndFrame=CreateWindowMF(classField,nullptr,hwnd,0,WS_VSCROLL);

						// Popup
						Popup=new Popup_t;
						Popup->init();

						// Lang
						hLang=Combobox::Create(&hwnd,ID_LANG);
						PostMessage(hwnd,WM_UPDATELANG,0,0);

						// Theme
						hTheme=Combobox::Create(&hwnd,ID_THEME);
						PostMessage(hwnd,WM_UPDATETHEME,1,0);

						// Misc
						vLang->StartMonitor();
						vTheme->StartMonitor();
						DragAcceptFiles(hwnd,true);

						// drag/drop in elevated processes
						if(pfnChangeWindowMessageFilter) // The function isn't available on Windows 2000 and XP
						{
								(*pfnChangeWindowMessageFilter)(WM_DROPFILES,1);
								(*pfnChangeWindowMessageFilter)(WM_COPYDATA,1);
								(*pfnChangeWindowMessageFilter)(0x0049,1);
						}

						manager_g->populate();
						manager_g->filter(Settings.filters);
						manager_g->setpos();
						break;

				case WM_CLOSE:
						if(installmode==MODE_NONE||(Settings.flags&FLAG_AUTOCLOSE))
								DestroyWindow(hwnd);
						else if(MessageBox(hMain,STR(STR_INST_QUIT_MSG),STR(STR_INST_QUIT_TITLE),MB_YESNO|MB_ICONQUESTION)==IDYES)
						{
								installmode=MODE_STOPPING;
								#ifdef USE_TORRENT
								Updater->pause();
								#endif
						}
						break;

				case WM_DESTROY:
						if(GetWindowPlacement(hwnd, &wndp))
						{
								Settings.wndsc=wndp.showCmd;
								Settings.wndwx=wndp.rcNormalPosition.right-wndp.rcNormalPosition.left;
								Settings.wndwy=wndp.rcNormalPosition.bottom-wndp.rcNormalPosition.top;
						}
						vLang->StopMonitor();
						vTheme->StopMonitor();
						delete canvasMain;
						delete Popup;
						PostQuitMessage(0);
						break;

				case WM_UPDATELANG:
						hLang->Clear();
						vLang->EnumFiles(hLang,L"langs",manager_g->getState()->getLocale());
						f=vLang->AutoPick();
						if(f<0)f=hLang->GetNumItems()-1;
						vLang->SwitchData((int)f);
						hLang->SetCurSel(f);
						//SendMessage(hLang,CB_SETCURSEL,f,0);
						lang_refresh();
						break;

				case WM_UPDATETHEME:
						hTheme->Clear();
						vTheme->EnumFiles(hTheme,L"Themes");
						f=hTheme->FindItem(Settings.curtheme);
						if(f==CB_ERR)f=vTheme->AutoPick();
						vTheme->SwitchData((int)f);
						if(Settings.wndwx)D(MAINWND_WX)=Settings.wndwx;
						if(Settings.wndwy)D(MAINWND_WY)=Settings.wndwy;
						hTheme->SetCurSel(f);
						theme_refresh(0);

						// Move to the center of the screen
						if(!wParam)break;
						GetWindowRect(GetDesktopWindow(),&rect);
						wndp.showCmd=Settings.wndsc;
						wndp.rcNormalPosition.left=(rect.right-D(MAINWND_WX))/2;
						wndp.rcNormalPosition.top=(rect.bottom-D(MAINWND_WY))/2;
						wndp.rcNormalPosition.right=wndp.rcNormalPosition.left+D(MAINWND_WX);
						wndp.rcNormalPosition.bottom=wndp.rcNormalPosition.top+D(MAINWND_WY);
						SetWindowPlacement(hwnd, &wndp);
						break;

				case WM_SEEDING:
						{
								switch(lParam)
								{
										case 1:
												ModifyMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_SEED,0,const_cast<wchar_t *>(STR(STR_SYST_STOP_SEED)));
												break;
										case 0:
												ModifyMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_SEED,0,const_cast<wchar_t *>(STR(STR_SYST_START_SEED)));
												break;
										default:
												break;
								}
						}
						break;

				case WM_BUNDLEREADY:
						{
								Bundle *bb=reinterpret_cast<Bundle *>(wParam);
								Manager *manager_prev=manager_g;
								uprintf("{Sync");
								if(CRITICAL_SECTION_ACTIVE)EnterCriticalSection(&sync);
								uprintf("...\n");
								manager_active++;
								manager_active&=1;
								manager_g=&manager_v[manager_active];
								manager_g->matcher=bb->getMatcher();
								manager_g->restorepos1(manager_prev);
						}
						break;

				case WM_TORRENT:
						{
								DownloadedTorrent(lParam);
								break;
						}
				case WM_INDICESSAVED:
						{
								break;
						}
				case WM_DROPFILES:
						{
								wchar_t lpszFile[MAX_PATH]={0};
								UINT uFile=0;
								HDROP hDrop=(HDROP)wParam;

								uFile=DragQueryFile(hDrop,0xFFFFFFFF,nullptr,0);
								if(uFile!=1)
								{
										//MessageBox(0,L"Dropping multiple files is not supported.",NULL,MB_ICONERROR);
										DragFinish(hDrop);
										break;
								}

								lpszFile[0] = '\0';
								if(DragQueryFile(hDrop,0,lpszFile,MAX_PATH))
								{
										uFile=GetFileAttributes(lpszFile);
										if(uFile!=INVALID_FILE_ATTRIBUTES&&uFile&FILE_ATTRIBUTE_DIRECTORY)
										{
												wcscpy(Settings.drpext_dir,lpszFile);
												invalidate(INVALIDATE_INDICES|INVALIDATE_MANAGER);
										}
										else if(StrStrIW(lpszFile,L".snp"))
										{
												wcscpy(Settings.state_file,lpszFile);
												Settings.statemode=STATEMODE_EMUL;
												invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
										}
										//else
										//    MessageBox(NULL,lpszFile,NULL,MB_ICONINFORMATION);
								}
								DragFinish(hDrop);
						}
						break;

				case WM_WINDOWPOSCHANGING:
						{
								Settings.autosized=false;
								WINDOWPOS *wpos=(WINDOWPOS*)lParam;

								rect.left=GetSystemMetrics(SM_XVIRTUALSCREEN);
								rect.top=GetSystemMetrics(SM_YVIRTUALSCREEN);
								rect.right=GetSystemMetrics(SM_CXVIRTUALSCREEN);
								rect.bottom=GetSystemMetrics(SM_CYVIRTUALSCREEN);

								if(rect.right<D(MAINWND_WX)||rect.bottom<D(MAINWND_WY))
									if(rect.right<wpos->cx||rect.bottom<wpos->cy) {
										Settings.autosized=true;

										wpos->x=rect.left;
										wpos->y=rect.top;
										wpos->cx=rect.right-wpos->x;
										wpos->cy=rect.bottom-wpos->y;
										//vvuprintf("%d,%d,%d,%d\n",rect.left,rect.top,rect.right,rect.bottom);
										Settings.scale=750*256/wpos->cy;
										//vvuprintf("(%d,%d,%d)\n",wpos->cx,wpos->cy,Settings.scale);
										MainWindow.theme_refresh(0);
									}
						}
						break;

				case WM_SIZING:
						{
								RECT *r=(RECT *)lParam;
								int minx=D_X(MAINWND_MINX);
								int miny=D_X(MAINWND_MINY);

								switch(wParam)
								{
										case WMSZ_LEFT:
										case WMSZ_TOPLEFT:
										case WMSZ_BOTTOMLEFT:
												if(r->right-r->left<minx)r->left=r->right-minx;
												break;

										case WMSZ_BOTTOM:
										case WMSZ_RIGHT:
										case WMSZ_TOP:
										case WMSZ_BOTTOMRIGHT:
										case WMSZ_TOPRIGHT:
												if(r->right-r->left<minx)r->right=r->left+minx;
												break;

										default:
												break;
								}
								switch(wParam)
								{
										case WMSZ_TOP:
										case WMSZ_TOPLEFT:
										case WMSZ_TOPRIGHT:
												if(r->bottom-r->top<miny)r->top=r->bottom-miny;
												break;

										case WMSZ_BOTTOM:
										case WMSZ_BOTTOMLEFT:
										case WMSZ_BOTTOMRIGHT:
										case WMSZ_LEFT:
										case WMSZ_RIGHT:
												if(r->bottom-r->top<miny)r->bottom=r->top+miny;
												break;

										default:
												break;
								}
								break;
						}

				case WM_KEYUP:
						if(ctrl_down&&(wParam==L'0'||wParam==VK_NUMPAD0))
						{
								Settings.scale=256;
								Settings.savedscale=Settings.scale;
								PostMessage(hwnd,WM_UPDATETHEME,0,0);
						}
						if(ctrl_down&&(wParam==VK_OEM_PLUS||wParam==VK_ADD))
						{
								Settings.scale-=6;
								Settings.savedscale=Settings.scale;
								PostMessage(hwnd,WM_UPDATETHEME,0,0);
						}
						if(ctrl_down&&(wParam==VK_OEM_MINUS||wParam==VK_SUBTRACT))
						{
								Settings.scale+=6;
								Settings.savedscale=Settings.scale;
								PostMessage(hwnd,WM_UPDATETHEME,0,0);
						}
						if(ctrl_down&&wParam==L'Z'){uprintf("\n*************\n");}
						if(ctrl_down&&wParam==L'A'){SelectAllCommand c;c.LeftClick();}
						if(ctrl_down&&wParam==L'N'){SelectNoneCommand c;c.LeftClick();}
						if(ctrl_down&&wParam==L'I'){InstallCommand c;c.LeftClick();}
						if(ctrl_down&&wParam==L'P')
						{
								ClickVisiter cv{ID_RESTPNT};
								wPanels->Accept(cv);
						}
						if(ctrl_down&&wParam==L'R')
						{
								ClickVisiter cv{ID_REBOOT};
								wPanels->Accept(cv);
						}
						if(wParam==VK_F1)
								CreateAboutBox();
						if(wParam==VK_F5&&ctrl_down)
								invalidate(INVALIDATE_DEVICES);else
						if(wParam==VK_F5)
								invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_INDICES|INVALIDATE_MANAGER);
						if(wParam==VK_F6&&ctrl_down)
						{
								manager_g->testitembars();
								manager_g->setpos();
								redrawfield();
						}
						if(wParam==VK_F7)
						{
								save_wndinfo();
								MessageBox(hMain,L"Windows data recorded into the log.",L"Message",0);
						}
						if(wParam==VK_F8)
						{
								switch(Settings.flags&(FLAG_SHOWDRPNAMES1|FLAG_SHOWDRPNAMES2))
								{
										case FLAG_SHOWDRPNAMES1:
												Settings.flags^=FLAG_SHOWDRPNAMES1;
												Settings.flags^=FLAG_SHOWDRPNAMES2;
												break;

										case FLAG_SHOWDRPNAMES2:
												Settings.flags^=FLAG_SHOWDRPNAMES2;
												break;

										case 0:
												Settings.flags^=FLAG_SHOWDRPNAMES1;
												break;

										default:
												break;
								}
								manager_g->filter(Settings.filters);
								manager_g->setpos();
								redrawfield();
						}
						if(wParam==VK_SPACE)
						{
								space_down=0;
								Popup->AddShift(0xffff);
						}
						break;

				case WM_SYSKEYDOWN:
						if(wParam==VK_MENU)break;
						return DefWindowProc(hwnd,uMsg,wParam,lParam);

				case WM_DEVICECHANGE:
						if(installmode==MODE_INSTALLING)break;
						//uprintf("WM_DEVICECHANGE(%x,%x)\n",wParam,lParam);
						invalidate(INVALIDATE_DEVICES);
						break;

				case WM_SIZE:
						SetLayeredWindowAttributes(hMain,0,(BYTE)D_1(MAINWND_TRANSPARENCY),LWA_ALPHA);
						Popup->setTransparency();

						GetWindowRect(hwnd,&rect);
						main1x_c=x;
						main1y_c=y;

						//i=D_X(PNLITEM_OFSX)+D_X(PANEL_LIST_OFSX);
						//f=D_X(PANEL_LIST_OFSX)?4:0;
						MoveWindow(hwndFrame,Xm(D_X(DRVLIST_OFSX),D_X(DRVLIST_WX)),Ym(D_X(DRVLIST_OFSY)),XM(D_X(DRVLIST_WX),D_X(DRVLIST_OFSX)),YM(D_X(DRVLIST_WY),D_X(DRVLIST_OFSY)),TRUE);

						wPanels->arrange();
						manager_g->setpos();

						redrawmainwnd();
						break;

				case WM_TIMER:
						if(manager_g->animate())redrawfield();
						else
						{
								if(wParam==2)MainWindow.ResetUpdater(Updater_t::activetorrent);
								KillTimer(hwnd,wParam);
						}
						break;

				case WM_PAINT:
						GetClientRect(hwnd,&rect);
						canvasMain->begin(&hwnd,rect.right,rect.bottom);

						canvasMain->DrawWidget(0,0,rect.right+1,rect.bottom+1,BOX_MAINWND);
						canvasMain->SetFont(hFont);
						drawnew(*canvasMain);
						canvasMain->end();
						break;

				case WM_SYSCOMMAND:
						{
								wp=LOWORD(wParam);
								switch (wp)
								{
										case IDM_ABOUT:
										{
												CreateAboutBox();
												return 0;
										}
										case IDC_LOG:
												// Place the log Window to the right (or left for RTL) of our dialog on first display
												if (first_log_display) {
														GetClientRect(GetDesktopWindow(), &DesktopRect);
														GetWindowRect(hLogDialog, &DialogRect);
														nWidth = DialogRect.right - DialogRect.left;
														nHeight = DialogRect.bottom - DialogRect.top;
														GetWindowRect(hwnd, &DialogRect);
														offset = GetSystemMetrics(SM_CXBORDER);
														if (WindowsVersion.Version >= WINDOWS_10) {
																// See https://stackoverflow.com/a/42491227/1069307
																// I agree with Stephen Hazel: Whoever at Microsoft thought it would be a great idea to
																// add a *FRIGGING INVISIBLE BORDER* in Windows 10 should face the harshest punishment!
																// Also calling this API will create DLL sideloading issues through 'dwmapi.dll' so make
																// sure you delay-load it in your application.
																DwmGetWindowAttribute(hLogDialog, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(RECT));
																offset += 2 * (DialogRect.left - rc.left);
														}
														if (right_to_left_mode)
																Point.x = std::max(DialogRect.left - offset - nWidth, static_cast<LONG>(0));
														else
																Point.x = std::min(DialogRect.right + offset, DesktopRect.right - nWidth);

														Point.y = std::max(DialogRect.top, DesktopRect.top - nHeight);
														MoveWindow(hLogDialog, Point.x, Point.y, nWidth, nHeight, FALSE);
														// The log may have been recentered to fit the screen, in which case, try to shift our main dialog left (or right for RTL)
														nWidth = DialogRect.right - DialogRect.left;
														nHeight = DialogRect.bottom - DialogRect.top;
														if (right_to_left_mode) {
																Point.x = DialogRect.left;
																GetWindowRect(hLogDialog, &DialogRect);
																Point.x = std::max(Point.x, DialogRect.right - DialogRect.left + offset);
														}
														else {
																Point.x = std::max((DialogRect.left < 0) ? DialogRect.left : 0, Point.x - offset - nWidth);
														}
														MoveWindow(hwnd, Point.x, Point.y, nWidth, nHeight, TRUE);
														first_log_display = FALSE;
												}
												// Display the log Window
												log_displayed = !log_displayed;
												// Set focus on the start button
												SendMessage(hMainDialog, WM_NEXTDLGCTL, (WPARAM)FALSE, 0);
												SendMessage(hMainDialog, WM_NEXTDLGCTL, (WPARAM)hStart, TRUE);
												// Must come last for the log window to get focus
												ShowWindow(hLogDialog, log_displayed ? SW_SHOW : SW_HIDE);
												break;
										case IDM_SEED:
										{
												#ifdef USE_TORRENT
												if(Updater)
												{
														if(Updater->isSeedingDrivers())Updater->StopSeedingDrivers();
														else Updater->StartSeedingDrivers();
												}
												#endif // USE_TORRENT
												return 0;
										}
										case IDM_UPDATES_SDI:
												{
														TorrentSelectionMode=TSM_NONE;
														MainWindow.ResetUpdater(1);
														return 0;
												}
										case IDM_UPDATES_DRIVERS:
												{
														TorrentSelectionMode=TSM_NONE;
														MainWindow.ResetUpdater(2);
														return 0;
												}
										case ID_COMPMNG:
												{
														// works on Windows XP and up
														System.run_command(L"compmgmt.msc",nullptr,SW_SHOW,0);
														return 0;
												}
										case ID_DEVICEMNG:
												{
														// works on Windows XP and up
														System.run_command(L"devmgmt.msc",nullptr,SW_SHOW,0);
														return 0;
												}
										case ID_DEVICEPRNT:
												{
														// works on Windows Vista and up
														System.run_controlpanel(L"/name Microsoft.DevicesAndPrinters");
														return 0;
												}
										case ID_SYSPROPS:
												{
														// works on Windows Vista and up
														System.run_controlpanel(L"system");
														return 0;
												}
										case ID_SYSPROPS_ADV:
												{
														// works on Windows Vista and up
														System.run_command32(L"SystemPropertiesAdvanced",nullptr,SW_NORMAL,0);
														return 0;
												}
										case ID_SYSCONTROL:
												{
														// works on Windows XP and up
														System.run_controlpanel(nullptr);
														return 0;
												}
										case ID_SYSPROT:
												{
														// works on Windows Vista and up
														System.run_command32(L"%windir%\\System32\\SystemPropertiesProtection.exe",nullptr,SW_NORMAL,0);
														return 0;
												}
										case ID_SYSREST:
												{
														// Windows XP
														std::wstring b=System.ExpandEnvVar(L"%windir%\\system32\\restore\\rstrui.exe");
														if(System.FileExists2(b.c_str()))
														{
																b=L"/c " + b;
																System.run_command(L"cmd",b.c_str(),SW_HIDE,0);
														}
														else
														{
																// works on Windows Vista and up
																System.run_command32(L"%windir%\\System32\\rstrui.exe",nullptr,SW_NORMAL,0);
														}
														return 0;
												}
										case IDM_DRVDIR:
										{
												if(System.ChooseDir(Settings.drpext_dir,STR(STR_DRVDIR)))
												{
														invalidate(INVALIDATE_INDICES|INVALIDATE_MANAGER);
												}
												return 0;
										}
										case IDM_OPENLOGS:
										{
												ShellExecute(MainWindow.hMain,L"explore",Settings.log_dir,nullptr,nullptr,SW_SHOW);
												return 0;
										}
										case IDM_WELCOME:
										{
												DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_WELCOME), MainWindow.hMain,(DLGPROC)WelcomeCallback);
												return 0;
										}
										case IDM_LICENSE:
										{
												DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_LICENSE),MainWindow.hMain,(DLGPROC)LicenseCallback);
												return 0;
										}
										case IDM_USBWIZARD:
										{
												USBWiz=new USBWizard;
												USBWiz->doWizard();
												delete USBWiz;
												return 0;
										}
										default:
												return DefWindowProc(hwnd, WM_SYSCOMMAND, wParam, lParam);
								}
						}

				case WM_ERASEBKGND:
						return 1;

				case WM_MOUSEMOVE:
						{
								HoverVisiter hv{x,y};
								wPanels->Accept(hv);
						}
						break;

				case WM_LBUTTONUP:
						if(mouseclick)
						{
								ClickVisiter cv{x,y};
								wPanels->Accept(cv);
						}
						break;

				case WM_RBUTTONUP:
						{
								ClickVisiter cv{x,y,true};
								wPanels->Accept(cv);
						}
						break;

				case WM_MOUSEWHEEL:
						i=GET_WHEEL_DELTA_WPARAM(wParam);
						if(ctrl_down)
						{
								Settings.scale-=i/20;
								if(Settings.scale<150){Settings.scale=150;break;}
								if(Settings.scale>350){Settings.scale=350;break;}
								Settings.savedscale=Settings.scale;
								Settings.wndwx=0;
								Settings.wndwy=0;
								PostMessage(hwnd,WM_UPDATETHEME,0,0);
						}
						if(space_down)
								Popup->AddShift(i);
						else
								SendMessage(hwndFrame,WM_VSCROLL,MAKELONG(i>0?SB_LINEUP:SB_LINEDOWN,0),0);
						break;

				case WM_COMMAND:
						wp=LOWORD(wParam);
						switch(wp)
						{
								case ID_SCHEDULE:
										manager_g->toggle(Popup->floating_itembar);
										redrawfield();
										break;

								case ID_SHOWALT:
										if(Popup->floating_itembar==SLOT_RESTORE_POINT)
										{
												// windows XP
												System.run_command(L"cmd",L"/c %windir%\\system32\\restore\\rstrui.exe",SW_HIDE,0);
												// access the 64-bit version from a 32-bit app - this works only on 64-bit windows
												System.run_command(L"cmd",L"/c %windir%\\Sysnative\\rstrui.exe",SW_HIDE,0);
												// otherwise do the normal call
												System.run_command(L"cmd",L"/c rstrui.exe",SW_HIDE,0);
										}
										else
										{
												manager_g->expand(Popup->floating_itembar,EXPAND_MODE::TOGGLE);
										}
										break;

								case ID_OPENINF:
								case ID_LOCATEINF:
										manager_g->getINFpath(wp);
										break;

								case ID_DEVICEMNG:
										System.run_command(L"devmgmt.msc",nullptr,SW_SHOW,0);
										break;

								case ID_EMU_32:
										Settings.virtual_arch_type=32;
										invalidate(INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
										break;

								case ID_EMU_64:
										Settings.virtual_arch_type=64;
										invalidate(INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
										break;

                case ID_DETECT_OS:
                    Settings.virtual_os_version=0;
                    Settings.virtual_arch_type=0;
                    invalidate(INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
                    break;

                case ID_DIS_INSTALL:
                    Settings.flags^=FLAG_DISABLEINSTALL;
                    break;

								case ID_DIS_RESTPNT:
										Settings.flags^=FLAG_NORESTOREPOINT;
										manager_g->itembar_setactive(SLOT_RESTORE_POINT,(Settings.flags&FLAG_NORESTOREPOINT)?0:1);
										manager_g->set_rstpnt(0);
										break;

                default:
                    break;
            }
            // select a virtual OS from the menu
            if(wp>=ID_OS_ITEMS&&wp<ID_OS_ITEMS+winVersions.Count())
            {
                vuprintf("Virtual OS Version: %S\n\n",winVersions.GetEntryW(wp-ID_OS_ITEMS));
                Settings.virtual_os_version=wp;
                invalidate(INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
            }
            if(wp>=ID_HWID_CLIP&&wp<=ID_HWID_WEB+100)
            {
                int id=wp%100;
                if(wp>=ID_HWID_WEB)
                {
                    wchar_t buf[MAX_DEVICE_ID_LEN+51];
                    wchar_t buf2[sizeof(buf)+20];
                    const wchar_t *str=manager_g->getHWIDby(id);
                    wsprintf(buf,L"https://catalog.update.microsoft.com/search.aspx?q=%s",str);
                    escapeAmpUrl(buf2,buf);
                    System.run_command(L"open",buf2,SW_SHOW,0);

								}
								else
								{
										const wchar_t *str=manager_g->getHWIDby(id);
										size_t len=wcslen(str)*2+2;
										HGLOBAL hMem=GlobalAlloc(GMEM_MOVEABLE,len);
										memcpy(GlobalLock(hMem),str,len);
										GlobalUnlock(hMem);
										OpenClipboard(nullptr);
										EmptyClipboard();
										SetClipboardData(CF_UNICODETEXT,hMem);
										CloseClipboard();
								}
						}

						if(HIWORD(wParam)==CBN_SELCHANGE)
						{
								if(wp==ID_LANG)
								{
					LRESULT j=SendMessage((HWND)lParam,CB_GETCURSEL,0,0);
										SendMessage((HWND)lParam,CB_GETLBTEXT,j,(LPARAM)Settings.curlang);
										vLang->SwitchData((int)j);
										lang_refresh();
								}

								if(wp==ID_THEME)
								{
					LRESULT j=SendMessage((HWND)lParam,CB_GETCURSEL,0,0);
										SendMessage((HWND)lParam,CB_GETLBTEXT,j,(LPARAM)Settings.curtheme);
					vTheme->SwitchData((int)j);
					Settings.autosized=false;
										theme_refresh(0);
								}
						}
						break;

				default:
						{
								LRESULT j=DefWindowProc(hwnd,uMsg,wParam,lParam);
								//checktimer(L"MainD",timer,uMsg);
								return j;
						}
		}
		//checktimer(L"Main",timer,uMsg);
		return 0;
}

void RestPointCheckboxCommand::RightClick(int x,int y)
{
		Popup->floating_itembar=SLOT_RESTORE_POINT;
		manager_g->contextmenu(x-Xm(D_X(DRVLIST_OFSX),D_X(DRVLIST_WX)),y-Ym(D_X(DRVLIST_OFSY)));
}

//{ Buttons
void RefreshCommand::LeftClick(bool)
{
    invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_INDICES|INVALIDATE_MANAGER);
}

void SnapshotCommand::LeftClick(bool)
{
    MainWindow.snapshot();
}

void ExtractCommand::LeftClick(bool)
{
    MainWindow.extractto();
}

void DrvDirCommand::LeftClick(bool)
{
    MainWindow.selectDrpDir();
}

void DrvOptionsCommand::LeftClick(bool)
{
		DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_DIALOG3),MainWindow.hMain,(DLGPROC)SettingsDialog);
}

void InstallCommand::LeftClick(bool)
{
    if(installmode==MODE_NONE)
    {
        if((Settings.flags&FLAG_EXTRACTONLY)==0)
        wsprintf(extractdir,L"%s\\SDI",manager_g->getState()->textas.getw(manager_g->getState()->getTemp()));
        manager_g->install(INSTALLDRIVERS);
    }
}

void SelectAllCommand::LeftClick(bool)
{
    manager_g->selectall();
    MainWindow.redrawmainwnd();
    MainWindow.redrawfield();
}

void SelectNoneCommand::LeftClick(bool)
{
    manager_g->selectnone();
    MainWindow.redrawmainwnd();
    MainWindow.redrawfield();
}
//}

LRESULT CALLBACK MainWindow_t::WndProcFieldCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    return MainWindow.WndProcField(hwnd,uMsg,wParam,lParam);
}

LRESULT MainWindow_t::WndProcField(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    SCROLLINFO si;
    RECT rect;
    int x,y;
    long long timer=System.GetTickCountWr();
    int i;

    x=LOWORD(lParam);
    y=HIWORD(lParam);
    if(WndProcCommon(hwnd,message,wParam,lParam))
    switch(message)
    {
        case WM_CREATE:
            canvasField=Canvas::Create();
            break;

        case WM_PAINT:
            y=getscrollpos();

            GetClientRect(hwnd,&rect);
            canvasField->begin(&hwnd,rect.right,rect.bottom);
            canvasField->CopyCanvas(canvasMain,Xm(D_X(DRVLIST_OFSX),D_X(DRVLIST_WX)),Ym(D_X(DRVLIST_OFSY)));
            canvasField->SetFont(hFont);
            manager_g->draw(*canvasField,y);
            canvasField->end();
            break;

        case WM_DESTROY:
            delete canvasField;
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            mainx_c=x;
            mainy_c=y;
            if(scrollvisible)mainx_c-=GetSystemMetrics(SM_CXVSCROLL);
            break;

        case WM_VSCROLL:
            si.cbSize=sizeof(si);
            si.fMask=SIF_ALL;
            si.nPos=getscrollpos();
            GetScrollInfo(hwnd,SB_VERT,&si);
            switch(LOWORD(wParam))
            {
                case SB_LINEUP:si.nPos-=35;break;
                case SB_LINEDOWN:si.nPos+=35;break;
                case SB_PAGEUP:si.nPos-=si.nPage;break;
                case SB_PAGEDOWN:si.nPos+=si.nPage;break;
                case SB_THUMBTRACK:si.nPos=si.nTrackPos;break;
                default:break;
            }
            offset_target=0;
            setscrollpos(si.nPos);
            redrawfield();
            break;

        case WM_LBUTTONUP:
            if(!mouseclick)break;
            manager_g->hitscan(x,y,&Popup->floating_itembar,&i);
            if(Popup->floating_itembar==SLOT_SNAPSHOT)
            {
                Settings.statemode=STATEMODE_REAL;
                invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
            }
            if(Popup->floating_itembar==SLOT_DPRDIR)
            {
                *Settings.drpext_dir=0;
                invalidate(INVALIDATE_INDICES|INVALIDATE_MANAGER);
            }
            if(Popup->floating_itembar==SLOT_EXTRACTING)
            {
                if(installmode==MODE_INSTALLING)
                    installmode=MODE_STOPPING;
                else if(installmode==MODE_NONE)
                    manager_g->clear();
            }
            if(Popup->floating_itembar==SLOT_DOWNLOAD)
            {
                #ifdef USE_TORRENT
                if(Updater->isSeedingDrivers())Updater->StopSeedingDrivers();
                else Updater->OpenDialog();
                #endif
                break;
            }
            if(Popup->floating_itembar>0&&(i==1||i==0||i==3))
            {
                manager_g->toggle(Popup->floating_itembar);
                if(wParam&MK_SHIFT&&installmode==MODE_NONE)
                {
                    if((Settings.flags&FLAG_EXTRACTONLY)==0)
                    wsprintf(extractdir,L"%s\\SDI",manager_g->getState()->textas.getw(manager_g->getState()->getTemp()));
                    manager_g->install(INSTALLDRIVERS);
                }
                redrawfield();
            }
            if(Popup->floating_itembar>0&&i==2)
            {
                manager_g->expand(Popup->floating_itembar,EXPAND_MODE::TOGGLE);
            }
            break;

        case WM_RBUTTONDOWN:
            manager_g->hitscan(x,y,&Popup->floating_itembar,&i);
            if(Popup->floating_itembar>0&&(i==0||i==3))
                manager_g->contextmenu(x,y);
            break;

        case WM_MBUTTONDOWN:
            mousedown=MOUSE_SCROLL;
            mousex=x;
            mousey=y;
            SetCursor(LoadCursor(nullptr,IDC_SIZEALL));
            SetCapture(hwnd);
            break;

        case WM_MOUSEMOVE:
            si.cbSize=sizeof(si);
            if(mousedown==MOUSE_SCROLL)
            {
                si.fMask=SIF_ALL;
                si.nPos=0;
                GetScrollInfo(hwnd,SB_VERT,&si);
                si.nPos+=mousey-y;
                si.fMask=SIF_POS;
                SetScrollInfo(hwnd,SB_VERT,&si,TRUE);

                mousex=x;
                mousey=y;
                redrawfield();
            }
            {
                int type=FLOATING_NONE;
                size_t itembar_i;

                if(space_down&&kbpanel)break;

                if(space_down)type=FLOATING_DRIVERLST;else
                if(ctrl_down||Settings.expertmode)type=FLOATING_CMPDRIVER;

                manager_g->hitscan(x,y,&itembar_i,&i);
                if((i==0||i==3)&&itembar_i>=RES_SLOTS&&(ctrl_down||space_down||Settings.expertmode))
                    Popup->drawpopup(itembar_i,0,type,x,y,hwndFrame);
                else if(itembar_i==SLOT_VIRUS_AUTORUN)
                    Popup->drawpopup(itembar_i,STR_VIRUS_AUTORUN_H,FLOATING_TOOLTIP,x,y,hwndFrame);
                else if(itembar_i==SLOT_VIRUS_RECYCLER)
                    Popup->drawpopup(itembar_i,STR_VIRUS_RECYCLER_H,FLOATING_TOOLTIP,x,y,hwndFrame);
                else if(itembar_i==SLOT_VIRUS_HIDDEN)
                    Popup->drawpopup(itembar_i,STR_VIRUS_HIDDEN_H,FLOATING_TOOLTIP,x,y,hwndFrame);
                else if(itembar_i==SLOT_EXTRACTING&&installmode)
                    Popup->drawpopup(itembar_i,(instflag&INSTALLDRIVERS)?STR_HINT_STOPINST:STR_HINT_STOPEXTR,FLOATING_TOOLTIP,x,y,hwndFrame);
                else if(itembar_i==SLOT_RESTORE_POINT)
                    Popup->drawpopup(itembar_i,STR_RESTOREPOINT_H,FLOATING_TOOLTIP,x,y,hwndFrame);
                else if(itembar_i==SLOT_DOWNLOAD)
                    Popup->drawpopup(itembar_i,0,FLOATING_DOWNLOAD,x,y,hwndFrame);
                else if(i==0&&itembar_i>=RES_SLOTS)
                    Popup->drawpopup(itembar_i,STR_HINT_DRIVER,FLOATING_TOOLTIP,x,y,hwndFrame);
                else
                    Popup->drawpopup(0,0,FLOATING_NONE,0,0,hwndFrame);

                if(itembar_i!=field_lasti||i!=field_lastz)redrawfield();
                field_lasti=itembar_i;
                field_lastz=i;
            }
            break;

        default:
			{
				LRESULT j=DefWindowProc(hwnd,message,wParam,lParam);
				checktimer(L"ListD",timer,message);
				return j;
			}
    }
    checktimer(L"List",timer,message);
    return 0;
}


void MainWindow_t::lang_refresh()
{
		if(!hMain||!hwndFrame)
		{
				uprintf("ERROR in lang_refresh(): hMain is %d, hwndFrame is %d\n",hMain,hwndFrame);
				return;
		}

		right_to_left_mode=std::get<int>(language[STR_RTL].value);
		if(right_to_left_mode!=1)right_to_left_mode=0;
		setMirroring(hwndFrame);
		setMirroring(hMain);
		hLang->SetMirroring();
		hTheme->SetMirroring();
		Popup->setMirroring();

		RECT rect;
		GetWindowRect(hMain,&rect);
		MoveWindow(hMain,rect.left,rect.top,D(MAINWND_WX),D(MAINWND_WY)+1,1);
		MoveWindow(hMain,rect.left,rect.top,D(MAINWND_WX),D(MAINWND_WY),1);

		LoadMenuItems();
}

std::vector<std::wstring> split(const std::wstring &s, wchar_t delim)
{
		std::vector<std::wstring> elems;
		split(s, delim, std::back_inserter(elems));
		return elems;
}

LRESULT CALLBACK PopupProcedure(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
		return Popup->PopupProcedure2(hwnd,message,wParam,lParam);
}

//{ 7-zip
size_t encode(char *dest,size_t dest_sz,const char *src,size_t src_sz)
{
    Lzma86_Encode((Byte *)dest,(SizeT *)&dest_sz,(const Byte *)src,src_sz,0,1<<23,SZ_FILTER_AUTO);
    return dest_sz;
}

size_t decode(char *dest,size_t dest_sz,const char *src,size_t src_sz) {
		Lzma86_Decode((Byte *)dest,(SizeT *)&dest_sz,(const Byte *)src,(SizeT *)&src_sz);
		return dest_sz;
}

		void registerall()
{
		NArchive::N7z::register7z();
		registerBCJ();
		registerBCJ2();
		registerBranch();
		//registerCopy();
		registerLZMA();
		registerLZMA2();

		//CrcGenerateTable();
}
//}
