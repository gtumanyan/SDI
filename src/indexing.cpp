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

//#define MERGE_FINDER
#include "com_header.h"
#include "common.h"
#include "logging.h"
#include "system.h"
#include "settings.h"
#include "matcher.h"
#include "indexing.h"
#include "manager.h"

#include "7zip.h"
#ifdef _MSC_VER
#include <process.h>
#endif

// Depend on Win32API
#include "enum.h"
#include "main.h"

#include <queue>

#define kInputBufSize ((size_t)1 << 18)

//{ Global variables
static const ISzAlloc g_Alloc = { SzAlloc, SzFree };
int drp_count;
int drp_cur;
int loaded_unpacked=0;
int volatile cur_,count_;
drplist_t *queuedriverpack_p;

const tbl_t table_version[NUM_VER_NAMES]=
{
    {"classguid",                  9},
    {"class",                      5},
    {"provider",                   8},
    {"catalogfile",                11},
    {"catalogfile.nt",             14},
    {"catalogfile.ntx86",          17},
    {"catalogfile.ntia64",         18},
    {"catalogfile.ntamd64",        19},
    {"driverver",                  9},
    {"driverpackagedisplayname",   24},
    {"driverpackagetype",          17}
};
const wchar_t *olddrps[]=
{
    L"DP_Video_Server_1",
    L"DP_Video_Others_1",
    L"DP_Video_nVIDIA_1",
    L"DP_Video_AMD_1",
    L"DP_Videos_AMD_1",
    L"DP_LAN_Realtek_1",
};
//}

//{ Concurrent_queue
template<typename Data>
class concurrent_queue
{
    concurrent_queue(const concurrent_queue&)=delete;
    concurrent_queue &operator = (const concurrent_queue&)=delete;

private:
    std::queue<Data> the_queue;
    mutable boost::mutex the_mutex;
    HANDLE notification;
    int num;

public:
    concurrent_queue()
    {
        num=0;
        notification=CreateEvent(nullptr,0,0,nullptr);
    }

    ~concurrent_queue()
    {
        CloseHandle(notification);
    }

    void push(Data const& data)
    {
        #ifndef _WIN64
        while(num>250)
        {
            Log.print_debug("The queue is full. Waiting...\n");
            Sleep(100);
        }
        #endif
        boost::mutex::scoped_lock lock(the_mutex);
        the_queue.push(data);
        num++;
        lock.unlock();
        SetEvent(notification);
    }

    void wait_and_pop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);

        while(the_queue.empty())
        {
            lock.unlock();
            WaitForSingleObject(notification,INFINITE);
            lock.lock();
        }
        popped_value=the_queue.front();
        num--;
        the_queue.pop();
    }
};
//}

//{ Misc functions
static int Buf_EnsureSize(CBuf *dest, size_t size)
{
  if (dest->size >= size)
    return 1;
  Buf_Free(dest, &g_Alloc);
  return Buf_Create(dest, size, &g_Alloc);
}

void mySzFree(void *p,void *address)
{
    UNREFERENCED_PARAMETER(p);

    delete[](char*)(address);
}

void findosattr(char *bufa,const char *adr,size_t len)
{
    size_t bufal=0;
    const char *p=adr;
    static wchar_t osatt[]={L'O',L'S',L'A',L't',L't'}; // OSAtt

    *bufa=0;
    while(p+11<adr+len)
    {
        if(*p=='O'&&!memcmp(p,osatt,10))
        {
            int ofs=p[19]=='2'||p[19]=='1'?1:0;
            if(!*bufa||bufal<wcslen((wchar_t *)(p+18+ofs)))
            {
                wsprintfA(bufa,"%ws",p+18+ofs);
                bufal=strlen(bufa);
            }
        }
        p++;
    }
}
//}

//{ Parser
void Parser::parseWhitespace(bool eatnewline=false)
{
    while(blockBeg<blockEnd)
    {
        switch(*blockBeg)
        {
//            case 0x1A:
            case '\n':
            case '\r':
                if(eatnewline==false)return;
                blockBeg++;
                break;

            case 32:  // space
            case '\t':// tab
                blockBeg++;
                break;

            case ';': // comment
                blockBeg++;
                while(blockBeg<blockEnd&&*blockBeg!='\n'&&*blockBeg!='\r')blockBeg++;
                break;

            case '\\': // continue line
                if(blockBeg+3<blockEnd&&blockBeg[1]=='\r'&&blockBeg[2]=='\n'){blockBeg+=3;break;}

            default:
                return;
        }
    }
}

void Parser::trimtoken()
{
    while(strEnd>strBeg&&(strEnd[-1]==32||strEnd[-1]=='\t')&&strEnd[-1]!='\"')strEnd--;
    if(*strBeg=='\"')strBeg++;
    if(*(strEnd-1)=='\"')strEnd--;
}

void Parser::subStr()
{
    if(!pack)return;

    // Fast string substitution
    const char *v1b=strBeg;
    if(*v1b=='%')
    {
        v1b++;
        ptrdiff_t vers_len=strEnd-v1b-1;
        if(strEnd[-1]!='%')vers_len++;
        if(vers_len<0)vers_len=0;

        strtolower(v1b,vers_len);
        auto rr=string_list->find(std::string(v1b,vers_len));
        if(rr!=string_list->end())
        {
            strBeg=const_cast<char *>(rr->second.c_str());
            strEnd=strBeg+strlen(strBeg);
            return;
        }
    }

    // Advanced string substitution
    char static_buf[BUFLEN];
    char *p_s=static_buf;
    int flag=0;
    v1b=strBeg;
    while(v1b<strEnd)
    {
        while(*v1b!='%'&&v1b<strEnd)*p_s++=*v1b++;
        if(*v1b=='%')
        {
            const char *p=v1b+1;
            while(*p!='%'&&p<strEnd)p++;
            if(*p=='%')
            {
                strtolower(v1b+1,p-v1b-1);
                auto rr=string_list->find(std::string(v1b+1,p-v1b-1));
                if(rr!=string_list->end())
                {
                    char *res=const_cast<char *>(rr->second.c_str());
                    strcpy(p_s,res);
                    p_s+=strlen(res);
                    v1b=p+1;
                    flag=1;
                }
#ifdef DEBUG_EXTRACHECKS
                else Log.print_con("String '%s' not found in %S(%S)\n",std::string(v1b+1,p-v1b-1).c_str(),pack->getFilename(),inffile);
#endif
            }
            if(v1b<strEnd)*p_s++=*v1b++;
        }
    }
    if(!flag)return;

    *p_s=0;
    strBeg=textholder.get(textholder.strcpy(static_buf));
    strEnd=strBeg+strlen(strBeg);
}

int Parser::parseItem()
{
    parseWhitespace(true);
    strBeg=blockBeg;

    const char *p=blockBeg;

    while(p<blockEnd-1)
    {
        switch(*p)
        {
            case '=':               // Item found
                blockBeg=p;
                strEnd=p;
                trimtoken();
                subStr();
                if(strBeg>strEnd)return 0;
                return 1;

            case '\n':case '\r':    // No item found
                blockBeg=p++;
                parseWhitespace(true);
                p=strBeg=blockBeg;
#ifdef DEBUG_EXTRACHECKS
                Log.print_con("ERROR: no item '%s' found in %S(%S){%s}\n\n",std::string(blockBeg,30).c_str(),pack->getFilename(),inffile,std::string(blockEnd,30).c_str());
#endif
                break;
            default:
                p++;
        }
    }
    strBeg=nullptr;
    strEnd=nullptr;
    return 0;
}

int Parser::parseField()
{
    if(blockBeg[0]!='='&&blockBeg[0]!=',')return 0;
    blockBeg++;
    parseWhitespace();

    const char *p=blockBeg;

    strBeg=strEnd=p;

    if(*p=='\"')    // "str"
    {
        strBeg++;
        p++;
        while(p<blockEnd)
        {
            switch(*p)
            {
                case '\r':case '\n': // no closing "
                    p++;
#ifdef DEBUG_EXTRACHECKS
                    log_file("ERR2 '%.*s'\n",30,s1b-1);
#endif
                    break;
                case '\"':          // "str"
                    strEnd=p;
                    blockBeg=strEnd+1;
                    subStr();
                    if(strBeg>strEnd)return 0;
                    return 1;

                default:
                    p++;
            }
        }
    }
    else
    {
        while(p<blockEnd)
        {
            switch(*p)
            {
                case '\n':case '\r':
                case ';':
                case ',':
                    strEnd=p;
                    blockBeg=p;
                    trimtoken();
                    subStr();
                    if(strBeg>strEnd)return 0;
                    return strEnd!=strBeg||*p==',';

                default:
                    p++;
            }
        }
    }
    return 0;
}

int Parser::readNumber()
{
    int n=atoi(strBeg);

    while(strBeg<strEnd&&*strBeg>='0'&&*strBeg<='9')strBeg++;
    if(strBeg<strEnd)strBeg++;
    return n;
}

int Parser::readHex()
{
    int val=0;

    while(strBeg<strEnd&&(*strBeg=='0'||*strBeg=='x'))strBeg++;
    if(strBeg<strEnd)
        val=toupper(*strBeg)-(*strBeg<='9'?'0':'A'-10);

    strBeg++;
    if(strBeg<strEnd)
    {
        val<<=4;
        val+=toupper(*strBeg)-(*strBeg<='9'?'0':'A'-10);
    }
    return val;
}

int Parser::readDate(Version *t)
{
    while(strBeg<strEnd&&!(*strBeg>='0'&&*strBeg<='9'))strBeg++;
    int m=readNumber();
    int d=readNumber();
    int y=readNumber();
    return t->setDate(d,m,y);

}

void Parser::readVersion(Version *t)
{
    int v1=readNumber();
    int v2=readNumber();
    int v3=readNumber();
    int v4=readNumber();
    t->setVersion(v1,v2,v3,v4);
}

void Parser::readStr(const char **vb,const char **ve)
{
    *vb=strBeg;
    *ve=strEnd;
}

Parser::Parser(Driverpack *drpv,std::unordered_map<std::string,std::string> &string_listv,const wchar_t *inf):
    pack(drpv),
    string_list(&string_listv),
    inffile(inf)
{
}

Parser::Parser(char *vb,char *ve):
    pack(nullptr),
    strBeg(vb),
    strEnd(ve)
{
}

void Parser::setRange(const sect_data_t *lnk)
{
    blockBeg=lnk->blockbeg;
    blockEnd=lnk->blockend;
}
//}

//{ Collection
int Collection::scanfolder_count(const wchar_t *path)
{
    int cnt=0;

    WStringShort buf;
    buf.sprintf(L"%s\\*.*",path);
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind=FindFirstFile(buf.Get(),&FindFileData);
    while(FindNextFile(hFind,&FindFileData)!=0)
    {
        if(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
        {
            if(lstrcmp(FindFileData.cFileName,L"..")==0)continue;
            buf.sprintf(L"%s\\%ws",path,FindFileData.cFileName);
            if(!loaded_unpacked)
                cnt+=scanfolder_count(buf.Get());
        }
        else
        {
            size_t i,len=wcslen(FindFileData.cFileName);
            if(len<3)continue;

            for(i=0;i<6;i++)
                if(StrStrIW(FindFileData.cFileName,olddrps[i]))
                {
                    if(Settings.flags&(FLAG_AUTOINSTALL|FLAG_NOGUI))break;
                    buf.sprintf(L" /c del \"%s\\%s*.7z\" /Q /F",driverpack_dir,olddrps[i]);
                    System.run_command(L"cmd",buf.Get(),SW_HIDE,1);
                    break;
                }
            if(i==6&&_wcsicmp(FindFileData.cFileName+len-3,L".7z")==0)
            {
                Driverpack drp{path,FindFileData.cFileName,this};
                if(Settings.flags&COLLECTION_FORCE_REINDEXING||!drp.checkindex())cnt++;
            }
        }
    }
    FindClose(hFind);
    return cnt;
}

void Collection::scanfolder(const wchar_t *path,void *arg)
{
    WStringShort buf;
    buf.sprintf(L"%s\\*.*",path);
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind=FindFirstFile(buf.Get(),&FindFileData);
    size_t pathlen=wcslen(driverpack_dir)+1;

    while(FindNextFile(hFind,&FindFileData)!=0)
    {
        if(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
        {
            if(lstrcmp(FindFileData.cFileName,L"..")==0)continue;
            buf.sprintf(L"%s\\%ws",path,FindFileData.cFileName);
            if(!loaded_unpacked)
                scanfolder(buf.Get(),arg);
        }
        else
        {
            size_t len=wcslen(FindFileData.cFileName);
            if(len<3)continue;

            if(_wcsicmp(FindFileData.cFileName+len-3,L".7z")==0)
            {
                driverpack_list.push_back(Driverpack(path,FindFileData.cFileName,this));
                reinterpret_cast<drplist_t *>(arg)->push(driverpack_task{&driverpack_list.back()});
            }
            else
                if((_wcsicmp(FindFileData.cFileName+len-4,L".inf")==0||
                    _wcsicmp(FindFileData.cFileName+len-4,L".cat")==0)&&loaded_unpacked==0)
                {
                    buf.sprintf(L"%s\\%s",path,FindFileData.cFileName);
                    FILE *f=_wfopen(buf.Get(),L"rb");
                    _fseeki64(f,0,SEEK_END);
                    len=static_cast<size_t>(_ftelli64(f));
                    _fseeki64(f,0,SEEK_SET);
                    char *buft=new char[len];
                    fread(buft,len,1,f);
                    fclose(f);
                    buf.sprintf(L"%s\\",path+pathlen);

                    if(len)
                    {
                        if(_wcsicmp(FindFileData.cFileName+wcslen(FindFileData.cFileName)-4,L".inf")==0)
                            driverpack_list[0].indexinf(buf.Get(),FindFileData.cFileName,buft,len);
                        else
                            driverpack_list[0].parsecat(buf.Get(),FindFileData.cFileName,buft,len);
                    }

                    delete[]buft;
                }
        }
    }
    FindClose(hFind);
}

void Collection::loadOnlineIndexes()
{
    // load online indexes for DriverPacks
    // that i don't have
    WStringShort buf;;
    buf.sprintf(L"%s\\_*.*",index_bin_dir);
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind=FindFirstFile(buf.Get(),&FindFileData);

    while(FindNextFile(hFind,&FindFileData)!=0)
    {
        wchar_t filename[BUFLEN];
        wsprintf(filename,L"%ws",FindFileData.cFileName);
        wcscpy(filename+wcslen(FindFileData.cFileName)-3,L"7z");

        buf.sprintf(L"drivers\\%ws",filename);
        (buf.GetV())[8]=L'D';
        if(System.FileExists(buf.Get()))
        {
            Log.print_debug("Skip %S\n",buf.Get());
            continue;
        }

        driverpack_list.push_back(Driverpack(driverpack_dir,filename,this));
        driverpack_list.back().loadindex();
    }
    FindClose(hFind);
}

void Collection::init(const wchar_t *driverpacks_dirv,const wchar_t *index_bin_dirv,const wchar_t *index_linear_dirv)
{
    driverpack_dir=driverpacks_dirv;
    index_bin_dir=index_bin_dirv;
    index_linear_dir=index_linear_dirv;
    driverpack_list.clear();
}

Collection::Collection(const wchar_t *driverpacks_dirv,const wchar_t *index_bin_dirv,const wchar_t *index_linear_dirv)
{
    driverpack_dir=driverpacks_dirv;
    index_bin_dir=index_bin_dirv;
    index_linear_dir=index_linear_dirv;
}

void Collection::updatedir()
{
    driverpack_dir=*Settings.drpext_dir?Settings.drpext_dir:Settings.drp_dir;
    populate();
}

void Collection::populate()
{
    Log.print_debug("Collection::populate\n");
    Driverpack *unpacked_drp;

    Timers.start(time_indexes);

    driverpack_list.reserve(drp_count+1+300); // TODO
    driverpack_list.push_back(Driverpack(driverpack_dir,L"unpacked.7z",this));
    unpacked_drp=&driverpack_list.back();

    if(Settings.flags&FLAG_KEEPUNPACKINDEX)loaded_unpacked=unpacked_drp->loadindex();
    drp_count=scanfolder_count(driverpack_dir);

    if((Settings.flags&FLAG_KEEPUNPACKINDEX)&&!loaded_unpacked)
        manager_g->itembar_settext(SLOT_INDEXING,1,L"Unpacked",1,++drp_count,0);

//{thread
    drplist_t queuedriverpack1;
    queuedriverpack_p=&queuedriverpack1;
    UInt32 num_thr=num_cores;
    UInt32 num_thr_1=num_cores;

    Log.print_debug("Collection::populate::num_thr::%d\n",num_thr);

    drplist_t queuedriverpack;
    std::vector<ThreadAbs*> cons;
    for(UInt32 i=0;i<num_thr;i++)
    {
        Log.print_debug("Collection::populate::ThreadAbs::%d\n",i);
        cons.push_back(CreateThread());
        cons[i]->start(&Driverpack::loaddrp_thread,&queuedriverpack);
    }

    Log.print_debug("Collection::populate::num_thr_1::%d\n",num_thr_1);
    std::vector<ThreadAbs*> thr;
    for(UInt32 i=0;i<num_thr_1;i++)
    {
        Log.print_debug("Collection::populate::ThreadAbs1::%d\n",i);
        thr.push_back(CreateThread());
        thr[i]->start(&Driverpack::indexinf_thread,&queuedriverpack1);
    }
//}thread

    drp_cur=1;

    Log.print_debug("Collection::populate::scanfolder::%S\n",driverpack_dir);
    scanfolder(driverpack_dir,&queuedriverpack);
    for(UInt32 i=0;i<num_thr;i++)
    {
        Log.print_debug("Collection::populate::queuedriverpack.push::%d\n",i);
        queuedriverpack.push(driverpack_task{nullptr});
    }

    for(UInt32 i=0;i<num_thr;i++)
    {
        Log.print_debug("Collection::populate::cons[i]->join::%d\n",i);
        cons[i]->join();
        delete cons[i];
    }

    Log.print_debug("Collection::populate::loadOnlineIndexes\n");
    loadOnlineIndexes();

    Log.print_debug("Collection::populate::itembar\n");
    manager_g->itembar_setactive(SLOT_INDEXING,0);
    if(driverpack_list.size()<=1&&(Settings.flags&FLAG_DPINSTMODE)==0)
    {
        emptydrp=true;
        if((Settings.flags&FLAG_CHECKUPDATES)==0)
            manager_g->itembar_settext(SLOT_NODRIVERS,L"",0);
    }
    else
        emptydrp=false;

    Log.print_debug("Collection::populate::genhashes\n");
    driverpack_list[0].genhashes();

//{thread
    Log.print_debug("Collection::populate::queuedriverpack1\n");
    for(UInt32 i=0;i<num_thr_1;i++)queuedriverpack1.push(driverpack_task{nullptr});

    for(UInt32 i=0;i<num_thr_1;i++)
    {
        thr[i]->join();
        delete thr[i];
    }
//}thread
    Settings.flags&=~COLLECTION_FORCE_REINDEXING;
    Log.print_debug("Collection::populate::driverpack_list.shrink_to_fit\n");
    driverpack_list.shrink_to_fit();
    Timers.stop(time_indexes);
    Log.print_debug("Collection::populate::Done\n");
}

void Collection::save()
{
    if(*Settings.drpext_dir)return;
    if(!System.canWriteDirectory(index_bin_dir))
    {
        Log.print_err("ERROR in collection_save(): Write-protected,'%S'\n",index_bin_dir);
        return;
    }
    Timers.start(time_indexsave);

    // Save indexes
    count_=0;
    cur_=1;
    if((Settings.flags&FLAG_KEEPUNPACKINDEX)==0)
        driverpack_list[0].setType(DRIVERPACK_TYPE_INDEXED);
    for(auto &driverpack:driverpack_list)
        if(driverpack.getType()==DRIVERPACK_TYPE_PENDING_SAVE)count_++;

    if(count_)Log.print_con("Saving indexes...\n");
    std::vector<ThreadAbs *> thr;
    drplist_t queuedriverpack_loc;
    for(UInt32 i=0;i<num_cores;i++)
    {
        thr.push_back(CreateThread());
        thr[i]->start(&Driverpack::savedrp_thread,&queuedriverpack_loc);
    }
    for(auto &driverpack:driverpack_list)
        if(driverpack.getType()==DRIVERPACK_TYPE_PENDING_SAVE)
            queuedriverpack_loc.push(driverpack_task{&driverpack});

    for(UInt32 i=0;i<num_cores;i++)queuedriverpack_loc.push(driverpack_task{nullptr});
    for(UInt32 i=0;i<num_cores;i++)
    {
        thr[i]->join();
        delete thr[i];
    }
    manager_g->itembar_settext(SLOT_INDEXING,0);
    if(count_)Log.print_con("DONE\n");

    // Delete unused indexes
    WIN32_FIND_DATA FindFileData;
    WStringShort filename;
    WStringShort buf;
    buf.sprintf(L"%ws\\*.*",index_bin_dir);
    HANDLE hFind=FindFirstFile(buf.Get(),&FindFileData);
    while(FindNextFile(hFind,&FindFileData)!=0)
    {
        if(!(FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
        {
            if(FindFileData.cFileName[0]==L'_')continue;
            filename.sprintf(L"%s\\%s",index_bin_dir,FindFileData.cFileName);
            size_t i;
            for(i=Settings.flags&FLAG_KEEPUNPACKINDEX?0:1;i<driverpack_list.size();i++)
            {
                wchar_t buf1[BUFLEN];
                driverpack_list[i].getindexfilename(index_bin_dir,L"bin",buf1);
                if(!_wcsicmp(buf1,filename.Get()))break;
            }
            if(i==driverpack_list.size())
            {
                Log.print_con("Deleting %S\n",filename.Get());
                _wremove(filename.Get());
            }
        }
    }
    Timers.stop(time_indexsave);
}

void Collection::printstats()
{
    if(Log.isHidden(LOG_VERBOSE_DRP))return;

    size_t sum=0;
    Log.print_file("DriverPacks\n");
    for(auto &drp:driverpack_list)
        sum+=drp.printstats();

    Log.print_file("  Sum: %d\n\n",sum);
}

void Collection::print_index_hr()
{
    Timers.start(time_indexprint);

    for(auto &drp:driverpack_list)
        drp.print_index_hr();

    Timers.stop(time_indexprint);
}

const wchar_t *Collection::finddrp(const wchar_t *fnd)
{
    int j;
    const wchar_t *s,*d,*n_s;

    j=0;
    n_s=nullptr;
    for(auto &drp:driverpack_list)
    {
        s=drp.getFilename();
        if(StrStrIW(s,fnd)&&drp.getType()!=DRIVERPACK_TYPE_UPDATE)
        {
            d=s;
            while(*d)
            {
                if(*d==L'_'&&d[1]>=L'0'&&d[1]<=L'9')
                {
                    if(j<_wtoi_my(d+1))
                    {
                        j=_wtoi_my(d+1);
                        n_s=s;
                    }
                    break;
                }
                d++;
            }
        }
    }
    return n_s;
}
//}

#ifdef MERGE_FINDER
class Filedata
{
    std::wstring str;
    int size;
    unsigned crc;

public:
    const wchar_t *getStr(){return str.c_str();}
    const wchar_t *getPath(){return str.c_str();}
    const wchar_t *getFilename(){return str.c_str();}
    bool checksize(int _size){return size==_size;}
    bool checkCRC(unsigned _CRC){return crc==_CRC;}
    bool checkself(const wchar_t *_str){return wcscmp(str.c_str(),_str);}
    Filedata(std::wstring _str,int _size,unsigned _crc):str(_str),size(_size),crc(_crc){}

    friend class Merger;
};

class Merger
{
    std::unordered_set<std::wstring> merged;
    std::unordered_set<std::wstring> dirlist;
    std::unordered_multimap<std::wstring,Filedata> filename2path;
    std::unordered_multimap<std::wstring,Filedata> path2filename;
    std::unordered_multimap<std::wstring,std::wstring> dir2dir;
    FILE *f;
    CSzArEx *db;

public:
    Merger(CSzArEx *_db,const wchar_t *fullname);
    ~Merger();
    void makerecords(int i);
    int checkfolders(const std::wstring &dir1,const std::wstring &dir2,int sub);
    void find_dups();
    int combine(std::wstring dir1,std::wstring dir2,int sz);
    void process_file(int i,unsigned *CRC,int *size,
                      std::wstring &_filename,std::wstring &_filepath,
                      std::wstring &_subdir1,std::wstring &_subdir2);
};

Merger::Merger(CSzArEx *_db,const wchar_t *fullname)
{
    f=_wfopen(fullname,L"wt");
    Log.print_con("Making %ws\n",fullname);
    db=_db;
}

Merger::~Merger()
{
    fclose(f);
}

void Merger::process_file(int i,unsigned *CRC,int *size,
                          std::wstring &_filename,std::wstring &_filepath,
                          std::wstring &_subdir1,std::wstring &_subdir2)
{
    *CRC=db->CRCs.Vals[i];
    *size=SzArEx_GetFileSize(db,i);
    wchar_t fullname[BUFLEN];
    SzArEx_GetFileNameUtf16(db,i,(UInt16 *)fullname);

    _filename.clear();
    _filepath.clear();
    _subdir1.clear();
    _subdir2.clear();

    wchar_t buf[BUFLEN];
    wchar_t filepath[BUFLEN];
    wcscpy(buf,fullname);
    wcscpy(filepath,fullname);
    wchar_t *p=buf,*filename=nullptr,*subdir1=buf,*subdir2=nullptr;

    while((p=wcschr(p,L'/'))!=nullptr)
    {
        p++;
        subdir2=filename;
        filename=p;
    }
    if(subdir1&&subdir2)
    {
        subdir2[-1]=0;
        subdir1[filename-buf-1]=0;
        _subdir1=subdir1;
        _subdir2=subdir2;
    }
    if(filename)
    {
        filepath[filename-buf-1]=0;
        _filename=filename;
        _filepath=filepath;
    }
    std::transform(_filename.begin(),_filename.end(),_filename.begin(),::tolower);
    std::transform(_filepath.begin(),_filepath.end(),_filepath.begin(),::tolower);
    std::transform(_subdir1.begin(),_subdir1.end(),_subdir1.begin(),::tolower);
    std::transform(_subdir2.begin(),_subdir2.end(),_subdir2.begin(),::tolower);
}

void Merger::makerecords(int i)
{
    std::wstring filename,filepath,subdir1,subdir2;
    unsigned CRC;
    int sz;

    process_file(i,&CRC,&sz,filename,filepath,subdir1,subdir2);
    //Log.print_con("%8X,%10d,{%ws},{%ws},{%ws},{%ws}\n",CRC,sz,filepath.c_str(),filename.c_str(),subdir1.c_str(),subdir2.c_str());

    if(!filename.empty())
    {
        filename2path.insert({filename,{filepath,sz,CRC}});
        path2filename.insert({filepath,{filename,sz,CRC}});
    }

    if(!subdir1.empty()&&!subdir2.empty())
    {
        std::wstring tstr=subdir1+subdir2;
        if(dirlist.find(tstr)==dirlist.end())
        {
            dir2dir.insert({subdir1,subdir2});
            dirlist.insert(tstr);
        }
    }
}

int Merger::checkfolders(const std::wstring &dir1,const std::wstring &dir2,int sub)
{
    int sizecom=0,sizedif=0;
    bool hasINF=false;

    if(merged.find(dir1)!=merged.end())return -1;
    if(merged.find(dir2)!=merged.end())return -1;

    auto range1=path2filename.equal_range(dir1);
    for(auto it1=range1.first;it1!=range1.second;it1++)
    {
        Filedata *d1=&it1->second;
        if(StrStrIW(d1->getStr(),L".inf"))hasINF=true;

        auto range2=filename2path.equal_range(d1->getStr());
        for(auto it2=range2.first;it2!=range2.second;it2++)
        {
            Filedata *d2=&it2->second;
            //Log.print_con("* %ws,%ws\n",it2->second.getStr(),dir2);
            if(d2->str==dir2)
            {
                if(d1->size==d2->size&&d1->crc==d2->crc)
                    sizecom+=d1->size;
                else
                    sizedif+=d1->size;
                //Log.print_con("* %ws\n",d1->getStr());

            }
        }
    }
    if(sub==0&&hasINF==false)
    {
        if(dir1.find(L"/")!=std::wstring::npos&&dir2.find(L"/")!=std::wstring::npos)
        {
            std::wstring dir1new=dir1;
            std::wstring dir2new=dir2;
            dir1new.resize(dir1new.rfind(L"/"));
            dir2new.resize(dir2new.rfind(L"/"));
            checkfolders(dir1new,dir2new,0);
            //Log.print_con("{%ws},{%ws}\n",dir1.c_str(),dir1new.c_str());
        }
        return -1;
    }

    auto range2=dir2dir.equal_range(dir1);
    for(auto it2=range2.first;it2!=range2.second;it2++)
    {
        int sz=checkfolders(dir1+L'/'+it2->second,dir2+L'/'+it2->second,1);
        //int sz=-1;
        if(sz<0)return -1;
        sizecom+=sz;
        //Log.print_con("# %ws\n",it2->second.c_str());
    }

    if(sizedif)return -1;
    //if(sub==0&&sizecom)Log.print_con("\n%d,%d\n%ws\n%ws\n",sizecom,sizedif,dir1.c_str(),dir2.c_str());

    if(sub==0&&sizecom>0)
    {
        if(combine(dir1,dir2,sizecom))
        {
            merged.insert(dir1);
            merged.insert(dir2);
        }
    }

    return sizecom;
}

void detectmarker(std::wstring str,int *i)
{
    char buf[BUFLEN];
    wsprintfA(buf,"%S",str.c_str());

    for(*i=0;*i<NUM_MARKERS;(*i)++)
    if(StrStrIA(buf,markers[*i].name))
    {
        return;
    }
    Log.print_con("Unk marker {%s}\n",buf);
    *i=-1;
}

int Merger::combine(std::wstring dir1,std::wstring dir2,int sz)
{
    int m1,m2;
    std::wstring dest(dir1);

    if(dir1.find(dir2)!=std::string::npos)return 0;
    if(dir2.find(dir1)!=std::string::npos)return 0;
    std::transform(dest.begin(),dest.end(),dest.begin(),::tolower);

    detectmarker(dir1,&m1);
    detectmarker(dir2,&m2);
    if(m1>=0&&m2>=0)
    {
        int major=markers[m1].major;
        int minor=markers[m1].minor;
        int arch=-1;

        if(markers[m1].major>=0&&markers[m1].minor>=0&&
           markers[m1].major>=markers[m2].major&&markers[m1].minor>=markers[m2].minor)
        {
            major=markers[m2].major;
            minor=markers[m2].minor;
        }
        else
        {
            major=markers[m1].major;
            minor=markers[m1].minor;
        }

        if(markers[m1].arch>=0)arch=markers[m1].arch;
        if(markers[m2].arch>=0)arch=markers[m2].arch;
        if(markers[m1].arch==0&&markers[m2].arch==1)arch=-1;
        if(markers[m1].arch==1&&markers[m2].arch==0)arch=-1;

        int i;
        for(i=0;i<NUM_MARKERS;i++)
        {
            if(markers[i].arch==arch&&
               markers[i].major==major&&
               markers[i].minor==minor)
            {
                wchar_t buf1[BUFLEN];
                wchar_t buf2[BUFLEN];
                wsprintfW(buf1,L"%S",markers[m1].name);
                wsprintfW(buf2,L"%S",markers[i].name);
                dest.replace(dest.find(buf1),wcslen(buf1),buf2);
                break;
            }
        }
        if(i==NUM_MARKERS)
        {
            /*wchar_t buf1[BUFLEN];
            wchar_t buf2[BUFLEN];
            wsprintfW(buf2,L" #(%d,%d,%d)",major,minor,arch);
            dest+=buf2;*/
        }
    }
    fprintf(f,"rem %d\n",sz);
    std::replace(dir1.begin(),dir1.end(),'/','\\');
    std::replace(dir2.begin(),dir2.end(),'/','\\');
    std::replace(dest.begin(),dest.end(),'/','\\');
    fprintf(f,"movefiles %S %S\n",dir1.c_str(),dest.c_str());
    fprintf(f,"movefiles %S %S\n\n",dir2.c_str(),dest.c_str());
    return 1;
}

static void PrintError(char *s)
{
  Print("\nERROR: ");
  Print(s);
  PrintLF();
}

void Merger::find_dups()
{
    Log.print_con("{");
    for(unsigned i=0;i<db->NumFiles;i++)
    {
        std::wstring filename,filepath,subdir1,subdir2;
        unsigned CRC;
        int sz;

        process_file(i,&CRC,&sz,filename,filepath,subdir1,subdir2);

        if(filename.empty())continue;
        if(merged.find(filepath)!=merged.end())continue;

        auto range=filename2path.equal_range(filename);
        for(auto it=range.first;it!=range.second;it++)
        {
            Filedata *d=&it->second;
            if(d->checkCRC(CRC)&&d->checksize(sz)&&d->checkself(filepath.c_str()))
            if(merged.find(d->getStr())==merged.end())
            {
                //Log.print_con(".");
                int szcom=checkfolders(filepath,d->str,0);
/*                if(szcom>1024*1024)
                {
                    combine(filepath,d->str,szcom);
                    //fprintf(f,"%S\n%S\n\n",filepath.c_str(),d->getStr());
                    merged.insert(filepath);
                    merged.insert(d->getStr());
                }*/
            }
        }
    }
    Log.print_con("}\n");
}
#endif

//{ Driverpack
int Driverpack::genindex()
{
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;

  CFileInStream archiveStream;
  CLookToRead2 lookStream;
  CSzArEx db;
  SRes res;

    wchar_t fullname[BUFLEN];
    WStringShort infpath;
    wchar_t *infname;

    WStringShort name;
    name.sprintf(L"%ws\\%ws",getPath(),getFilename());
  Log.print_con("\n7z Decoder " MY_VERSION_CPU " : " MY_COPYRIGHT_DATE "\n\n");
    Log.print_con("Indexing %S\n",name.Get());


  allocImp = g_Alloc;
  allocTempImp = g_Alloc;
    if (InFile_OpenW(&archiveStream.file,name.Get()));
  {
    Log.print_err("can not open input file");
    return 1;
  }

  FileInStream_CreateVTable(&archiveStream);
  LookToRead2_CreateVTable(&lookStream, False);
  lookStream.buf = NULL;

  res = SZ_OK;

  {
    lookStream.buf = (Byte *)ISzAlloc_Alloc(&allocImp, kInputBufSize);
    if (!lookStream.buf)
      res = SZ_ERROR_MEM;
    else
    {
      lookStream.bufSize = kInputBufSize;
      lookStream.realStream = &archiveStream.vt;
      LookToRead2_Init(&lookStream);
    }
  }
    
  CrcGenerateTable();
    
  SzArEx_Init(&db);
    
  if (res == SZ_OK)
  {
    res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
  }
    int cc=0;
  if (res == SZ_OK)
    {
#ifdef MERGE_FINDER
        getindexfilename(col->getIndex_linear_dir(),L"7z.bat",fullname);
        Merger merger{&db,fullname};
        for(unsigned i=0;i<db.NumFiles;i++)if(!SzArEx_IsDir(&db,i))merger.makerecords(i);
        merger.find_dups();
#endif
        /*
        if you need cache, use these 3 variables.
        if you use external function, you can make these variable as static.
        */
        UInt32 blockIndex=0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */
        Byte *outBuffer=nullptr; /* it must be 0 before first call for each new archive. */
        size_t outBufferSize=0;  /* it can have any value before first call (if outBuffer = 0) */

        for(unsigned i=0;i<db.NumFiles;i++)
        {
            size_t offset=0;
            size_t outSizeProcessed=0;
            if(SzArEx_IsDir(&db,i))continue;

            if(SzArEx_GetFileNameUtf16(&db,i,nullptr)>BUFLEN)
            {
                res=SZ_ERROR_MEM;
                Log.print_err("ERROR: mem\n");
                break;
            }
            SzArEx_GetFileNameUtf16(&db,i,(UInt16 *)fullname);

            size_t namelen=wcslen(fullname)-4;
            if(_wcsicmp(fullname+namelen,L".inf")==0||
                _wcsicmp(fullname+namelen,L".cat")==0)
            {
                //Log.print_con("{");

            tryagain:
                res=SzArEx_Extract(&db,&lookStream.vt,i,
                                   &blockIndex,&outBuffer,&outBufferSize,
                                   &offset,&outSizeProcessed,
                                   &allocImp,&allocTempImp);
                //Log.print_con("}");
                if(res==SZ_ERROR_MEM)
                {
                    Log.print_err("ERROR with %S:%d\n",getFilename(),res);
                    Sleep(100);
                    goto tryagain;
                    //continue;
                }

                wchar_t *ii=fullname;
                while(*ii){if(*ii=='/')*ii='\\';ii++;}
                infname=ii;
                while(infname!=fullname&&*infname!='\\')infname--;
                if(*infname=='\\'){*infname++=0;}
                infpath.sprintf(L"%ws\\",fullname);

                cc++;
                if(outSizeProcessed)
                {
                    if(StrStrIW(infname,L".infdrp"))
                        driverpack_indexinf_async(infpath.Get(),infname,outBuffer+offset,outSizeProcessed);
                    else if(StrStrIW(infname,L".inf"))
                        driverpack_indexinf_async(infpath.Get(),infname,outBuffer+offset,outSizeProcessed);
                    else
                        driverpack_parsecat_async(infpath.Get(),infname,outBuffer+offset,outSizeProcessed);
                }
            }
        }
        IAlloc_Free(&allocImp,outBuffer);
    }
    else
    {
        Log.print_err("ERROR with %S:%d\n",getFilename(),res);
    }
    SzArEx_Free(&db,&allocImp);
    File_Close(&archiveStream.file);
    return 1;
}

void Driverpack::driverpack_parsecat_async(wchar_t const *pathinf,wchar_t const *inffile1,const BYTE *adr,size_t len)
{
    inffile_task data;

    data.adr=new char[len];
    memmove(data.adr,adr,len);
    data.len=len;
    data.pathinf=new wchar_t[wcslen(pathinf)+1];
    data.inffile=new wchar_t[wcslen(inffile1)+1];
    wcscpy(data.pathinf,pathinf);
    wcscpy(data.inffile,inffile1);
    data.drp=this;
    objs_new->push(data);
}

void Driverpack::driverpack_indexinf_async(wchar_t const *pathinf,wchar_t const *inffile1,const BYTE *adr,size_t len)
{
    inffile_task data;

    data.drp=this;
    if(!adr)
    {
        data.adr=nullptr;// end marker
        data.inffile=nullptr;
        data.pathinf=nullptr;
        data.len=0;
        if(objs_new)objs_new->push(data);
        return;
    }

    if(len>4&&((adr[0]==0xFF&&adr[3]==0)||adr[0]==0))
    {
        data.adr=new char[len+2];
        if(!data.adr)
        {
            Log.print_err("ERROR in driverpack_indexinf: malloc(%d)\n",len+2);
            return;
        }
        len=unicode2ansi(adr,data.adr,len);
    }
    else
    {
        data.adr=new char[len];
        memmove(data.adr,adr,len);
    }

    data.len=len;
    data.pathinf=new wchar_t[wcslen(pathinf)+1];
    data.inffile=new wchar_t[wcslen(inffile1)+1];

    wcscpy(data.pathinf,pathinf);
    wcscpy(data.inffile,inffile1);
    objs_new->push(data);
}

void Driverpack::indexinf_ansi(wchar_t const *drpdir,wchar_t const *inffilename,const char *inf_base,size_t inf_len)
{
    // http://msdn.microsoft.com/en-us/library/ff547485(v=VS.85).aspx
    Version *cur_ver;

    size_t cur_inffile_index;
    data_inffile_t *cur_inffile;
    size_t cur_manuf_index;
    data_manufacturer_t *cur_manuf;
    size_t cur_desc_index;

    char secttry[256];
    char line[2048];
    ofst  strs[64];

    std::unordered_map<std::string,std::string> string_list;
    std::unordered_multimap<std::string,sect_data_t> section_list;

    const char *p=inf_base,*strend=inf_base+inf_len;
    const char *p2,*sectnmend;
    sect_data_t *lnk_s2=nullptr;

    cur_inffile_index=inffile.size();
    inffile.resize(cur_inffile_index+1);
    cur_inffile=&inffile[cur_inffile_index];
    wsprintfA(line,"%ws",drpdir);
    cur_inffile->infpath=static_cast<ofst>(text_ind.strcpy(line));
    wsprintfA(line,"%ws",inffilename);
    cur_inffile->inffilename=static_cast<ofst>(text_ind.strcpy(line));
    cur_inffile->infsize=static_cast<ofst>(inf_len);
    cur_inffile->infcrc=0;

    WStringShort inffull;
    inffull.sprintf(L"%s%s",drpdir,inffilename);

    for(int i=0; i<NUM_VER_NAMES; i++)cur_inffile->fields[i]=cur_inffile->cats[i]=0;

    Parser parse_info{this,string_list,inffull.Get()};
    Parser parse_info2{this,string_list,inffull.Get()};
    Parser parse_info3{this,string_list,inffull.Get()};
    //Log.print_con("%S%S\n",drpdir,inffilename);

    // Populate sections
    while(p<strend)
    {
        switch(*p)
        {
            case ' ':case '\t':case '\n':case '\r':
                p++;
                break;

            case ';':
                p++;
                while(p<strend&&*p!='\n'&&*p!='\r')p++;
                break;

            case '[':
                if(lnk_s2)
                    lnk_s2->blockend=p;
#ifdef DEBUG_EXTRACHECKS
                /*                  char *strings_base=inf_base+(*it).second.ofs;
                                    int strings_len=(*it).second.len-(*it).second.ofs;
                                    if(*(strings_base-1)!=']')
                                    log_file("B'%.*s'\n",1,strings_base-1);
                                    if(*(strings_base+strings_len)!='[')
                                    log_file("E'%.*s'\n",1,strings_base+strings_len);*/
#endif
                p++;
                p2=p;

                while(*p2!=']'&&p2<strend)
                {
#ifdef DEBUG_EXTRACHECKS
                    if(*p2=='\\')log_file("Err \\\n");else
                        if(*p2=='"')log_file("Err \"\n");else
                            if(*p2=='%')log_file("Err %\n");else
                                if(*p2==';')log_file("Err ;\n");
#endif
                    cur_inffile->infcrc+=*p2++;
                }
                sectnmend=p2;
                p2++;

                {
                    strtolower(p,sectnmend-p);
                    auto a=section_list.insert({std::string(p,sectnmend-p),sect_data_t(p2,inf_base+inf_len)});
                    lnk_s2=&a->second;
                    //Log.print_con("  %8d,%8d '%s' \n",strlink.ofs,strlink.len,std::string(p,sectnmend-p).c_str());
                }
                p=p2;
                break;

            default:
                //b=p;
                //while(*p!='\n'&&*p!='\r'&&*p!='['&&p<strend)p++;
                //if(*p=='['&&p<strend)log_file("ERROR in %S%S:\t\t\t'%.*s'(%d/%d)\n",drpdir,inffile,p-b+20,b,p,strend);
                while(p<strend&&*p!='\n'&&*p!='\r'/*&&*p!='['*/)cur_inffile->infcrc+=*p++;
        }
    }

    // Find [strings]
    auto range=section_list.equal_range("strings");
    if(range.first==range.second)Log.print_index("ERROR: missing [strings] in %S\n",inffull.Get());
    for(auto got=range.first;got!=range.second;++got)
    {
        sect_data_t *lnk=&got->second;
        const char *s1b,*s1e,*s2b,*s2e;

        parse_info.setRange(lnk);
        while(parse_info.parseItem())
        {
            parse_info.readStr(&s1b,&s1e);
            if(parse_info.parseField())
            {
                parse_info.readStr(&s2b,&s2e);
                strtolower(s1b,s1e-s1b);
                //Log.print_con("%s,%d,%d: tolower '%.10s'\n",line,s2b,s2e-s2b,s2b);
                string_list.insert({std::string(s1b,s1e-s1b),std::string(s2b,s2e-s2b)});
            }
        }
    }

    // Find [version]
    cur_ver=&cur_inffile->version;
    cur_ver->setInvalid();

    range=section_list.equal_range("version");
    if(range.first==range.second)Log.print_index("ERROR: missing [version] in %S\n",inffull.Get());
    //if(range.first==range.second)print_index("NOTE:  multiple [version] in %S\n",inffull.Get());
    for(auto got=range.first;got!=range.second;++got)
    {
        sect_data_t *lnk=&got->second;
        const char *s1b,*s1e;

        parse_info.setRange(lnk);
        while(parse_info.parseItem())
        {
            parse_info.readStr(&s1b,&s1e);
            //Log.print_con("%s,%d,%d: tolower '%.10s'\n",line,s1b,s1e,s1b);
            strtolower(s1b,s1e-s1b);

            int i;
            size_t sz=s1e-s1b;
            for(i=0;i<NUM_VER_NAMES;i++)
                if(table_version[i].sz==sz&&!memcmp(s1b,table_version[i].s,sz))
                {
                    if(i==DriverVer)
                    {
                        // date
                        if(parse_info.parseField())
                        {
                            i=parse_info.readDate(cur_ver);
                            /*if(i)log_index("ERROR: invalid date(%d.%d.%d)[%d] in %S\n",
                                 cur_ver->d,cur_ver->m,cur_ver->y,i,inffull.Get());*/
                        }

                        // version
                        if(parse_info.parseField())
                        {
                            parse_info.readVersion(cur_ver);
                        }

                    }
                    else
                    {
                        if(parse_info.parseField())
                        {
                            parse_info.readStr(&s1b,&s1e);
                            cur_inffile->fields[i]=static_cast<ofst>(text_ind.t_memcpyz(s1b,s1e-s1b));
                        }
                    }
                    break;
                }
            if(i==NUM_VER_NAMES)
            {
                //s1e=parse_info.se;
                //log_file("QQ '%.*s'\n",s1e-s1b,s1b);
            }
            while(parse_info.parseField());
        }
    }
    //if(cur_ver->y==-1) log_index("ERROR: missing date in %S\n",inffull.Get());
    //if(cur_ver->v1==-1)log_index("ERROR: missing build number in %S\n",inffull.Get());

    // Find [manufacturer] section
    range=section_list.equal_range("manufacturer");
    if(range.first==range.second)Log.print_index("ERROR: missing [manufacturer] in %S\n",inffull.Get());
    //if(lnk)log_index("NOTE:  multiple [manufacturer] in %S%S\n",drpdir,inffilename);
    for(auto got=range.first;got!=range.second;++got)
    {
        sect_data_t *lnk=&got->second;
        parse_info.setRange(lnk);
        while(parse_info.parseItem())
        {
            const char *s1b,*s1e;
            parse_info.readStr(&s1b,&s1e);

            cur_manuf_index=manufacturer_list.size();
            manufacturer_list.resize(cur_manuf_index+1);
            cur_manuf=&manufacturer_list[cur_manuf_index];
            cur_manuf->inffile_index=static_cast<ofst>(cur_inffile_index);
            cur_manuf->manufacturer=static_cast<ofst>(text_ind.t_memcpyz(s1b,s1e-s1b));
            cur_manuf->sections_n=0;

            if(parse_info.parseField())
            {
                parse_info.readStr(&s1b,&s1e);
                strtolower(s1b,s1e-s1b);
                strs[cur_manuf->sections_n++]=static_cast<ofst>(text_ind.t_memcpyz(s1b,s1e-s1b));
                while(1)
                {
                    if(cur_manuf->sections_n>1)
                        wsprintfA(secttry,"%s.%s",
                        text_ind.get(strs[0]),
                        text_ind.get(strs[cur_manuf->sections_n-1]));
                    else
                        wsprintfA(secttry,"%s",text_ind.get(strs[0]));

                    strtolower(secttry,strlen(secttry));

                    auto range2=section_list.equal_range(secttry);
                    if(range2.first==range2.second)Log.print_index("ERROR: missing [%s] in %S\n",secttry,inffull.Get());
                    for(auto got2=range2.first;got2!=range2.second;++got2)
                    {
                        sect_data_t *lnk2=&got2->second;
                        parse_info2.setRange(lnk2);
                        while(parse_info2.parseItem())
                        {
                            parse_info2.readStr(&s1b,&s1e);
                            size_t desc_c=text_ind.memcpyz_dup(s1b,s1e-s1b);

                            parse_info2.parseField();
                            parse_info2.readStr(&s1b,&s1e);
                            size_t inst_c=text_ind.memcpyz_dup(s1b,s1e-s1b);

                            //{ featurescore and install section
                            int feature_c=0xFF;
                            size_t install_picket_c;

                            char installsection[BUFLEN];
                            sect_data_t *lnk3;

                            memcpy(installsection,s1b,s1e-s1b);installsection[s1e-s1b]=0;
                            strcat(installsection,".nt");
                            strtolower(installsection,strlen(installsection));
                            auto range3=section_list.equal_range(installsection);
                            if(range3.first==range3.second)
                            {
                                memcpy(installsection,s1b,s1e-s1b);installsection[s1e-s1b]=0;
                                strtolower(installsection,strlen(installsection));
                                range3=section_list.equal_range(installsection);
                            }
                            if(range3.first==range3.second)
                            {
                                if(cur_manuf->sections_n>1)
                                {
                                    memcpy(installsection,s1b,s1e-s1b);installsection[s1e-s1b]=0;
                                    strcat(installsection,".");strcat(installsection,text_ind.get(strs[cur_manuf->sections_n-1]));
                                }
                                else
                                {
                                    memcpy(installsection,s1b,s1e-s1b);installsection[s1e-s1b]=0;
                                }

                                strtolower(installsection,strlen(installsection));
                                while(strlen(installsection)>=static_cast<size_t>(s1e-s1b))
                                {
                                    range3=section_list.equal_range(installsection);
                                    if(range3.first!=range3.second)break;
                                    //log_file("Tried '%s'\n",installsection);
                                    installsection[strlen(installsection)-1]=0;
                                }
                            }
                            char iii[BUFLEN];
                            *iii=0;
                            //int cnt=0;
                            if(range3.first==range3.second)
                            {
                                int i;
                                for(i=0;i<NUM_DECS;i++)
                                {
                                    //sprintf(installsection,"%.*s.%s",s1e-s1b,s1b,nts[i]);
                                    memcpy(installsection,s1b,s1e-s1b);installsection[s1e-s1b]=0;
                                    strcat(installsection,".");strcat(installsection,nts[i]);
                                    strtolower(installsection,strlen(installsection));
                                    auto range4=section_list.equal_range(installsection);
                                    if(range4.first!=range4.second)
                                    {
                                        //lnk3=tlnk;
                                        range3=range4;
                                        strcat(iii,installsection);
                                        strcat(iii,",");
                                    }
                                    //if(lnk3){log_file("Found '%s'\n",installsection);cnt++;}
                                }
                            }
                            //if(cnt>1)log_file("@num: %d\n",cnt);
                            //if(cnt>1&&!lnk3)log_file("ERROR in %S%S:\t\t\tMissing [%s]\n",drpdir,inffilename,iii);
                            if(range3.first!=range3.second)
                            {
                                if(*iii)wsprintfA(installsection,"$%s",iii);
                                install_picket_c=text_ind.memcpyz_dup(installsection,strlen(installsection));
                            }
                            else
                            {
                                install_picket_c=text_ind.memcpyz_dup("{missing}",9);
                            }

                            for(auto got3=range3.first;got3!=range3.second;++got3)
                            {
                                lnk3=&got3->second;
                                parse_info3.setRange(lnk3);
                                if(!strcmp(secttry,installsection))
                                {
                                    Log.print_index("ERROR: [%s] refers to itself in %S\n",installsection,inffull.Get());
                                    break;
                                }

                                while(parse_info3.parseItem())
                                {
                                    parse_info3.readStr(&s1b,&s1e);
                                    strtolower(s1b,s1e-s1b);
                                    size_t sz=s1e-s1b;
                                    if(sz==12&&!memcmp(s1b,"featurescore",sz))
                                    {
                                        parse_info3.parseField();
                                        feature_c=parse_info3.readHex();
                                    }
                                    while(parse_info3.parseField());
                                }
                            }
                            //} feature and install_picked section

                            cur_desc_index=desc_list.size();

                            desc_list.push_back(data_desc_t(cur_manuf_index,manufacturer_list[cur_manuf_index].sections_n-1,desc_c,inst_c,install_picket_c,feature_c));

                            int hwid_pos=0;
                            while(parse_info2.parseField())
                            {
                                parse_info2.readStr(&s1b,&s1e);
                                if(s1b>=s1e)continue;
                                strtoupper(s1b,s1e-s1b);

                                HWID_list.push_back(data_HWID_t(cur_desc_index,hwid_pos++,text_ind.memcpyz_dup(s1b,s1e-s1b)));
                            }
                        }
                    }

                    if(!parse_info.parseField())break;
                    parse_info.readStr(&s1b,&s1e);
                    if(s1b>s1e)break;
                    strtolower(s1b,s1e-s1b);
                    strs[cur_manuf->sections_n++]=static_cast<ofst>(text_ind.t_memcpyz(s1b,s1e-s1b));
                }
            }
            cur_manuf->sections=static_cast<ofst>(text_ind.t_memcpyz((char *)strs,sizeof(int)*cur_manuf->sections_n));
        }
    }
}

void Driverpack::getdrp_drvsectionAtPos(char *buf,size_t pos,size_t manuf_index)
{
    const int *rr=reinterpret_cast<const int *>(text_ind.get(manufacturer_list[manuf_index].sections));
    if(pos)
    {
        strcpy(buf,text_ind.get(rr[0]));
        strcat(buf,".");
        strcat(buf,text_ind.get(rr[pos]));
    }
    else
        strcpy(buf,text_ind.get(rr[pos]));
}

Driverpack::Driverpack(wchar_t const *driverpack_path,wchar_t const *driverpack_filename,Collection *col_v):
    type(DRIVERPACK_TYPE_PENDING_SAVE),
    col(col_v)
{
    drppath=static_cast<ofst>(text_ind.strcpyw(driverpack_path));
    drpfilename=static_cast<ofst>(text_ind.strcpyw(driverpack_filename));
    indexes.reset(0);
}

unsigned int __stdcall Driverpack::loaddrp_thread(void *arg)
{
    drplist_t *drplist=reinterpret_cast<drplist_t *>(arg);
    driverpack_task data;

    while(drplist->wait_and_pop(data),data.drp)
    {
        Driverpack *drp=data.drp;
        if(Settings.flags&COLLECTION_FORCE_REINDEXING||!drp->loadindex())
        {
            drp->objs_new=new concurrent_queue<inffile_task>;
            queuedriverpack_p->push(driverpack_task{drp});
            drp->genindex();
            drp->driverpack_indexinf_async(L"",L"",nullptr,0);
        }
    }
    return 0;
}

unsigned int __stdcall Driverpack::indexinf_thread(void *arg)
{
    drplist_t *drplist=reinterpret_cast<drplist_t *>(arg);
    inffile_task t;
    driverpack_task data;
    WStringShort bufw2;
    unsigned tm=0,last=0;

    while(drplist->wait_and_pop(data),data.drp)
    {
        if(!drp_count)drp_count=1;
        bufw2.sprintf(L"%s\\%s",data.drp->getPath(),data.drp->getFilename());
        manager_g->itembar_settext(SLOT_INDEXING,1,bufw2.Get(),drp_cur,drp_count,(drp_cur)*1000/drp_count);
        drp_cur++;

        while(1)
        {
            data.drp->objs_new->wait_and_pop(t);
            if(last)tm+=System.GetTickCountWr()-last;
            if(!t.adr)
            {
                t.drp->genhashes();
                t.drp->text_ind.shrink();
                last=System.GetTickCountWr();
                //Log.print_con("Trm %ws\n",data.drp->getFilename());
                delete data.drp->objs_new;
                break;
            }
            if(StrStrIW(t.inffile,L".inf"))
                t.drp->indexinf_ansi(t.pathinf,t.inffile,t.adr,t.len);
            else
                t.drp->parsecat(t.pathinf,t.inffile,t.adr,t.len);

            delete[] t.pathinf;
            delete[] t.inffile;
            delete[] t.adr;
            last=System.GetTickCountWr();
        }
        //Log.print_con("Fin %ws\n",data.drp->getFilename());
    }
    //Log.print_con("Starved for %ld\n",tm);
    return 0;
}

unsigned int __stdcall Driverpack::savedrp_thread(void *arg)
{
    drplist_t *drplist=reinterpret_cast<drplist_t *>(arg);
    driverpack_task data;
    WStringShort bufw2;

    while(drplist->wait_and_pop(data),data.drp)
    {
        bufw2.sprintf(L"%s\\%s",data.drp->getPath(),data.drp->getFilename());
        Log.print_con("Saving indexes for '%S'\n",bufw2.Get());
        if(Settings.flags&COLLECTION_USE_LZMA)
            manager_g->itembar_settext(SLOT_INDEXING,2,bufw2.Get(),cur_,count_);
        cur_++;
        data.drp->saveindex();
    }
    return 0;
}

int Driverpack::checkindex()
{
    if(*Settings.drpext_dir)return 0;

    wchar_t filename[BUFLEN];
    getindexfilename(col->getIndex_bin_dir(),L"bin",filename);
    FILE *f=_wfopen(filename,L"rb");
    if(!f)return 0;

    //_fseeki64(f,0,SEEK_END);
    //int sz=_ftelli64(f);
    //_fseeki64(f,0,SEEK_SET);

    char buf[3];
    int version;
    fread(buf,3,1,f);
    fread(&version,sizeof(int),1,f);
    //sz-=3+sizeof(int);
    fclose(f);

    if(memcmp(buf,"SDW",3)!=0||version!=VER_INDEX)if(version!=0x204)return 0;

    return 1;
}

int Driverpack::loadindex()
{
    wchar_t filename[BUFLEN];
    char buf[3];
    FILE *f;
    size_t sz;
    int version;
    char *mem,*p,*mem_unpack=nullptr;

    getindexfilename(col->getIndex_bin_dir(),L"bin",filename);
    f=_wfopen(filename,L"rb");
    if(!f)return 0;

    _fseeki64(f,0,SEEK_END);
    sz=static_cast<size_t>(_ftelli64(f));
    _fseeki64(f,0,SEEK_SET);

    fread(buf,3,1,f);
    fread(&version,sizeof(int),1,f);
    sz-=3+sizeof(int);

    if(memcmp(buf,"SDW",3)!=0||version!=VER_INDEX)if(version!=0x204)return 0;
    if(*Settings.drpext_dir)return 0;

    p=mem=new char[sz];
    fread(mem,sz,1,f);

    if(Settings.flags&COLLECTION_USE_LZMA)
    {
        UInt64 val;
        size_t sz_unpack;

        Lzma86_GetUnpackSize((Byte *)p,sz,&val);
        sz_unpack=(size_t)val;
        mem_unpack=new char[sz_unpack];
        decode(mem_unpack,sz_unpack,mem,sz);
        p=mem_unpack;
    }

    p=inffile.loaddata(p);
    p=manufacturer_list.loaddata(p);
    p=desc_list.loaddata(p);
    p=HWID_list.loaddata(p);
    p=text_ind.loaddata(p);
    p=indexes.loaddata(p);

    delete[] mem;
    delete[] mem_unpack;
    fclose(f);
    text_ind.shrink();

    type=StrStrIW(filename,L"\\_")?DRIVERPACK_TYPE_UPDATE:DRIVERPACK_TYPE_INDEXED;
    return 1;
}

void Driverpack::saveindex()
{
    wchar_t filename[BUFLEN];
    FILE *f;
    size_t sz;
    int version=VER_INDEX;
    char *mem,*p,*mem_pack;

    getindexfilename(col->getIndex_bin_dir(),L"bin",filename);
    if(!System.canWriteFile(filename,L"wb"))
    {
        Log.print_err("ERROR in driverpack_saveindex(): Write-protected,'%S'\n",filename);
        return;
    }
    f=_wfopen(filename,L"wb");

    sz=
        inffile.size()*sizeof(data_inffile_t)+
        manufacturer_list.size()*sizeof(data_manufacturer_t)+
        desc_list.size()*sizeof(data_desc_t)+
        HWID_list.size()*sizeof(data_HWID_t)+
        text_ind.getSize()+
        indexes.getSize()*sizeof(Hashitem)+sizeof(int)+
        6*sizeof(int)*2;

    p=mem=new char[sz];
    fwrite("SDW",3,1,f);
    fwrite(&version,sizeof(int),1,f);

    p=inffile.savedata(p);
    p=manufacturer_list.savedata(p);
    p=desc_list.savedata(p);
    p=HWID_list.savedata(p);
    p=text_ind.savedata(p);
    p=indexes.savedata(p);

    if(Settings.flags&COLLECTION_USE_LZMA)
    {
        mem_pack=new char[sz];
        sz=encode(mem_pack,sz,mem,sz);
        fwrite(mem_pack,sz,1,f);
        delete[] mem_pack;
    }
    else fwrite(mem,sz,1,f);

    delete[] mem;
    fclose(f);
    type=DRIVERPACK_TYPE_INDEXED;
}

void Driverpack::genhashes()
{
    // Driver signatures
    for(auto &it:inffile)
    {
        char filename[BUFLEN];
        strcpy(filename,text_ind.get(it.infpath));
        char *field=filename+strlen(filename);

        for(int j=CatalogFile;j<=CatalogFile_ntamd64;j++)if(it.fields[j])
        {
            strcpy(field,text_ind.get(it.fields[j]));
            strtolower(filename,strlen(filename));

            auto got=cat_list.find(filename);
            if(got!=cat_list.end())it.cats[j]=got->second;
        }
    }

    // Hashtable for fast search
    indexes.reset(HWID_list.size()/2);
    for(size_t i=0;i<HWID_list.size();i++)
    {
        const char *vv=text_ind.get(HWID_list[i].HWID);
        int val=indexes.gethashcode(vv,strlen(vv));
        indexes.additem(val,static_cast<int>(i));
    }
}

size_t Driverpack::printstats()
{
    size_t sum=0;

    Log.print_file("  %6d  %S\\%S\n",HWID_list.size(),getPath(),getFilename());
    sum+=HWID_list.size();
    return sum;
}

void Driverpack::print_index_hr()
{
    int pos;
    size_t inffile_index,manuf_index,HWID_index,desc_index;
    size_t n=inffile.size();
    Version *t;
    data_inffile_t *d_i;
    Hwidmatch hwidmatch(this,0);
    char buf[BUFLEN];
    wchar_t filename[BUFLEN];
    FILE *f;
    int cnts[NUM_DECS],plain;
    size_t HWID_index_last=0;
    size_t manuf_index_last=0;
    WStringShort date;
    WStringShort vers;
    int i;

    getindexfilename(col->getIndex_linear_dir(),L"txt",filename);
    f=_wfopen(filename,L"wt");

    Log.print_con("Saving %s\n",filename);
    fwprintf(f,L"%s\\%s (%d inf files)\n",getPath(),getFilename(),static_cast<int>(n));
    for(inffile_index=0;inffile_index<n;inffile_index++)
    {
        d_i=&inffile[inffile_index];
        fprintf(f,"  %s%s (%d bytes)\n",text_ind.get(d_i->infpath),text_ind.get(d_i->inffilename),d_i->infsize);
        for(i=0;i<(int)n;i++)if(i!=(int)inffile_index&&d_i->infcrc==inffile[i].infcrc)
            fprintf(f,"**%s%s\n",text_ind.get(inffile[i].infpath),text_ind.get(inffile[i].inffilename));

        t=&d_i->version;
        t->str_date(date,true);
        t->str_version(vers);
        fwprintf(f,L"    date\t\t\t%S\n",date.Get());
        fwprintf(f,L"    version\t\t\t%S\n",vers.Get());
        for(i=0;i<NUM_VER_NAMES;i++)
            if(d_i->fields[i])
            {
                fprintf(f,"    %-28s%s\n",table_version[i].s,text_ind.get(d_i->fields[i]));
                if(d_i->cats[i])fprintf(f,"      %s\n",text_ind.get(d_i->cats[i]));

            }

        memset(cnts,-1,sizeof(cnts));plain=0;
        for(manuf_index=manuf_index_last;manuf_index<manufacturer_list.size();manuf_index++)
            if(manufacturer_list[manuf_index].inffile_index==inffile_index)
            {
                manuf_index_last=manuf_index;
                //hwidmatch.HWID_index=HWID_index_last;
                if(manufacturer_list[manuf_index].manufacturer)
                    fprintf(f,"      {%s}\n",text_ind.get(manufacturer_list[manuf_index].manufacturer));
                for(pos=0;pos<manufacturer_list[manuf_index].sections_n;pos++)
                {
                    getdrp_drvsectionAtPos(buf,pos,manuf_index);
                    i=calc_secttype(buf);
                    if(i>=0&&cnts[i]<0)cnts[i]=0;
                    if(i<0&&pos>0)fprintf(f,"!!![%s]\n",buf);
                    fprintf(f,"        [%s]\n",buf);

                    for(desc_index=0;desc_index<desc_list.size();desc_index++)
                        if(desc_list[desc_index].manufacturer_index==manuf_index&&
                           desc_list[desc_index].sect_pos==pos)
                        {
                            for(HWID_index=HWID_index_last;HWID_index<HWID_list.size();HWID_index++)
                                if(HWID_list[HWID_index].desc_index==desc_index)
                                {
                                    if(HWID_index_last+1!=HWID_index&&HWID_index)fprintf(f,"Skip:%u,%u\n",static_cast<unsigned>(HWID_index_last),static_cast<unsigned>(HWID_index));
                                    HWID_index_last=HWID_index;
                                    hwidmatch.setHWID_index(HWID_index_last);

                                    //if(text+manufacturer_list[manuf_index].manufacturer!=get_manufacturer(&hwidmatch))
                                    //fprintf(f,"*%s\n",get_manufacturer(&hwidmatch));
                                    //get_section(&hwidmatch,buf+500);
                                    //fprintf(f,"*%s,%s\n",buf+1000,buf+500);
                                    if(i>=0)cnts[i]++;
                                    if(pos==0&&i<0)plain++;

                                    if(hwidmatch.getdrp_drvinfpos())
                                        wsprintfA(buf,"%-2d",hwidmatch.getdrp_drvinfpos());
                                    else
                                        wsprintfA(buf,"  ");

                                    fprintf(f,"       %s %-50s%-20s\t%s\n",buf,
                                            hwidmatch.getdrp_drvHWID(),
                                            hwidmatch.getdrp_drvinstall(),
                                            hwidmatch.getdrp_drvdesc());
                                    fprintf(f,"          feature:%-42hX%-20s\n\n",
                                            hwidmatch.getdrp_drvfeature()&0xFF,
                                            hwidmatch.getdrp_drvinstallPicked());
                                }
                                else if(HWID_index!=HWID_index_last)break;
                        }
                }
            }
            else if(manuf_index!=manuf_index_last)break;

        fprintf(f,"  Decors:\n");
        fprintf(f,"    %-15s%d\n","plain",plain);
        for(i=0;i<NUM_DECS;i++)
        {
            if(cnts[i]>=0)fprintf(f,"    %-15s%d\n",nts[i],cnts[i]);
        }
        fprintf(f,"\n");
    }
    fprintf(f,"  HWIDS:%u\n",(unsigned)HWID_list.size());
    fclose(f);
}

void Driverpack::fillinfo(const char *sect,const char *hwid,unsigned start_index,int *inf_pos,ofst *cat,int *catalogfile,int *feature)
{
    *inf_pos=-1;
    //log_file("Search[%s,%s,%d]\n",sect,hwid,start_index);
    for(unsigned HWID_index=start_index;HWID_index<HWID_list.size();HWID_index++)
    {
        if(!_strcmpi(text_ind.get(HWID_list[HWID_index].getHWID()),hwid))
        {
            Hwidmatch hwidmatch(this,HWID_index);
            if(!_strcmpi(hwidmatch.getdrp_drvinstallPicked(),sect)||
               StrStrIA(hwidmatch.getdrp_drvinstall(),sect))
            {
                if(*inf_pos<0||*inf_pos>hwidmatch.getdrp_drvinfpos())
                {
                    *feature=hwidmatch.getdrp_drvfeature();
                    *catalogfile=hwidmatch.calc_catalogfile();
                    *inf_pos=hwidmatch.getdrp_drvinfpos();
                }
                //log_file("Sect %s, %d, %d, %d (%d),%s\n",sect,*catalogfile,*feature,*inf_pos,HWID_index,hwidmatch.getdrp_drvinstallPicked());
            }
        }
    }
    if(*inf_pos==-1)
    {
        *inf_pos=0;
        *cat=0;
        *feature=0xFF;
        Log.print_err("ERROR: sect not found '%s'\n",sect);
    }
}

void Driverpack::getindexfilename(const wchar_t *dir,const wchar_t *ext,wchar_t *indfile)
{
    wchar_t *p;
    wchar_t buf[BUFLEN];
    wsprintf(buf,L"%s",getFilename());

    // sanity checks
    wsprintf(indfile,L"");
    size_t len=wcslen(getFilename());
    if(len>FILENAME_MAX)
        return;
    size_t plen=wcslen(getPath());
    if(plen>MAX_PATH)
        return;

    if(*(getPath()))
        wsprintf(buf+(len-3)*1,L"%s.%s",getPath()+wcslen(col->getDriverpack_dir()),ext);
    else
        wsprintf(buf+(len-3)*1,L".%s",ext);

    // replace all occurrences of '\\' and ' ' with '_'
    p=buf;
    while(*p){if(*p==L'\\'||*p==L' ')*p=L'_';p++;}
    wsprintf(indfile,L"%s\\%s",dir,buf);
}

void Driverpack::parsecat(wchar_t const *pathinf,wchar_t const *inffilename,const char *adr,size_t len)
{
    char bufa[BUFLEN];

    findosattr(bufa,adr,len);
    if(*bufa)
    {
        char filename[BUFLEN];
        wsprintfA(filename,"%ws%ws",pathinf,inffilename);
        strtolower(filename,strlen(filename));
        cat_list.insert({filename,static_cast<ofst>(text_ind.memcpyz_dup(bufa,strlen(bufa)))});
        //Log.print_con("(%s)\n##%s\n",filename,bufa);
    }
    else
    {
        Log.print_con("Not found signature in '%ws%ws'(%d)\n",pathinf,inffilename,len);
    }

}

void Driverpack::indexinf(wchar_t const *drpdir,wchar_t const *iinfdilename,const char *inf_base,size_t inf_len)
{
    if(inf_len>4&&((inf_base[0]==-1&&inf_base[3]==0)||inf_base[0]==0))
    {
        size_t size=inf_len;

        char *buf_out=new char[size+2];
        size=unicode2ansi((const unsigned char *)inf_base,buf_out,size);
        indexinf_ansi(drpdir,iinfdilename,buf_out,size);
        delete[] buf_out;
    }
    else
    {
        indexinf_ansi(drpdir,iinfdilename,inf_base,inf_len);
    }
}
//}
