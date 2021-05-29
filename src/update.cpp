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

#ifdef USE_TORRENT
#include "common.h"
#include "logging.h"
#include "settings.h"
#include "system.h"
#include "matcher.h"
#include "update.h"
#include "manager.h"
#include "theme.h"
#include "gui.h"
#include "draw.h"
#include "install.h"
#include <direct.h>     // _wgetcwd

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244)
#pragma warning(disable:4245)
#pragma warning(disable:4267)
#pragma warning(disable:4512)
#endif

#include "libtorrent/config.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/session_settings.hpp"
#include "libtorrent/file.hpp"
#include "libtorrent/torrent_info.hpp"

#ifdef _MSC_VER
#pragma comment(lib, "IPHLPAPI.lib") 
#pragma warning(pop)
#endif

#include <windows.h>
#include <shobjidl.h>
#ifdef _MSC_VER
#include <process.h>
#endif

// Depend on Win32API
#include "main.h"

#define SMOOTHING_FACTOR 0.005

// TorrentStatus
class TorrentStatus_t
{
    long long downloaded,downloadsize;
    long long uploaded;
    __int64 elapsed,remaining;

    int status_strid;
    wchar_t error[BUFLEN];
    int uploadspeed,downloadspeed;
    int seedstotal,seedsconnected;
    int peerstotal,peersconnected;
    int wasted,wastedhashfailes;

    bool sessionpaused,torrentpaused;

    friend class UpdaterImp;
};

// UpdateDialog
class UpdateDialog_t
{
    static const int cxn[];
    static WNDPROC wpOrigButtonProc;
    static int bMouseInWindow;
    static HWND hUpdate;
    int totalsize;
    int totalavail;

private:
    int  getnewver(const char *ptr);
    int  getcurver(const char *ptr);
    void calctotalsize();
    void calcavailablespace();
    void updateTexts();
    void updateButtons();

    void setCheckboxes();
    void setPriorities();
    static LRESULT CALLBACK NewButtonProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
    static BOOL CALLBACK UpdateProcedure(HWND hwnd,UINT Message,WPARAM wParam,LPARAM lParam);

public:
    int  populate(int flags,bool clearlist=false);
    void setFilePriority(const wchar_t *name,int pri);
    void openDialog();
    long LocalRevision;
    long TorrentRevision;
};

// Updater
class UpdaterImp:public Updater_t
{
    static Event *downloadmangar_event;
    static ThreadAbs *thandle_download;

    static int downloadmangar_exitflag;
    static bool finishedupdating;
    static bool finisheddownloading;
    static bool movingfiles;
    static bool closingsession;
    static bool SeedMode;

    int averageSpeed=0;
    long long torrenttime=0;

private:
    std::wstring const* active_torrent_url;
    std::wstring const* active_torrent_save_path;
    int downloadTorrent();
    void updateTorrentStatus();
    void removeOldDriverpacks(const wchar_t *ptr);
    void moveNewFiles();
    static unsigned int __stdcall thread_download(void *arg);

public:
    UpdaterImp();
    ~UpdaterImp();

    void ShowProgress(wchar_t *buf);
    void ShowPopup(Canvas &canvas);

    void checkUpdates();
    void resumeDownloading();
    void pause();

    bool isTorrentReady();
    bool isPaused();
    bool isUpdateCompleted();
    bool isSeedingDrivers();

    int  Populate(int flags);
    void SetFilePriority(const wchar_t *name,int pri);
    void SetLimits();
    void OpenDialog();

    void DownloadAll();
    void DownloadNetwork();
    void DownloadIndexes();
    void StartSeedingDrivers();
    void StopSeedingDrivers();

    int scriptInitUpdates(int torrentport);
    int scriptDownloadApp();
    int scriptDownloadIndexes();
    int scriptDownloadDrivers(std::wstring mode);
    int scriptDownloadEverything();
    int scriptDoDownload();
    int scriptInstall();
};
Updater_t *CreateUpdater(){return new UpdaterImp;}

//{ Global variables
lt::session *hSession=nullptr;
lt::torrent_handle hTorrent;
lt::settings_pack settings;

UpdateDialog_t UpdateDialog;
Updater_t *Updater;
TorrentStatus_t TorrentStatus;
int ListViewSortColumn=666;
bool ListViewSortAsc=TRUE;

enum DOWNLOAD_STATUS
{
    DOWNLOAD_STATUS_WAITING,
    DOWNLOAD_STATUS_DOWLOADING_TORRENT,
    DOWNLOAD_STATUS_TORRENT_GOT,
    DOWNLOAD_STATUS_DOWLOADING_DATA,
    DOWNLOAD_STATUS_FINISHED_DOWNLOADING,
    DOWNLOAD_STATUS_PAUSING,
    DOWNLOAD_STATUS_STOPPING,
};

// UpdateDialog (static)
const int UpdateDialog_t::cxn[]={199,60,44,70,70,119};
HWND UpdateDialog_t::hUpdate=nullptr;
WNDPROC UpdateDialog_t::wpOrigButtonProc;
int UpdateDialog_t::bMouseInWindow=0;

// Updater (static)
const std::wstring Updater_t::torrent_url =L"http://www.driveroff.net/SDI_Update.torrent";
const std::wstring Updater_t::torrent2_url =L"http://www.driveroff.net/Drivers.torrent";
const std::wstring Updater_t::torrent_save_path=L"update";
const std::wstring Updater_t::torrent2_save_path=L"update\\SDI_RUS";
int Updater_t::activetorrent=1;
int Updater_t::torrentport=50171;
int Updater_t::downlimit=0;
int Updater_t::uplimit=0;
int Updater_t::connections=0;
int UpdaterImp::downloadmangar_exitflag;
bool UpdaterImp::finishedupdating;
bool UpdaterImp::finisheddownloading;
bool UpdaterImp::movingfiles;
bool UpdaterImp::closingsession;
bool UpdaterImp::SeedMode;
Event *UpdaterImp::downloadmangar_event=nullptr;
ThreadAbs *UpdaterImp::thandle_download=nullptr;
//}

//{ ListView
class ListView_t
{
public:
    HWND hListg;

    void init(HWND hwnd)
    {
        hListg=GetDlgItem(hwnd,IDLIST);
        SendMessage(hListg,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT);
    }
    void close()
    {
        // free ItemData
        LVITEM item;
        type_item *ItemData;
        for(int i=0;i<GetItemCount();i++)
        {
            item.iItem=i;
            item.mask=LVIF_PARAM;
            GetItem(&item);
            ItemData=(type_item*)item.lParam;
            delete ItemData;
        }
        hListg=nullptr;
    }
    bool IsVisible(){ return hListg!=nullptr; }
    void DisableRedraw(bool clearlist)
    {
        if(hListg)
        {
            SendMessage(hListg,WM_SETREDRAW,0,0);
            if(clearlist)SendMessage(hListg,LVM_DELETEALLITEMS,0,0L);
        }
    }
    static LPARAM CALLBACK CompareFunc(LPARAM lParam1,LPARAM lParam2,LPARAM lParamSort)
    {
        // the first two parameters are the ItemData structures to be compared
        // use the third parameter to control which column is used and
        // whether it's ascending or descending
        // -1 -2 -3 sort descending,  1 2 3 sort ascending

        bool isAsc = (lParamSort > 0);
        int column = abs(lParamSort)-1;

        type_item *ItemData1=(type_item*)lParam1;
        type_item *ItemData2=(type_item*)lParam2;
        int nRet=0;

        // default
        if(column==666)
        {
            if(ItemData1->DefaultSort>ItemData2->DefaultSort)nRet=1;
            else if(ItemData1->DefaultSort<ItemData2->DefaultSort)nRet=-1;
        }
        // item name
        else if(column==0)
            nRet=wcscmp(ItemData1->ItemName,ItemData2->ItemName);
        // size
        else if(column==1)
        {
            if(ItemData1->SizeMB>ItemData2->SizeMB)nRet=1;
            else if(ItemData2->SizeMB>ItemData1->SizeMB)nRet=-1;
        }
        // percent
        else if(column==2)
        {
            if(ItemData1->Percent>ItemData2->Percent)nRet=1;
            else if(ItemData2->Percent>ItemData1->Percent)nRet=-1;
        }
        // new ver
        else if(column==3)
        {
            if(ItemData1->VersionNew>ItemData2->VersionNew)nRet=1;
            else if(ItemData2->VersionNew>ItemData1->VersionNew)nRet=-1;
        }
        // current ver
        else if(column==4)
        {
            if(ItemData1->VersionCurrent>ItemData2->VersionCurrent)nRet=1;
            else if(ItemData2->VersionCurrent>ItemData1->VersionCurrent)nRet=-1;
        }
        // for this pc
        if(column==5)
        {
            if(ItemData1->ForThisPC>ItemData2->ForThisPC)nRet=1;
            else if(ItemData1->ForThisPC<ItemData2->ForThisPC)nRet=-1;
        }
        if(!isAsc)nRet=nRet*-1;
        return nRet;
    }
    void EnableRedraw()
    {
        if(hListg)
        {
            SendMessage(hListg,LVM_SORTITEMS,667,(LPARAM)CompareFunc);
            SendMessage(hListg,WM_SETREDRAW,1,0);
        }
    }

    int GetItemCount()
    {
        return ListView_GetItemCount(hListg);
    }
    int GetCheckState(int i)
    {
        return ListView_GetCheckState(hListg,i);
    }
    void SetCheckState(int i,int val)
    {
        ListView_SetCheckState(hListg,i,val);
    }
    void SetItemState(int i,UINT32 state,UINT32 mask)
    {
        ListView_SetItemState(hListg,i,state,mask);
    }
    void GetItemText(int i,int sub,wchar_t *buf,int sz)
    {
        ListView_GetItemText(hListg,i,sub,buf,sz);
    }
    int InsertItem(const LVITEM *lvI)
    {
        return ListView_InsertItem(hListg,lvI);
    }
    void GetItem(LVITEM *item)
    {
        SendMessage(hListg,LVM_GETITEM,0,(LPARAM)item);
    }
    void InsertColumn(int i,const LVCOLUMN *lvc)
    {
        SendMessage(hListg,LVM_INSERTCOLUMN,i,reinterpret_cast<LPARAM>(lvc));
    }
    void SetColumn(int i,const LVCOLUMN *lvc)
    {
        SendMessage(hListg,LVM_SETCOLUMN,i,reinterpret_cast<LPARAM>(lvc));
    }
    void SetItemTextUpdate(int iItem,int iSubItem,const wchar_t *str)
    {
        wchar_t buf[BUFLEN];

        *buf=0;
        ListView_GetItemText(hListg,iItem,iSubItem,buf,BUFLEN);
        if(wcscmp(str,buf)!=0)
            ListView_SetItemText(hListg,iItem,iSubItem,const_cast<wchar_t *>(str));
    }
};
ListView_t ListView;

//}

//{ UpdateDialog
int UpdateDialog_t::getnewver(const char *s)
{
    while(*s)
    {
        if(*s=='_'&&s[1]>='0'&&s[1]<='9')
            return atoi(s+1);

        s++;
    }
    return 0;
}

int UpdateDialog_t::getcurver(const char *ptr)
{
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

static bool yes1(libtorrent::torrent_status const&){return true;}

void UpdateDialog_t::calctotalsize()
{
    totalsize=0;
    for(int i=0;i<ListView.GetItemCount();i++)
    if(ListView.GetCheckState(i))
    {
        wchar_t buf[BUFLEN];
        ListView.GetItemText(i,1,buf,32);
        totalsize+=_wtoi_my(buf);
    }
}

void UpdateDialog_t::calcavailablespace()
{
    // calculate available space on the download drive
    // for now this is the same as the exe path
    ULARGE_INTEGER lpFreeBytesAvailable;
    totalavail=0;
    if(GetDiskFreeSpaceEx(nullptr,
                          &lpFreeBytesAvailable,
                          nullptr,
                          nullptr))totalavail=lpFreeBytesAvailable.QuadPart>>20;
}

void UpdateDialog_t::updateTexts()
{
    if(!hUpdate)return;

    // Buttons
    SetWindowText(hUpdate,STR(STR_UPD_TITLE));
    SetWindowText(GetDlgItem(hUpdate,IDSELECTION),STR(STR_UPD_SELECTION));
    SetWindowText(GetDlgItem(hUpdate,IDOPTIONS),STR(STR_UPD_OPTIONS));
    SetWindowText(GetDlgItem(hUpdate,IDONLYUPDATE),STR(STR_UPD_ONLYUPDATES));
    SetWindowText(GetDlgItem(hUpdate,IDKEEPSEEDING),STR(STR_UPD_KEEPSEEDING));
    SetWindowText(GetDlgItem(hUpdate,IDCHECKALL),STR(STR_UPD_BTN_ALL));
    SetWindowText(GetDlgItem(hUpdate,IDUNCHECKALL),STR(STR_UPD_BTN_NONE));
    SetWindowText(GetDlgItem(hUpdate,IDCHECKNETWORK),STR(STR_UPD_BTN_NETWORK));
    SetWindowText(GetDlgItem(hUpdate,IDCHECKTHISPC),STR(STR_UPD_BTN_THISPC));
    SetWindowText(GetDlgItem(hUpdate,IDOK),STR(STR_UPD_BTN_OK));
    SetWindowText(GetDlgItem(hUpdate,IDCANCEL),STR(STR_UPD_BTN_CANCEL));
    SetWindowText(GetDlgItem(hUpdate,IDACCEPT),STR(STR_UPD_BTN_ACCEPT));

    // Total size and available space
    WStringShort buf;
    buf.sprintf(STR(STR_UPD_TOTALSIZE),totalsize);
    SetWindowText(GetDlgItem(hUpdate,IDTOTALSIZE),buf.Get());
    buf.sprintf(STR(STR_UPD_TOTALAVAIL),totalavail);
    SetWindowText(GetDlgItem(hUpdate,IDTOTALAVAIL),buf.Get());

    updateButtons();

    // Column headers
    LVCOLUMN lvc;
    lvc.mask=LVCF_TEXT;
    for(int i=0;i<6;i++)
    {
        lvc.pszText=const_cast<wchar_t *>(STR(STR_UPD_COL_NAME+i));
        ListView.SetColumn(i,&lvc);
    }
}

void UpdateDialog_t::updateButtons()
{
    // disable buttons if not enough space
    bool avail=UpdateDialog.totalsize<UpdateDialog.totalavail;
    EnableWindow(GetDlgItem(hUpdate, IDOK),avail);

    // disable Accept button not enough space or if seeding
    bool chk=SendMessage(GetDlgItem(hUpdate, IDKEEPSEEDING),BM_GETCHECK,0,0);
    EnableWindow(GetDlgItem(hUpdate, IDACCEPT),avail&&!chk);
}

void UpdateDialog_t::setCheckboxes()
{
    if(Updater->isPaused())return;

    // The app and indexes
    int baseChecked=0,indexesChecked=0;
    for(int i=0;i<Updater->numfiles;i++)
    if(hTorrent.file_priority(i)==2)
    {
<<<<<<< HEAD
        if(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"indexes\\"))
=======
        if(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"indexes\\"))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            indexesChecked=1;
        else
            baseChecked=1;
    }

    // Driverpacks
    for(int i=0;i<ListView.GetItemCount();i++)
    {
        LVITEM item;
        item.mask=LVIF_PARAM;
        item.iItem=i;
        ListView.GetItem(&item);
        int val=0;
        type_item *ItemData=(type_item*)item.lParam;

        if(ItemData->DefaultSort==-2)val=baseChecked;
        if(ItemData->DefaultSort==-1)val=indexesChecked;
        if(ItemData->DefaultSort>=0)val=hTorrent.file_priority((int)ItemData->DefaultSort);

        ListView.SetCheckState(i,val);
    }
}

void UpdateDialog_t::setPriorities()
{
    // set the torrent priorities based on the list items checkboxes

    // Clear priorities for driverpacks
    for(int i=0;i<Updater->numfiles;i++)
<<<<<<< HEAD
    if(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"drivers\\"))
=======
    if(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"drivers\\"))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
        hTorrent.file_priority(i,0);

    // Set priorities for driverpacks
    int base_pri=0,indexes_pri=0;
    for(int i=0;i<ListView.GetItemCount();i++)
    {
        // get each list view item
        LVITEM item;
        item.mask=LVIF_PARAM;
        item.iItem=i;
        ListView.GetItem(&item);
        // get the item check state
        int val=ListView.GetCheckState(i);

        type_item *ItemData=(type_item*)item.lParam;
        // app priority will be 2 if checked
        if(ItemData->DefaultSort==-2)base_pri=val?2:0;
        // index priority will be 2 if checked
        if(ItemData->DefaultSort==-1)indexes_pri=val?2:0;
        // driver priority will be 1 if checked
        if(ItemData->DefaultSort>= 0)hTorrent.file_priority(static_cast<int>(ItemData->DefaultSort),val);
    }

    // Set priorities for any torrent file that's not a driver
    for(int i=0;i<Updater->numfiles;i++)
<<<<<<< HEAD
    if(!StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"drivers\\"))
        hTorrent.file_priority(i,StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"indexes\\")?indexes_pri:base_pri);
=======
    if(!StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"drivers\\"))
        hTorrent.file_priority(i,StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"indexes\\")?indexes_pri:base_pri);
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
}

LRESULT CALLBACK UpdateDialog_t::NewButtonProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    short x,y;

    x=LOWORD(lParam);
    y=HIWORD(lParam);

    switch(uMsg)
    {
        case WM_MOUSEMOVE:
            Popup->drawpopup(0,STR_UPD_BTN_THISPC_H,FLOATING_TOOLTIP,x,y,hWnd);
            ShowWindow(Popup->hPopup,SW_SHOWNOACTIVATE);
            if(!bMouseInWindow)
            {
                bMouseInWindow=1;
                TRACKMOUSEEVENT tme;
                tme.cbSize=sizeof(tme);
                tme.dwFlags=TME_LEAVE;
                tme.hwndTrack=hWnd;
                TrackMouseEvent(&tme);
            }
            break;

        case WM_MOUSELEAVE:
            bMouseInWindow=0;
            Popup->drawpopup(0,0,FLOATING_NONE,0,0,hWnd);
            break;

        default:
            return CallWindowProc(wpOrigButtonProc,hWnd,uMsg,wParam,lParam);
    }
    return true;
}

BOOL CALLBACK UpdateDialog_t::UpdateProcedure(HWND hwnd,UINT Message,WPARAM wParam,LPARAM lParam)
{
    LVCOLUMN lvc;
    HWND thispcbut,chk1,chk2;
    wchar_t buf[32];
    int i;

    thispcbut=GetDlgItem(hwnd,IDCHECKTHISPC);
    chk1=GetDlgItem(hwnd,IDONLYUPDATE);
    chk2=GetDlgItem(hwnd,IDKEEPSEEDING);

    switch(Message)
    {
        case WM_INITDIALOG:
            setMirroring(hwnd);
            ListView.init(hwnd);
            lvc.mask=LVCF_FMT|LVCF_WIDTH|LVCF_SUBITEM|LVCF_TEXT;
            lvc.pszText=const_cast<wchar_t *>(L"");
            for(i=0;i<6;i++)
            {
                lvc.cx=cxn[i];
                lvc.iSubItem=i;
                lvc.fmt=i?LVCFMT_RIGHT:LVCFMT_LEFT;
                ListView.InsertColumn(i,&lvc);
            }

            hUpdate=hwnd;
            UpdateDialog.populate(0,true);
            UpdateDialog.updateTexts();
            UpdateDialog.setCheckboxes();
            if(Settings.flags&FLAG_ONLYUPDATES)SendMessage(chk1,BM_SETCHECK,BST_CHECKED,0);
            if(Settings.flags&FLAG_KEEPSEEDING)SendMessage(chk2,BM_SETCHECK,BST_CHECKED,0);

            wpOrigButtonProc=(WNDPROC)SetWindowLongPtr(thispcbut,GWLP_WNDPROC,(LONG_PTR)NewButtonProc);
            SetTimer(hwnd,1,2000,nullptr);

            if(Settings.flags&FLAG_AUTOUPDATE)SendMessage(hwnd,WM_COMMAND,IDCHECKALL,0);
            return TRUE;

        case WM_NOTIFY:
            {
                LPNMHDR lpnmh = (LPNMHDR)lParam;
                if(lpnmh->code==LVN_ITEMCHANGED)
                {
                    UpdateDialog.calctotalsize();
                    UpdateDialog.calcavailablespace();
                    UpdateDialog.updateTexts();
                    return TRUE;
                }
                // column sort
                if(lpnmh->code==LVN_COLUMNCLICK)
                    if(lpnmh->idFrom==IDLIST)
                    {
                        NMLISTVIEW* pListView = (NMLISTVIEW*)lParam;
                        if(pListView->iSubItem==ListViewSortColumn)
                            ListViewSortAsc=!ListViewSortAsc;
                        else
                        {
                            ListViewSortColumn=pListView->iSubItem;
                            ListViewSortAsc=TRUE;
                        }
                        LPARAM lParamSort=ListViewSortColumn+1;
                        if(!ListViewSortAsc)lParamSort=-lParamSort;
                        SendMessage(ListView.hListg,LVM_SORTITEMS,lParamSort,(LPARAM)ListView.CompareFunc);
                        return TRUE;
                    }
                break;
            }

        case WM_DESTROY:
            SetWindowLongPtr(thispcbut,GWLP_WNDPROC,(LONG_PTR)wpOrigButtonProc);
            ListView.close();
            break;

        case WM_TIMER:
            if(hSession&&hSession->is_paused()==0)UpdateDialog.populate(1);
            //Log.print_con(".");
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    hUpdate=nullptr;
                    UpdateDialog.setPriorities();
                    Settings.flags&=~FLAG_ONLYUPDATES;
                    if(SendMessage(chk1,BM_GETCHECK,0,0))Settings.flags|=FLAG_ONLYUPDATES;
                    Settings.flags&=~FLAG_KEEPSEEDING;
                    if(SendMessage(chk2,BM_GETCHECK,0,0))Settings.flags|=FLAG_KEEPSEEDING;
                    Updater->resumeDownloading();
                    EndDialog(hwnd,IDOK);
                    return TRUE;

                case IDACCEPT:
                    UpdateDialog.setPriorities();
                    Settings.flags&=~FLAG_ONLYUPDATES;
                    if(SendMessage(chk1,BM_GETCHECK,0,0))Settings.flags|=FLAG_ONLYUPDATES;
                    Settings.flags&=~FLAG_KEEPSEEDING;
                    if(SendMessage(chk2,BM_GETCHECK,0,0))Settings.flags|=FLAG_KEEPSEEDING;
                    Updater->resumeDownloading();
                    if(Settings.flags&FLAG_KEEPSEEDING)
                        EndDialog(hwnd,IDOK);
                    return TRUE;

                case IDCANCEL:
                    hUpdate=nullptr;
                    EndDialog(hwnd,IDCANCEL);
                    return TRUE;

                case IDONLYUPDATE:
                    Settings.flags&=~FLAG_ONLYUPDATES;
                    if(SendMessage(chk1,BM_GETCHECK,0,0))Settings.flags|=FLAG_ONLYUPDATES;
                    UpdateDialog.populate(0,true);
                    break;

                case IDKEEPSEEDING:
                    UpdateDialog.updateButtons();
                    return TRUE;

                case IDCHECKALL:
                case IDUNCHECKALL:
                    for(i=0;i<ListView.GetItemCount();i++)
                        ListView.SetCheckState(i,LOWORD(wParam)==IDCHECKALL?1:0);
                    if(Settings.flags&FLAG_AUTOUPDATE)
                    {
                        Settings.flags&=~FLAG_AUTOUPDATE;
                        SendMessage(hwnd,WM_COMMAND,IDOK,0);
                    }
                    return TRUE;

                case IDCHECKTHISPC:
                    for(i=0;i<ListView.GetItemCount();i++)
                    {
                        *buf=0;
                        ListView.GetItemText(i,5,buf,32);
                        ListView.SetCheckState(i,StrStrIW(buf,STR(STR_UPD_YES))?1:0);
                    }
                    return TRUE;

                case IDCHECKNETWORK:
                    for(i=0;i<ListView.GetItemCount();i++)
                    {
                        *buf=0;
                        ListView.GetItemText(i,0,buf,32);
                        bool chk=StrStrIW(buf,L"_LAN_")||StrStrIW(buf,L"_WLAN-WiFi_")||StrStrIW(buf,L"_WWAN-4G_");
                        ListView.SetCheckState(i,chk);
                    }
                default:
                    break;
            }
            break;

        case WM_CTLCOLORSTATIC:
            {
                // if not enough space to download turn label red
                bool avail=UpdateDialog.totalsize<UpdateDialog.totalavail;
                if((HWND)lParam==GetDlgItem(hUpdate,IDTOTALAVAIL)&&!avail)
                {
                    HDC hdcStatic=(HDC)wParam;
                    SetTextColor(hdcStatic, RGB(255,0,0));
                    SetBkColor(hdcStatic, GetSysColor(COLOR_BTNFACE));
                    return (LRESULT)GetStockObject(HOLLOW_BRUSH);
                }
                else return TRUE;
            }
            break;

        default:
            break;
    }
    return FALSE;
}

int UpdateDialog_t::populate(int update,bool clearlist)
{
    wchar_t buf[BUFLEN];
    int ret=0;
    LocalRevision=0;
    TorrentRevision=0;

    // Read torrent info
    std::vector<boost::int64_t> file_progress;
    auto ti=hTorrent.torrent_file();
    Updater->numfiles=0;
    if(!ti)return 0;
    hTorrent.file_progress(file_progress);
    Updater->numfiles=ti->num_files();

    // Calculate size and progress for the app and indexes
    int missingindexes=0;
    int newver=0;
    __int64 basesize=0,basedownloaded=0;
    __int64 indexsize=0,indexdownloaded=0;
    for(int i=0;i<Updater->numfiles;i++)
    {
<<<<<<< HEAD
        auto fe=ti->files().file_path(i);
=======
        auto fe=ti->files().file_name(i);
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
        // the file name minus the parent directory 'SDI_Update'
        const char *filenamefull=strchr(fe.c_str(),'\\')+1;

        // looking for missing "_" online indexes
        if(StrStrIA(filenamefull,"indexes\\"))
        {
            indexsize+=fe.size();
            indexdownloaded+=file_progress[i];
            wsprintf(buf,L"%S",filenamefull);
            *wcsstr(buf,L"DP_")=L'_';
            strsub(buf,L"indexes",Settings.index_dir);
            if(!System.FileExists(buf))
                missingindexes=1;
        }
        else if(!StrStrIA(filenamefull,"drivers\\"))
        {
            basesize+=fe.size();
            basedownloaded+=file_progress[i];
            if(StrStrIA(filenamefull,"sdi"))
                TorrentRevision=atol(StrStrIA(filenamefull,"sdi")+3);
        }
    }

    // Disable redrawing of the list
    ListView.DisableRedraw(clearlist);

    // Setup LVITEM
    LVITEM lvI;
    lvI.mask      =LVIF_TEXT|LVIF_STATE|LVIF_PARAM;
    lvI.stateMask =0;
    lvI.iSubItem  =0;
    lvI.state     =0;
    lvI.iItem     =0;

    int row=0;
    LocalRevision=System.FindLatestExeVersion();
//if(TorrentRevision>0)TorrentRevision=700;  // test
    // only return the torrent exe version if it's newer than what i have
    if(TorrentRevision>LocalRevision)ret+=TorrentRevision<<8;
    // the application entry
    if(TorrentRevision>LocalRevision&&ListView.IsVisible())
    {
        // the data item
        type_item *ItemData=new type_item;
        lvI.lParam=(LPARAM)ItemData;
        ItemData->DefaultSort=-2;
        wcscpy(ItemData->ItemName,STR(STR_UPD_APP));
        ItemData->SizeMB=basesize/1024/1024;
        ItemData->Percent=basedownloaded*100/basesize;
        ItemData->VersionNew=TorrentRevision;
        ItemData->VersionCurrent=LocalRevision;
        wcscpy(ItemData->ForThisPC,STR(STR_UPD_YES));
        // the list item
        lvI.pszText=const_cast<wchar_t *>(STR(STR_UPD_APP));
        if(!update)row=ListView.InsertItem(&lvI);
        wsprintf(buf,L"%d %s",(int)(basesize/1024/1024),STR(STR_UPD_MB));
        ListView.SetItemTextUpdate(row,1,buf);
        wsprintf(buf,L"%d%%",(int)(basedownloaded*100/basesize));
        ListView.SetItemTextUpdate(row,2,buf);
<<<<<<< HEAD
        wsprintf(buf,L" SDI%d",TorrentRevision);
        ListView.SetItemTextUpdate(row,3,buf);
        wsprintf(buf,L" SDI%d",LocalRevision);
=======
        wsprintf(buf,L" SDI_R%d",TorrentRevision);
        ListView.SetItemTextUpdate(row,3,buf);
        wsprintf(buf,L" SDI_R%d",LocalRevision);
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
        ListView.SetItemTextUpdate(row,4,buf);
        ListView.SetItemTextUpdate(row,5,STR(STR_UPD_YES));
        row++;
    }

    // Add indexes to the list
    if(missingindexes&&ListView.IsVisible())
    {
        // the data item
        type_item *ItemData=new type_item;
        lvI.lParam=(LPARAM)ItemData;
        ItemData->DefaultSort=-1;
        wcscpy(ItemData->ItemName,STR(STR_UPD_INDEXES));
        ItemData->SizeMB=indexsize/1024/1024;
        ItemData->Percent=indexdownloaded*100/indexsize;
        ItemData->VersionNew=0;
        ItemData->VersionCurrent=0;
        wcscpy(ItemData->ForThisPC,STR(STR_UPD_YES));
        // the list item
        lvI.pszText=const_cast<wchar_t *>(STR(STR_UPD_INDEXES));
        if(!update)row=ListView.InsertItem(&lvI);
        wsprintf(buf,L"%d %s",(int)(indexsize/1024/1024),STR(STR_UPD_MB));
        ListView.SetItemTextUpdate(row,1,buf);
        wsprintf(buf,L"%d%%",(int)(indexdownloaded*100/indexsize));
        ListView.SetItemTextUpdate(row,2,buf);
        ListView.SetItemTextUpdate(row,5,STR(STR_UPD_YES));
        row++;
    }

    // Add driverpacks to the list
    for(int i=0;i<Updater->numfiles;i++)
    {
        const char *filename=nullptr;
        const char *filenamefull=nullptr;
        filenamefull=strchr(ti->files().file_path(i).c_str(),'\\')+1;
        filename=strchr(filenamefull,'\\');
        if(filename)filename=strchr(filenamefull,'\\')+1;
        else filename=filenamefull;

        if(StrStrIA(filenamefull,".7z"))
        {
            int oldver;

            wsprintf(buf,L"%S",filename);
            lvI.pszText=buf;
            int sz=(int)(ti->files().file_size(i)/1024/1024);
            if(!sz)sz=1;

            newver=getnewver(filenamefull);
            oldver=getcurver(filename);

            // this flag means only get new versions of packs i already have
            if(Settings.flags&FLAG_ONLYUPDATES)
                {if(newver>oldver&&oldver)ret++;else continue;}
            else
                if(newver>oldver)ret++;

            if(newver>oldver&&ListView.IsVisible())
            {
                // the data item
                type_item *ItemData=new type_item;
                lvI.lParam=(LPARAM)ItemData;
                ItemData->DefaultSort=i;
                wcscpy(ItemData->ItemName,buf);
                ItemData->SizeMB=sz;
                ItemData->Percent=file_progress[i]*100/ti->files().file_size(i);
                ItemData->VersionNew=newver;
                ItemData->VersionCurrent=oldver;
                wcscpy(ItemData->ForThisPC,STR(STR_UPD_YES+manager_g->manager_drplive(buf)));
                // the list item
                if(!update)row=ListView.InsertItem(&lvI);
                wsprintf(buf,L"%d %s",sz,STR(STR_UPD_MB));
                ListView.SetItemTextUpdate(row,1,buf);
                wsprintf(buf,L"%d%%",(int)(file_progress[i]*100/ti->files().file_size(i)));
                ListView.SetItemTextUpdate(row,2,buf);
                wsprintf(buf,L"%d",newver);
                ListView.SetItemTextUpdate(row,3,buf);
                wsprintf(buf,L"%d",oldver);
                if(!oldver)wsprintf(buf,L"%ws",STR(STR_UPD_MISSING));
                ListView.SetItemTextUpdate(row,4,buf);
                wsprintf(buf,L"%S",filename);
                wsprintf(buf,L"%ws",STR(STR_UPD_YES+manager_g->manager_drplive(buf)));
                ListView.SetItemTextUpdate(row,5,buf);
                row++;
            }
        }
    }

    // Enable redrawing of the list
    ListView.EnableRedraw();

    // preselect the first item in the list
    ListView.SetItemState(0,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);

    if(update)return ret;

    if(ret)manager_g->itembar_settext(SLOT_NODRIVERS,0);
    manager_g->itembar_settext(SLOT_DOWNLOAD,ret?1:0,nullptr,ret,0,0);

    bool showpatreon=ret?1:0;
    if(emptydrp)showpatreon=false;
    if(Settings.flags&FLAG_HIDEPATREON)showpatreon=false;
    manager_g->itembar_settext(SLOT_PATREON,showpatreon,nullptr,ret,0,0);
      return ret;
}

void UpdateDialog_t::setFilePriority(const wchar_t *name,int pri)
{
    char buf[BUFLEN];
    wsprintfA(buf,"%S",name);

    for(int i=0;i<Updater->numfiles;i++)
<<<<<<< HEAD
    if(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),buf))
=======
    if(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),buf))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
    {
        hTorrent.file_priority(i,pri);
        Log.print_con("Req(%S,%d)\n",name,pri);
    }
}

void UpdateDialog_t::openDialog()
{
    DialogBox(ghInst,MAKEINTRESOURCE(IDD_DIALOG2),MainWindow.hMain,(DLGPROC)UpdateProcedure);
}
//}

//{ Updater
void UpdaterImp::updateTorrentStatus()
{
<<<<<<< HEAD
=======
    using namespace libtorrent;
    namespace lt = libtorrent;

>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
    std::vector<lt::torrent_status> temp;
    TorrentStatus_t *t=&TorrentStatus;

    memset(t,0,sizeof(TorrentStatus_t));
    if(!hSession)
    {
        wcscpy(t->error,STR(STR_DWN_ERRSES));
        return;
    }
    hSession->get_torrent_status(&temp,&yes1,0);

    if(temp.empty())
    {
        wcscpy(t->error,STR(STR_DWN_ERRTOR));
        return;
    }
    lt::torrent_status& st=temp[0];

    t->elapsed=13;

    wcscpy(t->error,L"");

    t->uploadspeed=st.upload_payload_rate;
    t->downloadspeed=st.download_payload_rate;

    t->seedstotal=st.list_seeds;
    t->peerstotal=st.list_peers;
    t->seedsconnected=st.num_seeds;
    t->peersconnected=st.num_peers;

    t->wasted=(int)st.total_redundant_bytes;
    t->wastedhashfailes=(int)st.total_failed_bytes;

    t->downloaded=st.total_wanted_done;
    t->downloadsize=st.total_wanted;
    t->uploaded=st.total_payload_upload;

    if(torrenttime)t->elapsed=System.GetTickCountWr()-torrenttime;
    if(t->downloadspeed)
    {
        averageSpeed=static_cast<int>(SMOOTHING_FACTOR*t->downloadspeed+(1-SMOOTHING_FACTOR)*averageSpeed);
        if(averageSpeed)t->remaining=(t->downloadsize-t->downloaded)/averageSpeed*1000;
    }

    // st.state is the enumerated type of the current state of the torrent
    // "There is only a distinction between finished and seeding if some pieces
    //  or files have been set to priority 0, i.e. are not downloaded."

    // if the session is shut down then clear the stats
    if(hSession->is_paused())
    {
        *t={};
        t->status_strid=STR_TR_ST4;
    }
    // if torrent is seeding
    else if((st.state==torrent_status::finished)&SeedMode)
        t->status_strid=STR_TR_ST5;
    // if we're moving the downloaded files
    else if(movingfiles)
        t->status_strid=STR_TR_ST8;
    // other torrent states
    else if(st.paused)
        t->status_strid=STR_TR_ST0;
    else if(st.state==torrent_status::checking_files)
        t->status_strid=STR_TR_ST1;
    else if(st.state==torrent_status::downloading_metadata)
        t->status_strid=STR_TR_ST2;
    else if(st.state==torrent_status::downloading)
        t->status_strid=STR_TR_ST3;
    else if(st.state==torrent_status::finished)
        t->status_strid=STR_TR_ST4;
    else if(st.state==torrent_status::seeding)
        t->status_strid=STR_TR_ST5;
    else if(st.state==torrent_status::allocating)
        t->status_strid=STR_TR_ST6;
    else if(st.state==torrent_status::checking_resume_data)
        t->status_strid=STR_TR_ST7;

    t->sessionpaused=hSession->is_paused();
    t->torrentpaused=st.paused;

    if(TorrentStatus.downloadsize)
        manager_g->itembar_settext(SLOT_DOWNLOAD,1,nullptr,-1,-1,TorrentStatus.downloaded*1000/TorrentStatus.downloadsize);
    MainWindow.redrawfield();
}

void UpdaterImp::removeOldDriverpacks(const wchar_t *ptr)
{
    WStringShort bffw;
    bffw.append(ptr);
    wchar_t *s=bffw.GetV();
    while(*s)
    {
        if(*s=='_'&&s[1]>='0'&&s[1]<='9')
        {
            *s=0;
            s=const_cast<wchar_t *>(manager_g->matcher->finddrp(bffw.Get()));
            if(!s)return;
            WStringShort buf;
            buf.sprintf(L"%ws\\%s",Settings.drp_dir,s);
            Log.print_con("Old file: %S\n",buf.Get());
            _wremove(buf.Get());
            return;
        }
        s++;
    }
}

void UpdaterImp::moveNewFiles()
{
    int i;

    auto ti=hTorrent.torrent_file();
    monitor_pause=1;

    // Delete old "_" online indexes if new are downloaded
    for(i=0;i<numfiles;i++)
        if(hTorrent.file_priority(i)&&
<<<<<<< HEAD
           StrStrIA(ti->files().file_path(i).c_str(),"indexes"))
=======
           StrStrIA(ti->files().file_name(i).c_str(),"indexes\\SDI"))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            break;
    if(i!=numfiles)
    {
        WStringShort buf;
        buf.sprintf(L"/c del %ws\\_*.bin",Settings.index_dir);
        System.run_command(L"cmd",buf.Get(),SW_HIDE,1);
    }

    for(i=0;i<numfiles;i++)
    if(hTorrent.file_priority(i))
    {
        std::string filenamefull= ti->files().file_path(i);
        if(activetorrent==1)
            filenamefull=strchr(filenamefull.c_str(),'\\')+1;
        // Skip autorun.inf and del_old_driverpacks.bat
        if(StrStrIA(filenamefull.c_str(),"autorun.inf")||StrStrIA(filenamefull.c_str(),".bat"))continue;

        wchar_t filenamefull_src[BUFLEN];
        wsprintf(filenamefull_src,L"%s\\%S", active_torrent_save_path->c_str(), ti->files().file_path(i).c_str());

        // Determine destination dirs
        wchar_t filenamefull_dst[BUFLEN];
        wsprintf(filenamefull_dst,L"%S",filenamefull.c_str());
        strsub(filenamefull_dst,L"indexes",Settings.index_dir);
        strsub(filenamefull_dst,L"drivers",Settings.drp_dir);
        strsub(filenamefull_dst,L"tools",Settings.data_dir);

        // Delete old driverpacks
        if(StrStrIA(filenamefull.c_str(),"drivers\\"))
            removeOldDriverpacks(filenamefull_dst+8);

        // Prepare "_" online indexes
        wchar_t *p=filenamefull_dst;
        if(p)
        {
            while(wcschr(p,L'\\'))p=wcschr(p,L'\\')+1;
            if(StrStrIW(filenamefull_src,L"indexes\\"))*p=L'_';

            // Create dirs for the file
            WStringShort dirs;
            dirs.append(filenamefull_dst);
            p=dirs.GetV();
            while(wcschr(p,L'\\'))p=wcschr(p,L'\\')+1;
            if(p[-1]==L'\\')
            {
                *--p=0;
                mkdir_r(dirs.Get());
            }
        }

        // can't move a file to a different drive
        // instead do a copy/delete

        // get current working drive
        wchar_t* buffer;
        int cwdDrive=-1;
        if ( (buffer = _wgetcwd(nullptr,BUFLEN) ) == nullptr)
            Log.print_con("_wgetcwd error");
        else
            cwdDrive = System.DriveNumber(buffer);

        // find the source  drive
        int srcDrive = System.DriveNumber(filenamefull_src);
        if (srcDrive==-1) srcDrive=cwdDrive;
        Log.print_con("Src: %d %S\n",srcDrive,filenamefull_src);
        // find the destination drive
        int destDrive = System.DriveNumber(filenamefull_dst);
        if ( (wcscspn(filenamefull_dst,L"\\\\")!=0) && (destDrive==-1) ) destDrive=cwdDrive;
        Log.print_con("Dst: %d %S\n",destDrive,filenamefull_dst);

        // if source and destination drive are the same then perform a move
        if (srcDrive==destDrive)
        {
            // Move file
            Log.print_con("Move new file: %S\n",filenamefull_dst);
            if(!MoveFileEx(filenamefull_src,filenamefull_dst,MOVEFILE_REPLACE_EXISTING||
                                                             MOVEFILE_COPY_ALLOWED||
                                                             MOVEFILE_WRITE_THROUGH))
                Log.print_syserr(GetLastError(),L"MoveFileEx()");
        }
        // if not perform a copy / delete
        else
        {
            Log.print_con("Copy new file: %S\n",filenamefull_dst);
            if(!CopyFileExW(filenamefull_src, filenamefull_dst,nullptr,nullptr,nullptr,0))
                Log.print_syserr(GetLastError(),L"CopyFileExW()");
            else if(System.FileExists(filenamefull_dst))
                System.deletefile(filenamefull_src);
        }
	}
    System.run_command(L"cmd",L" /c rd /s /q update",SW_HIDE,1);
}

void UpdaterImp::checkUpdates()
{
    if(!System.canWriteDirectory(L"update"))
    {
        Log.print_err("ERROR in checkUpdates(): Write-protected,'update'\n");
        return;
    }

    downloadmangar_exitflag=DOWNLOAD_STATUS_DOWLOADING_TORRENT;
    downloadmangar_event->raise();
}

void UpdaterImp::ShowProgress(wchar_t *buf)
{
    // updates the text on the updates button

    using namespace libtorrent;
    namespace lt = libtorrent;

    if(hSession)
    {
        wchar_t num1[BUFLEN],num2[BUFLEN],num3[BUFLEN],num4[BUFLEN];
        format_size(num1,TorrentStatus.downloaded,0);
        format_size(num2,TorrentStatus.downloadsize,0);
        format_size(num3,TorrentStatus.uploadspeed,1);
        format_size(num4,TorrentStatus.uploaded,0);

        std::vector<torrent_status> temp;
        hSession->get_torrent_status(&temp,&yes1,0);
        if(temp.empty())return;
        torrent_status& st=temp[0];
        if(closingsession)
            wsprintf(buf,STR(STR_DWN_CLOSING));
        else if(movingfiles)
            wsprintf(buf,STR(STR_TR_ST8));
        else if(st.paused)
            wsprintf(buf,STR(STR_TR_ST0));
        else if(st.state==torrent_status::checking_files)
            wsprintf(buf,STR(STR_UPD_CHECKINGFILES),num1,num2,
                (TorrentStatus.downloadsize)?TorrentStatus.downloaded*100/TorrentStatus.downloadsize:0);
        else if(st.state==torrent_status::downloading_metadata)
            wsprintf(buf,STR(STR_TR_ST2));
        else if(st.state==torrent_status::downloading)
            wsprintf(buf,STR(STR_UPD_PROGRES),num1,num2,
                (TorrentStatus.downloadsize)?TorrentStatus.downloaded*100/TorrentStatus.downloadsize:0);
        else if((st.state==torrent_status::finished)&SeedMode)
            wsprintf(buf,STR(STR_DWN_SEEDING),num3,num4);
        else if(st.state==torrent_status::finished)
            wsprintf(buf,STR(STR_TR_ST4));
        else if(st.state==torrent_status::seeding)
            wsprintf(buf,STR(STR_DWN_SEEDING),num3,num4);
        else if(st.state==torrent_status::allocating)
            wsprintf(buf,STR(STR_TR_ST6));
        else if(st.state==torrent_status::checking_resume_data)
            wsprintf(buf,STR(STR_TR_ST7));
    }
}

void UpdaterImp::ShowPopup(Canvas &canvas)
{
    textdata_vert td(canvas);
    TorrentStatus_t t;
    int p0=D_X(POPUP_OFSX),p1=D_X(POPUP_OFSX)+10;
    long long per=0;
    wchar_t num1[BUFLEN],num2[BUFLEN];

    td.y=D_X(POPUP_OFSY);

    //update_getstatus(&t);
    t=TorrentStatus;

    format_size(num1,t.downloaded,0);
    format_size(num2,t.downloadsize,0);
    if(t.downloadsize)per=t.downloaded*100/t.downloadsize;
    td.TextOutSF(STR(STR_DWN_DOWNLOADED),STR(STR_DWN_DOWNLOADED_F),num1,num2,per);
    format_size(num1,t.uploaded,0);
    td.TextOutSF(STR(STR_DWN_UPLOADED),num1);
    format_time(num1,t.elapsed);
    td.TextOutSF(STR(STR_DWN_ELAPSED),num1);
    format_time(num1,t.remaining);
    td.TextOutSF(STR(STR_DWN_REMAINING),num1);

    td.nl();
    if(t.status_strid)
        td.TextOutSF(STR(STR_DWN_STATUS),L"%s",STR(t.status_strid));
    if(*t.error)
    {
        td.col=D_C(POPUP_CMP_INVALID_COLOR);
        td.TextOutSF(STR(STR_DWN_ERROR),L"%s",t.error);
        td.col=D_C(POPUP_TEXT_COLOR);
    }
    format_size(num1,t.downloadspeed,1);
    td.TextOutSF(STR(STR_DWN_DOWNLOADSPEED),num1);
    format_size(num1,t.uploadspeed,1);
    td.TextOutSF(STR(STR_DWN_UPLOADSPEED),num1);

    td.nl();
    td.TextOutSF(STR(STR_DWN_SEEDS),STR(STR_DWN_SEEDS_F),t.seedsconnected,t.seedstotal);
    td.TextOutSF(STR(STR_DWN_PEERS),STR(STR_DWN_SEEDS_F),t.peersconnected,t.peerstotal);
    format_size(num1,t.wasted,0);
    format_size(num2,t.wastedhashfailes,0);
    td.TextOutSF(STR(STR_DWN_WASTED),STR(STR_DWN_WASTED_F),num1,num2);

//    TextOutSF(&td,L"Paused",L"%d,%d",t.sessionpaused,t.torrentpaused);
    Popup->popup_resize((int)(td.getMaxsz()+POPUP_SYSINFO_OFS+p0+p1),td.y+D_X(POPUP_OFSY));
}

UpdaterImp::UpdaterImp()
{
    SeedMode=false;
    closingsession=false;
    TorrentStatus.sessionpaused=1;
    downloadmangar_exitflag=DOWNLOAD_STATUS_WAITING;

    switch(activetorrent)
    {
        case 1:
            active_torrent_url=&torrent_url;
            active_torrent_save_path=&torrent_save_path;
            break;
        case 2:
            active_torrent_url=&torrent2_url;
            active_torrent_save_path=&torrent2_save_path;
            break;
        default:
            break;
    }

    downloadmangar_event=CreateEventWr(true);

    installupdate_exitflag=0;
    installupdate_event=CreateEventWr(true);

    thandle_download=CreateThread();
    thandle_download->start(&thread_download,nullptr);
}

UpdaterImp::~UpdaterImp()
{
    if(thandle_download)
    {
        downloadmangar_exitflag=DOWNLOAD_STATUS_STOPPING;
        downloadmangar_event->raise();
        thandle_download->join();
        delete thandle_download;
        delete downloadmangar_event;
    }
}

int UpdaterImp::downloadTorrent()
{
	// checking for updates
    manager_g->itembar_settext(SLOT_DOWNLOAD,1,STR(STR_UPD_CHECKING),0,0,0);

    lt::error_code ec;
    int i;
    lt::add_torrent_params params;
<<<<<<< HEAD

    // Setup path and URL
    char url[BUFSIZ];
    wcstombs(url, active_torrent_url->c_str(), BUFSIZ);
    char spath[BUFSIZ];
    wcstombs(spath, active_torrent_save_path->c_str(), BUFSIZ);
    params.url = url;
    params.save_path = spath;
    Log.print_con("Torrent: %s\n", url);
    params.flags |= lt::add_torrent_params::flag_paused;
    params.flags |= lt::add_torrent_params::flag_seed_mode;
    params.flags |= lt::add_torrent_params::flag_auto_managed;

    // Settings
    settings.set_str(lt::settings_pack::user_agent, "Snappy Drivers Installer " VER_VERSION_STR2);
    //   settings.always_send_user_agent=true; // By default include a user-agent to just the first request in a connection
 //   settings.anonymous_mode=false;     //Is false by default
    settings.set_int(lt::settings_pack::choking_algorithm, lt::settings_pack::rate_based_choker);
    //   settings.disk_cache_algorithm=session_settings::avoid_readback;   Disk caching was removed in libtorrent, at least 1.1, maybe earlier
//    settings.volatile_read_cache=false;   // Let's leave it on by default

    settings.set_str(lt::settings_pack::dht_bootstrap_nodes, "dht.libtorrent.org:25401,router.bittorrent.com:6881,router.utorrent.com:6881,dht.transmissionbt.com:6881,dht.aelitis.com:6881");
//    settings.set_bool(settings_pack::enable_dht, false);  // True by default
    settings.set_int(lt::settings_pack::alert_mask,
        lt::alert::dht_notification   |
        lt::alert::error_notification |
        lt::alert::tracker_notification |
        lt::alert::ip_block_notification |
        lt::alert::dht_notification |
        lt::alert::performance_warning |
        lt::alert::storage_notification);
    lt::dht_settings dht;
//	dht.max_peers_reply = 70;	//Let's leave a default
    //   dht.privacy_lookups=true; Privacy lookups slightly more expensive
    hSession->set_dht_settings(dht);
    

    hTorrent=hSession->add_torrent(params, ec);   // TODO: test async_add

    if (ec)Log.print_err("ERROR: failed to add torrent: %s\n", ec.message().c_str());

=======

    // Setup path and URL
    char url[BUFSIZ];
    wcstombs(url, active_torrent_url->c_str(), BUFSIZ);
    char spath[BUFSIZ];
    wcstombs(spath, active_torrent_save_path->c_str(), BUFSIZ);
    params.url = url;
    params.save_path = spath;
    Log.print_con("Torrent: %s\n", url);
    params.flags |= lt::add_torrent_params::flag_paused;
    params.flags |= lt::add_torrent_params::flag_seed_mode;
    params.flags |= lt::add_torrent_params::flag_auto_managed;

    // Settings
    settings.set_str(lt::settings_pack::user_agent, "Snappy Drivers Installer " VER_VERSION_STR2);
    //   settings.always_send_user_agent=true; // By default include a user-agent to just the first request in a connection
 //   settings.anonymous_mode=false;     //Is false by default
    settings.set_int(lt::settings_pack::choking_algorithm, lt::settings_pack::rate_based_choker);
    //   settings.disk_cache_algorithm=session_settings::avoid_readback;   Disk caching was removed in libtorrent, at least 1.1, maybe earlier
//    settings.volatile_read_cache=false;   // Let's leave it on by default

    settings.set_str(lt::settings_pack::dht_bootstrap_nodes, "dht.libtorrent.org:25401,router.bittorrent.com:6881,router.utorrent.com:6881,dht.transmissionbt.com:6881,dht.aelitis.com:6881");
//    settings.set_bool(settings_pack::enable_dht, false);  // True by default
    settings.set_int(lt::settings_pack::alert_mask,
        lt::alert::dht_notification   |
        lt::alert::error_notification |
        lt::alert::tracker_notification |
        lt::alert::ip_block_notification |
        lt::alert::dht_notification |
        lt::alert::performance_warning |
        lt::alert::storage_notification);
    lt::dht_settings dht;
//	dht.max_peers_reply = 70;	//Let's leave a default
    //   dht.privacy_lookups=true; Privacy lookups slightly more expensive
    hSession->set_dht_settings(dht);
    

    hTorrent=hSession->add_torrent(params, ec);   // TODO: test async_add

    if (ec)Log.print_err("ERROR: failed to add torrent: %s\n", ec.message().c_str());

>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
//   settings.always_send_user_agent=true; Include a user-agent to just the first request in a connection by default
 //   settings.anonymous_mode=false;     Default is false
 	settings.set_bool(lt::settings_pack::allow_multiple_connections_per_ip, true);            //*That way two people from behind the same NAT
 //   can use the service simultaneously 

 //   settings.disk_cache_algorithm=session_settings::avoid_readback;   Disk caching was removed in libtorrent, at least 1.1, maybe earlier
//    settings.volatile_read_cache=false;   Let's leave it on by default
//    s->set_settings(settings); That function is deprecated use lt::settings_pack pack;
    Timers.start(time_chkupdate);

//    hSession->start_lsd();    // Enabled by default
//    hSession->start_natpmp(); // Enabled by default

    // Connecting
    settings.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:torrentport,[::]:torrentport");
    if(ec)Log.print_err("ERROR: failed to open listen socket: %s\n",ec.message().c_str());
    Log.print_con("Listen port: %d (%s)\nDownload limit: %dKb\nUpload limit: %dKb\n",
            hSession->listen_port(),hSession->is_listening()?"connected":"disconnected",
            downlimit,uplimit);


    // Pause and set speed limits
    hSession->pause();
    SetLimits();
    hTorrent.resume();

    // Download torrent
    Log.print_con("Waiting for torrent");
    for(int i=0;i<200;i++)
    {
        Log.print_con(".");
        // test if torrent has been retrieved
        if(hTorrent.torrent_file())
        {
            downloadmangar_exitflag=DOWNLOAD_STATUS_TORRENT_GOT;
            Log.print_con("DONE\n");
            break;
        }
        // test if there was some error condition
        if(downloadmangar_exitflag!=DOWNLOAD_STATUS_DOWLOADING_TORRENT)break;
        Sleep(100);
    }

    if(Settings.flags&FLAG_SCRIPTMODE)
    {
        if(!hTorrent.torrent_file())
            downloadmangar_exitflag=DOWNLOAD_STATUS_STOPPING;
        return 0;
    }

	// checking for updates
	manager_g->itembar_settext(SLOT_DOWNLOAD,0,L"",0,0,0);

    // test if we failed to get the torrent
    if(!hTorrent.torrent_file())
    {
        if(emptydrp) manager_g->itembar_settext(SLOT_NODRIVERS,1);
        downloadmangar_exitflag=DOWNLOAD_STATUS_STOPPING;
        Log.print_con("FAILED\n");
        return 0;
    }

    // Populate list
    i=UpdateDialog.populate(0);
    if(UpdateDialog.TorrentRevision==0)
        Log.print_con("Latest Version: Not found.\n");
    else if(UpdateDialog.TorrentRevision<=UpdateDialog.LocalRevision)
        Log.print_con("Latest Version: R%d. Up to date.\n",UpdateDialog.TorrentRevision);
    else
        Log.print_con("Latest Version: R%d.\n",UpdateDialog.TorrentRevision);
    Log.print_con("Updated driver packs available: %d\n",i&0xFF);

    // clear the torrent priorities
    for(int j=0;j<numfiles;j++)hTorrent.file_priority(j,0);

    Timers.stop(time_chkupdate);
    return i;
}

void UpdaterImp::resumeDownloading()
{
    if(!hSession||!hTorrent.torrent_file())
    {
        finisheddownloading=1;
        finishedupdating=1;
        return;
    }
    if(hSession->is_paused())
    {
        hTorrent.force_recheck();
        Log.print_con("torrent_resume\n");
        downloadmangar_exitflag=DOWNLOAD_STATUS_DOWLOADING_DATA;
        downloadmangar_event->raise();
    }

    hSession->resume();
    // sometimes the torrent stays paused
    hTorrent.resume();
    finisheddownloading=0;
    finishedupdating=0;
    torrenttime=System.GetTickCountWr();
}

int UpdaterImp::scriptInitUpdates(int torrentport)
{
    Updater->torrentport=torrentport;
    checkUpdates();
    while(downloadmangar_exitflag==DOWNLOAD_STATUS_DOWLOADING_TORRENT)
        downloadmangar_event->wait();

    // Read torrent info
    boost::shared_ptr<lt::torrent_info const> ti;
    ti=hTorrent.torrent_file();
    Updater->numfiles=0;
    if(ti)
    {
        Updater->numfiles=ti->num_files();
        Settings.flags|=FLAG_UPDATESOK;
        Log.print_con("Torrent downloaded successfully\n");
    }
    else
    {
        Log.print_con("Torrent download failed\n");
        return 1;
    }

    return (downloadmangar_exitflag==DOWNLOAD_STATUS_TORRENT_GOT?0:1);
}

int UpdaterImp::scriptDownloadApp()
{
    if((Settings.flags&FLAG_UPDATESOK)==0)
    {
        Log.print_err("Error: get : Updates not initialised");
        return 1;
    }

    // select everything that's not indexes and drivers in the torrent
    for(int i=0;i<Updater->numfiles;i++)
    {
<<<<<<< HEAD
        if(!(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"indexes\\"))&&
           !(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"drivers\\")))
=======
        if(!(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"indexes\\"))&&
           !(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"drivers\\")))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            hTorrent.file_priority(i,2);
        else
            hTorrent.file_priority(i,0);
    }
    return scriptDoDownload();
}

int UpdaterImp::scriptDownloadIndexes()
{
    if((Settings.flags&FLAG_UPDATESOK)==0)
    {
        Log.print_err("Error: get : Updates not initialised");
        return 1;
    }
    boost::shared_ptr<lt::torrent_info const> ti=hTorrent.torrent_file();
    
    // select indexes in the torrent
    for(int i=0;i<Updater->numfiles;i++)
    {
        // get the file entry
        std::string file= ti->files().file_path(i);
        size_t p=file.find("indexes\\");
        if(p!=std::string::npos)
            hTorrent.file_priority(i,2);
        else
            hTorrent.file_priority(i,0);
    }
    return scriptDoDownload();
}

int UpdaterImp::scriptDownloadDrivers(std::wstring mode)
{
    if((Settings.flags&FLAG_UPDATESOK)==0)
    {
        Log.print_err("Error: get : Updates not initialised");
        return 1;
    }
    Log.print_debug("%d items selected\n",manager_g->selected());

    bool all=_wcsicmp(mode.c_str(),L"all")==0;
    bool missing=_wcsicmp(mode.c_str(),L"missing")==0;
    bool updates=_wcsicmp(mode.c_str(),L"updates")==0;
    bool selected=_wcsicmp(mode.c_str(),L"selected")==0;

    // set all active
    manager_g->filter(126);

    boost::shared_ptr<lt::torrent_info const> ti;
    ti=hTorrent.torrent_file();
    int updatecount=0;

    // iterate the torrent
    for(int i=0;i<Updater->numfiles;i++)
    {
        // disable all files by default
        hTorrent.file_priority(i,0);
        // get the file entry
        std::string file= ti->files().file_path(i);
        // look for driver entries
        size_t p=file.find("drivers\\");
        if(p!=std::string::npos)
        {
            file.erase(0,p+8);
            int curver=System.getcurver(file.c_str());
            int newver=System.getver(file.c_str());
            bool getfile=false;
            if(all)
                getfile=newver>curver;
            else if(missing)
                getfile=newver>curver&&!curver;
            else if(updates)
                getfile=newver>curver&&curver;
            else if (selected&&newver>curver)
            {
                std::wstring widestr = std::wstring(file.begin(), file.end());
                getfile=manager_g->isSelected(widestr.c_str()) &&
                        !manager_g->manager_drplive(widestr.c_str()); // 0 = yes
            }
            if(getfile)
            {
                Log.print_debug("Getting: %s\n", file.c_str());
                hTorrent.file_priority(i,1);
                updatecount++;
            }
        }
    }

    if(!updatecount)
    {
        Log.print_con("Driver packs are up to date, nothing to do\n");
        return 0;
    }

    Log.print_con("Getting %d driver packs\n",updatecount);
    return scriptDoDownload();
}

int UpdaterImp::scriptDownloadEverything()
{
    if((Settings.flags&FLAG_UPDATESOK)==0)
    {
        Log.print_err("Error: get : Updates not initialised");
        return 1;
    }

    boost::shared_ptr<lt::torrent_info const> ti;
    ti=hTorrent.torrent_file();

    for(int i=0;i<Updater->numfiles;i++)
        hTorrent.file_priority(i,0);

    // select everything that's not drivers
    for(int i=0;i<Updater->numfiles;i++)
    {
<<<<<<< HEAD
        if(!(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"drivers\\")))
=======
        if(!(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"drivers\\")))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            hTorrent.file_priority(i,2);
    }

    // missing and updated drivers
    for(int i=0;i<Updater->numfiles;i++)
    {
        // get the file entry
        std::string file=to_lower(ti->files().file_path(i));
        // look for driver entries
        size_t p=file.find("drivers\\");
        if(p!=std::string::npos)
        {
            file.erase(0,p+8);
            int curver=System.getcurver(file.c_str());
            int newver=System.getver(file.c_str());
            // newer or missing
            if(newver>curver)
            {
                Log.print_debug("Getting: %s\n", file.c_str());
                hTorrent.file_priority(i,1);
            }
        }
    }
    return scriptDoDownload();
}

int UpdaterImp::scriptDoDownload()
{
    int count=0;
    for(int i=0;i<Updater->numfiles;i++)
        if(hTorrent.file_priority(i))count++;
    if(!count)return 0;
    resumeDownloading();
    while(downloadmangar_exitflag==DOWNLOAD_STATUS_DOWLOADING_DATA)
        downloadmangar_event->wait();
    return (downloadmangar_exitflag==DOWNLOAD_STATUS_FINISHED_DOWNLOADING?0:1);
}

int UpdaterImp::scriptInstall()
{
    // check if anything selected
    if(manager_g->selected()==0)
    {
        Log.print_err("Error: install : Nothing selected.\n");
        return 1;
    }

    manager_g->itembar_setactive(SLOT_RESTORE_POINT,0);
    // switch off all files by default
    if(Settings.flags&FLAG_UPDATESOK)
    {
        for(int i=0;i<Updater->numfiles;i++)
            hTorrent.file_priority(i,0);
    }
    manager_g->install(INSTALLDRIVERS);
    installupdate_exitflag=0;
    while(installupdate_exitflag==0)
        installupdate_event->wait();
    return (installupdate_exitflag==1?0:1); // 1=success
}

void UpdaterImp::DownloadAll()
{
    for(int i=0;i<Updater->numfiles;i++)
<<<<<<< HEAD
        if(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"indexes\\"))
=======
        if(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"indexes\\"))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            hTorrent.file_priority(i,2);
        else
            hTorrent.file_priority(i,1);
    Updater->resumeDownloading();
}

void UpdaterImp::DownloadNetwork()
{
    for(int i=0;i<Updater->numfiles;i++)
    {
        // indexes
<<<<<<< HEAD
        if(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"indexes\\"))
=======
        if(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"indexes\\"))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            hTorrent.file_priority(i,2);
        else
        {
            // the file name minus the path
<<<<<<< HEAD
            std::string filename=strrchr(hTorrent.torrent_file()->files().file_path(i).c_str(),'\\')+1;
=======
            std::string filename=strrchr(hTorrent.torrent_file()->files().file_name(i).c_str(),'\\')+1;
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            // look for all networking packs
            size_t found1=filename.find("_LAN_");
            size_t found2=filename.find("_WLAN-WiFi_");
            size_t found3=filename.find("_WWAN-4G_");
            if((found1!=std::string::npos)||(found2!=std::string::npos)||(found3!=std::string::npos))
                hTorrent.file_priority(i,1);
        }
    }
    Updater->resumeDownloading();
}

void UpdaterImp::DownloadIndexes()
{
    for(int i=0;i<Updater->numfiles;i++)
<<<<<<< HEAD
        if(StrStrIA(hTorrent.torrent_file()->files().file_path(i).c_str(),"indexes\\"))
=======
        if(StrStrIA(hTorrent.torrent_file()->files().file_name(i).c_str(),"indexes\\"))
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
            hTorrent.file_priority(i,2);
    Updater->resumeDownloading();
}

void UpdaterImp::StartSeedingDrivers()
{
    if(!hSession)return;

    // don't interrupt the current session
    if(!hSession->is_paused())
        return;

    // i'm going to point the torrent files back to
    // the actual drivers directory rather than the update directory
    // and enable seeding for whatever files exist there
    // once i've done this it's no good for regular downloading

    wchar_t buf[BUFLEN];
    buf[0]=0;
    hSession->pause();
    boost::shared_ptr<lt::torrent_info const> ti;
    ti=hTorrent.torrent_file();
    if(!ti)return;

    // modify the files in the torrent
    for(int i=0;i<ti->num_files();i++)
    {
        // disable all files by default
        hTorrent.file_priority(i,0);
        // get the file entry
        std::string file=to_lower(ti->files().file_path(i));
        size_t p=file.find("drivers\\");
        if(p!=std::string::npos)
        // seed *existing* drivers only
        {
            // remove the parent directory 'SDI_Update'
            file.replace(0,p,"");
            // prepend the app path
            file=System.AppPathS()+"\\"+file;
            // if the file exists, reset the path in the torrent and enable it
            MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,file.c_str(),strlen(file.c_str())+1,buf,BUFLEN);
            if(System.FileExists2(buf))
            {
                hTorrent.rename_file(i,file);
                hTorrent.file_priority(i,1);
                Log.print_con("Seeding: %s\n",file.c_str());
            }
        }
    }

    // restart the torrent
    SeedMode=true;
    resumeDownloading();

    // update the system menu text
    PostMessage(MainWindow.hMain, WM_SEEDING, 0, 1);
}

void UpdaterImp::StopSeedingDrivers()
{
    // switching off SeedMode will trigger MoveFiles which we don't want
    if(isSeedingDrivers())
    {
        // update the button status via ShowProgress
        closingsession=true;
        // give the gui time to update
        // when it's done it will reset the updater via WM_TIMER
        // why is this not firing the WM_TIMER procedure?
        SetTimer(MainWindow.hMain,2,500,nullptr);
        // update the system menu text
        PostMessage(MainWindow.hMain, WM_SEEDING, 0, 0);

        MainWindow.ResetUpdater(Updater_t::activetorrent);
    }
}

unsigned int __stdcall UpdaterImp::thread_download(void *arg)
{
	UNREFERENCED_PARAMETER(arg);

    // Wait till is allowed to download the torrent
    Log.print_debug("{thread_download\n");
    downloadmangar_event->wait();
    if(downloadmangar_exitflag!=DOWNLOAD_STATUS_DOWLOADING_TORRENT)
    {
        Log.print_debug("}thread_download(never started)\n");
        return 0;
    }

    // Download torrent
    UpdaterImp *Updater1=dynamic_cast<UpdaterImp *>(Updater);
    int TorrentResults=Updater1->downloadTorrent();
    if(downloadmangar_exitflag!=DOWNLOAD_STATUS_TORRENT_GOT)
    {
        Log.print_con("}thread_download(failed to download torrent)\n");
        return 0;
    }

    Updater1->updateTorrentStatus();
    downloadmangar_event->reset();

    // successfully downloaded the torrent
    // notify main window
    PostMessage(MainWindow.hMain,WM_TORRENT,0,TorrentResults);

    // when run with /autoupdate parameter and nothing to download
    // openDialog locks up - possibly invisible modal dialog
    if(Settings.flags&FLAG_AUTOUPDATE&&TorrentResults==0)
    {
        finishedupdating=1;
        monitor_pause=0;
        downloadmangar_exitflag=DOWNLOAD_STATUS_STOPPING;
    }

    while(downloadmangar_exitflag!=DOWNLOAD_STATUS_STOPPING)
    {
        // Wait till is allowed to download driverpacks
        if(Settings.flags&FLAG_AUTOUPDATE&&System.canWriteDirectory(L"update"))
            UpdateDialog.openDialog();
        else
            downloadmangar_event->wait();

        if(downloadmangar_exitflag==DOWNLOAD_STATUS_STOPPING)break;

        // Downloading loop
        Log.print_con("{torrent_start\n");
        while(downloadmangar_exitflag==DOWNLOAD_STATUS_DOWLOADING_DATA&&hSession)
        {
            Sleep(500);

            // Show progress
            Updater1->updateTorrentStatus();
            if(!(Settings.flags&FLAG_SCRIPTMODE))
            {
                MainWindow.ShowProgressInTaskbar(true,TorrentStatus.downloaded,TorrentStatus.downloadsize);
                InvalidateRect(Popup->hPopup,nullptr,0);
            }

            // Send libtorrent messages to log
            std::vector<lt::alert*> alerts;
            hSession->pop_alerts(&alerts);
            for (auto a : alerts)
            {
                if (Log.isAllowed(LOG_VERBOSE_TORRENT)) continue;
                Log.print_con("Torrent: %s | %s\n", a->what(), a->message().c_str());
            }

            // process the downloads when finished and not seeding
            if((TorrentStatus.status_strid==STR_TR_ST0+libtorrent::torrent_status::finished)&&!SeedMode)
            {
                Log.print_con("Torrent: finished\n");
                hSession->pause();

                // Flush cache
                Log.print_con("Torrent: flushing cache...");
                hTorrent.flush_cache();
                // synchronize to make sure the files have been created on disk
                using namespace lt;
                while(1)
                {
                    auto const* a=hSession->wait_for_alert(seconds(60*2));
                    if(!a)
                    {
                        Log.print_con("time out\n");
                        break;
                    }
                    if(alert_cast<cache_flushed_alert>(a))
                    {
                        Log.print_con("done\n");
                        break;
                    }
                }

                // Move files
                movingfiles=1;
                Updater1->updateTorrentStatus();
                Updater1->moveNewFiles();
                movingfiles=0;
                Updater1->updateTorrentStatus();
                hTorrent.force_recheck();

                if(Settings.flags&FLAG_SCRIPTMODE)
                {
                    downloadmangar_exitflag=DOWNLOAD_STATUS_FINISHED_DOWNLOADING;
                    downloadmangar_event->raise();
                }
                else
                {
                    // Update list
                    UpdateDialog.populate(0,true);

                    // Execute user cmd
                    if(*Settings.finish_upd)
                    {
                        WStringShort buf;
                        buf.sprintf(L" /c %s",Settings.finish_upd);
                        System.run_command(L"cmd",buf.Get(),SW_HIDE,0);
                    }
                    if((Settings.flags&FLAG_AUTOCLOSE)&&!(Settings.flags&FLAG_AUTOINSTALL))
                        PostMessage(MainWindow.hMain,WM_CLOSE,0,0);

                    downloadmangar_exitflag=DOWNLOAD_STATUS_FINISHED_DOWNLOADING;
                    downloadmangar_event->raise();

                    // Flash in taskbar
                    MainWindow.ShowProgressInTaskbar(false);
                    FLASHWINFO fi;
                    fi.cbSize=sizeof(FLASHWINFO);
                    fi.hwnd=MainWindow.hMain;
                    fi.dwFlags=FLASHW_ALL|FLASHW_TIMERNOFG;
                    fi.uCount=1;
                    fi.dwTimeout=0;
                    if(installmode==MODE_NONE)
                    {
                        FlashWindowEx(&fi);
                        invalidate(INVALIDATE_INDEXES|INVALIDATE_MANAGER);
                    }
                }// else script mode
            }
        }
        // Download is completed
        finishedupdating=1;
        hSession->pause();
        Updater1->updateTorrentStatus();
        monitor_pause=0;
        Log.print_con("}torrent_stop\n");
        downloadmangar_event->reset();
    } // !=DOWNLOAD_STATUS_STOPPING

    if(hSession)
    {
        Log.print_con("Closing torrent session...");
        hSession->remove_torrent(hTorrent);
        hSession->pause();
        hSession->abort();
        //delete ses;  not needed with s.reset
        Log.print_con("DONE\n");
        hSession=nullptr;
    }
    Log.print_con("}thread_download\n");
    return 0;
}

void UpdaterImp::pause(){downloadmangar_exitflag=DOWNLOAD_STATUS_PAUSING;}

bool UpdaterImp::isTorrentReady(){return hTorrent.torrent_file()!=nullptr;}
bool UpdaterImp::isPaused(){return TorrentStatus.sessionpaused;}
bool UpdaterImp::isUpdateCompleted(){return finishedupdating;}
bool UpdaterImp::isSeedingDrivers(){return SeedMode;}
int  UpdaterImp::Populate(int flags){return UpdateDialog.populate(flags,!flags);}
void UpdaterImp::SetFilePriority(const wchar_t *name,int pri){UpdateDialog.setFilePriority(name,pri);}
void UpdaterImp::SetLimits()
{
    if(!hSession)return;

    hTorrent.set_download_limit(downlimit*1024);
    hTorrent.set_upload_limit(uplimit*1024);
    if(connections)hTorrent.set_max_connections(connections);
}
void UpdaterImp::OpenDialog(){UpdateDialog.openDialog();}
//}
#else

#include "update.h"

Updater_t *Updater;
int Updater_t::torrentport=50171;
int Updater_t::downlimit=0;
int Updater_t::uplimit=0;
int Updater_t::connections=0;
wchar_t Updater_t::torrent_url[BUFSIZ];
#endif
