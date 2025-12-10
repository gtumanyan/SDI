#include <windows.h>
#include <commctrl.h>

#include "SDI.h"
#include "resource.h"
#include "msapi_utf8.h"

/* Message table */
const char* default_msg_table[MSG_MAX-MSG_000] = {"%s", 0};
const char** msg_table = NULL;

/*
 * The following calls help display a localized message on the info field or status bar as well as its
 * _English_ counterpart in the log (if debug is set).
 * If duration is non zero, that message is displayed for at least duration ms, regardless of
 * any other incoming message. After that time, the display reverts to the last non-timeout message.
 */
// TODO: handle a timeout message overriding a timeout message
#define MSG_LEN      256
#define MSG_STATUS   0
#define MSG_INFO     1
#define MSG_LOW_PRI  0
#define MSG_HIGH_PRI 1
char szMessage[2][2][MSG_LEN] = { {"", ""}, {"", ""} };
char* szStatusMessage = szMessage[MSG_STATUS][MSG_HIGH_PRI];
static BOOL bStatusTimerArmed = FALSE, bOutputTimerArmed[2] = { FALSE, FALSE };
static char *output_msg[2];
static uint64_t last_msg_time[2] = { 0, 0 };

static void PrintInfoMessage(char* msg) {
	SetWindowTextU(hProgress, msg);
	InvalidateRect(hProgress, NULL, TRUE);
}
static void PrintStatusMessage(char* msg) {
	SendMessageLU(hStatus, SB_SETTEXTW, SBT_OWNERDRAW | SB_SECTION_LEFT, msg);
}
typedef void PRINT_FUNCTION(char*);
PRINT_FUNCTION *PrintMessage[2] = { PrintInfoMessage, PrintStatusMessage };

/*
 * The following timer call is used, along with MAX_REFRESH, to prevent obnoxious flicker
 * on the Info and Status fields due to messages being updated too quickly.
 */
static void CALLBACK OutputMessageTimeout(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	int i = (idEvent == TID_OUTPUT_INFO)? 0 : 1;

	KillTimer(hMainDialog, idEvent);
	bOutputTimerArmed[i] = FALSE;

	PrintMessage[i](output_msg[i]);
	last_msg_time[i] = GetTickCount64();
}

static void OutputMessage(BOOL info, char* msg)
{
	uint64_t delta;
	int i = info ? 0 : 1;

	if (bOutputTimerArmed[i]) {
		// Already have a delayed message going - just change that message to latest
		output_msg[i] = msg;
	} else {
		// Find if we need to arm a timer
		delta = GetTickCount64() - last_msg_time[i];
		if (delta < (2 * MAX_REFRESH)) {
			// Not enough time has elapsed since our last output => arm a timer
			output_msg[i] = msg;
			SetTimer(hMainDialog, TID_OUTPUT_INFO + i, (UINT)((2 * MAX_REFRESH) - delta), OutputMessageTimeout);
			bOutputTimerArmed[i] = TRUE;
		} else {
			PrintMessage[i](msg);
			last_msg_time[i] = GetTickCount64();
		}
	}
}

static void CALLBACK PrintMessageTimeout(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	bStatusTimerArmed = FALSE;
	// We're going to print high priority message, so restore our pointer
	if (idEvent != TID_MESSAGE_INFO)
		szStatusMessage = szMessage[MSG_STATUS][MSG_HIGH_PRI];
	OutputMessage((idEvent == TID_MESSAGE_INFO), szMessage[(idEvent == TID_MESSAGE_INFO)?MSG_INFO:MSG_STATUS][MSG_HIGH_PRI]);
	KillTimer(hMainDialog, idEvent);
}

void PrintStatusInfo(BOOL info, BOOL debug, unsigned int duration, int msg_id, ...)
{
	const char* format = NULL;
	char	buf[MSG_LEN];
	char *msg_hi = szMessage[info?MSG_INFO:MSG_STATUS][MSG_HIGH_PRI];
	char *msg_lo = szMessage[info?MSG_INFO:MSG_STATUS][MSG_LOW_PRI];
	char *msg_cur = (duration > 0)?msg_lo:msg_hi;
	va_list args;

	if (msg_id < 0) {
		// A negative msg_id clears the message
		msg_hi[0] = 0;
		OutputMessage(info, msg_hi);
		return;
	}

	if ((msg_id < MSG_000) || (msg_id >= MSG_MAX)) {
		uprintf("PrintStatusInfo: invalid MSG_ID\n");
		return;
	}

	// We need to keep track of where szStatusMessage should point to so that ellipses work
	if (!info)
		szStatusMessage = szMessage[MSG_STATUS][(duration > 0)?MSG_LOW_PRI:MSG_HIGH_PRI];

	if ((msg_id >= MSG_000) && (msg_id < MSG_MAX))
		format = msg_table[msg_id - MSG_000];
	if (format == NULL) {
		safe_sprintf(msg_hi, MSG_LEN, "MSG_%03d UNTRANSLATED", msg_id - MSG_000);
    uprintf("%s", msg_hi);
		OutputMessage(info, msg_hi);
		return;
	}

	va_start(args, msg_id);
	safe_vsnprintf(msg_cur, MSG_LEN, format, args);
	va_end(args);
	msg_cur[MSG_LEN - 1] = '\0';

	if ((duration != 0) || (!bStatusTimerArmed))
		OutputMessage(info, msg_cur);

	if (duration != 0) {
		SetTimer(hMainDialog, (info)?TID_MESSAGE_INFO:TID_MESSAGE_STATUS, duration, PrintMessageTimeout);
		bStatusTimerArmed = TRUE;
	}

	// Because we want the log messages in English, we go through the VA business once more, but this time with default_msg_table
	if (debug) {
		if ((msg_id >= MSG_000) && (msg_id < MSG_MAX))
			format = default_msg_table[msg_id - MSG_000];
		if (format == NULL) {
			safe_sprintf(buf, sizeof(buf), "(default) MSG_%03d UNTRANSLATED", msg_id - MSG_000);
			return;
		}
		va_start(args, msg_id);
		safe_vsnprintf(buf, MSG_LEN, format, args);
		va_end(args);
		buf[MSG_LEN - 1] = '\0';
		// buf may(?) containt a '%' so don't feed it as a naked format string
		uprintf("%s", buf);
	}
}
