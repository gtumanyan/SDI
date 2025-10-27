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
#include <windows.h>
#include <unordered_map>
#include <string>
#include <vector>

#include "utils/BaseUtil.h"
#include "VersionEx.h"
#include "draw.h"

#pragma once

/* Convenient to have around */
#define KB                          1024LL

#define LEFT_TO_RIGHT_MARK          "\u200e"
#define MAX_SIZE_SUFFIXES           6			// bytes, KB, MB, GB, TB, PB
#define MAX_PROGRESS                0xFFFF
#define MAX_REFRESH                 25			// How long we should wait to refresh UI elements (in ms)

#define MAX_LOG_SIZE                0x7FFFFFFE
#define NET_SESSION_TIMEOUT         3500		// How long we should wait to connect, send or receive internet data (in ms)
#define WRITE_RETRIES               4
#define UBUFFER_SIZE                4096
#define IGNORE_RETVAL(expr)         do { (void)(expr); } while(0)

#define safe_free(p) do { free((void*)p); p = NULL; } while(0)
static __inline void safe_strcp(char* dst, const size_t dst_max, const char* src, const size_t count) {
	memmove(dst, src, (std::min)(count, dst_max));
	if (dst != NULL) dst[std::min(count, dst_max) - 1] = 0;
}
#define safe_strcpy(dst, dst_max, src) safe_strcp(dst, dst_max, src, safe_strlen(src) + 1)
#define static_strcpy(dst, src) safe_strcpy(dst, sizeof(dst), src)
#define safe_closehandle(h) do { if ((h != INVALID_HANDLE_VALUE) && (h != NULL)) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } } while(0)
#define safe_release_dc(hDlg, hDC) do { if ((hDC != INVALID_HANDLE_VALUE) && (hDC != NULL)) { ReleaseDC(hDlg, hDC); hDC = NULL; } } while(0)
#define safe_delete_object(hObj) do { if (hObj != NULL) { DeleteObject(hObj); hObj = NULL; } } while(0)
#define safe_sprintf(dst, count, ...) do { size_t _count = count; char* _dst = dst; _snprintf_s(_dst, _count, _TRUNCATE, __VA_ARGS__); \
	if (_dst != NULL) _dst[(_count) - 1] = 0; } while(0)
#define static_sprintf(dst, ...) safe_sprintf(dst, sizeof(dst), __VA_ARGS__)
#define safe_strlen(str) ((((char*)(str))==NULL) ? 0 : strlen(str))
#if defined(_MSC_VER)
#define safe_vsnprintf(buf, size, format, arg) _vsnprintf_s(buf, size, _TRUNCATE, format, arg)
#else
#define safe_vsnprintf vsnprintf
#endif

class wFont;
class Combobox;

extern class Popup_t *Popup;


extern void uprintf(const char *format, ...);
extern void uprintfs(const char *str);
#ifdef _DEBUG
#define duprintf uprintf
#else
#define duprintf(...)
#endif

/* Custom Windows messages */
enum MessagesWND {
    WM_BUNDLEREADY     = WM_APP+1,
    WM_UPDATELANG      = WM_APP+2,
    WM_UPDATETHEME     = WM_APP+3,
    WM_SEEDING         = WM_APP+4,
    WM_TORRENT         = WM_APP+5,
    WM_INDEXESSAVED    = WM_APP+6,
		UM_PROGRESS_INIT,
		UM_RESIZE_BUTTONS
};
/* Status Bar sections */
#define SB_SECTION_LEFT         0

/* Timers used throughout the program */
enum timer_type {
	TID_MESSAGE_INFO = 0x1000,
	TID_MESSAGE_STATUS,
	TID_OUTPUT_INFO,
	TID_OUTPUT_STATUS,
	TID_BADBLOCKS_UPDATE,
	TID_APP_TIMER,
	TID_BLOCKING_TIMER,
	TID_REFRESH_TIMER,
	TID_MARQUEE_TIMER
};

/* Action type, for progress bar breakdown */
enum action_type {
		OP_NOOP_WITH_TASKBAR = -3,
		OP_NOOP = -2,
		OP_INIT = -1,
		OP_ANALYZE_MBR = 0,
		OP_BADBLOCKS,
		OP_ZERO_MBR,
		OP_PARTITION,
		OP_FORMAT,
		OP_CREATE_FS,
		OP_FIX_MBR,
		OP_FILE_COPY,
		OP_PATCH,
		OP_FINALIZE,
		OP_EXTRACT_ZIP,
		OP_MAX
};

enum file_io_type {
	FILE_IO_READ = 0,
	FILE_IO_WRITE,
	FILE_IO_APPEND
};

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
    INVALIDATE_INDICES  =1,
    INVALIDATE_SYSINFO  =2,
    INVALIDATE_DEVICES  =4,
    INVALIDATE_MANAGER  =8,
};

static __inline USHORT GetApplicationArch(void)
{
#if defined(_M_AMD64)
		return IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_IX86)
		return IMAGE_FILE_MACHINE_I386;
#elif defined(_M_ARM64)
		return IMAGE_FILE_MACHINE_ARM64;
#elif defined(_M_ARM)
		return IMAGE_FILE_MACHINE_ARM;
#else
		return IMAGE_FILE_MACHINE_UNKNOWN;
#endif
}

static __inline const char* GetArchName(USHORT uArch)
{
		switch (uArch) {
		case IMAGE_FILE_MACHINE_AMD64:
				return "x64";
		case IMAGE_FILE_MACHINE_I386:
				return "x86";
		case IMAGE_FILE_MACHINE_ARM64:
				return "ARM64";
		case IMAGE_FILE_MACHINE_ARM:
				return "ARM32";
		default:
				return "Unknown";
		}
}

/* Windows versions */
enum WindowsVersion {
		WINDOWS_UNDEFINED = 0,
		WINDOWS_XP = 0x51,
		WINDOWS_2003 = 0x52,		// Also XP_64
		WINDOWS_VISTA = 0x60,		// Also Server 2008
		WINDOWS_7 = 0x61,		// Also Server 2008_R2
		WINDOWS_8 = 0x62,		// Also Server 2012
		WINDOWS_8_1 = 0x63,		// Also Server 2012_R2
		WINDOWS_10_PREVIEW1 = 0x64,
		WINDOWS_10 = 0xA0,		// Also Server 2016, also Server 2019
		WINDOWS_11 = 0xB0,		// Also Server 2022
		WINDOWS_MAX = 0xFFFF,
};

typedef struct {
		DWORD Major;
		DWORD Minor;
		DWORD Micro;
		DWORD Nano;
} version_t;

typedef struct {
		DWORD Version;
		DWORD Major;
		DWORD Minor;
		DWORD BuildNumber;
		DWORD Ubr;
		DWORD Edition;
		USHORT Arch;
		char VersionStr[128];
} windows_version_t;

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


// torrents
enum TORRENT_SELECTION_MODE
{
    TSM_NONE           = 0,
    TSM_AUTO           = 1
};

/*
 * Globals
 */
extern int volatile installmode;
extern HINSTANCE hMainInstance;
extern HWND hMainDialog, hLogDialog, hStatus;
extern uint16_t SDI_version[3];
extern CRITICAL_SECTION sync;
extern bool CRITICAL_SECTION_ACTIVE;
extern bool emptydrp;
extern HWND hLog, hProgress;
extern DWORD ErrorStatus;
extern BOOL right_to_left_mode;
extern uint8_t num_cores;
extern int invaidate_set;
extern int dialog_showing;
extern size_t ubuffer_pos;
extern windows_version_t WindowsVersion;
extern char ubuffer[UBUFFER_SIZE];


/*
 * Shared prototypes
 */
extern void GetWindowsVersion(windows_version_t* WindowsVersion);
extern const char* WindowsErrorString(void);
extern void PrintStatusInfo(BOOL info, BOOL debug, unsigned int duration, int msg_id, ...);
#define PrintStatus(...) PrintStatusInfo(FALSE, FALSE, __VA_ARGS__)
#define PrintInfo(...) PrintStatusInfo(TRUE, FALSE, __VA_ARGS__)
extern void _UpdateProgressWithInfo(int op, int msg, uint64_t processed, uint64_t total, BOOL force);
#define UpdateProgressWithInfo(op, msg, processed, total) _UpdateProgressWithInfo(op, msg, processed, total, FALSE)
#define UpdateProgressWithInfoInit(hProgressDialog, bNoAltMode) UpdateProgressWithInfo(OP_INIT, (int)bNoAltMode, (uint64_t)(uintptr_t)hProgressDialog, 0);
extern char* SizeToHumanReadable(uint64_t size, BOOL copy_to_log, BOOL fake_units);
extern INT_PTR MyDialogBox(HINSTANCE hInstance, int Dialog_ID, HWND hWndParent, DLGPROC lpDialogFunc);
extern void ResizeMoveCtrl(HWND hDlg, HWND hCtrl, int dx, int dy, int dw, int dh, float scale);
extern void ResizeButtonHeight(HWND hDlg, int id);
extern void SetHyperLinkFont(HWND hWnd, HDC hDC, HFONT* hFont, BOOL underlined);
extern BOOL SetTaskbarProgressValue(ULONGLONG ullCompleted, ULONGLONG ullTotal);
extern INT_PTR CreateAboutBox(void);
extern char* FileDialog(BOOL save, char* path, const ext_t* ext, UINT* selected_ext);
extern BOOL FileIO(enum file_io_type io_type, char* path, char** buffer, DWORD* size);
extern uint8_t* GetResource(HMODULE module, char* name, char* type, const char* desc, DWORD* len, BOOL duplicate);
extern uint64_t DownloadToFileOrBufferEx(const char* url, const char* file, const char* user_agent,
		BYTE** buffer, HWND hProgressDialog, BOOL bTaskBarProgress);
extern BOOL IsFontAvailable(const char* font_name);
extern void setMirroring(HWND hwnd);

// Vector templates
template <class T>
char *vector_save(std::vector<T> *v,char *p)
{
		size_t used=v->size()*sizeof(T);
		size_t val=v->size();

		int *pi=reinterpret_cast<int *>(p);
		*pi++=static_cast<int>(used);p+=sizeof(int);
		*pi++=static_cast<int>(val);p+=sizeof(int);
		memcpy(p,&v->front(),used);p+=used;
		return p;
}

template <class T>
char *vector_load(std::vector<T> *v,char *p)
{
		size_t sz=0,num=0;

		int *pi=reinterpret_cast<int *>(p);
		sz=*pi++;p+=sizeof(int);
		num=*pi++;p+=sizeof(int);
		if(!num)num=sz;
		v->resize(num);
		memcpy(v->data(),p,sz);p+=sz;
		return p;
}

template <class T>
class loadable_vector:public std::vector<T>
{
public:
		char *savedata(char *p){return vector_save(this,p);}
		char *loaddata(char *p){return vector_load(this,p);}
};

// Strings
void strsub(wchar_t *str,const wchar_t *pattern,const wchar_t *rep);
void strtoupper(const char *s,size_t len);
void strtolower(const char *s,size_t len);
size_t unicode2ansi(const unsigned char *s,char *out,size_t size);
int _wtoi_my(const wchar_t *str);

// Popup
class Popup_t
{
    Popup_t(const Popup_t&) = delete;
    Popup_t& operator=(const Popup_t&) = delete;

private:
    Canvas* canvasPopup = nullptr;
    int floating_type = 0;
    int floating_x = 1, floating_y = 1;
    bool wait = false;
    int horiz_sh = 0;

public:
    HWND hPopup = nullptr;
    Popup_t();
    ~Popup_t();
    void init();
    void drawpopup(size_t itembar, int str_id, int type, int x, int y, HWND hwnd);
    void popup_resize(int x, int y);
    void onHover();
    void onLeave();
    void setMirroring();
    void setTransparency();
    void getPos(long int* x, long int* y);

    int  getShift() { return horiz_sh; }
    void AddShift(int v);

    wFont* hFontP;
    wFont* hFontBold;
    size_t floating_itembar = 0;
    int floating_str_id = 0;

    LRESULT PopupProcedure2(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};
LRESULT CALLBACK PopupProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// Window
class MainWindow_t
{
    MainWindow_t(const MainWindow_t&)=delete;
    void operator=(const MainWindow_t&)=delete;

private:
    Canvas *canvasMain;
    Canvas *canvasField;
    static const wchar_t classMain[];
    static const wchar_t classField[];
    static const wchar_t classPopup[];

    int mousex=-1,mousey=-1,mousedown=MOUSE_NONE,mouseclick=0;
    int scrollvisible=0;
    size_t field_lasti;
    int field_lastz;

    wFont *hFont;

    friend class Popup_t;

public:
    int main1x_c,main1y_c;
    int mainx_c,mainy_c;

    HWND hMain,hwndFrame;
    Combobox *hLang;
    Combobox *hTheme;
    int offset_target;
    int ctrl_down;
    int space_down;
    int shift_down;

    int kbpanel,kbfield,kbinstall;

private:
    static LRESULT CALLBACK WndProcMainCallback(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK WndProcFieldCallback(HWND,UINT,WPARAM,LPARAM);
    LRESULT WndProcCommon(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
    LRESULT MainCallback(HWND,UINT,WPARAM,LPARAM);
    LRESULT WndProcField(HWND,UINT,WPARAM,LPARAM);
    void AddMenuItem(HMENU parent,UINT mask,UINT id,UINT type,UINT state,HMENU hSubMenu,wchar_t* typedata);
    void ModifyMenuItem(HMENU parent, UINT mask,UINT id,UINT state,wchar_t* typedata);

public:
    void MainLoop(int nCmd);
    void LoadMenuItems();
    void lang_refresh();
    void theme_refresh(int shift);
    void redrawfield();
    void redrawmainwnd();

    void tabadvance(int v);
    void arrowsAdvance(int v);

    // Commands
    void snapshot();
    void extractto();
    void selectDrpDir();

    // Scrollbar
    void setscrollrange(int y);
    int  getscrollpos();
    void setscrollpos(int pos);

    void ShowProgressInTaskbar(bool show,long long complited=0,long long total=0);
    void DownloadedTorrent(int TorrentResults);
    void ResetUpdater(int activetorrent=1);
    void UpdateTorrentItems(int activetorrent);

    MainWindow_t();
    ~MainWindow_t();
};
extern MainWindow_t MainWindow;

//}

// Subroutes
void drp_callback(const wchar_t *szFile,int action,int lParam);
void invalidate(int v);

// Misc
void escapeAmpUrl(wchar_t *buf,const wchar_t *source);
void escapeAmp(wchar_t *buf,const wchar_t *source);

// GUI Helpers
void setMirroringEdit(HWND hwnd);

// GUI
BOOL CALLBACK WelcomeCallback(HWND hwnd,UINT Message,WPARAM wParam,LPARAM lParam);

class WString_dyn
{
protected:
		wchar_t *buf_dyn=nullptr;
		wchar_t *buf_cur;
		size_t len;
		bool debug;

public:
		WString_dyn(size_t sz,wchar_t *buf,bool debug_=true):buf_cur(buf),len(sz),debug(debug_){}
		virtual ~WString_dyn(){delete[] buf_dyn;}
		void Resize(size_t size);

		void sprintf(const wchar_t *format,...);
        void vsprintf(const wchar_t *format,va_list args);
		void append(const wchar_t *str);
		void strcpy(const wchar_t *Str);

		wchar_t *GetV()const{return buf_cur;}
		const wchar_t *Get()const{return buf_cur;}
		size_t Length()const{return len;}
};

class WString:public WString_dyn
{
		wchar_t buf[MAX_PATH]{};
public:
		WString(bool debug_=false):WString_dyn(dimof(buf) - 1,buf,debug_){*buf=0;}
};

class WStringShort:public WString_dyn
{
		const static int size=128;
		wchar_t buf[size];
public:
		WStringShort(bool debug_=false):WString_dyn(size,buf,debug_){*buf=0;}
};

// Version
class Version
{
		int d,m,y;
		int v1,v2,v3,v4;

public:
		int  setDate(int d_,int m_,int y_);
		void setVersion(int v1_,int v2_,int v3_,int v4_);
		void setInvalid(){y=v1=-1;}
		int  GetV1()const{return v1;}
		void str_date(WStringShort &buf,bool invariant=false)const;
		void str_version(WStringShort &buf)const;

		Version():d(0),m(0),y(0),v1(-2),v2(0),v3(0),v4(0){}
		Version(int d1,int m1,int y1):d(d1),m(m1),y(y1),v1(-2),v2(0),v3(0),v4(0){}

		friend class datum;
		friend int cmpdate(const Version *t1,const Version *t2);
		friend int cmpversion(const Version *t1,const Version *t2);
};
int cmpdate(const Version *t1,const Version *t2);
int cmpversion(const Version *t1,const Version *t2);

// Txt
class Txt
{
		std::unordered_map<std::string,size_t> dub;
		loadable_vector<char> text;

public:
		size_t getSize()const{return text.size();}
		const char *get(uint offset)const{return &text[offset];}
		char *getV(uint offset)const{ return const_cast<char *>(&text[offset]); }
		const wchar_t *getw(uint offset)const{ return reinterpret_cast<const wchar_t *>(&text[offset]); }
		wchar_t *getwV(uint offset)const{ return const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(&text[offset])); }
		const wchar_t *getw2(uint offset)const{ return reinterpret_cast<const wchar_t *>(&text[offset-(text[0]?2:0)]); }

		size_t strcpy(const char *mem);
		size_t strcpyw(const wchar_t *mem);
		size_t t_memcpy(const char *mem,size_t sz);
		size_t t_memcpyz(const char *mem,size_t sz);
		size_t memcpyz_dup(const char *mem,size_t sz);
		size_t alloc(size_t sz);

		char *savedata(char *p){return text.savedata(p);}
		char *loaddata(char *p){return text.loaddata(p);};

		Txt();
		void reset(size_t sz);
		void shrink();
};

// Hashtable
class Hashitem
{
		int key;
		int value;
		int next;
		int valuelen;

public:
		Hashitem():key(0),value(0),next(0),valuelen(0){}

		friend class Hashtable;
};

class Hashtable
{
		int findnext_v;
		int findstr;
		int size;
		loadable_vector<Hashitem> items;

public:
	uint getSize()const{ return static_cast<uint>(items.size()); }

		static unsigned gethashcode(const char *s,size_t sz);
		void reset(size_t size);
		char *savedata(char *p);
		char *loaddata(char *p);
		void additem(int key,int value);
		int  find(int vl,int *isfound);
		int  findnext(int *isfound);
};

// 7-zip
size_t  encode(char *dest,size_t dest_sz,const char *src,size_t src_sz);
size_t  decode(char *dest,size_t dest_sz,const char *src,size_t src_sz);
void registerall();

namespace NArchive{
namespace N7z{
		extern void register7z();
}}
extern int  Extract7z(const wchar_t *str);
extern void registerBCJ();
extern void registerBCJ2();
extern void registerBranch();
//extern void registerCopy();
extern void registerLZMA();
extern void registerLZMA2();

#include <sstream>

template<typename Out>
void split(const std::wstring &s, wchar_t delim, Out result)
{
		std::wstringstream ss;
		ss.str(s);
		std::wstring item;
		while (std::getline(ss, item, delim)) {
				*(result++) = item;
		}
}
std::vector<std::wstring> split(const std::wstring& s, wchar_t delim);

#define         MAX_LIBRARY_HANDLES 64
extern HMODULE  OpenedLibrariesHandle[MAX_LIBRARY_HANDLES];
extern uint16_t OpenedLibrariesHandleSize;
#define         OPENED_LIBRARIES_VARS HMODULE OpenedLibrariesHandle[MAX_LIBRARY_HANDLES]; uint16_t OpenedLibrariesHandleSize = 0
#define         CLOSE_OPENED_LIBRARIES while(OpenedLibrariesHandleSize > 0) FreeLibrary(OpenedLibrariesHandle[--OpenedLibrariesHandleSize])
static __inline HMODULE GetLibraryHandle(const char* szLibraryName) {
		HMODULE h = NULL;
		wchar_t* wszLibraryName = NULL;
		int size;
		if (szLibraryName == NULL || szLibraryName[0] == 0)
				goto out;
		size = MultiByteToWideChar(CP_UTF8, 0, szLibraryName, -1, NULL, 0);
		if ((size <= 1) || ((wszLibraryName = (wchar_t*)calloc(size, sizeof(wchar_t))) == NULL) ||
				(MultiByteToWideChar(CP_UTF8, 0, szLibraryName, -1, wszLibraryName, size) != size))
				goto out;
		// If the library is already opened, just return a handle (that doesn't need to be freed)
		if ((h = GetModuleHandleW(wszLibraryName)) != NULL)
				goto out;
		// Sanity check
		if (OpenedLibrariesHandleSize >= MAX_LIBRARY_HANDLES) {
				uprintf("Error: MAX_LIBRARY_HANDLES is too small\n");
				goto out;
		}
		h = LoadLibraryExW(wszLibraryName, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
		if (h != NULL)
				OpenedLibrariesHandle[OpenedLibrariesHandleSize++] = h;
		else
				uprintf("Unable to load '%S.dll': %s", wszLibraryName, WindowsErrorString());
out:
		free(wszLibraryName);
		return h;
}
#define PF_TYPE(api, ret, proc, args)		typedef ret (api *proc##_t)args
#define PF_DECL(proc)						static proc##_t pf##proc = NULL
#define PF_TYPE_DECL(api, ret, proc, args)	PF_TYPE(api, ret, proc, args); PF_DECL(proc)
#define PF_INIT(proc, name)					if (pf##proc == NULL) pf##proc = \
	(proc##_t) GetProcAddress(GetLibraryHandle(#name), #proc)
#define PF_INIT_OR_OUT(proc, name)			do {PF_INIT(proc, name);         \
	if (pf##proc == NULL) {uprintf("Unable to locate %s() in '%s.dll': %s",  \
	#proc, #name, WindowsErrorString()); goto out;} } while(0)
#define PF_INIT_OR_SET_STATUS(proc, name)	do {PF_INIT(proc, name);         \
	if ((pf##proc == NULL) && (NT_SUCCESS(status))) status = STATUS_PROCEDURE_NOT_FOUND; } while(0)
#if defined(_MSC_VER)
#define TRY_AND_HANDLE(exception, TRY_CODE, EXCEPTION_CODE) __try TRY_CODE   \
	__except (GetExceptionCode() == exception ? EXCEPTION_EXECUTE_HANDLER :  \
			  EXCEPTION_CONTINUE_SEARCH) EXCEPTION_CODE
#else
// NB: Eventually we may try __try1 and __except1 from MinGW...
#define TRY_AND_HANDLE(exception, TRY_CODE, EXCEPTION_CODE) TRY_CODE
#endif

/* Custom application errors */
#define FAC(f)                         ((f)<<16)

#define SDI_ERROR(err)               (ERROR_SEVERITY_ERROR | FAC(FACILITY_STORAGE) | (err))
