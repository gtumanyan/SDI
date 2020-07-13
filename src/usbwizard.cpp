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

// have to link in comctl32
// and call InitCommonControlsEx

#include "com_header.h"
#include <windows.h>
#include "logging.h"

#include "common.h"
#include "system.h"

#include <commctrl.h>
#include <windowsx.h>
#include <string>
#include <prsht.h>
#include "settings.h"
#include "usbwizard.h"
#include "wizards.h"
#include "manager.h"
#include "matcher.h"
#include "main.h"
#include "indexing.h"
#include <shlobj.h>
#include "theme.h"
#include <stdio.h>
#include <iostream>
#include <fstream>

extern HINSTANCE ghInst;
extern USBWizard *USBWiz;
extern Manager *manager_g;

static bool DriversSelected()
{
    // see if any drivers are selected on the main window
    Collection *col=manager_g->matcher->getCol();
    for(Driverpack &drp:*col->getList())
    {
        // get selected DriverPacks
        std::wstring filename=drp.getFilename();
        if(manager_g->isSelected(filename.c_str()))
            return true;
    }
    return false;
}

static LRESULT CALLBACK Page1DlgProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
            {
                // center the wizard over the application
                RECT rcOwner,rcDlg;
                HWND hwndOwner = GetParent(GetParent(hwnd));
                HWND hwndDlg =GetParent(hwnd);
                GetWindowRect(hwndOwner, &rcOwner);
                GetWindowRect(hwndDlg, &rcDlg);
                int left = (rcOwner.right - rcOwner.left)/2 - (rcDlg.right - rcDlg.left)/2 + rcOwner.left;
                int top = (rcOwner.bottom - rcOwner.top)/2 - (rcDlg.bottom - rcDlg.top)/2 + rcOwner.top;
                SetWindowPos (hwndDlg, nullptr, left, top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);

                // create the title static text with custom font
                HWND hStaticText=CreateWindowEx(WS_EX_TRANSPARENT,
                                                L"Static",
                                                STR(STR_USBWIZ_TITLE),
                                                WS_CHILD|WS_VISIBLE|SS_LEFT,
                                                210,20,280,40,
                                                hwnd,nullptr,ghInst,nullptr);
                if(hStaticText==nullptr)return true;
                // create a font for the static control
                HFONT hFont=CreateFont(22,0,0,0,650,
                                       FALSE,FALSE,FALSE,
                                       ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                       ANTIALIASED_QUALITY,DEFAULT_PITCH,
                                       L"MS ShellDlg");
                // Set the new font for the control
                SendMessage(hStaticText, WM_SETFONT, WPARAM (hFont), TRUE);
                // set the description text
                SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE1_DESC),STR(STR_USBWIZ_PAGE1_DESC));
            }
            return TRUE;

        case WM_NOTIFY:
            {
                LPNMHDR pnmh = (LPNMHDR)lParam;
                switch(pnmh->code)
                {
                    case PSN_SETACTIVE:
                        {
                            PropSheet_SetWizButtons(GetParent(hwnd), PSWIZB_NEXT);
                            break;
                        }
                    case PSN_WIZFINISH:
                        break;
                    case PSN_WIZBACK:
                        break;
                    case PSN_RESET:
                        break;
                    default:
                        break;
                }
                break;
            }
        default:
            break;
    }
    return FALSE;
}

static void Page2EnableButtons(HWND hwnd)
{
    // enable the next button if combo has a selection
    HWND hCtl=GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_COMBO);
    int ItemCount=ComboBox_GetCount(hCtl);
    if(ItemCount==0)
        PropSheet_SetWizButtons(GetParent(hwnd), PSWIZB_BACK);
    else
        PropSheet_SetWizButtons(GetParent(hwnd), PSWIZB_BACK|PSWIZB_NEXT);
}

static void Page2PopulateCombo(HWND hwnd)
{
    SetCursor(LoadCursor(nullptr,IDC_WAIT));

    // populate the combo with removable drives
    HWND hCtl=GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_COMBO);
    int LastSelected=ComboBox_GetCurSel(hCtl);
    ComboBox_ResetContent(hCtl);

    wchar_t myDrives[] = L"A:\\";

    // get the system drive
    wchar_t systemDrive[BUFLEN]={0};
    GetEnvironmentVariable(L"SystemDrive",systemDrive,BUFLEN);
    wcscat(systemDrive,L"\\");

    // get all logical drives
    DWORD drives=GetLogicalDrives();

    wchar_t vol[MAX_PATH+1];
    __int64 lpFreeBytesAvailable,lpTotalNumberOfBytes,lpTotalNumberOfFreeBytes;

    // iterate logical drives
    while(drives)
    {
        if(drives&1)
        {
            // get the drive type
            // some USB drives show up as 'fixed'
            // so I'll give the option of showing more drives
            UINT driveType=GetDriveType(myDrives);
            if( (wcscmp(systemDrive,myDrives)!=0) &&   // always ignore system drive
                (driveType!=DRIVE_CDROM) &&            // always ignore cd drive
                ( (USBWiz->ShowAllDrives) ||           // show all other drives
                  (driveType!=DRIVE_FIXED) )           // or show all other non fixed drives
               )
            {
                // get the volume label
                GetVolumeInformation(myDrives,vol,MAX_PATH+1,nullptr,nullptr,nullptr,nullptr,0);
                // get the drive stats
                GetDiskFreeSpaceEx(myDrives,(PULARGE_INTEGER)&lpFreeBytesAvailable,(PULARGE_INTEGER)&lpTotalNumberOfBytes,(PULARGE_INTEGER)&lpTotalNumberOfFreeBytes);
                // add to the combo
                std::wstring buf=vol;
                buf+=L" (";buf+=myDrives;buf+=L")";buf+=L"  ";
                buf+=std::to_wstring(lpTotalNumberOfBytes/1024/1024);buf+=L" MB, ";
                buf+=std::to_wstring(lpFreeBytesAvailable/1024/1024);buf+=L" MB free";
                int item=ComboBox_AddString(hCtl,buf.c_str());
                ComboBox_SetItemData(hCtl,item,(INT)myDrives[0]);
            }
        }
        myDrives[0]++;
        drives>>=1;
    }

    int ItemCount=ComboBox_GetCount(hCtl);
    // reselect the previously selected item if available
    // otherwise select the first item
    if((LastSelected==-1)&&(ItemCount>0))
        LastSelected=0;
    ComboBox_SetCurSel(hCtl,LastSelected);
    USBWiz->TargetDrive=ComboBox_GetItemData(hCtl,LastSelected);
    // enable the clear buttons
    Button_Enable(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_DELETE),ItemCount>0);
    Button_Enable(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_FORMAT),ItemCount>0);
    SetCursor(LoadCursor(nullptr,IDC_ARROW));
    Page2EnableButtons(hwnd);
}

static void BuildFilesList(HWND hwnd)
{
    // compile the files list
    // based on the user selections
    SetCursor(LoadCursor(nullptr,IDC_WAIT));

    wchar_t targetDrive[] = L"A:\\";
    targetDrive[0]=USBWiz->TargetDrive;
    __int64 lpTotalNumberOfBytes=0,lpTotalNumberOfFreeBytes=0;
    std::wstring SourceFileName,TargetFileName;
    USBWiz->ClearBuildList();

    // get the drive stats
    if(USBWiz->TargetDrive>-1)
        GetDiskFreeSpaceEx(targetDrive,(PULARGE_INTEGER)&USBWiz->BytesAvailable,(PULARGE_INTEGER)&lpTotalNumberOfBytes,(PULARGE_INTEGER)&lpTotalNumberOfFreeBytes);

    // DriverPacks option
    switch(USBWiz->DriverPackOption)
    {
        // all DriverPacks and indexes
        case 0:
        {
            // iterate active DriverPacks
            Collection *col=manager_g->matcher->getCol();
            for(Driverpack &drp:*col->getList())
            {
                // DriverPack
                SourceFileName=drp.getPath();
                SourceFileName.append(L"\\"+(std::wstring)drp.getFilename());
                TargetFileName=targetDrive;
                TargetFileName.append(L"drivers\\"+(std::wstring)drp.getFilename());
                USBWiz->AddFile(SourceFileName,TargetFileName);

                // index
                wchar_t indexname[BUFLEN];
                drp.getindexfilename(col->getIndex_bin_dir(),L"bin",indexname);
                SourceFileName=indexname;
                TargetFileName=targetDrive+(std::wstring)indexname;
                USBWiz->AddFile(SourceFileName,TargetFileName);
            }
            break;
        }

        // network DriverPacks and indexes
        case 1:
        {
            // iterate active DriverPacks
            Collection *col=manager_g->matcher->getCol();
            for(Driverpack &drp:*col->getList())
            {
                // network DriverPacks
                std::wstring filename=drp.getFilename();
                if(StrStrIW(filename.c_str(),L"_LAN_")||
                   StrStrIW(filename.c_str(),L"_WLAN-WiFi_")||
                   StrStrIW(filename.c_str(),L"_WWAN-4G_"))
                {
                    // DriverPack
                    SourceFileName=drp.getPath();
                    SourceFileName.append(L"\\"+(std::wstring)drp.getFilename());
                    TargetFileName=targetDrive;
                    TargetFileName.append(L"drivers\\"+(std::wstring)drp.getFilename());
                    USBWiz->AddFile(SourceFileName,TargetFileName);

                    // index
                    wchar_t indexname[BUFLEN];
                    drp.getindexfilename(col->getIndex_bin_dir(),L"bin",indexname);
                    SourceFileName=indexname;
                    TargetFileName=targetDrive+(std::wstring)indexname;
                    USBWiz->AddFile(SourceFileName,TargetFileName);
                }
            }
            break;
        }

        // selected DriverPacks and indexes
        case 2:
        {
            // iterate active DriverPacks
            Collection *col=manager_g->matcher->getCol();
            for(Driverpack &drp:*col->getList())
            {
                // get selected DriverPacks
                std::wstring filename=drp.getFilename();
                if(manager_g->isSelected(filename.c_str()))
                {
                    // DriverPack
                    SourceFileName=drp.getPath();
                    SourceFileName.append(L"\\"+filename);
                    TargetFileName=targetDrive;
                    TargetFileName.append(L"drivers\\"+filename);
                    USBWiz->AddFile(SourceFileName,TargetFileName);

                    // index
                    wchar_t indexname[BUFLEN];
                    drp.getindexfilename(col->getIndex_bin_dir(),L"bin",indexname);
                    SourceFileName=(std::wstring)indexname;
                    TargetFileName=targetDrive+(std::wstring)indexname;
                    USBWiz->AddFile(SourceFileName,TargetFileName);
                }
            }
            break;
        }

        default:
            break;
    }

    // additional driver path
    if((USBWiz->AdditionalPath.size()>0)&&System.DirectoryExists(USBWiz->AdditionalPath.c_str()))
    {
        // get the directory name to be included in the copy
        std::wstring d(USBWiz->AdditionalPath);
        size_t found=d.find_last_of(L"/\\");
        if(found!=std::wstring::npos)
            d.erase(0,found+1);
        USBWiz->AddDirectory(USBWiz->AdditionalPath,L"\\drivers\\"+d);
    }

    // online indexes
    if(USBWiz->IncludeOnlineIndexes)
    {
        Collection *col=manager_g->matcher->getCol();
        std::wstring spec(col->getIndex_bin_dir());
        spec.append(L"\\_*.bin");

        WIN32_FIND_DATAW FindFileData;
        HANDLE hFind=FindFirstFileW(spec.c_str(),&FindFileData);
        if(hFind!=INVALID_HANDLE_VALUE)
        {
            do
            {
                if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)
                {
                    SourceFileName=col->getIndex_bin_dir();
                    SourceFileName.append(L"\\");
                    SourceFileName.append(FindFileData.cFileName);
                    TargetFileName=targetDrive;
                    TargetFileName.append(L"indexes\\SDI\\");
                    TargetFileName.append(FindFileData.cFileName);
                    USBWiz->AddFile(SourceFileName,TargetFileName);
                }
            }
            while(FindNextFile(hFind,&FindFileData)!=0);
            FindClose(hFind);
        }
    }

    // languages
    switch(USBWiz->Languages)
    {
        // all languages
        case 0:
            {
                std::wstring spec(Settings.data_dir);
                spec.append(L"\\langs");
                USBWiz->AddDirectory(spec,spec);
                break;
            }

        // current selected language
        case 1:
            {
                // get current language from combo and look up it's file name
                int j=SendMessage(GetDlgItem(MainWindow.hMain,ID_LANG),CB_GETCURSEL,0,0);
                std::wstring filename=vLang->GetFileName(j);
                SourceFileName=filename;
                size_t found=filename.find_last_of(L"/\\");
                if(found!=std::wstring::npos)
                    filename=filename.substr(found+1);
                TargetFileName=targetDrive;
                TargetFileName.append(L"tools\\SDI\\langs\\"+filename);
                USBWiz->AddFile(SourceFileName,TargetFileName);
                break;
            }
        // default language
        case 2:
            break;
        default:
            break;
    }

    // themes
    switch(USBWiz->Themes)
    {
        // all themes
        case 0:
        {
            std::wstring dir(Settings.data_dir);
            dir.append(L"\\themes");
            USBWiz->AddDirectory(dir,dir);
            break;
        }
        // current selected theme
        case 1:
        {
            // get the file name
            std::wstring filename=vTheme->GetFileName(Settings.curtheme);
            SourceFileName=filename;

            size_t found=filename.find_last_of(L"/\\");
            if(found!=std::wstring::npos)
                filename=filename.substr(found+1);
            TargetFileName=targetDrive;
            TargetFileName.append(L"tools\\SDI\\themes\\"+filename);
            USBWiz->AddFile(SourceFileName,TargetFileName);
            // copy the theme folder
            filename=vTheme->GetFileName(Settings.curtheme);
            found=filename.find_last_of(L".");
            if(found!=std::wstring::npos)
                filename=filename.substr(0,found);
            USBWiz->AddDirectory(filename,filename);
            break;
        }
        // default theme
        case 2:
            break;
        default:
            break;
    }

    // both executables
    int ver=System.FindLatestExeVersion();
    std::wstring exe32=L"SDI_R"+std::to_wstring(ver)+L".exe";
    std::wstring exe64=L"SDI_x64_R"+std::to_wstring(ver)+L".exe";
    USBWiz->AddFile(exe32,targetDrive+exe32);
    USBWiz->AddFile(exe64,targetDrive+exe64);
    __int64 ExecutableSize=System.FileSize(exe32.c_str())+System.FileSize(exe64.c_str());

    wchar_t wTempPath[MAX_PATH] = {0};
    GetTempPath(MAX_PATH, wTempPath);

    // configuration file
    // most things are default and so don't need to be defined
    SourceFileName=wTempPath+(std::wstring)L"\\sdi.cfg";
    FILE *f=_wfopen(SourceFileName.c_str(),L"wt");
    if(f)
    {
        fwprintf(f,L"\"-lang:%ws\"\n",STR(STR_LANG_ID));
        fwprintf(f,L"-license:%d\n",Settings.license);
        if(USBWiz->Themes==0||USBWiz->Themes==1)
            fwprintf(f,L"\"-theme:%ws\"\n",Settings.curtheme);

        if(USBWiz->ExpertMode)fwprintf(f,L"-expertmode ");
        if(!USBWiz->NoUpdates)fwprintf(f,L"-checkupdates ");
        if(USBWiz->NoSnapshots)fwprintf(f,L"-nosnapshot ");
        if(USBWiz->NoLogs)fwprintf(f,L"-nologfile ");
        if(Settings.flags&FLAG_SHOWCONSOLE)fwprintf(f,L"-showconsole ");
        if(Settings.flags&FLAG_HIDEPATREON)fwprintf(f,L"-hidepatreon ");
        if(Settings.flags&FLAG_SHOWDRPNAMES1)fwprintf(f,L"-showdrpnames1 ");
        if(Settings.flags&FLAG_SHOWDRPNAMES2)fwprintf(f,L"-showdrpnames2 ");
        fclose(f);

        TargetFileName=targetDrive+(std::wstring)L"sdi.cfg";
        USBWiz->AddFile(SourceFileName,TargetFileName);
    }

    // auto files
    if(USBWiz->IncludeAutoFiles)
    {
        // create the autorun.inf file locally then copy it
        // create it in the user temp directory
        SourceFileName=wTempPath;
        SourceFileName.append(L"\\autorun.inf");
        f=_wfopen(SourceFileName.c_str(),L"wt");
        if(f)
        {
            fwprintf(f,L"[autorun]\n");
            fwprintf(f,L"open=SDI_auto.bat\n");
            fwprintf(f,L"icon=%ws\n\n",exe32.c_str());
            fwprintf(f,L"[NOT_A_VIRUS]\n");
            fclose(f);
        }
        TargetFileName=targetDrive;
        TargetFileName.append(L"autorun.inf");
        USBWiz->AddFile(SourceFileName,TargetFileName);
        // SDI_auto.bat
        TargetFileName=targetDrive;
        TargetFileName.append(L"SDI_auto.bat");
        USBWiz->AddFile(L"SDI_auto.bat",TargetFileName);
    }

    // update all wizard text
    std::wstring buf1;
    std::wstring buf2=std::to_wstring(USBWiz->BytesAvailable/1024/1024) + L" MB";
    std::wstring buf3=std::to_wstring(ExecutableSize/1024/1024) + L" MB";

    if(USBWiz->BytesRequired/1024/1024<1)
        buf1=std::to_wstring(USBWiz->BytesRequired/1024) + L" KB";
    else
        buf1=std::to_wstring(USBWiz->BytesRequired/1024/1024) + L" MB";

    // page 2
    SetDlgItemText(hwnd,IDC_USBWIZ_PAGE2_SPACEREQ_VAL,buf1.c_str());
    SetDlgItemText(hwnd,IDC_USBWIZ_PAGE2_SPACEAVAIL_VAL,buf2.c_str());
    // page 3
    SetDlgItemText(hwnd,IDC_USBWIZ_PAGE3_SPACEREQ_VAL,buf1.c_str());
    SetDlgItemText(hwnd,IDC_USBWIZ_PAGE3_SPACEAVAIL_VAL,buf2.c_str());
    // page 4
    SetDlgItemText(hwnd,IDC_USBWIZ_PAGE4_SPACEREQ_VAL,buf1.c_str());
    SetDlgItemText(hwnd,IDC_USBWIZ_PAGE4_SPACEAVAIL_VAL,buf2.c_str());
    SetDlgItemText(hwnd,IDC_USBWIZ_PAGE4_EXESIZE_VAL,buf3.c_str());

    InvalidateRect(hwnd,nullptr,TRUE);
    USBWiz->pathChanged=false;
    SetCursor(LoadCursor(nullptr,IDC_ARROW));
}

static LRESULT CALLBACK Page2DlgProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            // control text
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_SPACEREQ),STR(STR_USBWIZ_SPACEREQ));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_SPACEAVAIL),STR(STR_USBWIZ_SPACEAVAIL));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_DESC),STR(STR_USBWIZ_PAGE2_DESC));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_DRIVE),STR(STR_USBWIZ_PAGE2_DRIVE));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_MORE),STR(STR_USBWIZ_PAGE2_MORE));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_CLEAR),STR(STR_USBWIZ_PAGE2_CLEAR));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_DELETE),STR(STR_USBWIZ_PAGE2_DELETE));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_DELETEDESC),STR(STR_USBWIZ_PAGE2_DELETEDESC));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_FORMAT),STR(STR_USBWIZ_PAGE2_FORMAT));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_FORMATDESC),STR(STR_USBWIZ_PAGE2_FORMATDESC));
            Page2PopulateCombo(hwnd);
            return TRUE;
        }
        case WM_COMMAND:
            {
                DWORD cid=LOWORD(wParam);
                DWORD ntc=HIWORD(wParam);
                HWND hCtl=(HWND)lParam;
                // target drive combo item selected
                if((cid==IDC_USBWIZ_PAGE2_COMBO)&&(ntc==CBN_SELCHANGE))
                {
                    int sel=ComboBox_GetCurSel(hCtl);
                    int d=ComboBox_GetItemData(hCtl,sel);
                    USBWiz->TargetDrive=d;
                    BuildFilesList(hwnd);
                    Page2EnableButtons(hwnd);
                }
                // delete files button
                else if((cid==IDC_USBWIZ_PAGE2_DELETE)&&(ntc==BN_CLICKED))
                {
                    wchar_t targetDrive[] = L"A:";
                    targetDrive[0]=USBWiz->TargetDrive;
                    std::wstring b=STR(STR_USBWIZ_PAGE2_DELCONF1);
                    b.append(L" "+(std::wstring)targetDrive+L"\n\n");
                    b.append(STR(STR_USBWIZ_PAGE2_DELCONF2));
                    if(MessageBox(hwnd,b.c_str(),STR(STR_USBWIZ_PAGE2_DELCONF),MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)==IDYES)
                    {
                        SetCursor(LoadCursor(ghInst,IDC_WAIT));
                        USBWiz->ClearTarget(hwnd);
                        BuildFilesList(hwnd);
                        Page2PopulateCombo(hwnd);
                    }
                }
                // quick format button
                else if((cid==IDC_USBWIZ_PAGE2_FORMAT)&&(ntc==BN_CLICKED))
                {
                    USBWiz->QuickFormatTarget(hwnd);
                    BuildFilesList(hwnd);
                    Page2PopulateCombo(hwnd);
                }
                // all drives checkbox
                else if((cid==IDC_USBWIZ_PAGE2_MORE)&&(ntc==BN_CLICKED))
                {
                    USBWiz->ShowAllDrives=IsDlgButtonChecked(hwnd,IDC_USBWIZ_PAGE2_MORE);
                    Page2PopulateCombo(hwnd);
                    BuildFilesList(hwnd);
                }
            }
            break;
        case WM_NOTIFY:
            {
                LPNMHDR pnmh = (LPNMHDR)lParam;
                if(pnmh->code)
                    switch(pnmh->code)
                    {
                        case PSN_SETACTIVE:
                            BuildFilesList(hwnd);
                            Page2EnableButtons(hwnd);
                            break;
                        case PSN_WIZFINISH:
                            break;
                        case PSN_WIZBACK:
                            break;
                        case PSN_RESET:
                            break;
                        default:
                            break;
                    }
                break;
            }
            break;

        case WM_DEVICECHANGE:
            {
                Page2PopulateCombo(hwnd);
                BuildFilesList(hwnd);
                break;
            }

        case WM_CTLCOLORSTATIC:
            {
                // space required goes red if too big
                HDC hdc=(HDC)wParam;
                HWND hsc=(HWND)lParam;
                if(((hsc==GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_SPACEREQ))||
                   (hsc==GetDlgItem(hwnd,IDC_USBWIZ_PAGE2_SPACEREQ_VAL)))&&
                   (USBWiz->BytesRequired>=USBWiz->BytesAvailable))
                {
                    SetTextColor(hdc, RGB(255,0,0));
                    SetBkMode(hdc,TRANSPARENT);
                    return (LRESULT)GetStockObject(HOLLOW_BRUSH);
                }
                break;
            }

        default:
            break;
    }
    return FALSE;
}

static LRESULT CALLBACK Page3DlgProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            if(USBWiz->DriverPackOption==2)
                SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE3_SELECTED,BM_SETCHECK,BST_CHECKED,0);
            else
                SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE3_ALLPACKS,BM_SETCHECK,BST_CHECKED,0);

            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_SPACEREQ),STR(STR_USBWIZ_SPACEREQ));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_SPACEAVAIL),STR(STR_USBWIZ_SPACEAVAIL));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_SELECT),STR(STR_USBWIZ_PAGE3_SELECT));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_ALLPACKS),STR(STR_USBWIZ_PAGE3_ALLPACKS));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_NETWORK),STR(STR_USBWIZ_PAGE3_NETWORK));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_SELECTED),STR(STR_USBWIZ_PAGE3_SELECTED));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_NOPACKS),STR(STR_USBWIZ_PAGE3_NOPACKS));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_ADDPATH),STR(STR_USBWIZ_PAGE3_ADDPATH));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_INDEXES),STR(STR_USBWIZ_PAGE3_INDEXES));
            return TRUE;
        }
        case WM_COMMAND:
            {
                DWORD cid=LOWORD(wParam);
                DWORD ntc=HIWORD(wParam);
//                HWND hCtl=(HWND)lParam;
             // radio buttons and checkboxes
                if((cid==IDC_USBWIZ_PAGE3_ALLPACKS)&&(ntc==BN_CLICKED)&&SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_ALLPACKS),BM_GETCHECK,0,0))
                {
                    USBWiz->DriverPackOption=0;
                    BuildFilesList(hwnd);
                }
                else if((cid==IDC_USBWIZ_PAGE3_NETWORK)&&(ntc==BN_CLICKED)&&SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_NETWORK),BM_GETCHECK,0,0))
                {
                    USBWiz->DriverPackOption=1;
                    BuildFilesList(hwnd);
                }
                else if((cid==IDC_USBWIZ_PAGE3_SELECTED)&&(ntc==BN_CLICKED)&&SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_SELECTED),BM_GETCHECK,0,0))
                {
                    USBWiz->DriverPackOption=2;
                    BuildFilesList(hwnd);
                }
                else if((cid==IDC_USBWIZ_PAGE3_NOPACKS)&&(ntc==BN_CLICKED)&&SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_NOPACKS),BM_GETCHECK,0,0))
                {
                    USBWiz->DriverPackOption=3;
                    BuildFilesList(hwnd);
                }
                else if(cid==IDC_USBWIZ_PAGE3_INDEXES)
                {
                    USBWiz->IncludeOnlineIndexes=SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_INDEXES),BM_GETCHECK,0,0);
                    BuildFilesList(hwnd);
                }
                else if((cid==IDC_USBWIZ_PAGE3_PATHEDIT)&&(ntc==EN_CHANGE))
                {
                    wchar_t buf[BUFSIZ];
                    SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_PATHEDIT),WM_GETTEXT,BUFSIZ,LPARAM(buf));
                    USBWiz->AdditionalPath=buf;
                    USBWiz->pathChanged=true;
                }
                else if((cid==IDC_USBWIZ_PAGE3_PATHBUTTON)&&(ntc==BN_CLICKED))
                {
                    wchar_t path[BUFSIZ];
                    if(USBWiz->AdditionalPath.size()>0)
                        wcscpy(path,USBWiz->AdditionalPath.c_str());
                    else
                        wcscpy(path,System.AppPathW().c_str());
                    if(System.ChooseDir(path,L"Select Additional Path"))
                    {
                        SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_PATHEDIT),WM_SETTEXT,0,LPARAM(path));
                        BuildFilesList(hwnd);
                    }
                }

            }
            break;

        case WM_NOTIFY:
            {
                LPNMHDR pnmh = (LPNMHDR)lParam;
                switch(pnmh->code)
                {
                    case PSN_SETACTIVE:
                        BuildFilesList(hwnd);
                        PropSheet_SetWizButtons(GetParent(hwnd), PSWIZB_BACK|PSWIZB_NEXT);
                        break;
                    case PSN_WIZFINISH:
                        break;
                    case PSN_WIZNEXT:
                        {
                            if(USBWiz->pathChanged)
                                BuildFilesList(hwnd);
                            break;
                        }
                    case PSN_WIZBACK:
                        break;
                    case PSN_RESET:
                        break;
                    default:
                        break;
                }
                break;
            }
            break;

        case WM_CTLCOLORSTATIC:
            {
                // space required goes red if too big
                HDC hdc=(HDC)wParam;
                HWND hsc=(HWND)lParam;
                if(((hsc==GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_SPACEREQ))||
                   (hsc==GetDlgItem(hwnd,IDC_USBWIZ_PAGE3_SPACEREQ_VAL)))&&
                   (USBWiz->BytesRequired>=USBWiz->BytesAvailable))
                {
                    SetTextColor(hdc, RGB(255,0,0));
                    SetBkMode(hdc,TRANSPARENT);
                    return (LRESULT)GetStockObject(HOLLOW_BRUSH);
                }
                break;
            }

        default:
            break;
    }
    return FALSE;
}

static LRESULT CALLBACK Page4DlgProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE4_ALLLANG,BM_SETCHECK,BST_CHECKED,0);
            SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE4_ALLTHEME,BM_SETCHECK,BST_CHECKED,0);
            SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE4_EXPERT,BM_SETCHECK,USBWiz->ExpertMode,0);
            SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE4_NOUPD,BM_SETCHECK,USBWiz->NoUpdates,0);
            SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE4_NOSNAP,BM_SETCHECK,USBWiz->NoSnapshots,0);
            SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE4_NOLOG,BM_SETCHECK,USBWiz->NoLogs,0);
            SendDlgItemMessage(hwnd,IDC_USBWIZ_PAGE4_INCAUTO,BM_SETCHECK,USBWiz->IncludeAutoFiles,0);
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_LANGS),STR(STR_USBWIZ_PAGE4_LANGS));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_ALLLANG),STR(STR_USBWIZ_PAGE4_ALLLANG));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_CURLANG),STR(STR_USBWIZ_PAGE4_CURLANG));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_THEMES),STR(STR_USBWIZ_PAGE4_THEMES));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_ALLTHEME),STR(STR_USBWIZ_PAGE4_ALLTHEME));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_CURTHEME),STR(STR_USBWIZ_PAGE4_CURTHEME));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_DEFTHEME),STR(STR_USBWIZ_PAGE4_DEFTHEME));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_CONFIG),STR(STR_USBWIZ_PAGE4_CONFIG));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_EXPERT),STR(STR_USBWIZ_PAGE4_EXPERT));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_NOUPD),STR(STR_USBWIZ_PAGE4_NOUPD));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_NOSNAP),STR(STR_USBWIZ_PAGE4_NOSNAP));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_NOLOG),STR(STR_USBWIZ_PAGE4_NOLOG));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_INCAUTO),STR(STR_USBWIZ_PAGE4_INCAUTO));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_SPACEREQ),STR(STR_USBWIZ_SPACEREQ));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_SPACEAVAIL),STR(STR_USBWIZ_SPACEAVAIL));
            SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_EXESIZE),STR(STR_USBWIZ_PAGE4_EXESIZE));
            return TRUE;
        }
        case WM_COMMAND:
            {
                DWORD cid=LOWORD(wParam);
                DWORD ntc=HIWORD(wParam);
//                HWND hCtl=(HWND)lParam;
                // radio buttons and checkboxes
                if((cid==IDC_USBWIZ_PAGE4_ALLLANG)&&(ntc==BN_CLICKED))
                    USBWiz->Languages=0;
                else if((cid==IDC_USBWIZ_PAGE4_CURLANG)&&(ntc==BN_CLICKED))
                    USBWiz->Languages=1;
                else if((cid==IDC_USBWIZ_PAGE4_ALLTHEME)&&(ntc==BN_CLICKED))
                    USBWiz->Themes=0;
                else if((cid==IDC_USBWIZ_PAGE4_CURTHEME)&&(ntc==BN_CLICKED))
                    USBWiz->Themes=1;
                else if((cid==IDC_USBWIZ_PAGE4_DEFTHEME)&&(ntc==BN_CLICKED))
                    USBWiz->Themes=2;
                else if((cid==IDC_USBWIZ_PAGE4_EXPERT)&&(ntc==BN_CLICKED))
                    USBWiz->ExpertMode=SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_EXPERT),BM_GETCHECK,0,0);
                else if((cid==IDC_USBWIZ_PAGE4_NOUPD)&&(ntc==BN_CLICKED))
                    USBWiz->NoUpdates=SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_NOUPD),BM_GETCHECK,0,0);
                else if((cid==IDC_USBWIZ_PAGE4_NOSNAP)&&(ntc==BN_CLICKED))
                    USBWiz->NoSnapshots=SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_NOSNAP),BM_GETCHECK,0,0);
                else if((cid==IDC_USBWIZ_PAGE4_NOLOG)&&(ntc==BN_CLICKED))
                    USBWiz->NoLogs=SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_NOLOG),BM_GETCHECK,0,0);
                else if((cid==IDC_USBWIZ_PAGE4_INCAUTO)&&(ntc==BN_CLICKED))
                    USBWiz->IncludeAutoFiles=SendMessage(GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_INCAUTO),BM_GETCHECK,0,0);
                BuildFilesList(hwnd);
            }
            break;

        case WM_NOTIFY:
            {
                LPNMHDR pnmh = (LPNMHDR)lParam;
                switch(pnmh->code)
                {
                    case PSN_SETACTIVE:
                        BuildFilesList(hwnd);
                        if(USBWiz->BytesRequired>=USBWiz->BytesAvailable)
                            PropSheet_SetWizButtons(GetParent(hwnd), PSWIZB_BACK);
                        else
                            PropSheet_SetWizButtons(GetParent(hwnd), PSWIZB_BACK|PSWIZB_NEXT);
                        break;
                    case PSN_WIZFINISH:
                        break;
                    case PSN_WIZBACK:
                        break;
                    case PSN_RESET:
                        break;
                    default:
                        break;
                }
                break;
            }
            break;

        case WM_CTLCOLORSTATIC:
            {
                // space required goes red if too big
                HDC hdc=(HDC)wParam;
                HWND hsc=(HWND)lParam;
                if(((hsc==GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_SPACEREQ))||
                   (hsc==GetDlgItem(hwnd,IDC_USBWIZ_PAGE4_SPACEREQ_VAL)))&&
                   (USBWiz->BytesRequired>=USBWiz->BytesAvailable))
                {
                    SetTextColor(hdc, RGB(255,0,0));
                    SetBkMode(hdc,TRANSPARENT);
                    return (LRESULT)GetStockObject(HOLLOW_BRUSH);
                }
                break;
            }

        default:
            break;
    }
    return FALSE;
}

static LRESULT CALLBACK Page5DlgProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
            {
                // Create static text
                HWND hStaticText=CreateWindowEx(WS_EX_TRANSPARENT,
                                                L"Static",
                                                STR(STR_USBWIZ_PAGE5_TITLE),
                                                WS_CHILD|WS_VISIBLE|SS_LEFT,
                                                210,20,640,26,
                                                hwnd,nullptr,ghInst,nullptr);
                if(hStaticText==nullptr)return true;
                // create a font for the static control
                HFONT hFont=CreateFont(22,0,0,0,650,
                                       FALSE,FALSE,FALSE,
                                       ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                       ANTIALIASED_QUALITY,DEFAULT_PITCH,
                                       L"MS ShellDlg");
                // Set the new font for the control
                SendMessage(hStaticText, WM_SETFONT, WPARAM (hFont), TRUE);
                // control text
                SetWindowText(GetDlgItem(hwnd,IDC_USBWIZ_PAGE5_DESC),STR(STR_USBWIZ_PAGE5_DESC));
            }
            return TRUE;

        case WM_NOTIFY:
            {
                LPNMHDR pnmh = (LPNMHDR)lParam;
                switch(pnmh->code)
                {
                    case PSN_SETACTIVE:
                        PropSheet_SetWizButtons(GetParent(hwnd), PSWIZB_BACK|PSWIZB_FINISH);
                        break;
                    case PSN_WIZFINISH:
                        return false;
                    case PSN_WIZBACK:
                        break;
                    case PSN_RESET:
                        break;
                    default:
                        break;
                }
                break;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

// http://www.jose.it-berater.org/comctrl/propsheet/propsheetpage.htm

USBWizard::USBWizard()
{
    // initialise the common controls
    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize=sizeof(INITCOMMONCONTROLSEX);
    iccx.dwICC=ICC_COOL_CLASSES|ICC_NATIVEFNTCTL_CLASS|ICC_STANDARD_CLASSES|ICC_TAB_CLASSES|ICC_USEREX_CLASSES|ICC_WIN95_CLASSES;
    InitCommonControlsEx(&iccx);

    if(DriversSelected())
        DriverPackOption=2;
    else
        DriverPackOption=0;
}

bool USBWizard::doWizard()
{
    psp[0].dwSize=sizeof(PROPSHEETPAGE);
    psp[0].dwFlags=PSP_HIDEHEADER|PSP_USETITLE;
    psp[0].hInstance=ghInst;
    psp[0].pszTemplate=MAKEINTRESOURCE(IDD_USBWIZ_PAGE1);
    psp[0].lParam=0;
    psp[0].pszTitle=STR(STR_USBWIZ_TITLE);
    psp[0].pfnDlgProc=(DLGPROC)Page1DlgProc;

    psp[1].dwSize=sizeof(PROPSHEETPAGE);
    psp[1].dwFlags=PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp[1].hInstance=ghInst;
    psp[1].pszTemplate=MAKEINTRESOURCE(IDD_USBWIZ_PAGE2);
    psp[1].pszHeaderTitle=STR(STR_USBWIZ_PAGE2_TITLE);
    psp[1].pszHeaderSubTitle=STR(STR_USBWIZ_PAGE2_SUBTITLE);
    psp[1].lParam=0;
    psp[1].pfnDlgProc=(DLGPROC)Page2DlgProc;

    psp[2].dwSize=sizeof(PROPSHEETPAGE);
    psp[2].dwFlags=PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp[2].hInstance=ghInst;
    psp[2].pszTemplate=MAKEINTRESOURCE(IDD_USBWIZ_PAGE3);
    psp[2].pszHeaderTitle=STR(STR_USBWIZ_PAGE3_TITLE);
    psp[2].pszHeaderSubTitle=STR(STR_USBWIZ_PAGE3_SUBTITLE);
    psp[2].lParam=0;
    psp[2].pfnDlgProc=(DLGPROC)Page3DlgProc;

    psp[3].dwSize=sizeof(PROPSHEETPAGE);
    psp[3].dwFlags=PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    psp[3].hInstance=ghInst;
    psp[3].pszTemplate=MAKEINTRESOURCE(IDD_USBWIZ_PAGE4);
    psp[3].pszHeaderTitle=STR(STR_USBWIZ_PAGE4_TITLE);
    psp[3].pszHeaderSubTitle=STR(STR_USBWIZ_PAGE4_SUBTITLE);
    psp[3].lParam=0;
    psp[3].pfnDlgProc=(DLGPROC)Page4DlgProc;

    psp[4].dwSize=sizeof(PROPSHEETPAGE);
    psp[4].dwFlags=PSP_HIDEHEADER;
    psp[4].hInstance=ghInst;
    psp[4].pszTemplate=MAKEINTRESOURCE(IDD_USBWIZ_PAGE5);
    psp[4].pszHeaderTitle=STR(STR_USBWIZ_PAGE5_TITLE);
    psp[4].lParam=0;
    psp[4].pfnDlgProc=(DLGPROC)Page5DlgProc;

    psh.dwSize=sizeof(PROPSHEETHEADER);
    psh.dwFlags=PSH_PROPSHEETPAGE|PSH_WIZARD97|PSH_USEICONID|PSH_WATERMARK;
    psh.hInstance=ghInst;
    psh.hwndParent=MainWindow.hMain;
    psh.nPages=5;
    psh.nStartPage=0;
    psh.pszIcon=MAKEINTRESOURCE(IDI_ICON1);
    psh.pszbmWatermark=MAKEINTRESOURCE(IDB_WATERMARK);
    psh.ppsp=(LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback=nullptr;

    INT_PTR err=PropertySheet(&psh);
    switch(err)
    {
        case -1:
            MessageBox(MainWindow.hMain, L"An error occurred.",L"Error", MB_ICONERROR|MB_OK);
            return false;
        case 0:
            return false;
        case 1:
            {
                BuildDrive(MainWindow.hMain);
                return true;
            }
        default:
            return false;
    }
}

void USBWizard::ClearTarget(HWND hwnd)
{
    if(TargetDrive>0)
    {
        // the progress dialog
        CoInitialize(nullptr);
        HRESULT hr=CoCreateInstance(CLSID_ProgressDialog,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_IProgressDialog,
                                    (void **)&ipd);
        if(SUCCEEDED(hr))
        {
            // initialise the progress dialog
            ipd->SetTitle(STR(STR_USBWIZ_PAGE2_DELCONF));
            ipd->SetLine(1,STR(STR_USBWIZ_PROGR_DELETING),FALSE,nullptr);
            hr=ipd->StartProgressDialog(hwnd,
                                        nullptr,
                                        PROGDLG_NORMAL|PROGDLG_NOPROGRESSBAR,
                                        nullptr);
            if(SUCCEEDED(hr))
            {
                wchar_t myDrives[]=L"A:\\";
                myDrives[0]=TargetDrive;
                ClearDirectory(myDrives);
                ipd->StopProgressDialog();
            }
        }
    }
}

void USBWizard::ClearDirectory(const std::wstring directory)
{
    // assumes a full path and no file spec
    std::wstring cwd(directory);
    // add missing backslash
    if(cwd.back()!=L'\\')
        cwd+=L'\\';
    std::wstring spec(cwd);
    spec+=L"*.*";

    WIN32_FIND_DATA FindFileData;
    HANDLE hFind=FindFirstFile(spec.c_str(),&FindFileData);

    if(hFind==INVALID_HANDLE_VALUE)
        return;

    do
    {
        if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_SYSTEM)==0)
        {
            if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
            {
                if((_wcsicmp(FindFileData.cFileName,L".")!=0)&&(_wcsicmp(FindFileData.cFileName,L"..")!=0))
                {
                    spec=cwd+(std::wstring)FindFileData.cFileName;
                    // i will try 3 times to clear the directory before moving on
                    int errcount=0;
                    DWORD err;
                    do
                    {
                        ClearDirectory(spec);
                        err=RemoveDirectory(spec.c_str());
                        // return value of 0 means error
                        if(err==0)
                        {
                            errcount++;
                            Sleep(1000);
                        }
                    }while((err==0)&&(errcount<3));
                    if(err==0)
                    {
                        int LastError=GetLastError();
                        std::wstring b=L"Error " + std::to_wstring(LastError) +
                                       L" removing directory " + spec;
                        MessageBox(nullptr, b.c_str(), L"Error", MB_ICONERROR|MB_OK);
                    }
                }
            } // dir
            else
            {
                spec=cwd+(std::wstring)FindFileData.cFileName;
                ipd->SetLine(2,spec.c_str(),TRUE,nullptr);
                SetFileAttributes(spec.c_str(),FILE_ATTRIBUTE_NORMAL);
                DeleteFile(spec.c_str());
            }
        }

    } while(FindNextFile(hFind,&FindFileData));

    FindClose(hFind);

}

void USBWizard::QuickFormatTarget(HWND parent)
{
    if(TargetDrive>0)
        SHFormatDrive(parent,TargetDrive-65,SHFMT_OPT_FULL,0);
}

static DWORD CALLBACK CopyProgressRoutine(
  _In_     LARGE_INTEGER TotalFileSize,
  _In_     LARGE_INTEGER TotalBytesTransferred,
  _In_     LARGE_INTEGER StreamSize,
  _In_     LARGE_INTEGER StreamBytesTransferred,
  _In_     DWORD         dwStreamNumber,
  _In_     DWORD         dwCallbackReason,
  _In_     HANDLE        hSourceFile,
  _In_     HANDLE        hDestinationFile,
  _In_opt_ LPVOID        lpData
)
{
//    UNREFERENCED_PARAMETER(TotalFileSize);
    UNREFERENCED_PARAMETER(StreamSize);
    UNREFERENCED_PARAMETER(StreamBytesTransferred);
    UNREFERENCED_PARAMETER(dwStreamNumber);
    UNREFERENCED_PARAMETER(dwCallbackReason);
    UNREFERENCED_PARAMETER(hSourceFile);
    UNREFERENCED_PARAMETER(hDestinationFile);
    UNREFERENCED_PARAMETER(lpData);
    USBWiz->SetProgress(TotalBytesTransferred.QuadPart,TotalFileSize.QuadPart);
    return PROGRESS_CONTINUE;
}

void USBWizard::SetProgress(__int64 chunk,__int64 TotalFileSize)
{
    std::wstring b(SourceList[CurrentFile]);
    std::wstring bytes;
    if(TotalFileSize<1024)bytes=std::to_wstring(TotalFileSize)+L" bytes";
    else if(TotalFileSize<1024*1024)bytes=std::to_wstring(TotalFileSize/1024)+L"KB";
    else bytes=std::to_wstring(TotalFileSize/1024/1024)+L"MB";

    b.append(L" ("+bytes+L")");
    ipd->SetLine(2,b.c_str(),TRUE,nullptr);
    ipd->SetProgress64(BytesCopied+chunk,BytesRequired);
}

void USBWizard::ClearBuildList()
{
    SourceList.clear();
    TargetList.clear();
    BytesAvailable=0;
    BytesCopied=0;
    BytesRequired=0;
}

void USBWizard::BuildDrive(HWND hwnd)
{
    // the progress dialog
    CoInitialize(nullptr);
    HRESULT hr=CoCreateInstance(CLSID_ProgressDialog,
                                 nullptr,
                                 CLSCTX_INPROC_SERVER,
                                 IID_IProgressDialog,
                                 (void **)&ipd);
    if(SUCCEEDED(hr))
    {
        // initialise the progress dialog
        ipd->SetTitle(STR(STR_USBWIZ_PROGR_TITLE));
        ipd->SetLine(1,STR(STR_USBWIZ_PROGR_COPYING),FALSE,nullptr);
        hr=ipd->StartProgressDialog(hwnd,
                                    nullptr,
                                    PROGDLG_NORMAL|PROGDLG_AUTOTIME,
                                    nullptr);

        if(SUCCEEDED(hr))
        {
            // copy the files
            DWORD err=0;
            for(CurrentFile=0;CurrentFile<SourceList.size();CurrentFile++)
            {
                // create destination directory
                std::wstring path=TargetList[CurrentFile].substr(0,TargetList[CurrentFile].find_last_of(L"/\\"));
                mkdir_r(path.c_str());
                // copy the file
                if(CopyFileEx(SourceList[CurrentFile].c_str(),
                              TargetList[CurrentFile].c_str(),
                              (LPPROGRESS_ROUTINE)CopyProgressRoutine,
                              nullptr,nullptr,0)==0)
                {
                    err=GetLastError();
                    break;
                }
                BytesCopied=BytesCopied+System.FileSize(SourceList[CurrentFile].c_str());
                if(ipd->HasUserCancelled())
                    break;
            }

            ipd->StopProgressDialog();
            if(err)
            {
                std::wstring b=L"Copy failed with error ";
                b.append(std::to_wstring(err)+L".");
                MessageBox(MainWindow.hMain,b.c_str(),L"Create USB Drive",MB_ICONERROR|MB_OK);
            }
        }
    }
}

void USBWizard::AddFile(std::wstring source,std::wstring dest)
{
    if(System.FileExists2(source.c_str()))
    {
        SourceList.push_back(source);
        TargetList.push_back(dest);
        BytesRequired=BytesRequired+System.FileSize(source.c_str());
    }
}

void USBWizard::AddDirectory(std::wstring dir,std::wstring targetPath)
{
    // recurse the given path
    std::wstring spec(dir);
    spec.append(L"\\*.*");
    wchar_t targetDrive[] = L"A:\\";
    targetDrive[0]=TargetDrive;

    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind=FindFirstFileW(spec.c_str(),&FindFileData);
    if(hFind==INVALID_HANDLE_VALUE)return;

    do
    {
        if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
        {
            if((_wcsicmp(FindFileData.cFileName,L".")!=0)&&(_wcsicmp(FindFileData.cFileName,L"..")!=0))
                AddDirectory(dir+L"\\"+FindFileData.cFileName,targetPath+L"\\"+FindFileData.cFileName);
        }
        else
        {
            std::wstring SourceFileName(dir);
            SourceFileName.append(L"\\"+(std::wstring)FindFileData.cFileName);
            SourceList.push_back(SourceFileName);

            ULARGE_INTEGER ul;
            ul.HighPart=FindFileData.nFileSizeHigh;
            ul.LowPart=FindFileData.nFileSizeLow;
            BytesRequired=BytesRequired+ul.QuadPart;

            std::wstring TargetFileName=targetDrive;
            TargetFileName.append(targetPath+L"\\"+FindFileData.cFileName);
            while(TargetFileName.find(L"\\\\")!=std::wstring::npos)
                TargetFileName.erase(TargetFileName.find(L"\\\\"),1);
            TargetList.push_back(TargetFileName);
        }
    }
    while(FindNextFile(hFind,&FindFileData)!=0);
    FindClose(hFind);

}
