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

#pragma once

//{ Defines
#define safe_release_dc(hDlg, hDC) do { if ((hDC != INVALID_HANDLE_VALUE) && (hDC != NULL)) { ReleaseDC(hDlg, hDC); hDC = NULL; } } while(0)
#define safe_delete_object(hObj) do { if (hObj != NULL) { DeleteObject(hObj); hObj = NULL; } } while(0)
class Manager;
class wFont;
class Matcher;
class Hwidmatch;
class Canvas;
class Console_t;
class Combobox;

extern class Popup_t *Popup;

#include "resource.h"

// VersionEx.h.tpl
#define APPTITLE            MY_APPNAME
#define VER_MARKER          "SDW"
#define VER_STATE           0x102
#define VER_INDEX           0x205
/*
 * Structure and macros used for the extensions specification of FileDialog()
 * You can use:
 *   EXT_DECL(my_extensions, "default.std", __VA_GROUP__("*.std", "*.other"), __VA_GROUP__("Standard type", "Other Type"));
 * to define an 'ext_t my_extensions' variable initialized with the relevant attributes.
 */
typedef struct ext_t {
	size_t count;
	const char* filename;
	const char** extension;
	const char** description;
} ext_t;

#ifndef __VA_GROUP__
#define __VA_GROUP__(...)  __VA_ARGS__
#endif
#define EXT_X(prefix, ...) const char* _##prefix##_x[] = { __VA_ARGS__ }
#define EXT_D(prefix, ...) const char* _##prefix##_d[] = { __VA_ARGS__ }
#define EXT_DECL(var, filename, extensions, descriptions)                   \
	EXT_X(var, extensions);                                                 \
	EXT_D(var, descriptions);                                               \
	ext_t var = { ARRAYSIZE(_##var##_x), filename, _##var##_x, _##var##_d }

// Mode
enum INVALIDATE
{
    INVALIDATE_INDEXES  =1,
    INVALIDATE_SYSINFO  =2,
    INVALIDATE_DEVICES  =4,
    INVALIDATE_MANAGER  =8,
};

// Popup window
enum FLOATING_TYPE
{
    FLOATING_NONE       =0,
    FLOATING_TOOLTIP    =1,
    FLOATING_SYSINFO    =2,
    FLOATING_CMPDRIVER  =3,
    FLOATING_DRIVERLST  =4,
    FLOATING_ABOUT      =5,
    FLOATING_DOWNLOAD   =6,
};

// kb panels
enum KB_ID
{
    KB_NONE           =  0,
    KB_FIELD          =  1,
    KB_LANG           =  2,
    KB_THEME          =  3,
    KB_EXPERT         =  4,
    KB_INSTALL        =  5,
    KB_ACTIONS        =  6,
    KB_PANEL1         =  7,
    KB_PANEL2         =  8,
    KB_PANEL3         =  9,
    KB_PANEL_CHK      = 10,
};

// Mouse state
enum MOUSE_STATE
{
    MOUSE_NONE         = 0,
    MOUSE_CLICK        = 1,
    MOUSE_MOVE         = 2,
    MOUSE_SCROLL       = 3,
};

// torrents
enum TORRENT_SELECTION_MODE
{
    TSM_NONE           = 0,
    TSM_AUTO           = 1
};
//}

/*
 * Globals
 */
// Manager
extern Console_t *Console;

// Console
class Console_t
{
public:
    virtual ~Console_t(){}
    virtual void Show()=0;
    virtual void Hide()=0;
};

// Subroutes
void drp_callback(const wchar_t *szFile,int action,int lParam);
void invalidate(int v);

// Misc
void escapeAmpUrl(wchar_t *buf,const wchar_t *source);
void escapeAmp(wchar_t *buf,const wchar_t *source);

// GUI Helpers
void checktimer(const wchar_t *str,long long t,int uMsg);

// GUI
BOOL CALLBACK WelcomeProcedure(HWND hwnd,UINT Message,WPARAM wParam,LPARAM lParam);

//#include "debug_new.h"
void* operator new(size_t size, const char* file, int line);
void* operator new[](size_t size, const char* file, int line);
/*#define DEBUG_NEW new(__FILE__, __LINE__)
#define new DEBUG_NEW
namespace nvwa {extern size_t total_mem_alloc;}*/
//}

#if defined(_MSC_VER)
#define TRY_AND_HANDLE(exception, TRY_CODE, EXCEPTION_CODE) __try TRY_CODE   \
	__except (GetExceptionCode() == exception ? EXCEPTION_EXECUTE_HANDLER :  \
			  EXCEPTION_CONTINUE_SEARCH) EXCEPTION_CODE
#else
// NB: Eventually we may try __try1 and __except1 from MinGW...
#define TRY_AND_HANDLE(exception, TRY_CODE, EXCEPTION_CODE) TRY_CODE
#endif
