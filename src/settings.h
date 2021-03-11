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

#ifndef SETTINGS_H
#define SETTINGS_H

#define SAVE_INSTALLED_ID_DEF   L"-save-installed-id"
#define HWIDINSTALLED_DEF       L"-HWIDInstalled:"
#define GFG_DEF                 L"-cfg:"

extern int ret_global;

// Mode
enum STATEMODE
{
    STATEMODE_REAL      =0,
    STATEMODE_EMUL      =1,
    STATEMODE_EXIT      =2,
};

// Left panel IDs
enum install_mode
{
    MODE_NONE          = 0,
    MODE_INSTALLING    = 1,
    MODE_STOPPING      = 2,
    MODE_SCANNING      = 3,
};

// IDs
enum GUI_ID
{
    ID_SHOW_MISSING     =1,
    ID_SHOW_NEWER       =2,
    ID_SHOW_CURRENT     =3,
    ID_SHOW_OLD         =4,
    ID_SHOW_BETTER      =5,
    ID_SHOW_WORSE_RANK  =6,

    ID_SHOW_NF_MISSING  =7,
    ID_SHOW_NF_UNKNOWN  =8,
    ID_SHOW_NF_STANDARD= 9,

    ID_SHOW_ONE        =10,
    ID_SHOW_DUP        =11,
    ID_SHOW_INVALID    =12,

    ID_INSTALL         =13,
    ID_SELECT_ALL      =14,
    ID_SELECT_NONE     =15,
    ID_EXPERT_MODE     =16,

    ID_LANG            =17,
    ID_THEME           =18,

    ID_OPENLOGS        =19,
    ID_SNAPSHOT        =20,
    ID_EXTRACT         =21,
    ID_DRVDIR          =22,

    ID_SCHEDULE        =23,
    ID_SHOWALT         =24,
    ID_OPENINF         =25,
    ID_LOCATEINF       =26,

    ID_EMU_32          =27,
    ID_EMU_64          =28,
    ID_DEVICEMNG       =29,
    ID_DIS_INSTALL     =30,
    ID_DIS_RESTPNT     =31,

    // not used - menu items now use ID_OS_ITEMS + index
    ID_WIN_2000        =32,
    ID_WIN_XP          =33,
    ID_WIN_VISTA       =34,
    ID_WIN_7           =35,
    ID_WIN_8           =36,
    ID_WIN_81          =37,
    ID_WIN_10          =38,

    ID_RESTPNT         =39,
    ID_REBOOT          =40,

    ID_URL0            =41,
    ID_URL1            =42,
    ID_URL2            =43,
    ID_URL3            =44,
    ID_URL4            =45,

    ID_SYSPROPS        =50,
    ID_COMPMNG         =51,
    ID_SYSPROT         =52,
    ID_DEVICEPRNT      =53,
    ID_SYSCONTROL      =54,
    ID_SYSREST         =55,
    ID_SYSPROPS_ADV    =57,

    ID_HWID_CLIP      =100,
    ID_HWID_WEB       =200,
    ID_OS_ITEMS      =1000,
};

// filter_show
enum fileter_show
{
    FILTER_SHOW_MISSING    = (1<<ID_SHOW_MISSING),        // 1<<1=2
    FILTER_SHOW_NEWER      = (1<<ID_SHOW_NEWER),          // 1<<2=4
    FILTER_SHOW_CURRENT    = (1<<ID_SHOW_CURRENT),        // 1<<3=8
    FILTER_SHOW_OLD        = (1<<ID_SHOW_OLD),            // 1<<4=16
    FILTER_SHOW_BETTER     = (1<<ID_SHOW_BETTER),         // 1<<5=32
    FILTER_SHOW_WORSE_RANK = (1<<ID_SHOW_WORSE_RANK),     // 1<<6=64

    FILTER_SHOW_NF_MISSING = (1<<ID_SHOW_NF_MISSING),     // 1<<7=128
    FILTER_SHOW_NF_UNKNOWN = (1<<ID_SHOW_NF_UNKNOWN),     // 1<<8=256
    FILTER_SHOW_NF_STANDARD= (1<<ID_SHOW_NF_STANDARD),    // 1<<9=512

    FILTER_SHOW_ONE        = (1<<ID_SHOW_ONE),            // 1<<10=1024
    FILTER_SHOW_DUP        = (1<<ID_SHOW_DUP),            // 1<<11=2048
    FILTER_SHOW_INVALID    = (1<<ID_SHOW_INVALID),        // 1<<12=4096
};

// Global flags
enum FLAG
{
    COLLECTION_FORCE_REINDEXING = 0x00000001,
    COLLECTION_USE_LZMA         = 0x00000002,
    COLLECTION_PRINT_INDEX      = 0x00000004,
    FLAG_NOGUI                  = 0x00000010,
    FLAG_CHECKUPDATES           = 0x00000020,
    FLAG_DISABLEINSTALL         = 0x00000040,
    FLAG_AUTOINSTALL            = 0x00000080,
    FLAG_FAILSAFE               = 0x00000100,
    FLAG_AUTOCLOSE              = 0x00000200,
    FLAG_NORESTOREPOINT         = 0x00000400,
    FLAG_NOLOGFILE              = 0x00000800,
    FLAG_NOSNAPSHOT             = 0x00001000,
    FLAG_NOSTAMP                = 0x00002000,
    FLAG_NOVIRUSALERTS          = 0x00004000,
    FLAG_PRESERVECFG            = 0x00008000,
    FLAG_EXTRACTONLY            = 0x00010000,
    FLAG_KEEPUNPACKINDEX        = 0x00020000,
    FLAG_KEEPTEMPFILES          = 0x00040000,
    FLAG_SHOWDRPNAMES1          = 0x00080000,
    FLAG_DPINSTMODE             = 0x00100000,
    FLAG_SHOWCONSOLE            = 0x00200000,
    FLAG_DELEXTRAINFS           = 0x00400000,
    FLAG_SHOWDRPNAMES2          = 0x00800000,
    FLAG_ONLYUPDATES            = 0x01000000,
    FLAG_AUTOUPDATE             = 0x02000000,
    FLAG_FILTERSP               = 0x04000000,
    FLAG_OLDSTYLE               = 0x08000000,
    FLAG_HIDEPATREON            = 0x10000000,
    FLAG_NOSTOP                 = 0x20000000,
    FLAG_KEEPSEEDING            = 0x40000000,
    FLAG_SCRIPTMODE             = 0x80000000,
    FLAG_UPDATESOK              = 0x100000000
};

class Settings_t
{
public:
    wchar_t curlang   [BUFLEN];
    wchar_t curtheme  [BUFLEN];
    wchar_t logO_dir  [BUFLEN];

    wchar_t drp_dir   [BUFLEN];
    wchar_t output_dir[BUFLEN];
    wchar_t drpext_dir[BUFLEN];
    wchar_t index_dir [BUFLEN];
    wchar_t data_dir  [BUFLEN];
    wchar_t log_dir   [BUFLEN];

    wchar_t state_file[BUFLEN];
    wchar_t finish    [BUFLEN];
    wchar_t finish_upd[BUFLEN];
    wchar_t finish_rb [BUFLEN];
    wchar_t device_list_filename[BUFLEN];

    int flags;
    int statemode;
    int expertmode;
    int hintdelay;
    int license;
    int scale;
    int wndwx,wndwy,wndsc;
    int filters;
    int virtual_os_version;  // index into the array in WinVersions
    int virtual_arch_type;

    bool autosized=false;
    int  savedscale;

public:
    Settings_t();
    void parse(const wchar_t *str,size_t ind);
    bool load(const wchar_t *filename);
    bool load_cfg_switch(const wchar_t *cmdParams);
    void save();
    void loginfo();

private:
    bool argstr(const wchar_t *s,const wchar_t *cmp,wchar_t *d);
    bool argint(const wchar_t *s,const wchar_t *cmp,int *d);
    bool argopt(const wchar_t *s,const wchar_t *cmp,int *d);
    bool argflg(const wchar_t *s,const wchar_t *cmp,int f);

    bool loadCFGFile(const wchar_t *FileName,wchar_t *DestStr);
    wchar_t *ltrim(wchar_t *s);
};
extern Settings_t Settings;

#endif
