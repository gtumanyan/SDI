/*
This file is part of Snappy Driver Installer.

Snappy Driver Installer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Snappy Driver Installer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Snappy Driver Installer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "com_header.h"
#include "common.h"
#include "logging.h"
#include "settings.h"
#include "system.h"
#include "theme.h"
#include "draw.h"

#include <windows.h>

// Depend on Win32API
#include "main.h"

#include <memory>
#include <string.h>

#include "theme_imp.h"

//{ Global vars
Vaul *vLang;
Vaul *vTheme;
//}

Vaul *CreateVaultLang(entry_t *entry,size_t num,int res)
{
    return new VaultLang(entry,num,res,WM_UPDATELANG,L"langs");
}
Vaul *CreateVaultTheme(entry_t *entry,size_t num,int res)
{
    return new VaultTheme(entry,num,res,WM_UPDATETHEME,L"themes");
}

//{ Vault
int VaultImp::findvar(wchar_t *str)
{
    int i;
    wchar_t *p;
    wchar_t c;

    while(*str&&(*str==L' '||*str==L'\t'))str++;
    p=str;
    while(*p&&(*p!=L' '&&*p!=L'\t'))p++;
    c=*p;
    *p=0;

    auto got=lookuptbl.find(std::wstring(str));
    i=(got!=lookuptbl.end())?got->second:0;
    i--;
    *p=c;
    return i;
}

wchar_t *VaultImp::findstr(wchar_t *str)const
{
    wchar_t *b,*e;

    b=wcschr(str,L'\"');
    if(!b)return nullptr;
    b++;
    e=wcschr(b,L'\"');
    if(!e)return nullptr;
    *e=0;
    return b;
}

int VaultImp::readvalue(const wchar_t *str)
{
    const wchar_t *p;

    p=wcsstr(str,L"0x");
    return p?wcstol(str,nullptr,16):_wtoi_my(str);
}

void VaultImp::parse()
{
    wchar_t *lhs,*rhs,*le;
    le=lhs=datav_ptr.get();

    while(le)
    {
        // Split lines
        le=wcschr(lhs,L'\n');
        if(le)*le=0;

        // Comments
        if(wcsstr(lhs,L"//"))*wcsstr(lhs,L"//")=0;

        // Split LHS and RHS
        rhs=wcschr(lhs,L'=');
        if(!rhs)
        {
            lhs=le+1;// next line
            continue;
        }
        *rhs=0;rhs++;

        // Parse LHS
        int r=findvar(lhs);
        if(r<0)
        {
            Log.print_err("ERROR: unknown var '%S'\n",lhs);
        }else
        {
            wchar_t *r1;
            int r2;
            size_t ri=static_cast<size_t>(r);
            r1=findstr(rhs);
            r2=findvar(rhs);

            if(r1)               // String
            {
                while(wcsstr(r1,L"\\n"))
                {
                    wchar_t *yy=wcsstr(r1,L"\\n");
                    wcscpy(yy,yy+1);
                    *yy=L'\n';
                }
                while(wcsstr(r1,L"\\\\"))
                {
                    wchar_t *yy=wcsstr(r1,L"\\\\");
                    wcscpy(yy,yy+1);
                }
                while(wcsstr(r1,L"/0"))
                {
                    wchar_t *yy=wcsstr(r1,L"/0");
                    wcscpy(yy,yy+1);
                    *yy=1;
                }
                {
                    size_t l=wcslen(r1);
                    for(size_t i=0;i<l;i++)if(r1[i]==1)r1[i]=0;
                }
                entry[ri].valstr=r1;
                entry[ri].init=1;
            }
            else if(r2>=0)      // Var
            {
                entry[ri].val=entry[r2].val;
                entry[ri].init=10+r2;
            }
            else                // Number
            {
                int val=readvalue(rhs);
                //if(!entry[r].init&&entry[r].val==val)Log.print_err("WARNNING: double definition for '%S'\n",lhs);
                entry[ri].val=val;
                entry[ri].init=2;
            }
        }
        lhs=le+1; // next line
    }
    odata_ptr=std::move(data_ptr);
    //data_ptr.reset(datav);
    data_ptr=std::move(datav_ptr);
}

static void myswab(const char *s,char *d,size_t sz)
{
    while(sz--)
    {
        d[0]=s[1];
        d[1]=s[0];
        d+=2;s+=2;
    }
}

bool VaultImp::loadFromEncodedFile(const wchar_t *filename)
{
    FILE *f=_wfopen(filename,L"rb");
    if(!f)
    {
        Log.print_err("ERROR in loadfile(): failed _wfopen(%S)\n",filename);
        return false;
    }

    _fseeki64(f,0,SEEK_END);
    size_t sz=static_cast<size_t>(_ftelli64(f));
    _fseeki64(f,0,SEEK_SET);
    if(sz<10)
    {
        Log.print_err("ERROR in loadfile(): '%S' has only %d bytes\n",filename,sz);
        fclose(f);
        return false;
    }
    datav_ptr.reset(new wchar_t[sz+1]);
    wchar_t *datav=datav_ptr.get();
    Log.print_con("Read '%S':%d\n",filename,sz);

    fread(datav,2,1,f);
    if(!memcmp(datav,"\xEF\xBB",2))// UTF-8
    {
        size_t szo;
        fread(datav,1,1,f);
        sz-=3;
        size_t q=fread(datav,1,sz,f);
        szo=MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<LPCSTR>(datav),static_cast<int>(q),nullptr,0);
        wchar_t *dataloc1=new wchar_t[szo+1];
        sz=MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<LPCSTR>(datav),static_cast<int>(q),dataloc1,static_cast<int>(szo));
        fclose(f);
        dataloc1[sz]=0;
        datav_ptr.reset(dataloc1);
        return true;
    }else
    if(!memcmp(datav,"\xFF\xFE",2))// UTF-16 LE
    {
        fread(datav,sz,1,f);
        sz>>=1;(sz)--;
        fclose(f);
        datav[sz]=0;
        return true;
    }else
    if(!memcmp(datav,"\xFE\xFF",2))// UTF-16 BE
    {
        fread(datav,sz,1,f);
        myswab((char *)datav,(char *)datav,sz);
        sz>>=1;(sz)--;
        fclose(f);
        datav[sz]=0;
        return true;
    }else                         // ANSI
    {
        fclose(f);
        f=_wfopen(filename,L"rt");
        if(!f)
        {
            Log.print_err("ERROR in loadfile(): failed _wfopen(%S)\n",filename);
            return false;
        }
        wchar_t *p=datav;(sz)--;
        while(!feof(f))
        {
            fgetws(p,static_cast<int>(sz),f);
            p+=wcslen(p);
        }
        fclose(f);
        datav[sz]=0;
        return true;
    }
}

void VaultImp::loadFromFile(const wchar_t *filename)
{
    if(!filename[0])return;
    if(!loadFromEncodedFile(filename))
    {
        Log.print_err("ERROR in vault_loadfromfile(): failed to load '%S'\n",filename);
        return;
    }
    parse();

    for(size_t i=0;i<num;i++)
        if(entry[i].init>=10)entry[i].val=entry[entry[i].init-10].val;
}

void VaultImp::loadFromRes(int id)
{
    char *data1;
    size_t sz;

    get_resource(id,(void **)&data1,&sz);
    datav_ptr.reset(new wchar_t[sz+1]);
    wchar_t *datav=datav_ptr.get();
    for(size_t i=0;i<sz;i++)
    {
        if(data1[i]==L'\r')datav[i]=L' ';else
        datav[i]=data1[i];
    }
    datav[sz]=0;
    parse();
    for(size_t i=0;i<num;i++)
        if(entry[i].init<1)Log.print_err("ERROR in vault_loadfromres: not initialized '%S'\n",entry[i].name);
}

VaultImp::VaultImp(entry_t *entryv,size_t numv,int resv,int elem_id_,const wchar_t *folder_):
    entry(entryv),
    num(numv),
    res(resv),
    elem_id(elem_id_),
    folder(folder_)
{
    for(size_t i=0;i<num;i++)
        lookuptbl.insert({std::wstring(entry[i].name),static_cast<int>(i)+1});
}

void VaultImp::load(int i)
{
    Log.print_con("vault %d,'%S'\n",i,namelist[i]);
    loadFromRes(res);
    if(i<0)return;
    loadFromFile(namelist[i]);
}
//}

//{ Lang/theme
VaultLang::VaultLang(entry_t *entryv,size_t numv,int resv,int elem_id_,const wchar_t *folder_):
    VaultImp{entryv,numv,resv,elem_id_,folder_}
{
    load(-1);
}

int VaultLang::AutoPick()
{
    int f=-1;
    int j=MainWindow.hLang->GetNumItems();
    for(int i=0;i<j;i++)
        if(StrStrIW(lang_ids[i],Settings.curlang))
           {f=i;break;}

    return f;
}

int VaultTheme::AutoPick()
{
    int f=0;
    int j=MainWindow.hTheme->GetNumItems();
    for(int i=0;i<j;i++)
        if(StrStrIW(namelist[i],D_STR(THEME_NAME))&&
            StrStrIW(namelist[i],L"big")==nullptr){f=i;break;}

    return f;
}

void VaultLang::SwitchData(int i)
{
    if(Settings.flags&FLAG_NOGUI)return;
    load(i);
}

void VaultTheme::SwitchData(int i)
{
    if(Settings.flags&FLAG_NOGUI)return;
    load(i);
    Images->LoadAll();
    Icons->LoadAll();
}
void VaultLang::EnumFiles(Combobox *lst,const wchar_t *path,int locale)
{
    WStringShort buf;
    HANDLE hFind;
    WIN32_FIND_DATA FindFileData;
    int i=0;

    if(Settings.flags&FLAG_NOGUI)return;

    int lang_auto=-1;
    WStringShort lang_auto_str;
    lang_auto_str.sprintf(L"Auto (English)");

    buf.sprintf(L"%s\\%s\\*.txt",Settings.data_dir,path);
    hFind=FindFirstFile(buf.Get(),&FindFileData);
    if(hFind!=INVALID_HANDLE_VALUE)
    do
    if(!(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
    {
        buf.sprintf(L"%s\\%s\\%s",Settings.data_dir,path,FindFileData.cFileName);
        loadFromFile(buf.Get());
        if((((language[STR_LANG_CODE].val&0xFF00)==0)&&language[STR_LANG_CODE].val==(locale&0xFF))||
           (language[STR_LANG_CODE].val==locale))
        {
            lang_auto_str.sprintf(L"Auto (%s)",STR(STR_LANG_NAME));
            lang_auto=i;
        }
        lst->AddItem(STR(System.IsLangInstalled(language[STR_LANG_GROUP].val)?STR_LANG_NAME:STR_LANG_ID));
        wcscpy(namelist[i],buf.Get());
        wcscpy(lang_ids[i],STR(STR_LANG_ID));
        i++;
    }
    while(FindNextFile(hFind,&FindFileData)!=0);
    FindClose(hFind);

    if(!i)
    {
        lst->AddItem(L"English");
        namelist[i][0]=0;
    }else
    {
        lst->AddItem(lang_auto_str.Get());
        wcscpy(namelist[i],(lang_auto>=0)?namelist[lang_auto]:L"");
    }
}

void VaultTheme::EnumFiles(Combobox *lst,const wchar_t *path,int arg)
{
	UNREFERENCED_PARAMETER(arg);

    WStringShort buf;
    HANDLE hFind;
    WIN32_FIND_DATA FindFileData;
    int i=0;

    if(Settings.flags&FLAG_NOGUI)return;
    buf.sprintf(L"%s\\%s\\*.txt",Settings.data_dir,path);
    hFind=FindFirstFile(buf.Get(),&FindFileData);
    if(hFind!=INVALID_HANDLE_VALUE)
    do
    if(!(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
    {
        buf.sprintf(L"%s\\%s\\%s",Settings.data_dir,path,FindFileData.cFileName);
        loadFromFile(buf.Get());
        lst->AddItem(D_STR(THEME_NAME));
        wcscpy(namelist[i],buf.Get());
        wcscpy(theme_ids[i],D_STR(THEME_NAME));
        i++;
    }
    while(FindNextFile(hFind,&FindFileData)!=0);
    FindClose(hFind);

    if(!i)
    {
        lst->AddItem(L"(default)");
        namelist[i][0]=0;
    }
    load(-1);
}

void VaultLang::StartMonitor()
{
    WStringShort buf;
    buf.sprintf(L"%s\\%s",Settings.data_dir,folder);
    mon=CreateFilemon(buf.Get(),1,updateCallback);
}

void VaultTheme::StartMonitor()
{
    WStringShort buf;
    buf.sprintf(L"%s\\%s",Settings.data_dir,folder);
    mon=CreateFilemon(buf.Get(),1,updateCallback);
}

void VaultLang::updateCallback(const wchar_t *szFile,int action,int lParam)
{
    UNREFERENCED_PARAMETER(szFile);
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(lParam);

    PostMessage(MainWindow.hMain,WM_UPDATELANG,0,0);
}

void VaultTheme::updateCallback(const wchar_t *szFile,int action,int lParam)
{
    UNREFERENCED_PARAMETER(szFile);
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(lParam);

    PostMessage(MainWindow.hMain,WM_UPDATETHEME,0,0);
}

std::wstring VaultLang::GetFileName(std::wstring id)
{
    std::wstring ret;
    int j=MainWindow.hLang->GetNumItems();
    for(int i=0;i<j;i++)
        if(StrStrIW(lang_ids[i],id.c_str()))
        {
            ret=namelist[i];
            break;
        }
    return ret;
}

std::wstring VaultLang::GetFileName(int id)
{
    std::wstring ret;
    if(id<MainWindow.hLang->GetNumItems())
        ret=namelist[id];
    return ret;
}

std::wstring VaultTheme::GetFileName(std::wstring id)
{
    std::wstring ret;
    int j=MainWindow.hTheme->GetNumItems();
    for(int i=0;i<j;i++)
        if(StrStrIW(theme_ids[i],id.c_str()))
        {
            ret=namelist[i];
            break;
        }
    return ret;
}

std::wstring VaultTheme::GetFileName(int id)
{
    std::wstring ret;
    if(id<MainWindow.hTheme->GetNumItems())
        ret=namelist[id];
    return ret;
}

//}
