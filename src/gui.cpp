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

#include <windows.h>
#include <windowsx.h>

#include "SDI.h"
#include "system.h"
#include "Settings.h"
#include "VersionEx.h"
#include "manager.h"
#include "theme.h"
#include "gui.h"
#include "draw.h"
#include "update.h"
#include "resource.h"

#include "enum.h"

static BOOL log_displayed = FALSE;

//{ Global vars
WidgetComposite *wPanels=nullptr;
//}

int update_progress_type = UPT_PERCENT;
// (empty) check box width, (empty) drop down width, button height (for and without dropdown match)
int cbw, ddw, ddbh = 0, bh = 0;

/*
 * The following is used to allocate slots within the progress bar
 * 0 means unused (no operation or no progress allocated to it)
 * +n means allocate exactly n bars (n percent of the progress bar)
 * -n means allocate a weighted slot of n from all remaining
 *    bars. E.g. if 80 slots remain and the sum of all negative entries
 *    is 10, -4 will allocate 4/10*80 = 32 bars (32%) for OP progress
 */
static int nb_slots[OP_MAX];
static float slot_end[OP_MAX+1];	// shifted +1 so that we can subtract 1 to OP indexes
static float previous_end;

// Move a control along the Y axis
static __inline void MoveCtrlY(HWND hDlg, int nID, int vertical_shift) {
	ResizeMoveCtrl(hDlg, GetDlgItem(hDlg, nID), 0, vertical_shift, 0, 0, 1.0f);
}

void GetRelativeCtrlRect(HWND hWnd,RECT *rc)
{
		GetWindowRect(hWnd,rc);
		ScreenToClient(GetParent(hWnd),(LPPOINT)&((LPPOINT)rc)[0]);
		ScreenToClient(GetParent(hWnd),(LPPOINT)&((LPPOINT)rc)[1]);
		//MapWindowPoints(nullptr,hWnd,(LPPOINT)&rc,2);
		rc->right-=rc->left;
		rc->bottom-=rc->top;
}

// Position the progress bar within each operation range
void UpdateProgress(int op, float percent)
{
	int pos;
	static uint64_t LastRefresh = 0;

	if ((op < 0) || (op >= OP_MAX)) {
		duprintf("UpdateProgress: invalid op %d\n", op);
		return;
	}
	if (percent > 100.1f) {
		// duprintf("UpdateProgress(%d): invalid percentage %0.2f\n", op, percent);
		return;
	}
	if ((percent < 0.0f) && (nb_slots[op] <= 0)) {
		duprintf("UpdateProgress(%d): error negative percentage sent for negative slot value\n", op);
		return;
	}
	if (nb_slots[op] == 0)
		return;
	if (previous_end < slot_end[op]) {
		previous_end = slot_end[op];
	}

	if (percent < 0.0f) {
		// Negative means advance one slot (1.0%) - requires a positive slot allocation
		previous_end += (slot_end[op + 1] - slot_end[op]) / (1.0f * nb_slots[op]);
		pos = (int)(previous_end / 100.0f * MAX_PROGRESS);
	} else {
		pos = (int)((previous_end + ((slot_end[op + 1] - previous_end) * (percent / 100.0f))) / 100.0f * MAX_PROGRESS);
	}
	if (pos > MAX_PROGRESS) {
		duprintf("UpdateProgress(%d): rounding error - pos %d is greater than %d", op, pos, MAX_PROGRESS);
		pos = MAX_PROGRESS;
	}

	// Reduce the refresh rate, to avoid weird effects on the sliding part of progress bar
	if (GetTickCount64() > LastRefresh + (2 * MAX_REFRESH)) {
		LastRefresh = GetTickCount64();
		SendMessage(hProgress, PBM_SETPOS, (WPARAM)pos, 0);
		SetTaskbarProgressValue(pos, MAX_PROGRESS);
	}
}

/*
 * The following is taken from GNU wget (progress.c)
 */
struct bar_progress {
    uint64_t total_length;          // expected total byte count when the download finishes
    uint64_t count;                 // bytes downloaded so far
    uint64_t last_screen_update;    // time of the last screen update, measured since the beginning of download.
    uint64_t dltime;                // download time so far
    // Keep track of recent download speeds.
    struct bar_progress_hist {
        uint64_t pos;
        uint64_t times[SPEED_HISTORY_SIZE];
        uint64_t bytes[SPEED_HISTORY_SIZE];
        // The sum of times and bytes respectively, maintained for efficiency.
        uint64_t total_time;
        uint64_t total_bytes;
    } hist;
    uint64_t recent_start;          // timestamp of beginning of current position.
    uint64_t recent_bytes;          // bytes downloaded so far.
    BOOL stalled;                   // set when no data arrives for longer than STALL_START_TIME, then reset when new data arrives.

    // The following are used to make sure that ETA information doesn't flicker.
    uint64_t last_eta_time;         // time of the last update to download speed and ETA, measured since the beginning of download.
    int last_eta_value;
};

// This code attempts to maintain the notion of a "current" download speed, over the course
// of no less than 3s. (Shorter intervals produce very erratic results.)
//
// To do so, it samples the speed in 150ms intervals and stores the recorded samples in a
// FIFO history ring. The ring stores no more than 20 intervals, hence the history covers
// the period of at least three seconds and at most 20 reads into the past. This method
// should produce reasonable results for downloads ranging from very slow to very fast.
//
// The idea is that for fast downloads, we get the speed over exactly the last three seconds.
// For slow downloads (where a network read takes more than 150ms to complete), we get the
// speed over a larger time period, as large as it takes to complete twenty reads. This is
// good because slow downloads tend to fluctuate more and a 3-second average would be too
// erratic.
static void bar_update(struct bar_progress* bp, uint64_t howmuch, uint64_t dltime)
{
    auto hist = &bp->hist;
    uint64_t recent_age = dltime - bp->recent_start;

    // Update the download count.
    bp->recent_bytes += howmuch;

    // For very small time intervals, we return after having updated the
    // "recent" download count. When its age reaches or exceeds minimum
    // sample time, it will be recorded in the history ring.
    if (recent_age < SPEED_SAMPLE_MIN)
        return;

    if (howmuch == 0) {
        // If we're not downloading anything, we might be stalling,
        // i.e. not downloading anything for an extended period of time.
        // Since 0-reads do not enter the history ring, recent_age
        // effectively measures the time since last read.
        if (recent_age >= STALL_START_TIME) {
            // If we're stalling, reset the ring contents because it's
            // stale and because it will make bar_update stop printing
            // the (bogus) current bandwidth.
            bp->stalled = TRUE;
            memset(hist, 0, sizeof(struct bar_progress::bar_progress_hist));
            bp->recent_bytes = 0;
        }
        return;
    }

    // We now have a non-zero amount of to store to the speed ring.

    // If the stall status was acquired, reset it.
    if (bp->stalled) {
        bp->stalled = FALSE;
        // "recent_age" includes the entire stalled period, which
        // could be very long. Don't update the speed ring with that
        // value because the current bandwidth would start too small.
        // Start with an arbitrary (but more reasonable) time value and
        // let it level out.
        recent_age = 1000;
    }

    // Store "recent" bytes and download time to history ring at the position POS.

    // To correctly maintain the totals, first invalidate existing data
    // (least recent in time) at this position. */
    hist->total_time -= hist->times[hist->pos];
    hist->total_bytes -= hist->bytes[hist->pos];

    // Now store the new data and update the totals.
    hist->times[hist->pos] = recent_age;
    hist->bytes[hist->pos] = bp->recent_bytes;
    hist->total_time += recent_age;
    hist->total_bytes += bp->recent_bytes;

    // Start a new "recent" period.
    bp->recent_start = dltime;
    bp->recent_bytes = 0;

    // Advance the current ring position.
    if (++hist->pos == SPEED_HISTORY_SIZE)
        hist->pos = 0;
}

// This updates the progress bar as well as the data displayed on it so that we can
// display percentage completed, rate of transfer and estimated remaining duration.
// During init (op = OP_INIT) an optional HWND can be passed on which to look for
// a progress bar. Part of the code (eta, speed) comes from GNU wget.
void _UpdateProgressWithInfo(int op, int msg, uint64_t processed, uint64_t total, BOOL force)
{
    static int last_update_progress_type = UPT_PERCENT;
    static struct bar_progress bp = { 0 };
    HWND hProgressDialog = (HWND)(uintptr_t)processed;
    static HWND hProgressBar = NULL;
    static uint64_t start_time = 0, last_refresh = 0;
    uint64_t speed = 0, current_time = GetTickCount64();
    double percent = 0.0;
    char msg_data[128];
    static BOOL bNoAltMode = FALSE;

    if (op == OP_INIT) {
        start_time = current_time - 1;
        last_refresh = 0;
        last_update_progress_type = UPT_PERCENT;
        percent = 0.0f;
        speed = 0;
        memset(&bp, 0, sizeof(bp));
        bp.total_length = total;
        hProgressBar = NULL;
        bNoAltMode = (BOOL)msg;
        if (hProgressDialog != NULL) {
            // Use the progress control provided, if any
            hProgressBar = GetDlgItem(hProgressDialog, IDC_PROGRESS);
            if (hProgressBar != NULL) {
                SendMessage(hProgressBar, PBM_SETSTATE, (WPARAM)PBST_NORMAL, 0);
                SendMessage(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                SendMessage(hProgressBar, PBM_SETPOS, 0, 0);
            }
            SendMessage(hProgressDialog, UM_PROGRESS_INIT, 0, 0);
        }
    }
    else if ((hProgressBar != NULL) || (op > 0)) {
        uint64_t dl_total_time = current_time - start_time;
        uint64_t howmuch = processed - bp.count;
        bp.count = processed;
        bp.total_length = total;
        if (bp.count > bp.total_length)
            bp.total_length = bp.count;
        if (bp.total_length > 0)
            percent = (100.0f * bp.count) / (1.0f * bp.total_length);
        else
            percent = 0.0f;

        if ((bp.hist.total_time > 999) && (bp.hist.total_bytes != 0)) {
            // Calculate the download speed using the history ring and
            // recent data that hasn't made it to the ring yet.
            uint64_t dlquant = bp.hist.total_bytes + bp.recent_bytes;
            uint64_t dltime = bp.hist.total_time + (dl_total_time - bp.recent_start);
            speed = (dltime == 0) ? 0 : (dlquant * 1000) / dltime;
        }
        else {
            speed = 0;
        }
        bar_update(&bp, howmuch, dl_total_time);

        if (bNoAltMode)
            update_progress_type = UPT_PERCENT;
        switch (update_progress_type) {
        case UPT_SPEED:
            if (speed != 0)
                static_sprintf(msg_data, "%s/s", SizeToHumanReadable(speed, FALSE, FALSE));
            else
                static_sprintf(msg_data, "---");
            break;
        case UPT_ETA:
            if ((bp.total_length > 0) && (bp.count > 0) && (dl_total_time > 3000)) {
                uint32_t eta = 0;

                // Don't change the value of ETA more than approximately once
                // per second; doing so would cause flashing without providing
                // any value to the user.
                if ((bp.total_length != processed) && (bp.last_eta_value != 0) &&
                    (dl_total_time - bp.last_eta_time < ETA_REFRESH_INTERVAL)) {
                    eta = bp.last_eta_value;
                }
                else {
                    // Calculate ETA using the average download speed to predict
                    // the future speed. If you want to use a speed averaged
                    // over a more recent period, replace dl_total_time with
                    // hist->total_time and bp->count with hist->total_bytes.
                    // I found that doing that results in a very jerky and
                    // ultimately unreliable ETA.
                    uint64_t bytes_remaining = bp.total_length - processed;
                    double d_eta = (dl_total_time / 1000.0) * (bytes_remaining * 1.0) / (bp.count * 1.0);
                    if (d_eta >= INT_MAX - 1)
                        goto skip_eta;
                    eta = (uint32_t)(d_eta + 0.5);
                    bp.last_eta_value = eta;
                    bp.last_eta_time = dl_total_time;
                }
                static_sprintf(msg_data, "%d:%02d:%02d", eta / 3600, (uint16_t)((eta % 3600) / 60), (uint16_t)(eta % 60));
            }
            else {
            skip_eta:
                static_sprintf(msg_data, "-:--:--");
            }
            break;
        default:
            static_sprintf(msg_data, "%0.1f%%", percent);
            break;
        }
        if ((force) || (bp.count == bp.total_length) || (current_time > last_refresh + MAX_REFRESH)) {
            if (op < 0) {
                SendMessage(hProgressBar, PBM_SETPOS, (WPARAM)(MAX_PROGRESS * percent / 100.0f), 0);
                if (op == OP_NOOP_WITH_TASKBAR)
                    SetTaskbarProgressValue((ULONGLONG)(MAX_PROGRESS * percent / 100.0f), MAX_PROGRESS);
            }
            else {
                UpdateProgress(op, (float)percent);
            }
            if ((force) || ((msg >= 0) && ((current_time > bp.last_screen_update + SCREEN_REFRESH_INTERVAL) ||
                (last_update_progress_type != update_progress_type) || (bp.count == bp.total_length)))) {
                PrintInfo(0, msg, msg_data);
                bp.last_screen_update = current_time;
            }
            last_refresh = current_time;
        }
        last_update_progress_type = update_progress_type;
    }
}

class autorun
{
public:
    autorun()
    {
        wPanel *p,*r;
        wPanels=new WidgetComposite;

        // SysInfo
        p=new wPanel{3,BOX_PANEL1};
        p->Add(new wTextSys1);
        p->Add(new wTextSys2);
        p->Add(new wTextSys3);
        wPanels->Add(p);

        // Theme/lang
        p=new wPanel{5,BOX_PANEL3,KB_EXPERT};
        p->Add(new wText    {STR_LANG});
        p->Add(new wLang);
        p->Add(new wText    {STR_THEME});
        p->Add(new wTheme);
        p->Add(new wCheckbox{STR_EXPERT,            new ExpertmodeCheckboxCommand});
        wPanels->Add(p);

        // Install
        p=new wPanel{3,BOX_PANEL2};
        wPanels->Add(p);

        // Install button
        r=new wPanel{1,BOX_PANEL9,KB_INSTALL,false,1};
        r->Add(new wButtonInst{STR_INSTALL,         new InstallCommand});
        wPanels->Add(r);

        // Select all button
        r=new wPanel{1,BOX_PANEL10,KB_INSTALL,false,2};
        r->Add(new wButton  {STR_SELECT_ALL,        new SelectAllCommand});
        wPanels->Add(r);

        // Select none button
        r=new wPanel{1,BOX_PANEL11,KB_INSTALL,false,3};
        r->Add(new wButton  {STR_SELECT_NONE,       new SelectNoneCommand});
        wPanels->Add(r);

        // Actions
        p=new wPanel{4,BOX_PANEL4,KB_ACTIONS,true};
        p->Add(new wButton  {STR_REFRESH,           new RefreshCommand});
        p->Add(new wButton  {STR_SNAPSHOT,          new SnapshotCommand});
        p->Add(new wButton  {STR_EXTRACT,           new ExtractCommand});
        //p->Add(new wButton  {STR_DRVDIR,            new DrvDirCommand});
        p->Add(new wButton  {STR_OPTIONS_BTN,       new DrvOptionsCommand});
        wPanels->Add(p);

        // Filters (found)
        p=new wPanel{7,BOX_PANEL5,KB_PANEL1,true};
        p->Add(new wText    {STR_SHOW_FOUND});
        p->Add(new wCheckbox{STR_SHOW_MISSING,      new FiltersCommand{ID_SHOW_MISSING}});
        p->Add(new wCheckbox{STR_SHOW_NEWER,        new FiltersCommand{ID_SHOW_NEWER}});
        p->Add(new wCheckbox{STR_SHOW_CURRENT,      new FiltersCommand{ID_SHOW_CURRENT}});
        p->Add(new wCheckbox{STR_SHOW_OLD,          new FiltersCommand{ID_SHOW_OLD}});
        p->Add(new wCheckbox{STR_SHOW_BETTER,       new FiltersCommand{ID_SHOW_BETTER}});
        p->Add(new wCheckbox{STR_SHOW_WORSE_RANK,   new FiltersCommand{ID_SHOW_WORSE_RANK}});
        wPanels->Add(p);

        // Filters (not found)
        p=new wPanel{4,BOX_PANEL6,KB_PANEL2,true};
        p->Add(new wText    {STR_SHOW_NOTFOUND});
        p->Add(new wCheckbox{STR_SHOW_NF_MISSING,   new FiltersCommand{ID_SHOW_NF_MISSING}});
        p->Add(new wCheckbox{STR_SHOW_NF_UNKNOWN,   new FiltersCommand{ID_SHOW_NF_UNKNOWN}});
        p->Add(new wCheckbox{STR_SHOW_NF_STANDARD,  new FiltersCommand{ID_SHOW_NF_STANDARD}});
        wPanels->Add(p);

        // Filters (special)
        p=new wPanel{3,BOX_PANEL7,KB_PANEL3,true};
        p->Add(new wCheckbox{STR_SHOW_ONE,          new FiltersCommand{ID_SHOW_ONE}});
        p->Add(new wCheckbox{STR_SHOW_DUP,          new FiltersCommand{ID_SHOW_DUP}});
        p->Add(new wCheckbox{STR_SHOW_INVALID,      new FiltersCommand{ID_SHOW_INVALID}});
        wPanels->Add(p);

        // Options
        p=new wPanel{3,BOX_PANEL12,KB_PANEL_CHK};
        p->Add(new wText    {STR_OPTIONS});
        p->Add(new wCheckbox{STR_RESTOREPOINT,      new RestPointCheckboxCommand});
        p->Add(new wCheckbox{STR_REBOOT,            new RebootCheckboxCommand});
        wPanels->Add(p);

        // Logo - STR{0} used to be STR_EMPTY
        p=new wLogo{1,BOX_PANEL13};
        //p->Add(new wText    {0});
        wPanels->Add(p);

        // Revision
        p=new wPanel{1,BOX_PANEL8};
        p->Add(new wTextRev);
        wPanels->Add(p);
    }
};
autorun obj;

void drawnew(Canvas &canvas)
{
    wPanels->draw(canvas);
}

void FiltersCommand::UpdateCheckbox(bool *checked)
{
    *checked=(Settings.filters&(1<<action_id))?true:false;
}

void ExpertmodeCheckboxCommand::UpdateCheckbox(bool *checked)
{
    *checked=Settings.expertmode!=0;
}

void wPanel::arrange()
{
    int ofsx=(D_X(PNLITEM_OFSX)), ofsy=D_X(PNLITEM_OFSY);
    int wy1=D_X(PANEL_WY+indofs);

    x1=Xm(D_X(PANEL_OFSX+indofs),D_X(PANEL_WX+indofs));
    y1=Ym(D_X(PANEL_OFSY+indofs));
    wx=XM(D_X(PANEL_WX+indofs),D_X(PANEL_OFSX+indofs));
    wy=wy1*sz+ofsy*2;

    if(!D_1(PANEL_WY+indofs))wy=0;

    for(cur_item=0;cur_item<num;cur_item++)if(widgets[cur_item]->SetFocus())break;
    for(int i=0;i<num;i++)
    {
        widgets[i]->flags=D_1(PANEL_OUTLINE_WIDTH+indofs)<0?1:0;
        widgets[i]->setboundbox(x1+ofsx,y1+ofsy+i*D_X(PNLITEM_WY),wx-ofsx*2,wy1);
        widgets[i]->arrange();
    }

    if(D_1(PANEL_OUTLINE_WIDTH+indofs)<0)
    {
        flags=1;
        x1+=ofsx;
        y1+=ofsy;
        wx-=ofsx*2;
        wy-=ofsy*2;
    }
    else
        flags=0;
}

void wPanel::PrevItem()
{
    if(MainWindow.kbpanel==kb&&cur_item>0&&widgets[cur_item-1]->SetFocus())
        cur_item--;
    else
    {
        cur_item=num-1;
        if(cur_item<0)cur_item=0;
    }
}
void wPanel::NextItem()
{
    if(MainWindow.kbpanel==kb&&cur_item<num-1)
        cur_item++;
    else
    {
        cur_item=0;
        while(cur_item<num&&!widgets[cur_item]->SetFocus())cur_item++;
    }

}

void WidgetComposite::NextPanel()
{
    if(MainWindow.kbpanel==KB_FIELD||MainWindow.kbpanel==KB_LANG||MainWindow.kbpanel==KB_THEME)
    {
        MainWindow.kbpanel++;
        return;
    }
    while(1)
    {
        cur_item++;
        if(cur_item>=num)
        {
            cur_item=0;
            MainWindow.kbpanel=KB_FIELD;
            break;
        }
        int newkb=dynamic_cast<wPanel*>(widgets[cur_item])->kb;
        if(newkb&&newkb!=MainWindow.kbpanel&&!widgets[cur_item]->IsHidden())
        {
            MainWindow.kbpanel=newkb;
            break;
        }
    }
}
void WidgetComposite::PrevPanel()
{
    if(MainWindow.kbpanel==KB_EXPERT||MainWindow.kbpanel==KB_THEME||MainWindow.kbpanel==KB_LANG)
    {
        MainWindow.kbpanel--;
        return;
    }
    while(1)
    {
        cur_item--;
        if(cur_item<0)cur_item=num-1;
        int newkb=dynamic_cast<wPanel*>(widgets[cur_item])->kb;
        if(newkb&&newkb!=MainWindow.kbpanel&&!widgets[cur_item]->IsHidden())
        {
            MainWindow.kbpanel=newkb;
            break;
        }
    }
}

void Widget::hitscan(int x,int y)
{
    bool newisSelected=(x>=x1&&x<x1+wx&&y>=y1&&y<y1+wy);

    if(MainWindow.kbpanel)newisSelected=parent&&parent->IsFocused(this);

    if(newisSelected!=isSelected)
    {
        invalidate();
        isSelected=newisSelected;
    }
}

void Widget::invalidate()
{
    RECT rect;

    rect.left=x1;
    rect.top=y1;
    rect.right=x1+wx;
    rect.bottom=y1+wy;

    if(right_to_left_mode)
        InvalidateRect(MainWindow.hMain,nullptr,0);
    else
        InvalidateRect(MainWindow.hMain,&rect,0);
}

void wLang::arrange()
{
    MainWindow.hLang->Move(x1,y1-5,wx,360);
}

void wTheme::arrange()
{
    MainWindow.hTheme->Move(x1,y1-5,wx,360);
}

void MainWindow_t::theme_refresh(int shift)
{
		hFont->SetFont(D_STR(FONT_NAME),D_X(FONT_SIZE));
		int fz=D_X(POPUP_FONT_SIZE);
		if(fz<10)fz=10;
		Popup->hFontP->SetFont(D_STR(FONT_NAME),fz);
		Popup->hFontBold->SetFont(D_STR(FONT_NAME),fz,true);
		D(POPUP_WY)=fz*120/100*Settings.scale/256;

		hLang->SetFont(hFont);
		hTheme->SetFont(hFont);

		if(!hMain||!hwndFrame)
		{
				uprintf("ERROR in theme_refresh(): hMain is %d, hwndFrame is %d\n",hMain,hwndFrame);
				return;
		}

		if(Settings.autosized)
		{
				MoveWindow(hwndFrame,Xm(D_X(DRVLIST_OFSX),D_X(DRVLIST_WX)),Ym(D_X(DRVLIST_OFSY)),XM(D_X(DRVLIST_WX),D_X(DRVLIST_OFSX)),YM(D_X(DRVLIST_WY),D_X(DRVLIST_OFSY)),TRUE);
				wPanels->arrange();
				manager_g->setpos();
				MainWindow.redrawmainwnd();
				MainWindow.redrawfield();
				return;
		}

		// Resize window
		RECT rect;
		POINT point;

		GetWindowRect(hMain,&rect);
		MoveWindow(hMain,rect.left,rect.top,D(MAINWND_WX),D(MAINWND_WY)+1,1);
		MoveWindow(hMain,rect.left,rect.top,D(MAINWND_WX),D(MAINWND_WY),1);

		// Resize the log
		GetWindowRect(hLogDialog, &rect);
		point.x = (rect.right - rect.left);
		point.y = (rect.bottom - rect.top);
		MoveWindow(hLogDialog, rect.left, rect.top, point.x, point.y + shift, TRUE);
		MoveCtrlY(hLogDialog, IDC_LOG_CLEAR, shift);
		MoveCtrlY(hLogDialog, IDC_LOG_SAVE, shift);
		MoveCtrlY(hLogDialog, IDCANCEL, shift);
		GetWindowRect(hLog, &rect);
		point.x = (rect.right - rect.left);
		point.y = (rect.bottom - rect.top) + shift;
		SetWindowPos(hLog, NULL, 0, 0, point.x, point.y, SWP_NOZORDER);
		// Don't forget to scroll the edit to the bottom after resize
		Edit_Scroll(hLog, 0, Edit_GetLineCount(hLog));
}

void wPanel::draw(Canvas &canvas)
{
    if(!wy)return;
    if(IsHidden())
    {
        for(int i=0;i<num;i++)widgets[i]->draw(canvas);
        return;
    }

    // Draw panel
    if(kb==KB_INSTALL)
    {
        canvas.DrawWidget(x1,y1,x1+wx,y1+wy,boxi+(widgets[0]->isSelected&&flags?1:0));
    }
    else
        canvas.DrawWidget(x1,y1,x1+wx,y1+wy,boxi+(isSelected&&flags?1:0));

    // Draw childs
    ClipRegion rgn;
    rgn.setRegion(x1,y1,x1+wx,y1+wy);
    canvas.SetClipRegion(rgn);
    for(int i=0;i<num;i++)widgets[i]->draw(canvas);
    canvas.ClearClipRegion();
}

void wText::draw(Canvas &canvas)
{
    if(parent->IsHidden())return;
    canvas.SetTextColor(D_C(isSelected?CHKBOX_TEXT_COLOR_H:CHKBOX_TEXT_COLOR));
    canvas.DrawTextXY(mirw(x1,0,wx),y1+0,STR(str_id));
    //canvas.drawrect(x1,y1,x1+wx,y1+wy,0xFF000000,0xFF,1,0);
}

void wTextRev::draw(Canvas &canvas)
{
    Version v{VERSION_FILEVERSION_NUM };
    WStringShort buf;
    WStringShort date;

    v.str_date(date);
    //buf.sprintf(L"%d.%d",VERSION_FILEVERSION_NUM);
    if(right_to_left_mode)buf.append(L"\u200E");
    canvas.SetTextColor(D_C(CHKBOX_TEXT_COLOR));
    canvas.DrawTextXY(mirw(x1,0,wx),y1,buf.Get());
}

void wCheckbox::draw(Canvas &canvas)
{
    command->UpdateCheckbox(&checked);
    if(parent->IsHidden())return;

    if(isSelected&&MainWindow.kbpanel)
    {
        canvas.DrawWidget(x1,y1,x1+wx,y1+wy-4,BOX_KBHLT);
        //isSelected=false;
    }
    canvas.DrawCheckbox(mirw(x1,0,wx-D_X(CHKBOX_SIZE)-2),y1,D_X(CHKBOX_SIZE)-2,D_X(CHKBOX_SIZE)-2,checked,isSelected,0);
    canvas.SetTextColor(D_C(isSelected?CHKBOX_TEXT_COLOR_H:CHKBOX_TEXT_COLOR));
    canvas.DrawTextXY(mirw(x1,D_X(CHKBOX_TEXT_OFSX),wx),y1,STR(str_id));
}

void wButton::draw(Canvas &canvas)
{
    if(parent->IsHidden())return;
    if(!flags)canvas.DrawWidget(x1,y1,x1+wx,y1+wy-1,isSelected?BOX_BUTTON_H:BOX_BUTTON);

    canvas.SetTextColor(D_C(CHKBOX_TEXT_COLOR));
    canvas.DrawTextXY(mirw(x1,wy/2,wx),y1+(wy-D_X(FONT_SIZE)-2)/2,STR(str_id));
    //canvas.drawrect(x1,y1,x1+wx,y1+wy,0xFF000000,0xFF,1,0);
}

void wButtonInst::draw(Canvas &canvas)
{
    if(!flags)canvas.DrawWidget(x1,y1,x1+wx,y1+wy-1,isSelected?BOX_BUTTON_H:BOX_BUTTON);

    WStringShort buf;
    canvas.SetTextColor(D_C(CHKBOX_TEXT_COLOR));
    buf.sprintf(L"%s (%d)",STR(str_id),manager_g->countItems());
    int nwy=D_X(PANEL9_OFSX)==D_X(PANEL10_OFSX)?D_X(PANEL10_WY):wy;
    canvas.DrawTextXY(mirw(x1,nwy/2,wx),y1+(wy-D_X(FONT_SIZE)-2)/2,buf.Get());
}

void wTextSys1::draw(Canvas &canvas)
{
    canvas.SetTextColor(D_C(CHKBOX_TEXT_COLOR));
    canvas.DrawTextXY(x1+10+SYSINFO_COL0,y1,STR(STR_SHOW_SYSINFO));
    canvas.DrawTextXY(x1+10+SYSINFO_COL1,y1,STR(STR_SYSINF_MOTHERBOARD));
    canvas.DrawTextXY(x1+10+SYSINFO_COL2,y1,STR(STR_SYSINF_ENVIRONMENT));
}

void wTextSys2::draw(Canvas &canvas)
{
    State *state=manager_g->getState();
    WStringShort buf;

    // not enough room for long server product names
    int p=state->getPlatformProductType();
    if(p==2||p==3)
        buf.sprintf(L"%s",state->get_winverstr());
    else
        buf.sprintf(L"%s (%d-bit)%s",state->get_winverstr(),state->getArchitecture()?64:32,right_to_left_mode?L"\u200E":L"");

    canvas.DrawTextXY(x1+10+SYSINFO_COL0,y1,buf.Get());
    canvas.DrawTextXY(x1+10+SYSINFO_COL1,y1,state->getProduct());
    canvas.DrawTextXY(x1+10+SYSINFO_COL2,y1,STR(STR_SYSINF_WINDIR));
    canvas.DrawTextXY(x1+10+SYSINFO_COL3,y1,state->textas.getw(state->getWindir()));
}

void wTextSys3::draw(Canvas &canvas)
{
    State *state=manager_g->getState();
    WStringShort buf;

    //buf.sprintf(L"%s",(wx<10+SYSINFO_COL1)?state->getProduct():state->get_szCSDVersion());
    if(right_to_left_mode)buf.append(L"\u200E");
    canvas.DrawTextXY(x1+10+SYSINFO_COL0,y1,buf.Get());
    //buf.sprintf(L"%s: %s",STR(STR_SYSINF_TYPE),STR(state->isLaptop?STR_SYSINF_LAPTOP:STR_SYSINF_DESKTOP));
    canvas.DrawTextXY(x1+10+SYSINFO_COL1,y1,buf.Get());
    canvas.DrawTextXY(x1+10+SYSINFO_COL2,y1,STR(STR_SYSINF_TEMP));
    canvas.DrawTextXY(x1+10+SYSINFO_COL3,y1,state->textas.getw(state->getTemp()));
}

//{ Accepters
void Widget::Accept(WidgetVisitor &visitor)
{
    visitor.VisitWidget(this);
}

void WidgetComposite::Accept(WidgetVisitor &visitor)
{
    visitor.VisitWidget(this);
    for(int i=0;i<num;i++)widgets[i]->Accept(visitor);
}

bool wPanel::IsFocused(Widget *a)
{
    if(MainWindow.kbpanel==KB_INSTALL&&kb==MainWindow.kbpanel)
    {
        //if(MainWindow.kbinstall<0)MainWindow.kbinstall=2;
        //if(MainWindow.kbinstall>2)MainWindow.kbinstall=0;
        return MainWindow.kbinstall+1==kbi;
    }
    return kb==MainWindow.kbpanel&&WidgetComposite::IsFocused(a);
}

void wPanel::Accept(WidgetVisitor &visitor)
{
    if(!Settings.expertmode&&isAdvanced)return;

    visitor.VisitWidget(this);
    for(int i=0;i<num;i++)widgets[i]->Accept(visitor);
}

void wText::Accept(WidgetVisitor &visitor)
{
    visitor.VisitwText(this);
}

void wCheckbox::Accept(WidgetVisitor &visitor)
{
    visitor.VisitwCheckbox(this);
}

void wButton::Accept(WidgetVisitor &visitor)
{
    visitor.VisitwButton(this);
}

void wLogo::Accept(WidgetVisitor &visitor)
{
    UNREFERENCED_PARAMETER(visitor);
//    visitor.VisitwLogo(this);
}

void wTextRev::Accept(WidgetVisitor &visitor)
{
    UNREFERENCED_PARAMETER(visitor);
//    visitor.VisitwTextRev(this);
}

void wTextSys1::Accept(WidgetVisitor &visitor)
{
    visitor.VisitwTextSys1(this);
}
//}

//{ HoverVisiter
HoverVisiter::~HoverVisiter()
{
    if(popup_active==false)
        Popup->drawpopup(0,0,FLOATING_NONE,x,y,MainWindow.hMain);
}

void HoverVisiter::VisitWidget(Widget *a)
{
    a->hitscan(x,y);
    if(a->isSelected&&a->str_id)
    {
        Popup->drawpopup(0,a->str_id+1,FLOATING_TOOLTIP,x,y,MainWindow.hMain);
        popup_active=true;
    }
}

void HoverVisiter::VisitWidgetComposite(WidgetComposite *a)
{
    a->hitscan(x,y);
}

void HoverVisiter::VisitwText(wText *a)
{
    VisitWidget(a);
    a->isSelected=0;
}

void HoverVisiter::VisitwLogo(wLogo *a)
{
    a->hitscan(x,y);
    if(a->isSelected)
    {
        SetCursor(LoadCursor(nullptr,IDC_HAND));
        Popup->drawpopup(0,0,FLOATING_ABOUT,x,y,MainWindow.hMain);
        popup_active=true;
    }
}

void HoverVisiter::VisitwTextRev(wTextRev *a)
{
    if(popup_active)return;
    a->hitscan(x,y);
    if(a->isSelected)
    {
        SetCursor(LoadCursor(nullptr,IDC_HAND));
        Popup->drawpopup(0,0,FLOATING_ABOUT,x,y,MainWindow.hMain);
        popup_active=true;
    }
}

void HoverVisiter::VisitwTextSys1(wTextSys1 *a)
{
    a->hitscan(x,y);
    if(a->isSelected)
    {
        SetCursor(LoadCursor(nullptr,IDC_HAND));
        Popup->drawpopup(0,a->str_id,FLOATING_SYSINFO,x,y,MainWindow.hMain);
        popup_active=true;
    }
}
//}

//{ ClickVisitor
ClickVisiter::~ClickVisiter()
{
    if(Settings.filters!=sum&&Settings.expertmode)
    {
        Settings.filters=sum;
        manager_g->filter(Settings.filters);
        manager_g->setpos();
    }
}

void ClickVisiter::VisitwCheckbox(wCheckbox *a)
{
    a->hitscan(x,y);
    if(a->isSelected||action_id==a->command->GetActionID())
    {
        triggered=true;
        if(right)
        {
            a->command->RightClick(x,y);
        }
        else
            if(act==CHECKBOX::GET)
                value=a->checked;
            else
            {
                switch(act)
                {
                    case CHECKBOX::SET:
                        if(a->checked)return;
                        a->checked=true;
                        break;

                    case CHECKBOX::CLEAR:
                        if(!a->checked)return;
                        a->checked=false;
                        break;

                    case CHECKBOX::TOGGLE:
                        a->checked^=1;
                        break;

                    case CHECKBOX::GET:
                    default:
                        break;
                }
                a->invalidate();
                a->command->LeftClick(a->checked);
            }
    }

    // Calc filters
    if(a->checked)sum+=a->command->GetBitfieldState();
}

void ExpertmodeCheckboxCommand::LeftClick(bool checked)
{
    Settings.expertmode=checked;

    if (MainWindow.ctrl_down) {
        // Display the log Window
        log_displayed = !log_displayed;
        // Must come last for the log window to get focus
        ShowWindow(hLogDialog, log_displayed ? SW_SHOW : SW_HIDE);
    }
    InvalidateRect(MainWindow.hMain,nullptr,0);
}

void RestPointCheckboxCommand::LeftClick(bool checked)
{
    manager_g->set_rstpnt(checked);
}

void RebootCheckboxCommand::LeftClick(bool checked)
{
    if(checked)
        Settings.flags|=FLAG_AUTOINSTALL;
    else
        Settings.flags&=~FLAG_AUTOINSTALL;
}

void ClickVisiter::VisitwButton(wButton *a)
{
    a->hitscan(x,y);
    if(a->isSelected&&!right)a->command->LeftClick();
}

void ClickVisiter::VisitwTextSys1(wTextSys1 *a)
{
    a->hitscan(x,y);
    if(a->isSelected)
    {
        if(right)
            manager_g->getState()->contextmenu2(x,y);
        else
            System.run_command(L"devmgmt.msc",nullptr,SW_SHOW,0);
    }
}
//}

//{ Text
textdata_t::textdata_t(Canvas &canvas_,int xofs):
    pcanvas(&canvas_),
    ofsx(xofs),
    wy(D_X(POPUP_WY)),
    maxsz(0),
    col(D_C(POPUP_TEXT_COLOR)),
    x(D_X(POPUP_OFSX)+xofs),
    y(D_X(POPUP_OFSY))
{
}

void textdata_t::ret()
{
    x=D_X(POPUP_OFSX)+ofsx;
}

void textdata_t::ret_ofs(int a)
{
    x=D_X(POPUP_OFSX)+a;
}

void textdata_t::nl()
{
    y+=wy;
}

void textdata_horiz_t::limitskip()
{
    x+=limits[i++];
}

void textdata_vert::shift_r(){maxsz+=POPUP_SYSINFO_OFS;}
void textdata_vert::shift_l(){maxsz-=POPUP_SYSINFO_OFS;}

void textdata_t::TextOut_CM(int x1,int y1,const wchar_t *str,int color,int *maxsz1,int mode1)
{
    int ss=pcanvas->GetTextExtent(str);
    if(ss>*maxsz1)*maxsz1=ss;

    if(!mode1)return;
    pcanvas->SetTextColor(color);
    pcanvas->DrawTextXY(x1,y1,str);
}

void textdata_horiz_t::TextOutP(const wchar_t *format,...)
{
    WStringShort buffer;
    va_list args;
    va_start(args,format);
    buffer.vsprintf(format,args);

    TextOut_CM(x,y,buffer.Get(),col,&limits[i],mode);
    x+=limits[i];
    i++;
    va_end(args);
}

void textdata_t::TextOutF(int col1,const wchar_t *format,...)
{
    WStringShort buffer;
    va_list args;
    va_start(args,format);
    buffer.vsprintf(format,args);

    TextOut_CM(x,y,buffer.Get(),col1,&maxsz,1);
    y+=wy;
    va_end(args);
}

void textdata_t::TextOutF_RTL(int col1,int wx,const wchar_t *format,...)
{
    WStringShort buffer;
    va_list args;
    va_start(args,format);
    buffer.vsprintf(format,args);

    int ss=pcanvas->GetTextExtent(buffer.Get());
    TextOut_CM(x-ss+wx-30,y,buffer.Get(),col1,&maxsz,1);
    y+=wy;
    va_end(args);
}

void textdata_t::TextOutF(const wchar_t *format,...)
{
    WStringShort buffer;
    va_list args;
    va_start(args,format);
    buffer.vsprintf(format,args);

    TextOut_CM(x,y,buffer.Get(),col,&maxsz,1);
    y+=wy;
    va_end(args);
}

void textdata_t::TextOutBold(const wchar_t *format,...)
{
    WStringShort buffer;
    va_list args;
    va_start(args,format);
    buffer.vsprintf(format,args);

    pcanvas->SetFont(Popup->hFontBold);
    TextOut_CM(x,y,buffer.Get(),col,&maxsz,1);
    pcanvas->SetFont(Popup->hFontP);
    y+=wy;
    va_end(args);
}

void textdata_vert::TextOutSF(const wchar_t *str,const wchar_t *format,...)
{
    WStringShort buffer;
    va_list args;
    va_start(args,format);
    buffer.vsprintf(format,args);
    TextOut_CM(x,y,str,col,&maxsz,1);
    TextOut_CM((x+POPUP_SYSINFO_OFS),y,buffer.Get(),col,&maxsz,1);
    y+=wy;
    va_end(args);
}
//}

int mirw(int x,int ofs,int w)
{
    UNREFERENCED_PARAMETER(w);
    return x+ofs;
}
int Xm(int x,int o)
{
    UNREFERENCED_PARAMETER(o);
    return x>=0?x:(MainWindow.main1x_c+x);
}
int Ym(int y){return y>=0?y:(MainWindow.main1y_c+y);}
int XM(int w,int x){return w>=0?w:(w+MainWindow.main1x_c-x);}
int YM(int y,int o){return y>=0?y:(MainWindow.main1y_c+y-o);}

int Xg(int x,int o)
{
    UNREFERENCED_PARAMETER(o);
    return x>=0?x:(MainWindow.mainx_c+x);
}
int Yg(int y){return y>=0?y:(MainWindow.mainy_c+y);}
int XG(int x,int o){return x>=0?x:(MainWindow.mainx_c+x-o);}
int YG(int y,int o){return y>=0?y:(MainWindow.mainy_c+y-o);}

bool isRebootDesired()
{
    ClickVisiter cv{ID_REBOOT,CHECKBOX::GET};
    wPanels->Accept(cv);
    return cv.GetValue()!=0;
}

Popup_t::Popup_t():
		hFontP(wFont::Create()),
		hFontBold(wFont::Create())
{
}
void Popup_t::init()
{
		hPopup=CreateWindowEx(WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TRANSPARENT,
				MainWindow.classPopup,L"",WS_POPUP,
				0,0,0,0,MainWindow.hMain,(HMENU)nullptr,hMainInstance,nullptr);
}

void Popup_t::AddShift(int i)
{
		if(i==0xffff)
				horiz_sh=0;
		else
				horiz_sh-=i/5;
		if(horiz_sh>0)horiz_sh=0;
		InvalidateRect(hPopup,nullptr,0);
}

Popup_t::~Popup_t()
{
		delete hFontP;
		delete hFontBold;
}

LRESULT Popup_t::PopupProcedure2(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
		RECT rect;
		WINDOWPOS *wp;

		switch(message)
		{
				case WM_WINDOWPOSCHANGING:
						if(floating_type!=FLOATING_TOOLTIP)break;

						wp=(WINDOWPOS*)lParam;
						GetClientRect(hwnd,&rect);
						rect.right=D_X(POPUP_WX);
						rect.bottom=floating_y;

						canvasPopup->SetFont(Popup->hFontP);
						if(!floating_str_id)break;
						canvasPopup->CalcBoundingBox(STR(floating_str_id),reinterpret_cast<RECT_WR *>(&rect));

						AdjustWindowRectEx(&rect,WS_POPUPWINDOW|WS_VISIBLE,0,0);
						popup_resize(rect.right-rect.left+D_X(POPUP_OFSX)*2,rect.bottom-rect.top+D_X(POPUP_OFSY)*2);
						wp->cx=rect.right+D_X(POPUP_OFSX)*2;
						wp->cy=rect.bottom+D_X(POPUP_OFSY)*2;
						break;

				case WM_CREATE:
						canvasPopup=Canvas::Create();
						break;

				case WM_DESTROY:
						delete canvasPopup;
						break;

				case WM_PAINT:
						GetClientRect(hwnd,&rect);
						canvasPopup->begin(&hwnd,rect.right,rect.bottom,/*floating_type!=FLOATING_CMPDRIVER&&floating_type!=FLOATING_DRIVERLST*/1);

						canvasPopup->DrawWidget(0,0,rect.right,rect.bottom,BOX_POPUP);
						switch(floating_type)
						{
								case FLOATING_SYSINFO:
										canvasPopup->SetFont(Popup->hFontP);
										manager_g->getState()->popup_sysinfo(*canvasPopup);
										break;

								case FLOATING_TOOLTIP:
										rect.left+=D_X(POPUP_OFSX);
										rect.top+=D_X(POPUP_OFSY);
										rect.right-=D_X(POPUP_OFSX);
										rect.bottom-=D_X(POPUP_OFSY);
										canvasPopup->SetFont(Popup->hFontP);
										canvasPopup->SetTextColor(D_C(POPUP_TEXT_COLOR));
										if(floating_str_id)canvasPopup->DrawTextRect(STR(floating_str_id),reinterpret_cast<RECT_WR *>(&rect));
										break;

								case FLOATING_CMPDRIVER:
										canvasPopup->SetFont(Popup->hFontP);
										manager_g->popup_drivercmp(manager_g,*canvasPopup,rect.right,rect.bottom,floating_itembar);
										break;

								case FLOATING_DRIVERLST:
										canvasPopup->SetFont(Popup->hFontP);
										manager_g->popup_driverlist(*canvasPopup,rect.right,rect.bottom,floating_itembar);
										break;

								case FLOATING_DOWNLOAD:
										canvasPopup->SetFont(Popup->hFontP);
										#ifdef USE_TORRENT
										Updater->ShowPopup(*canvasPopup);
										#endif
										break;

								default:
										break;
						}

						canvasPopup->end();
						break;

				case WM_ERASEBKGND:
						return 1;

				default:
						return DefWindowProc(hwnd,message,wParam,lParam);
		}
		return 0;
}

