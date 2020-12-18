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

#ifndef ENUM_H
#define ENUM_H

// Declarations
class Manager;
class State;
class Device;
class infdata_t;
class Canvas;
class Version;
class Driverpack;

typedef std::unordered_map <std::wstring,infdata_t> inflist_tp;
typedef PVOID HDEVINFO;
typedef unsigned ofst;

// Misc struct
class infdata_t
{
    int catalogfile=0;
    int feature=0;
    int inf_pos;
    ofst cat;
    int start_index;

public:
    infdata_t(int vcatalogfile,int vfeature,int vinf_pos,ofst vcat,int vindex):
        catalogfile(vcatalogfile),feature(vfeature),inf_pos(vinf_pos),cat(vcat),start_index(vindex){};

    friend class Driver;
};

struct SP_DEVINFO_DATA_32
{
    DWORD     cbSize;
    GUID      ClassGuid;
    DWORD     DevInst;
    int       Reserved;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
//warning: 'class Driverpack' has pointer data members
#endif
    SP_DEVINFO_DATA_32():cbSize(sizeof(SP_DEVINFO_DATA_32)),DevInst(0),Reserved(0){}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
};

// Device
class Device
{
    int driver_index;

    ofst Devicedesc;
    ofst HardwareID;
    ofst CompatibleIDs;
    ofst Driver;
    ofst Mfg;
    ofst FriendlyName;
    unsigned Capabilities;
    unsigned ConfigFlags;

    ofst InstanceId;
    ULONG status,problem;
    int ret;

    SP_DEVINFO_DATA_32 DeviceInfoData;     // ClassGuid,DevInst

private:
    void print_guid(const GUID *g);
    void read_device_property(HDEVINFO hDevInfo,State *state,int id,ofst *val);

public:
    void setDriverIndex(int v){driver_index=v;}
    int  getDriverIndex()const{return driver_index;}
    ofst getHardwareID()const{return HardwareID;}
    ofst getCompatibleIDs()const{return CompatibleIDs;}
    ofst getFriendlyName()const{return FriendlyName;}
    ofst getDriver()const{return Driver;}
    ofst getDescr()const{return Devicedesc;}
    ofst getMfg()const{return Mfg;}
    ofst getRet()const{return ret;}
    ofst getProblem()const{return problem;}

    int  print_status();
    void print(const State *state);
    void printHWIDS(const State *state);
    const wchar_t *getHWIDby(int num,const State *state);
    void getClassDesc(wchar_t *str);

    //Device(const Device &)=delete;
    //Device &operator=(const Device &)=delete;
    //Device(Device &&)=default;
    Device(HDEVINFO hDevInfo,State *state,int i);
    Device(State *state);
    Device():driver_index(-1),Devicedesc(0),HardwareID(0),CompatibleIDs(0),Driver(0),
        Mfg(0),FriendlyName(0),Capabilities(0),ConfigFlags(0),
        InstanceId(0),status(0),problem(0),ret(0),DeviceInfoData(){}

    friend class Manager; // TODO: friend
    friend class itembar_t; // TODO: friend
    friend class CanvasImp;
};

// Driver
class Driver
{
    ofst DriverDesc;
    ofst ProviderName;
    ofst DriverDate;
    ofst DriverVersion;
    ofst MatchingDeviceId;
    ofst InfPath;
    ofst InfSection;
    ofst InfSectionExt;
    ofst cat;
    Version version;

    int catalogfile;
    int feature;
    int identifierscore;

private:
    void read_reg_val(HKEY hkey,State *state,const wchar_t *key,ofst *val);
    void scaninf(State *state,Driverpack *unpacked_drp,int &inf_pos);
    int findHWID_in_list(const wchar_t *p,const wchar_t *str);
    void calc_dev_pos(const Device *cur_device,const State *state,int *ishw,int *dev_pos);

public:
    ofst getProviderName()const{return ProviderName;}
    ofst getDriverDate()const{return DriverDate;}
    ofst getDriverVersion()const{return DriverVersion;}
    ofst getInfPath()const{return InfPath;}
    ofst getInfSection()const{return InfSection;}
    ofst getMatchingDeviceId()const{return MatchingDeviceId;}
    ofst getDriverDesc()const{return DriverDesc;}
    ofst getCat()const{return cat;}
    const Version *getVersion()const{return &version;}

    unsigned calc_score_h(const State *state)const;
    int  isvalidcat(const State *state)const;
    void print(const State *state)const;


    //Driver(const Driver &)=delete;
    //Driver &operator=(const Driver &)=delete;
    //Driver(Driver &&)=default;
    Driver(State *state,Device *cur_device,HKEY hkey,Driverpack *unpacked_drp);
    Driver():DriverDesc(0),ProviderName(0),DriverDate(0),DriverVersion(0),MatchingDeviceId(0),
        InfPath(0),InfSection(0),InfSectionExt(0),cat(0),version(),catalogfile(0),feature(0),identifierscore(0){}

    friend class Manager; // TODO: friend
    friend class itembar_t; // TODO: friend
};

// State (POD)
class state_m_t
{
    OSVERSIONINFOEX platform;
    int locale;
    int architecture;

    ofst manuf;
    ofst model;
    ofst product;
    ofst monitors;
    ofst battery;

    ofst windir;
    ofst temp;

    ofst cs_manuf;
    ofst cs_model;
    int ChassisType;
    int revision;
    char reserved[1024];

    char reserved1[676];
};

struct VER_STRUCT
{
    int ver;
    bool server;
    const wchar_t* vers;
};
class WinVersions
{
    static const VER_STRUCT _versions[16];
    const wchar_t* UnknownOS=L"Unknown OS";
public:
    int GetEntry(int num);                             // returns entry version number
    const wchar_t* GetEntryW(int num);                 // returns entry version string
    bool GetEntryServer(int num);                      // returns entry server
    int GetVersionIndex(int vernum,bool server);       // find the matching entry
    const wchar_t* GetVersion(int vernum,bool server); // find the matching entry
    static int Count();
};

// State
class State
{
    OSVERSIONINFOEX platform;
    int locale;
    int architecture;

    ofst manuf;
    ofst model;
    ofst product;
    ofst monitors;
    ofst battery;

    ofst windir;
    ofst temp;

    ofst cs_manuf;
    ofst cs_model;
    int ChassisType;
    int revision;
    char reserved[1024];

    char reserved1[676];

    // --- End of POD ---

    loadable_vector<Device> Devices_list;
    loadable_vector<Driver> Drivers_list;

public:
    Txt textas;
    inflist_tp inf_list_new;
    int isLaptop;

private:
    int getbaseboard(WStringShort &manuf,WStringShort &model,WStringShort &product,WStringShort &cs_manuf,WStringShort &cs_model,int *type);
    void fakeOSversion();

public:
    ofst getWindir()const{return windir;}
    ofst getTemp()const{return temp;}
    int getLocale()const{return locale;}
    int getArchitecture()const{return architecture;}
    void getWinVer(int *major,int *minor)const;
    const wchar_t *get_szCSDVersion()const{return platform.szCSDVersion;}
    std::vector<Device> *getDevices_list(){return &Devices_list;}
    const Driver *getCurrentDriver(const Device *dev)const{return (dev->getDriverIndex()>=0)?&Drivers_list[dev->getDriverIndex()]:nullptr;}
    WinVersions winVersions;

    const wchar_t *getProduct();
    const wchar_t *getManuf();
    const wchar_t *getModel();
    int getPlatformProductType();

    State();
    void print();
    void popup_sysinfo(Canvas &canvas);
    void contextmenu2(int x,int y);

    int save(const wchar_t *filename);
    int  load(const wchar_t *filename);
    void getsysinfo_fast();
    void getsysinfo_slow();
    void getsysinfo_slow(const State *prev);
    void scanDevices();
    void init();

    const wchar_t *get_winverstr();
    size_t opencatfile(const Driver *cur_driver);
    void genmarker(); // in matcher.cpp
    void isnotebook_a();
};

// Monitor info
int GetMonitorDevice(const wchar_t* adapterName,DISPLAY_DEVICE *ddMon);
int GetMonitorSizeFromEDID(const wchar_t* adapterName,int *Width,int *Height);
int iswide(int x,int y);

// Calc
int calc_identifierscore(int dev_pos,int dev_ishw,int inf_pos);
unsigned calc_score(int catalogfile,int feature,int rank,const State *state,int isnt);
int calc_secttype(const char *s);
int calc_signature(int catalogfile,const State *state,int isnt);

#endif
