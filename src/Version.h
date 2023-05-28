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

#ifndef SDI_VERSION_H
#define SDI_VERSION_H

#define __CC(p,s) p ## s
#define _V(s)  __CC(v,s)
#define _W(s)  __CC(L,s)

#define _STRINGIFY(s) #s
#define _STRG(s)  _STRINGIFY(s)

// ----------------------------------------------------------------------------

#define VERSION_FILEVERSION          VERSION_MAJOR.VERSION_MINOR.VERSION_REV
#define VERSION_FILEVERSION_NUM      VERSION_MAJOR,VERSION_MINOR,VERSION_REV
#define VERSION_BUILD_INFO_LIB		VERSION_WEBP L", " VERSION_LIBTORRENT L", " VERSION_7ZIP
#define VERSION_BUILD_TOOL_NAME		L"Visual C++"
#define VERSION_BUILD_TOOL_MAJOR	(_MSC_VER / 100) // 2-digit
#define VERSION_BUILD_TOOL_MINOR	(_MSC_VER % 100) // 2-digit
#define VERSION_BUILD_TOOL_PATCH	(_MSC_FULL_VER % 100000) // 5-digit
#define VERSION_BUILD_TOOL_BUILD	_MSC_BUILD // 2?-digit
#undef VERSION_BUILD_INFO_FORMAT
#define VERSION_BUILD_INFO_FORMAT	L"Compiled on " __DATE__ L" with %s %d.%02d.%05d.%d\n" VERSION_BUILD_INFO_LIB

#if defined(_WIN64)
#define VERSION_FILEVERSION_LONG     APPNAME (x64)  VERSION_FILEVERSION  VERSION_PATCH
#else
#define VERSION_FILEVERSION_LONG     APPNAME (x86)  VERSION_FILEVERSION  VERSION_PATCH
#endif


#if (defined(_DEBUG) || defined(DEBUG)) && !defined(NDEBUG)
#pragma message("Debug Build: " _STRG(VERSION_FILEVERSION_LONG))
#else
#pragma message("Release Build: " _STRG(VERSION_FILEVERSION_LONG))
#endif

#define VERSION_LEGALCOPYRIGHT      "Copyright © 2022-2023 WindR"
#define MY_APPNAME_DESCRIPTION		"Snappy Driver Installer"
#define VERSION_AUTHORNAME			"Gregory Tumanyan <https://t.me/gtumanyan> (Current Maintainer), Glenn Delahoy, BadPointer (Founder)."
#define VERSION_WEBPAGEDISPLAY		"https://t.me/Snappy_Driver_Installer"
#define WEB_BOOSTYPAGE				"https://boosty.to/snappydriverinstaller/donate"
#define VERSION_TELEGRAM_DISPLAY	"https://t.me/Snappy_Driver_Installer"



#define VERSION_WEBP				   L"WebP " _W(_STRG(_V(WEBP_VER)))
#define VERSION_LIBTORRENT	           L"LibTorrent " _W(_STRG(_V(TORRENT_VER)))
#define VERSION_7ZIP                   L"7zip " _W(_STRG(_V(SZIP_VER)))


#endif // NOTEPAD3_VERSION_H
