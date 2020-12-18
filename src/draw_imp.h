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

//{ wFont
class wFontImp:public wFont
{
private:
    HFONT hFont;
    friend class CanvasImp;
    friend class ComboboxImp;

public:
    wFontImp():hFont(nullptr){}
    ~wFontImp();
    void SetFont(const wchar_t *name,int size,bool bold=false);
};
//}

//{ ClipRegion
class ClipRegionImp
{
    ClipRegionImp(const ClipRegionImp&)=delete;
    ClipRegionImp &operator = (const ClipRegionImp&)=delete;

private:
    HRGN hrgn;
public:
    ClipRegionImp():hrgn(nullptr){}
    ClipRegionImp(int x1,int y1,int x2,int y2);
    ClipRegionImp(int x1,int y1,int x2,int y2,int r);
    ~ClipRegionImp();

    void setRegion(int x1,int y1,int x2,int y2);

    friend class CanvasImp;
};
//}

//{ Combobox
class ComboboxImp:public Combobox
{
    HWND handle;

public:
    ComboboxImp(HWND hwnd,int id);
    void Clear();
    void AddItem(const wchar_t *str);
    int FindItem(const wchar_t *str);
    int GetNumItems();
    void SetCurSel(int i);
    void Focus();
    void SetFont(wFont *font);
    void Move(int x1,int y1,int wx,int wy);
    void SetMirroring();
};
//}

//{ Image
class ImageImp:public Image
{
    HBITMAP bitmap=nullptr;
    HGDIOBJ oldbitmap=nullptr;
    HDC ldc=nullptr;
    int sx=0,sy=0,hasalpha=0;
    int iscopy=0;

private:
    void LoadFromFile(wchar_t *filename);
    void LoadFromRes(int id);
    void CreateMyBitmap(BYTE *data,size_t sz);
    void Draw(HDC dc,int x1,int y1,int x2,int y2,int anchor,int fill);
    void Release();
    bool IsLoaded()const;
    friend class CanvasImp;

public:
    ~ImageImp(){Release();}
    void Load(int strid);
    void MakeCopy(ImageImp &t);
};
//}

//{ ImageStorange
class ImageStorangeImp:public ImageStorange
{
    ImageImp *a;
    size_t num;
    int add;
    const int *index;

public:
    ImageStorangeImp(size_t n,const int *ind,int add_=0);
    ~ImageStorangeImp();
    Image *GetImage(size_t n);
    void LoadAll();
};
//}

//{ Canvas
class CanvasImp:public Canvas
{
    int x,y;
    HDC localDC;
    HDC hdcMem;
    HBITMAP bitmap,oldbitmap;
    PAINTSTRUCT ps;
    HWND hwnd;
    HRGN clipping;

private:
    HICON CreateMirroredIcon(HICON hiconOrg);
    void loadGUID(GUID *g,const char *s);

public:
    CanvasImp();
    ~CanvasImp();

    void CopyCanvas(Canvas *source,int x1,int y1);
    void begin(p_wnd_handle_type hwnd,int x,int y,bool mirror=true);
    void end();

    void SetClipRegion(ClipRegion &clip);
    void ClearClipRegion();

    void DrawEmptyRect(int x1,int y1,int x2,int y2,int color);
    void DrawFilledRect(int x1,int y1,int x2,int y2,int color1,int color2,int w,int r);
    void DrawWidget(int x1,int y1,int x2,int y2,int i);
    void DrawImage(Image &image,int x1,int y1,int wx,int wy,int flags1,int flags2);
    void DrawLine(int x1,int y1,int x2,int y2);
    void DrawCheckbox(int x,int y,int wx,int wy,int checked,int active,int type);
    void DrawConnection(int x,int pos,int ofsy,int curpos);
    void DrawIcon(int x1,int y1,const char *guid_driverpack,const Device *device);

    void SetTextColor(int color);
    void SetFont(wFont *font);
    void DrawTextXY(int x,int y,const wchar_t *buf);
    void DrawTextRect(const wchar_t *bufw,RECT_WR *rect,int flags=0);
    void CalcBoundingBox(const wchar_t *str,RECT_WR *rect);
    int  GetTextExtent(const wchar_t *str);
};
//}
