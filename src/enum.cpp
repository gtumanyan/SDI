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
#include "indexing.h"
#include "theme.h"
#include "gui.h"
#include "draw.h" // for rtl

#include "7zip.h"
#include "device.h"
#include <windows.h>

// Depend on Win32API
#include "enum.h"
#include "main.h"

//{ Global variables

#ifdef STORE_PROPS
const dev devtbl[NUM_PROPS]=
{
/**/{SPDRP_DEVICEDESC                   ,"Devicedesc"},
/**/{SPDRP_HARDWAREID                   ,"HardwareID"},
/**/{SPDRP_COMPATIBLEIDS                ,"CompatibleIDs"},
//   SPDRP_UNUSED0           3
    {SPDRP_SERVICE                      ,"Service"},
//   SPDRP_UNUSED1           5
//   SPDRP_UNUSED2           6
    {SPDRP_CLASS                        ,"Class"},
    {SPDRP_CLASSGUID                    ,"ClassGUID"},
/**/{SPDRP_DRIVER                       ,"Driver"},
    {SPDRP_CONFIGFLAGS                  ,"ConfigFlags"},
/**/{SPDRP_MFG                          ,"Mfg"},
/**/{SPDRP_FRIENDLYNAME                 ,"FriendlyName"},
    {SPDRP_LOCATION_INFORMATION         ,"LocationInformation"},
    {SPDRP_PHYSICAL_DEVICE_OBJECT_NAME  ,"PhysicalDeviceObjectName"},
    {SPDRP_CAPABILITIES                 ,"Capabilities"},
    {SPDRP_UI_NUMBER                    ,"UINumber"},
//   SPDRP_UPPERFILTERS      17
//   SPDRP_LOWERFILTERS      18
    {SPDRP_BUSTYPEGUID                  ,"BusTypeGUID"},
    {SPDRP_LEGACYBUSTYPE                ,"LegacyBusType"},
    {SPDRP_BUSNUMBER                    ,"BusNumber"},
    {SPDRP_ENUMERATOR_NAME              ,"EnumeratorName"},
//   SPDRP_SECURITY          23
//   SPDRP_SECURITY_SDS      24
//   SPDRP_DEVTYPE           25
//   SPDRP_EXCLUSIVE         26
//   SPDRP_CHARACTERISTICS   27
    {SPDRP_ADDRESS                      ,"Address"},
//                           29
    {SPDRP_UI_NUMBER_DESC_FORMAT        ,"UINumberDescFormat"},
};
#endif // STORE_PROPS
//}

//{ Device
void Device::print_guid(const GUID *g)
{
    WString buffer;

    if(!SetupDiGetClassDescription(g,buffer.GetV(),static_cast<DWORD>(buffer.Length()),nullptr))
    {
        Log.print_file("%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",g->Data1,g->Data2,g->Data3,
            g->Data4[0],g->Data4[1],
            g->Data4[2],g->Data4[3],g->Data4[4],
            g->Data4[5],g->Data4[6],g->Data4[7]);

        unsigned lr=GetLastError();
        if(lr!=0xE0000206)Log.print_syserr(lr,L"print_guid()");
    }
    Log.print_file("%S\n",buffer.Get());
}

void Device::read_device_property(HDEVINFO hDevInfo,State *state,int id,ofst *val)
{
    DWORD buffersize=0;
    DWORD DataT=0;
    PBYTE p;
    auto DeviceInfoDataloc=(SP_DEVINFO_DATA *)&DeviceInfoData;

    *val=0;
    if(!SetupDiGetDeviceRegistryProperty(hDevInfo,DeviceInfoDataloc,id,&DataT,nullptr,0,&buffersize))
    {
        int ret_er=GetLastError();
        if(ret_er==ERROR_INVALID_DATA)return;
        if(ret_er!=ERROR_INSUFFICIENT_BUFFER)
        {
            Log.print_file("Property %d\n",id);
            Log.print_syserr(ret_er,L"read_device_property()");
            return;
        }
    }

    if(DataT==REG_DWORD)
    {
        p=(PBYTE)val;
    }
    else
    {
        *val=static_cast<ofst>(state->textas.alloc(buffersize));
        p=(PBYTE)(state->textas.get(*val));
        *p=0;
    }
    if(!SetupDiGetDeviceRegistryProperty(hDevInfo,DeviceInfoDataloc,id,&DataT,p,buffersize,&buffersize))
    {
        int ret_er=GetLastError();
        Log.print_file("Property %d\n",id);
        Log.print_syserr(ret_er,L"read_device_property()");
        return;
    }
}

int Device::print_status()
{
    int isPhantom=0;

    if(ret!=CR_SUCCESS)
    {
        if((ret==CR_NO_SUCH_DEVINST)||(ret==CR_NO_SUCH_VALUE))isPhantom=1;
    }

    if(isPhantom)
        return 0;
    else
    {
        if((status&DN_HAS_PROBLEM)&&problem==CM_PROB_DISABLED)
            return 1;
        else
        {
            if(status&DN_HAS_PROBLEM)
                return 2;
            else if(status&DN_PRIVATE_PROBLEM)
                return 3;
            else if(status&DN_STARTED)
                return 4;
            else
                return 5;
        }
    }
}

void Device::print(const State *state)
{
    static const char *deviceststus_str[]=
    {
        "Device is not present",
        "Device is disabled",
        "The device has the following problem: %d",
        "The driver reported a problem with the device",
        "Driver is running",
        "Device is currently stopped"
    };

    const Txt *txt=&state->textas;
    Log.print_file("DeviceInfo\n");
    Log.print_file("  Name:         %S\n",txt->get(Devicedesc));
    Log.print_file("  Status:       ");
    Log.print_file(deviceststus_str[print_status()],problem);
    Log.print_file("\n  Manufacturer: %S\n",txt->getw(Mfg));
    Log.print_file("  HWID_reg      %S\n",txt->getw(Driver));
    Log.print_file("  Class:        ");print_guid(&DeviceInfoData.ClassGuid);
    Log.print_file("  Location:     \n");
    Log.print_file("  ConfigFlags:  %d\n",ConfigFlags);
    Log.print_file("  Capabilities: %d\n",Capabilities);
}

void Device::printHWIDS(const State *state)
{
    if(HardwareID)
    {
        const wchar_t *p=state->textas.getw(HardwareID);
        Log.print_file("HardwareID\n");
        while(*p)
        {
            Log.print_file("  %S\n",p);
            p+=wcslen(p)+1;
        }
    }
    else
    {
        Log.print_file("NoID\n");
    }

    if(CompatibleIDs)
    {
        const wchar_t *p=state->textas.getw(CompatibleIDs);
        Log.print_file("CompatibleID\n");
        while(*p)
        {
            Log.print_file("  %S\n",p);
            p+=wcslen(p)+1;
        }
    }
}

void Device::getClassDesc(wchar_t *bufw)
{
    SetupDiGetClassDescription(&DeviceInfoData.ClassGuid,bufw,BUFLEN,nullptr);
}

const wchar_t *Device::getHWIDby(int num,const State *state)
{
    int i=0;

    if(HardwareID)
    {
        const wchar_t *p=state->textas.getw(HardwareID);
        while(*p)
        {
            if(i==num)return p;
            p+=wcslen(p)+1;
            i++;
        }
    }
    if(CompatibleIDs)
    {
        const wchar_t *p=state->textas.getw(CompatibleIDs);
        while(*p)
        {
            if(i==num)return p;
            p+=wcslen(p)+1;
            i++;
        }
    }
    return L"";
}

Device::Device(State *state):
driver_index(-1),Devicedesc(0),HardwareID(0),CompatibleIDs(0),Driver(0),
        Mfg(0),FriendlyName(0),Capabilities(0),ConfigFlags(0),
        InstanceId(0),status(0),problem(0),ret(0)
{
    wchar_t buf[BUFLEN];

    //wsprintf(buf,L"%S",ex.GetHWID());
    //Log.print_con("Fake '%S'\n",buf);
    buf[wcslen(buf)+2]=0;
    problem=2;
    HardwareID=static_cast<ofst>(state->textas.t_memcpy((char *)buf,wcslen(buf)*2+4));
}

Device::Device(HDEVINFO hDevInfo,State *state,int i)
{
    DWORD buffersize;
    SP_DEVINFO_DATA *DeviceInfoDataloc;

    DeviceInfoDataloc=(SP_DEVINFO_DATA *)&DeviceInfoData;
    memset(&DeviceInfoData,0,sizeof(SP_DEVINFO_DATA));
    DeviceInfoData.cbSize=sizeof(SP_DEVINFO_DATA);

    driver_index=-1;
    if(!SetupDiEnumDeviceInfo(hDevInfo,i,DeviceInfoDataloc))
    {
        ret=GetLastError();
        return;
    }

    SetupDiGetDeviceInstanceId(hDevInfo,DeviceInfoDataloc,nullptr,0,&buffersize);
    InstanceId=static_cast<ofst>(state->textas.alloc(buffersize));
    SetupDiGetDeviceInstanceId(hDevInfo,DeviceInfoDataloc,const_cast<wchar_t *>(state->textas.getw(InstanceId)),buffersize,nullptr);

    read_device_property(hDevInfo,state,SPDRP_DEVICEDESC,    &Devicedesc);
    read_device_property(hDevInfo,state,SPDRP_HARDWAREID,    &HardwareID);
    read_device_property(hDevInfo,state,SPDRP_COMPATIBLEIDS, &CompatibleIDs);
    read_device_property(hDevInfo,state,SPDRP_DRIVER,        &Driver);
    read_device_property(hDevInfo,state,SPDRP_MFG,           &Mfg);
    read_device_property(hDevInfo,state,SPDRP_FRIENDLYNAME,  &FriendlyName);
    read_device_property(hDevInfo,state,SPDRP_CAPABILITIES,  &Capabilities);
    read_device_property(hDevInfo,state,SPDRP_CONFIGFLAGS,   &ConfigFlags);

    ret=CM_Get_DevNode_Status(&status,&problem,DeviceInfoDataloc->DevInst,0);
    if(ret!=CR_SUCCESS)
    {
        Log.print_err("ERROR %d with CM_Get_DevNode_Status()\n",ret);
    }
}
//}

//{ Driver
void Driver::read_reg_val(HKEY hkey,State *state,const wchar_t *key,ofst *val)
{
    DWORD dwType,dwSize=0;
    int lr;

    *val=0;
    lr=RegQueryValueEx(hkey,key,nullptr,nullptr,nullptr,&dwSize);
    if(lr==ERROR_FILE_NOT_FOUND)return;
    if(lr!=ERROR_SUCCESS)
    {
        Log.print_err("Key %S\n",key);
        Log.print_syserr(lr,L"RegQueryValueEx()");
        return;
    }

    *val=static_cast<ofst>(state->textas.alloc(dwSize));
    lr=RegQueryValueEx(hkey,key,nullptr,&dwType,(unsigned char*)state->textas.get(*val),&dwSize);
    if(lr!=ERROR_SUCCESS)
    {
        Log.print_err("Key %S\n",key);
        Log.print_syserr(lr,L"read_reg_val()");
    }
}

void Driver::scaninf(State *state,Driverpack *unpacked_drp,int &inf_pos)
{
    auto inf_list=&state->inf_list_new;
    unsigned start_index=0;

    if(Settings.flags&FLAG_FAILSAFE)
    {
        inf_pos=0;
        return;
    }

    WStringShort filename;
    WStringShort fnm_hwid;
    filename.sprintf(L"%s%s",state->textas.get(state->getWindir()),state->textas.get(InfPath));
    fnm_hwid.sprintf(L"%s%s",filename.Get(),state->textas.get(MatchingDeviceId));

    auto got=inf_list->find(std::wstring(fnm_hwid.Get()));
    if(got!=inf_list->end())
    {
        infdata_t *infdata=&got->second;
        //log_file("Match_hwid '%S' %d,%d,%d,%d\n",fnm_hwid.Get(),infdata->feature,infdata->catalogfile,infdata->cat,infdata->inf_pos);
        feature=infdata->feature;
        catalogfile=infdata->catalogfile;
        cat=infdata->cat;
        inf_pos=infdata->inf_pos;
        return;
    }

    got=inf_list->find(std::wstring(filename.Get()));
    if(got!=inf_list->end())
    {
        infdata_t *infdata=&got->second;
        cat=infdata->cat;
        catalogfile=infdata->catalogfile;
        start_index=infdata->start_index;
        //log_file("Match_inf  '%S',%d,%d\n",filename.Get(),cat,catalogfile);
    }
    else
    {
        FILE *f;
        size_t len;

        //log_file("Reading '%S' for (%S)\n",filename.Get(),state->textas.get(MatchingDeviceId));
        f=_wfopen(filename.Get(),L"rb");
        if(!f)
        {
            Log.print_err("ERROR: file not found '%S'\n",filename.Get());
            return;
        }
        _fseeki64(f,0,SEEK_END);
        len=static_cast<size_t>(_ftelli64(f));
        _fseeki64(f,0,SEEK_SET);
        std::unique_ptr<char[]> buft(new char[len]);
        fread(buft.get(),len,1,f);
        fclose(f);

        if(len>0)
        {
            start_index=unpacked_drp->getSize();
            unpacked_drp->indexinf(state->textas.getw(state->getWindir()),state->textas.getw(InfPath),buft.get(),len);
        }

        cat=static_cast<ofst>(state->opencatfile(this));
    }

    char sect[BUFLEN];
    char hwid[BUFLEN];
    inf_pos=-1;
    wsprintfA(sect,"%ws%ws",state->textas.getw(InfSection),state->textas.getw(InfSectionExt));
    wsprintfA(hwid,"%ws",state->textas.getw(MatchingDeviceId));
    unpacked_drp->fillinfo(sect,hwid,start_index,&inf_pos,&cat,&catalogfile,&feature);

    //log_file("Added  %d,%d,%d,%d\n",feature,catalogfile,cat,inf_pos);
    inf_list->insert({std::wstring(fnm_hwid.Get()),infdata_t(catalogfile,feature,inf_pos,cat,start_index)});
    inf_list->insert({std::wstring(filename.Get()),infdata_t(catalogfile,0,0,cat,start_index)});
}

int Driver::findHWID_in_list(const wchar_t *p,const wchar_t *str)
{
    int dev_pos=0;
    while(*p)
    {
        if(!_wcsicmp(p,str))return dev_pos;
        p+=wcslen(p)+1;
        dev_pos++;
    }
    return -1;
}

void Driver::calc_dev_pos(const Device *cur_device,const State *state,int *ishw,int *dev_pos)
{
    *ishw=1;
    *dev_pos=findHWID_in_list(state->textas.getw(cur_device->getHardwareID()),state->textas.getw(MatchingDeviceId));
    if(*dev_pos<0&&cur_device->getCompatibleIDs())
    {
        *ishw=0;
        *dev_pos=findHWID_in_list(state->textas.getw(cur_device->getCompatibleIDs()),state->textas.getw(MatchingDeviceId));
    }
}

int calc_signature(int catalogfile,const State *state,int isnt)
{
    if(state->getArchitecture())
    {
        if(catalogfile&(1<<CatalogFile|1<<CatalogFile_nt|1<<CatalogFile_ntamd64|1<<CatalogFile_ntia64))
            return 0;
    }
    else
    {
        if(catalogfile&(1<<CatalogFile|1<<CatalogFile_nt|1<<CatalogFile_ntx86))
            return 0;
    }
    if(isnt)return 0x8000;
    return 0xC000;
}

unsigned calc_score(int catalogfile,int feature,int rank,const State *state,int isnt)
{
    int major,minor;
    state->getWinVer(&major,&minor);
    if(major>=6)
        return (calc_signature(catalogfile,state,isnt)<<16)+(feature<<16)+rank;
    else
        return calc_signature(catalogfile,state,isnt)+rank;
}

unsigned Driver::calc_score_h(const State *state)const
{
    return calc_score(catalogfile,feature,identifierscore, // TODO: check signature
                      state,StrStrIW(state->textas.getw(InfSectionExt),L".nt")||StrStrIW(state->textas.getw(InfSection),L".nt")?1:0);
}

int Driver::isvalidcat(const State *state)const
{
    char bufa[BUFLEN];
    if(!cat)return 0;
    const char *s=state->textas.get(cat);

    int major,minor;
    state->getWinVer(&major,&minor);
    wsprintfA(bufa,"2:%d.%d",major,minor);
    if(!*s)return 0;
    return strstr(s,bufa)?1:0;
}

void Driver::print(const State *state)const
{
    const Txt *txt=&state->textas;
    WStringShort date;
    WStringShort vers;

    version.str_date(date);
    version.str_version(vers);
    Log.print_file("  Name:     %S\n",txt->getw(DriverDesc));
    Log.print_file("  Provider: %S\n",txt->getw(ProviderName));
    Log.print_file("  Date:     %S\n",date.Get());
    Log.print_file("  Version:  %S\n",vers.Get());
    Log.print_file("  HWID:     %S\n",txt->getw(MatchingDeviceId));
    Log.print_file("  inf:      %S%S,%S%S\n",txt->getw(state->getWindir()),txt->getw(InfPath),txt->getw(InfSection),txt->getw(InfSectionExt));
    Log.print_file("  Score:    %08X %04x\n",calc_score_h(state),identifierscore);
    //Log.print_file("  Sign:     '%s'(%d)\n",txt->get(cat),catalogfile);

    if(Log.isAllowed(LOG_VERBOSE_BATCH))
        Log.print_file("  Filter:   \"%S\"=a,%S\n",txt->getw(DriverDesc),txt->getw(MatchingDeviceId));
}

int calc_identifierscore(int dev_pos,int dev_ishw,int inf_pos)
{
    if(dev_ishw&&inf_pos==0)    // device hardware ID and a hardware ID in an INF
        return 0x0000+dev_pos;

    if(dev_ishw)                // device hardware ID and a compatible ID in an INF
        return 0x1000+dev_pos+0x100*inf_pos;

    if(inf_pos==0)              // device compatible ID and a hardware ID in an INF
        return 0x2000+dev_pos;

                                // device compatible ID and a compatible ID in an INF
    return 0x3000+dev_pos+0x100*inf_pos;
}

Driver::Driver(State *state,Device *cur_device,HKEY hkey,Driverpack *unpacked_drp)
{
    char bufa[BUFLEN];
    int dev_pos,ishw,inf_pos=-1;
    DriverDate=0;
    DriverVersion=0;
    cat=0;

    read_reg_val(hkey,state,L"DriverDesc",         &DriverDesc);
    read_reg_val(hkey,state,L"ProviderName",       &ProviderName);
    read_reg_val(hkey,state,L"DriverDate",         &DriverDate);
    read_reg_val(hkey,state,L"DriverVersion",      &DriverVersion);
    read_reg_val(hkey,state,L"MatchingDeviceId",   &MatchingDeviceId);

    read_reg_val(hkey,state,L"InfPath",            &InfPath);
    read_reg_val(hkey,state,L"InfSection",         &InfSection);
    read_reg_val(hkey,state,L"InfSectionExt",      &InfSectionExt);

    calc_dev_pos(cur_device,state,&ishw,&dev_pos);

    if(InfPath)
        scaninf(state,unpacked_drp,inf_pos);

    identifierscore=calc_identifierscore(dev_pos,ishw,inf_pos);
    //log_file("%d,%d,%d,(%x)\n",dev_pos,ishw,inf_pos,identifierscore);

    if(DriverDate)
    {
        wsprintfA(bufa,"%ws",state->textas.getw(DriverDate));
        Parser pi{bufa,bufa+strlen(bufa)};
        pi.readDate(&version);
    }

    if(DriverVersion)
    {
        wsprintfA(bufa,"%ws",state->textas.getw(DriverVersion));
        Parser pi{bufa,bufa+strlen(bufa)};
        pi.readVersion(&version);
    }
}
//}

//{ State
void State::fakeOSversion()
{
    if(Settings.virtual_arch_type==32)architecture=0;
    if(Settings.virtual_arch_type==64)architecture=1;
    // virtual_os_version holds the index into the versions array+ID_OS_ITEMS
    if(Settings.virtual_os_version)
    {
        int ver=winVersions.GetEntry(Settings.virtual_os_version-ID_OS_ITEMS);
        bool serv=winVersions.GetEntryServer(Settings.virtual_os_version-ID_OS_ITEMS);
        platform.dwMajorVersion=ver/10;
        platform.dwMinorVersion=ver%10;
        if(serv)platform.wProductType=3;
        else platform.wProductType=1;
    }
}

void State::getWinVer(int *major,int *minor)const
{
    *major=platform.dwMajorVersion;
    *minor=platform.dwMinorVersion;
}

const wchar_t *State::getProduct()
{
    const wchar_t *s=textas.getw(product);

    if(StrStrIW(s,L"Product"))return textas.getw(cs_model);
    return s;
}

const wchar_t *State::getManuf()
{
    const wchar_t *s=textas.getw(manuf);

    if(StrStrIW(s,L"Vendor")||StrStrIW(s,L"Quanta"))return textas.getw(cs_manuf);
    return s;
}

const wchar_t *State::getModel()
{
    const wchar_t *s=textas.getw(model);

    if(!*s)return textas.getw(cs_model);
    return s;
}

int State::getPlatformProductType()
{
    return platform.wProductType;
}

State::State():
    locale(0),
    architecture(0),

    manuf(0),
    model(0),
    product(0),
    monitors(0),
    battery(0),

    windir(0),
    temp(0),

    cs_manuf(0),
    cs_model(0),
    ChassisType(0),
    revision(GIT_REV)
{
    memset(this,0,sizeof(state_m_t));
    revision=GIT_REV;

    //Log.print_con("sizeof(Device)=%d\nsizeof(Driver)=%d\n\n",sizeof(Device),sizeof(Driver));
}

void State::print()
{
    unsigned i;
    wchar_t *buf;
    SYSTEM_POWER_STATUS *batteryloc;

    if(Log.isAllowed(LOG_VERBOSE_SYSINFO|LOG_VERBOSE_BATCH))
    {
        Log.print_file("%S (%d.%d.%d), ",get_winverstr(),platform.dwMajorVersion,platform.dwMinorVersion,platform.dwBuildNumber);
        Log.print_file("%s\n",architecture?"64-bit":"32-bit");
        Log.print_file("%s, ",isLaptop?"Laptop":"Desktop");
        Log.print_file("Product='%S', ",textas.getw(product));
        Log.print_file("Model='%S', ",textas.get(model));
        Log.print_file("Manuf='%S'\n",textas.get(manuf));
    }else
    if(Log.isAllowed(LOG_VERBOSE_SYSINFO))
    {
        Log.print_file("Windows\n");
        Log.print_file("  Version:     %S (%d.%d.%d)\n",get_winverstr(),platform.dwMajorVersion,platform.dwMinorVersion,platform.dwBuildNumber);
        Log.print_file("  PlatformId:  %d\n",platform.dwPlatformId);
        Log.print_file("  Update:      %S\n",platform.szCSDVersion);
        if(platform.dwOSVersionInfoSize == sizeof(OSVERSIONINFOEX))
        {
            Log.print_file("  ServicePack: %d.%d\n",platform.wServicePackMajor,platform.wServicePackMinor);
            Log.print_file("  SuiteMask:   %d\n",platform.wSuiteMask);
            Log.print_file("  ProductType: %d\n",platform.wProductType);
        }
        Log.print_file("\nEnvironment\n");
        Log.print_file("  windir:      %S\n",textas.get(windir));
        Log.print_file("  temp:        %S\n",textas.get(temp));

        Log.print_file("\nMotherboard\n");
        Log.print_file("  Product:     %S\n",textas.get(product));
        Log.print_file("  Model:       %S\n",textas.get(model));
        Log.print_file("  Manuf:       %S\n",textas.get(manuf));
        Log.print_file("  cs_Model:    %S\n",textas.get(cs_model));
        Log.print_file("  cs_Manuf:    %S\n",textas.get(cs_manuf));
        Log.print_file("  Chassis:     %d\n",ChassisType);

        Log.print_file("\nBattery\n");
        batteryloc=(SYSTEM_POWER_STATUS *)(textas.get(battery));
        Log.print_file("  AC_Status:   ");
        switch(batteryloc->ACLineStatus)
        {
            case 0:Log.print_file("Offline\n");break;
            case 1:Log.print_file("Online\n");break;
            default:
            case 255:Log.print_file("Unknown\n");break;
        }
        i=batteryloc->BatteryFlag;
        Log.print_file("  Flags:       %d",i);
        if(i&1)Log.print_file("[high]");
        if(i&2)Log.print_file("[low]");
        if(i&4)Log.print_file("[critical]");
        if(i&8)Log.print_file("[charging]");
        if(i&128)Log.print_file("[no battery]");
        if(i==255)Log.print_file("[unknown]");
        Log.print_file("\n");
        if(batteryloc->BatteryLifePercent!=255)
            Log.print_file("  Charged:      %d\n",batteryloc->BatteryLifePercent);
        if(batteryloc->BatteryLifeTime!=static_cast<DWORD>(-1))
            Log.print_file("  LifeTime:     %d mins\n",batteryloc->BatteryLifeTime/60);
        if(batteryloc->BatteryFullLifeTime!=static_cast<DWORD>(-1))
            Log.print_file("  FullLifeTime: %d mins\n",batteryloc->BatteryFullLifeTime/60);

        buf=textas.getwV(monitors);
        Log.print_file("\nMonitors\n");
        for(i=0;i<buf[0];i++)
        {
            int x=buf[1+i*2];
            int y=buf[2+i*2];
            Log.print_file("  %dcmx%dcm (%.1fin)\t%.3f %s\n",x,y,sqrt(x*x+y*y)/2.54,(double)y/x,iswide(x,y)?"wide":"");
        }

        Log.print_file("\nMisc\n");
        Log.print_file("  Type:        %s\n",isLaptop?"Laptop":"Desktop");
        Log.print_file("  Locale:      %X\n",locale);
        Log.print_file("  CPU_Arch:    %s\n",architecture?"64-bit":"32-bit");
        Log.print_file("\n");
    }

    if(Log.isAllowed(LOG_VERBOSE_DEVICES))
    for(auto &cur_device:Devices_list)
    {
        cur_device.print(this);

        Log.print_file("DriverInfo\n");
        if(cur_device.getDriverIndex()>=0)
            Drivers_list[cur_device.getDriverIndex()].print(this);
        else
            Log.print_file("  NoDriver\n");

        cur_device.printHWIDS(this);
        Log.print_file("\n\n");
    }

    //Log.print_con("State: %d+%d+%d*%d+%d*%d\n",sizeof(State),textas.getSize(),Devices_list.size(),sizeof(Device),Drivers_list.size(),sizeof(Driver));
    //log_file("Errors: %d\n",error_count);
}

void State::popup_sysinfo(Canvas &canvas)
{
    wchar_t bufw[BUFLEN];
    int i;
    int p0=D_X(POPUP_OFSX),p1=D_X(POPUP_OFSX)+10;

    textdata_vert td(canvas);
    td.ret();
    td.TextOutBold(STR(STR_SYSINF_WINDOWS));
    td.ret_ofs(10);

    td.TextOutSF(STR(STR_SYSINF_VERSION),L"%s (%d.%d.%d)%s",get_winverstr(),platform.dwMajorVersion,platform.dwMinorVersion,platform.dwBuildNumber,rtl?L"\u200E":L"");
    td.TextOutSF(STR(STR_SYSINF_UPDATE),L"%s",platform.szCSDVersion);
    td.TextOutSF(STR(STR_SYSINF_CPU_ARCH),L"%s",architecture?STR(STR_SYSINF_64BIT):STR(STR_SYSINF_32BIT));
    td.TextOutSF(STR(STR_SYSINF_LOCALE),L"%X",locale);
    //TextOutSF(STR(STR_SYSINF_PLATFORM),L"%d",platform.dwPlatformId);
    /*if(platform.dwOSVersionInfoSize == sizeof(OSVERSIONINFOEX))
    {
        td.TextOutSF(STR(STR_SYSINF_SERVICEPACK),L"%d.%d",platform.wServicePackMajor,platform.wServicePackMinor);
        td.TextOutSF(STR(STR_SYSINF_SUITEMASK),L"%d",platform.wSuiteMask);
        td.TextOutSF(STR(STR_SYSINF_PRODUCTTYPE),L"%d",platform.wProductType);
    }*/
    td.ret();
    td.TextOutBold(STR(STR_SYSINF_ENVIRONMENT));
    td.ret_ofs(10);
    td.TextOutSF(STR(STR_SYSINF_WINDIR),L"%s%s",textas.get(windir),rtl?L"\u200E":L"");
    td.TextOutSF(STR(STR_SYSINF_TEMP),L"%s%s",textas.get(temp),rtl?L"\u200E":L"");

    td.ret();
    td.TextOutBold(STR(STR_SYSINF_MOTHERBOARD));
    td.ret_ofs(10);
    td.TextOutSF(STR(STR_SYSINF_PRODUCT),L"%s%s",getProduct(),rtl?L"\u200E":L"");
    td.TextOutSF(STR(STR_SYSINF_MODEL),L"%s",getModel(),rtl?L"\u200E":L"");
    td.TextOutSF(STR(STR_SYSINF_MANUF),L"%s",getManuf());
    td.TextOutSF(STR(STR_SYSINF_TYPE),L"%s[%d]",isLaptop?STR(STR_SYSINF_LAPTOP):STR(STR_SYSINF_DESKTOP),ChassisType);

    td.ret();
    td.TextOutBold(STR(STR_SYSINF_BATTERY));
    td.ret_ofs(10);
    SYSTEM_POWER_STATUS *battery_loc=(SYSTEM_POWER_STATUS *)(textas.get(battery));
    switch(battery_loc->ACLineStatus)
    {
        case 0:wcscpy(bufw,STR(STR_SYSINF_OFFLINE));break;
        case 1:wcscpy(bufw,STR(STR_SYSINF_ONLINE));break;
        default:
        case 255:wcscpy(bufw,STR(STR_SYSINF_UNKNOWN));break;
    }
    td.TextOutSF(STR(STR_SYSINF_AC_STATUS),L"%s",bufw);

    i=battery_loc->BatteryFlag;
    *bufw=0;
    if(i&1)wcscat(bufw,STR(STR_SYSINF_HIGH));
    if(i&2)wcscat(bufw,STR(STR_SYSINF_LOW));
    if(i&4)wcscat(bufw,STR(STR_SYSINF_CRITICAL));
    if(i&8)wcscat(bufw,STR(STR_SYSINF_CHARGING));
    if(i&128)wcscat(bufw,STR(STR_SYSINF_NOBATTERY));
    if(i==255)wcscat(bufw,STR(STR_SYSINF_UNKNOWN));
    td.TextOutSF(STR(STR_SYSINF_FLAGS),L"%s",bufw);

    if(battery_loc->BatteryLifePercent!=255)
        td.TextOutSF(STR(STR_SYSINF_CHARGED),L"%d%%",battery_loc->BatteryLifePercent);
    if(battery_loc->BatteryLifeTime!=static_cast<DWORD>(-1))
        td.TextOutSF(STR(STR_SYSINF_LIFETIME),L"%d %s",battery_loc->BatteryLifeTime/60,STR(STR_SYSINF_MINS));
    if(battery_loc->BatteryFullLifeTime!=static_cast<DWORD>(-1))
        td.TextOutSF(STR(STR_SYSINF_FULLLIFETIME),L"%d %s",battery_loc->BatteryFullLifeTime/60,STR(STR_SYSINF_MINS));

    wchar_t *buf=textas.getwV(monitors);
    td.ret();
    td.TextOutBold(STR(STR_SYSINF_MONITORS));
    td.ret_ofs(10);
    for(i=0;i<buf[0];i++)
    {
        int x,y;
        x=buf[1+i*2];
        y=buf[2+i*2];
        td.shift_r();
        td.TextOutF(L"%d%s x %d%s (%.1f %s) %.3f %s",
                    x,STR(STR_SYSINF_CM),
                    y,STR(STR_SYSINF_CM),
                    sqrt(x*x+y*y)/2.54,STR(STR_SYSINF_INCH),
                    (double)y/x,
                    iswide(x,y)?STR(STR_SYSINF_WIDE):L"");

        td.shift_l();
    }

    td.ret();
    td.shift_r();
    td.nl();
    td.TextOutF(D_C(POPUP_CMP_BETTER_COLOR),STR(STR_SYSINF_MISC));
    td.ret_ofs(10);
    td.shift_l();
    Popup->popup_resize((td.getMaxsz()+POPUP_SYSINFO_OFS+p0+p1),td.getY()+D_X(POPUP_OFSY));
}

void State::contextmenu2(int x,int y)
{
    HMENU hPopupMenu=CreatePopupMenu();
    HMENU hSub1=CreatePopupMenu();
    // find the version array index for the current platform
    int ver=platform.dwMinorVersion+10*platform.dwMajorVersion;
    bool serv=platform.wProductType==2||platform.wProductType==3;
    int veridx=winVersions.GetVersionIndex(ver,serv);

    // create a menu item for each entry in the version array
    // and checkmark the current platform
    for(int i=0;i<winVersions.Count();i++)
    {
        // storing the version array index in the id of the menu item
        InsertMenu(hSub1,i,MF_BYPOSITION|MF_STRING|(i==veridx?MF_CHECKED:0),
                   ID_OS_ITEMS+i,winVersions.GetEntryW(i));
    }

    int i=0;
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)hSub1,STR(STR_SYS_WINVER));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|(architecture==0?MF_CHECKED:0),ID_EMU_32,STR(STR_SYS_32));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|(architecture==1?MF_CHECKED:0),ID_EMU_64,STR(STR_SYS_64));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_SEPARATOR,0,nullptr);
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING,ID_DEVICEMNG,STR(STR_SYS_DEVICEMNG));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_SEPARATOR,0,nullptr);
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|((Settings.flags&FLAG_DISABLEINSTALL)?MF_CHECKED:0),ID_DIS_INSTALL,STR(STR_SYS_DISINSTALL));
    InsertMenu(hPopupMenu,i++,MF_BYPOSITION|MF_STRING|((Settings.flags&FLAG_NORESTOREPOINT)?MF_CHECKED:0),ID_DIS_RESTPNT,STR(STR_SYS_DISRESTPNT));

    RECT rect;
    SetForegroundWindow(MainWindow.hMain);
    if(rtl)x=MainWindow.main1x_c-x;
    GetWindowRect(MainWindow.hMain,&rect);
    TrackPopupMenu(hPopupMenu,TPM_LEFTALIGN,rect.left+x,rect.top+y,0,MainWindow.hMain,nullptr);
}

int State::save(const wchar_t *filename)
{
    FILE *f;
    size_t sz;
    int version=VER_STATE;

    if(Settings.flags&FLAG_NOSNAPSHOT)return 0;
    Log.print_con("Saving state in '%S'...",filename);
    if(!System.canWriteFile(filename,L"wb"))
    {
        Log.print_err("ERROR in state_save(): Write-protected,'%S'\n",filename);
        return 1;
    }
    f=_wfopen(filename,L"wb");
    if(!f)
    {
        Log.print_err("ERROR in state_save(): failed _wfopen(%S)\n",errno_str());
        return 1;
    }

    sz=
        sizeof(state_m_t)+
        Drivers_list.size()*sizeof(Driver)+
        Devices_list.size()*sizeof(Device)+
        textas.getSize()+
        2*3*sizeof(int);  // 3 heaps

    std::unique_ptr<char[]> mem(new char[sz]);
    char *p=mem.get();

    fwrite(VER_MARKER,3,1,f);
    fwrite(&version,sizeof(int),1,f);

    memcpy(p,this,sizeof(state_m_t));p+=sizeof(state_m_t);
    p=Devices_list.savedata(p);
    p=Drivers_list.savedata(p);
    p=textas.savedata(p);

    //if(1)
    {
        std::unique_ptr<char[]> mem_pack(new char[sz]);
        sz=encode(mem_pack.get(),sz,mem.get(),sz);
        fwrite(mem_pack.get(),sz,1,f);
    }
    //else fwrite(mem.get(),sz,1,f);

    fclose(f);
    Log.print_con("OK\n");
    return 0;
}

int  State::load(const wchar_t *filename)
{
    char buf[BUFLEN];
    FILE *f;
    size_t sz;
    int version;

    Log.print_con("Loading state from '%S'...",filename);
    f=_wfopen(filename,L"rb");
    if(!f)
    {
        Log.print_err("ERROR in State::load(): failed _wfopen(%S)\n",errno_str());
        return 0;
    }

    _fseeki64(f,0,SEEK_END);
    sz=static_cast<size_t>(_ftelli64(f));
    _fseeki64(f,0,SEEK_SET);

    fread(buf,3,1,f);
    fread(&version,sizeof(int),1,f);
    sz-=3+sizeof(int);

    if(memcmp(buf,VER_MARKER,3)!=0)
    {
        Log.print_err("ERROR in State::load(): invalid snapshot\n");
        return 0;
    }
    if(version!=VER_STATE)
    {
        Log.print_err("ERROR in State::load(): invalid version(%d)\n",version);
        return 0;
    }

    std::unique_ptr<char[]> mem(new char[sz]);
    char *p=mem.get();
    fread(mem.get(),sz,1,f);

    size_t sz_unpack;
    UInt64 val;
    Lzma86_GetUnpackSize((Byte *)p,sz,&val);
    sz_unpack=(size_t)val;
    std::unique_ptr<char[]> mem_unpack(new char[sz_unpack]);
    decode(mem_unpack.get(),sz_unpack,mem.get(),sz);
    p=mem_unpack.get();

    memcpy(this,p,sizeof(state_m_t));p+=sizeof(state_m_t);
    p=Devices_list.loaddata(p);
    p=Drivers_list.loaddata(p);
    p=textas.loaddata(p);

    fakeOSversion();

    fclose(f);
    Log.print_con("OK\n");
    return 1;
}

void State::getsysinfo_fast()
{
    Log.print_debug("State::getsysinfo_fast\n");
    wchar_t buf[BUFLEN];

    // Battery
    Log.print_debug("State::getsysinfo_fast::GetSystemPowerStatus\n");
    battery=static_cast<ofst>(textas.alloc(sizeof(SYSTEM_POWER_STATUS)));
    SYSTEM_POWER_STATUS *batteryloc=(SYSTEM_POWER_STATUS *)(textas.get(battery));
    GetSystemPowerStatus(batteryloc);

    // Monitors
    Log.print_debug("State::getsysinfo_fast::Monitors\n");
    DISPLAY_DEVICE DispDev;
    memset(&DispDev,0,sizeof(DispDev));
    DispDev.cb=sizeof(DispDev);
    buf[0]=0;
    int i=0;
    while(EnumDisplayDevices(nullptr,i,&DispDev,0))
    {
        int x,y;
        GetMonitorSizeFromEDID(DispDev.DeviceName,&x,&y);
        if(x&&y)
        {
            buf[buf[0]*2+1]=(short)x;
            buf[buf[0]*2+2]=(short)y;
            buf[0]++;
        }
        i++;
    }
    monitors=static_cast<ofst>(textas.t_memcpy((char *)buf,(1+buf[0]*2)*2));

    // Windows version
    Log.print_debug("State::getsysinfo_fast::Windows\n");
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
    platform.dwOSVersionInfoSize=sizeof(OSVERSIONINFOEX);
    if(!(GetVersionEx((OSVERSIONINFO*)&platform)))
    {
        platform.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
        if(!GetVersionEx((OSVERSIONINFO*)&platform))
            Log.print_syserr(GetLastError(),L"GetVersionEx()");
    }
    locale=GetUserDefaultLCID();
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    // Environment
    Log.print_debug("State::getsysinfo_fast::Environment\n");
    GetEnvironmentVariable(L"windir",buf,BUFLEN);
    wcscat(buf,L"\\inf\\");
    windir=static_cast<ofst>(textas.strcpyw(buf));

    // get the system drive
    wchar_t systemDrive[BUFLEN]={0};
    GetEnvironmentVariable(L"SystemDrive",systemDrive,BUFLEN);
    wcscat(systemDrive,L"\\temp");

    // temp directory
    GetEnvironmentVariable(L"TEMP",buf,BUFLEN);

    // if the TEMP environment variable is not set then use the system drive
    if(wcslen(buf)==0)
        wcscpy(buf,systemDrive);

    temp=static_cast<ofst>(textas.strcpyw(buf));

    // 64-bit detection
    Log.print_debug("State::getsysinfo_fast::Architecture\n");
    architecture=0;
    *buf=0;
    GetEnvironmentVariable(L"PROCESSOR_ARCHITECTURE",buf,BUFLEN);
    if(!lstrcmpi(buf,L"AMD64"))architecture=1;
    *buf=0;
    GetEnvironmentVariable(L"PROCESSOR_ARCHITEW6432",buf,BUFLEN);
    if(*buf)architecture=1;

    fakeOSversion();
}

void State::getsysinfo_slow()
{
    Log.print_debug("State::getsysinfo_slow\n");
    WStringShort smanuf;
    WStringShort smodel;
    WStringShort sproduct;
    WStringShort scs_manuf;
    WStringShort scs_model;

    Timers.start(time_sysinfo);

    Log.print_debug("State::getsysinfo_slow1::getbaseboard\n");
    getbaseboard(smanuf,smodel,sproduct,scs_manuf,scs_model,&ChassisType);

    manuf=static_cast<ofst>(textas.strcpyw(smanuf.Get()));
    product=static_cast<ofst>(textas.strcpyw(sproduct.Get()));
    model=static_cast<ofst>(textas.strcpyw(smodel.Get()));
    cs_manuf=static_cast<ofst>(textas.strcpyw(scs_manuf.Get()));
    cs_model=static_cast<ofst>(textas.strcpyw(scs_model.Get()));

    Log.print_debug("State::getsysinfo_slow1::getbaseboard::manuf::%S\n",smanuf.Get());
    Log.print_debug("State::getsysinfo_slow1::getbaseboard::product::%S\n",sproduct.Get());
    Log.print_debug("State::getsysinfo_slow1::getbaseboard::model::%S\n",smodel.Get());
    Log.print_debug("State::getsysinfo_slow1::getbaseboard::cs_manuf::%S\n",scs_manuf.Get());
    Log.print_debug("State::getsysinfo_slow1::getbaseboard::cs_model::%S\n",scs_model.Get());

    Timers.stop(time_sysinfo);
    Log.print_debug("State::getsysinfo_slow1::Done\n");
}

void State::getsysinfo_slow(const State *prev)
{
    Log.print_debug("State::getsysinfo_slow2\n");
    Timers.reset(time_sysinfo);
    manuf=static_cast<ofst>(textas.strcpyw(prev->textas.getw(prev->manuf)));
    product=static_cast<ofst>(textas.strcpyw(prev->textas.getw(prev->product)));
    model=static_cast<ofst>(textas.strcpyw(prev->textas.getw(prev->model)));
    cs_manuf=static_cast<ofst>(textas.strcpyw(prev->textas.getw(prev->cs_manuf)));
    cs_model=static_cast<ofst>(textas.strcpyw(prev->textas.getw(prev->cs_model)));
}

void State::scanDevices()
{
    Log.print_debug("State::scanDevices\n");
    HDEVINFO hDevInfo;
    HKEY   hkey;
    wchar_t buf[BUFLEN];
    Collection collection{textas.getw(windir),L"",L""};
    Driverpack unpacked_drp{L"",L"windir.7z",&collection};

    Timers.start(time_devicescan);
    //collection.init(textas.getw(windir),L"",L"");

    Log.print_debug("State::scanDevices::SetupDiGetClassDevs\n");
    hDevInfo=SetupDiGetClassDevs(nullptr,nullptr,nullptr,DIGCF_PRESENT|DIGCF_ALLCLASSES);
    if(hDevInfo==INVALID_HANDLE_VALUE)
    {
        Log.print_syserr(GetLastError(),L"SetupDiGetClassDevs()");
        return;
    }

    unsigned DeviceCount=0;
    for(unsigned i=0;;i++)
    {
        // Device
        Devices_list.emplace_back((Device(hDevInfo,this,i)));
        Device *cur_device=&Devices_list.back();

        int ret=cur_device->getRet();
        if(ret)
        {
            Devices_list.pop_back();
            if(ret==ERROR_NO_MORE_ITEMS)
                break;
            else
                continue;
        }

        // Driver
        if(!cur_device->getDriver())continue;
        wsprintf(buf,L"SYSTEM\\CurrentControlSet\\Control\\Class\\%s",textas.getw(cur_device->getDriver()));
        Log.print_debug("State::scanDevices::Driver::%S\n",buf);
        ret=RegOpenKeyEx(HKEY_LOCAL_MACHINE,buf,0,KEY_QUERY_VALUE,&hkey);
        switch(ret)
        {
            case ERROR_SUCCESS:
                cur_device->setDriverIndex(Drivers_list.size());
                Drivers_list.emplace_back(Driver(this,cur_device,hkey,&unpacked_drp));
                break;

            default:
                Log.print_syserr(ret,L"RegOpenKeyEx()");

            case ERROR_FILE_NOT_FOUND:
                break;
        }
        RegCloseKey(hkey);
        DeviceCount++;
    }

    Log.print_debug("State::scanDevices::Count::%d\n",DeviceCount);
    Log.print_debug("State::scanDevices::SetupDiDestroyDeviceInfoList\n");
    if(SetupDiDestroyDeviceInfoList(hDevInfo))
        Log.print_debug("State::scanDevices::SetupDiDestroyDeviceInfoList::Success\n");
    else
    {
        DWORD error=GetLastError();
        Log.print_debug("State::scanDevices::SetupDiDestroyDeviceInfoList::Error:%d\n",error);
    }
    Timers.stop(time_devicescan);
    Log.print_debug("State::scanDevices::Done\n");
}

void State::init()
{
    Devices_list.clear();
    Drivers_list.clear();
    inf_list_new.clear();
    textas.reset(2);
}

const wchar_t *State::get_winverstr()
{
    // retrieve the version string for the current platform
    int ver=platform.dwMinorVersion;
    ver+=10*platform.dwMajorVersion;
    bool serv=platform.wProductType==2||platform.wProductType==3;
    return winVersions.GetVersion(ver,serv);
}

size_t State::opencatfile(const Driver *cur_driver)
{
    wchar_t filename[BUFLEN];
    char bufa[BUFLEN];
    FILE *f;
    *bufa=0;

    wcscpy(filename,textas.getw(windir));
    wsprintf(filename+wcslen(filename)-4,
             L"system32\\CatRoot\\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}\\%ws",
             textas.getw(cur_driver->getInfPath()));
    wcscpy(filename+wcslen(filename)-3,L"cat");

    f=_wfopen(filename,L"rb");
    //Log.print_con("Open '%S'\n",filename);
    if(f)
    {
        _fseeki64(f,0,SEEK_END);
        size_t len=static_cast<size_t>(_ftelli64(f));
        _fseeki64(f,0,SEEK_SET);
        std::unique_ptr<char[]> buft(new char[len]);
        fread(buft.get(),len,1,f);
        fclose(f);

        findosattr(bufa,buft.get(),len);
    }

    if(*bufa)
    {
        //Log.print_con("'%s'\n",bufa);
        return textas.strcpy(bufa);
    }
    return 0;
}

/*
 1   ***, XX..14, 4:3         ->  desktop
 2   ***, 15..16, 4:3         ->  desktop
 3   ***, 17..XX, 4:3         ->  desktop
 4   ***, XX..14, Widescreen  ->  desktop
 5   ***, 15..16, Widescreen  ->  desktop
 6   ***, 17..XX, Widescreen  ->  desktop
 7   +/-, XX..14, 4:3         ->  assume desktop
 8   +/-, 15..16, 4:3         ->  desktop
 9   +/-, 17..XX, 4:3         ->  desktop
10   +/-, XX..14, Widescreen  ->  assume laptop
11   +/-, 15..18, Widescreen  ->  assume laptop
12   +/-, 18..XX, Widescreen  ->  assume desktop
*/
void State::isnotebook_a()
{
    unsigned int i;
    int min_v=99,min_x=0,min_y=0;
    int batdev=0;
    const wchar_t *buf;
    SYSTEM_POWER_STATUS *batteryloc;

    buf=textas.getw(monitors);
    batteryloc=(SYSTEM_POWER_STATUS *)(textas.get(battery));

    if(ChassisType==3)
    {
        isLaptop=0;
        return;
    }
    if(ChassisType==10)
    {
        isLaptop=1;
        return;
    }

    for(i=0;i<buf[0];i++)
    {
        int x=buf[1+i*2];
        int y=buf[2+i*2];
        int diag=static_cast<int>(sqrt(x*x+y*y)/2.54);

        if(diag<min_v||(diag==min_v&&iswide(x,y)))
        {
            min_v=diag;
            min_x=x;
            min_y=y;
        }
    }

    for(auto &cur_device:Devices_list)
    {
        if(cur_device.getHardwareID())
        {
            const wchar_t *p=textas.getw(cur_device.getHardwareID());
            while(*p)
            {
                if(StrStrIW(p,L"*ACPI0003"))batdev=1;
                p+=wcslen(p)+1;
            }
        }
    }

    if((batteryloc->BatteryFlag&128)==0||batdev)
    {
        if(!buf[0])
            isLaptop=1;
        else if(iswide(min_x,min_y))
            isLaptop=min_v<=18?1:0;
        else
            isLaptop=0;
    }
    else
        isLaptop=0;
}
//}

//{ Monitor info
int GetMonitorDevice(const wchar_t* adapterName,DISPLAY_DEVICE *ddMon)
{
    DWORD devMon=0;

    while(EnumDisplayDevices(adapterName,devMon,ddMon,0))
    {
        if(ddMon->StateFlags&DISPLAY_DEVICE_ACTIVE&&
           ddMon->StateFlags&DISPLAY_DEVICE_ATTACHED) // for ATI, Windows XP
           break;
        devMon++;
    }
    return *ddMon->DeviceID!=0;
}

int GetMonitorSizeFromEDID(const wchar_t* adapterName,int *Width,int *Height)
{
    DISPLAY_DEVICE ddMon;
    ZeroMemory(&ddMon,sizeof(ddMon));
    ddMon.cb=sizeof(ddMon);

    *Width=0;
    *Height=0;
    if(GetMonitorDevice(adapterName,&ddMon))
    {
        wchar_t model[18];
        wchar_t* s=wcschr(ddMon.DeviceID,L'\\')+1;
        size_t len=wcschr(s,L'\\')-s;
        wcsncpy(model,s,len);
        model[len]=0;

        wchar_t *path=wcschr(ddMon.DeviceID,L'\\')+1;
        wchar_t str[MAX_PATH]=L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\";
        wcsncat(str,path,wcschr(path,L'\\')-path);
        path=wcschr(path,L'\\')+1;
        HKEY hKey;
        if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,str,0,KEY_READ,&hKey)==ERROR_SUCCESS)
        {
            DWORD i=0;
            DWORD size=MAX_PATH;
            FILETIME ft;
            while(RegEnumKeyEx(hKey,i,str,&size,nullptr,nullptr,nullptr,&ft)==ERROR_SUCCESS)
            {
                HKEY hKey2;
                if(RegOpenKeyEx(hKey,str,0,KEY_READ,&hKey2)==ERROR_SUCCESS)
                {
                    size=MAX_PATH;
                    if(RegQueryValueEx(hKey2,L"Driver",nullptr,nullptr,(LPBYTE)&str,&size)==ERROR_SUCCESS)
                    {
                        if(wcscmp(str,path)==0)
                        {
                            HKEY hKey3;
                            if(RegOpenKeyEx(hKey2,L"Device Parameters",0,KEY_READ,&hKey3)==ERROR_SUCCESS)
                            {
                                BYTE EDID[256];
                                size=256;
                                if(RegQueryValueEx(hKey3,L"EDID",nullptr,nullptr,(LPBYTE)&EDID,&size)==ERROR_SUCCESS)
                                {
                                    DWORD p=8;
                                    wchar_t model2[9];

                                    char byte1=EDID[p];
                                    char byte2=EDID[p+1];
                                    model2[0]=((byte1&0x7C)>>2)+64;
                                    model2[1]=((byte1&3)<<3)+((byte2&0xE0)>>5)+64;
                                    model2[2]=(byte2&0x1F)+64;
                                    wsprintf(model2+3,L"%X%X%X%X",(EDID[p+3]&0xf0)>>4,EDID[p+3]&0xf,(EDID[p+2]&0xf0)>>4,EDID[p+2]&0x0f);
                                    if(wcscmp(model,model2)==0)
                                    {
                                        *Width=EDID[22];
                                        *Height=EDID[21];
                                        return 1;
                                    }
                                }
                                RegCloseKey(hKey3);
                            }
                        }
                    }
                    RegCloseKey(hKey2);
                }
                i++;
            }
            RegCloseKey(hKey);
        }
    }
    return 0;
}

int iswide(int x,int y)
{
    return (static_cast<double>(y)/x)>1.35?1:0;
}
//}

// https://msdn.microsoft.com/en-au/library/windows/desktop/ms724832(v=vs.85).aspx
const VER_STRUCT WinVersions::_versions[16]={{50, false,L"Windows 2000"},
                                             {51, false,L"Windows XP"},
                                             {52, false,L"Windows XP 64"},
                                             {52, true, L"Windows Server 2003"},
                                             {52, true, L"Windows Server 2003 R2"},
                                             {60, false,L"Windows Vista"},
                                             {60, true, L"Windows Server 2008"},
                                             {61, true, L"Windows Server 2008 R2"},
                                             {61, false,L"Windows 7"},
                                             {62, true, L"Windows Server 2012"},
                                             {62, false,L"Windows 8"},
                                             {63, true, L"Windows Server 2012 R2"},
                                             {63, false,L"Windows 8.1"},
                                             {64, false,L"Windows 10 Tech Preview"},
                                             {100,true, L"Windows Server 2016"},
                                             {100,false,L"Windows 10"}};
int WinVersions::GetEntry(int num)
{
    // returns a version number
    if(num>=0&&num<Count())
        return _versions[num].ver;
    else
        return -1;
}
const wchar_t* WinVersions::GetEntryW(int num)
{
    // returns a version string
    if(num>=0&&num<Count())
        return _versions[num].vers;
    else
        return UnknownOS;
}
bool WinVersions::GetEntryServer(int num)
{
    // returns true if version is server
    if(num>=0&&num<Count())
        return _versions[num].server;
    else
        return false;
}
int WinVersions::GetVersionIndex(int vernum,bool server)
{
    // find the matching entry and return it's array index
    for(int i=0;i<Count();i++)
        if(_versions[i].ver==vernum&&_versions[i].server==server)
            return i;
    return -1;
}
const wchar_t* WinVersions::GetVersion(int vernum,bool server)
{
    // find the matching entry and return it's version string
    for(int i=0;i<Count();i++)
        if(_versions[i].ver==vernum&&_versions[i].server==server)
            return _versions[i].vers;
    return UnknownOS;
}
int WinVersions::Count()
{
    return ARRAYSIZE(_versions);
}
