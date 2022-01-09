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
#pragma once

#include "VersionEx.h"

#define DO_STRINGIFY(x)		#x
#define STRINGIFY(x)		DO_STRINGIFY(x)

#define MY_APPNAME									"SDI2"
#define MY_APPNAME_DESCRIPTION			"Snappy Drivers Installer"
#define VERSION_FILEVERSION_NUM      VERSION_MAJOR,VERSION_MINOR,VERSION_REV,VERSION_BUILD
#define VERSION_FILEVERSION          STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." \
				STRINGIFY(VERSION_REV) "." STRINGIFY(VERSION_BUILD)
#define VERSION_LEGALCOPYRIGHT      L"Copyright © 2022"
#define VERSION_AUTHORNAME					L"BadPointer, WindR, Delahoy"
#define VERSION_WEBPAGE_DISPLAY			L"http://sdi-tool.org/"
#define WEB_PATREONPAGE							L"https://www.patreon.com/SamLab"
#define VERSION_TELEGRAM_DISPLAY		L"https://t.me/Snappy_Driver_Installer"
#if defined(_MSC_VER)
#define VERSION_BUILD_TOOL_NAME		L"Visual C++"
#define VERSION_BUILD_TOOL_MAJOR	(_MSC_VER / 100) // 2-digit
#define VERSION_BUILD_TOOL_MINOR	(_MSC_VER % 100) // 2-digit
#define VERSION_BUILD_TOOL_PATCH	(_MSC_FULL_VER % 100000) // 5-digit
#define VERSION_BUILD_TOOL_BUILD	_MSC_BUILD // 2?-digit
#define VERSION_BUILD_INFO_FORMAT	L"Compiled on " __DATE__ L" with %s %d.%02d.%05d.%d"
#endif

#if defined(_WIN64)
		#define VERSION_FILEVERSION_ARCH	" (64-bit) "
#else
		#define VERSION_FILEVERSION_ARCH	" (32-bit) "
#endif

#define VERSION_FILEVERSION_LONG	MY_APPNAME VERSION_FILEVERSION_ARCH STRINGIFY(VERSION_MAJOR) "." \
									STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_REV) VERSION_STATUS

#define _WEBP_BUILD                    L"WebP: "
#define _TORR_BUILD                    L"Libtorrent: "

#define VERSION_WEBP										_WEBP_BUILD STRINGIFY(WEBP_VER)
#define VERSION_LIBTORRENT	           _TORR_BUILD STRINGIFY(TORRENT_VER)
#define VERSION_ONIGURUMA              L"Oniguruma " _W(_STRG(_V(ONIGURUMA_REGEX_VER)))
#define VERSION_UCHARDET               L"UChardet " _W(_STRG(_V(UCHARDET_VER)))
#define VERSION_TINYEXPR               L"TinyExpr " _W(_STRG(_V(TINYEXPR_VER)))
#define VERSION_UTHASH                 L"UTHash " _W(_STRG(_V(UTHASH_VER)))

