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

#ifndef _USBWIZARD_H_
#define _USBWIZARD_H_

#include <windows.h>
#include <prsht.h>
#include <shlobj.h>
#include "system.h"

class USBWizard
{
    PROPSHEETHEADER psh;
    PROPSHEETPAGE psp[5];
    IProgressDialog *ipd;
    private:
        std::vector<std::wstring>SourceList;
        std::vector<std::wstring>TargetList;
        UINT CurrentFile=0;
        void ClearDirectory(std::wstring directory);
    public:
        USBWizard();
        __int64 BytesRequired=0;
        __int64 BytesAvailable=0;
        __int64 BytesCopied=0;
        int TargetDrive=0;                         // 65=A: , 66=B: etc
        int DriverPackOption=0;                    // 0=all, 1=network, 2=selected, 3=none
        int Languages=0;                           // 0=all, 1=current, 2=default english
        int Themes=0;                              // 0=all, 1=current, 2=default
        std::wstring AdditionalPath;
        bool pathChanged=false;
        bool IncludeOnlineIndexes=false;
        bool ExpertMode=false;
        bool NoUpdates=true;
        bool NoSnapshots=false;
        bool NoLogs=false;
        bool IncludeAutoFiles=true;
        bool ShowAllDrives=false;
        bool doWizard();
        void ClearTarget(HWND hwnd);
        void QuickFormatTarget(HWND parent);
        void ClearBuildList();
        void AddFile(std::wstring source,std::wstring dest);
        void AddDirectory(std::wstring dir,std::wstring targetPath);
        void BuildDrive(HWND hwnd);
        void SetProgress(__int64 chunk,__int64 TotalFileSize);
};

#endif
