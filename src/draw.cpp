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
#include "gui.h"
#include "theme.h"
#include "draw.h"

#include <windows.h>

// Depend on Win32API
#include "enum.h"     // todo: lots of Win32
#include "main.h"     // todo: lots of Win32

#include <setupapi.h>       // for SetupDiGetClassDescription()
#include <webp\decode.h>
#include <memory>

#include "draw_imp.h"

//{ Global vars
int rtl=0;
//}

//{ wFont
wFont *wFont::Create(){return new wFontImp;}

void wFontImp::SetFont(const wchar_t *name,int size,bool bold)
{
		if(hFont&&!DeleteObject(hFont))
				Log.print_err("ERROR in setfont(): failed DeleteObject\n");

		hFont=CreateFont(-size,0,0,0,bold?FW_BOLD:FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
										 CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH,name);

		if(!hFont)Log.print_err("ERROR in setfont(): failed CreateFont\n");
}

wFontImp::~wFontImp()
{
		if(hFont&&!DeleteObject(hFont))
				Log.print_err("ERROR in manager_free(): failed DeleteObject\n");
}
//}

//{ ClipRegion
ClipRegionImp::ClipRegionImp(int x1,int y1,int x2,int y2):
		hrgn(CreateRectRgn(x1,y1,x2,y2))
{
		if(!hrgn)Log.print_err("ERROR in ClipRegion(): failed CreateRectRgn\n");
}

ClipRegionImp::ClipRegionImp(int x1,int y1,int x2,int y2,int r):
		hrgn(CreateRoundRectRgn(x1,y1,x2,y2,r,r))
{
		if(!hrgn)Log.print_err("ERROR in ClipRegion(): failed CreateRoundRectRgn\n");
}

void ClipRegionImp::setRegion(int x1,int y1,int x2,int y2)
{
		if(hrgn)DeleteObject(hrgn);
		hrgn=CreateRectRgn(x1,y1,x2,y2);
		if(!hrgn)Log.print_err("ERROR in ClipRegion(): failed setRegion\n");
}

ClipRegionImp::~ClipRegionImp()
{
		if(hrgn)DeleteObject(hrgn);
}

ClipRegion::ClipRegion(int x1,int y1,int x2,int y2):imp(new ClipRegionImp(x1,y1,x2,y2)){}
ClipRegion::ClipRegion(int x1,int y1,int x2,int y2,int r):imp(new ClipRegionImp(x1,y1,x2,y2,r)){}
ClipRegion::ClipRegion():imp(new ClipRegionImp()){}
ClipRegion::~ClipRegion(){delete imp;}
void ClipRegion::setRegion(int x1,int y1,int x2,int y2){imp->setRegion(x1,y1,x2,y2);}
//}

//{ ComboBox
ComboboxImp::ComboboxImp(HWND hwnd,int id)
{
		handle=CreateWindowMF(WC_COMBOBOX,L"",hwnd,id,CBS_DROPDOWNLIST|CBS_HASSTRINGS|WS_OVERLAPPED|WS_VSCROLL);
}
void ComboboxImp::Clear()
{
		SendMessage(handle,CB_RESETCONTENT,0,0);
}
void ComboboxImp::AddItem(const wchar_t *str)
{
		SendMessage(handle,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(str));
}
int ComboboxImp::FindItem(const wchar_t *str)
{
		return static_cast<int>(SendMessage(handle,CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(str)));
}
int ComboboxImp::GetNumItems()
{
		return static_cast<int>(SendMessage(handle,CB_GETCOUNT,0,0));
}
void ComboboxImp::SetCurSel(int i)
{
		SendMessage(handle,CB_SETCURSEL,i,0);
}
void ComboboxImp::Focus()
{
		SetFocus(handle);
}
void ComboboxImp::SetFont(wFont *font)
{
		SendMessage(handle,WM_SETFONT,(WPARAM)dynamic_cast<wFontImp *>(font)->hFont,MAKELPARAM(FALSE,0));
}
void ComboboxImp::Move(int x1,int y1,int wx,int wy)
{
		MoveWindow(handle,x1,y1,wx,wy,false);
}

void ComboboxImp::SetMirroring()
{
		setMirroring(handle);
}

Combobox *Combobox::Create(p_wnd_handle_type hwnd,int id)
{
		return new ComboboxImp(*reinterpret_cast<HWND *>(hwnd),id);
}
//}

//{ Image
void ImageImp::MakeCopy(ImageImp &t)
{
		Release();
		bitmap=t.bitmap;
		ldc=t.ldc;
		sx=t.sx;
		sy=t.sy;
		hasalpha=t.hasalpha;
		iscopy=1;
}

void ImageImp::Load(int i)
{
		wchar_t *filename=D_STR(i);

		Release();

		if(wcsstr(filename,L"RES_"))
				LoadFromRes(_wtoi_my(filename+4));
		else
				LoadFromFile(filename);
}

bool ImageImp::IsLoaded()const{return ldc!=nullptr;}

void ImageImp::Release()
{
		if(bitmap&&!iscopy)
		{
				SelectObject(ldc,oldbitmap);
				int r=DeleteDC(ldc);
				if(!r)Log.print_err("ERROR in box_init(): failed DeleteDC\n");
				r=DeleteObject(bitmap);
				if(!r)Log.print_err("ERROR in box_init(): failed DeleteObject\n");
		}
		bitmap=nullptr;
		ldc=nullptr;
		iscopy=0;
}

void ImageImp::LoadFromFile(const wchar_t *filename)
{
		if(!filename||!*filename)return;

		WStringShort name;
		name.sprintf(L"%s\\themes\\%s",Settings.data_dir,filename);
		FILE *f=_wfopen(name.Get(),L"rb");
		if(!f)
		{
				Log.print_err("ERROR in image_loadFile(): file '%S' not found\n",name.Get());
				return;
		}
		_fseeki64(f,0,SEEK_END);
		size_t sz=static_cast<size_t>(_ftelli64(f));
		_fseeki64(f,0,SEEK_SET);
		std::unique_ptr<BYTE[]> imgbuf(new BYTE[sz]);

		sz=fread(imgbuf.get(),1,sz,f);
		if(!sz)
		{
				Log.print_err("ERROR in image_loadFile(): cannnot read from file '%S'\n",name.Get());
				return;
		}
		fclose(f);
		CreateMyBitmap(imgbuf.get(),sz);
}

void ImageImp::LoadFromRes(int id)
{
		size_t sz;
		HGLOBAL myResourceData;

		get_resource(id,&myResourceData,&sz);
		if(!sz)
		{
				Log.print_err("ERROR in image_loadRes(): failed get_resource\n");
				return;
		}
		CreateMyBitmap(static_cast<BYTE *>(myResourceData),sz);
}

void ImageImp::CreateMyBitmap(BYTE *data,size_t sz)
{
		BYTE *big;
		hasalpha=sx=sy=0;
		ldc=nullptr;

		int ret= WebPGetInfo(data,sz,&sx,&sy);
		if(!ret)
		{
				Log.print_err("ERROR in image_load(): failed WebPGetInfo(%d)\n",ret);
				return;
		}
		big=WebPDecodeBGRA(data,sz,&sx,&sy);
		if(!big)
		{
				Log.print_err("ERROR in image_load(): failed WebPDecodeBGRA\n");
				return;
		}

		BITMAPINFO bmi;
		ZeroMemory(&bmi,sizeof(BITMAPINFO));
		bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth=sx;
		bmi.bmiHeader.biHeight=-sy;
		bmi.bmiHeader.biPlanes=1;
		bmi.bmiHeader.biBitCount=32;
		bmi.bmiHeader.biCompression=BI_RGB;
		bmi.bmiHeader.biSizeImage=sx*sy*4;

		BYTE *bits;
		ldc=CreateCompatibleDC(nullptr);
		bitmap=CreateDIBSection(ldc,&bmi,DIB_RGB_COLORS,reinterpret_cast<void **>(&bits),nullptr,0);
		if(!bitmap)
		{
				Log.print_err("ERROR in CreateMyBitmap(): failed CreateDIBSection\n");
				free(big);
				return;
		}

		BYTE *p2=big;
		for(int i=0;i<sy*sx;i++)
		{
				int B,G,R,A;
				B=static_cast<int>(*p2++);
				G=static_cast<int>(*p2++);
				R=static_cast<int>(*p2++);
				A=static_cast<int>(*p2++);
				if(A!=255)hasalpha=1;

				*bits++=static_cast<BYTE>(B*A/256);
				*bits++=static_cast<BYTE>(G*A/256);
				*bits++=static_cast<BYTE>(R*A/256);
				*bits++=static_cast<BYTE>(A);
		}
		SelectObject(ldc,bitmap);
		free(big);
		//    Log.print_con("%dx%d:%d,%d\n",sx,sy,hasalpha,index);
}

void ImageImp::Draw(HDC dc,int x1,int y1,int x2,int y2,int anchor,int fill)
{
		BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
		int xi,yi,wx,wy,wx1,wy1,wx2,wy2;

		if(!sx)return;

		SetLayout(ldc,rtl?LAYOUT_RTL:0);

		wx=(fill&HSTR)?x2-x1:sx;
		wy=(fill&VSTR)?y2-y1:sy;
		if(fill&ASPECT)
		{
				if(fill&HSTR)wy=sy*wx/sx;
				if(fill&VSTR)wx=sx*wy/sy;
		}

		for(xi=0;xi<x2;xi+=wx)
		{
				for(yi=0;yi<y2;yi+=wy)
				{
						int x=x1+xi,y=y1+yi;
						if(anchor&RIGHT)  x=x2-xi-wx;
						if(anchor&BOTTOM) y=y2-yi-wy;
						if(anchor&HCENTER)x=(x2-x1-wx)/2;
						if(anchor&VCENTER)y=(y2-y1-wy)/2;

						wx1=(x+wx>x2)?x2-x:wx;
						wy1=(y+wy>y2)?y2-y:wy;
						wx2=(x+wx>x2)?wx1:sx;
						wy2=(y+wy>y2)?wy1:sy;

						if(hasalpha)
								AlphaBlend(dc,x,y,wx1,wy1,ldc,0,0,wx2,wy2,blend);
						else if(wx==wx2&&wy==wy2)
								BitBlt(dc,x,y,wx1,wy1,ldc,0,0,SRCCOPY);
						else
								StretchBlt(dc,x,y,wx1,wy1,ldc,0,0,wx2,wy2,SRCCOPY);

						if((fill&VTILE)==0)break;
				}
				if((fill&HTILE)==0)break;
		}
		//drawrect(dc,x1,y1,x2,y2,0xFF000000,0xFF00,1,0);
}
//}

//{ ImageStorange
ImageStorange *CreateImageStorange(size_t n,const int *ind,int add_)
{
		return new ImageStorangeImp(n,ind,add_);
}
ImageStorangeImp::ImageStorangeImp(size_t n,const int *ind,int add_)
{
		a=new ImageImp[n];
		num=n;
		index=ind;
		add=add_;
}
ImageStorangeImp::~ImageStorangeImp()
{
		delete[] a;
}
Image *ImageStorangeImp::GetImage(size_t n)
{
		return &a[n];
}
void ImageStorangeImp::LoadAll()
{
		for(size_t i=0;i<num;i++)
		{
				wchar_t *str=D_STR(index[i]+add);
				if (!wcscmp(str, L"")) continue;
				size_t j;
				for(j=0;j<i;j++)
						//Log.print_con("%d Copying %S %d\n",i,str,j);
						if(!wcscmp(str,D_STR(index[j]+add)))
				{
						a[i].MakeCopy(a[j]);
						//Log.print_con("%d Copy %S %d\n",i,str,j);
						break;
				}
				if(i==j)
				{
						Log.print_con("Load %S for %S\n",str, theme[index[i]+add].name);
						a[i].Load(index[i]+add);
				}
		}
}
//}

//{ Canvas
Canvas *Canvas::Create(){return new CanvasImp;}

void CanvasImp::DrawConnection(int x1,int pos,int ofsy,int curpos)
{
		HGDIOBJ oldpen,newpen;

		newpen=CreatePen(PS_SOLID,D_1(DRVITEM_LINE_WIDTH),D_C(DRVITEM_LINE_COLOR));
		oldpen=SelectObject(hdcMem,newpen);
		MoveToEx(hdcMem,x1-D_X(DRVITEM_LINE_INTEND)/2,curpos-D_X(DRVITEM_DIST_Y0)+D_X(DRVITEM_WY)-ofsy,nullptr);
		LineTo(hdcMem,x1-D_X(DRVITEM_LINE_INTEND)/2,pos+D_X(DRVITEM_WY)/2);
		LineTo(hdcMem,x1,pos+D_X(DRVITEM_WY)/2);
		SelectObject(hdcMem,oldpen);
		DeleteObject(newpen);
}

CanvasImp::CanvasImp():
		x(0),
		y(0),
		localDC(nullptr),
		hdcMem(CreateCompatibleDC(nullptr)),
		bitmap(nullptr),
		oldbitmap(nullptr),
		//ps(nullptr),
		hwnd(nullptr),
		clipping(nullptr)
{
		if(!hdcMem)
				Log.print_err("ERROR in canvas_init(): failed CreateCompatibleDC\n");
		else
		{
				int r=SetBkMode(hdcMem,TRANSPARENT);
				if(!r)Log.print_err("ERROR in canvas_init(): failed SetBkMode\n");
		}
}

CanvasImp::~CanvasImp()
{
		if(hdcMem)
		{
				int r=DeleteDC(hdcMem);
				if(!r)Log.print_err("ERROR in canvas_free(): failed DeleteDC\n");
				hdcMem=nullptr;
		}

		if(bitmap)
		{
				//r=(int)SelectObject(hdcMem,oldbitmap);
				//if(!r)Log.log_err("ERROR in canvas_free(): failed SelectObject\n");
				int r=DeleteObject(bitmap);
				if(!r)Log.print_err("ERROR in canvas_free(): failed DeleteObject\n");
				bitmap=nullptr;
		}
}

void CanvasImp::begin(p_wnd_handle_type nhwnd,int nx,int ny,bool mirror)
{
		unsigned r32;

		hwnd=*reinterpret_cast<HWND *>(nhwnd);
		localDC=BeginPaint(hwnd,&ps);
		if(!localDC)Log.print_err("ERROR in canvas_begin(): failed BeginPaint\n");

		if(x!=nx||y!=ny)
		{
				x=nx;
				y=ny;
				if(bitmap)
				{
						HGDIOBJ r=SelectObject(hdcMem,oldbitmap);
						if(!r)Log.print_err("ERROR in canvas_begin(): failed SelectObject(oldbitmap)\n");
						r32=DeleteObject(bitmap);
						if(!r32)Log.print_err("ERROR in canvas_begin(): failed DeleteObject\n");
				}
				bitmap=CreateCompatibleBitmap(localDC,x,y);
				if(!bitmap)
						Log.print_err("ERROR in canvas_begin(): failed CreateCompatibleBitmap\n");
				else
				{
						oldbitmap=static_cast<HBITMAP>(SelectObject(hdcMem,bitmap));
						if(!oldbitmap)Log.print_err("ERROR in canvas_begin(): failed SelectObject(bitmap)\n");
				}
		}
		clipping=CreateRectRgnIndirect(&ps.rcPaint);
		if(!clipping)Log.print_err("ERROR in canvas_begin(): failed BeginPaint\n");
		SetStretchBltMode(hdcMem,HALFTONE);
		r32=SelectClipRgn(hdcMem,clipping);
		if(!r32)Log.print_err("ERROR in canvas_begin(): failed SelectClipRgn\n");
		if(mirror)
				SetLayout(hdcMem,rtl?LAYOUT_RTL:0);
		else
				SetLayout(hdcMem,0);
}

void CanvasImp::end()
{
		int r;

		r=BitBlt(localDC,
						 ps.rcPaint.left,ps.rcPaint.top,ps.rcPaint.right,ps.rcPaint.bottom,
						 hdcMem,
						 ps.rcPaint.left,ps.rcPaint.top,
						 SRCCOPY);
		SelectClipRgn(hdcMem,nullptr);
		if(!r)Log.print_err("ERROR in canvas_end(): failed BitBlt\n");
		r=DeleteObject(clipping);
		if(!r)Log.print_err("ERROR in canvas_end(): failed DeleteObject\n");
		EndPaint(hwnd,&ps);
}
//}

//{ Draw
void CanvasImp::DrawTextXY(int x1,int y1,LPCTSTR buf)
{
		TextOut(hdcMem,x1,y1,buf,static_cast<int>(wcslen(buf)));
}

void CanvasImp::SetTextColor(int color)
{
		::SetTextColor(hdcMem,color);
}

void CanvasImp::SetFont(wFont *font)
{
		SelectObject(hdcMem,dynamic_cast<wFontImp *>(font)->hFont);
}

void CanvasImp::SetClipRegion(ClipRegion &clip)
{
		SelectClipRgn(hdcMem,clip.imp->hrgn);
}

void CanvasImp::ClearClipRegion()
{
		SelectClipRgn(hdcMem,nullptr);
}

void CanvasImp::CalcBoundingBox(const wchar_t *str,RECT_WR *rect)
{
		DrawText(hdcMem,str,-1,reinterpret_cast<RECT *>(rect),DT_WORDBREAK|DT_CALCRECT);
}

void CanvasImp::DrawTextRect(const wchar_t *bufw,RECT_WR *rect,int flags)
{
		DrawText(hdcMem,bufw,-1,reinterpret_cast<RECT *>(rect),DT_WORDBREAK|flags);
}

int  CanvasImp::GetTextExtent(const wchar_t *str)
{
		SIZE ss;
		GetTextExtentPoint32(hdcMem,str,static_cast<int>(wcslen(str)),&ss);
		return ss.cx;
}

void CanvasImp::DrawImage(Image &image,int x1,int y1,int wx,int wy,int flags1,int flags2)
{
		(dynamic_cast<ImageImp &>(image)).Draw(hdcMem,x1,y1,wx,wy,flags1,flags2);
}

void CanvasImp::CopyCanvas(Canvas *source,int x1,int y1)
{
		BitBlt(hdcMem,0,0,x,y,dynamic_cast<CanvasImp *>(source)->hdcMem,x1,y1,SRCCOPY);
}

void CanvasImp::loadGUID(GUID *g,const char *s)
{
		char d[3];
		d[2]=0;
		g->Data1=strtol(s+1,nullptr,16);
		g->Data2=strtol(s+10,nullptr,16)&0xFFFF;
		g->Data3=strtol(s+15,nullptr,16)&0xFFFF;
		memcpy(d,s+15+5,2);g->Data4[0]=strtol(d,nullptr,16)&0xFF;
		memcpy(d,s+15+5+2,2);g->Data4[1]=strtol(d,nullptr,16)&0xFF;
		memcpy(d,s+15+6+4,2);g->Data4[2]=strtol(d,nullptr,16)&0xFF;
		memcpy(d,s+15+6+6,2);g->Data4[3]=strtol(d,nullptr,16)&0xFF;
		memcpy(d,s+15+6+8,2);g->Data4[4]=strtol(d,nullptr,16)&0xFF;
		memcpy(d,s+15+6+10,2);g->Data4[5]=strtol(d,nullptr,16)&0xFF;
		memcpy(d,s+15+6+12,2);g->Data4[6]=strtol(d,nullptr,16)&0xFF;
		memcpy(d,s+15+6+14,2);g->Data4[7]=strtol(d,nullptr,16)&0xFF;

		/*Log.print_con("%ses\n",ses);
		Log.print_con("{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n\n",g->Data1,g->Data2,g->Data3,
		(int)(g->Data4[0]),(int)(g->Data4[1]),
		(int)(g->Data4[2]),(int)(g->Data4[3]),(int)(g->Data4[4]),
		(int)(g->Data4[5]),(int)(g->Data4[6]),(int)(g->Data4[7]));*/
}

void CanvasImp::DrawIcon(int x1,int y1,const char *guid_driverpack,const Device *device)
{
		HICON hIcon=nullptr;
		BOOL ret=false;
		if(guid_driverpack)
		{
				GUID gd;
				loadGUID(&gd,guid_driverpack);
				ret=SetupDiLoadClassIcon(&gd,&hIcon,nullptr);
		}
		if(!ret)ret=SetupDiLoadClassIcon(&device->DeviceInfoData.ClassGuid,&hIcon,nullptr);
		if(hIcon)
		{
				if(rtl)
				{
						HICON miricon;
						miricon=CreateMirroredIcon(hIcon);
						DestroyIcon(hIcon);
						hIcon=miricon;
				}
				DrawIconEx(hdcMem,x1,y1,hIcon,D_X(ITEM_ICON_SIZE),D_X(ITEM_ICON_SIZE),0,nullptr,DI_NORMAL);
				DestroyIcon(hIcon);
		}
}

void CanvasImp::DrawEmptyRect(int x1,int y1,int x2,int y2,int color)
{
		SelectObject(hdcMem,GetStockObject(DC_BRUSH));
		SelectObject(hdcMem,GetStockObject(DC_PEN));
		SetDCBrushColor(hdcMem,color);
		Rectangle(hdcMem,x1,y1,x2,y2);
}

void CanvasImp::DrawLine(int x1,int y1,int x2,int y2)
{
		MoveToEx(hdcMem,x1,y1,nullptr);
		LineTo(hdcMem,x2,y2);
}

void CanvasImp::DrawFilledRect(int x1,int y1,int x2,int y2,int color1,int color2,int w,int rn)
{
		HPEN newpen,oldpen=nullptr;
		HBRUSH /*newbrush,*/oldbrush;
		HGDIOBJ r;
		unsigned r32;

		if(x1>x2)return;
		oldbrush=static_cast<HBRUSH>(SelectObject(hdcMem,GetStockObject(color1&0xFF000000?NULL_BRUSH:DC_BRUSH)));
		//newbrush=CreateSolidBrush(color1);
		//oldbrush=(HBRUSH)SelectObject(hdcMem,newbrush);
		if(color1&0xFF000000)(HBRUSH)SelectObject(hdcMem,GetStockObject(NULL_BRUSH));

		if(!oldbrush)Log.print_err("ERROR in drawrect(): failed SelectObject(GetStockObject)\n");
		r32=SetDCBrushColor(hdcMem,color1);
		if(r32==CLR_INVALID)Log.print_err("ERROR in drawrect(): failed SetDCBrushColor\n");

		newpen=CreatePen(w?PS_SOLID:PS_NULL,w,color2);
		if(newpen)
		{
				oldpen=static_cast<HPEN>(SelectObject(hdcMem,newpen));
				if(!oldpen)Log.print_err("ERROR in drawrect(): failed SelectObject(newpen)\n");
		}
		else
				Log.print_err("ERROR in drawrect(): failed CreatePen\n");

		if(rn)
				RoundRect(hdcMem,x1,y1,x2,y2,rn,rn);
		else
				Rectangle(hdcMem,x1,y1,x2,y2);

		if(oldpen)
		{
				r=SelectObject(hdcMem,oldpen);
				if(!r)Log.print_err("ERROR in drawrect(): failed SelectObject(oldpen)\n");
		}
		if(oldbrush)
		{
				r=SelectObject(hdcMem,oldbrush);
				if(!r)Log.print_err("ERROR in drawrect(): failed SelectObject(oldbrush)\n");
		}
		if(newpen)
		{
				r32=DeleteObject(newpen);
				if(!r32)Log.print_err("ERROR in drawrect(): failed DeleteObject(newpen)\n");
		}
}

void CanvasImp::DrawWidget(int x1,int y1,int x2,int y2,int id)
{
		if(id<0||id>=BOX_NUM)
		{
				Log.print_err("ERROR in box_draw(): invalid id=%d\n",id);
				return;
		}
		int i=boxindex[id];
		if(i<0||i>=THEME_NM)
		{
				Log.print_err("ERROR in box_draw(): invalid index=%d\n",i);
				return;
		}
		DrawFilledRect(x1,y1,x2,y2,D_C(i),D_C(i+1),D_1(i+2),D_X(i+3));
		DrawImage(*vTheme->GetImage(id),x1,y1,x2,y2,D_1(i+5),D_1(i+6));
}

void CanvasImp::DrawCheckbox(int x1,int y1,int wx,int wy,int checked,int active,int type)
{
		RECT rect;
		int i=4+(active?1:0)+(checked?2:0)+(type?4:0);

		rect.left=x1;
		rect.top=y1;
		rect.right=x1+wx;
		rect.bottom=y1+wy;

		if((dynamic_cast<ImageImp *>(vTheme->GetIcon(i)))->IsLoaded())
				DrawImage(*vTheme->GetIcon(i),x1,y1,x1+wx,y1+wy,0,Image::HSTR|Image::VSTR);
		else
				DrawFrameControl(hdcMem,&rect,DFC_BUTTON,DFCS_BUTTONCHECK|(checked?DFCS_CHECKED:0));
}

HICON CanvasImp::CreateMirroredIcon(HICON hiconOrg)
{
		HDC hdcScreen,hdcBitmap,hdcMask=nullptr;
		HBITMAP hbm,hbmMask,hbmOld,hbmOldMask;
		BITMAP bm;
		ICONINFO ii;
		HICON hicon=nullptr;

		hdcBitmap=CreateCompatibleDC(nullptr);
		if(hdcBitmap)
		{
				hdcMask=CreateCompatibleDC(nullptr);
				if(hdcMask)
				{
						SetLayout(hdcBitmap,LAYOUT_RTL);
						SetLayout(hdcMask,LAYOUT_RTL);
				}
				else
				{
						DeleteDC(hdcBitmap);
						hdcBitmap=nullptr;
				}
		}
		hdcScreen=GetDC(nullptr);
		if(hdcScreen)
		{
				if(hdcBitmap&&hdcMask)
				{
						if(hiconOrg)
						{
								if(GetIconInfo(hiconOrg,&ii)&&GetObject(ii.hbmColor,sizeof(BITMAP),&bm))
								{
										// Do the cleanup for the bitmaps.
										DeleteObject(ii.hbmMask);
										DeleteObject(ii.hbmColor);
										ii.hbmMask=ii.hbmColor=nullptr;
										hbm=CreateCompatibleBitmap(hdcScreen,bm.bmWidth,bm.bmHeight);
										hbmMask=CreateBitmap(bm.bmWidth,bm.bmHeight,1,1,nullptr);
										hbmOld=static_cast<HBITMAP>(SelectObject(hdcBitmap,hbm));
										hbmOldMask=static_cast<HBITMAP>(SelectObject(hdcMask,hbmMask));
										DrawIconEx(hdcBitmap,0,0,hiconOrg,bm.bmWidth,bm.bmHeight,0,nullptr,DI_IMAGE);
										DrawIconEx(hdcMask,0,0,hiconOrg,bm.bmWidth,bm.bmHeight,0,nullptr,DI_MASK);
										SelectObject(hdcBitmap,hbmOld);
										SelectObject(hdcMask,hbmOldMask);

										// Create the new mirrored icon and delete bitmaps
										ii.hbmMask=hbmMask;
										ii.hbmColor=hbm;
										hicon=CreateIconIndirect(&ii);
										DeleteObject(hbm);
										DeleteObject(hbmMask);
								}
						}
				}
				ReleaseDC(nullptr,hdcScreen);
		}
		if(hdcBitmap)DeleteDC(hdcBitmap);
		if(hdcMask)DeleteDC(hdcMask);
		return hicon;
}
//}

//{Popup
void Popup_t::popup_resize(int x,int y)
{
		if(floating_x!=x||floating_y!=y)
		{
				POINT p1;

				floating_x=x;
				floating_y=y;
				GetCursorPos(&p1);
				SetCursorPos(p1.x+1,p1.y);
				SetCursorPos(p1.x,p1.y);
		}
}

void Popup_t::drawpopup(size_t itembar,int str_id,int type,int x,int y,HWND hwnd)
{
		POINT p={x,y};
		HMONITOR hMonitor;
		MONITORINFO mi;
		int needupdate;

		if((type==FLOATING_CMPDRIVER||type==FLOATING_DRIVERLST)&&itembar==0)type=FLOATING_NONE;
		auto floating_string = *STR(str_id);
		//Log.print_debug("%S\n", floating_string);
		if(type==FLOATING_TOOLTIP && (str_id<=1 || !floating_string)) type=FLOATING_NONE;

		if(rtl)p.x+=floating_x;
		ClientToScreen(hwnd,&p);
		needupdate=floating_itembar!=itembar||floating_type!=type;
		floating_itembar=itembar;
		floating_str_id=str_id;
		floating_type=type;

		if(type!=FLOATING_NONE)
		{
				//if(type==FLOATING_ABOUT)p.y=p.y-floating_y-30;
				//if(type==FLOATING_CMPDRIVER||type==FLOATING_DRIVERLST)
				{
						hMonitor=MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST);
						mi.cbSize=sizeof(MONITORINFO);
						GetMonitorInfo(hMonitor,&mi);

						mi.rcWork.right-=15;
						if(p.x+floating_x>mi.rcWork.right)p.x=mi.rcWork.right-floating_x;
						if(p.x<mi.rcWork.left+5)p.x=mi.rcWork.left+5;
						if(p.y+floating_y>mi.rcWork.bottom-20)p.y=p.y-floating_y-30;
						if(p.y<mi.rcWork.top+5)p.y=mi.rcWork.top+5;
				}

				MoveWindow(hPopup,p.x+10,p.y+20,floating_x,floating_y,1);
				if(needupdate)InvalidateRect(hPopup,nullptr,0);

				{
						TRACKMOUSEEVENT tme;
						tme.cbSize=sizeof(tme);
						tme.hwndTrack=hwnd;
						tme.dwFlags=TME_LEAVE|TME_HOVER;
						tme.dwHoverTime=(MainWindow.ctrl_down||MainWindow.space_down)?0:Settings.hintdelay;

						if(!tme.dwHoverTime)
								onHover();
						else
								TrackMouseEvent(&tme);
				}
		}
		if(type==FLOATING_NONE)ShowWindow(hPopup,SW_HIDE);
}

void Popup_t::onHover()
{
		if(GetForegroundWindow()!=MainWindow.hMain)return;
		wait=false;
		if(MainWindow.kbpanel)return;
		if(floating_type&&rtl)
		{
				/*if(floating_type==FLOATING_CMPDRIVER||floating_type==FLOATING_DRIVERLST)
				{
						rtl=0;
						::setMirroring(hPopup);
						rtl=1;
				}
				else*/
						//::setMirroring(hPopup);
		}
		InvalidateRect(hPopup,nullptr,0);
		ShowWindow(hPopup,floating_type==FLOATING_NONE?SW_HIDE:SW_SHOWNOACTIVATE);
}

void Popup_t::onLeave()
{
		wait=false;
		ShowWindow(hPopup,SW_HIDE);
}

void Popup_t::setMirroring()
{
		::setMirroring(hPopup);
		InvalidateRect(hPopup,nullptr,0);
}

void Popup_t::setTransparency()
{
		SetLayeredWindowAttributes(hPopup,0,(BYTE)D_1(POPUP_TRANSPARENCY),LWA_ALPHA);
}

void Popup_t::getPos(long int *x,long int *y)
{
		POINT p;
		p.x=0;
		p.y=0;
		ClientToScreen(hPopup,&p);
		*x=p.x;
		*y=p.y;
}
//}

void format_size(wchar_t *buf,long long val,int isspeed)
{
#ifdef USE_TORRENT
		StrFormatSize(val,buf,BUFLEN);
#else
		buf[0]=0;
		UNREFERENCED_PARAMETER(val);
#endif
		if(isspeed)wcscat(buf,STR(STR_UPD_SEC));
}

void format_time(wchar_t *buf,long long val)
{
		long long days,hours,mins,secs;

		secs=val/1000;
		mins=secs/60;
		hours=mins/60;
		days=hours/24;

		secs%=60;
		mins%=60;
		hours%=24;

		wcscpy(buf,L"\x221E");
		if(secs) wsprintf(buf,L"%d %s",static_cast<int>(secs),STR(STR_UPD_TSEC));
		if(mins) wsprintf(buf,L"%d %s %d %s",static_cast<int>(mins),STR(STR_UPD_TMIN),static_cast<int>(secs),STR(STR_UPD_TSEC));
		if(hours)wsprintf(buf,L"%d %s %d %s",static_cast<int>(hours),STR(STR_UPD_THOUR),static_cast<int>(mins),STR(STR_UPD_TMIN));
		if(days) wsprintf(buf,L"%d %s %d %s",static_cast<int>(days),STR(STR_UPD_TDAY),static_cast<int>(hours),STR(STR_UPD_THOUR));
}
