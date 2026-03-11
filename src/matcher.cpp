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

#include <cstdio>
#include <windows.h>
#include <Shlwapi.h>

#include "logging.h"
#include "version.h"
#include "system.h"
#include "Settings.h"
#include "indexing.h"
#include "SDI.h"
#include "matcher.h"
#include "theme.h"
#include "gui.h"

// Depend on Win32API
#include "enum.h"
#include "device.h"  // for CM_PROB_DISABLED

//{ Global variables

/*
For Windows XP to Windows 10, version 1511, the format of TargetOSVersion decoration is as follows:
INF

NT[Architecture][.[OSMajorVersion][.[OSMinorVersion][.[ProductType][.SuiteMask]]]]

Starting with Windows 10, version 1607 (Build 14310 and later), the format of the TargetOSVersion decoration is as follows:
INF

NT[Architecture][.[OSMajorVersion][.[OSMinorVersion][.[ProductType][.[SuiteMask][.[BuildNumber]]]]


Windows 11 21H2 10.0.22000
Windows 11 22H2 10.0.22261
Windows 11 23H2 10.0.22631
Windows 11 24H2 10.0.26100
Windows 11 25H2 10.0.26200

Windows 10        10.0.10240
Windows 10 (1511) 10.0.10586
Windows 10 (1607) 10.0.14310
Windows 10 (1607) 10.0.14393
Windows 10 (1703) 10.0.15063
Windows 10 (1709) 10.0.16299
Windows 10 (1803) 10.0.17134
Windows 10 (1809) 10.0.17763
Windows 10 (1903) 10.0.18362
Windows 10 (1909) 10.0.18363
Windows 10 (2004) 10.0.19041
Windows 10 (20H2) 10.0.19042
Windows 10 (21H1) 10.0.19043
Windows 10 (21H2) 10.0.19044
Windows 10 (22H2) 10.0.19045
*/

/*
Invalid:
    me          Windows ME
    ntx64       ntAMD64
    ntai64      ntIA64
    ntarm       ARM
    ntx64.6.0   ntAMD64
    nt.7        future
*/
// following matches the optional architecture field in the [Manufacturer]
// section of the inf file
const char *nts[num_decs]=
{
		// https://betawiki.net/wiki/Windows_11                                                                                              // CLIENT                   SERVER
        "nt.5",            "ntx86.5",            "ntamd64.5",            "ntia64.5",            "ntarm.5",            "ntarm64.5",           // 2000  
        "nt.5.0",          "ntx86.5.0",          "ntamd64.5.0",          "ntia64.5.0",          "ntarm.5.0",          "ntarm64.5.0",         // 2000
        "nt.5.1",          "ntx86.5.1",          "ntamd64.5.1",          "ntia64.5.1",          "ntarm.5.1",          "ntarm64.5.1",         // XP
        "nt.5.2",          "ntx86.5.2",          "ntamd64.5.2",          "ntia64.5.2",          "ntarm.5.2",          "ntarm64.5.2",         // XP x64                 Server 2003
        "nt.6",            "ntx86.6",            "ntamd64.6",            "ntia64.6",            "ntarm.6",            "ntarm64.6",           // Vista                  Server 2008
        "nt.6.0",          "ntx86.6.0",          "ntamd64.6.0",          "ntia64.6.0",          "ntarm.6.0",          "ntarm64.6.0",         // Vista                  Server 2008
        "nt.6.1",          "ntx86.6.1",          "ntamd64.6.1",          "ntia64.6.1",          "ntarm.6.1",          "ntarm64.6.1",         // 7                      Server 2008 R2
        "nt.6.2",          "ntx86.6.2",          "ntamd64.6.2",          "ntia64.6.2",          "ntarm.6.2",          "ntarm64.6.2",         // 8                      Server 2012
        "nt.6.3",          "ntx86.6.3",          "ntamd64.6.3",          "ntia64.6.3",          "ntarm.6.3",          "ntarm64.6.3",         // 8.1                    Server 2012 R2
        "nt.6.4",          "ntx86.6.4",          "ntamd64.6.4",          "ntia64.6.4",          "ntarm.6.4",          "ntarm64.6.4",         // 10                     
        "nt.10",           "ntx86.10",           "ntamd64.10",           "ntia64.10",           "ntarm.10",           "ntarm64.10",          // 10                     
        "nt.10.0",         "ntx86.10.0",         "ntamd64.10.0",         "ntia64.10.0",         "ntarm.10.0",         "ntarm64.10.0",        // 10                     Server 2016                                                                                           
        "nt.10.0...10240", "ntx86.10.0...10240", "ntamd64.10.0...10240", "ntia64.10.0...10240", "ntarm.10.0...10240", "ntarm64.10.0...10240", //Win 10 v1507           
        "nt.10.0...10586", "ntx86.10.0...10586", "ntamd64.10.0...10586", "ntia64.10.0...10586", "ntarm.10.0...10586", "ntarm64.10.0...10586", //Win 10 v1511           Server 2016 TP4
        "nt.10.0...14310", "ntx86.10.0...14310", "ntamd64.10.0...14310", "ntia64.10.0...14310", "ntarm.10.0...14310", "ntarm64.10.0...14310", //Win 10 v1607		   Server 2016
        "nt.10.0...14393", "ntx86.10.0...14393", "ntamd64.10.0...14393", "ntia64.10.0...14393", "ntarm.10.0...14393", "ntarm64.10.0...14393", //Win 10 v1607           Server 2016
        "nt.10.0...15063", "ntx86.10.0...15063", "ntamd64.10.0...15063", "ntia64.10.0...15063", "ntarm.10.0...15063", "ntarm64.10.0...15063", //Win 10 v1703              
        "nt.10.0...16209", "ntx86.10.0...16209", "ntamd64.10.0...16209", "ntia64.10.0...16209", "ntarm.10.0...16209", "ntarm64.10.0...16209", //Win 10 v1709 EEAP         
        "nt.10.0...16273", "ntx86.10.0...16273", "ntamd64.10.0...16273", "ntia64.10.0...16273", "ntarm.10.0...16273", "ntarm64.10.0...16273", //Win 10 v1709 preview   Server 1709
        "nt.10.0...16288", "ntx86.10.0...16288", "ntamd64.10.0...16288", "ntia64.10.0...16288", "ntarm.10.0...16288", "ntarm64.10.0...16288", //Win 10 v1709 preview   
        "nt.10.0...16299", "ntx86.10.0...16299", "ntamd64.10.0...16299", "ntia64.10.0...16299", "ntarm.10.0...16299", "ntarm64.10.0...16299", //Win 10 v1709           Server 2016
        "nt.10.0...17134", "ntx86.10.0...17134", "ntamd64.10.0...17134", "ntia64.10.0...17134", "ntarm.10.0...17134", "ntarm64.10.0...17134", //Win 10 v1803           Server 2016
        "nt.10.0...17704", "ntx86.10.0...17704", "ntamd64.10.0...17704", "ntia64.10.0...17704", "ntarm.10.0...17704", "ntarm64.10.0...17704", //Win 10 v1809 preview   Server 2019
        "nt.10.0...17735", "ntx86.10.0...17735", "ntamd64.10.0...17735", "ntia64.10.0...17735", "ntarm.10.0...17735", "ntarm64.10.0...17735", //Win 10 v1809 preview   Server 2019
        "nt.10.0...17763", "ntx86.10.0...17763", "ntamd64.10.0...17763", "ntia64.10.0...17763", "ntarm.10.0...17763", "ntarm64.10.0...17763", //Win 10 v1809           Server 2019
        "nt.10.0...18362", "ntx86.10.0...18362", "ntamd64.10.0...18362", "ntia64.10.0...18362", "ntarm.10.0...18362", "ntarm64.10.0...18362", //Win 10 v1903           Server 2019
        "nt.10.0...18363", "ntx86.10.0...18363", "ntamd64.10.0...18363", "ntia64.10.0...18363", "ntarm.10.0...18363", "ntarm64.10.0...18363", //Win 10 v1909           Server 2019
        "nt.10.0...18900", "ntx86.10.0...18900", "ntamd64.10.0...18900", "ntia64.10.0...18900", "ntarm.10.0...18900", "ntarm64.10.0...18900", //Win 10 v20H1 preview   
        "nt.10.0...19041", "ntx86.10.0...19041", "ntamd64.10.0...19041", "ntia64.10.0...19041", "ntarm.10.0...19041", "ntarm64.10.0...19041", //Win 10 v2004/20H1      Server 2019
        "nt.10.0...19042", "ntx86.10.0...19042", "ntamd64.10.0...19042", "ntia64.10.0...19042", "ntarm.10.0...19042", "ntarm64.10.0...19042", //Win 10 v20H2           Server 2019
        "nt.10.0...19043", "ntx86.10.0...19043", "ntamd64.10.0...19043", "ntia64.10.0...19043", "ntarm.10.0...19043", "ntarm64.10.0...19043", //Win 10 v21H1         
        "nt.10.0...19044", "ntx86.10.0...19044", "ntamd64.10.0...19044", "ntia64.10.0...19044", "ntarm.10.0...19044", "ntarm64.10.0...19044", //Win 10 v21H2         
        "nt.10.0...19045", "ntx86.10.0...19045", "ntamd64.10.0...19045", "ntia64.10.0...19045", "ntarm.10.0...19045", "ntarm64.10.0...19045", //Win 10 v22H2         
        "nt.10.0...19586", "ntx86.10.0...19586", "ntamd64.10.0...19586", "ntia64.10.0...19586", "ntarm.10.0...19586", "ntarm64.10.0...19586", //Win 11 preview       
        "nt.10.0...20190", "ntx86.10.0...20190", "ntamd64.10.0...20190", "ntia64.10.0...20190", "ntarm.10.0...20190", "ntarm64.10.0...20190", //Win 11 preview	 	   Server 2022
        "nt.10.0...20348", "ntx86.10.0...20348", "ntamd64.10.0...20348", "ntia64.10.0...20348", "ntarm.10.0...20348", "ntarm64.10.0...20348", //                       Server 2022
        "nt.10.0...21250", "ntx86.10.0...21250", "ntamd64.10.0...21250", "ntia64.10.0...21250", "ntarm.10.0...21250", "ntarm64.10.0...21250", //Win 11 preview         
        "nt.10.0...21262", "ntx86.10.0...21262", "ntamd64.10.0...21262", "ntia64.10.0...21262", "ntarm.10.0...21262", "ntarm64.10.0...21262", //Win 11 Cobalt          
        "nt.10.0...22000", "ntx86.10.0...22000", "ntamd64.10.0...22000", "ntia64.10.0...22000", "ntarm.10.0...22000", "ntarm64.10.0...22000", //Win 11 v21H2           
        "nt.10.0...22621", "ntx86.10.0...22621", "ntamd64.10.0...22621", "ntia64.10.0...22621", "ntarm.10.0...22621", "ntarm64.10.0...22621", //Win 11 v22H2           
        "nt.10.0...22631", "ntx86.10.0...22631", "ntamd64.10.0...22631", "ntia64.10.0...22631", "ntarm.10.0...22631", "ntarm64.10.0...22631", //Win 11 v23H2           
        "nt.10.0...22635", "ntx86.10.0...22635", "ntamd64.10.0...22635", "ntia64.10.0...22635", "ntarm.10.0...22635", "ntarm64.10.0...22635", //Win 11 v23H2           
        "nt.10.0...25216", "ntx86.10.0...25216", "ntamd64.10.0...25216", "ntia64.10.0...25216", "ntarm.10.0...25216", "ntarm64.10.0...25216", //Win 11 v24H2 Preview   Server 2025
        "nt.10.0...25398", "ntx86.10.0...25398", "ntamd64.10.0...25398", "ntia64.10.0...25398", "ntarm.10.0...25398", "ntarm64.10.0...25398", //Win 11				   Server 23H2
        "nt.10.0...25952", "ntx86.10.0...25952", "ntamd64.10.0...25952", "ntia64.10.0...25952", "ntarm.10.0...25952", "ntarm64.10.0...25952", //Win 11 v24H2 Preview   
        "nt.10.0...26052", "ntx86.10.0...26052", "ntamd64.10.0...26052", "ntia64.10.0...26052", "ntarm.10.0...26052", "ntarm64.10.0...26052", //Win 11 v24H2 Preview   Server 2025
        "nt.10.0...26063", "ntx86.10.0...26063", "ntamd64.10.0...26063", "ntia64.10.0...26063", "ntarm.10.0...26063", "ntarm64.10.0...26063", //Win 11 v24H2 PreSSE4.2 Server 2025
        "nt.10.0...26080", "ntx86.10.0...26080", "ntamd64.10.0...26080", "ntia64.10.0...26080", "ntarm.10.0...26080", "ntarm64.10.0...26080", //Win 11 v24H2 Dev	   Server 2025
        "nt.10.0...26100", "ntx86.10.0...26100", "ntamd64.10.0...26100", "ntia64.10.0...26100", "ntarm.10.0...26100", "ntarm64.10.0...26100", //Win 11 v24H2           Server 2025
        "nt.10.0...26200", "ntx86.10.0...26200", "ntamd64.10.0...26200", "ntia64.10.0...26200", "ntarm.10.0...26200", "ntarm64.10.0...26200", //Win 11 v25H2
        "nt",              "ntx86",              "ntamd64",              "ntia64",              "ntarm",              "ntarm64",              // NT specifies Win 2000 and later
        "nt..",            "ntx86..",            "ntamd64..",            "ntia64..",            "ntarm..",            "ntarm64.."             
};

// this represents the inf file target version = major * 10 + minor
// where win11 = 10.0.22000 or later
const int nts_version[num_decs]=
{
    50,    50,    50,    50,    50,   50, // 2000
    50,    50,    50,    50,    50,   50, // 2000
    51,    51,    51,    51,    51,   51, // XP
    52,    52,    52,    52,    52,   52, // Server 2003
    60,    60,    60,    60,    60,   60, // Vista
    60,    60,    60,    60,    60,   60, // Vista
    61,    61,    61,    61,    61,   61, // 7
    62,    62,    62,    62,    62,   62, // 8
    63,    63,    63,    63,    63,   63, // 8.1
    64,    64,    64,    64,    64,   64, // 10
   100,   100,   100,   100,   100,  100, // 10
   100,   100,   100,   100,   100,  100, // 10
   //
   100,   100,   100,   100,   100,  100, // 10 (1507)
   100,   100,   100,   100,   100,  100, // 10 (1511)
   100,   100,   100,   100,   100,  100, // 10 (1607) (14310)
   100,   100,   100,   100,   100,  100, // 10 (1607)
   100,   100,   100,   100,   100,  100, // 10 (1703)
   100,   100,   100,   100,   100,  100, // 10 (creators update)
   100,   100,   100,   100,   100,  100, // 10 (insider preview)
   100,   100,   100,   100,   100,  100, // 10 (insider preview)
   100,   100,   100,   100,   100,  100, // 10 (1709)
   100,   100,   100,   100,   100,  100, // 10 (1803)
   100,   100,   100,   100,   100,  100, // 10 (1803)
   100,   100,   100,   100,   100,  100, // 10 (insider preview)
   100,   100,   100,   100,   100,  100, // 10 (1809)
   100,   100,   100,   100,   100,  100, // 10 (1903)
   100,   100,   100,   100,   100,  100, // 10 (1909)
   100,   100,   100,   100,   100,  100, // 10 (1909)
   100,   100,   100,   100,   100,  100, // 10 (20H1)
   100,   100,   100,   100,   100,  100, // 10 (20H2)
   100,   100,   100,   100,   100,  100, // 10 (21H1)
   100,   100,   100,   100,   100,  100, // 10 (21H2)
   100,   100,   100,   100,   100,  100, // 10 (22H2)
   100,   100,   100,   100,   100,  100, // 10 (?)
   100,   100,   100,   100,   100,  100, // 10 (insider preview)
   100,   100,   100,   100,   100,  100, // 10 (Server 2022)
   100,   100,   100,   100,   100,  100, // 10 (?)
   100,   100,   100,   100,   100,  100, // 10 (insider preview)
   //
   100,   100,   100,   100,   100,  100, // 11 (21H2)
   100,   100,   100,   100,   100,  100, // 11 (22H2)
   100,   100,   100,   100,   100,  100, // 11 (23H2)
   100,   100,   100,   100,   100,  100, // 11
   100,   100,   100,   100,   100,  100, // 11
   100,   100,   100,   100,   100,  100, // 11 (?)
   100,   100,   100,   100,   100,  100, // 11 (?)
   100,   100,   100,   100,   100,  100, // 11 (insider preview)
   100,   100,   100,   100,   100,  100, // 11 (insider preview)
   100,   100,   100,   100,   100,  100, // 11 (insider preview)
   100,   100,   100,   100,   100,  100, // 11 (24H2 - Server 2025)
   100,   100,   100,   100,   100,  100, // 11 (25H2)
   //
     0,     0,     0,     0,     0,    0,
     0,     0,     0,     0,     0,    0,
   //
//     -1,
};

const int nts_build[num_decs]=
{
    0,    0,    0,    0,    0,   0, // 2000
    0,    0,    0,    0,    0,   0, // 2000
    0,    0,    0,    0,    0,   0, // XP
    0,    0,    0,    0,    0,   0, // Server 2003
    0,    0,    0,    0,    0,   0, // Vista
    0,    0,    0,    0,    0,   0, // Vista
    0,    0,    0,    0,    0,   0, // 7
    0,    0,    0,    0,    0,   0, // 8
    0,    0,    0,    0,    0,   0, // 8.1
    0,    0,    0,    0,    0,   0, // 10
    0,    0,    0,    0,    0,   0, // 10
    0,    0,    0,    0,    0,   0, // 10
   //
       0,       0,       0,       0,       0,      0, // 10 (1507)
       0,       0,       0,       0,       0,      0, // 10 (1511)
   14310,   14310,   14310,   14310,   14310,  14310, // 10 (1607) (14310)
   14393,   14393,   14393,   14393,   14393,  14393, // 10 (1607)
   15063,   15063,   15063,   15063,   15063,  15063, // 10 (1703)
   16209,   16209,   16209,   16209,   16209,  16209, // 10 (creators update)
   16273,   16273,   16273,   16273,   16273,  16273, // 10 (insider preview)
   16288,   16288,   16288,   16288,   16288,  16288, // 10 (insider preview)
   16299,   16299,   16299,   16299,   16299,  16299, // 10 (1709)
   17134,   17134,   17134,   17134,   17134,  17134, // 10 (1803)
   17704,   17704,   17704,   17704,   17704,  17704, // 10 (1803)
   17735,   17735,   17735,   17735,   17735,  17735, // 10 (insider preview)
   17763,   17763,   17763,   17763,   17763,  17763, // 10 (1809)
   18362,   18362,   18362,   18362,   18362,  18362, // 10 (1903)
   18363,   18363,   18363,   18363,   18363,  18363, // 10 (1909)
   18900,   18900,   18900,   18900,   18900,  18900, // 10
   19041,   19041,   19041,   19041,   19041,  19041, // 10 (20H1)
   19042,   19042,   19042,   19042,   19042,  19042, // 10 (20H2)
   19043,   19043,   19043,   19043,   19043,  19043, // 10 (21H1)
   19044,   19044,   19044,   19044,   19044,  19044, // 10 (21H2)
   19045,   19045,   19045,   19045,   19045,  19045, // 10 (22H2)
   19586,   19586,   19586,   19586,   19586,  19586, // 10 (?)
   20190,   20190,   20190,   20190,   20190,  20190, // 10 (insider preview)
   20348,   20348,   20348,   20348,   20348,  20348, // 10 (Server 2022)
   21250,   21250,   21250,   21250,   21250,  21250, // 10 (?)
   21262,   21262,   21262,   21262,   21262,  21262, // 10 (insider preview)
   //
   22000,   22000,   22000,   22000,   22000,  22000, // 11 (21H2)
   22621,   22621,   22621,   22621,   22621,  22621, // 11 (22H2)
   22631,   22631,   22631,   22631,   22631,  22631, // 11 (23H2)
   22635,   22635,   22635,   22635,   22635,  22635, // 11 (23H2)
   25216,   25216,   25216,   25216,   25216,  25216, // 11 (?)
   25398,   25398,   25398,   25398,   25398,  25398, // 11 (?)
   25952,   25952,   25952,   25952,   25952,  25952, // 11 (?)
   26052,   26052,   26052,   26052,   26052,  26052, // 11 (insider preview)
   26063,   26063,   26063,   26063,   26063,  26063, // 11 (insider preview)
   26080,   26080,   26080,   26080,   26080,  26080, // 11 (insider preview)
   26100,   26100,   26100,   26100,   26100,  26100, // 11 (24H2 - Server 2025)
   26200,   26200,   26200,   26200,   26200,  26200, // 11 (25H2)
   //
   0,   0,   0,   0,   0,  0,
   0,   0,   0,   0,   0,  0,
   //
//   -1,
};

// 0=unknown/don't care/ignore, 1=x86, 2=amd64, 3=ia64, 4=arm, 5=arm64
const int nts_arch[num_decs]=
{
    0,  1,  2,  3,  4,  5, // 2000
    0,  1,  2,  3,  4,  5, // 2000
    0,  1,  2,  3,  4,  5, // XP
    0,  1,  2,  3,  4,  5, // Serve
    0,  1,  2,  3,  4,  5, // Vista
    0,  1,  2,  3,  4,  5, // Vista
    0,  1,  2,  3,  4,  5, // 7
    0,  1,  2,  3,  4,  5, // 8
    0,  1,  2,  3,  4,  5, // 8.1
    0,  1,  2,  3,  4,  5, // 10
    0,  1,  2,  3,  4,  5, // 10
    0,  1,  2,  3,  4,  5, // 10
    //
    0,  1,  2,  3,  4,  5, // 10 (1507)
    0,  1,  2,  3,  4,  5, // 10 (1511)
    0,  1,  2,  3,  4,  5, // 10 (1607)
    0,  1,  2,  3,  4,  5, // 10 (1607)
    0,  1,  2,  3,  4,  5, // 10 (1703)
    0,  1,  2,  3,  4,  5, // 10 (creators update)
    0,  1,  2,  3,  4,  5, // 10 (insider preview)
    0,  1,  2,  3,  4,  5, // 10 (insider preview)
    0,  1,  2,  3,  4,  5, // 10 (1709)
    0,  1,  2,  3,  4,  5, // 10 (1803)
    0,  1,  2,  3,  4,  5, // 10 (1803)
    0,  1,  2,  3,  4,  5, // 10 (insider preview)
    0,  1,  2,  3,  4,  5, // 10 (1809)
    0,  1,  2,  3,  4,  5, // 10 (1903)
    0,  1,  2,  3,  4,  5, // 10 (1909)
    0,  1,  2,  3,  4,  5, // 10
    0,  1,  2,  3,  4,  5, // 10 (20H1)
    0,  1,  2,  3,  4,  5, // 10 (20H2)
    0,  1,  2,  3,  4,  5, // 10 (21H1)
    0,  1,  2,  3,  4,  5, // 10 (21H2)
    0,  1,  2,  3,  4,  5, // 10 (22H2)
    0,  1,  2,  3,  4,  5, // 10 (?)
    0,  1,  2,  3,  4,  5, // 10 (insider preview)
    0,  1,  2,  3,  4,  5, // 10 (Server 2022)
    0,  1,  2,  3,  4,  5, // 10 (?)
    0,  1,  2,  3,  4,  5, // 10 (insider preview)
    //
    0,  1,  2,  3,  4,  5, // 11 (21H2)
    0,  1,  2,  3,  4,  5, // 11 (22H2)
    0,  1,  2,  3,  4,  5, // 11 (23H2)
    0,  1,  2,  3,  4,  5, // 11
    0,  1,  2,  3,  4,  5, // 11
    0,  1,  2,  3,  4,  5, // 11 (?)
    0,  1,  2,  3,  4,  5, // 11 (?)
    0,  1,  2,  3,  4,  5, // 11 (insider preview)
    0,  1,  2,  3,  4,  5, // 11 (insider preview)
    0,  1,  2,  3,  4,  5, // 11 (insider preview)
    0,  1,  2,  3,  4,  5, // 11 (24H2 - Server 2025)
    0,  1,  2,  3,  4,  5, // 11 (25H2)
    //
    0,  1,  2,  3,  4,  5,
    0,  1,  2,  3,  4,  5,
    //
//    -1,
};

// d: 0=x86, 1=amd64, 2=ia64, 3=arm, 4=arm64, -1=ignore
const int nts_score[num_decs]=
{
    50,   150,   150,   150,   150,  150, // 2000
    50,   150,   150,   150,   150,  150, // 2000
    51,   151,   151,   151,   151,  151, // XP
    52,   152,   152,   152,   152,  152, // Server 2003
    60,   160,   160,   160,   160,  160, // Vista
    60,   160,   160,   160,   160,  160, // Vista
    61,   161,   161,   161,   161,  161, // 7
    62,   162,   162,   162,   162,  162, // 8
    63,   163,   163,   163,   163,  163, // 8.1
    64,   164,   164,   164,   164,  164, // 10
    64,   164,   164,   164,   164,  164, // 10
    64,   164,   164,   164,   164,  164, // 10
    //
    64,   164,   164,   164,   164,  164, // 10 (1507)
    64,   164,   164,   164,   164,  164, // 10 (1511)
    64,   164,   164,   164,   164,  164, // 10 (1607) (14310)
    64,   164,   164,   164,   164,  164, // 10 (1607)
    64,   164,   164,   164,   164,  164, // 10 (1703)
    64,   164,   164,   164,   164,  164, // 10 (creators update)
    64,   164,   164,   164,   164,  164, // 10 (insider preview)
    64,   164,   164,   164,   164,  164, // 10 (insider preview)
    64,   164,   164,   164,   164,  164, // 10 (1709)
    64,   164,   164,   164,   164,  164, // 10 (1803)
    64,   164,   164,   164,   164,  164, // 10 (1803)
    64,   164,   164,   164,   164,  164, // 10 (insider preview)
    64,   164,   164,   164,   164,  164, // 10 (1809)
    64,   164,   164,   164,   164,  164, // 10 (1903)
    64,   164,   164,   164,   164,  164, // 10 (1909)
    64,   164,   164,   164,   164,  164, // 10
    64,   164,   164,   164,   164,  164, // 10 (20H1)
    64,   164,   164,   164,   164,  164, // 10 (20H2)
    64,   164,   164,   164,   164,  164, // 10 (21H1)
    64,   164,   164,   164,   164,  164, // 10 (21H2)
    64,   164,   164,   164,   164,  164, // 10 (22H2)
    64,   164,   164,   164,   164,  164, // 10 (?)
    64,   164,   164,   164,   164,  164, // 10 (insider preview)
    64,   164,   164,   164,   164,  164, // 10 (Server 2022)
    64,   164,   164,   164,   164,  164, // 10 (?)
    64,   164,   164,   164,   164,  164, // 10 (insider preview)
    //
    65,   165,   165,   165,   165,  165, // 11 (21H2)
    65,   165,   165,   165,   165,  165, // 11 (22H2)
    65,   165,   165,   165,   165,  165, // 11 (23H2)
    65,   165,   165,   165,   165,  165, // 11
    65,   165,   165,   165,   165,  165, // 11
    65,   165,   165,   165,   165,  165, // 11 (?)
    65,   165,   165,   165,   165,  165, // 11 (?)
    65,   165,   165,   165,   165,  165, // 11 (insider preview)
    65,   165,   165,   165,   165,  165, // 11 (insider preview)
    65,   165,   165,   165,   165,  165, // 11 (insider preview)
    65,   165,   165,   165,   165,  165, // 11 (24H2 - Server 2025)
    65,   165,   165,   165,   165,  165, // 11 (25H2)
    //
    10,   100,   100,   100,   100,  100,
    10,   100,   100,   100,   100,  100,
    //
//    -1,
};

const markers_t markers[num_markers]=
{
    // Exact x86
    {"5x86",    5, 1, 0},
    {"5x86",    5, 2, 0},
    {"6x86",    6, 0, 0},
    {"7x86",    6, 1, 0},
    {"8x86",    6, 2, 0},
    {"81x86",   6, 3, 0},
    {"10x86",   6, 4, 0},             // to be confirmed
    {"10x86",  10, 0, 0},             // to be confirmed
    {"U10x86", 10, 0, 0},             // to be confirmed
    {"11x86",  11, 0, 0},             // to be confirmed

    {"67x86",   6, 0, 0},
    {"6xx86",   6, 0, 0},
    {"78x86",   6, 1, 0},
    {"781x86",  6, 1, 0},
    {"710x86",  6, 1, 0},             // to be confirmed
    {"88110x86",6, 3, 0},             // to be confirmed
    {"8110x86", 6, 3, 0},             // to be confirmed

    // Exact x64
    {"5x64",    5, 1, 1},             // XP
    {"5x64",    5, 2, 1},
    {"6x64",    6, 0, 1},
    {"7x64",    6, 1, 1},
    {"8x64",    6, 2, 1},
    {"81x64",   6, 3, 1},
    {"10x64",   6, 4, 1},             // to be confirmed
    {"10x64",  10, 0, 1},             // to be confirmed
    {"U10x64", 10, 0, 1},             // to be confirmed
    {"11x64",  11, 0, 1},             // to be confirmed

    {"67x64",   6, 0, 1},
    {"6xx64",   6, 0, 1},
    {"78x64",   6, 1, 1},
    {"781x64",  6, 1, 1},
    {"710x64",  6, 1, 1},             // to be confirmed
    {"88110x64",6, 3, 1},             // to be confirmed
    {"8110x64", 6, 3, 1},             // to be confirmed

    // exact ia64
    {"5ia64",   5, 1, 2},             // XP
    {"5ia64",   5, 2, 2},
    {"6ia64",   6, 0, 2},
    {"7ia64",   6, 1, 2},
    {"8ia64",   6, 2, 2},
    {"10ia64", 10, 0, 2},
    {"11ia64", 11, 0, 2},

    // exact arm
    {"5arm",    5, 1, 3},             // XP
    {"5arm",    5, 2, 3},
    {"6arm",    6, 0, 3},
    {"7arm",    6, 1, 3},
    {"8arm",    6, 2, 3},
    {"10arm",  10, 0, 3},
    {"11arm",  11, 0, 3},

    // exact arm64
    {"5arm64",  5, 1, 4},             // XP
    {"5arm64",  5, 2, 4},
    {"6arm64",  6, 0, 4},
    {"7arm64",  6, 1, 4},
    {"8arm64",  6, 2, 4},
    {"10arm64",10, 0, 4},
    {"11arm64",11, 0, 4},

    // Each OS, ignore arch
    {"allnt",   4, 0,-1},
    {"allxp",   5, 1,-1},
    {"allxp",   5, 2,-1},
    {"all6",    6, 0,-1},
    {"all7",    6, 1,-1},
    {"all8\\",  6, 2,-1},
    {"all81",   6, 3,-1},
    {"all10",   6, 4,-1},             // to be confirmed
    {"all10",  10, 0,-1},             // to be confirmed
    {"all11",  11, 0,-1},             // to be confirmed

    {"allntx64x86",4, 0, -1},             // to be confirmed
    {"all5x86x64",5, 1, -1},             // to be confirmed
    {"all5x86x64",5, 2, -1},             // to be confirmed
    {"all6x86x64",6, 0, -1},             // to be confirmed
    {"all7x86x64",6, 1, -1},             // to be confirmed
    {"all8x86x64",6, 2, -1},             // to be confirmed
    {"all10x86x64",10, 0, -1},             // to be confirmed
    {"all11x86x64",11, 0, -1},             // to be confirmed

    // arch
    {"allx86",  -1, -1, 0},
    {"allx64",  -1, -1, 1},
    {"all8x86",  6,  2, 0},
    {"all8x64",  6,  2, 1},
    {"allia64", -1, -1, 2},
    {"allarm",  -1, -1, 3},
    {"allarm64",-1, -1, 4},
    {"ntx86",   -1, -1, 0},
    {"ntx64",   -1, -1, 1},
    {"x86\\",   -1, -1, 0},
    {"x64\\",   -1, -1, 1},

    {"winall", -1,-1,-1},
//    ("", -1,-1,-1),
};

char marker[MAX_INF_STRING_LENGTH];
int isLaptop;
//}

//{ Calc
void State::genmarker()
{
    static const wchar_t *Filter_1[]={L"Acer",L"acer",L"emachines",L"packard",L"bell",L"gateway",L"aspire",nullptr};
    static const wchar_t *Filter_2[]={L"Apple",L"apple",nullptr};
    static const wchar_t *Filter_3[]={L"Asus",L"asus",nullptr};
    static const wchar_t *Filter_4[]={L"OEM",L"clevo",L"eurocom",L"sager",L"iru",L"viewsonic",L"viewbook",nullptr};
    static const wchar_t *Filter_5[]={L"Dell",L"dell",L"alienware",L"arima",L"jetway",L"gericom",nullptr};
    static const wchar_t *Filter_6[]={L"Fujitsu",L"fujitsu",L"sieme",nullptr};
    static const wchar_t *Filter_7[]={L"OEM",L"ecs",L"elitegroup",L"roverbook",L"rover",L"shuttle",nullptr};
    static const wchar_t *Filter_8[]={L"HP",L"hp",L"hewle",L"compaq",nullptr};
    static const wchar_t *Filter_9[]={L"OEM",L"intel",L"wistron",nullptr};
    static const wchar_t *Filter_10[]={L"Lenovo",L"lenovo",L"compal",L"ibm",nullptr};
    static const wchar_t *Filter_11[]={L"LG",L"lg",nullptr};
    static const wchar_t *Filter_12[]={L"OEM",L"mitac",L"mtc",L"depo",L"getac",nullptr};
    static const wchar_t *Filter_13[]={L"MSI",L"msi",L"micro-star",nullptr};
    static const wchar_t *Filter_14[]={L"Panasonic",L"panasonic",L"matsushita",nullptr};
    static const wchar_t *Filter_15[]={L"OEM",L"quanta",L"prolink",L"nec",L"k-systems",L"benq",L"vizio",nullptr};
    static const wchar_t *Filter_16[]={L"OEM",L"pegatron",L"medion",nullptr};
    static const wchar_t *Filter_17[]={L"Samsung",L"samsung",nullptr};
    static const wchar_t *Filter_18[]={L"Gigabyte",L"gigabyte",nullptr};
    static const wchar_t *Filter_19[]={L"Sony",L"sony",L"vaio",nullptr};
    static const wchar_t *Filter_20[]={L"Toshiba",L"toshiba",nullptr};
    static const wchar_t *Filter_21[]={L"OEM",L"twinhead",L"durabook",nullptr};
    static const wchar_t *Filter_22[]={L"NEC",L"Nec_brand",L"nec",nullptr};

    static const wchar_t **filter_list[num_filters]=
    {
        Filter_1, Filter_2, Filter_3, Filter_4, Filter_5, Filter_6, Filter_7,
        Filter_8, Filter_9, Filter_10,Filter_11,Filter_12,Filter_13,Filter_14,
        Filter_15,Filter_16,Filter_17,Filter_18,Filter_19,Filter_20,Filter_21,
        Filter_22,
    };

    wsprintfA(marker,"OEM_nb");
    const wchar_t *str=getManuf();
    if(!str)return;

    for(size_t i=0;i<num_filters;i++)
    for(size_t j=1;filter_list[i][j];j++)
        if(StrStrIW(str,filter_list[i][j]))
            wsprintfA(marker,"%S_nb",filter_list[i][0]);

    Log.print_con("Marker: '%s'\n",marker);
}

int calc_secttype(const char *s)
{
    char buf[MAX_INF_SECTION_NAME_LENGTH];
//    char *p=buf;

    s=StrStrIA(s,".nt");
    if(!s)return -1;
/*
    strcpy(buf,s);
    // looking for a match in nts[]

    if((p=strchr(p+1,'.')))
        if((p=strchr(p+1,'.')))
            if((p=strchr(p+1,'.')))
                if((p=strchr(p+1,'.')))
                    if((p=strchr(p+1,'.')))
                        if((p=strchr(p+1,'.')))*p=0;
*/

    // in order to get a match in nts[]
    // i need to ignore the ProductType and SuiteMask fields
    // but include the BuildNumber field
    // NT[Architecture][.[OSMajorVersion][.[OSMinorVersion][.[ProductType][.[SuiteMask][.[BuildNumber]]]]]
    std::string ss=s;
    std::string sections[7];
    std::string del=".";
    int ndx=0;
    auto pos=ss.find(del);
    while(pos!=std::string::npos && ndx<7)
    {
        sections[ndx]=ss.substr(0,pos);
        ss.erase(0,pos+del.length());
        pos=ss.find(del);
        ndx++;
    }
    if(ndx<7)sections[ndx]=ss;
    ss="";
    if(sections[1].size()>0)        // Architecture
        ss+="."+sections[1];
    if(sections[2].size()>0)        // OSMajorVersion
        ss+="."+sections[2];
    if(sections[3].size()>0)        // OSMinorVersion
        ss+="."+sections[3];
    if(sections[6].size()>0)        // BuildNumber
        ss+="..."+sections[6];

    strcpy(buf,ss.c_str());

    for(int i=0;i<num_decs;i++)if(!_strcmpi(buf+3,nts[i]+2))return i;
    return -1;
}

// calculate the inf win version decoration score
int Hwidmatch::calc_decorscore(int id,const State *state)
{
    int major,
        minor,
        build,
        arch=state->getArchitecture()+1;
    state->getWinVer(&major,&minor,&build);

    // id is the index into the nts, nts_version, nts_build, nts_arch, nts_score arrays
    if(id<0)return 1;

    // for the purposes of matching with the inf file decorator then 11 = 10
    if(major==11)major=10;

    // if the inf win version is greater than current os then fail
    if(nts_version[id]&&major*10+minor<nts_version[id])return 0;
    // if the inf win architecture is not current os then fail
    if(nts_arch[id]&&arch!=nts_arch[id])return 0;

    // starting with 10.0.14310, if major and minor match exactly
    // then nts_build is the minimum build number
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-manufacturer-section
    if(nts_version[id]&&nts_version[id]>=100&&nts_build[id]>=14310)
    {
        if(major*10+minor==nts_version[id]&&nts_build[id]>build)return 0;
    }

    // return the assigned score
    return nts_score[id];
}

int Hwidmatch::calc_markerscore(const State *state,const char *path)
{
    char buf[MAX_PATH];
    int majver,
        minver,
        build,
        arch=state->getArchitecture(),
        curmaj=-1,curmin=-1,curarch=-1;
    int i;
    int score_l=0;
    state->getWinVer(&majver,&minver,&build);

    strcpy(buf,path);
    strtolower(buf,strlen(buf));

    for(i=0;i<num_markers;i++)
    {
        if(StrStrIA(buf,markers[i].name))
        {
            score_l=1;
            if(markers[i].major==0){continue;}
            if(markers[i].major>curmaj)curmaj=markers[i].major;
            if(markers[i].minor>curmin)curmin=markers[i].minor;
            if(markers[i].arch>curarch)curarch=markers[i].arch;
        }
    }
/*
    +1  if at least one marker specified
    +2  if OS version allows
    +4  if arch allows
    +8  if OS version matches
    +16 if arch matches
*/
    if(curmaj>=0&&curmin>=0&&majver==curmaj&&minver==curmin)score_l|=8;
    if(curmaj>=0&&curmin>=0&&majver>=curmaj&&minver>=curmin)score_l|=2;
    if(curmaj<0&&score_l)score_l|=2;
    if(curarch>=0&&curarch==arch)score_l|=4+16;
    if(curarch<0&&score_l)score_l|=4;
    return score_l;
}
//}

//{ Misc
int cmpunsigned(unsigned a,unsigned b)
{
    if(a>b)return 1;
    if(a<b)return -1;
    return 0;
}
//}

//{ Matcher
class MatcherImp:public Matcher
{
    State *state=nullptr;
    Collection *col=nullptr;

    std::vector<Devicematch> devicematch_list;
    std::vector<Hwidmatch> hwidmatch_list;

private:
    void sort();

public:
    ~MatcherImp(){}

    void findHWIDs(Devicematch *device_match,const wchar_t *hwid,int dev_pos,int ishw);
    void init(State *state1,Collection *col1){state=state1;col=col1;devicematch_list.clear();hwidmatch_list.clear();}
    void populate();
    void print();
    void sorta(size_t *v);
    int write_device_list(wchar_t *filename);

    const wchar_t *finddrp(const wchar_t *s) override{return col->finddrp(s);}
    State *getState(){return state;}
    Collection *getCol(){return col;}
    size_t getDwidmatch_list() { return devicematch_list.size(); }
    Devicematch *getDevicematch_i(size_t i){return &devicematch_list[i];}
    size_t getHwidmatch_list(){return hwidmatch_list.size();}
    void Insert(const Hwidmatch &a){hwidmatch_list.push_back(a);}
    Hwidmatch *getHwidmatch_i(size_t i){return &hwidmatch_list[i];}
};

Matcher::~Matcher(){}
Matcher *CreateMatcher()
{
    return new MatcherImp();
}


void MatcherImp::findHWIDs(Devicematch *devicematch,const wchar_t *hwidv,int dev_pos,int ishw)
{
    // this searches the driver packs for the given hwid
    // each index has it's own HashTable
    // which will handle hash collisions within an index
    // but there is no accounting for possible hash collisions across indices
    // so: it's possible for the same hash to show up in different
    // indices for different device HWIDs with no collision handling

    char hwid[MAX_DEVICE_ID_LEN];
    wsprintfA(hwid,"%ws",hwidv);

    // calc hash from hwid
	size_t sz = strlen(hwid);
    strtoupper(hwid,sz);
	int code = Hashtable::gethashcode(hwid, sz);

	// iterate the driver packs
    for(auto &drp:*col->getList())
    {
        int isfound;
        // look for the hash
        int val=drp.find((int)code,&isfound);
        while(isfound)
        {
            // found a match - add it to a list
            hwidmatch_list.push_back(Hwidmatch(&drp,val,dev_pos,ishw,state,devicematch));
            devicematch->num_matches++;
            val=drp.findnext(&isfound);
        }
    }
}

void MatcherImp::sort()
{
    Hwidmatch *match1,*match2,*bestmatch;
    Hwidmatch matchtmp(nullptr,0);
    char sect1[MAX_INF_SECTION_NAME_LENGTH];

    for(auto &devicematch:devicematch_list)
    {
        // Sort
        match1=&hwidmatch_list[devicematch.start_matches];
        for(unsigned i=0;i+1<devicematch.num_matches;i++,match1++)
        {
            match2=&hwidmatch_list[devicematch.start_matches+i+1];
            bestmatch=match1;
            for(unsigned j=i+1;j<devicematch.num_matches;j++,match2++)
                if(bestmatch->cmp(match2)<0)bestmatch=match2;

            if(bestmatch!=match1)
            {
                memcpy(&matchtmp,match1,sizeof(Hwidmatch));
                memcpy(match1,bestmatch,sizeof(Hwidmatch));
                memcpy(bestmatch,&matchtmp,sizeof(Hwidmatch));
            }
        }

        // Mark dups
        match1=&hwidmatch_list[devicematch.start_matches];
        for(unsigned i=0;i+1<devicematch.num_matches;i++,match1++)
        {
            match1->getdrp_drvsection(sect1);

            match2=&hwidmatch_list[devicematch.start_matches+i+1];
            for(unsigned j=i+1;j<devicematch.num_matches;j++,match2++)
                if(match1->isdup(match2,sect1))match2->markDup();
        }
    }
}

void MatcherImp::populate()
{
    //Timers.start(time_matcher);

    isLaptop=state->isLaptop;
    //wcscpy(marker,state->marker);

    for(auto &cur_device:*state->getDevices_list())
    {
        const Driver *cur_driver=state->getCurrentDriver(&cur_device);
        devicematch_list.push_back(Devicematch(&cur_device,cur_driver,hwidmatch_list.size(),this));
    }
    sort();
    devicematch_list.shrink_to_fit();
    hwidmatch_list.shrink_to_fit();

    //Timers.stop(time_matcher);
}

void MatcherImp::print()
{
    // dump all the device/matcher info to the log file
    int limits[7];

    if(Log.isHidden(LOG_VERBOSE_MATCHER))return;
    Log.print_file("\n{matcher_print[devices=%d,hwids=%d]\n",devicematch_list.size(),hwidmatch_list.size());
    for(auto &devicematch:devicematch_list)
    {
        devicematch.device->print(state);
        Log.print_file("DriverInfo\n");
        if(devicematch.driver)
            devicematch.driver->print(state);
        else
            Log.print_file("  NoDriver\n");

        memset(limits,0,sizeof(limits));
        Hwidmatch *hwidmatch;
        hwidmatch=&hwidmatch_list[devicematch.start_matches];
        for(unsigned j=0;j<devicematch.num_matches;j++,hwidmatch++)
            hwidmatch->calclen(limits);

        Log.print_file("  altsectscore | score | date | decorscore | markerscore | status | drvsection | packname | infcrc | inffile | manuf | version | HWID | drvdesc\n");

        hwidmatch=&hwidmatch_list[devicematch.start_matches];
        for(unsigned j=0;j<devicematch.num_matches;j++,hwidmatch++)
            hwidmatch->print_tbl(limits);
        Log.print_file("\n");
    }
    Log.print_file("}matcher_print\n\n");
}

int MatcherImp::write_device_list(wchar_t *filename)
{
    if(!System.canWriteFile(filename,L"wt"))
    {
        Log.print_err("ERROR in write_device_list(): Unwriteable,'%S'\n",filename);
        return 1;
    }

    FILE *f=_wfopen(filename,L"wt");
    if(!f)
    {
        Log.print_err("ERROR in write_device_list(): Failed to open file,'%S'\n",filename);
        return 1;
    }

    wchar_t buffer[1024];
    int n;
    const Txt *txt=&state->textas;

    for(auto &devicematch:devicematch_list)
    {
        n=wsprintf(buffer,L"[Device]\nName: %s\nDescription: %s\nManufacturer: %s\nHardware ID: %s\nDriver: %s\n\n",
                   txt->get(devicematch.device->getFriendlyName()),
                   txt->get(devicematch.device->getDescr()),
                   txt->get(devicematch.device->getMfg()),
                   txt->get(devicematch.device->getHardwareID()),
                   txt->get(devicematch.device->getDriver()));
        if(n)
        {
            fputws(buffer,f);

            if(devicematch.driver)
            {
                n=wsprintf(buffer,L"[Driver]\nDescription: %s\nProvider: %s\nDate: %s\nVersion: %s\nMatching Device ID: %s\nInf Path: %s\nInf Section: %s\n\n",
                           txt->get(devicematch.driver->getDriverDesc()),
                           txt->get(devicematch.driver->getProviderName()),
                           txt->get(devicematch.driver->getDriverDate()),
                           txt->get(devicematch.driver->getDriverVersion()),
                           txt->get(devicematch.driver->getMatchingDeviceId()),
                           txt->get(devicematch.driver->getInfPath()),
                           txt->get(devicematch.driver->getInfSection()));
                if(n)fputws(buffer,f);
            }
        }
    }
    fclose(f);
    return 0;
}

void MatcherImp::sorta(size_t *v)
{
    Devicematch *devicematch_i,*devicematch_j;
    Hwidmatch *hwidmatch_i,*hwidmatch_j;
    size_t i,j;
	size_t num;

    num=devicematch_list.size();

    for(i=0;i<num;i++)v[i]=i;

    for(i=0;i<num;i++)
    {
        for(j=i+1;j<num;j++)
        {
            devicematch_i=&devicematch_list[v[i]];
            devicematch_j=&devicematch_list[v[j]];
            hwidmatch_i=(devicematch_i->num_matches)?&hwidmatch_list[devicematch_i->start_matches]:nullptr;
            hwidmatch_j=(devicematch_j->num_matches)?&hwidmatch_list[devicematch_j->start_matches]:nullptr;
            int ismi=devicematch_i->isMissing(state);
            int ismj=devicematch_j->isMissing(state);

            if(ismi<ismj)
            {
                size_t t;

                t=v[i];
                v[i]=v[j];
                v[j]=t;
            }
            else
            if(ismi==ismj)
            if((hwidmatch_i&&hwidmatch_j&&hwidmatch_i->cmpnames(hwidmatch_j)>0)
               ||
               (!hwidmatch_i&&hwidmatch_j))
            {
                size_t t;

                t=v[i];
                v[i]=v[j];
                v[j]=t;
            }
        }
    }
}
//}

//{ Devicematch
Devicematch::Devicematch(Device *cur_device,const Driver *cur_driver,size_t items,Matcher *matcher):
    start_matches(items),
    num_matches(0),
    device(cur_device),
    driver(cur_driver)
{
    // this looks for a matching hardware id in the driver packs

    State *state=matcher->getState();

    if(device->getHardwareID())
    {
        // p is the PBYTE pointing to a REG_MULTI_SZ
        const wchar_t *p=state->textas.getw(device->getHardwareID());
        int dev_pos=0;
        // iterate the null terminated strings contained in the REG_MULTI_SZ
        while(*p)
        {
            // look for this hwid string in the index
            matcher->findHWIDs(this,p,dev_pos,1);
            p+=wcslen(p)+1;
            dev_pos++;
        }
    }

    // do the same for CompatibleIDs
    if(device->getCompatibleIDs())
    {
        const wchar_t *p=state->textas.getw(device->getCompatibleIDs());
        int dev_pos=0;
        while(*p)
        {
            matcher->findHWIDs(this,p,dev_pos,0);
            p+=wcslen(p)+1;
            dev_pos++;
        }
    }
    if(num_matches==0)
    {
        matcher->Insert(Hwidmatch(nullptr,0));

        if(isMissing(state))
            status=STATUS_NF_MISSING;
        else if(driver&&StrStrIW(state->textas.getw(driver->getInfPath()),L"oem"))
            status=STATUS_NF_UNKNOWN;
        else
            status=STATUS_NF_STANDARD;
    }
}

const GUID nonPnP={0x8ecc055d,0x047f,0x11d1,{0xa5,0x37,0x00,0x00,0xf8,0x75,0x3e,0xd1}};
int Devicematch::isMissing(const State *state)
{
    if(device->getProblem()==CM_PROB_DISABLED)return 0;
    if(device->getProblem()&&device->getHardwareID())return 1;
    if(!driver)
    {
        if(StrStrIW(device->getHWIDby(0,state),L"USBPRINT"))return 1;
        if(StrStrIW(device->getHWIDby(0,state),L"DOT4PRT"))return 1;
        if(StrStrIW(device->getHWIDby(0,state),L"BTHENUM"))return 1;
        //if(memcmp(device->getGUID(),&nonPnP,sizeof(GUID)))return 1;
    }
    if(driver&&!_wcsicmp(state->textas.getw(driver->getMatchingDeviceId()),L"PCI\\CC_0300"))return 1;
    return 0;
}
//}

//{ Hwidmatch
int Hwidmatch::isvalid_usb30hub(const State *state,const wchar_t *str)
{
    if(StrStrIW(state->textas.getw(devicematch->device->getHardwareID()),str))return 1;
    return 0;
}

int Hwidmatch::isblacklisted(const State *state,const wchar_t *hwid,const char *section)
{
    if(StrStrIW(state->textas.getw(devicematch->device->getHardwareID()),hwid))
    {
        char buf[256];
        getdrp_drvsection(buf);
        if(StrStrIA(buf,section))return 1;
    }
    return 0;
}

int Hwidmatch::isvalid_ver(const State *state)
{
    Version *v;
    int major,minor,build;
    state->getWinVer(&major,&minor,&build);

    v=getdrp_drvversion();
    switch(v->GetV1())
    {
        case 5:if(major!=5)return 0;break;
        case 6:if(major==5)return 0;break;
        case 106:if(major!=6||minor!=0)return 0;break;
        default:break;
    }
    return 1;
}

int Hwidmatch::calc_notebook()
{
    if(StrStrIA(getdrp_infpath(),"_nb\\")||
       StrStrIA(getdrp_infpath(),"Touchpad_Mouse\\"))
    {
        if(!isLaptop)return 0;
        if(!*marker)return 0;
        if(!StrStrIA(getdrp_infpath(),marker))return 0;
    }
    return 1;
}

int Hwidmatch::calc_catalogfile()
{
    int r=0,i;

    for(i=CatalogFile;i<=CatalogFile_ntamd64;i++)
        if(*getdrp_drvfield(i))r+=1<<i;

    return r;
}

int Hwidmatch::calc_altsectscore(const State *state,int curscore)
{
    size_t desc_index,manufacturer_index;

    desc_index=drp->HWID_list[HWID_index].desc_index;
    manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    int numsects=drp->manufacturer_list[manufacturer_index].sections_n;

    for(int pos=0;pos<numsects;pos++)
    {
        char buf[256];
        drp->getdrp_drvsectionAtPos(buf,pos,manufacturer_index);
        if(calc_decorscore(calc_secttype(buf),state)>curscore)
            return 0;
    }

    if(!calc_notebook())return 0;

    const char *intel2="intel_2nd\\";
    const char *intel4="intel_4th\\";
    {
        const wchar_t *s=getdrp_packname();
        int v=0;
        while(*s)
        {
            if(*s=='_'&&s[1]>='0'&&s[1]<='9')
            {
                v=_wtoi_my(s+1);
                break;
            }
            s++;
        }
        //Log.print_con("%S: %d\n",getdrp_packname(),v);
        if(v&&v>16073)
        {
            intel2="intel_sdi_2nd\\";
            intel4="intel_sdi_4th\\";
        }
    }

    if(StrStrIA(getdrp_infpath(),intel2))
        if(!isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_1E31"))return 0;

    if(StrStrIA(getdrp_infpath(),intel4))
        if(!isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_8C31")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_8D31")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_8C7F")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_9C7F")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_9C31")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_9CB1")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_A12F")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_A22F")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_9D2F")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_A2AF")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_22B5")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_15B5")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_15B6")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_15C1")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_15DB")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_15D4")&&
//           !isvalid_usb30hub(hwidmatch,state,L"pnp0a08")&&
           !isvalid_usb30hub(state,L"IUSB3\\ROOT_HUB30&VID_8086&PID_0F35"))return 0;

    if(StrStrIA(getdrp_infpath(),"matchver\\")||
       StrStrIA(getdrp_infpath(),"L\\Realtek\\")||
       StrStrIA(getdrp_infpath(),"L\\R\\"))
        if(!isvalid_ver(state))return 0;

    if(isblacklisted(state,L"VEN_168C&DEV_002B&SUBSYS_30A117AA","Realtek"))return 0;

    if(StrStrIA(getdrp_infpath(),"matchmarker\\"))
        if((calc_markerscore(state,getdrp_infpath())&7)!=7)return 0;

    if(Settings.flags&FLAG_FILTERSP)return 2;

    if(StrStrIA(getdrp_infpath(),"tweak"))return 1;
    if(StrStrIA(getdrp_infname(),"tweak"))return 1;
    return isvalidcat(state)?2:1;
}

int Hwidmatch::calc_status(const State *state)
{
    int r=0;
    const Driver *cur_driver=devicematch->driver;

    if(devicematch->isMissing(state))return STATUS_MISSING;

    if(devicematch->driver)
    {
        if(getdrp_drvversion())
        {
            int res=cmpdate(devicematch->driver->getVersion(),getdrp_drvversion());
            if(res<0)r+=STATUS_NEW;else
            if(res>0)r+=STATUS_OLD;else
                r+=STATUS_CURRENT;
        }

        int scorev=cur_driver->calc_score_h(state);
        int res=cmpunsigned(scorev,score);
        if(r&STATUS_CURRENT&&StrStrIA(getdrp_infpath(),"feature_"))
            res=cmpunsigned(scorev&0xFF00FFFF,score&0xFF00FFFF);
        if(res>0)r+=STATUS_BETTER;else
        if(res<0)r+=STATUS_WORSE;else
            r+=STATUS_SAME;
    }
    else
        r+=STATUS_BETTER;

    if(!altsectscore)r+=STATUS_INVALID;
    return r;
}

Hwidmatch::Hwidmatch(Driverpack *drp1,int HWID_index1,int dev_pos,int ishw,State *state,Devicematch *devicematch1):
    drp(drp1),
    HWID_index(HWID_index1),
    devicematch(devicematch1)
{
    char buf[256];

    getdrp_drvsection(buf);

    identifierscore=calc_identifierscore(dev_pos,ishw,drp->HWID_list[HWID_index].inf_pos);
    decorscore=calc_decorscore(calc_secttype(buf),state);
    markerscore=calc_markerscore(state,getdrp_infpath());
    altsectscore=calc_altsectscore(state,decorscore);
    score=calc_score(calc_catalogfile(),getdrp_drvfeature(),
        identifierscore,state,StrStrIA(getdrp_drvinstallPicked(),".nt")?1:0);
    status=calc_status(state);
}

Hwidmatch::Hwidmatch(Driverpack *drp1,int HWID_index1):
    drp(drp1),
    HWID_index(HWID_index1)
{
}

/*
0 section
1 driverpack
2 inffile
3 manuf
4 version
5 hwid
6 desc
*/

void Hwidmatch::minlen(const char *s,int *len)
{
	int l=(int)strlen(s);
    if(*len<l)*len=l;
}

void Hwidmatch::calclen(int *limits)
{
    Version *v;
    char buf[FILENAME_MAX];
    WStringShort vers;

    getdrp_drvsection(buf);
    v=getdrp_drvversion();
    v->str_version(vers);

    minlen(buf,&limits[0]);
    wsprintfA(buf,"%ws%ws",getdrp_packpath(),getdrp_packname());
    minlen(buf,&limits[1]);
    wsprintfA(buf,"%s%s",getdrp_infpath(),getdrp_infname());
    minlen(buf,&limits[2]);
    minlen(getdrp_drvmanufacturer(),&limits[3]);
    wsprintfA(buf,"%ws",vers.Get());
    minlen(buf,&limits[4]);
    minlen(getdrp_drvHWID(),&limits[5]);
    minlen(getdrp_drvdesc(),&limits[6]);
}

void Hwidmatch::print_tbl(const int *limits)
{
    char buf[FILENAME_MAX];
    Version *v;
    WStringShort date;
    WStringShort vers;

    v=getdrp_drvversion();
    v->str_date(date,true);
    v->str_version(vers);

    Log.print_file("  %d |",      altsectscore);
    Log.print_file(" %08X |",     score);
    Log.print_file(" %S |",       date.Get());
    Log.print_file(" %3d |",      decorscore);
    Log.print_file(" %2d |",      markerscore);
    Log.print_file(" %3X |",      status);
        getdrp_drvsection(buf);
    Log.print_file(" %-*s |",limits[0],buf);

    wsprintfA(buf,"%ws\\%ws",     getdrp_packpath(),getdrp_packname());
    Log.print_file(" %-*s |",     limits[1],buf);
    Log.print_file(" %8X|",       getdrp_infcrc());
    wsprintfA(buf,"%s%s",         getdrp_infpath(),getdrp_infname());
    Log.print_file(" %-*s |",     limits[2],buf);
    Log.print_file(" %-*s |",     limits[3],    getdrp_drvmanufacturer());
    wsprintfA(buf,"%ws",          vers.Get());
    Log.print_file(" %*s |",      limits[4],buf);
    Log.print_file(" %-*s |",     limits[5],    getdrp_drvHWID());
    Log.print_file(" %-*s",       limits[6],      getdrp_drvdesc());
    Log.print_file("\n");
}

void Hwidmatch::print_hr()
{
    char buf[FILENAME_MAX];
    Version *v;
    WStringShort date;
    WStringShort vers;

    v=getdrp_drvversion();
    v->str_date(date,true);
    v->str_version(vers);
/*    log_file("  Alt:   %d\n",               altsectscore);
    log_file("  Decor: %3d\n",               decorscore);
    log_file("  CRC:  %8X%\n",              getdrp_infcrc(this));
    log_file("  Marker %d\n",                markerscore);
    log_file("  Status %3X\n",               status);*/
    Log.print_file("  Pack:     %S\\%S\n",       getdrp_packpath(),getdrp_packname());

    Log.print_file("  Name:     %s\n",     getdrp_drvdesc());
    Log.print_file("  Provider: %s\n",     getdrp_drvmanufacturer());
    Log.print_file("  Date:     %S\n",     date.Get());
    Log.print_file("  Version:  %S\n",     vers.Get());
    Log.print_file("  HWID:     %s\n",     getdrp_drvHWID());
    getdrp_drvsection(buf);
    Log.print_file("  inf:      %s%s,%s\n",getdrp_infpath(),getdrp_infname(),buf);
    Log.print_file("  Score:    %08X\n",   score);
    Log.print_file("\n");
}

void Hwidmatch::popup_driverline(int *limits,Canvas &canvas,int y,int mode,size_t index)
{
    char buf[256];
    wchar_t bufw[256];
    Version *v=getdrp_drvversion();

    textdata_horiz_t td(canvas,Popup->getShift(),limits,mode);
    td.y=y;
    td.col=0;

    if(!altsectscore)td.col=D_C(POPUP_LST_INVALID_COLOR);
    else
    {
        if(status&STATUS_BETTER||status&STATUS_NEW)td.col=D_C(POPUP_LST_BETTER_COLOR);else
        if(status&STATUS_WORSE||status&STATUS_OLD)td.col=D_C(POPUP_LST_WORSE_COLOR);else
        td.col=D_C(POPUP_TEXT_COLOR);
    }

    WStringShort date;
    WStringShort vers;

    v->str_date(date);
    v->str_version(vers);

    td.TextOutP(L"$%04d",index);
    td.TextOutP(L"| %d",altsectscore);
    td.TextOutP(L"| %08X",score);
    td.TextOutP(L"| %s",date.Get());
    td.TextOutP(L"| %3d",decorscore);
    td.TextOutP(L"| %d",markerscore);
    td.TextOutP(L"| %3X",status);getdrp_drvsection(buf);
    td.TextOutP(L"| %S",buf);
    td.TextOutP(L"| %s\\%s",getdrp_packpath(),getdrp_packname());
    td.TextOutP(L"| %08X",getdrp_infcrc());
    td.TextOutP(L"| %S%S",getdrp_infpath(),getdrp_infname());
    td.TextOutP(L"| %S",getdrp_drvmanufacturer());
    td.TextOutP(L"| %s",vers.Get());
    td.TextOutP(L"| %S",getdrp_drvHWID());wsprintf(bufw,L"%S",getdrp_drvdesc());
    td.TextOutP(L"| %s",bufw);
}

int Hwidmatch::cmp(const Hwidmatch *match2)
{
    int res;

    res=altsectscore-match2->altsectscore;
    if(res)return res;

    res=cmpunsigned(score,match2->score);
    if(res)return -res;

    res=cmpdate(getdrp_drvversion(),match2->getdrp_drvversion());
    if(res)return res;

    res=decorscore-match2->decorscore;
    if(res)return res;

    res=markerscore-match2->markerscore;
    if(res)return res;

    res=(status&~STATUS_DUP)-(match2->status&~STATUS_DUP);
    if(res)return res;

    return 0;
}

int Hwidmatch::isdup(const Hwidmatch *match2,const char *sect1)
{
    char sect2[MAX_INF_SECTION_NAME_LENGTH];
    match2->getdrp_drvsection(sect2);

    if(getdrp_infcrc()==match2->getdrp_infcrc()&&
        !strcmp(getdrp_drvHWID(),match2->getdrp_drvHWID())&&
        !strcmp(sect1,sect2))return 1;
    return 0;
}

int Hwidmatch::isdrivervalid()
{
    if(altsectscore>0&&decorscore>0)return 1;
    return 0;
}

int Hwidmatch::isvalidcat(const State *state)
{
    char bufa[8];
    int n=pickcat(state);
    const char *s=getdrp_drvcat(n);
    if(!*s)return 0;

    int major,minor,build;
    state->getWinVer(&major,&minor,&build);

    // for the purposes of matching with the inf file then 11 = 10
    if(major==11)major=10;

    wsprintfA(bufa,"2:%d.%d",major,minor);
    int res=strstr(s,bufa)?1:0;
    return res;
}

int Hwidmatch::pickcat(const State *state)
{
    // if amd64 and catalog found then return that
    if(state->getArchitecture()==1&&*getdrp_drvcat(CatalogFile_ntamd64))
    {
        return CatalogFile_ntamd64;
    }
    // else if x86 catalog found then return that
    else if(*getdrp_drvcat(CatalogFile_ntx86))
    {
        return CatalogFile_ntx86;
    }

    // if nt catalog found then return that
    if(*getdrp_drvcat(CatalogFile_nt))
       return CatalogFile_nt;

    // else if generic catalog found then return that
    if(*getdrp_drvcat(CatalogFile))
       return CatalogFile;

    return 0;
}
int Hwidmatch::cmpnames(const Hwidmatch *match2)
{
    if(wcsstr(drp->getFilename(),L"unpacked.7z"))
        return strcmp(getdrp_infpath(),match2->getdrp_infpath());
    else
        return wcscmp(getdrp_packname(),match2->getdrp_packname());
}
//}

//{ Getters
//driverpack
const wchar_t *Hwidmatch::getdrp_packpath()const
{
    return drp->getPath();
}
const wchar_t *Hwidmatch::getdrp_packname()const
{
    return drp->getFilename();
}
void Hwidmatch::getdrp_packnameVirtual(WStringShort &s)const
{
    const wchar_t *t=drp->getFilename();

    if(wcsstr(t,L"unpacked.7z"))
        s.sprintf(L"%S",getdrp_infpath());
    else
        s.sprintf(L"%s",t);
}
int Hwidmatch::getdrp_packontorrent()const
{
    return drp->type==DRIVERPACK_TYPE_UPDATE;
}

//inf file
const char *Hwidmatch::getdrp_infpath()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;
    return drp->text_ind.get(drp->inffile[inffile_index].infpath);
}
std::string Hwidmatch::getdrp_infmarker()
{
    std::string path;
    std::string buf;
    std::string::size_type pos,len;

    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;

    // this is the full path of the inf file in the driver pack
    path=drp->text_ind.get(drp->inffile[inffile_index].infpath);
    // make it lower case
    std::transform(path.begin(), path.end(),path.begin(), ::tolower);
    // iterate my list of known markers
    for(int i=0;i<num_markers;i++)
    {
        // wrap it in backslashes
        buf=markers[i].name;
        len=buf.length();
        buf="\\"+buf+"\\";
        // see if this known marker is in the inf path
        pos=path.find(buf);
		if (pos != std::string::npos)
        {
            // get the un-lower case path
            path=drp->text_ind.get(drp->inffile[inffile_index].infpath);
            // get the marker from the path with the correct case
            buf=path.substr(pos+1,len);
            return buf;
        }
    }
    return "";
}
const char *Hwidmatch::getdrp_infname()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;
    return drp->text_ind.get(drp->inffile[inffile_index].inffilename);
}
const char *Hwidmatch::getdrp_drvfield(int n)const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;
    if(!drp->inffile[inffile_index].fields[n])return "";
    return drp->text_ind.get(drp->inffile[inffile_index].fields[n]);
}
const char *Hwidmatch::getdrp_drvcat(int n)const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;
    if(!drp->inffile[inffile_index].cats[n])return "";
    return drp->text_ind.get(drp->inffile[inffile_index].cats[n]);
}
Version *Hwidmatch::getdrp_drvversion()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;
    return &drp->inffile[inffile_index].version;
}
int Hwidmatch::getdrp_infsize()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;
    return drp->inffile[inffile_index].infsize;
}
int Hwidmatch::getdrp_infcrc()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    size_t inffile_index=drp->manufacturer_list[manufacturer_index].inffile_index;
    return drp->inffile[inffile_index].infcrc;
}

//manufacturer
const char *Hwidmatch::getdrp_drvmanufacturer()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    return drp->text_ind.get(drp->manufacturer_list[manufacturer_index].manufacturer);
}
void Hwidmatch::getdrp_drvsection(char *buf)const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    size_t manufacturer_index=drp->desc_list[desc_index].manufacturer_index;
    drp->getdrp_drvsectionAtPos(buf,drp->desc_list[desc_index].sect_pos,manufacturer_index);
}

//desc
const char *Hwidmatch::getdrp_drvdesc()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    return drp->text_ind.get(drp->desc_list[desc_index].desc);
}
const char *Hwidmatch::getdrp_drvinstall()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    return drp->text_ind.get(drp->desc_list[desc_index].install);
}
const char *Hwidmatch::getdrp_drvinstallPicked()const
{
    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    return drp->text_ind.get(drp->desc_list[desc_index].install_picked);
}
int Hwidmatch::getdrp_drvfeature()const
{
    char *p;
    p=StrStrIA(getdrp_infpath(),"feature_");
    if(p)return atoi(p+8);

    size_t desc_index=drp->HWID_list[HWID_index].desc_index;
    return drp->desc_list[desc_index].feature&0xFF;
}

//HWID
int Hwidmatch::getdrp_drvinfpos()const
{
    return drp->HWID_list[HWID_index].inf_pos;
}
const char *Hwidmatch::getdrp_drvHWID()const
{
    return drp->text_ind.get(drp->HWID_list[HWID_index].HWID);
}
//}
