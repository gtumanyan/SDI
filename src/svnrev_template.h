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

#ifndef SVNREV_H
#define SVNREV_H
#define SVN_REV $WCREV$
#define SVN_REV_STR "$WCREV$"

/* Date of the last commit */
#define SVN_REV_D "$WCDATE=%d$"
#define SVN_REV_M "$WCDATE=%m$"
#define SVN_REV_Y $WCDATE=%Y$

#if $WCMODS?1:0$
#define SVN_BUILD_NOTE "TEST BUILD - NOT FOR DISTRIBUTION"
#else
#define SVN_BUILD_NOTE ""
#endif

#define SVN_BUILD_REV "Revision: R$WCREV$"
#define SVN_BUILD_DATE "Build Date: $WCNOW=%d$ $WCNOW=%b$ $WCNOW=%Y$"
#define COPYRIGHT_DATE "(C) Copyright $WCNOW=%Y$"

#endif
