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

#ifndef SEVENZIP_H
#define SEVENZIP_H

extern "C"
{
#include "7z.h"
#include "7zAlloc.h"
#include <7zCrc.h>
#include <Bcj2.h>		//for BCJ2 support
#include <Bra.h>		//for branch support
#include <7zFile.h>
#include <7zVersion.h>
#include <Lzma86.h>
#include <LzmaDec.h>
#include <Lzma2Dec.h>
//#include "LzmaEnc.h"		replaced by LzmaDec
}

#endif

#define k_Copy 0
#define k_LZMA2 0x21
#define k_LZMA  0x30101
#define k_BCJ2  0x303011B


