#include <windows.h>
#include <shlobj.h>
#include <assert.h>

#include "SDI.h"
#include "resource.h"
#include "msapi_utf8.h"
#include "localization.h"
#include "draw.h"
#include <update.h>
#include "gui.h"

#include "registry.h"
#include "settings.h"
#include "system.h"
#include "theme.h"
#include "Version.h"
#include "darkmode.h"

// http://www.winprog.org/tutorial/dlgfaq.html
HBRUSH g_hbrDlgBackground = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

/*
 * Return the UTF8 path of a file selected through a load or save dialog
 * All string parameters are UTF-8
 * IMPORTANT NOTE: Remember that you need to call CoInitializeEx() for
 * *EACH* thread you invoke FileDialog from, as GetDisplayName() will
 * return error 0x8001010E otherwise.
 */
char* FileDialog(BOOL save, char* path, const ext_t* ext, UINT* selected_ext)
{
	size_t i;
	char* filepath = NULL;
	HRESULT hr = FALSE;
	IFileDialog *pfd = NULL;
	IShellItem *psiResult;
	COMDLG_FILTERSPEC* filter_spec = NULL;
	wchar_t *wpath = NULL, *wfilename = NULL, *wext = NULL;
	IShellItem *si_path = NULL;	// Automatically freed

	if ((ext == NULL) || (ext->count == 0) || (ext->extension == NULL) || (ext->description == NULL))
		return NULL;

	filter_spec = (COMDLG_FILTERSPEC*)calloc(ext->count + 1, sizeof(COMDLG_FILTERSPEC));
	if (filter_spec == NULL)
		return NULL;

	dialog_showing++;

	// Setup the file extension filter table
	for (i = 0; i < ext->count; i++) {
		filter_spec[i].pszSpec = utf8_to_wchar(ext->extension[i]);
		filter_spec[i].pszName = utf8_to_wchar(ext->description[i]);
	}
	filter_spec[i].pszSpec = L"*.*";
	filter_spec[i].pszName = (STR(MSG_107));

	hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, NULL, CLSCTX_INPROC,
		IID_IFileDialog, (LPVOID*)&pfd);
	if (SUCCEEDED(hr) && (pfd == NULL))	// Never trust Microsoft APIs to do the right thing
		hr = SDI_ERROR(ERROR_API_UNAVAILABLE);

	if (FAILED(hr)) {
		SetLastError(hr);
		uprintf("CoCreateInstance for FileOpenDialog failed: %s", WindowsErrorString());
		goto out;
	}

	// Set the file extension filters
	pfd->SetFileTypes((UINT)ext->count + 1, filter_spec);

	if (path == NULL) {
		// Try to use the "Downloads" folder as the initial default directory
		const GUID download_dir_guid =
			{ 0x374de290, 0x123f, 0x4565, { 0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b } };
		hr = SHGetKnownFolderPath(download_dir_guid, 0, 0, &wpath);
		if (SUCCEEDED(hr)) {
			hr = SHCreateItemFromParsingName(wpath, NULL, IID_IShellItem, (LPVOID*)&si_path);
			if (SUCCEEDED(hr)) {
				pfd->SetDefaultFolder(si_path);
			}
			CoTaskMemFree(wpath);
		}
	} else {
		wpath = utf8_to_wchar(path);
		hr = SHCreateItemFromParsingName(wpath, NULL, IID_IShellItem, (LPVOID*)&si_path);
		if (SUCCEEDED(hr)) {
				pfd->SetFolder(si_path);
		}
		safe_free(wpath);
	}

	// Set the default filename
	wfilename = utf8_to_wchar((ext->filename == NULL) ? "" : ext->filename);
	if (wfilename != NULL)
		pfd->SetFileName(wfilename);
	// Set a default extension so that when the user switches filters it gets
	// automatically updated. Note that the IFileDialog::SetDefaultExtension()
	// doc says the extension shouldn't be prefixed with unwanted characters
	// but it appears to work regardless so we don't bother cleaning it.
	wext = utf8_to_wchar((ext->extension == NULL) ? "" : ext->extension[0]);
	if (wext != NULL)
		pfd->SetDefaultExtension(wext);
	// Set the current selected extension
	pfd->SetFileTypeIndex(selected_ext == NULL ? 0 : *selected_ext);

	// Display the dialog and (optionally) get the selected extension index
	hr = pfd->Show(hMainDialog);
	if (selected_ext != NULL)
		pfd->GetFileTypeIndex(selected_ext);

	// Cleanup
	safe_free(wext);
	safe_free(wfilename);
	for (i = 0; i < ext->count; i++) {
		safe_free(filter_spec[i].pszSpec);
		safe_free(filter_spec[i].pszName);
	}
	safe_free(filter_spec[i].pszName);
	safe_free(filter_spec);

	if (SUCCEEDED(hr)) {
		// Obtain the result of the user's interaction with the dialog.
		hr = pfd->GetResult(&psiResult);
		if (SUCCEEDED(hr)) {
			hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &wpath);
			if (SUCCEEDED(hr)) {
				filepath = wchar_to_utf8(wpath);
				CoTaskMemFree(wpath);
			} else {
				SetLastError(hr);
				uprintf("Unable to access file path: %s", WindowsErrorString());
			}
			psiResult->Release();
		}
	} else if (HRESULT_CODE(hr) != ERROR_CANCELLED) {
		// If it's not a user cancel, assume the dialog didn't show and fallback
		SetLastError(hr);
		uprintf("Could not show FileOpenDialog: %s", WindowsErrorString());
	}

out:
	safe_free(filter_spec);
	if (pfd != NULL)
		pfd->Release();
	dialog_showing--;
	return filepath;
}


// http://stackoverflow.com/questions/431470/window-border-width-and-height-in-win32-how-do-i-get-it
SIZE GetBorderSize(HWND hDlg)
{
	RECT rect = {0, 0, 0, 0};
	SIZE size = {0, 0};
	WINDOWINFO wi;
	wi.cbSize = sizeof(WINDOWINFO);

	GetWindowInfo(hDlg, &wi);

	AdjustWindowRectEx(&rect, wi.dwStyle, FALSE, wi.dwExStyle);
	size.cx = rect.right - rect.left;
	size.cy = rect.bottom - rect.top;
	return size;
}

void ResizeMoveCtrl(HWND hDlg, HWND hCtrl, int dx, int dy, int dw, int dh, float scale)
{
	RECT rect;
	POINT point;
	SIZE border;

	GetWindowRect(hCtrl, &rect);
	point.x = (right_to_left_mode && (hDlg != hCtrl))?rect.right:rect.left;
	point.y = rect.top;
	if (hDlg != hCtrl)
		ScreenToClient(hDlg, &point);
	GetClientRect(hCtrl, &rect);

	// If the control has any borders (dialog, edit box), take them into account
	border = GetBorderSize(hCtrl);
	MoveWindow(hCtrl, point.x + (int)(scale*(float)dx), point.y + (int)(scale*(float)dy),
		(rect.right - rect.left) + (int)(scale*(float)dw + border.cx),
		(rect.bottom - rect.top) + (int)(scale*(float)dh + border.cy), TRUE);
	// Don't be tempted to call InvalidateRect() here - it causes intempestive whole screen refreshes
}

void ResizeButtonHeight(HWND hDlg, int id)
{
	HWND hCtrl, hPrevCtrl;
	RECT rc;
	int dy = 0;

	hCtrl = GetDlgItem(hDlg, id);
	GetWindowRect(hCtrl, &rc);
	MapWindowPoints(NULL, hDlg, (POINT*)&rc, 2);
	if (rc.bottom - rc.top < bh)
		dy = (bh - (rc.bottom - rc.top)) / 2;
	hPrevCtrl = GetNextWindow(hCtrl, GW_HWNDPREV);
	SetWindowPos(hCtrl, hPrevCtrl, rc.left, rc.top - dy, rc.right - rc.left, bh, 0);
}

/*
 * Welcome dialog
 */
BOOL CALLBACK WelcomeCallback(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
		static HFONT hTitleFont = NULL, hHyperlinkFont = NULL, hFont = NULL;
		HWND SetTitleFont;
		HWND SetVersionFont;
		HWND SetSubTitleFont;

		switch (msg) {
		case WM_INITDIALOG:
				SetDarkModeForDlg(hDlg);
				WCHAR wch[1024];
#if defined(VERSION_BUILD_TOOL_BUILD)
						wsprintf(wch, L"Compiled on " "Oct 19 2024" L" with %s %d.%02d.%05d.%d" L"WebP " L"v1.3.2" L", " L"LibTorrent " L"v2.0.11" L", " L"7zip " L"v23.01", VERSION_BUILD_TOOL_NAME,
                VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH, VERSION_BUILD_TOOL_BUILD);
#else
            wsprintf(wch, VERSION_BUILD_INFO_FORMAT, VERSION_BUILD_TOOL_NAME,
                VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH);
#endif
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_TITLE),STR(STR_WELCOME_TITLE));
        SetWindowText(GetDlgItem(hDlg, IDC_VERSION), _W(_STRG(VERSION_FILEVERSION_LONG)));
        SetWindowText(GetDlgItem(hDlg, IDC_BUILD_INFO), wch);
        SetWindowText(GetDlgItem(hDlg,IDC_COPYRIGHT), _W(VERSION_LEGALCOPYRIGHT));
        SetWindowText(GetDlgItem(hDlg, IDC_WEBLINK), _W(VERSION_WEBPAGEDISPLAY));
        SetWindowText(GetDlgItem(hDlg, IDC_SUPPORTLINK), STR(STR_BOOSTY1));
        SetWindowText(GetDlgItem(hDlg,IDD_WELC_SUBTITLE),STR(STR_WELCOME_SUBTITLE));
        SetWindowText(GetDlgItem(hDlg,IDD_WELC_INTRO),STR(STR_WELCOME_INTRO));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_INTRO2),STR(STR_WELCOME_INTRO2));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_BUTTON1),STR(STR_WELCOME_BUTTON1));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_BUTTON1_DESC),STR(STR_WELCOME_BUTTON1_DESC));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_BUTTON2),STR(STR_WELCOME_BUTTON2));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_BUTTON2_DESC),STR(STR_WELCOME_BUTTON2_DESC));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_BUTTON3),STR(STR_WELCOME_BUTTON3));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_BUTTON3_DESC),STR(STR_WELCOME_BUTTON3_DESC));
				SetWindowText(GetDlgItem(hDlg,IDD_WELC_CLOSE),STR(STR_WELCOME_CLOSE));
				// set focus to first button
				SetFocus(GetDlgItem(hDlg,IDD_WELC_BUTTON1));
				return TRUE;

		case WM_SETCURSOR:
				// 2 hyperlinks
				if ((LOWORD(lParam)==HTCLIENT) &&
						((GetDlgCtrlID((HWND)wParam) == IDC_WEBLINK)||
						 (GetDlgCtrlID((HWND)wParam) == IDC_SUPPORTLINK)))
				{
						SetCursor(LoadCursor(nullptr, IDC_HAND));
						SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
						return true;
				}
				break;

		case WM_COMMAND:
				switch(LOWORD(wParam))
				{
						case IDD_WELC_CLOSE:
								EndDialog(hDlg,wParam);
								return TRUE;
						case IDCANCEL:
								EndDialog(hDlg,wParam);
								break;
						case IDD_WELC_BUTTON1:
								// download everything
								EndDialog(hDlg,wParam);
								Settings.flags&=~FLAG_AUTOUPDATE;
								Updater->DownloadAll();
								return TRUE;
						case IDD_WELC_BUTTON2:
								// download network only
								EndDialog(hDlg,wParam);
								Settings.flags&=~FLAG_AUTOUPDATE;
								Updater->DownloadNetwork();
								return TRUE;
						case IDD_WELC_BUTTON3:
								// download indices only
								EndDialog(hDlg,wParam);
								Settings.flags&=~FLAG_AUTOUPDATE;
								Updater->DownloadIndexes();
								return TRUE;
						case IDC_WEBLINK:
                            ShellExecute(hDlg, L"open", _W(VERSION_WEBPAGEDISPLAY), NULL, NULL, SW_SHOWNORMAL);
                            break;
						case IDC_SUPPORTLINK:
                            ShellExecute(hDlg, L"open", _W(WEB_BOOSTYPAGE), NULL, NULL, SW_SHOWNORMAL);
                            break;
						default:
								break;
				}
				break;

		case WM_CTLCOLORSTATIC:
						// modify the fonts for colours and bold and size etc
						SetTitleFont=GetDlgItem(hDlg,IDD_WELC_TITLE);
						SetVersionFont=GetDlgItem(hDlg, IDC_VERSION);
						SetHyperLinkFont(GetDlgItem(hDlg,IDC_WEBLINK), (HDC)wParam, &hHyperlinkFont, FALSE);
						SetSubTitleFont=GetDlgItem(hDlg,IDD_WELC_SUBTITLE);
						SetHyperLinkFont(GetDlgItem(hDlg, IDC_SUPPORTLINK), (HDC)wParam, &hHyperlinkFont, FALSE);

						if((HWND)lParam==SetTitleFont)
						{
								hTitleFont = CreateFont(28,12,0,0,620,
																						 FALSE,FALSE,FALSE,
																						 ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
																						 ANTIALIASED_QUALITY,DEFAULT_PITCH,
																						 L"Tahoma");
								SetTextColor((HDC)wParam, RGB(0,0,0));
								SelectObject((HDC)wParam,hTitleFont);
						}
						else if(((HWND)lParam == SetVersionFont) || (HWND)lParam==SetSubTitleFont)
						{
								HFONT hFont = CreateFont(9,0,0,0,700,
																						 FALSE,FALSE,FALSE,
																						 ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
																						 ANTIALIASED_QUALITY,DEFAULT_PITCH,
																						 L"MS Sans Serif");
								SelectObject((HDC)wParam,hFont);
						}
						//else if(((HWND)lParam==Ctl3) || (HWND)lParam==Ctl5)
						//{
						//		//HFONT hFont = CreateFont(10,0,0,0,550,
						//		//														 FALSE,FALSE,FALSE,
						//		//														 ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
						//		//														 ANTIALIASED_QUALITY,DEFAULT_PITCH,
						//		//														 L"Segoe UI");
						//		SetTextColor(hdcStatic, RGB(0,0,255));
						//		//SelectObject(hdcStatic,hFont);
						//}

						SetBkMode((HDC)wParam,TRANSPARENT);
						return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);

		case WM_NCDESTROY:
				safe_delete_object(hTitleFont);
				safe_delete_object(hFont);
				safe_delete_object(hHyperlinkFont);
				break;
		}
		return FALSE;
}

/*
 * About dialog callback
 */
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
        WCHAR wch[128];
#if defined(VERSION_BUILD_TOOL_BUILD)
        wsprintf(wch, VERSION_BUILD_INFO_FORMAT, VERSION_BUILD_TOOL_NAME,
            VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH, VERSION_BUILD_TOOL_BUILD);
#else
        wsprintf(wch, VERSION_BUILD_INFO_FORMAT, VERSION_BUILD_TOOL_NAME,
            VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH);
#endif
        SetDlgItemText(hwnd, IDC_VERSION, _W(_STRG(VERSION_FILEVERSION_LONG)));
				SetDlgItemText(hwnd, IDC_BUILD_INFO, wch);
				SetDlgItemText(hwnd, IDC_COPYRIGHT, _W(VERSION_LEGALCOPYRIGHT));
        SetDlgItemText(hwnd, IDC_WEBLINK, _W(VERSION_WEBPAGEDISPLAY));
        SetDlgItemText(hwnd, IDC_SUPPORTLINK, STR(STR_BOOSTY1));
				SetDlgItemText(hwnd, IDC_WEBP_VERSION, VERSION_WEBP);
				SetDlgItemText(hwnd, IDC_TORR_VERSION, VERSION_LIBTORRENT);
				SetDlgItemText(hwnd, IDC_7ZIP_VERSION, VERSION_7ZIP);

		//CenterDlgInParent(hwnd);
		}
		return TRUE;

    case WM_SETCURSOR:
				// 2 hyperlinks
				if ((LOWORD(lParam)==HTCLIENT) &&
						((GetDlgCtrlID((HWND)wParam) == IDC_WEBLINK)||
						 (GetDlgCtrlID((HWND)wParam) == IDC_SUPPORTLINK)))
				{
            SetCursor(LoadCursor(NULL, IDC_HAND));
            SetWindowLongPtr(hwnd, DWLP_MSGRESULT, (LONG_PTR)true);
            return TRUE;
        }
    break;

		case WM_COMMAND:
		switch (LOWORD(wParam)) {
						case IDOK:
                EndDialog(hwnd,wParam);
                return TRUE;
						case IDCANCEL:
                EndDialog(hwnd,wParam);
                break;
						case IDC_WEBLINK:
								ShellExecuteW(hwnd, L"open", _W(VERSION_WEBPAGEDISPLAY), NULL, NULL, SW_SHOWNORMAL);
						case IDC_SUPPORTLINK:
								ShellExecuteW(hwnd, L"open", _W(WEB_BOOSTYPAGE),NULL, NULL, SW_SHOWNORMAL);
			break;
		}
		return TRUE;
		case WM_CTLCOLORSTATIC:
		{
				// modify the fonts for colours and bold and size etc
				HWND Ctl1=GetDlgItem(hwnd,IDD_ABOUT_T1);
				//HWND Ctl3=GetDlgItem(hwnd, IDC_STATIC_AUTHORS);
				HWND Ctl4=GetDlgItem(hwnd, IDC_VERSION);
                HWND Ctl5 = GetDlgItem(hwnd, IDC_TECHNOLOGIES);
			    HWND Ctl6=GetDlgItem(hwnd,IDC_DEVELOPERS);
				HWND Ctl8=GetDlgItem(hwnd,IDC_WEBLINK);
				HWND Ctl9=GetDlgItem(hwnd, IDC_SUPPORTLINK);
				HDC hdcStatic=(HDC)wParam;

				if((HWND)lParam==Ctl1||(HWND)lParam==Ctl4)
				{
				    HFONT hTitleFont = CreateFont(20,9,0,0,600,
				                                 FALSE,FALSE,FALSE,
				                                 ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
                                                 ANTIALIASED_QUALITY,DEFAULT_PITCH,
				                                 L"Anklepants");
				    SelectObject(hdcStatic,hTitleFont);
				    SetTextColor(hdcStatic, RGB(0,0,0));
				}
				if((HWND)lParam==Ctl5 || (HWND)lParam==Ctl6)
				{
						HFONT hFont = CreateFont(9,0,0,0,700,
																				 FALSE,FALSE,FALSE,
																				 ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
																				 ANTIALIASED_QUALITY,DEFAULT_PITCH,
																				 L"MS Sans Serif");
						SelectObject(hdcStatic,hFont);
				}
			  if(((HWND)lParam==Ctl8)||(HWND)lParam==Ctl9)
				{
						HFONT hFont = CreateFont(10,0,0,0,550,
																				 FALSE,FALSE,FALSE,
																				 ANSI_CHARSET,OUT_DEVICE_PRECIS,CLIP_MASK,
																				 ANTIALIASED_QUALITY,DEFAULT_PITCH,
																				 L"MS Sans Serif");
						SelectObject(hdcStatic,hFont);
						SetTextColor(hdcStatic, RGB(0,0,255));
				}
				SetBkMode(hdcStatic,TRANSPARENT);
				return (INT_PTR)g_hbrDlgBackground;
		}
		case WM_CTLCOLORDLG:
				return (INT_PTR)g_hbrDlgBackground;

		default:
				break;
		}
		return FALSE;
}


INT_PTR CreateAboutBox(void)
{
		INT_PTR r;
		dialog_showing++;
		r = MyDialogBox(hMainInstance, IDD_ABOUT, MainWindow.hMain, AboutDlgProc);
		dialog_showing--;
		return r;
}

void setMirroring(HWND hwnd)
{
		LONG_PTR v=GetWindowLongPtr(hwnd,GWL_EXSTYLE);
		if(right_to_left_mode) v|=WS_EX_LAYOUTRTL; else v&=~WS_EX_LAYOUTRTL;
		SetWindowLongPtr(hwnd,GWL_EXSTYLE,v);
}

void CreateStaticFont(HDC hDC, HFONT* hFont, BOOL underlined)
{
	TEXTMETRIC tm;
	LOGFONT lf;

	if (*hFont != NULL)
		return;
	GetTextMetrics(hDC, &tm);
	lf.lfHeight = tm.tmHeight;
	lf.lfWidth = 0;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfWeight = tm.tmWeight;
	lf.lfItalic = tm.tmItalic;
	lf.lfUnderline = underlined;
	lf.lfStrikeOut = tm.tmStruckOut;
	lf.lfCharSet = tm.tmCharSet;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = DEFAULT_QUALITY;
	lf.lfPitchAndFamily = tm.tmPitchAndFamily;
	GetTextFace(hDC, LF_FACESIZE, lf.lfFaceName);
	*hFont = CreateFontIndirect(&lf);
}

void SetHyperLinkFont(HWND hWnd, HDC hDC, HFONT* hFont, BOOL underlined)
{
	static BOOL use_static_font = FALSE;
	LOGFONT lf = { 0 };
	HFONT hFontTmp = NULL;

	if (*hFont == NULL) {
		hFontTmp = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
		if (hFontTmp != NULL) {
			GetObject(hFontTmp, sizeof(LOGFONT), &lf);
			use_static_font = FALSE;
		} else {
			NONCLIENTMETRICS ncm = { 0 };
			ncm.cbSize = sizeof(NONCLIENTMETRICS);
			if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0)) {
				lf = ncm.lfStatusFont;
				use_static_font = FALSE;
			} else {
				CreateStaticFont(hDC, hFont, underlined);
				use_static_font = TRUE;
			}
		}
		if (!use_static_font) {
			lf.lfUnderline = underlined;
			*hFont = CreateFontIndirect(&lf);
		}
	}
	if (!use_static_font)
		SendMessage(hWnd, WM_SETFONT, (WPARAM)*hFont, TRUE);
}

/*
 * Windows taskbar icon handling (progress bar overlay, etc)
 */
static ITaskbarList3* ptbl = NULL;

BOOL SetTaskbarProgressValue(ULONGLONG ullCompleted, ULONGLONG ullTotal)
{
		if (ptbl == NULL)
				return FALSE;
		return !FAILED(ptbl->SetProgressValue(hMainDialog, ullCompleted, ullTotal));
}


/*
 * The following is used to work around dialog template limitations when switching from LTR to RTL
 * or switching the font. This avoids having to multiply similar templates in the RC.
 * TODO: Can we use http://stackoverflow.com/questions/6057239/which-font-is-the-default-for-mfc-dialog-controls?
  */

// Produce a dialog template from our RC, and update its RTL and Font settings dynamically
// See http://blogs.msdn.com/b/oldnewthing/archive/2004/06/21/163596.aspx as well as
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms645389.aspx for a description
// of the Dialog structure
LPCDLGTEMPLATE GetDialogTemplate(int Dialog_ID)
{
	int i;
	size_t len;
	DWORD size = 0;
	DWORD* dwBuf;
	WCHAR* wBuf;
	LPCDLGTEMPLATE rcTemplate = (LPCDLGTEMPLATE) GetResource(hMainInstance, MAKEINTRESOURCEA(Dialog_ID),
			MAKEINTRESOURCEA(5), MAKEINTRESOURCEA(Dialog_ID), &size, TRUE);

	if ((size == 0) || (rcTemplate == NULL)) {
		safe_free(rcTemplate);
		return NULL;
	}
	if (right_to_left_mode) {
		// Add the RTL styles into our RC copy, so that we don't have to multiply dialog definitions in the RC
		dwBuf = (DWORD*)rcTemplate;
		dwBuf[2] = WS_EX_APPWINDOW | WS_EX_LAYOUTRTL;
	}

	// All our dialogs are set to use 'Segoe UI Symbol' by default:
	// 1. So that we can replace the font name with 'Segoe UI'
	// 2. So that Thai displays properly on RTF controls as it won't work with regular
	// 'Segoe UI'... but Cyrillic won't work with 'Segoe UI Symbol'

	// If 'Segoe UI Symbol' is available, and we are using Thai, we're done here
	if (IsFontAvailable("Segoe UI Symbol") && StrStrIW(STR(STR_LANG_ID), L"Thai"))
		return rcTemplate;

	// 'Segoe UI Symbol' cannot be used => Fall back to the best we have
	wBuf = (WCHAR*)rcTemplate;
	wBuf = &wBuf[14];	// Move to class name
	// Skip class name and title
	for (i = 0; i<2; i++) {
		if (*wBuf == 0xFFFF)
			wBuf = &wBuf[2];	// Ordinal
		else
			wBuf = &wBuf[wcslen(wBuf) + 1]; // String
	}
	// NB: to change the font size to 9, you can use
	// wBuf[0] = 0x0009;
	wBuf = &wBuf[3];
	// Make sure we are where we want to be and adjust the font
	if (wcscmp(L"Segoe UI Symbol", wBuf) == 0) {
		uintptr_t src, dst, start = (uintptr_t)rcTemplate;
		// We can't simply zero the characters we don't want, as the size of the font
		// string determines the next item lookup. So we must memmove the remaining of
		// our buffer. Oh, and those items are DWORD aligned.
		// 'Segoe UI Symbol' -> 'Segoe UI'
		wBuf[8] = 0;
		len = wcslen(wBuf);
		wBuf[len + 1] = 0;
		dst = (uintptr_t)&wBuf[len + 2];
		dst &= ~3;
		src = (uintptr_t)&wBuf[17];
		src &= ~3;
		memmove((void*)dst, (void*)src, size - (src - start));
	} else {
		uprintf("Could not locate font for %s!", MAKEINTRESOURCEA(Dialog_ID));
	}
	return rcTemplate;
}


INT_PTR MyDialogBox(HINSTANCE hInstance, int Dialog_ID, HWND hWndParent, DLGPROC lpDialogFunc)
{
	INT_PTR ret;
	LPCDLGTEMPLATE rcTemplate = GetDialogTemplate(Dialog_ID);

	// A DialogBox doesn't handle reduce/restore so it won't pass restore messages to the
	// main dialog if the main dialog was minimized. This can result in situations where the
	// user cannot restore the main window if a new dialog prompt was triggered while the
	// main dialog was reduced => Ensure the main dialog is visible before we display anything.
	ShowWindow(hMainDialog, SW_NORMAL);

	assert(rcTemplate != NULL);
	ret = DialogBoxIndirect(hMainInstance, rcTemplate, hWndParent, lpDialogFunc);
	safe_free(rcTemplate);
	return ret;
}

