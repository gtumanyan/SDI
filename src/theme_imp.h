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

//{ Vault
class VaultImp:public Vaul
{
    VaultImp(const VaultImp&)=delete;
    VaultImp &operator=(const VaultImp&)=delete;

protected:
    entry_t *entry;
    size_t num;
    std::unique_ptr<wchar_t []> data_ptr,odata_ptr,datav_ptr;

    std::unordered_map <std::wstring,int> lookuptbl;
    int res;

    Filemon *mon=nullptr;
    int elem_id;
    const wchar_t *folder;

    wchar_t namelist[41][128];

protected:
    int  findvar(wchar_t *str);
    wchar_t *findstr(wchar_t *str)const;
    int  readvalue(const wchar_t *str);
    void parse();
    bool loadFromEncodedFile(const wchar_t *filename);
    void loadFromFile(const wchar_t *filename);
    void loadFromRes(int id);

public:
    VaultImp(entry_t *entry,size_t num,int res,int elem_id_,const wchar_t *folder_);
    virtual ~VaultImp(){}
    void load(int i);

    virtual void SwitchData(int i)=0;
    virtual void EnumFiles(Combobox *lst,const wchar_t *path,int arg=0)=0;
    virtual void StartMonitor()=0;
    virtual void StopMonitor(){delete mon;};
    virtual std::wstring GetFileName(std::wstring id)=0;
    virtual std::wstring GetFileName(int id)=0;
    void updateCallback(const wchar_t *szFile,int action,int lParam);
};
//}

//{ VaultLang
class VaultLang:public VaultImp
{
    wchar_t lang_ids[40][128];

public:
    VaultLang(entry_t *entry,size_t num,int res,int elem_id_,const wchar_t *folder_);
    int  AutoPick();
    void SwitchData(int i);
    void EnumFiles(Combobox *lst,const wchar_t *path,int arg=0);
    void StartMonitor();
    Image *GetIcon(int){return nullptr;}
    Image *GetImage(int){return nullptr;}
    static void updateCallback(const wchar_t *szFile,int action,int lParam);
    std::wstring GetFileName(std::wstring id);
    std::wstring GetFileName(int id);
};
//}

//{ VaultTheme
class VaultTheme:public VaultImp
{
    wchar_t theme_ids[64][128];
    ImageStorange *Images;
    ImageStorange *Icons;

public:
    int  AutoPick();
    void SwitchData(int i);
    void EnumFiles(Combobox *lst,const wchar_t *path,int arg=0);
    void StartMonitor();
    Image *GetIcon(int i){return Icons->GetImage(i);}
    Image *GetImage(int i){return Images->GetImage(i);}

    VaultTheme(entry_t *entryv,size_t numv,int resv,int elem_id_,const wchar_t *folder_):
        VaultImp{entryv,numv,resv,elem_id_,folder_}
    {
        Images=CreateImageStorange(BOX_NUM,boxindex,4);
        Icons=CreateImageStorange(ICON_NUM,iconindex);
    }
    ~VaultTheme()
    {
        delete Images;
        delete Icons;
    }
    static void updateCallback(const wchar_t *szFile,int action,int lParam);
    std::wstring GetFileName(std::wstring id);
    std::wstring GetFileName(int id);
};
//}
