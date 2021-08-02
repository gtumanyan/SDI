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

#include "com_header.h"
#include "common.h"
#include "logging.h"
#include "system.h"
#include "settings.h"
#include "theme.h"
#include "manager.h"

#include "7zip.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
#define BOOST_SYSTEM_NO_DEPRECATED
#include <boost/thread/condition_variable.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//{ Version
int Version::setDate(int d_,int m_,int y_)
{
    d=d_;
    m=m_;
    y=y_;

    int flag=0;
    if(y<100)y+=1900;
    if(y<1990)flag=1;
    if(y>2015)flag=2;
    switch(m)
    {
        case 1:case 3:case 5:case 7:case 8:case 10:case 12:
            if(d<1||d>31)flag=3;
            break;
        case 4:case 6:case 9:case 11:
            if(d<1||d>30)flag=4;
            break;
        case 2:
            if(d<1||d>((((y%4==0)&&(y%100))||(y%400==0))?29:28))flag=5;
            break;
        default:
            flag=6;
    }
    return flag;
}

void Version::setVersion(int v1_,int v2_,int v3_,int v4_)
{
    v1=v1_;
    v2=v2_;
    v3=v3_;
    v4=v4_;
}

void Version::str_date(WStringShort &buf,bool invariant)const
{
    SYSTEMTIME tm;
    FILETIME ft;

    memset(&tm,0,sizeof(SYSTEMTIME));
    tm.wDay=(WORD)d;
    tm.wMonth=(WORD)m;
    tm.wYear=(WORD)y;
    SystemTimeToFileTime(&tm,&ft);
    int r=FileTimeToSystemTime(&ft,&tm);

    if(y<1000||!r)
        buf.sprintf(STR(STR_HINT_UNKNOWN));
    else if(invariant)
        buf.sprintf(L"%02d/%02d/%d",m,d,y);
    else
        GetDateFormat(invariant?LOCALE_INVARIANT:manager_g->getlocale(),0,&tm,nullptr,buf.GetV(),static_cast<int>(buf.Length()));
}

void Version::str_version(WStringShort &buf)const
{
    if(v1<0)
        buf.sprintf(STR(STR_HINT_UNKNOWN));
    else
        buf.sprintf(L"%d.%d.%d.%d",v1,v2,v3,v4);
}

int cmpdate(const Version *t1,const Version *t2)
{
    int res;

    if(Settings.flags&FLAG_FILTERSP&&t2->y<1000)return 0;

    res=t1->y-t2->y;
    if(res)return res;

    res=t1->m-t2->m;
    if(res)return res;

    res=t1->d-t2->d;
    if(res)return res;

    return 0;
}

int cmpversion(const Version *t1,const Version *t2)
{
    int res;

    if(Settings.flags&FLAG_FILTERSP&&t2->v1<0)return 0;

    res=t1->v1-t2->v1;
    if(res)return res;

    res=t1->v2-t2->v2;
    if(res)return res;

    res=t1->v3-t2->v3;
    if(res)return res;

    res=t1->v4-t2->v4;
    if(res)return res;

    return 0;
}
//}

//{ Txt
size_t Txt::strcpy(const char *str)
{
    size_t r=text.size();
    text.insert(text.end(),str,str+strlen(str)+1);
    return r;
}

size_t Txt::strcpyw(const wchar_t *str)
{
    size_t r=text.size();
    text.insert(text.end(),reinterpret_cast<const char *>(str),reinterpret_cast<const char *>(str+wcslen(str)+1));
    return r;
}

size_t Txt::t_memcpy(const char *mem,size_t sz)
{
    size_t r=text.size();
    text.insert(text.end(),mem,mem+sz);
    return r;
}

size_t Txt::t_memcpyz(const char *mem,size_t sz)
{
    size_t r=text.size();
    text.insert(text.end(),mem,mem+sz);
    text.insert(text.end(),0);
    return r;
}

size_t Txt::memcpyz_dup(const char *mem,size_t sz)
{
    std::string str(mem,sz);
    auto it=dub.find(str);

    if(it==dub.end())
    {
        size_t r=text.size();
        text.insert(text.end(),mem,mem+sz);
        text.insert(text.end(),0);

        dub.insert({std::move(str),r});
        return r;
    }
    else
    {
        return it->second;
    }
}

size_t Txt::alloc(size_t sz)
{
    size_t r=text.size();
    text.resize(r+sz);
    return r;
}

Txt::Txt()
{
    reset(2);
    text[0]=text[1]=0;
}

void Txt::reset(size_t sz)
{
    text.resize(sz);
    text.reserve(1024*1024*2); //TODO
}

void Txt::shrink()
{
    //Log.print_con("Text_usage %d/%d\n",text.size(),text.capacity());
    text.shrink_to_fit();
}
//}

//{ Hashtable
unsigned Hashtable::gethashcode(const char *s,size_t sz)
{
    int h=5381;

    while(sz--)
    {
        int ch=*s++;
        h=((h<<5)+h)^ch;
    }
    return h;
}

void Hashtable::reset(size_t size1)
{
    size=(int)size1;
    if(!size)size=1;
    items.resize(size);
    items.reserve(size*sizeof(int));
    memset(items.data(),0,size*sizeof(Hashitem));
}

char *Hashtable::savedata(char *p)
{
    memcpy(p,&size,sizeof(int));p+=sizeof(int);
    p=items.savedata(p);
    return p;
}

char *Hashtable::loaddata(char *p)
{
    memcpy(&size,p,sizeof(int));p+=sizeof(int);
    items.resize(size);
    return items.loaddata(p);
}

/*
next
      0: free
     -1: used,next is free
  1..x : used,next is used
*/
void Hashtable::additem(int key,int value)
{
    int curi=gethashcode((char *)&key,sizeof(int))%size;
    Hashitem *cur=&items[curi];

    int previ=-1;
    if(cur->next!=0)
    do
    {
        cur=&items[curi];
        previ=curi;
    }
    while((curi=cur->next)>0);

    if(cur->next==-1)
    {
        items.emplace_back(Hashitem());
        cur=&items.back();
        curi=(int)(items.size()-1);
    }

    cur->key=key;
    cur->value=value;
    cur->next=-1;
    if(previ>=0)(&items[previ])->next=curi;
}

int Hashtable::find(int key,int *isfound)
{
    if(!size)
    {
        *isfound=0;
        return 0;
    }

    int curi=gethashcode((char *)&key,sizeof(int))%size;
    Hashitem *cur=&items[curi];

    if(cur->next<0)
    {
        if(key==cur->key)
        {
            findnext_v=cur->next;
            findstr=key;
            *isfound=1;
            return cur->value;
        }
    }

    if(cur->next==0)
    {
        *isfound=0;
        return 0;
    }

    do
    {
        cur=&items[curi];
        if(key==cur->key)
        {
            findnext_v=cur->next;
            findstr=key;
            *isfound=1;
            return cur->value;
        }
    } while((curi=cur->next)>0);

    *isfound=0;
    return 0;
}

int Hashtable::findnext(int *isfound)
{
    Hashitem *cur;
    int curi=findnext_v;

    *isfound=0;
    if(curi<=0)return 0;

    cur=&items[curi];
    do
    {
        cur=&items[curi];
        if(cur->key==findstr)
        {
            findnext_v=cur->next;
            *isfound=1;
            return cur->value;
        }
    } while((curi=cur->next)>0);
    return 0;
}
//}

//{ Strings
void WString_dyn::Resize(size_t size)
{
    Log.print_con("Resize to %d->",len);
    len=size;
#ifndef _MSC_VER
    wchar_t *old=buf_dyn;
#endif
    buf_dyn=new wchar_t[len];
    lstrcpy(buf_dyn,buf_cur);
    buf_cur=buf_dyn;
#ifndef _MSC_VER
    delete[] old;
#endif
    Log.print_con("%d\n",len);
}

void WString_dyn::sprintf(const wchar_t *format,...)
{
    va_list args;
    va_start(args,format);
    vsprintf(format,args);
    va_end(args);
}
void WString_dyn::vsprintf(const wchar_t *format,va_list args)
{
    unsigned r=System._vscwprintf_dll(format,args)+1;
    if(r>len)Resize(r);
#ifdef _MSC_VER
    r=vswprintf_s(buf_cur,len,format,args);
#else
    r=_vswprintf(buf_cur,format,args);
#endif
    if(debug)Log.print_con("[%S]\n",buf_cur);
}

void WString_dyn::append(const wchar_t *str)
{
    size_t sz=wcslen(buf_cur)+wcslen(str)+1;
    if(sz>len)Resize(sz);
#ifdef _MSC_VER
    wcscat_s(buf_cur,len,str);
#else
    wcscat(buf_cur,str);
#endif
}

void WString_dyn::strcpy(const wchar_t *str)
{
    size_t sz=wcslen(str)+1;
    if(sz>len)Resize(sz);
    wcscpy(buf_cur,str);
}

void strsub(wchar_t *str,const wchar_t *pattern,const wchar_t *rep)
{
    wchar_t *s;

    s=StrStrIW(str,pattern);
    if(s)
    {
        wchar_t buf[BUFLEN];
        wcscpy(buf,s);
        wcscpy(s,rep);
        wcscpy(s+wcslen(rep),buf+wcslen(pattern));
    }
}

void strtoupper(const char *s1,size_t len)
{
    char *s=const_cast<char *>(s1);
    while(len--)
    {
        *s=static_cast<char>(toupper(*s));
        s++;
    }
}

void strtolower(const char *s1,size_t len)
{
    char *s=const_cast<char *>(s1);
    if(len)
    while(len--)
    {
        *s=static_cast<char>(tolower(*s));
        s++;
    }
}

std::string to_lower(std::string str)
{
    for( char& c : str ) c = std::tolower(c) ;
    return str ;
}

size_t unicode2ansi(const unsigned char *s,char *out,size_t size)
{
    size_t ret;
    int flag;
    size/=2;
    /*if(!out)Log.log_err("Error out:\n");
    if(!ses)Log.log_err("Error in:\n");
    if(size<0)Log.log_err("Error size:\n");*/
    ret=WideCharToMultiByte(CP_ACP,0,(wchar_t *)(s+(s[0]==0xFF?2:0)),(int)(size-(s[0]==0xFF?1:0)),out,(int)size,nullptr,&flag);
    if(!ret)Log.print_syserr(GetLastError(),L"unicode2ansi()");
    out[size]=0;
    return ret;
}

int _wtoi_my(const wchar_t *str)
{
    int val;
    swscanf(str,L"%d",&val);
    return val;
}
//}

//{ 7-zip
size_t encode(char *dest,size_t dest_sz,const char *src,size_t src_sz)
{
    Lzma86_Encode((Byte *)dest,(SizeT *)&dest_sz,(const Byte *)src,src_sz,0,1<<23,SZ_FILTER_AUTO);
    return dest_sz;
}

size_t decode(char *dest,size_t dest_sz,const char *src,size_t src_sz)
{
    Lzma86_Decode((Byte *)dest,(SizeT *)&dest_sz,(const Byte *)src,(SizeT *)&src_sz);
    return dest_sz;
}

void registerall()
{
    NArchive::N7z::register7z();
    registerBCJ();
    registerBCJ2();
    //registerBranch();
    registerCopy();
    registerLZMA();
    registerLZMA2();

    //CrcGenerateTable();
}
//}

std::vector<std::wstring> split(const std::wstring &s, wchar_t delim)
{
    std::vector<std::wstring> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}
