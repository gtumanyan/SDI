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
#include "system.h"     // non-portable
#include "settings.h"
#include "cli.h"
#include "indexing.h"
#include "manager.h"
#include "update.h"
#include "install.h"    // non-portable
#include "gui.h"
#include "draw.h"   // non-portable
#include "theme.h"
#include "usbwizard.h"

#include <winuser.h>
#include <setupapi.h>       // for CommandLineToArgvW
#include <shobjidl.h>       // for TBPF_NORMAL
#ifdef _MSC_VER
#include <shellapi.h>
#endif
#include <process.h>
#include <signal.h>
#include <iostream>

// Depend on Win32API
#include "enum.h"   // non-portable
#include "main.h"
#include "model.h"
#include "script.h"

#include "wizards.h"

//{ Global variables
Manager manager_v[2];
Manager *manager_g=&manager_v[0];
Console_t *Console;
USBWizard *USBWiz;

volatile int installupdate_exitflag=0;
Event *installupdate_event;

volatile int deviceupdate_exitflag=0;
Event *deviceupdate_event;
HINSTANCE ghInst;
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

// http://www.winprog.org/tutorial/dlgfaq.html
HBRUSH g_hbrDlgBackground = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

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

class Console1:public Console_t
{
    bool keep_open;

public:
    Console1()
    {
        DWORD dwProcessId;
        GetWindowThreadProcessId(GetConsoleWindow(),&dwProcessId);
        keep_open=GetCurrentProcessId()!=dwProcessId;
        if(!keep_open)ShowWindow(GetConsoleWindow(),SW_HIDE);
    }
    ~Console1()
    {
        if(keep_open)return;
        ShowWindow(GetConsoleWindow(),SW_SHOW);
    }
    void Show()
    {
        if(keep_open)return;
        ShowWindow(GetConsoleWindow(),SW_SHOWNOACTIVATE);
    }
    void Hide()
    {
        if(keep_open)return;
        ShowWindow(GetConsoleWindow(),SW_HIDE);
    }
};

class Console2:public Console_t
{
public:
    ~Console2()
    {
        FreeConsole();
    }
    void Show()
    {
        AllocConsole();
        freopen("CONIN$","r",stdin);
        freopen("CONOUT$","w",stdout);
        freopen("CONOUT$","w",stderr);
    }
    void Hide()
    {
        FreeConsole();
    }
};

//{  Main
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hinst,LPSTR pStr,int nCmd)
{
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(pStr);
    ghInst=hInst;

    Timers.start(time_total);

    std::cout << VERSION_FILEVERSION_LONG << "\n";
    std::cout << VERSION_COMMIT_ID << "\n\n";

    // Determine number of CPU cores ("Logical Processors")
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    num_cores=siSysInfo.dwNumberOfProcessors;

    // 7-zip
    registerall();

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

    // Hide the console window as soon as possible
#ifdef _MSC_VER
    Console=new Console2;
#else
    Console=new Console1;
#endif

    // Check if the mouse present
    if(!GetSystemMetrics(SM_MOUSEPRESENT))MainWindow.kbpanel=KB_FIELD;

    // Runtime error handlers
    //start_exception_handlers();
    //HMODULE backtrace=LoadLibraryA("backtrace.dll");
    //if(!backtrace)signal(SIGSEGV,SignalHandler);

    // Load settings
    init_CLIParam();
    if(!Settings.load_cfg_switch(GetCommandLineW()))
        Settings.load(L"sdi.cfg");

    Settings.parse(GetCommandLineW(),1);
    RUN_CLI();

    // Close the app if the work is done
    if(Settings.statemode==STATEMODE_EXIT)
    {
        //if(backtrace)FreeLibrary(backtrace);
        delete Console;
        return ret_global;
    }

    // Bring back the console window
    if(Settings.flags&FLAG_SHOWCONSOLE)
        Console->Show();
    else
        Console->Hide();

    // Start logging
    ExpandEnvironmentStrings(Settings.logO_dir,Settings.log_dir,BUFLEN);
    Log.start(Settings.log_dir);
    Settings.loginfo();
    #ifndef NDEBUG
    Log.print_con("Debug info present\n");
    //if(backtrace)Log.print_con("Backtrace is loaded\n");
    #endif

    #ifdef BENCH_MODE
    System.benchmark();
    #endif

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
    invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_INDEXES|INVALIDATE_MANAGER);
    ThreadAbs *thr=CreateThread();
    thr->start(&Bundle::thread_loadall,&bundle[0]);

    // Check updates
    #ifdef USE_TORRENT
    Updater=CreateUpdater();
    TorrentSelectionMode=TSM_AUTO;
    #endif

    // Start folder monitors
    Filemon *mon_drp=CreateFilemon(Settings.drp_dir,1,drp_callback);
    //Filemon *mon_vir=CreateFilemon(L"\\",0,viruscheck);
    //viruscheck(L"",0,0);

    // MAIN GUI LOOP
    MainWindow.MainLoop(nCmd);

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
    //delete mon_vir;

    // Bring the console window back
    ShowWindow(GetConsoleWindow(),SW_SHOWNOACTIVATE);

    // Stop runtime error handlers
    /*if(backtrace)
        FreeLibrary(backtrace);
    else
        signal(SIGSEGV,SIG_DFL);*/

    // Stop logging
    //time_total=System.GetTickCountWr()-time_total;
    Timers.print();
    Log.stop();
    delete Console;

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

void MainWindow_t::MainLoop(int nCmd)
{
    if((Settings.flags&FLAG_NOGUI)&&(Settings.flags&FLAG_AUTOINSTALL)==0)return;

    // Register classMain
    WNDCLASSEX wcx;
    memset(&wcx,0,sizeof(WNDCLASSEX));
    wcx.cbSize=         sizeof(WNDCLASSEX);
    wcx.lpfnWndProc=    WndProcMainCallback;
    wcx.hInstance=      ghInst;
    wcx.hIcon=          LoadIcon(ghInst,MAKEINTRESOURCE(IDR_MAINWND));
    wcx.hCursor=        LoadCursor(nullptr,IDC_ARROW);
    wcx.lpszClassName=  classMain;
    wcx.hbrBackground=  (HBRUSH)(COLOR_WINDOW+1);
    if(!RegisterClassEx(&wcx))
    {
        Log.print_err("ERROR in gui(): failed to register '%S' class\n",wcx.lpszClassName);
        return;
    }

    // Register classPopup
    wcx.lpfnWndProc=PopupProcedure;
    wcx.lpszClassName=classPopup;
    wcx.hIcon=nullptr;
    if(!RegisterClassEx(&wcx))
    {
        Log.print_err("ERROR in gui(): failed to register '%S' class\n",wcx.lpszClassName);
        System.UnregisterClass_log(classMain,L"gui",L"classMain");
        return;
    }

    // Register classField
    wcx.lpfnWndProc=WndProcFieldCallback;
    wcx.lpszClassName=classField;
    if(!RegisterClassEx(&wcx))
    {
        Log.print_err("ERROR in gui(): failed to register '%S' class\n",wcx.lpszClassName);
        System.UnregisterClass_log(classMain,L"gui",L"classMain");
        System.UnregisterClass_log(classPopup,L"gui",L"classPopup");
        return;
    }

    // Main windows
    hMain=CreateWindowEx(WS_EX_LAYERED,
                        classMain,
                        _W(APPTITLE),
                        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                        CW_USEDEFAULT,CW_USEDEFAULT,D(MAINWND_WX),D(MAINWND_WY),
                        nullptr,nullptr,ghInst,nullptr);
    if(!hMain)
    {
        Log.print_err("ERROR in gui(): failed to create '%S' window\n",classMain);
        return;
    }

    // license dialog
    if(!Settings.license)
        DialogBox(ghInst,MAKEINTRESOURCE(IDD_LICENSE),nullptr,(DLGPROC)LicenseProcedure);

    // Enable updates notifications
    if(Settings.license==2)
    {
        /*int f;
        f=lang_enum(hLang,L"langs",manager_g->matcher->state->locale);
        Log.print_con("lang %d\n",f);
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
        ShowWindow(hMain,(Settings.flags&FLAG_NOGUI)?SW_HIDE:nCmd);
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
                            SendMessage(hField,WM_LBUTTONDOWN,0,0);
                            SendMessage(hField,WM_LBUTTONUP,0,0);
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
                        Popup->drawpopup(0,0,FLOATING_NONE,0,0,hField);
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

    System.UnregisterClass_log(classMain,L"gui",L"classMain");
    System.UnregisterClass_log(classPopup,L"gui",L"classPopup");
    System.UnregisterClass_log(classField,L"gui",L"classField");
}
//}

//{ Subroutes
void drp_callback(const wchar_t *szFile,int action,int lParam)
{
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(lParam);

    if(StrStrIW(szFile,L".7z")&&Updater->isPaused())invalidate(INVALIDATE_INDEXES);
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

void MainWindow_t::lang_refresh()
{
    if(!hMain||!hField)
    {
        Log.print_err("ERROR in lang_refresh(): hMain is %d, hField is %d\n",hMain,hField);
        return;
    }

    rtl=language[STR_RTL].val;
    if(rtl!=1)rtl=0;
    setMirroring(hField);
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

void MainWindow_t::theme_refresh()
{
    hFont->SetFont(D_STR(FONT_NAME),D_X(FONT_SIZE));
    int fz=D_X(POPUP_FONT_SIZE);
    if(fz<10)fz=10;
    Popup->hFontP->SetFont(D_STR(FONT_NAME),fz);
    Popup->hFontBold->SetFont(D_STR(FONT_NAME),fz,true);
    D(POPUP_WY)=fz*120/100*Settings.scale/256;

    hLang->SetFont(hFont);
    hTheme->SetFont(hFont);

    if(!hMain||!hField)
    {
        Log.print_err("ERROR in theme_refresh(): hMain is %d, hField is %d\n",hMain,hField);
        return;
    }

    if(Settings.autosized)
    {
        MoveWindow(hField,Xm(D_X(DRVLIST_OFSX),D_X(DRVLIST_WX)),Ym(D_X(DRVLIST_OFSY)),XM(D_X(DRVLIST_WX),D_X(DRVLIST_OFSX)),YM(D_X(DRVLIST_WY),D_X(DRVLIST_OFSY)),TRUE);
        wPanels->arrange();
        manager_g->setpos();
        MainWindow.redrawmainwnd();
        MainWindow.redrawfield();
        return;
    }

    // Resize window
    RECT rect;
    GetWindowRect(hMain,&rect);
    MoveWindow(hMain,rect.left,rect.top,D(MAINWND_WX),D(MAINWND_WY)+1,1);
    MoveWindow(hMain,rect.left,rect.top,D(MAINWND_WX),D(MAINWND_WY),1);
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

static BOOL CALLBACK DialogProc1(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
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

            data.pages[0]=CreateDialog(ghInst,MAKEINTRESOURCE(IDD_Page1),hwnd,(DLGPROC)DialogPage);
            data.pages[1]=CreateDialog(ghInst,MAKEINTRESOURCE(IDD_Page2),hwnd,(DLGPROC)DialogPage);
            data.pages[2]=CreateDialog(ghInst,MAKEINTRESOURCE(IDD_Page3),hwnd,(DLGPROC)DialogPage);
            data.pages[3]=CreateDialog(ghInst,MAKEINTRESOURCE(IDD_Page4),hwnd,(DLGPROC)DialogPage);

            data.tab=GetDlgItem(hwnd,IDC_TAB1);
            if(data.tab)
            {
                TCITEM tci;
                tci.mask = TCIF_TEXT;
                tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_VIEW_TAB));
                if(TabCtrl_InsertItem(data.tab, 0, &tci)==-1)Log.print_err("ERROR in winMain(): failed to insert page in tab control.\n");
                tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_UPDATES_TAB));
                if(TabCtrl_InsertItem(data.tab, 1, &tci)==-1)Log.print_err("ERROR in winMain(): failed to insert page in tab control.\n");
                tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_PATH_TAB));
                if(TabCtrl_InsertItem(data.tab, 2, &tci)==-1)Log.print_err("ERROR in winMain(): failed to insert page in tab control.\n");
                tci.pszText = const_cast<wchar_t *>(STR(STR_OPTION_ADVANCED_TAB));
                if(TabCtrl_InsertItem(data.tab, 3, &tci)==-1)Log.print_err("ERROR in winMain(): failed to insert page in tab control.\n");

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
                SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR2),STR(STR_OPTION_DIR_INDEXES));
                SetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR3),STR(STR_OPTION_DIR_INDEXESH));
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

                str.sprintf(L"%d",Updater->torrentport);
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

                if (rtl)
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
            Log.print_con("asd");
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
                    Updater->torrentport=_wtoi_my(num);
                    GetWindowText(GetDlgItem(data.pages[1],IDD_P2_CONE),num,32);
                    Updater->connections=_wtoi_my(num);
                    GetWindowText(GetDlgItem(data.pages[1],IDD_P2_DOWNE),num,32);
                    Updater->downlimit=_wtoi_my(num);
                    GetWindowText(GetDlgItem(data.pages[1],IDD_P2_UPE),num,32);
                    Updater->uplimit=_wtoi_my(num);
                    Updater->SetLimits();

                    if(!SendMessage(GetDlgItem(data.pages[1],IDD_P2_UPD),BM_GETCHECK,0,0))
                        Settings.flags|=FLAG_CHECKUPDATES;
                    else
                        Settings.flags&=~FLAG_CHECKUPDATES;

                    if(SendMessage(GetDlgItem(data.pages[1],IDONLYUPDATE),BM_GETCHECK,0,0))
                        Settings.flags|=FLAG_ONLYUPDATES;
                    else
                        Settings.flags&=~FLAG_ONLYUPDATES;

                    GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR1E),Settings.drp_dir,BUFLEN);
                    GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR2E),Settings.index_dir,BUFLEN);
                    GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR3E),Settings.output_dir,BUFLEN);
                    GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR4E),Settings.data_dir,BUFLEN);
                    GetWindowText(GetDlgItem(data.pages[2],IDD_P3_DIR5E),Settings.logO_dir,BUFLEN);

                    GetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD1E),Settings.finish,BUFLEN);
                    GetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD2E),Settings.finish_rb,BUFLEN);
                    GetWindowText(GetDlgItem(data.pages[3],IDD_P4_CMD3E),Settings.finish_upd,BUFLEN);

                    if(SendMessage(GetDlgItem(data.pages[3],IDD_P4_CONSL),BM_GETCHECK,0,0))
                    {
                        Settings.flags|=FLAG_SHOWCONSOLE;
                        Console->Show();
                    }
                    else
                    {
                        Settings.flags&=~FLAG_SHOWCONSOLE;
                        Console->Hide();
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

//=============================================================================
//
// AboutBoxProc()
//
static BOOL CALLBACK AboutBoxProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG: {
		SetWindowText(GetDlgItem(hwnd,IDC_AUTHORS),STR(STR_ABOUT_DEV_TITLE));
		//SetDlgItemText(hwnd, IDC_VERSION, TEXT(VERSION_FILEVERSION_LONG));
		//SetDlgItemText(hwnd, IDC_BUILD_INFO, wch);
		//SetDlgItemText(hwnd, IDC_COPYRIGHT, VERSION_LEGALCOPYRIGHT);
  //  SetDlgItemText(hwnd, IDC_WEBP_VERSION, VERSION_WEBP);
  //  SetDlgItemText(hwnd, IDC_TORR_VERSION, VERSION_LIBTORRENT);

		//HFONT hFontTitle = (HFONT)SendDlgItemMessage(hwnd, IDC_VERSION, WM_GETFONT, 0, 0);
		//if (hFontTitle == NULL) {
		//	hFontTitle = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		//}

		//LOGFONT lf;
		//GetObject(hFontTitle, sizeof(LOGFONT), &lf);
		//lf.lfWeight = FW_BOLD;
		//hFontTitle = CreateFontIndirect(&lf);
		//SendDlgItemMessage(hwnd, IDC_VERSION, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
		//SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)(hFontTitle));

		//if (GetDlgItem(hwnd, IDC_WEBPAGE_LINK) == NULL) {
		//	SetDlgItemText(hwnd, IDC_WEBPAGE_TEXT, VERSION_WEBPAGE_DISPLAY);
		//	ShowWindow(GetDlgItem(hwnd, IDC_WEBPAGE_TEXT), SW_SHOWNORMAL);
		//} else {
		//	wsprintf(wch, L"<A>%s</A>", VERSION_WEBPAGE_DISPLAY);
		//	SetDlgItemText(hwnd, IDC_WEBPAGE_LINK, wch);
		//}

		//if (GetDlgItem(hwnd, IDC_TELEGRAM_LINK) == NULL) {
		//	SetDlgItemText(hwnd, IDC_TELEGRAM_TEXT, VERSION_TELEGRAM_DISPLAY);
		//	ShowWindow(GetDlgItem(hwnd, IDC_TELEGRAM_TEXT), SW_SHOWNORMAL);
		//} else {
		//	wsprintf(wch, L"<A>%s</A>", VERSION_TELEGRAM_DISPLAY);
		//	SetDlgItemText(hwnd, IDC_TELEGRAM_LINK, wch);
		//}

		//CenterDlgInParent(hwnd);
	  }
	  return TRUE;

    case WM_SETCURSOR:
        // 2 hyperlinks
        if ((LOWORD(lParam)==HTCLIENT) &&
            ((GetDlgCtrlID((HWND)wParam) == IDC_WEBPAGE_LINK)||
             (GetDlgCtrlID((HWND)wParam) == IDC_TELEGRAM_LINK)))
        {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            SetWindowLongPtr(hwnd, DWLP_MSGRESULT, TRUE);
            return true;
        }
        break;

    case WM_COMMAND:
		switch (LOWORD(wParam)) {
            case IDOK:
            case IDCANCEL:
			EndDialog(hwnd, IDOK);
            case IDC_WEBPAGE_LINK:
                System.run_command(L"open", VERSION_WEBPAGE_DISPLAY,SW_SHOWNORMAL,0);
            case IDC_PATREON_LINK:
                System.run_command(L"open",WEB_PATREONPAGE,SW_SHOWNORMAL,0);
			break;
		}
		return TRUE;
    case WM_CTLCOLORSTATIC:
    {
        // modify the fonts for colours and bold and size etc
        //HWND Ctl1=GetDlgItem(hwnd,IDD_ABOUT_T1);
        HWND Ctl3=GetDlgItem(hwnd, IDC_AUTHORS);
        //HWND Ctl4=GetDlgItem(hwnd,IDD_MAINTAINERS);
       /* HWND Ctl6=GetDlgItem(hwnd,IDD_ABOUT_T6);
        HWND Ctl8=GetDlgItem(hwnd,IDD_ABOUT_T8);
        HWND Ctl9=GetDlgItem(hwnd,IDD_ABOUT_T9);*/
        HDC hdcStatic=(HDC)wParam;

        //if((HWND)lParam==Ctl1)
        //{
        //    HFONT hTitleFont = CreateFont(28,12,0,0,620,
        //                                 FALSE,FALSE,FALSE,
        //                                 ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
        //                                 ANTIALIASED_QUALITY,DEFAULT_PITCH,
        //                                 L"Tahoma");
        //    SelectObject(hdcStatic,hTitleFont);
        //    SetTextColor(hdcStatic, RGB(248,171,3));
        //}
        if((HWND)lParam==Ctl3)//||((HWND)lParam==Ctl4)||(HWND)lParam==Ctl6)
        {
            HFONT hFont = CreateFont(9,0,0,0,700,
                                         FALSE,FALSE,FALSE,
                                         ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                         ANTIALIASED_QUALITY,DEFAULT_PITCH,
                                         L"MS Sans Serif");
            SelectObject(hdcStatic,hFont);
        }
      /*  if(((HWND)lParam==Ctl8)||(HWND)lParam==Ctl9)
        {
            HFONT hFont = CreateFont(10,0,0,0,550,
                                         FALSE,FALSE,FALSE,
                                         ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                         ANTIALIASED_QUALITY,DEFAULT_PITCH,
                                         L"MS Sans Serif");
            SelectObject(hdcStatic,hFont);
            SetTextColor(hdcStatic, RGB(0,0,255));
        }*/
        SetBkMode(hdcStatic,TRANSPARENT);
        return (INT_PTR)g_hbrDlgBackground;
    }
    case WM_CTLCOLORDLG:
        return (INT_PTR)g_hbrDlgBackground;

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
    wchar_t dir[BUFLEN];
    std::wstring path=System.AppPathW();
    wcscpy(dir,path.c_str());

    if(System.ChooseDir(dir,STR(STR_EXTRACTFOLDER)))
    {
        int argc;
        wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        WStringShort buf;
        buf.sprintf(L"%s\\drv.exe",dir);
        if(!CopyFile(argv[0],buf.Get(),0))
            Log.print_err("ERROR in extractto(): failed CopyFile(%S,%S)\n",argv[0],buf.Get());
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
        invalidate(INVALIDATE_INDEXES|INVALIDATE_MANAGER);
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
    if(!hField)
    {
        Log.print_err("ERROR in setscrollrange(): hField is 0\n");
        return;
    }

    RECT rect;
    GetClientRect(hField,&rect);

    SCROLLINFO si;
    si.cbSize=sizeof(si);
    si.fMask =SIF_RANGE|SIF_PAGE;
    si.nMin  =0;
    si.nMax  =y;
    si.nPage =rect.bottom;
    scrollvisible=rect.bottom>y;
    SetScrollInfo(hField,SB_VERT,&si,TRUE);
}

int MainWindow_t::getscrollpos()
{
    if(!hField)
    {
        Log.print_err("ERROR in getscrollpos(): hField is 0\n");
        return 0;
    }

    SCROLLINFO si;
    si.cbSize=sizeof(si);
    si.fMask=SIF_POS;
    si.nPos=0;
    GetScrollInfo(hField,SB_VERT,&si);
    return si.nPos;
}

void MainWindow_t::setscrollpos(int pos)
{
    if(!hField)
    {
        Log.print_err("ERROR in setscrollpos(): hField is 0\n");
        return;
    }

    SCROLLINFO si;
    si.cbSize=sizeof(si);
    si.fMask=SIF_POS;
    si.nPos=pos;
    SetScrollInfo(hField,SB_VERT,&si,TRUE);
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
    return CreateWindow(type,name,WS_CHILD|WS_VISIBLE|f,0,0,0,0,hwnd,(HMENU)(id),ghInst,NULL);
}

void GetRelativeCtrlRect(HWND hWnd,RECT *rc)
{
    GetWindowRect(hWnd,rc);
    ScreenToClient(GetParent(hWnd),(LPPOINT)&((LPPOINT)rc)[0]);
    ScreenToClient(GetParent(hWnd),(LPPOINT)&((LPPOINT)rc)[1]);
    //MapWindowPoints(nullptr,hWnd,(LPPOINT)&rc,2);
    rc->right-=rc->left;
    rc->bottom-=rc->top;
}

void setMirroring(HWND hwnd)
{
    LONG_PTR v=GetWindowLongPtr(hwnd,GWL_EXSTYLE);
    if(rtl)v|=WS_EX_LAYOUTRTL;else v&=~WS_EX_LAYOUTRTL;
    SetWindowLongPtr(hwnd,GWL_EXSTYLE,v);
}

void setMirroringEdit(HWND hwnd)
{
    setMirroring(hwnd);

    // reposition edit controls for right-to-left
    if(rtl)
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

void checktimer(const wchar_t *str,long long t,int uMsg)
{
    if(System.GetTickCountWr()-t>20&&Log.isAllowed(LOG_VERBOSE_LAGCOUNTER))
        Log.print_con("GUI lag in %S[%X]: %ld\n",str,uMsg,System.GetTickCountWr()-t);
}

void MainWindow_t::redrawfield()
{
    if(Settings.flags&FLAG_NOGUI)return;
    if(!hField)
    {
        Log.print_err("ERROR in redrawfield(): hField is 0\n");
        return;
    }
    InvalidateRect(hField,nullptr,0);
}

void MainWindow_t::redrawmainwnd()
{
    if(Settings.flags&FLAG_NOGUI)return;
    if(!hMain)
    {
        Log.print_err("ERROR in redrawmainwnd(): hMain is 0\n");
        return;
    }
    InvalidateRect(hMain,nullptr,0);
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
    wchar_t spec1[BUFLEN];
    wchar_t spec2[BUFLEN];
    wcscpy(spec1,Settings.drp_dir);wcscat(spec1,L"\\*.*");
    wcscpy(spec2,Settings.index_dir);wcscat(spec2,L"\\*.*");
    int argc;
    CommandLineToArgvW(GetCommandLineW(),&argc);

    // torrent results
    int NewVersion=TorrentResults>>8;
    int LatestExeVersion=System.FindLatestExeVersion();
    int DriverPacksAvailable=TorrentResults&0xFF;

    // user selected "continue seeding"
    if(Settings.flags&FLAG_KEEPSEEDING)
    {
        if(Updater)
        {
            Settings.flags&=~FLAG_KEEPSEEDING;
            Updater->StartSeedingDrivers();
        }
    }

	else if(TorrentSelectionMode==TSM_AUTO)
	{
        // just finished downloading the first torrent after startup
        // if there are no drivers and no indexes
        // and no command line then show the welcome screen
        if(!System.FileExists2(spec1)&&!System.FileExists2(spec2)&&(argc<2))
        {
            TorrentSelectionMode=TSM_NONE;
            DialogBox(ghInst,MAKEINTRESOURCE(IDD_WELCOME), MainWindow.hMain,(DLGPROC)WelcomeProcedure);
        }
        // otherwise if there are updates on the current torrent then stop switching
        else if((NewVersion>LatestExeVersion)||(DriverPacksAvailable>0))
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

//{ GUI
/*LRESULT CALLBACK MainWindow_t::WndProcCommon(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    return MainWindow.WndProcCommon2(hwnd,uMsg,wParam,lParam);
}*/

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
                    MoveWindow(hMain,rect.left+(x-mousex)*(rtl?-1:1),rect.top+y-mousey,
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
    return MainWindow.WndProcMain(hwnd,uMsg,wParam,lParam);
}

LRESULT MainWindow_t::WndProcMain(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WINDOWPLACEMENT wndp;
    wndp.length=sizeof(WINDOWPLACEMENT);
    RECT rect;
    short x,y;

    int i;
	int f;
    int wp;
    //long long timer=System.GetTickCountWr();

    x=LOWORD(lParam);
    y=HIWORD(lParam);

    if(WndProcCommon(hwnd,uMsg,wParam,lParam))
    switch(uMsg)
    {
        case WM_CREATE:
            // Canvas
            canvasMain=Canvas::Create();
            hMain=hwnd;

            // Field
            hField=CreateWindowMF(classField,nullptr,hwnd,0,WS_VSCROLL);

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
            DragAcceptFiles(hwnd,1);

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
            vTheme->EnumFiles(hTheme,L"themes");
            f=hTheme->FindItem(Settings.curtheme);
            if(f==CB_ERR)f=vTheme->AutoPick();
			vTheme->SwitchData((int)f);
            if(Settings.wndwx)D(MAINWND_WX)=Settings.wndwx;
            if(Settings.wndwy)D(MAINWND_WY)=Settings.wndwy;
            hTheme->SetCurSel(f);
            theme_refresh();

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
                        ModifyMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_SEED,0,const_cast<wchar_t *>STR(STR_SYST_STOP_SEED));
                        break;
                    case 0:
                        ModifyMenuItem(pSysMenu,MIIM_STRING|MIIM_ID,IDM_SEED,0,const_cast<wchar_t *>STR(STR_SYST_START_SEED));
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
                Log.print_con("{Sync");
                if(CRITICAL_SECTION_ACTIVE)EnterCriticalSection(&sync);
                Log.print_con("...\n");
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
        case WM_INDEXESSAVED:
            {
                if(Settings.flags&FLAG_KEEPSEEDING)
                {
                    ResetUpdater(Updater_t::activetorrent);
                }
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
                        invalidate(INVALIDATE_INDEXES|INVALIDATE_MANAGER);
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
                  if(rect.right<wpos->cx||rect.bottom<wpos->cy)
                {
                    Settings.autosized=true;

                    wpos->x=rect.left;
                    wpos->y=rect.top;
                    wpos->cx=rect.right-wpos->x;
                    wpos->cy=rect.bottom-wpos->y;
                    //Log.print_con("%d,%d,%d,%d\n",rect.left,rect.top,rect.right,rect.bottom);
                    Settings.scale=750*256/wpos->cy;
                    //Log.print_con("(%d,%d,%d)\n",wpos->cx,wpos->cy,Settings.scale);
                    MainWindow.theme_refresh();
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
            if(ctrl_down&&wParam==L'Z'){Log.print_con("\n*************\n");}
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
                DialogBox(ghInst, MAKEINTRESOURCE(IDD_ABOUT), MainWindow.hMain, (DLGPROC)AboutBoxProc);
            if(wParam==VK_F5&&ctrl_down)
                invalidate(INVALIDATE_DEVICES);else
            if(wParam==VK_F5)
                invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_INDEXES|INVALIDATE_MANAGER);
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
            Log.print_con("WM_DEVICECHANGE(%x,%x)\n",wParam,lParam);
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
            MoveWindow(hField,Xm(D_X(DRVLIST_OFSX),D_X(DRVLIST_WX)),Ym(D_X(DRVLIST_OFSY)),XM(D_X(DRVLIST_WX),D_X(DRVLIST_OFSX)),YM(D_X(DRVLIST_WY),D_X(DRVLIST_OFSY)),TRUE);

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
                        DialogBox( ghInst,MAKEINTRESOURCE(IDD_ABOUT), MainWindow.hMain,(DLGPROC)AboutBoxProc);
                        return 0;
                    }
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
                            invalidate(INVALIDATE_INDEXES|INVALIDATE_MANAGER);
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
                        DialogBox(ghInst,MAKEINTRESOURCE(IDD_WELCOME), MainWindow.hMain,(DLGPROC)WelcomeProcedure);
                        return 0;
                    }
                    case IDM_LICENSE:
                    {
                        DialogBox(ghInst,MAKEINTRESOURCE(IDD_LICENSE),MainWindow.hMain,(DLGPROC)LicenseProcedure);
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
                SendMessage(hField,WM_VSCROLL,MAKELONG(i>0?SB_LINEUP:SB_LINEDOWN,0),0);
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
                Settings.virtual_os_version=wp;
                invalidate(INVALIDATE_SYSINFO|INVALIDATE_MANAGER);
            }
            if(wp>=ID_HWID_CLIP&&wp<=ID_HWID_WEB+100)
            {
                int id=wp%100;
                if(wp>=ID_HWID_WEB)
                {
                    wchar_t buf[BUFLEN];
                    wchar_t buf2[BUFLEN];
                    const wchar_t *str=manager_g->getHWIDby(id);
                    wsprintf(buf,L"http://catalog.update.microsoft.com/v7/site/search.aspx?q=%s",str);
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
                    theme_refresh();
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
    invalidate(INVALIDATE_DEVICES|INVALIDATE_SYSINFO|INVALIDATE_INDEXES|INVALIDATE_MANAGER);
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
    DialogBox(ghInst,MAKEINTRESOURCE(IDD_DIALOG3),MainWindow.hMain,(DLGPROC)DialogProc1);
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
                invalidate(INVALIDATE_INDEXES|INVALIDATE_MANAGER);
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
            if(Popup->floating_itembar==SLOT_PATREON)
            {
                if(StrStrIW(STR(STR_LANG_ID),L"Russian"))
                    System.run_command(L"open",L"http://vk.com/snappydriverinstaller?w=page-71369181_50543112",SW_SHOWNORMAL,0);
                else
                    System.run_command(L"open",L"https://www.patreon.com/SamLab",SW_SHOWNORMAL,0);
                break;
            }

            if(Popup->floating_itembar==SLOT_TRANSLATION)
            {
                System.run_command(L"open",L"https://www.transifex.com/snappy-driver-installer/snappy-driver-installer",SW_SHOWNORMAL,0);
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
                    Popup->drawpopup(itembar_i,0,type,x,y,hField);
                else if(itembar_i==SLOT_VIRUS_AUTORUN)
                    Popup->drawpopup(itembar_i,STR_VIRUS_AUTORUN_H,FLOATING_TOOLTIP,x,y,hField);
                else if(itembar_i==SLOT_VIRUS_RECYCLER)
                    Popup->drawpopup(itembar_i,STR_VIRUS_RECYCLER_H,FLOATING_TOOLTIP,x,y,hField);
                else if(itembar_i==SLOT_VIRUS_HIDDEN)
                    Popup->drawpopup(itembar_i,STR_VIRUS_HIDDEN_H,FLOATING_TOOLTIP,x,y,hField);
                else if(itembar_i==SLOT_EXTRACTING&&installmode)
                    Popup->drawpopup(itembar_i,(instflag&INSTALLDRIVERS)?STR_HINT_STOPINST:STR_HINT_STOPEXTR,FLOATING_TOOLTIP,x,y,hField);
                else if(itembar_i==SLOT_RESTORE_POINT)
                    Popup->drawpopup(itembar_i,STR_RESTOREPOINT_H,FLOATING_TOOLTIP,x,y,hField);
                else if(itembar_i==SLOT_DOWNLOAD)
                    Popup->drawpopup(itembar_i,0,FLOATING_DOWNLOAD,x,y,hField);
                else if(itembar_i==SLOT_PATREON)
                    Popup->drawpopup(itembar_i,STR_PATREON_H,FLOATING_TOOLTIP,x,y,hField);
                else if(i==0&&itembar_i>=RES_SLOTS)
                    Popup->drawpopup(itembar_i,STR_HINT_DRIVER,FLOATING_TOOLTIP,x,y,hField);
                else
                    Popup->drawpopup(0,0,FLOATING_NONE,0,0,hField);

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

LRESULT CALLBACK PopupProcedure(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    return Popup->PopupProcedure2(hwnd,message,wParam,lParam);
}

Popup_t::Popup_t():
    hFontP(wFont::Create()),
    hFontBold(wFont::Create())
{
}
void Popup_t::init()
{
    hPopup=CreateWindowEx(WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TRANSPARENT,
        MainWindow.classPopup,L"",WS_POPUP,
        0,0,0,0,MainWindow.hMain,(HMENU)nullptr,ghInst,nullptr);
}

void Popup_t::AddShift(int i)
{
    if(i==0xffff)
        horiz_sh=0;
    else
        horiz_sh-=i/5;
    if(horiz_sh>0)horiz_sh=0;
    InvalidateRect(hPopup,nullptr,0);
}

Popup_t::~Popup_t()
{
    delete hFontP;
    delete hFontBold;
}

LRESULT Popup_t::PopupProcedure2(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    RECT rect;
    WINDOWPOS *wp;

    switch(message)
    {
        case WM_WINDOWPOSCHANGING:
            if(floating_type!=FLOATING_TOOLTIP)break;

            wp=(WINDOWPOS*)lParam;
            GetClientRect(hwnd,&rect);
            rect.right=D_X(POPUP_WX);
            rect.bottom=floating_y;

            canvasPopup->SetFont(Popup->hFontP);
            if(!floating_str_id)break;
            canvasPopup->CalcBoundingBox(STR(floating_str_id),reinterpret_cast<RECT_WR *>(&rect));

            AdjustWindowRectEx(&rect,WS_POPUPWINDOW|WS_VISIBLE,0,0);
            popup_resize(rect.right-rect.left+D_X(POPUP_OFSX)*2,rect.bottom-rect.top+D_X(POPUP_OFSY)*2);
            wp->cx=rect.right+D_X(POPUP_OFSX)*2;
            wp->cy=rect.bottom+D_X(POPUP_OFSY)*2;
            break;

        case WM_CREATE:
            canvasPopup=Canvas::Create();
            break;

        case WM_DESTROY:
            delete canvasPopup;
            break;

        case WM_PAINT:
            GetClientRect(hwnd,&rect);
            canvasPopup->begin(&hwnd,rect.right,rect.bottom,/*floating_type!=FLOATING_CMPDRIVER&&floating_type!=FLOATING_DRIVERLST*/1);

            canvasPopup->DrawWidget(0,0,rect.right,rect.bottom,BOX_POPUP);
            switch(floating_type)
            {
                case FLOATING_SYSINFO:
                    canvasPopup->SetFont(Popup->hFontP);
                    manager_g->getState()->popup_sysinfo(*canvasPopup);
                    break;

                case FLOATING_TOOLTIP:
                    rect.left+=D_X(POPUP_OFSX);
                    rect.top+=D_X(POPUP_OFSY);
                    rect.right-=D_X(POPUP_OFSX);
                    rect.bottom-=D_X(POPUP_OFSY);
                    canvasPopup->SetFont(Popup->hFontP);
                    canvasPopup->SetTextColor(D_C(POPUP_TEXT_COLOR));
                    if(floating_str_id)canvasPopup->DrawTextRect(STR(floating_str_id),reinterpret_cast<RECT_WR *>(&rect));
                    break;

                case FLOATING_CMPDRIVER:
                    canvasPopup->SetFont(Popup->hFontP);
                    manager_g->popup_drivercmp(manager_g,*canvasPopup,rect.right,rect.bottom,floating_itembar);
                    break;

                case FLOATING_DRIVERLST:
                    canvasPopup->SetFont(Popup->hFontP);
                    manager_g->popup_driverlist(*canvasPopup,rect.right,rect.bottom,floating_itembar);
                    break;

                case FLOATING_DOWNLOAD:
                    canvasPopup->SetFont(Popup->hFontP);
                    #ifdef USE_TORRENT
                    Updater->ShowPopup(*canvasPopup);
                    #endif
                    break;

                default:
                    break;
            }

            canvasPopup->end();
            break;

        case WM_ERASEBKGND:
            return 1;

        default:
            return DefWindowProc(hwnd,message,wParam,lParam);
    }
    return 0;
}

BOOL CALLBACK LicenseProcedure(HWND hwnd,UINT Message,WPARAM wParam,LPARAM lParam)
{
    WINDOWPOS *wpos;
    HWND hEditBox;
    RECT rect;
    LPCSTR s;
    size_t sz;

    switch(Message)
    {
        case WM_INITDIALOG:
            get_resource(IDR_LICENSE,(void **)&s,&sz);
            hEditBox=GetDlgItem(hwnd,IDC_EDIT1);
            SetWindowTextA(hEditBox,s);
            SendMessage(hEditBox,EM_SETREADONLY,1,0);
            // only show decline button on startup
            if(GetParent(hwnd))
            {
                ShowWindow(GetDlgItem(hwnd,IDCANCEL),SW_HIDE);
                SetFocus(GetDlgItem(hwnd,IDOK));
            }
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    Settings.license=2;
                    EndDialog(hwnd,IDOK);
                    return TRUE;

                case IDCANCEL:
                    if(!GetParent(hwnd))Settings.license=0;
                    EndDialog(hwnd,IDCANCEL);
                    return TRUE;

                default:
                    break;
            }
            break;

        case WM_WINDOWPOSCHANGED:
            wpos=(WINDOWPOS*)lParam;
            {
                int r=SystemParametersInfo(SPI_GETWORKAREA,0,&rect,0);
                if(r&&wpos->cy-rect.bottom>0)
                {
                    int sz1=rect.bottom-20-wpos->cy;
                    wpos->y=10;
                    wpos->cy=rect.bottom-20;
                    MoveWindow(hwnd,wpos->x,wpos->y,wpos->cx,wpos->cy,1);

                    GetRelativeCtrlRect(GetDlgItem(hwnd,IDC_EDIT1),&rect);
                    rect.bottom+=sz1;
                    MoveWindow(GetDlgItem(hwnd,IDC_EDIT1),rect.left,rect.top,rect.right,rect.bottom,1);

                    GetRelativeCtrlRect(GetDlgItem(hwnd,IDOK),&rect);
                    rect.top+=sz1;
                    MoveWindow(GetDlgItem(hwnd,IDOK),rect.left,rect.top,rect.right,rect.bottom,1);

                    GetRelativeCtrlRect(GetDlgItem(hwnd,IDCANCEL),&rect);
                    rect.top+=sz1;
                    MoveWindow(GetDlgItem(hwnd,IDCANCEL),rect.left,rect.top,rect.right,rect.bottom,1);
                }
            }
            return TRUE;

        case WM_CTLCOLORSTATIC:
            hEditBox=GetDlgItem(hwnd,IDC_EDIT1);
            if((HWND)lParam==hEditBox)
            {
                HDC hdcStatic=(HDC)wParam;
                SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
                SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
                return (LRESULT)GetStockObject(HOLLOW_BRUSH);
            }
            else
            {
                HDC hdcStatic=(HDC)wParam;
                SetBkMode(hdcStatic,TRANSPARENT);
                return (INT_PTR)g_hbrDlgBackground;
            }

        case WM_CTLCOLORDLG:
            return (INT_PTR)g_hbrDlgBackground;

        default:
            break;
    }
    return FALSE;
}

BOOL CALLBACK WelcomeProcedure(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    HWND Ctl1;
    HWND Ctl2;
    HWND Ctl3;
    HWND Ctl4;

    switch (msg)
    {
    case WM_INITDIALOG:
        // languages
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_TITLE),STR(STR_WELCOME_TITLE));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_SUBTITLE),STR(STR_WELCOME_SUBTITLE));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_INTRO),STR(STR_WELCOME_INTRO));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_INTRO2),STR(STR_WELCOME_INTRO2));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_BUTTON1),STR(STR_WELCOME_BUTTON1));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_BUTTON1_DESC),STR(STR_WELCOME_BUTTON1_DESC));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_BUTTON2),STR(STR_WELCOME_BUTTON2));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_BUTTON2_DESC),STR(STR_WELCOME_BUTTON2_DESC));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_BUTTON3),STR(STR_WELCOME_BUTTON3));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_BUTTON3_DESC),STR(STR_WELCOME_BUTTON3_DESC));
        SetWindowText(GetDlgItem(hwnd,IDD_WELC_CLOSE),STR(STR_WELCOME_CLOSE));
        // set focus to first button
        SetFocus(GetDlgItem(hwnd,IDD_WELC_BUTTON1));
        return TRUE;

    case WM_SETCURSOR:
        // 2 hyperlinks
        if ((LOWORD(lParam)==HTCLIENT) &&
            ((GetDlgCtrlID((HWND)wParam) == IDD_WELC_LINK1)||
             (GetDlgCtrlID((HWND)wParam) == IDD_WELC_LINK2)))
        {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            SetWindowLongPtr(hwnd, DWLP_MSGRESULT, TRUE);
            return true;
        }
        break;

    case WM_COMMAND:
        switch(wParam)
        {
            case IDD_WELC_CLOSE:
                EndDialog(hwnd,wParam);
                return TRUE;
            case IDCANCEL:
                EndDialog(hwnd,wParam);
                break;
            case IDD_WELC_BUTTON1:
                // download everything
                EndDialog(hwnd,wParam);
                Settings.flags&=~FLAG_AUTOUPDATE;
                Updater->DownloadAll();
                return TRUE;
            case IDD_WELC_BUTTON2:
                // download network only
                EndDialog(hwnd,wParam);
                Settings.flags&=~FLAG_AUTOUPDATE;
                Updater->DownloadNetwork();
                return TRUE;
            case IDD_WELC_BUTTON3:
                // download indexes only
                EndDialog(hwnd,wParam);
                Settings.flags&=~FLAG_AUTOUPDATE;
                Updater->DownloadIndexes();
                return TRUE;
            case IDD_WELC_LINK1:
                System.run_command(L"open", VERSION_WEBPAGE_DISPLAY,SW_SHOWNORMAL,0);
                break;
            case IDD_WELC_LINK2:
                System.run_command(L"open",WEB_PATREONPAGE,SW_SHOWNORMAL,0);
                break;
            default:
                break;
        }
        break;

    case WM_CTLCOLORSTATIC:
        {
            // modify the fonts for colours and bold and size etc
            Ctl1=GetDlgItem(hwnd,IDD_WELC_TITLE);
            Ctl2=GetDlgItem(hwnd,IDD_WELC_LINK1);
            Ctl3=GetDlgItem(hwnd,IDD_WELC_LINK2);
            Ctl4=GetDlgItem(hwnd,IDD_WELC_SUBTITLE);
            HDC hdcStatic=(HDC)wParam;

            if((HWND)lParam==Ctl1)
            {
                HFONT hTitleFont = CreateFont(28,12,0,0,620,
                                             FALSE,FALSE,FALSE,
                                             ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                             ANTIALIASED_QUALITY,DEFAULT_PITCH,
                                             L"Tahoma");
                SetTextColor(hdcStatic, RGB(248,171,3));
                SelectObject(hdcStatic,hTitleFont);
            }
            else if((HWND)lParam==Ctl4)
            {
                HFONT hFont = CreateFont(9,0,0,0,700,
                                             FALSE,FALSE,FALSE,
                                             ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                             ANTIALIASED_QUALITY,DEFAULT_PITCH,
                                             L"MS Sans Serif");
                SelectObject(hdcStatic,hFont);
            }
            else if(((HWND)lParam==Ctl2)||(HWND)lParam==Ctl3)
            {
                HFONT hFont = CreateFont(10,0,0,0,550,
                                             FALSE,FALSE,FALSE,
                                             ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                             ANTIALIASED_QUALITY,DEFAULT_PITCH,
                                             L"MS Sans Serif");
                SetTextColor(hdcStatic, RGB(0,0,255));
                SelectObject(hdcStatic,hFont);
            }

            SetBkMode(hdcStatic,TRANSPARENT);
            return (INT_PTR)g_hbrDlgBackground;
        }

    case WM_CTLCOLORDLG:
        return (INT_PTR)g_hbrDlgBackground;
    default:
        break;
    }
    return FALSE;
}


//}
