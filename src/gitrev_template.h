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

#ifndef GITREV_H
#define GITREV_H
#define GIT_REV $WCREV$
#define GIT_REV_STR "$WCREV$"

/* Date of the last commit */
#define GIT_REV_D "$WCDATE=%d$"
#define GIT_REV_M "$WCDATE=%m$"
#define GIT_REV_Y $WCDATE=%Y$

#if $WCMODS?1:0$
#define GIT_BUILD_NOTE "TEST BUILD - NOT FOR DISTRIBUTION"
#else
#define GIT_BUILD_NOTE ""
#endif

#define GIT_BUILD_REV "Revision: R$WCREV$"
#define GIT_BUILD_DATE "Build Date: $WCNOW=%d$ $WCNOW=%b$ $WCNOW=%Y$"
#define COPYRIGHT_DATE "(C) Copyright $WCNOW=%Y$"

#endif
