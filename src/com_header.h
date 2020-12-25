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

#ifndef COM_HEADER_H
#define COM_HEADER_H

#include <stddef.h>
#include <wchar.h>
#include "version_rev.h"

typedef unsigned ofst;

// Misc
#define BUFLEN 4096
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_IE
#define _WIN32_IE 0x0501
#endif
//#undef __STRICT_ANSI__
//#define BENCH_MODE

#define _CONSOLE
#define EXCLUDE_COM
#define NO_REGISTRY
#define _MBCS
#define _NO_CRYPTO
#define EXTRACT_ONLY

// BOOST
#define BOOST_ALL_NO_LIB
#define BOOST_ASIO_ENABLE_CANCELIO
#define BOOST_ASIO_HASH_MAP_BUCKETS 1021
#define BOOST_ASIO_SEPARATE_COMPILATION
#define BOOST_MULTI_INDEX_DISABLE_SERIALIZATION
#define BOOST_SYSTEM_NO_DEPRECATED
#define BOOST_SYSTEM_STATIC_LINK 1
#define BOOST_NO_AUTO_PTR

// Torrent
#define TORRENT_DISABLE_GEO_IP
#define TORRENT_NO_DEPRECATE
#define TORRENT_PRODUCTION_ASSERTS 1
#define TORRENT_RELEASE_ASSERTS 1
#define TORRENT_USE_I2P 0
#define TORRENT_USE_ICONV 0
#define TORRENT_USE_IPV6 0
#define TORRENT_USE_TOMMATH

#ifdef _MSC_VER
#include <shlwapi.h>
#else
#ifndef _INC_SHLWAPI
#define _INC_SHLWAPI
  extern "C" __stdcall char *StrStrIA(const char *lpFirst,const char *lpSrch);
  extern "C" __stdcall wchar_t *StrStrIW(const wchar_t *lpFirst,const wchar_t *lpSrch);
#endif
#endif

#endif
