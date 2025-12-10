#include "SDI.h"
#include "version.h"

#include "manager.h"
#include "Settings.h"
#include "theme.h"

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
