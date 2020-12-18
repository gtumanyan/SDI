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

#ifndef DRAW_H
#define DRAW_H

// Declarations
class Image;
class ClipRegionImp;
class ImageImp;
class Device;
struct RECT_WR;
//typedef HWND *p_wnd_handle_type;
typedef void *p_wnd_handle_type;

// Global vars
extern int rtl;

//{ Font
class wFont
{
public:
    static wFont *Create();
    virtual ~wFont(){}
    virtual void SetFont(const wchar_t *name,int size,bool bold=false)=0;
};
//}

//{ ClipRegion
class ClipRegion
{
    ClipRegion(const ClipRegion&)=delete;
    void operator=(const ClipRegion&)=delete;

private:
    ClipRegionImp *imp;
    friend class CanvasImp;

public:
    ClipRegion();
    ClipRegion(int x1,int y1,int x2,int y2);
    ClipRegion(int x1,int y1,int x2,int y2,int r);
    void setRegion(int x1,int y1,int x2,int y2);
    ~ClipRegion();
};
//}

//{ Combobox
class Combobox
{
public:
    virtual ~Combobox(){}

    virtual void Clear()=0;
    virtual void AddItem(const wchar_t *str)=0;
    virtual int FindItem(const wchar_t *str)=0;
    virtual int GetNumItems()=0;
    virtual void SetCurSel(int i)=0;
    virtual void Focus()=0;
    virtual void SetFont(wFont *font)=0;
    virtual void Move(int x1,int y1,int wx,int wy)=0;
    virtual void SetMirroring()=0;

    static Combobox *Create(p_wnd_handle_type hwnd,int id);
};
//}

//{ Image
class Image
{
public:
    enum align
    {
        RIGHT   = 1,
        BOTTOM  = 2,
        HCENTER = 4,
        VCENTER = 8,
    };
    enum fillmode
    {
        HTILE   =  1,
        VTILE   =  2,
        HSTR    =  4,
        VSTR    =  8,
        ASPECT  = 16,
    };

    virtual ~Image(){}
    virtual void Load(int strid)=0;
    virtual void MakeCopy(ImageImp &t)=0;
};
//}

//{ ImageStorange
class ImageStorange
{
public:
    virtual ~ImageStorange(){}
    virtual Image *GetImage(size_t n)=0;
    virtual void LoadAll()=0;
};
ImageStorange *CreateImageStorange(size_t n,const int *ind,int add_=0);
//}

//{ Canvas
class Canvas
{
public:
    virtual ~Canvas(){}

    virtual void CopyCanvas(Canvas *source,int x1,int y1)=0;
    virtual void begin(p_wnd_handle_type hwnd,int x,int y,bool mirror=true)=0;
    virtual void end()=0;

    virtual void SetClipRegion(ClipRegion &clip)=0;
    virtual void ClearClipRegion()=0;

    virtual void DrawEmptyRect(int x1,int y1,int x2,int y2,int color)=0;
    virtual void DrawFilledRect(int x1,int y1,int x2,int y2,int color1,int color2,int w,int r)=0;
    virtual void DrawWidget(int x1,int y1,int x2,int y2,int i)=0;
    virtual void DrawImage(Image &image,int x1,int y1,int wx,int wy,int flags1,int flags2)=0;
    virtual void DrawLine(int x1,int y1,int x2,int y2)=0;
    virtual void DrawCheckbox(int x,int y,int wx,int wy,int checked,int active,int type)=0;
    virtual void DrawConnection(int x,int pos,int ofsy,int curpos)=0;
    virtual void DrawIcon(int x1,int y1,const char *guid_driverpack,const Device *device)=0;

    virtual void SetTextColor(int color)=0;
    virtual void SetFont(wFont *font)=0;
    virtual void DrawTextXY(int x,int y,const wchar_t *buf)=0;
    virtual void DrawTextRect(const wchar_t *bufw,RECT_WR *rect,int flags=0)=0;
    virtual void CalcBoundingBox(const wchar_t *str,RECT_WR *rect)=0;
    virtual int  GetTextExtent(const wchar_t *str)=0;

    static Canvas *Create();
};
//}

//{ Popup
void popup_about(Canvas &canvas);
void format_size(wchar_t *buf,long long val,int isspeed);
void format_time(wchar_t *buf,long long val);
//}

#endif
