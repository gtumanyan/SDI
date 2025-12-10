// encoding: UTF-8
/******************************************************************************
*
This file is part of Snappy Driver Installer.

Snappy Driver Installer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License or (at your option) any later version.

Snappy Driver Installer is distributed in the hope that it will be useful
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Snappy Driver Installer.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/
#include "VersionEx.h"

#ifndef _T
#if !defined(ISPP_INVOKED) && (defined(UNICODE) || defined(_UNICODE))
#define _T(text) L##text
#else
#define _T(text) text
#endif
#endif

#define DO_STRINGIFY(x) _T(#x)
#define STRINGIFY(x)    DO_STRINGIFY(x)

// ----------------------------------------------------------------------------

#define VERSION_FILEVERSION_NUM     VERSION_MAJOR,VERSION_MINOR,VERSION_REV
#define VERSION_FILEVERSION_SHORT	STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_REV)
#define VERSION_WEBP				L"WebP " STRINGIFY(WEBP_VER)
#define VERSION_BUILD_INFO_LIB		VERSION_WEBP L", " VERSION_LIBTORRENT L", " VERSION_7ZIP
#define VERSION_BUILD_TOOL_NAME		L"Visual C++"
#define VERSION_BUILD_TOOL_MAJOR	(_MSC_VER / 100) // 2-digit
#define VERSION_BUILD_TOOL_MINOR	(_MSC_VER % 100) // 2-digit
#define VERSION_BUILD_TOOL_PATCH	(_MSC_FULL_VER % 100000) // 5-digit
#define VERSION_BUILD_TOOL_BUILD	_MSC_BUILD // 2?-digit
#undef VERSION_BUILD_INFO_FORMAT
#define VERSION_BUILD_INFO_FORMAT	L"Compiled on " __DATE__ L" with %s %d.%02d.%05d.%d"

#define VERSION_FILEVERSION_LONG    STRINGIFY(APPNAME) L" " VERSION_FILEVERSION_SHORT L" " STRINGIFY(VERSION_PATCH)

#define VERSION_LEGALCOPYRIGHT      L"Copyright © 2014-2026 all authors (GPLv3)"
#define MY_APPNAME_DESCRIPTION		"Snappy Driver Installer"
#define VERSION_AUTHORNAME			"Gregory Tumanyan <https://t.me/flounderingreg> (Current Maintainer), Glenn Delahoy, BadPointer (Founder)."
#define VERSION_WEBPAGEDISPLAY		L"https://t.me/Snappy_Driver_Installer"
#define WEB_BOOSTYPAGE				L"https://boosty.to/snappydriverinstaller/donate"
#define VERSION_TELEGRAM_DISPLAY	"https://t.me/Snappy_Driver_Installer"

#define VER_MARKER					"SDW"
#define VERSION_INDEX				0x205
#define VERSION_STATE				0x102

#define VERSION_LIBTORRENT	        L"LibTorrent " STRINGIFY(TORRENT_VER)
#define VERSION_7ZIP                L"7zip " STRINGIFY(SZIP_VER)

// Version
class Version
{
    int d,m,y;
    int v1,v2,v3,v4;

public:
    int  setDate(int d_,int m_,int y_);
    void setVersion(int v1_,int v2_,int v3_,int v4_);
    void setInvalid(){y=v1=-1;}
    int  GetV1()const{return v1;}
    void str_date(WStringShort &buf,bool invariant=false)const;
    void str_version(WStringShort &buf)const;

    Version():d(0),m(0),y(0),v1(-2),v2(0),v3(0),v4(0){}
    Version(int d1,int m1,int y1):d(d1),m(m1),y(y1),v1(-2),v2(0),v3(0),v4(0){}

    friend int cmpdate(const Version *t1,const Version *t2);
    friend int cmpversion(const Version *t1,const Version *t2);
};
int cmpdate(const Version *t1,const Version *t2);
int cmpversion(const Version *t1,const Version *t2);