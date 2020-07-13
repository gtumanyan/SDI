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

#ifndef GUI_H
#define GUI_H

#include "themelist.h"

// Declarations
class Canvas;
class WidgetVisitor;
class WidgetComposite;

// Global vars
extern WidgetComposite *wPanels;

//{ ### Command ###
class Command
{
public:
    virtual ~Command(){}
    virtual void LeftClick(bool=false){}
    virtual void RightClick(int,int){}

    virtual void UpdateCheckbox(bool *){}
    virtual int  GetBitfieldState(){return 0;}
    virtual int  GetActionID(){return -1;}
};

// Checkboxes
class FiltersCommand:public Command
{
public:
    int action_id;

public:
    FiltersCommand(int a):action_id(a){}
    void UpdateCheckbox(bool *checked);
    int  GetBitfieldState(){return 1<<action_id;}
    int  GetActionID(){return action_id;}
};

class ExpertmodeCheckboxCommand:public Command
{
public:
    void UpdateCheckbox(bool *checked);
    void LeftClick(bool);
    int  GetActionID(){return ID_EXPERT_MODE;}
};

class RestPointCheckboxCommand:public Command
{
public:
    void LeftClick(bool);
    void RightClick(int x,int y);
    int  GetActionID(){return ID_RESTPNT;}
};

class RebootCheckboxCommand:public Command
{
public:
    void LeftClick(bool);
    int  GetActionID(){return ID_REBOOT;}
};

// Misc
class AboutCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class SysInfoCommand:public Command
{
public:
    void LeftClick(bool=false);
    void RightClick(bool=false);
};

// Buttons
class RefreshCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class SnapshotCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class ExtractCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class DrvDirCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class DrvOptionsCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class InstallCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class SelectAllCommand:public Command
{
public:
    void LeftClick(bool=false);
};

class SelectNoneCommand:public Command
{
public:
    void LeftClick(bool=false);
};
//}

//{ ### Widget ###
/*
    Widget
    ├───wLang
    │   └───wTheme
    ├───wCheckbox
    ├───wButton
    │   └───wButtonInst
    ├───wText
    │   ├───wTextSys1
    │   │   ├───wTextSys2
    │   │   └───wTextSys3
    │   └────TextRev
    └───WidgetComposite
        └───wPanel
            └───wLogo
*/

class Widget
{
public:
    int x1=0,y1=0,wx=0,wy=0;

public:
    Command *command=nullptr;
    Widget *parent=nullptr;
    bool isSelected=false;
    int str_id;
    int flags=0;

public:
    Widget(int str_id_):str_id(str_id_){}
    virtual ~Widget(){delete command;}
    virtual void NextItem(){}
    virtual void PrevItem(){}
    virtual void draw(Canvas &){}
    virtual void arrange(){}
    virtual bool SetFocus(){return false;}
    virtual bool IsFocused(Widget *){return false;}
    virtual bool IsHidden(){return false;}

    void setboundbox(int v1,int v2,int v3,int v4){x1=v1;y1=v2;wx=v3;wy=v4;}
    void hitscan(int x,int y);
    void invalidate();

    virtual void Accept(WidgetVisitor &);
};

// WidgetComposite
class WidgetComposite:public Widget
{
protected:
    int cur_item=0;
    int num=0;
    Widget *widgets[20];

public:
    void NextItem(){for(int i=0;i<num;i++)widgets[i]->NextItem();}
    void PrevItem(){for(int i=0;i<num;i++)widgets[i]->PrevItem();}
    void NextPanel();
    void PrevPanel();
    virtual void Add(Widget *w)
    {
        widgets[num]=w;
        widgets[num]->parent=this;
        num++;
    }
    void Accept(WidgetVisitor &);
    WidgetComposite():Widget(0){}
    ~WidgetComposite()
    {
        for(int i=0;i<num;i++)delete widgets[i];
    }
    bool IsFocused(Widget *a)
    {
        return widgets[cur_item]==a;
    }
    void draw(Canvas &canvas)
    {
        if(num-1>=0)widgets[num-1]->draw(canvas);
        for(int i=0;i<num-1;i++)widgets[i]->draw(canvas);
    }
    void arrange()
    {
        for(int i=0;i<num;i++)widgets[i]->arrange();
    }
};

// wPanel
class wPanel:public WidgetComposite
{
    int sz,indofs,boxi,kb;
    bool isAdvanced;
    int kbi;

public:
    wPanel(int sz_,int box_,int kb_=0,bool isAdv=false,int kbi_=0):sz(sz_),indofs(((box_-BOX_PANEL)/2)*18),boxi(box_),kb(kb_),isAdvanced(isAdv),kbi(kbi_){}
    bool IsHidden(){return (!Settings.expertmode&&isAdvanced)||wy==0;}
    void PrevItem();
    void NextItem();
    bool IsFocused(Widget *a);
    void Accept(WidgetVisitor &);
    void arrange();
    void draw(Canvas &canvas);

    friend class WidgetComposite;
};

// wText
class wText:public Widget
{
public:
    void Accept(WidgetVisitor &visitor);
    void draw(Canvas &canvas);
    wText(int str_id_):Widget(str_id_){}
};

// wLogo
class wLogo:public wPanel
{
public:
    void Accept(WidgetVisitor &visitor);
    wLogo(int sz_,int box_):wPanel{sz_,box_}{}
};

// wLang
class wLang:public Widget
{
public:
    void arrange();
    wLang():Widget(0){}
};

// wLang
class wTheme:public wLang
{
public:
    void arrange();
    wTheme():wLang(){}
};

// wTextRev
class wTextRev:public wText
{
public:
    void Accept(WidgetVisitor &visitor);
    void draw(Canvas &canvas);
    wTextRev():wText(0){}
};

// wTextSys1
class wTextSys1:public wText
{
public:
    void Accept(WidgetVisitor &visitor);
    void draw(Canvas &canvas);
    wTextSys1():wText(0){}
};

// wTextSys2
class wTextSys2:public wTextSys1
{
public:
    void draw(Canvas &canvas);
};

// wTextSys3
class wTextSys3:public wTextSys1
{
public:
    void draw(Canvas &canvas);
};

// wCheckbox
class wCheckbox:public Widget
{
public:
    bool checked=false;

public:
    bool SetFocus(){return true;}
    void draw(Canvas &canvas);
    void Accept(WidgetVisitor &visitor);
    wCheckbox(int str_id_,Command *c):Widget(str_id_){command=c;}
};

// wButton
class wButton:public Widget
{
public:
    bool SetFocus(){return true;}
    void Accept(WidgetVisitor &visitor);
    void draw(Canvas &canvas);
    wButton(int str_id_,Command *c):Widget(str_id_){command=c;}
};

// wButtonInst
class wButtonInst:public wButton
{
public:
    void draw(Canvas &canvas);
    wButtonInst(int str_id_,Command *c):wButton(str_id_,c){}
};
//}

//{ ### Visiters ###
class WidgetVisitor
{
public:
    virtual ~WidgetVisitor(){}

    virtual void VisitWidget(Widget *){}
    virtual void VisitWidgetComposite(WidgetComposite *a){VisitWidget(a);}
    virtual void VisitwText(wText *a){VisitWidget(a);}
    virtual void VisitwCheckbox(wCheckbox *a){VisitWidget(a);}
    virtual void VisitwButton(wButton *a){VisitWidget(a);}
    virtual void VisitwLogo(wLogo *a){VisitWidget(a);}
    virtual void VisitwTextRev(wTextRev *a){VisitwText(a);}
    virtual void VisitwTextSys1(wTextSys1 *a){VisitwText(a);}
};

class HoverVisiter:public WidgetVisitor
{
    int x,y;
    bool popup_active=false;

public:
    HoverVisiter(int xv,int yv):x(xv),y(yv){}
    ~HoverVisiter();

    void VisitWidget(Widget *);
    void VisitWidgetComposite(WidgetComposite *);
    void VisitwText(wText *);
    void VisitwLogo(wLogo *);
    void VisitwTextRev(wTextRev *);
    void VisitwTextSys1(wTextSys1 *);
};

enum class CHECKBOX
{
    SET,
    CLEAR,
    TOGGLE,
    GET,
};

class ClickVisiter:public WidgetVisitor
{
    int x,y;
    int action_id;
    int sum=0;

    CHECKBOX act;
    int value=0;
    bool right;
    bool triggered=false;

public:
    ClickVisiter(int xv,int yv,bool rightv=false):x(xv),y(yv),action_id(0),act(CHECKBOX::TOGGLE),right(rightv){}
    ClickVisiter(int id,CHECKBOX c=CHECKBOX::TOGGLE):x(0),y(0),action_id(id),act(c),right(0){}
    ~ClickVisiter();
    int GetValue(){return value;}

    void VisitwCheckbox(wCheckbox *);
    void VisitwButton(wButton *);
    void VisitwTextSys1(wTextSys1 *);
};
//}

//{ ### Text ###
class textdata_t
{
protected:
    Canvas *pcanvas;

    int ofsx;
    int wy;
    int maxsz;

public:
    int col;
    int x;
    int y;

protected:
    void TextOut_CM(int x,int y,const wchar_t *str,int color,int *maxsz,int mode);

public:
    textdata_t(Canvas &canvas,int ofsx=0);

    void TextOutF(int col,const wchar_t *format,...);
    void TextOutF_RTL(int col,int wx,const wchar_t *format,...);
    void TextOutF(const wchar_t *format,...);
    void TextOutBold(const wchar_t *format,...);
    void ret();
    void nl();
    void ret_ofs(int a);
    int getX(){return x;}
    int getY(){return y;}
};

class textdata_horiz_t:public textdata_t
{
    size_t i;
	int *limits;
    int mode;

public:
    textdata_horiz_t(Canvas &canvas,int ofsx1,int *lim,int mode1):textdata_t(canvas,ofsx1),i(0),limits(lim),mode(mode1){}
    void limitskip();
    void TextOutP(const wchar_t *format,...);
};

class textdata_vert:public textdata_t
{

public:
    textdata_vert(Canvas &canvas,int ofsx1=0):textdata_t(canvas,ofsx1)
    {

    }
    void shift_r();
    void shift_l();
    int getMaxsz(){return maxsz;}
    void TextOutSF(const wchar_t *str,const wchar_t *format,...);
};
//}

// Misc functions
void drawnew(Canvas &canvas);
bool isRebootDesired();

int Xg(int x,int o);
int Yg(int y);
int XG(int x,int o);
int YG(int y,int o);

int mirw(int x,int ofs,int w);
int Xm(int x,int o);
int Ym(int y);
int XM(int x,int o);
int YM(int y,int o);

#endif
