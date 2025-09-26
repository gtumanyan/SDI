#include <dwmapi.h>
#include <versionhelpers.h>

#include "SDI.h"
#include "gui.h"
#include "darkmode.h"

PF_TYPE_DECL(WINAPI, BOOL, SetWindowCompositionAttribute, (HWND, WINDOWCOMPOSITIONATTRIBDATA*));

BOOL is_darkmode_enabled = FALSE;

static COLORREF color_accent = TOOLBAR_ICON_COLOR;

static inline BOOL IsAtLeastWin10Build(DWORD buildNumber)
{
		OSVERSIONINFOEXW osvi = { 0 };

		if (!IsWindows10OrGreater())
				return FALSE;

		osvi.dwOSVersionInfoSize = sizeof(osvi);
		osvi.dwBuildNumber = buildNumber;

		return VerifyVersionInfoW(&osvi, VER_BUILDNUMBER, VerSetConditionMask(0, VER_BUILDNUMBER, VER_GREATER_EQUAL));
}

static inline BOOL IsAtLeastWin10(void)
{
		return IsAtLeastWin10Build(WIN10_1809);
}

static inline BOOL IsAtLeastWin11(void)
{
		return IsAtLeastWin10Build(WIN11_21H2);
}

void SetDarkTitleBar(HWND hWnd)
{
		if (IsAtLeastWin11()) {
				DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_darkmode_enabled, sizeof(is_darkmode_enabled));
				return;
		}
		if (IsAtLeastWin10Build(WIN10_1903)) {
				PF_INIT_OR_OUT(SetWindowCompositionAttribute, user32);
				WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &is_darkmode_enabled, sizeof(is_darkmode_enabled) };
				pfSetWindowCompositionAttribute(hWnd, &data);
				return;
		}
		// only for Windows 10 1809 build 17763
		if (IsAtLeastWin10()) {
				SetPropW(hWnd, L"UseImmersiveDarkModeColors", (HANDLE)(intptr_t)is_darkmode_enabled);
				return;
		}

out:
		is_darkmode_enabled = FALSE;
}

/*
 * Static text section
 */
static LRESULT CALLBACK StaticTextSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
		StaticTextData* data = (StaticTextData*)dwRefData;

		switch (uMsg) {
		case WM_NCDESTROY:
				RemoveWindowSubclass(hWnd, StaticTextSubclass, uIdSubclass);
				free(data);
				break;

		case WM_ENABLE:
				RECT rcClient = { 0 };
				const LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
				data->disabled = (wParam == FALSE);
				if (data->disabled)
						SetWindowLongPtr(hWnd, GWL_STYLE, style & ~WS_DISABLED);
				GetClientRect(hWnd, &rcClient);
				MapWindowPoints(hWnd, GetParent(hWnd), (LPPOINT)&rcClient, 2);
				RedrawWindow(GetParent(hWnd), &rcClient, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				if (data->disabled)
						SetWindowLongPtr(hWnd, GWL_STYLE, style | WS_DISABLED);
				return 0;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/*
 * Dark mode custom colors
 */
static ThemeResources theme_resources = {
	NULL, NULL, NULL, NULL, NULL, NULL
};

static HBRUSH GetDlgBackgroundBrush(void)
{
		if (theme_resources.hbrBackground == NULL)
				theme_resources.hbrBackground = CreateSolidBrush(DARKMODE_NORMAL_DIALOG_BACKGROUND_COLOR);
		return theme_resources.hbrBackground;
}

static HBRUSH GetCtrlBackgroundBrush(void)
{
		if (theme_resources.hbrBackgroundControl == NULL)
				theme_resources.hbrBackgroundControl = CreateSolidBrush(DARKMODE_NORMAL_CONTROL_BACKGROUND_COLOR);
		return theme_resources.hbrBackgroundControl;
}

/*
 * Ctl color messages section
 */
static LRESULT OnCtlColorDlg(HDC hdc)
{
		SetTextColor(hdc, DARKMODE_NORMAL_TEXT_COLOR);
		SetBkColor(hdc, DARKMODE_NORMAL_DIALOG_BACKGROUND_COLOR);
		return (LRESULT)GetDlgBackgroundBrush();
}

static LRESULT OnCtlColorStatic(WPARAM wParam, LPARAM lParam)
{
		HDC hdc = (HDC)wParam;
		HWND hWnd = (HWND)lParam;
		WCHAR str[32] = { 0 };

		GetClassName(hWnd, str, ARRAYSIZE(str));
		if (_wcsicmp(str, WC_STATIC) == 0) {
				if ((GetWindowLongPtr(hWnd, GWL_STYLE) & SS_NOTIFY) == SS_NOTIFY) {
						DWORD_PTR dwRefData = 0;
						COLORREF cText = color_accent;
						if (GetWindowSubclass(hWnd, StaticTextSubclass, (UINT_PTR)StaticTextSubclassID, &dwRefData)) {
								if (((StaticTextData*)dwRefData)->disabled)
										cText = DARKMODE_DISABLED_TEXT_COLOR;
						}
						SetTextColor(hdc, cText);
						SetBkColor(hdc, DARKMODE_NORMAL_DIALOG_BACKGROUND_COLOR);
						return (LRESULT)GetDlgBackgroundBrush();
				}
		}
		// Read-only WC_EDIT
		return OnCtlColorDlg(hdc);
}

static LRESULT OnCtlColorCtrl(HDC hdc)
{
		SetTextColor(hdc, DARKMODE_NORMAL_TEXT_COLOR);
		SetBkColor(hdc, DARKMODE_NORMAL_CONTROL_BACKGROUND_COLOR);
		return (LRESULT)GetCtrlBackgroundBrush();
}

static INT_PTR OnCtlColorListbox(WPARAM wParam, LPARAM lParam)
{
		HDC hdc = (HDC)wParam;
		HWND hWnd = (HWND)lParam;
		const LONG_PTR nStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
		BOOL isComboBox = (nStyle & LBS_COMBOBOX) == LBS_COMBOBOX;

		if ((!isComboBox || !is_darkmode_enabled) && IsWindowEnabled(hWnd))
				return (INT_PTR)OnCtlColorCtrl(hdc);

		return (INT_PTR)OnCtlColorDlg(hdc);
}

static LRESULT CALLBACK WindowCtlColorSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
		(void)dwRefData;

		switch (uMsg) {
		case WM_NCDESTROY:
				RemoveWindowSubclass(hWnd, WindowCtlColorSubclass, uIdSubclass);
				break;
		case WM_CTLCOLOREDIT:
				return OnCtlColorCtrl((HDC)wParam);
		case WM_CTLCOLORLISTBOX:
				return OnCtlColorListbox(wParam, lParam);
		case WM_CTLCOLORDLG:
				return OnCtlColorDlg((HDC)wParam);
		case WM_CTLCOLORSTATIC:
				return OnCtlColorStatic(wParam, lParam);
		case WM_PRINTCLIENT:
				return TRUE;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void SubclassCtlColor(HWND hWnd)
{
		if (!GetWindowSubclass(hWnd, WindowCtlColorSubclass, (UINT_PTR)WindowCtlColorSubclassID, NULL))
				SetWindowSubclass(hWnd, WindowCtlColorSubclass, (UINT_PTR)WindowCtlColorSubclassID, 0);
}

