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
#include "matcher.h"
#include "indexing.h"
#include "manager.h"
#include "update.h"
#include "install.h"
#include "gui.h"
#include "draw.h"
#include "theme.h"

#include <windows.h>
#include <process.h>

// Depend on Win32API
#include "enum.h"
#include "main.h"

//{ Global vars
const status_t statustnl[NUM_STATUS]=
{
    {FILTER_SHOW_CURRENT,   STATUS_CURRENT},
    {FILTER_SHOW_NEWER,     STATUS_NEW},
    {FILTER_SHOW_OLD,       STATUS_OLD},
    {FILTER_SHOW_WORSE_RANK,STATUS_WORSE},
    {FILTER_SHOW_BETTER,    STATUS_BETTER},
    {FILTER_SHOW_MISSING,   STATUS_MISSING},
};
//}

//{ Itembar
itembar_t::itembar_t(Devicematch *devicematch1,Hwidmatch *hwidmatch1,size_t groupindex,size_t rm1,int first1)
{
    memset(this,0,sizeof(itembar_t));
    devicematch=devicematch1;
    hwidmatch=hwidmatch1;
    curpos=(-D_X(DRVITEM_DIST_Y0))<<16;
    tagpos=(-D_X(DRVITEM_DIST_Y0))<<16;
    index=groupindex;
    rm=rm1;
    first=first1;
}

void itembar_t::itembar_setpos(int *pos,int *cnt,bool addspace)
{
    if(isactive)
    {
        *pos+=*cnt?D_X(DRVITEM_DIST_Y1):D_X(DRVITEM_DIST_Y0);
        if(addspace)*pos+=D_X(DRVITEM_DIST_Y2);
        (*cnt)--;
    }
    oldpos=curpos;
    tagpos=*pos<<16;
    accel=(tagpos-curpos)/(1000/2);
    if(accel==0)accel=(tagpos<curpos)?500:-500;
}

void itembar_t::str_status(wchar_t *buf)
{
    buf[0]=0;

    if(hwidmatch)
    {
        int status=hwidmatch->getStatus();
        if(status&STATUS_INVALID)
            wcscat(buf,STR(STR_STATUS_INVALID));
        else
        {
            if(status&STATUS_MISSING)
                wsprintf(buf,L"%s",STR(STR_STATUS_MISSING));
            else
            {
                if(status&STATUS_BETTER&&status&STATUS_NEW)        wcscat(buf,STR(STR_STATUS_BETTER_NEW));
                if(status&STATUS_SAME  &&status&STATUS_NEW)        wcscat(buf,STR(STR_STATUS_SAME_NEW));
                if(status&STATUS_WORSE &&status&STATUS_NEW)        wcscat(buf,STR(STR_STATUS_WORSE_NEW));

                if(status&STATUS_BETTER&&status&STATUS_CURRENT)    wcscat(buf,STR(STR_STATUS_BETTER_CUR));
                if(status&STATUS_SAME  &&status&STATUS_CURRENT)    wcscat(buf,STR(STR_STATUS_SAME_CUR));
                if(status&STATUS_WORSE &&status&STATUS_CURRENT)    wcscat(buf,STR(STR_STATUS_WORSE_CUR));

                if(status&STATUS_BETTER&&status&STATUS_OLD)        wcscat(buf,STR(STR_STATUS_BETTER_OLD));
                if(status&STATUS_SAME  &&status&STATUS_OLD)        wcscat(buf,STR(STR_STATUS_SAME_OLD));
                if(status&STATUS_WORSE &&status&STATUS_OLD)        wcscat(buf,STR(STR_STATUS_WORSE_OLD));
            }
        }
        if(status&STATUS_DUP)wcscat(buf,STR(STR_STATUS_DUP));
        if(hwidmatch->getAltsectscore()<2)
        {
            wcscat(buf,STR(STR_STATUS_NOTSIGNED));
        }
        if(hwidmatch->getdrp_packontorrent())wcscat(buf,STR(STR_UPD_WEBSTATUS));
    }
    else
    //if(devicematch)
    {
        if(devicematch->getStatus()&STATUS_NF_STANDARD)wcscat(buf,STR(STR_STATUS_NF_STANDARD));
        if(devicematch->getStatus()&STATUS_NF_UNKNOWN) wcscat(buf,STR(STR_STATUS_NF_UNKNOWN));
        if(devicematch->getStatus()&STATUS_NF_MISSING) wcscat(buf,STR(STR_STATUS_NF_MISSING));
    }
}

void itembar_t::drawbutton(Canvas &canvas,int x,int pos,const wchar_t *str1,const wchar_t *str2)
{
    if(Settings.flags&FLAG_SCRIPTMODE)return;
    pos+=D_X(ITEM_TEXT_OFS_Y);
    canvas.SetTextColor(D_C(boxindex[box_status()]+14));
    canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos,str1);
    canvas.SetTextColor(D_C(boxindex[box_status()]+15));
    canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y),str2);
}

static int showpercent(int a)
{
    switch(a)
    {
        case STR_INST_EXTRACT:
        case STR_INST_INSTALL:
        case STR_INST_INSTALLING:
        case STR_EXTR_EXTRACTING:
        case STR_REST_CREATING:
            return 1;

        default:
            return 0;
    }
}

void itembar_t::updatecur()
{
    if(itembar_act==SLOT_RESTORE_POINT)return;
    if(showpercent(install_status)&&ar_total)
        percent=(ar_proceed*(instflag&INSTALLDRIVERS&&checked?900:1000))/ar_total;
    else
        percent=0;
}

int itembar_t::box_status()
{
    switch(index)
    {
        case SLOT_VIRUS_AUTORUN:
        case SLOT_VIRUS_RECYCLER:
        case SLOT_VIRUS_HIDDEN:
            return BOX_DRVITEM_VI;

        case SLOT_NODRIVERS:
        case SLOT_DPRDIR:
        case SLOT_SNAPSHOT:
            return BOX_DRVITEM_IF;

        case SLOT_DOWNLOAD:
        case SLOT_NOUPDATES:
            return BOX_NOUPDATES;

        case SLOT_BOOSTY:
            return BOX_BOOSTY;

        //case SLOT_TRANSLATION:
        //    return BOX_TRANSLATION;

        case SLOT_RESTORE_POINT:
            switch(install_status)
            {
                case STR_REST_CREATING:
                    return BOX_DRVITEM_D0;

                case STR_REST_CREATED:
                    return BOX_DRVITEM_D1;

                case STR_REST_FAILED:
                case STR_RESTOREPOINTS_DISABLED:
                    return BOX_DRVITEM_DE;

                default:
                    break;
            }
            break;

        case SLOT_EXTRACTING:
            switch(install_status)
            {
                case STR_EXTR_EXTRACTING:
                case STR_INST_INSTALLING:
                    return BOX_DRVITEM_D0;

                case STR_INST_COMPLITED:
                    return BOX_DRVITEM_D1;

                case STR_INST_COMPLITED_RB:
                    return BOX_DRVITEM_D2;

                case STR_INST_STOPPING:
                    return BOX_DRVITEM_DE;

                default:break;
            }
            break;

        default:
            break;
    }
    if(hwidmatch)
    {
        int status=hwidmatch->getStatus();

        if(first&2)return BOX_DRVITEM_PN;

        if(status&STATUS_INVALID)
            return BOX_DRVITEM_IN;
        else
        {
            switch(install_status)
            {
                case STR_INST_EXTRACT:
                case STR_INST_INSTALL:
                    return BOX_DRVITEM_D0;

                case STR_INST_OK:
                case STR_EXTR_OK:
                    return BOX_DRVITEM_D1;

                case STR_INST_REBOOT:
                    return BOX_DRVITEM_D2;

                case STR_INST_STOPPING:
                case STR_INST_FAILED:
                case STR_EXTR_FAILED:
                    return BOX_DRVITEM_DE;

                default:break;
            }
            if(status&STATUS_MISSING)
                return BOX_DRVITEM_MS;
            else
            {
                if(hwidmatch->getAltsectscore()<2)return BOX_DRVITEM_WO;

                if(status&STATUS_BETTER&&status&STATUS_NEW)        return BOX_DRVITEM_BN;
                if(status&STATUS_SAME  &&status&STATUS_NEW)        return BOX_DRVITEM_SN;
                if(status&STATUS_WORSE &&status&STATUS_NEW)        return BOX_DRVITEM_WN;

                if(status&STATUS_BETTER&&status&STATUS_CURRENT)    return BOX_DRVITEM_BC;
                if(status&STATUS_SAME  &&status&STATUS_CURRENT)    return BOX_DRVITEM_SC;
                if(status&STATUS_WORSE &&status&STATUS_CURRENT)    return BOX_DRVITEM_WC;

                if(status&STATUS_BETTER&&status&STATUS_OLD)        return BOX_DRVITEM_BO;
                if(status&STATUS_SAME  &&status&STATUS_OLD)        return BOX_DRVITEM_SO;
                if(status&STATUS_WORSE &&status&STATUS_OLD)        return BOX_DRVITEM_WO;
            }
        }
    }
    else
    if(devicematch)
    {
        if(devicematch->getStatus()&STATUS_NF_STANDARD)  return BOX_DRVITEM_NS;
        if(devicematch->getStatus()&STATUS_NF_UNKNOWN)   return BOX_DRVITEM_NU;
        if(devicematch->getStatus()&STATUS_NF_MISSING)   return BOX_DRVITEM_NM;
    }
        //if(status&STATUS_DUP)wcscat(buf,STR(STR_STATUS_DUP));
    return BOX_DRVITEM;
}

void itembar_t::contextmenu(int x,int y)
{
    HMENU hPopupMenu=CreatePopupMenu();

    int flags1=checked?MF_CHECKED:0;
    if(!hwidmatch&&index!=SLOT_RESTORE_POINT)flags1|=MF_GRAYED;
    if(rtl)x=MainWindow.mainx_c-x;

    if(Popup->floating_itembar==SLOT_RESTORE_POINT)
    {
        InsertMenu(hPopupMenu,0,MF_BYPOSITION|MF_STRING|flags1,ID_SCHEDULE, STR(STR_REST_SCHEDULE));
        InsertMenu(hPopupMenu,1,MF_BYPOSITION|MF_STRING,       ID_SHOWALT,  STR(STR_REST_ROLLBACK));

        RECT rect;
        SetForegroundWindow(MainWindow.hMain);
        GetWindowRect(MainWindow.hField,&rect);
        TrackPopupMenu(hPopupMenu,TPM_LEFTALIGN,rect.left+x,rect.top+y,0,MainWindow.hMain,nullptr);
        return;
    }
    if(Popup->floating_itembar<RES_SLOTS)return;

    const Driver *cur_driver=nullptr;

    const Txt *txt=&manager_g->matcher->getState()->textas;
    if(devicematch->driver)cur_driver=devicematch->driver;
    int flags2=isactive&2?MF_CHECKED:0;
    int flags3=cur_driver?0:MF_GRAYED;
    if(manager_g->groupsize(index)<2)flags2|=MF_GRAYED;
    wchar_t buf[512];

    int i=0;
    HMENU hSub1=CreatePopupMenu();
    HMENU hSub2=CreatePopupMenu();
    if(devicematch->device->getHardwareID())
    {
        wchar_t *p=txt->getwV(devicematch->device->getHardwareID());
        while(*p)
        {
            escapeAmp(buf,p);
            InsertMenu(hSub1,i,MF_BYPOSITION|MF_STRING,ID_HWID_WEB+i,buf);
            InsertMenu(hSub2,i,MF_BYPOSITION|MF_STRING,ID_HWID_CLIP+i,buf);
            p+=wcslen(p)+1;
            i++;
        }
    }
    if(devicematch->device->getCompatibleIDs())
    {
        wchar_t *p=txt->getwV(devicematch->device->getCompatibleIDs());
        while(*p)
        {
            escapeAmp(buf,p);
            InsertMenu(hSub1,i,MF_BYPOSITION|MF_STRING,ID_HWID_WEB+i,buf);
            InsertMenu(hSub2,i,MF_BYPOSITION|MF_STRING,ID_HWID_CLIP+i,buf);
            p+=wcslen(p)+1;
            i++;
        }
    }
    int flagssubmenu=i?0:MF_GRAYED;

    i=0;
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|flags1,ID_SCHEDULE, STR(STR_CONT_INSTALL));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|flags2,ID_SHOWALT,  STR(STR_CONT_SHOWALT));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_SEPARATOR,0,nullptr);
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|MF_POPUP|flagssubmenu,(UINT_PTR)hSub1,STR(STR_CONT_HWID_SEARCH));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|MF_POPUP|flagssubmenu,(UINT_PTR)hSub2,STR(STR_CONT_HWID_CLIP));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_SEPARATOR,0,nullptr);
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|flags3,ID_OPENINF,  STR(STR_CONT_OPENINF));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|flags3,ID_LOCATEINF,STR(STR_CONT_LOCATEINF));

    RECT rect;
    SetForegroundWindow(MainWindow.hMain);
    GetWindowRect(MainWindow.hField,&rect);
    TrackPopupMenu(hPopupMenu,TPM_LEFTALIGN,rect.left+x,rect.top+y,0,MainWindow.hMain,nullptr);
}

void itembar_t::popup_drivercmp(Manager *manager,Canvas &canvas,int wx,int wy,size_t index1)
{
    if(index1<RES_SLOTS)return;

    //itembar_t *itembar=&manager->items_list[index];
    Devicematch *devicematch_f=devicematch;
    Hwidmatch *hwidmatch_f=hwidmatch;
    State *state=manager->matcher->getState();

    wchar_t bufw[BUFLEN];
    wchar_t i_hwid[BUFLEN];
    wchar_t a_hwid[BUFLEN];

    const Txt *txt=&state->textas;
    int maxln=0;
    int bolder=wx/2;
    const wchar_t *p;
    const Driver *cur_driver=nullptr;
    textdata_vert td(canvas);
    Version *a_v=nullptr;
    unsigned score=0;
    *i_hwid=*a_hwid=0;
    int cm_ver=0,cm_date=0,cm_score=0,cm_hwid=0;
    int c0=D_C(POPUP_TEXT_COLOR),cb=D_C(POPUP_CMP_BETTER_COLOR);
    int p0=D_X(POPUP_OFSX);

    if(devicematch_f->driver)
    {
        int i;
        cur_driver=devicematch_f->driver;
        wsprintf(bufw,L"%s",txt->getw(cur_driver->MatchingDeviceId));
        for(i=0;bufw[i];i++)
            i_hwid[i]=(char)toupper(bufw[i]);
        i_hwid[i]=0;
    }
    if(hwidmatch_f)
    {
        a_v=hwidmatch_f->getdrp_drvversion();
        wsprintf(a_hwid,L"%S",hwidmatch_f->getdrp_drvHWID());
    }
    if(cur_driver&&hwidmatch_f)
    {
        int r=cmpdate(&cur_driver->version,a_v);
        if(r>0)cm_date=1;
        if(r<0)cm_date=2;

        score=cur_driver->calc_score_h(state);
        if(score<hwidmatch_f->getScore())cm_score=1;
        if(score>hwidmatch_f->getScore())cm_score=2;

        r=cmpversion(&cur_driver->version,a_v);
        if(r>0)cm_ver=1;
        if(r<0)cm_ver=2;
    }

    // Device info (hwidmatch_f,devicematch_f)
    td.ret();
    td.TextOutBold(L"%s",STR(STR_HINT_ANALYSIS));
    td.ret_ofs(10);
    td.TextOutF(c0,L"$%04d",index1);
    if(hwidmatch_f)
    {
        td.ret();
        td.TextOutBold(L"%s",STR(STR_HINT_DRP));
        td.ret_ofs(10);
        td.TextOutF(c0,L"%s\\%s",hwidmatch_f->getdrp_packpath(),hwidmatch_f->getdrp_packname());
        td.TextOutF(hwidmatch_f->calc_notebook()?c0:D_C(POPUP_CMP_INVALID_COLOR)
                 ,L"%S%S",hwidmatch_f->getdrp_infpath(),hwidmatch_f->getdrp_infname());
    }

    bufw[0]=0;
    devicematch_f->device->getClassDesc(bufw);

    td.ret();
    td.TextOutBold(L"%s",STR(STR_HINT_DEVICE));
    td.ret_ofs(10);
    td.TextOutF(c0,L"%s",txt->get(devicematch_f->device->getDescr()));
    td.TextOutF(c0,L"%s%s",STR(STR_HINT_MANUF),txt->get(devicematch_f->device->Mfg));
    if(bufw[0])td.TextOutF(c0,L"%s",bufw);
    td.TextOutF(c0,L"%s",txt->get(devicematch_f->device->Driver));
    wsprintf(bufw,STR(STR_STATUS_NOTPRESENT+devicematch_f->device->print_status()),devicematch_f->device->problem);
    td.TextOutF(c0,L"%s",bufw);

    // HWID list (devicematch_f)
    maxln=td.y;
    td.y=D_X(POPUP_OFSY);
    if(devicematch_f->device->HardwareID)
    {
        td.ret_ofs(bolder);
        td.TextOutBold(L"%s",STR(STR_HINT_HARDWAREID));
        td.ret_ofs(bolder+10);
        p=txt->getw(devicematch_f->device->HardwareID);
        while(*p)
        {
            int pp=0;
            if(!_wcsicmp(i_hwid,p))pp|=1;
            if(!_wcsicmp(a_hwid,p))pp|=2;
            if(!cm_hwid&&(pp==1||pp==2))cm_hwid=pp;
            if(rtl)
                td.TextOutF_RTL(pp?D_C(POPUP_HWID_COLOR):c0,bolder,L"%s",p);
            else
                td.TextOutF(pp?D_C(POPUP_HWID_COLOR):c0,L"%s",p);
            p+=wcslen(p)+1;
        }
    }
    if(devicematch_f->device->CompatibleIDs)
    {
        td.ret_ofs(bolder);
        td.TextOutBold(L"%s",STR(STR_HINT_COMPID));
        td.ret_ofs(bolder+10);
        p=txt->getw(devicematch_f->device->CompatibleIDs);
        while(*p)
        {
            int pp=0;
            if(!_wcsicmp(i_hwid,p))pp|=1;
            if(!_wcsicmp(a_hwid,p))pp|=2;
            if(!cm_hwid&&(pp==1||pp==2))cm_hwid=pp;
            if(rtl)
                td.TextOutF_RTL(pp?D_C(POPUP_HWID_COLOR):c0,bolder,L"%s",p);
            else
                td.TextOutF(pp?D_C(POPUP_HWID_COLOR):c0,L"%s",p);
            p+=wcslen(p)+1;
        }
    }
    if(!cur_driver||!hwidmatch_f)cm_hwid=0;
    if(td.y>maxln)maxln=td.y;
    maxln+=D_X(POPUP_WY);
    td.y=maxln;

    // Cur driver (cur_driver)
    if(cur_driver||hwidmatch_f)
    {
        canvas.DrawLine(0,td.y-D_X(POPUP_WY)/2,wx,td.y-D_X(POPUP_WY)/2);
    }
    if(devicematch_f->device->HardwareID||hwidmatch_f)
    {
        canvas.DrawLine(bolder,0,bolder,wy);
    }
    td.ret();
    td.TextOutBold(L"%s",STR(STR_HINT_INSTDRV));
    td.ret_ofs(10);
    if(cur_driver)
    {
        WStringShort date;
        WStringShort vers;

        cur_driver->version.str_date(date);
        cur_driver->version.str_version(vers);

        td.TextOutF(               c0,L"%s",txt->get(cur_driver->DriverDesc));
        td.TextOutF(cur_driver->isvalidcat(state)?c0:D_C(POPUP_CMP_INVALID_COLOR),L"%s%S",STR(STR_HINT_SIGNATURE),txt->get(cur_driver->cat));
        td.TextOutF(               c0,L"%s%s",STR(STR_HINT_PROVIDER),txt->get(cur_driver->ProviderName));
        td.TextOutF(cm_date ==1?cb:c0,L"%s%s",STR(STR_HINT_DATE),date.Get());
        td.TextOutF(cm_ver  ==1?cb:c0,L"%s%s",STR(STR_HINT_VERSION),vers.Get());
        td.TextOutF(cm_hwid ==1?cb:c0,L"%s%s%s",STR(STR_HINT_ID),rtl?L"\u200F":L"",i_hwid);
        td.TextOutF(               c0,L"%s%s%s",STR(STR_HINT_INF),rtl?L"\u200F":L"",txt->get(cur_driver->InfPath));
        td.TextOutF(               c0,L"%s%s%s",STR(STR_HINT_SECTION),txt->get(cur_driver->InfSection),txt->get(cur_driver->InfSectionExt));
        td.TextOutF(cm_score==1?cb:c0,L"%s%08X",STR(STR_HINT_SCORE),score);
    }
    else
    {
        td.TextOutF(L"%s",STR(STR_SHOW_MISSING));
    }

    // Available driver (hwidmatch_f)
    if(hwidmatch_f)
    {
        WStringShort date;
        WStringShort vers;

        a_v->str_date(date);
        a_v->str_version(vers);
        hwidmatch_f->getdrp_drvsection((CHAR *)(bufw+500));

        td.y=maxln;
        td.ret_ofs(bolder);
        td.TextOutBold(L"%s",STR(STR_HINT_AVAILDRV));
        td.ret_ofs(bolder+10);
        wsprintf(bufw+1000,L"%S",hwidmatch_f->getdrp_drvdesc());
        td.TextOutF(               c0,L"%s",bufw+1000);
        td.TextOutF(hwidmatch_f->isvalidcat(state)?c0:D_C(POPUP_CMP_INVALID_COLOR),L"%s%S",STR(STR_HINT_SIGNATURE),hwidmatch_f->getdrp_drvcat(hwidmatch_f->pickcat(state)));
        td.TextOutF(               c0,L"%s%S",STR(STR_HINT_PROVIDER),hwidmatch_f->getdrp_drvmanufacturer());
        td.TextOutF(cm_date ==2?cb:c0,L"%s%s",STR(STR_HINT_DATE),date.Get());
        td.TextOutF(cm_ver  ==2?cb:c0,L"%s%s",STR(STR_HINT_VERSION),vers.Get());
        td.TextOutF(cm_hwid ==2?cb:c0,L"%s%s%S",STR(STR_HINT_ID),rtl?L"\u200F":L"",hwidmatch_f->getdrp_drvHWID());
        td.TextOutF(               c0,L"%s%s%S%S",STR(STR_HINT_INF),rtl?L"\u200F":L"",hwidmatch_f->getdrp_infpath(),hwidmatch_f->getdrp_infname());
        td.TextOutF(hwidmatch_f->getDecorscore()?c0:D_C(POPUP_CMP_INVALID_COLOR),L"%s%S",STR(STR_HINT_SECTION),bufw+500);
        td.TextOutF(cm_score==2?cb:c0,L"%s%08X",STR(STR_HINT_SCORE),hwidmatch_f->getScore());
    }

    int zz=td.getMaxsz();
    if(!devicematch_f->device->HardwareID&&!hwidmatch_f)zz/=2;
    Popup->popup_resize((zz+10+p0*2)*2,td.y+D_X(POPUP_OFSY));
}

int itembar_cmp(const itembar_t *a,const itembar_t *b,const Txt *ta,const Txt *tb)
{
    if(a->hwidmatch&&b->hwidmatch)
    {
        if(a->hwidmatch->getHWID_index()==b->hwidmatch->getHWID_index())return 3;
        return 0;
    }
    if(*ta->getw(a->devicematch->device->getDriver()))
    {
        if(!wcscmp(ta->getw(a->devicematch->device->getDriver()),tb->getw(b->devicematch->device->getDriver())))return 2;
    }
    else
    {
        if(*ta->getw(a->devicematch->device->getDescr()))
        {
            if(!wcscmp(ta->getw(a->devicematch->device->getDescr()),tb->getw(b->devicematch->device->getDescr())))return 1;
        }
    }

    return 0;
}
//}

//{ Manager
void Manager::init(Matcher *matchera)
{
    matcher=matchera;
    items_list.clear();

    for(int i=0;i<RES_SLOTS;i++)
        items_list.push_back(itembar_t(nullptr,nullptr,i,0,1));
}

int  Manager::manager_drplive(const wchar_t *s)
{
    itembar_t *itembar;
    size_t k;

    itembar=&items_list[RES_SLOTS];
    for(k=RES_SLOTS;k<items_list.size();k++,itembar++)
    if(itembar->hwidmatch&&StrStrIW(itembar->hwidmatch->getdrp_packname(),s))
    {
        if(itembar->isactive)
        {
            if(itembar->hwidmatch->getdrp_packontorrent())return 0;// Yes
        }
    }
    return 1;
}

bool Manager::isSelected(const wchar_t *s)
{
    itembar_t *itembar;
    bool ret=false;
    itembar=&items_list[RES_SLOTS];
    for(size_t k=RES_SLOTS;k<items_list.size();k++,itembar++)
    {
        if(itembar->checked && itembar->hwidmatch)
        {
            std::wstring drp=itembar->hwidmatch->getdrp_packname();
            if(StrStrIW(drp.c_str(),s))
            {
                ret=true;
                //if(ret)Log.print_debug("%S is selected.\n", s);
                break;
            }
        }
    }
    return ret;
}

void Manager::populate()
{
    size_t remap[1024*8];
    matcher->sorta(remap);

    items_list.resize(RES_SLOTS);
    /*items_list.clear();

    for(int i=0;i<RES_SLOTS;i++)
        items_list.push_back(itembar_t(nullptr,nullptr,i,0,1));*/

    for(size_t i=0;i<matcher->getDwidmatch_list();i++)
    {
        Devicematch *devicematch=matcher->getDevicematch_i(remap[i]);
        Hwidmatch *hwidmatch=matcher->getHwidmatch_i(devicematch->start_matches);

        for(size_t j=0;j<devicematch->num_matches;j++,hwidmatch++)
        {
            items_list.push_back(itembar_t(devicematch,hwidmatch,i+RES_SLOTS,remap[i],2));
            items_list.push_back(itembar_t(devicematch,hwidmatch,i+RES_SLOTS,remap[i],j?0:1));
        }
        if(!devicematch->num_matches)
        {
            items_list.push_back(itembar_t(devicematch,nullptr,i+RES_SLOTS,remap[i],1));
        }
    }
    items_list.shrink_to_fit();
}

void Manager::filter(int options,std::vector<std::wstring> *drpfilter)
{
    Devicematch *devicematch;
    itembar_t *itembar,*itembar1,*itembar_drp=nullptr,*itembar_drpcur=nullptr;
    size_t i,j,k;
    int cnt[NUM_STATUS+1];
    int ontorrent;
    int o1=options&FILTER_SHOW_ONE;
    o1=1;

	if(items_list.size()<=RES_SLOTS)return;
	itembar=&items_list[RES_SLOTS];

    for(i=RES_SLOTS;i<items_list.size();)
    {
        devicematch=itembar->devicematch;
        memset(cnt,0,sizeof(cnt));
        ontorrent=0;
        if(!devicematch){itembar++;i++;continue;}
        for(j=0;j<devicematch->num_matches;j++,itembar++,i++)
        {
            if(!itembar)Log.print_err("ERROR a%d\n",j);
            // default state is inactive
            itembar->isactive=0;
            //if(!itembar->hwidmatch)Log.print_con("ERROR %d,%d\n",itembar->index,j);
            if(!itembar->hwidmatch)continue;


            if(itembar->first&2)
            {
                itembar->isactive=0;
                itembar_drp=itembar;
                j--;
                continue;
            }
            if(Settings.flags&FLAG_FILTERSP&&j)continue;

            if(itembar->checked||itembar->install_status)itembar->isactive=1;

            if((options&FILTER_SHOW_INVALID)==0&&!itembar->hwidmatch->isdrivervalid())
                continue;

            if((options&FILTER_SHOW_DUP)==0&&itembar->hwidmatch->getStatus()&STATUS_DUP)
                continue;

            if((options&FILTER_SHOW_DUP)&&itembar->hwidmatch->getStatus()&STATUS_DUP)
            {
                itembar1=&items_list[i];
                for(k=0;k<devicematch->num_matches-j;k++,itembar1++)
                    if(itembar1->first&2)k--;
                        else
                    if(itembar1->isactive&&
                       itembar1->index==itembar->index&&
                       itembar1->hwidmatch->getdrp_infcrc()==itembar->hwidmatch->getdrp_infcrc())
                        break;

                if(k!=j)
                    itembar->isactive=1;
            }

            if((!o1||!cnt[NUM_STATUS])&&(options&FILTER_SHOW_MISSING)&&itembar->hwidmatch->getStatus()&STATUS_MISSING)
            {
                itembar->isactive=1;
                if(itembar->hwidmatch->getdrp_packontorrent()&&!ontorrent)
                    ontorrent=1;
                else
                    cnt[NUM_STATUS]++;
            }

            if(Settings.flags&FLAG_FILTERSP&&itembar->hwidmatch->getAltsectscore()==2&&!itembar->hwidmatch->isvalidcat(matcher->getState()))
                itembar->hwidmatch->setAltsectscore(1);

            for(k=0;k<NUM_STATUS;k++)
                if((!o1||!cnt[NUM_STATUS])&&(options&statustnl[k].filter)&&itembar->hwidmatch->getStatus()&statustnl[k].status)
            {
                if((options&FILTER_SHOW_WORSE_RANK)==0/*&&(options&FILTER_SHOW_OLD)==0*/&&(options&FILTER_SHOW_INVALID)==0&&
                   devicematch->device->problem==0&&devicematch->driver&&itembar->hwidmatch->getAltsectscore()<2)continue;

                if((options&FILTER_SHOW_OLD)!=0&&(itembar->hwidmatch->getStatus()&STATUS_BETTER))continue;

                // hide if
                //[X] Newer
                //[ ] Worse
                //worse, no problem
                if((options&FILTER_SHOW_NEWER)!=0 &&
                   (options&FILTER_SHOW_WORSE_RANK)==0 &&
                   (options&FILTER_SHOW_INVALID)==0 &&
                   itembar->hwidmatch->getStatus()&STATUS_WORSE &&
                   devicematch->device->problem==0 &&
                   devicematch->driver)
                   continue;

                if(itembar->hwidmatch->getdrp_packontorrent()&&!ontorrent)
                    ontorrent=1;
                else
                {
                    cnt[k]++;
                    cnt[NUM_STATUS]++;
                }
                itembar->isactive=1;
            }

            if(itembar->isactive&&Settings.flags&FLAG_SHOWDRPNAMES2)
            {
                if(itembar_drp)
                {
                    if(!itembar_drpcur||(itembar_drp->hwidmatch->cmpnames(itembar_drpcur->hwidmatch)!=0))
                    {
                        itembar_drp->isactive=1;
                        itembar_drpcur=itembar_drp;
                    }
                }
            }

            if(!itembar->hwidmatch->getdrp_packontorrent())
                if(o1&&itembar->hwidmatch->getStatus()&STATUS_CURRENT)
                    cnt[NUM_STATUS]++;
        }
        if(!devicematch->num_matches)
        {
            itembar->isactive=0;
            if(options&FILTER_SHOW_NF_STANDARD&&devicematch->status&STATUS_NF_STANDARD)itembar->isactive=1;
            if(options&FILTER_SHOW_NF_UNKNOWN&&devicematch->status&STATUS_NF_UNKNOWN)itembar->isactive=1;
            if(options&FILTER_SHOW_NF_MISSING&&devicematch->status&STATUS_NF_MISSING)itembar->isactive=1;
            if(itembar->first&2)
            {
                itembar->isactive=0;
                itembar_drp=itembar;
            }
            itembar++;i++;
        }
    }

    // driver pack filters
    if(drpfilter&&drpfilter->size()>0)
    {
        itembar=&items_list[RES_SLOTS];
        for(k=RES_SLOTS;k<items_list.size();k++,itembar++)
            if(itembar->isactive&&itembar->hwidmatch)
            {
                bool filtermatch=false;
                std::wstring drpname=itembar->hwidmatch->getdrp_packname();
                for(std::vector<std::wstring>::iterator txt = drpfilter->begin(); txt != drpfilter->end(); ++txt)
                {
                    std::wstring w=*txt;
                    w.insert(0,L"dp_");w.append(L"_");
                    if(StrStrIW(drpname.c_str(),w.c_str()))
                    {
                        filtermatch=true;
                        break;
                    }
                }
                if(!filtermatch)itembar->isactive=false;
            }
    }

    i=0;
    itembar=&items_list[RES_SLOTS];
    for(k=RES_SLOTS;k<items_list.size();k++,itembar++)
        if(itembar->isactive&&itembar->hwidmatch)i++;else itembar->checked=0;

    items_list[SLOT_NOUPDATES].isactive=
        items_list.size()==RES_SLOTS||
        (i==0&&Settings.statemode==0&&matcher->getCol()->size()>1)?1:0;

    // Uncomment to enable restore point panel
    /*items_list[SLOT_RESTORE_POINT].isactive=
        Settings.statemode==STATEMODE_EMUL||i==0||(Settings.flags&FLAG_NORESTOREPOINT)?0:1;
    *///set_rstpnt(0);

    setRestorePointStatus(false);
}

void Manager::print_tbl()
{
	int limits[7];

    if(Log.isHidden(LOG_VERBOSE_MANAGER))return;
    Log.print_file("{manager_print\n");
    memset(limits,0,sizeof(limits));

    for(auto itembar=items_list.begin()+RES_SLOTS;itembar!=items_list.end();++itembar)
        if(itembar->isactive&&itembar->hwidmatch)
            itembar->hwidmatch->calclen(limits);


    unsigned k=0,act=0;
    for(auto itembar=items_list.begin()+RES_SLOTS;itembar!=items_list.end();++itembar,k++)
        if(itembar->isactive&&(itembar->first&2)==0)
        {
            Log.print_file("$%04d|",k);
            if(itembar->hwidmatch)
                itembar->hwidmatch->print_tbl(limits);
            else
                Log.print_file("'%S'\n",matcher->getState()->textas.get(itembar->devicematch->device->Devicedesc));
            act++;
        }else
        {
//            log_file("$%04d|^^ %d,%d\n",k,itembar->devicematch->num_matches,(itembar->hwidmatch)?itembar->hwidmatch->status:-1);
        }

    Log.print_file("}manager_print[%d]\n\n",act);
}

int Manager::getlocale()
{
    return manager_g->matcher->getState()->getLocale();
}

State *Manager::getState()
{
    return matcher->getState();
}

void Manager::print_hr()
{
    if(Log.isHidden(LOG_VERBOSE_MANAGER))return;
    Log.print_file("{manager_print\n");

    unsigned k=0,act=0;
    for(auto itembar=items_list.begin()+RES_SLOTS;itembar!=items_list.end();++itembar,k++)
        if(itembar->isactive&&(itembar->first&2)==0)
        {
            if(Settings.flags&FLAG_FILTERSP&&!itembar->hwidmatch->isvalidcat(matcher->getState()))continue;
            wchar_t buf[BUFLEN];
            itembar->str_status(buf);
            Log.print_file("\n$%04d, %S\n",k,buf);
            if(itembar->devicematch->device)
            {
                itembar->devicematch->device->print(matcher->getState());
                //device_printHWIDS(itembar->devicematch->device,matcher->state);
            }
            if(itembar->devicematch->driver)
            {
                Log.print_file("Installed driver\n");
                itembar->devicematch->driver->print(matcher->getState());
            }

            if(itembar->hwidmatch)
            {
                Log.print_file("Available driver\n");
                itembar->hwidmatch->print_hr();
            }

            act++;
        }else
        {
//            log_file("$%04d|^^ %d,%d\n",k,itembar->devicematch->num_matches,(itembar->hwidmatch)?itembar->hwidmatch->status:-1);
        }

    Log.print_file("}manager_print[%d]\n\n",act);
}

//{ User interaction
// Zones:
// 0 button
// 1 checkbox
// 2 downarrow
// 3 text
int setaa=0;
void Manager::hitscan(int x,int y,size_t *r,int *zone)
{
    itembar_t *itembar;
    size_t i;
    int pos;
    int ofsy=MainWindow.getscrollpos();
    int cutoff=calc_cutoff()+D_X(DRVITEM_DIST_Y0);
    int ofs=0;
    int wx=XG(D_X(DRVITEM_WX),Xg(D_X(DRVITEM_OFSX),D_X(DRVITEM_WX)));

    *r=0;
    *zone=0;
    int cnt=0;

    if(MainWindow.kbpanel==KB_FIELD)
    {
        int max_cnt=0;
        itembar=&items_list[0];
        for(i=0;i<items_list.size();i++,itembar++)
        if(itembar->isactive&&(itembar->first&2)==0)max_cnt++;

        if(MainWindow.kbfield<0)MainWindow.kbfield=max_cnt-1;
        if(MainWindow.kbfield>=max_cnt)MainWindow.kbfield=0;
    }

    y-=-D_X(DRVITEM_DIST_Y0);
    x-=Xg(D_X(DRVITEM_OFSX),D_X(DRVITEM_WX));
    if(MainWindow.kbpanel==KB_NONE)if(x<0||x>wx)return;
    itembar=&items_list[0];
    for(i=0;i<items_list.size();i++,itembar++)
    if(itembar->isactive&&(itembar->first&2)==0)
    {

        if(MainWindow.kbpanel==KB_FIELD)
        {
            *r=i;
            if(MainWindow.kbfield==cnt)
            {
                if(setaa)
                {
                    animstart=System.GetTickCountWr();
                    MainWindow.offset_target=(itembar->curpos>>16);
                    SetTimer(MainWindow.hMain,1,1000/60,nullptr);
                    setaa=0;
                }
                return;
            }
            cnt++;
            continue;
        }
        pos=itembar->curpos>>16;
        if(i>=SLOT_RESTORE_POINT&&y<cutoff)continue;
        if(i>=SLOT_RESTORE_POINT)pos-=ofsy;
        if(y>pos&&y<pos+D_X(DRVITEM_WY))
        {
            x-=D_X(ITEM_CHECKBOX_OFS_X);
            y-=D_X(ITEM_CHECKBOX_OFS_Y)+pos;
            ofs=(itembar->first&1)?0:D_X(DRVITEM_LINE_INTEND);
            if(x-ofs>0)*r=i;
            if(x-ofs>0&&x-ofs<D_X(ITEM_CHECKBOX_SIZE)&&y>0&&y<D_X(ITEM_CHECKBOX_SIZE))*zone=1;
            if(x>wx-D_X(ITEM_ICON_SIZE)*32/21&&!ofs)*zone=2;
            if(!*zone&&(x-ofs<D_X(ITEM_CHECKBOX_SIZE)))*zone=3;
            if(!*zone&&(x>240+190))*zone=3;
            if(MainWindow.kbpanel==KB_NONE)return;
        }
    }
    *r=0;
}

void Manager::clear()
{
    itembar_t *itembar;
    size_t i;

    itembar=&items_list[RES_SLOTS];
    for(i=RES_SLOTS;i<items_list.size();i++,itembar++)
    {
        itembar->install_status=0;
        itembar->percent=0;
    }
    items_list[SLOT_EXTRACTING].isactive=0;
    setRestorePointStatus(true);
    filter(Settings.filters);
    setpos();
    invalidate(INVALIDATE_DEVICES|INVALIDATE_MANAGER);
}

void Manager::updateoverall()
{
    int _totalitems=0;
    int _processeditems=0;
    size_t j;

    if(installmode==MODE_NONE)
    {
        items_list[SLOT_EXTRACTING].percent=0;
        return;
    }
    itembar_t *itembar1=&items_list[RES_SLOTS];
    for(j=RES_SLOTS;j<items_list.size();j++,itembar1++)
    if(itembar1->install_status!=STR_INST_STOPPING)
    {
        if(itembar1->checked||itembar1->install_status){_totalitems++;}
        if(itembar1->install_status&&!itembar1->checked){_processeditems++;}
    }
    if(_totalitems)
    {
        __int64 d=items_list[itembar_act].percent/_totalitems;
        if(items_list[itembar_act].checked==0)d=0;
        if(itembar_act==SLOT_RESTORE_POINT) d=0;
        items_list[SLOT_EXTRACTING].percent=_processeditems*1000/_totalitems+d;
        items_list[SLOT_EXTRACTING].val1=_processeditems;
        items_list[SLOT_EXTRACTING].val2=_totalitems;
        if(manager_g->items_list[SLOT_EXTRACTING].percent>0&&installmode==MODE_INSTALLING&&Updater->isPaused())
            MainWindow.ShowProgressInTaskbar(true,items_list[SLOT_EXTRACTING].percent,1000);
    }
}
size_t Manager::install(int flagsv)
{
    instflag=flagsv;
    return _beginthreadex(nullptr,0,&thread_install,nullptr,0,nullptr);
}

void Manager::testitembars()
{
    itembar_t *itembar;
    size_t i,j=0,index=RES_SLOTS+1;
    size_t prev_index=0;

    itembar=&items_list[0];

    filter(FILTER_SHOW_CURRENT|FILTER_SHOW_NEWER);
    wcscpy(Settings.drpext_dir,L"drpext");
    items_list[SLOT_EMPTY].curpos=1;

    for(i=0;i<items_list.size();i++,itembar++)
    if(i>SLOT_EMPTY&&i<RES_SLOTS)
    {
        if(i==SLOT_VIRUS_HIDDEN||i==SLOT_VIRUS_RECYCLER||i==SLOT_NODRIVERS||i==SLOT_DPRDIR)continue;
        itembar_settext(i,1);
    }
    else if(itembar->isactive)
    {
        if(!itembar->devicematch||prev_index==itembar->index){itembar->isactive=0;continue;}
        prev_index=itembar->index;
        itembar->checked=0;
        if(j==0||j==6||j==9||j==18||j==21)index++;
        itembar->index=index;
        itembar->hwidmatch->setAltsectscore(2);
        switch(j++)
        {
            case  0:itembar->install_status=STR_INST_EXTRACT;itembar->percent=300;itembar->checked=1;break;
            case  1:itembar->install_status=STR_INST_INSTALL;itembar->percent=900;itembar->checked=1;break;
            case  2:itembar->install_status=STR_INST_EXTRACT;itembar->percent=400;break;
            case  3:itembar->install_status=STR_INST_OK;break;
            case  4:itembar->install_status=STR_INST_REBOOT;break;
            case  5:itembar->install_status=STR_INST_FAILED;break;

            case  6:itembar->hwidmatch->setStatus(STATUS_INVALID);break;
            case  7:itembar->hwidmatch->setStatus(STATUS_MISSING);break;
            case  8:itembar->hwidmatch->setStatus(STATUS_CURRENT|STATUS_SAME|STATUS_DUP);break;

            case  9:itembar->hwidmatch->setStatus(STATUS_NEW|STATUS_BETTER);break;
            case 10:itembar->hwidmatch->setStatus(STATUS_NEW|STATUS_SAME);break;
            case 11:itembar->hwidmatch->setStatus(STATUS_NEW|STATUS_WORSE);break;
            case 12:itembar->hwidmatch->setStatus(STATUS_CURRENT|STATUS_BETTER);break;
            case 13:itembar->hwidmatch->setStatus(STATUS_CURRENT|STATUS_SAME);break;
            case 14:itembar->hwidmatch->setStatus(STATUS_CURRENT|STATUS_WORSE);break;
            case 15:itembar->hwidmatch->setStatus(STATUS_OLD|STATUS_BETTER);break;
            case 16:itembar->hwidmatch->setStatus(STATUS_OLD|STATUS_SAME);break;
            case 17:itembar->hwidmatch->setStatus(STATUS_OLD|STATUS_WORSE);break;

            case 18:itembar->devicematch->status=STATUS_NF_MISSING;itembar->hwidmatch=nullptr;break;
            case 19:itembar->devicematch->status=STATUS_NF_STANDARD;itembar->hwidmatch=nullptr;break;
            case 20:itembar->devicematch->status=STATUS_NF_UNKNOWN;itembar->hwidmatch=nullptr;break;
            default:itembar->isactive=0;
        }
    }

}

void Manager::toggle(size_t index)
{
    itembar_t *itembar,*itembar1;
    size_t i;
    size_t group;

    #ifdef USE_TORRENT
    if(installmode&&!Updater->isPaused())return;
    #endif

    itembar1=&items_list[index];
    if(index>=RES_SLOTS&&!itembar1->hwidmatch)return;
    itembar1->checked^=1;
    if(installmode)
    {
        if(itembar1->checked)
        {
            if(itembar1->install_status==STR_INST_STOPPING)itembar1->install_status=0;
        }
        else
        {
            itembar1->install_status=STR_INST_STOPPING;
        }
    }
    if(index==SLOT_RESTORE_POINT)
    {
        set_rstpnt(itembar1->checked);
    }
    group=itembar1->index;

    itembar=&items_list[0];
    for(i=0;i<items_list.size();i++,itembar++)
        if(itembar!=itembar1&&itembar->index==group)
            itembar->checked&=~1;

    if(itembar1->checked)expand(index,EXPAND_MODE::COLLAPSE);
    MainWindow.redrawmainwnd();
}

void Manager::expand(size_t index,EXPAND_MODE f)
{
    itembar_t *itembar,*itembar1;
    size_t i;
    size_t group;

    itembar1=&items_list[index];
    group=itembar1->index;

    itembar=&items_list[0];
    if((itembar1->isactive&2)==0)// collapsed
    {
        if(f==EXPAND_MODE::COLLAPSE)return;
        for(i=0;i<items_list.size();i++,itembar++)
            if(itembar->index==group&&itembar->hwidmatch&&(itembar->hwidmatch->getStatus()&STATUS_INVALID)==0&&(itembar->first&2)==0)
                {
                    itembar->isactive|=2; // expand
                }
    }
    else
    {
        if(f==EXPAND_MODE::EXPAND)return;
        for(i=0;i<items_list.size();i++,itembar++)
            if(itembar->index==group&&(itembar->first&2)==0)
            {
                itembar->isactive&=1; //collapse
                if(itembar->checked)itembar->isactive|=4;
            }
    }
    setpos();
}

void Manager::selectnone()
{
    itembar_t *itembar;
    size_t i;

    #ifdef USE_TORRENT
    if(installmode&&!Updater->isPaused())return;
    #endif

    if(items_list[SLOT_RESTORE_POINT].isactive)
    {
        set_rstpnt(0);
    }
    itembar=&items_list[RES_SLOTS];
    for(i=RES_SLOTS;i<items_list.size();i++,itembar++)itembar->checked=0;
}

void Manager::selectall()
{
    itembar_t *itembar;
    size_t i;
    size_t group=0;

    #ifdef USE_TORRENT
    if(installmode&&!Updater->isPaused())return;
    #endif

    itembar=&items_list[SLOT_RESTORE_POINT];
    if(itembar->install_status==STR_RESTOREPOINT&&itembar->isactive)
        set_rstpnt(1);

    itembar=&items_list[RES_SLOTS];
    for(i=RES_SLOTS;i<items_list.size();i++,itembar++)
    {
        itembar->checked=0;
        if(itembar->isactive&&group!=itembar->index&&itembar->hwidmatch&&(itembar->first&2)==0)
        {
            if(itembar->install_status==0)itembar->checked=1;
            group=itembar->index;
        }
    }
}

int Manager::selected()
{
    int count=0;
    itembar_t *itembar;
    itembar=&items_list[RES_SLOTS];
    for(size_t i=RES_SLOTS;i<items_list.size();i++,itembar++)
        if(itembar->checked)
        {
            count++;
            //Log.print_debug("%S\n",itembar->hwidmatch->getdrp_packname());
        }
    return count;
}

int Manager::active()
{
    int count=0;
    itembar_t *itembar;
    itembar=&items_list[RES_SLOTS];
    for(size_t i=RES_SLOTS;i<items_list.size();i++,itembar++)
        if(itembar->isactive)
        count++;
    return count;
}
//}

//{ Helpers
void Manager::itembar_settext(size_t i,const wchar_t *txt1,int percent)
{
    if(Settings.flags&FLAG_SCRIPTMODE)return;
    itembar_t *itembar=&items_list[i];
    wcscpy(itembar->txt1,txt1);
    itembar->percent=percent;
    itembar->isactive=1;
    MainWindow.redrawfield();
}

void Manager::itembar_settext(size_t i,int act,const wchar_t *txt1,__int64 val1v,__int64 val2v,__int64 percent)
{
    if(Settings.flags&FLAG_SCRIPTMODE)return;
    itembar_t *itembar=&items_list[i];
    if(txt1)wcscpy(itembar->txt1,txt1);
    if(val1v>=0)itembar->val1=val1v;
    if(val2v>=0)itembar->val2=val2v;
    if(!val2v)val2v++;
    itembar->percent=(percent>=0)?percent:val1v*1000/val2v;
    itembar->isactive=act;
    setpos();
    MainWindow.redrawfield();
}

void Manager::set_rstpnt(int checked)
{
    if(MainWindow.kbpanel)return;

    ClickVisiter cv{ID_RESTPNT,checked?CHECKBOX::SET:CHECKBOX::CLEAR};
    wPanels->Accept(cv);

    items_list[SLOT_RESTORE_POINT].checked=checked;
    setpos();
    MainWindow.redrawfield();
}

void Manager::itembar_setactive(size_t i,int val){items_list[i].isactive=val;}
void Manager::popup_drivercmp(Manager *manager,Canvas &canvas,int wx,int wy,size_t index){ items_list[Popup->floating_itembar].popup_drivercmp(manager,canvas,wx,wy,index); }
void Manager::contextmenu(int x,int y){items_list[Popup->floating_itembar].contextmenu(x,y);}
const wchar_t *Manager::getHWIDby(int id)const{return items_list[Popup->floating_itembar].devicematch->device->getHWIDby(id,matcher->getState());}

void Manager::getINFpath(int wp)
{
    WStringShort buf;
    State *state=matcher->getState();

    buf.sprintf(L"%s%s%s",
            (wp==ID_LOCATEINF)?L"/select,":L"",
            state->textas.get(state->getWindir()),
            state->textas.get(items_list[Popup->floating_itembar].devicematch->driver->getInfPath()));

    if(wp==ID_OPENINF)
        System.run_command(buf.Get(),L"",SW_SHOW,0);
    else
        System.run_command(L"explorer.exe",buf.Get(),SW_SHOW,0);
}
//}

//{ Driver list
void Manager::setpos()
{
    Devicematch *devicematch;
    itembar_t *itembar,*lastitembar=nullptr;
    size_t k;
    int cnt=0;
    int pos=D_X(DRVITEM_OFSY);
    //int pos=0;
    size_t group=0;
    size_t lastmatch=0;

//0:wide
//1:narrow

    itembar=&items_list[0];
    bool prev_was_a_slot=false;
    for(k=0;k<items_list.size();k++,itembar++)
    {
        devicematch=itembar->devicematch;
        cnt=group==itembar->index?1:0;

        //if(lastitembar&&lastitembar->index<SLOT_RESTORE_POINT&&itembar->index<SLOT_RESTORE_POINT)cnt=1;
        if(devicematch&&!devicematch->num_matches&&!lastmatch&&lastitembar&&lastitembar->index>=SLOT_RESTORE_POINT)cnt=1;

        itembar->itembar_setpos(&pos,&cnt,prev_was_a_slot);
        if(itembar->isactive)
        {
            lastitembar=itembar;
            prev_was_a_slot=k<RES_SLOTS;
            group=itembar->index;
            if(devicematch)lastmatch=devicematch->num_matches;
        }
    }
    SetTimer(MainWindow.hMain,1,1000/60,nullptr);
    animstart=System.GetTickCountWr();
}

int Manager::animate()
{
    int chg=0;
    int tt1=System.GetTickCountWr()-animstart;

    // Move itembars
    for(auto &itembar:items_list)
    {
        if(itembar.curpos==itembar.tagpos)continue;
        chg=1;
        int pos=itembar.oldpos+itembar.accel*tt1;
        if(itembar.accel>0&&pos>itembar.tagpos)pos=itembar.tagpos;
        if(itembar.accel<0&&pos<itembar.tagpos)pos=itembar.tagpos;
        itembar.curpos=pos;
    }

    // Animate scrolling
    int i=MainWindow.getscrollpos();
    if(MainWindow.offset_target)
    {
        int v=MainWindow.offset_target-D_X(DRVITEM_DIST_Y0)*2;
        if(i>v)
        {
            i--;
            i-=(i-v)/10;
            if(i<v)i=v;
            MainWindow.setscrollpos(i);
            chg=1;
        }

        v=MainWindow.offset_target+D_X(DRVITEM_DIST_Y0)-MainWindow.mainy_c;
        if(i<v)
        {
            i++;
            i-=(i-v)/10;
            if(i>v)i=v;
            MainWindow.setscrollpos(i);
            chg=1;
        }
    }

    return chg||
        (installmode==MODE_NONE&&items_list[SLOT_EXTRACTING].install_status);
}

int Manager::groupsize(size_t index)
{
    int num=0;

    for(auto &itembar:items_list)
        if(itembar.index==index&&itembar.hwidmatch&&(itembar.hwidmatch->getStatus()&STATUS_INVALID)==0&&(itembar.first&2)==0)
            num++;

    return num;
}

size_t Manager::countItems()
{
    size_t j,cnt=0;
    itembar_t *itembar;

	if(items_list.size()<=RES_SLOTS)return 0;
	itembar=&items_list[RES_SLOTS];
    for(j=RES_SLOTS;j<items_list.size();j++,itembar++)
    if(itembar->checked)cnt++;
    return cnt;
}

int Manager::drawitem(Canvas &canvas,size_t index,int ofsy,int zone,int cutoff)
{
    itembar_t *itembar=&items_list[index];

    wchar_t bufw[BUFLEN];
    int x=Xg(D_X(DRVITEM_OFSX),D_X(DRVITEM_WX));
    int wx=XG(D_X(DRVITEM_WX),x);
    int r=D_X(boxindex[itembar->box_status()]+3);
    size_t intend=0;
    int oldstyle=Settings.flags&FLAG_SHOWDRPNAMES1||Settings.flags&FLAG_OLDSTYLE;

    int pos=(itembar->curpos>>16)-D_X(DRVITEM_DIST_Y0);
    if(index>=SLOT_RESTORE_POINT) pos-=ofsy;

    if(!(itembar->first&1))
    {
        size_t i=index;

        while(i>0&&!(items_list[i].first&1&&items_list[i].isactive))i--;
        if(items_list[i].index==itembar->index)intend=i;
        //itembar->index=intend;
    }
    if(intend)
    {
        x+=D_X(DRVITEM_LINE_INTEND);
        wx-=D_X(DRVITEM_LINE_INTEND);
    }
    if(pos<=-D_X(DRVITEM_DIST_Y0))return 0;
    if(pos>MainWindow.mainy_c)return 0;
    if(wx<0)return 0;

    //canvas.SetFont(MainWindow.hFont);

    if(index<SLOT_RESTORE_POINT)cutoff=D_X(DRVITEM_OFSY);
    ClipRegion hrgn2{0,cutoff,x+wx,MainWindow.mainy_c};
    ClipRegion hrgn{x,(pos<cutoff)?cutoff:pos,x+wx,pos+D_X(DRVITEM_WY),r};
    int cl=((zone>=0)?1:0);
    if(index==SLOT_EXTRACTING&&itembar->install_status&&installmode==MODE_NONE)
        cl=((System.GetTickCountWr()-animstart)/200)%2;
    canvas.SetClipRegion(hrgn2);
    if(intend&&D_1(DRVITEM_LINE_WIDTH)&&!(itembar->first&2))
        canvas.DrawConnection(x,pos,ofsy,items_list[intend].curpos>>16);

    canvas.DrawWidget(x,pos,x+wx,pos+D_X(DRVITEM_WY),itembar->box_status()+cl);
    canvas.SetClipRegion(hrgn);

    if(itembar->percent)
    {
        //printf("%d\n",itembar->percent);
        int a=BOX_PROGR;
        //if(index==SLOT_EXTRACTING&&installmode==MODE_STOPPING)a=BOX_PROGR_S;
        //if(index>=RES_SLOTS&&(!itembar->checked||installmode==MODE_STOPPING))a=BOX_PROGR_S;
        canvas.DrawWidget(x,pos,(int)(x+wx*itembar->percent/1000),pos+D_X(DRVITEM_WY),a);
    }

    canvas.SetTextColor(0); // todo: color
    switch(index)
    {
        case SLOT_RESTORE_POINT:
            canvas.DrawCheckbox(x+D_X(ITEM_CHECKBOX_OFS_X),pos+D_X(ITEM_CHECKBOX_OFS_Y),
                         D_X(ITEM_CHECKBOX_SIZE),D_X(ITEM_CHECKBOX_SIZE),
                         itembar->checked,zone>=0,1);

            wcscpy(bufw,STR(itembar->install_status));
            canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
            canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y)/2,bufw);
            break;

        case SLOT_INDEXING:
            wsprintf(bufw,L"%s (%d%s%d)",STR(itembar->isactive==2?STR_INDEXLZMA:STR_INDEXING),
                        (int)items_list[SLOT_INDEXING].val1,STR(STR_OF),
                        (int)items_list[SLOT_INDEXING].val2);
            canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
            canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos,bufw);

            if(*itembar->txt1)
            {
                wsprintf(bufw,L"%s",itembar->txt1);
                canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y),bufw);
            }
            break;

        case SLOT_EXTRACTING:
            pos+=D_X(ITEM_TEXT_OFS_Y);
            if(installmode)
            {
                if(installmode==MODE_INSTALLING)
                {
                wsprintf(bufw,L"%s (%d%s%d)",STR(itembar->install_status),
                        (int)items_list[SLOT_EXTRACTING].val1+1,STR(STR_OF),
                        (int)items_list[SLOT_EXTRACTING].val2);

                }
                else
                    if(itembar->install_status)wsprintf(bufw,STR(itembar->install_status),itembar->percent);

                canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
                canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos,bufw);
                if(itembar_act>=RES_SLOTS)
                {
                    wsprintf(bufw,L"%S",items_list[itembar_act].hwidmatch->getdrp_drvdesc());
                    canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+15));
                    canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y),bufw);
                }
            }else
            {
                wsprintf(bufw,L"%s",STR(itembar->install_status));
                canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
                canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos,bufw);
                wsprintf(bufw,L"%s",STR(STR_INST_CLOSE));
                canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+15));
                canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y),bufw);
            }
            break;

        case SLOT_NODRIVERS:
            itembar->drawbutton(canvas,x-D_X(ITEM_TEXT_OFS_X)+D_X(ITEM_CHECKBOX_OFS_X),pos,STR(STR_EMPTYDRP),matcher->getCol()->getDriverpack_dir());
            break;

        case SLOT_NOUPDATES:
            pos+=D_X(ITEM_TEXT_OFS_Y);
            if(*itembar->txt1)
                wsprintf(bufw,L"%s",itembar->txt1);
            else if(items_list.size()>RES_SLOTS)
                wsprintf(bufw,L"%s",STR(STR_NOUPDATES));
            else
                wsprintf(bufw,L"%s",STR(STR_INITIALIZING));
            //wsprintf(bufw,L"%s",STR(items_list.size()>RES_SLOTS?STR_NOUPDATES:STR_INITIALIZING));
            canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
            canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y)/2,bufw);
            break;

        case SLOT_BOOSTY:
            pos+=D_X(ITEM_TEXT_OFS_Y);
            canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
            canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos,STR(STR_BOOSTY1));
            canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+15));
            canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y),STR(STR_BOOSTY2));
            break;

        case SLOT_DOWNLOAD:
            if(*itembar->txt1)
                wsprintf(bufw,L"%s",itembar->txt1);
            else if(itembar->val1>>8)
                wsprintf(bufw,STR(itembar->val1&0xFF?STR_UPD_AVAIL3:STR_UPD_AVAIL1),static_cast<int>(itembar->val1>>8),static_cast<int>(itembar->val1&0xFF));
            else if(itembar->val1&0xff)
                wsprintf(bufw,STR(STR_UPD_AVAIL2),static_cast<int>(itembar->val1&0xFF));

#ifdef USE_TORRENT
            if(!Updater->isPaused())
            {
                Updater->ShowProgress(bufw);
                if(Updater->isSeedingDrivers())
                    itembar->drawbutton(canvas,x,pos,bufw,STR(STR_DWN_MODIFY));
                else
                    itembar->drawbutton(canvas,x,pos,bufw,STR(STR_UPD_MODIFY));
            }
            else
#endif
                itembar->drawbutton(canvas,x,pos,bufw,STR(STR_UPD_START));

            break;

        case SLOT_SNAPSHOT:
            itembar->drawbutton(canvas,x,pos,Settings.state_file,STR(STR_CLOSE_SNAPSHOT));
            break;

        case SLOT_DPRDIR:
            itembar->drawbutton(canvas,x,pos,Settings.drpext_dir,STR(STR_CLOSE_DRPEXT));
            break;

        case SLOT_VIRUS_AUTORUN:
            itembar->drawbutton(canvas,x,pos,STR(STR_VIRUS),STR(STR_VIRUS_AUTORUN));
            break;

        case SLOT_VIRUS_RECYCLER:
            itembar->drawbutton(canvas,x,pos,STR(STR_VIRUS),STR(STR_VIRUS_RECYCLER));
            break;

        case SLOT_VIRUS_HIDDEN:
            itembar->drawbutton(canvas,x,pos,STR(STR_VIRUS),STR(STR_VIRUS_HIDDEN));
            break;

        default:
            if(itembar->first&2&&itembar->hwidmatch)
            {
                    /*wsprintf(bufw,L"%ws",matcher->state->text+itembar->devicematch->device->Devicedesc);
                    canvas.setTextColor(D_C(boxindex[box_status(index)]+14));
                    TextOutH(hdc,x+D_X(ITEM_TEXT_OFS_X),pos,bufw);*/

                    //str_status(bufw,itembar);
                    WStringShort bufw1;
                    itembar->hwidmatch->getdrp_packnameVirtual(bufw1);
                    canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+15));
                    canvas.DrawTextXY(x+D_X(ITEM_CHECKBOX_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y)+5,bufw1.Get());
                    break;
            }
            if(itembar->hwidmatch)
            {
                // Checkbox
                canvas.DrawCheckbox(x+D_X(ITEM_CHECKBOX_OFS_X),pos+D_X(ITEM_CHECKBOX_OFS_Y),
                         D_X(ITEM_CHECKBOX_SIZE),D_X(ITEM_CHECKBOX_SIZE),
                         itembar->checked,zone>=0,1);

                // Available driver desc
                pos+=D_X(ITEM_TEXT_OFS_Y);
                wsprintf(bufw,L"%S",itembar->hwidmatch->getdrp_drvdesc());
                canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
                RECT_WR rect;
                int wx1=wx-D_X(ITEM_TEXT_OFS_X)-D_X(ITEM_ICON_OFS_X);
                rect.left=x+D_X(ITEM_TEXT_OFS_X);
                rect.top=pos;
                if(intend)wx1-=D_X(DRVITEM_LINE_INTEND);
                rect.right=rect.left+wx1/2-D_X(20);
                rect.bottom=rect.top+90;
                if(oldstyle)
                    canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos,bufw);
                else
                    canvas.DrawTextRect(bufw,&rect,rtl?DT_RIGHT:0);


                // Available driver status
                canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+15));
                itembar->str_status(bufw);
                switch(itembar->install_status)
                {
                    case STR_INST_FAILED:
                    case STR_EXTR_FAILED:
                        wsprintf(bufw,L"%s %X",STR(itembar->install_status),static_cast<unsigned>(itembar->val1));
                        break;

                    case STR_INST_EXTRACT:
                        wsprintf(bufw,STR(STR_INST_EXTRACT),(itembar->percent+100)/10);
                        break;

                    case STR_EXTR_EXTRACTING:
                        wsprintf(bufw,L"%s %d%%",STR(STR_EXTR_EXTRACTING),static_cast<int>(itembar->percent/10));
                        break;

                    case 0:
                        break;

                    default:
                        wcscpy(bufw,STR(itembar->install_status));
                }
                rect.left=x+D_X(ITEM_TEXT_OFS_X)+wx1/2;
                rect.top=pos;
                rect.right=rect.left+wx1/2;
                rect.bottom=rect.top+90;
                if(oldstyle)
                    canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y),bufw);
                else
                    canvas.DrawTextRect(bufw,&rect);

                if(Settings.flags&FLAG_SHOWDRPNAMES1)
                {
                    size_t len=wcslen(matcher->getCol()->getDriverpack_dir());
                    size_t lnn=len-wcslen(itembar->hwidmatch->getdrp_packpath());

                    canvas.SetTextColor(0);// todo: color
                    WStringShort packname;
                    itembar->hwidmatch->getdrp_packnameVirtual(packname);

                    wsprintf(bufw,L"%ws%ws%ws",
                            itembar->hwidmatch->getdrp_packpath()+len+(lnn?1:0),
                            lnn?L"\\":L"",
                            packname.Get());
                    canvas.DrawTextXY(rect.left,pos+D_X(ITEM_TEXT_DIST_Y),bufw);
                }
            }
            else
            {
                // Device desc
                pos+=D_X(ITEM_TEXT_OFS_Y);
                if(itembar->devicematch)
                {
                    wsprintf(bufw,L"%ws",matcher->getState()->textas.getw(itembar->devicematch->device->Devicedesc));
                    canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+14));
                    RECT_WR rect;
                    int wx1=wx-D_X(ITEM_TEXT_OFS_X)-D_X(ITEM_ICON_OFS_X);
                    rect.left=x+D_X(ITEM_TEXT_OFS_X);
                    rect.top=pos;
                    rect.right=rect.left+wx1/2;
                    rect.bottom=rect.top+90;
                    if(oldstyle)
                        canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos,bufw);
                    else
                        canvas.DrawTextRect(bufw,&rect);

                    itembar->str_status(bufw);
                    canvas.SetTextColor(D_C(boxindex[itembar->box_status()]+15));
                    rect.left=x+D_X(ITEM_TEXT_OFS_X)+wx1/2;
                    rect.top=pos;
                    rect.right=rect.left+wx1/2;
                    rect.bottom=rect.top+90;
                    if(oldstyle)
                        canvas.DrawTextXY(x+D_X(ITEM_TEXT_OFS_X),pos+D_X(ITEM_TEXT_DIST_Y),bufw);
                    else
                        canvas.DrawTextRect(bufw,&rect);
                }
            }
            // Device icon
            if(itembar->devicematch)
            {
                canvas.DrawIcon(x+D_X(ITEM_ICON_OFS_X),pos+D_X(ITEM_ICON_OFS_Y),
                                (itembar->hwidmatch)?itembar->hwidmatch->getdrp_drvfield(ClassGuid_):nullptr,
                                itembar->devicematch->device);
            }

            // Expand icon
            if(groupsize(itembar->index)>1&&itembar->first&1)
            {
                int xo=x+wx-D_X(ITEM_ICON_SIZE)*39/32;
                pos+=D_X(ITEM_ICON_SIZE)*4/32;
                canvas.DrawImage(*vTheme->GetIcon((itembar->isactive&2?0:2)+(zone==2?1:0)),xo,pos,xo+D_X(ITEM_ICON_SIZE),pos+D_X(ITEM_ICON_SIZE),0,Image::HSTR|Image::VSTR);
            }
            break;

    }

    canvas.ClearClipRegion();
    return 1;
}

void Manager::setRestorePointStatus(bool clr)
{
    if(!items_list[SLOT_RESTORE_POINT].install_status||clr)
    {
        State *state=matcher->getState();
        if(System.SystemProtectionEnabled(state))
            items_list[SLOT_RESTORE_POINT].install_status=STR_RESTOREPOINT;
        else
            items_list[SLOT_RESTORE_POINT].install_status=STR_RESTOREPOINTS_DISABLED;
    }
}
int Manager::isbehind(int pos,int ofsy,size_t j)
{
    itembar_t *itembar;

    if(j<SLOT_RESTORE_POINT)return 0;
    if(pos-ofsy<=-D_X(DRVITEM_DIST_Y0))return 1;
    if(pos-ofsy>MainWindow.mainy_c)return 1;

    itembar=&items_list[j-1];
    if((itembar->curpos>>16)==pos)return 1;

    return 0;
}

int Manager::calc_cutoff()
{
    int i,cutoff=0;

    for(i=0;i<SLOT_RESTORE_POINT;i++)
        if(items_list[i].isactive)cutoff=(items_list[i].curpos>>16);

    return cutoff;
}

void Manager::draw(Canvas &canvas,int ofsy)
{
    size_t i;
    int maxpos=0;
    int nm=0;
    size_t cur_i;
    int zone;
    int cutoff=0;
    POINT p;
    RECT rect;

    GetCursorPos(&p);
    ScreenToClient(MainWindow.hField,&p);
    hitscan(p.x,p.y,&cur_i,&zone);

    GetClientRect(MainWindow.hField,&rect);
    canvas.DrawWidget(0,0,rect.right,rect.bottom,BOX_DRVLIST);

    cutoff=calc_cutoff();
    items_list[itembar_act].updatecur();
    updateoverall();
    i=items_list.size()-1;
    for(auto itembar=items_list.crbegin();itembar!=items_list.crend();++itembar,--i)
    {
        if(itembar->isactive)continue;
        if(isbehind((itembar->curpos>>16),ofsy,i))continue;
        nm+=drawitem(canvas,i,ofsy,-1,cutoff);
    }
    i=items_list.size()-1;
    for(auto itembar=items_list.rbegin();itembar!=items_list.rend();++itembar,--i)
    {
        if(itembar->isactive==0)continue;
        if(itembar->curpos>maxpos)maxpos=itembar->curpos;
        nm+=drawitem(canvas,i,ofsy,cur_i==i?zone:-1,cutoff);
    }
    //printf("nm:%3d, ofs:%d\n",nm,ofsy);
    MainWindow.setscrollrange((maxpos>>16)+20);
}

void Manager::restorepos1(Manager *manager_prev)
{
    int i;

    std::copy_n(manager_prev->items_list.begin(),(int)RES_SLOTS,items_list.begin());
    //memcpy(&items_list.front(),&manager_prev->items_list.front(),sizeof(itembar_t)*RES_SLOTS);
    populate();
    filter(Settings.filters);
    items_list[SLOT_SNAPSHOT].isactive=Settings.statemode==STATEMODE_EMUL?1:0;
    items_list[SLOT_DPRDIR].isactive=*Settings.drpext_dir?1:0;
    restorepos(manager_prev);
    //viruscheck(L"",0,0);
    setpos();
    Log.print_con("}Sync\n");
    invaidate_set=0;
    if(CRITICAL_SECTION_ACTIVE)LeaveCriticalSection(&sync);

    #ifdef USE_TORRENT
    Updater->Populate(0);
    #endif
    //Log.print_con("Mode in WM_BUNDLEREADY: %d\n",installmode);
    if(Settings.flags&FLAG_AUTOINSTALL)
    {
        int cnt=0;
        if(installmode==MODE_SCANNING)
        {
            if(!isRebootDesired())selectall();
            itembar_t *itembar=&items_list[RES_SLOTS];
            for(i=RES_SLOTS;(unsigned)i<items_list.size();i++,itembar++)
                if(itembar->checked)
            {
                cnt++;
            }

            if(!cnt)Settings.flags&=~FLAG_AUTOINSTALL;
            Log.print_con("Autoinstall rescan: %d found\n",cnt);
        }

        if(installmode==MODE_NONE||(installmode==MODE_SCANNING&&cnt))
        {
            if(!isRebootDesired())selectall();
            if((Settings.flags&FLAG_EXTRACTONLY)==0)
            wsprintf(extractdir,L"%s\\SDI",matcher->getState()->textas.getw(matcher->getState()->getTemp()));
            install(INSTALLDRIVERS);
        }
        else
        {
            wchar_t buf[BUFLEN];

            installmode=MODE_NONE;
            if(isRebootDesired())
                wcscpy(buf,L" /c Shutdown.exe -r -t 3");
            else
                wsprintf(buf,L" /c %s",needreboot?Settings.finish_rb:Settings.finish);

            if(*(needreboot?Settings.finish_rb:Settings.finish)||isRebootDesired())
                System.run_command(L"cmd",buf,SW_HIDE,0);

            if(Settings.flags&FLAG_AUTOCLOSE)PostMessage(MainWindow.hMain,WM_CLOSE,0,0);
        }
    }
    else
        if(installmode==MODE_SCANNING)installmode=MODE_NONE;
}

//Keep when:
//* installing drivers
//* device update
//Discard when:
//* loading a snapshot
//* returning to real machine
//* driverpack update
void Manager::restorepos(Manager *manager_old)
{
    itembar_t *itembar_new,*itembar_old;
    Txt *t_new,*t_old;
    size_t i,j;
    bool show_changes=manager_old->items_list.size()>20;

    //if(statemode==STATEMODE_LOAD)show_changes=0;
    if(Log.isHidden(LOG_VERBOSE_DEVSYNC))show_changes=0;
    //show_changes=1;

    t_old=&manager_old->matcher->getState()->textas;
    t_new=&matcher->getState()->textas;

    if(manager_old->items_list[SLOT_EMPTY].curpos==1)
    {
        return;
    }
    if(invaidate_set&INVALIDATE_MANAGER)return;

    Log.print_con("{Updated %d->%d %d\n",manager_old->items_list.size(),items_list.size(),t_new);
    Log.set_mode(1);
    itembar_new=&items_list[RES_SLOTS];
    for(i=RES_SLOTS;i<items_list.size();i++,itembar_new++)
    {
        itembar_old=&manager_old->items_list[RES_SLOTS];

        if(itembar_act&&itembar_cmp(itembar_new,&manager_old->items_list[itembar_act],t_new,t_old))
        {
            Log.print_con("Act %d -> %d\n",itembar_act,i);
            itembar_act=i;
        }

        for(j=RES_SLOTS;j<manager_old->items_list.size();j++,itembar_old++)
        {
            if(itembar_old->isactive!=9)
            {
                if(itembar_cmp(itembar_new,itembar_old,t_new,t_old))
                {
                    wcscpy(itembar_new->txt1,itembar_old->txt1);
                    itembar_new->install_status=itembar_old->install_status;
                    itembar_new->val1=itembar_old->val1;
                    itembar_new->val2=itembar_old->val2;
                    itembar_new->percent=itembar_old->percent;

                    itembar_new->isactive=itembar_old->isactive;
                    itembar_new->checked=itembar_old->checked;

                    itembar_new->oldpos=itembar_old->oldpos;
                    itembar_new->curpos=itembar_old->curpos;
                    itembar_new->tagpos=itembar_old->tagpos;
                    itembar_new->accel=itembar_old->accel;

                    itembar_old->isactive=9;
                    break;
                }
            }
        }
        if(show_changes)
        if(j==manager_old->items_list.size())
        {
            Log.print_con("\nAdded   $%04d|%S|%S|",i,t_new->getw(itembar_new->devicematch->device->Driver),
                    t_new->getw(itembar_new->devicematch->device->Devicedesc));

            if(itembar_new->hwidmatch)
            {
				int limits[7];
                memset(limits,0,sizeof(limits));
                Log.print_con("%d|\n",itembar_new->hwidmatch->getHWID_index());
                itembar_new->hwidmatch->print_tbl(limits);
            }
            else
                itembar_new->devicematch->device->print(matcher->getState());
        }
    }

    itembar_old=&manager_old->items_list[RES_SLOTS];
    if(show_changes)
    for(j=RES_SLOTS;j<manager_old->items_list.size();j++,itembar_old++)
    {
        if(itembar_old->isactive!=9)
        {
            Log.print_con("\nDeleted $%04d|%S|%S|",j,t_old+itembar_old->devicematch->device->Driver,
                    t_old+itembar_old->devicematch->device->getDescr());
            if(itembar_old->hwidmatch)
            {
				int limits[7];
                memset(limits,0,sizeof(limits));
                Log.print_con("%d|\n",itembar_old->hwidmatch->getHWID_index());
                itembar_old->hwidmatch->print_tbl(limits);
            }
            else
                itembar_old->devicematch->device->print(manager_old->matcher->getState());

        }
    }
    Log.set_mode(0);
    Log.print_con("}Updated\n");
}
//}

//{ Popup
void Manager::popup_driverlist(Canvas &canvas,int wx,int wy,size_t i)
{
	UNREFERENCED_PARAMETER(wy);

    itembar_t *itembar;
    POINT p;
    wchar_t i_hwid[BUFLEN];
    wchar_t bufw[BUFLEN];
    int lne=D_X(POPUP_WY);
    size_t k;
    int maxsz=0;
	int limits[30];
    int c0=D_C(POPUP_TEXT_COLOR);
    textdata_horiz_t td(canvas,Popup->getShift(),limits,1);

    if(i<RES_SLOTS)return;

    td.y=D_X(POPUP_OFSY);
    td.col=0;

    size_t group=items_list[i].index;
    const Driver *cur_driver=items_list[i].devicematch->driver;
    const Txt *txt=&matcher->getState()->textas;

    memset(limits,0,sizeof(limits));

    itembar=&items_list[0];
    for(k=0;k<items_list.size();k++,itembar++)
        if(itembar->index==group&&itembar->hwidmatch)
            itembar->hwidmatch->popup_driverline(limits,canvas,td.y,0,k);


    td.TextOutBold(STR(STR_HINT_INSTDRV));

    if(cur_driver)
    {
        wsprintf(bufw,L"%s",txt->getw(cur_driver->MatchingDeviceId));
        for(k=0;bufw[k];k++)
            i_hwid[k]=(char)toupper(bufw[k]);
        i_hwid[k]=0;

        WStringShort date;
        WStringShort vers;
        cur_driver->version.str_date(date);
        cur_driver->version.str_version(vers);

        td.TextOutP(L"$%04d",i);
        td.limitskip();
        td.col=c0;
        td.TextOutP(L"| %08X",cur_driver->calc_score_h(matcher->getState()));
        td.TextOutP(L"| %s",date.Get());
        for(k=0;k<6;k++)td.limitskip();
        td.TextOutP(L"| %s%s",txt->getw(matcher->getState()->getWindir()),txt->getw(cur_driver->InfPath));
        td.TextOutP(L"| %s",txt->getw(cur_driver->ProviderName));
        td.TextOutP(L"| %s",vers.Get());
        td.TextOutP(L"| %s",i_hwid);
        td.TextOutP(L"| %s",txt->getw(cur_driver->DriverDesc));
        td.y+=lne;
    }
    else
    {
        td.TextOutF(L"%s",STR(STR_SHOW_MISSING));
    }
    td.y+=lne;
    td.ret();
    td.TextOutBold(STR(STR_HINT_AVAILDRVS));

    itembar=&items_list[0];
    for(k=0;k<items_list.size();k++,itembar++)
        if(itembar->index==group&&itembar->hwidmatch&&(itembar->first&2)==0)
    {
        if(k==i)
        {
            canvas.DrawEmptyRect(D_X(POPUP_OFSX)+Popup->getShift(),td.y,
                            wx+Popup->getShift()-D_X(POPUP_OFSX),td.y+lne,
                            D_C(POPUP_LST_SELECTED_COLOR));
        }
        itembar->hwidmatch->popup_driverline(limits,canvas,td.y,1,k);
        td.y+=lne;
    }

    RECT rect;
    GetWindowRect(GetDesktopWindow(),&rect);
    Popup->getPos(&p.x,&p.y);

    maxsz=0;
    for(k=0;k<30;k++)maxsz+=limits[k];
    if(p.x+maxsz+D_X(POPUP_OFSX)*3>rect.right)
    {
        td.y+=lne;
        td.ret();
        td.TextOutF(c0,STR(STR_HINT_SCROLL));
        td.y+=lne;
    }
    Popup->popup_resize(maxsz+D_X(POPUP_OFSX)*3,td.y+D_X(POPUP_OFSY));
}
//}
