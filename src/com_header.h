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

#ifndef COM_HEADER_H
#define COM_HEADER_H

#include <stddef.h>
#include <wchar.h>
#include "Version.h"

#ifndef _W
#define __CC(p,s) p ## s
#define _W(s)  __CC(L,s)
#endif

typedef unsigned ofst;

// Misc
#define BUFLEN 4096
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0501
#endif
//#undef __STRICT_ANSI__
//#define BENCH_MODE

#define _CONSOLE
//#define EXCLUDE_COM   //it is some old macro from 9.31 unused now in source code
#define NO_REGISTRY
//#define _MBCS         //7zip supports unicode since 4.58 alpha 8
#define _NO_CRYPTO
#define EXTRACT_ONLY

// BOOST
#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif
#ifndef BOOST_ASIO_ENABLE_CANCELIO
#define BOOST_ASIO_ENABLE_CANCELIO
#endif
#ifndef BOOST_ASIO_HAS_STD_CHRONO
#define BOOST_ASIO_HAS_STD_CHRONO
#endif
//#define BOOST_ASIO_HASH_MAP_BUCKETS 1021  // Can be set to auto since Asio 1.4.3 / Boost 1.40
//#define BOOST_ASIO_SEPARATE_COMPILATION   // BOOST_ASIO_SEPARATE_COMPILATION - is evil
#define BOOST_MULTI_INDEX_DISABLE_SERIALIZATION
#define BOOST_NO_DEPRECATED
#define BOOST_SYSTEM_NO_DEPRECATED
//#define BOOST_SYSTEM_STATIC_LINK 1           // No references, no longer used?
//#define BOOST_NO_AUTO_PTR                   // The standard library no longer supports std::auto_ptr. It was deprecated in C++11 and is removed from C++14

// Torrent
//#define TORRENT_DISABLE_GEO_IP            // deprecated and gone in 2016
//#define TORRENT_NO_DEPRECATE              // todo: comply with torrent 2.0
//#define TORRENT_PRODUCTION_ASSERTS 1      // Enables assert logging in release builds
//#define TORRENT_RELEASE_ASSERTS 1         // Deprecated and gone in RC_1_2
#define TORRENT_USE_I2P 0                 
#define TORRENT_USE_ICONV 0                 // Already disabled for windows in config.hpp
//#define TORRENT_USE_IPV6 1                // Outdated: TORRENT_USE_IPV6 is used in libtorrent to simply disable any mention of IPv6 specific types and functions.
                                            // The purpose is to be able to build on systems that do not support IPv6.
//#define TORRENT_USE_TOMMATH               // Switched to boost.multiprecision instead in RC 1.1

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
