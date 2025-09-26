#pragma once

extern BOOL is_darkmode_enabled;

typedef enum _WindowsBuild {
		WIN10_1809 = 17763, // first build to support dark mode
		WIN10_1903 = 18362,
		WIN10_22H2 = 19045,
		WIN11_21H2 = 22000,
} WindowsBuild;

typedef enum _SubclassID {
		ButtonSubclassID = 42,
		GroupboxSubclassID,
		WindowNotifySubclassID,
		StatusBarSubclassID,
		ProgressBarSubclassID,
		StaticTextSubclassID,
		WindowCtlColorSubclassID
} SubclassID;

typedef enum _WINDOWCOMPOSITIONATTRIB {
		WCA_UNDEFINED = 0,
		WCA_NCRENDERING_ENABLED = 1,
		WCA_NCRENDERING_POLICY = 2,
		WCA_TRANSITIONS_FORCEDISABLED = 3,
		WCA_ALLOW_NCPAINT = 4,
		WCA_CAPTION_BUTTON_BOUNDS = 5,
		WCA_NONCLIENT_RTL_LAYOUT = 6,
		WCA_FORCE_ICONIC_REPRESENTATION = 7,
		WCA_EXTENDED_FRAME_BOUNDS = 8,
		WCA_HAS_ICONIC_BITMAP = 9,
		WCA_THEME_ATTRIBUTES = 10,
		WCA_NCRENDERING_EXILED = 11,
		WCA_NCADORNMENTINFO = 12,
		WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
		WCA_VIDEO_OVERLAY_ACTIVE = 14,
		WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
		WCA_DISALLOW_PEEK = 16,
		WCA_CLOAK = 17,
		WCA_CLOAKED = 18,
		WCA_ACCENT_POLICY = 19,
		WCA_FREEZE_REPRESENTATION = 20,
		WCA_EVER_UNCLOAKED = 21,
		WCA_VISUAL_OWNER = 22,
		WCA_HOLOGRAPHIC = 23,
		WCA_EXCLUDED_FROM_DDA = 24,
		WCA_PASSIVEUPDATEMODE = 25,
		WCA_USEDARKMODECOLORS = 26,
		WCA_LAST = 27
} WINDOWCOMPOSITIONATTRIB;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
		WINDOWCOMPOSITIONATTRIB Attrib;
		PVOID pvData;
		SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef struct {
		HBRUSH hbrBackground;
		HBRUSH hbrBackgroundControl;
		HBRUSH hbrBackgroundHot;
		HBRUSH hbrEdge;
		HPEN hpnEdge;
		HPEN hpnEdgeHot;
} ThemeResources;

typedef struct {
		BOOL disabled;
} StaticTextData;

void SetDarkTitleBar(HWND hWnd);
void SubclassCtlColor(HWND hWnd);

static __inline void SetDarkModeForDlg(HWND hWnd)
{
	if (is_darkmode_enabled) {
		SetDarkTitleBar(hWnd);
		SubclassCtlColor(hWnd);
	}
}
