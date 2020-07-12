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


#ifndef SVNREV_H
#define SVNREV_H
#define SVN_MODIFIED ""
#define SVN_REV 1811

/* Date of the last commit */
#define SVN_REV_D "$WCNOW=%d$"
#define SVN_REV_M "$WCNOW=%m$"
#define SVN_REV_Y $WCNOW=%Y$

#if $WCMODS?1:0$
#define SVN_REV_STR "1.18.11"
#define SVN_REV2    "1.18.11"
#define SVN_BUILD_NOTE "" SVN_MODIFIED
#else
#define SVN_REV_STR "1.18.11"
#define SVN_REV2    "1.18.11"
#define SVN_BUILD_NOTE ""
#endif

#define SVN_BUILD_REV "Revision: 1811"
#define SVN_BUILD_DATE "Build Date: $WCNOW=%d$ $WCNOW=%b$ $WCNOW=%Y$"
#define COPYRIGHT_DATE "(C) Copyright $WCNOW=%Y$"

#endif
