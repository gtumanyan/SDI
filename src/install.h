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

#ifndef INSTALL_H
#define INSTALL_H

// Global vars
extern long long ar_total,ar_proceed;
extern int instflag;
extern size_t itembar_act;
extern int needreboot;
extern wchar_t extractdir[BUFLEN];

extern volatile int installupdate_exitflag;
extern Event *installupdate_event;

extern long long totalinstalltime,totalextracttime;

// Installer
enum INSTALLER
{
    INSTALLDRIVERS     = 1,
    OPENFOLDER         = 2,
};

void _7z_total(long long i);
int  _7z_setcomplited(long long i);
void driver_install(wchar_t *hwid,const wchar_t *inf,int *ret,int *needrb);
void removeextrainfs(wchar_t *inf);
void save_wndinfo();

#endif
