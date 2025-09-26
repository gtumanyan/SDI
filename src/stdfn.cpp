#include <windows.h>
#include <sddl.h>
#include <assert.h>

#include "SDI.h"
#include "msapi_utf8.h"

#include "settings.h"

// MinGW doesn't yet know these (from wldp.h)
typedef enum WLDP_WINDOWS_LOCKDOWN_MODE
{
		WLDP_WINDOWS_LOCKDOWN_MODE_UNLOCKED = 0,
		WLDP_WINDOWS_LOCKDOWN_MODE_TRIAL,
		WLDP_WINDOWS_LOCKDOWN_MODE_LOCKED,
		WLDP_WINDOWS_LOCKDOWN_MODE_MAX,
} WLDP_WINDOWS_LOCKDOWN_MODE, * PWLDP_WINDOWS_LOCKDOWN_MODE;

windows_version_t WindowsVersion = { 0 };

static const char* GetEdition(DWORD ProductType)
{
	static char unknown_edition_str[64];

	// From: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getproductinfo
	// These values can be found in the winnt.h header.
	switch (ProductType) {
	case 0x00000000: return "";	//  Undefined
	case 0x00000001: return "Ultimate";
	case 0x00000002: return "Home Basic";
	case 0x00000003: return "Home Premium";
	case 0x00000004: return "Enterprise";
	case 0x00000005: return "Home Basic N";
	case 0x00000006: return "Business";
	case 0x00000007: return "Server Standard";
	case 0x00000008: return "Server Datacenter";
	case 0x00000009: return "Smallbusiness Server";
	case 0x0000000A: return "Server Enterprise";
	case 0x0000000B: return "Starter";
	case 0x0000000C: return "Server Datacenter (Core)";
	case 0x0000000D: return "Server Standard (Core)";
	case 0x0000000E: return "Server Enterprise (Core)";
	case 0x00000010: return "Business N";
	case 0x00000011: return "Web Server";
	case 0x00000012: return "HPC Edition";
	case 0x00000013: return "Storage Server (Essentials)";
	case 0x0000001A: return "Home Premium N";
	case 0x0000001B: return "Enterprise N";
	case 0x0000001C: return "Ultimate N";
	case 0x00000022: return "Home Server";
	case 0x00000024: return "Server Standard without Hyper-V";
	case 0x00000025: return "Server Datacenter without Hyper-V";
	case 0x00000026: return "Server Enterprise without Hyper-V";
	case 0x00000027: return "Server Datacenter without Hyper-V (Core)";
	case 0x00000028: return "Server Standard without Hyper-V (Core)";
	case 0x00000029: return "Server Enterprise without Hyper-V (Core)";
	case 0x0000002A: return "Hyper-V Server";
	case 0x0000002F: return "Starter N";
	case 0x00000030: return "Pro";
	case 0x00000031: return "Pro N";
	case 0x00000034: return "Server Solutions Premium";
	case 0x00000035: return "Server Solutions Premium (Core)";
	case 0x00000040: return "Server Hyper Core V";
	case 0x00000042: return "Starter E";
	case 0x00000043: return "Home Basic E";
	case 0x00000044: return "Premium E";
	case 0x00000045: return "Pro E";
	case 0x00000046: return "Enterprise E";
	case 0x00000047: return "Ultimate E";
	case 0x00000048: return "Enterprise (Eval)";
	case 0x0000004F: return "Server Standard (Eval)";
	case 0x00000050: return "Server Datacenter (Eval)";
	case 0x00000054: return "Enterprise N (Eval)";
	case 0x00000057: return "Thin PC";
	case 0x00000058: case 0x00000059: case 0x0000005A: case 0x0000005B: case 0x0000005C: return "Embedded";
	case 0x00000062: return "Home N";
	case 0x00000063: return "Home China";
	case 0x00000064: return "Home Single Language";
	case 0x00000065: return "Home";
	case 0x00000067: return "Pro with Media Center";
	case 0x00000069: case 0x0000006A: case 0x0000006B: case 0x0000006C: return "Embedded";
	case 0x0000006F: return "Home Connected";
	case 0x00000070: return "Pro Student";
	case 0x00000071: return "Home Connected N";
	case 0x00000072: return "Pro Student N";
	case 0x00000073: return "Home Connected Single Language";
	case 0x00000074: return "Home Connected China";
	case 0x00000079: return "Education";
	case 0x0000007A: return "Education N";
	case 0x0000007D: return "Enterprise LTSB";
	case 0x0000007E: return "Enterprise LTSB N";
	case 0x0000007F: return "Pro S";
	case 0x00000080: return "Pro S N";
	case 0x00000081: return "Enterprise LTSB (Eval)";
	case 0x00000082: return "Enterprise LTSB N (Eval)";
	case 0x0000008A: return "Pro Single Language";
	case 0x0000008B: return "Pro China";
	case 0x0000008C: return "Enterprise Subscription";
	case 0x0000008D: return "Enterprise Subscription N";
	case 0x00000091: return "Server Datacenter SA (Core)";
	case 0x00000092: return "Server Standard SA (Core)";
	case 0x00000095: return "Utility VM";
	case 0x000000A1: return "Pro for Workstations";
	case 0x000000A2: return "Pro for Workstations N";
	case 0x000000A4: return "Pro for Education";
	case 0x000000A5: return "Pro for Education N";
	case 0x000000AB: return "Enterprise G";	// I swear Microsoft are just making up editions...
	case 0x000000AC: return "Enterprise G N";
	case 0x000000B2: return "Cloud";
	case 0x000000B3: return "Cloud N";
	case 0x000000B6: return "Home OS";
	case 0x000000B7: case 0x000000CB: return "Cloud E";
	case 0x000000B9: return "IoT OS";
	case 0x000000BA: case 0x000000CA: return "Cloud E N";
	case 0x000000BB: return "IoT Edge OS";
	case 0x000000BC: return "IoT Enterprise";
	case 0x000000BD: return "Lite";
	case 0x000000BF: return "IoT Enterprise S";
	case 0x000000C0: case 0x000000C2: case 0x000000C3: case 0x000000C4: case 0x000000C5: case 0x000000C6: return "XBox";
	case 0x000000C7: case 0x000000C8: case 0x00000196: case 0x00000197: case 0x00000198: return "Azure Server";
	case 0xABCDABCD: return "(Unlicensed)";
	default:
		static_sprintf(unknown_edition_str, "(Unknown Edition 0x%02X)", (uint32_t)ProductType);
		return unknown_edition_str;
	}
}

PF_TYPE_DECL(WINAPI, HRESULT, WldpQueryWindowsLockdownMode, (PWLDP_WINDOWS_LOCKDOWN_MODE));
BOOL isSMode(void)
{
		BOOL r = FALSE;
		WLDP_WINDOWS_LOCKDOWN_MODE mode;
		HRESULT hr = pfWldpQueryWindowsLockdownMode(&mode);
		PF_INIT_OR_OUT(WldpQueryWindowsLockdownMode, Wldp);

		if (hr != S_OK) {
				SetLastError((DWORD)hr);
				uprintf("Could not detect S Mode: %s", WindowsErrorString());
	} else {
				r = (mode != WLDP_WINDOWS_LOCKDOWN_MODE_UNLOCKED);
		}

out:
		return r;
}

/*
 * Modified from smartmontools' os_win32.cpp
 */
void GetWindowsVersion(windows_version_t* windows_version)
{
	OSVERSIONINFOEXA vi, vi2;
	DWORD dwProductType = 0;
	const char* w = NULL;
	const char* arch_name;
	char *vptr;
	size_t vlen;
	DWORD major = 0, minor = 0;
	USHORT ProcessMachine = IMAGE_FILE_MACHINE_UNKNOWN, NativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
	ULONGLONG major_equal, minor_equal;
	BOOL ws, is_wow64 = FALSE;

	PF_TYPE_DECL(WINAPI, BOOL, IsWow64Process2, (HANDLE, USHORT*, USHORT*));
	PF_INIT(IsWow64Process2, Kernel32);

	memset(windows_version, 0, sizeof(windows_version_t));
	static_strcpy(windows_version->VersionStr, "Windows Undefined");

	memset(&vi, 0, sizeof(vi));
	vi.dwOSVersionInfoSize = sizeof(vi);
	if (!GetVersionExA((OSVERSIONINFOA *)&vi)) {
		memset(&vi, 0, sizeof(vi));
		vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
		if (!GetVersionExA((OSVERSIONINFOA *)&vi))
			return;
	}

	if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT) {

		if (vi.dwMajorVersion > 6 || (vi.dwMajorVersion == 6 && vi.dwMinorVersion >= 2)) {
			// Starting with Windows 8.1 Preview, GetVersionEx() does no longer report the actual OS version
			// See: http://msdn.microsoft.com/en-us/library/windows/desktop/dn302074.aspx
			// And starting with Windows 10 Preview 2, Windows enforces the use of the application/supportedOS
			// manifest in order for VerSetConditionMask() to report the ACTUAL OS major and minor...

			major_equal = VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL);
			for (major = vi.dwMajorVersion; major <= 9; major++) {
				memset(&vi2, 0, sizeof(vi2));
				vi2.dwOSVersionInfoSize = sizeof(vi2); vi2.dwMajorVersion = major;
				if (!VerifyVersionInfoA(&vi2, VER_MAJORVERSION, major_equal))
					continue;
				if (vi.dwMajorVersion < major) {
					vi.dwMajorVersion = major; vi.dwMinorVersion = 0;
				}

				minor_equal = VerSetConditionMask(0, VER_MINORVERSION, VER_EQUAL);
				for (minor = vi.dwMinorVersion; minor <= 9; minor++) {
					memset(&vi2, 0, sizeof(vi2)); vi2.dwOSVersionInfoSize = sizeof(vi2);
					vi2.dwMinorVersion = minor;
					if (!VerifyVersionInfoA(&vi2, VER_MINORVERSION, minor_equal))
						continue;
					vi.dwMinorVersion = minor;
					break;
				}

				break;
			}
		}

		if (vi.dwMajorVersion <= 0xf && vi.dwMinorVersion <= 0xf) {
			ws = (vi.wProductType <= VER_NT_WORKSTATION);
			windows_version->Version = vi.dwMajorVersion << 4 | vi.dwMinorVersion;
			switch (windows_version->Version) {
			case WINDOWS_XP: w = "XP";
				break;
			case WINDOWS_2003: w = (ws ? "XP_64" : (!GetSystemMetrics(89) ? "Server 2003" : "Server 2003_R2"));
				break;
			case WINDOWS_VISTA: w = (ws ? "Vista" : "Server 2008");
				break;
			case WINDOWS_7: w = (ws ? "7" : "Server 2008_R2");
				break;
			case WINDOWS_8: w = (ws ? "8" : "Server 2012");
				break;
			case WINDOWS_8_1: w = (ws ? "8.1" : "Server 2012_R2");
				break;
			case WINDOWS_10_PREVIEW1: w = (ws ? "10 (Preview 1)" : "Server 10 (Preview 1)");
				break;
			// Starting with Windows 10 Preview 2, the major is the same as the public-facing version
			case WINDOWS_10:
				if (vi.dwBuildNumber < 20000) {
					w = (ws ? "10" : ((vi.dwBuildNumber < 17763) ? "Server 2016" : "Server 2019"));
					break;
				}
				windows_version->Version = WINDOWS_11;
				major = 11;
				// Fall through
			case WINDOWS_11: w = (ws ? "11" : "Server 2022");
				break;
			default:
				if (windows_version->Version < WINDOWS_XP)
					windows_version->Version = WINDOWS_UNDEFINED;
				else
					w = "12 or later";
				break;
			}
		}
	}
	windows_version->Major = major;
	windows_version->Minor = minor;

	if ((pfIsWow64Process2 != NULL) && pfIsWow64Process2(GetCurrentProcess(), &ProcessMachine, &NativeMachine)) {
		windows_version->Arch = NativeMachine;
	} else {
		// Assume same arch as the app
		windows_version->Arch = GetApplicationArch();
		// Fix the Arch if we have a 32-bit app running under WOW64
		if ((sizeof(uintptr_t) < 8) && IsWow64Process(GetCurrentProcess(), &is_wow64) && is_wow64) {
			if (windows_version->Arch == IMAGE_FILE_MACHINE_I386)
				windows_version->Arch = IMAGE_FILE_MACHINE_AMD64;
			else if (windows_version->Arch == IMAGE_FILE_MACHINE_ARM)
				windows_version->Arch = IMAGE_FILE_MACHINE_ARM64;
			else // I sure wanna be made aware of this scenario...
				assert(FALSE);
		}
		uprintf("Note: Underlying Windows architecture was guessed and may be incorrect...");
	}
	arch_name = GetArchName(windows_version->Arch);

	GetProductInfo(vi.dwMajorVersion, vi.dwMinorVersion, vi.wServicePackMajor, vi.wServicePackMinor, &dwProductType);
	vptr = &windows_version->VersionStr[sizeof("Windows ") - 1];
	vlen = sizeof(windows_version->VersionStr) - sizeof("Windows ") - 1;
	if (!w)
		safe_sprintf(vptr, vlen, "%s %u.%u %s", (vi.dwPlatformId == VER_PLATFORM_WIN32_NT ? "NT" : "??"),
			(unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion, arch_name);
	else if (vi.wServicePackMinor)
		safe_sprintf(vptr, vlen, "%s SP%u.%u %s", w, vi.wServicePackMajor, vi.wServicePackMinor, arch_name);
	else if (vi.wServicePackMajor)
		safe_sprintf(vptr, vlen, "%s SP%u %s", w, vi.wServicePackMajor, arch_name);
	else
		safe_sprintf(vptr, vlen, "%s%s%s %s",
			w, (dwProductType != 0) ? " " : "", GetEdition(dwProductType), arch_name);

	windows_version->Edition = (int)dwProductType;

	// Add the build number (including UBR if available)
	windows_version->BuildNumber = vi.dwBuildNumber;
	windows_version->Ubr = ReadRegistryKey32(REGKEY_HKLM, "Software\\Microsoft\\Windows NT\\CurrentVersion\\UBR");
	vptr = &windows_version->VersionStr[safe_strlen(windows_version->VersionStr)];
	vlen = sizeof(windows_version->VersionStr) - safe_strlen(windows_version->VersionStr) - 1;
	if (windows_version->Ubr != 0)
		safe_sprintf(vptr, vlen, " (Build %lu.%lu)", windows_version->BuildNumber, windows_version->Ubr);
	else
		safe_sprintf(vptr, vlen, " (Build %lu)", windows_version->BuildNumber);
	vptr = &windows_version->VersionStr[safe_strlen(windows_version->VersionStr)];
	vlen = sizeof(windows_version->VersionStr) - safe_strlen(windows_version->VersionStr) - 1;
	if (isSMode())
		safe_sprintf(vptr, vlen, " in S Mode");
}

/*
 * Retrieve the SID of the current user. The returned PSID must be freed by the caller using LocalFree()
 */
static PSID GetSID(void) {
	TOKEN_USER* tu = NULL;
	DWORD len;
	HANDLE token;
	PSID ret = NULL;
	char* psid_string = NULL;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
		uprintf("OpenProcessToken failed: %s", WindowsErrorString());
		return NULL;
	}

	if (!GetTokenInformation(token, TokenUser, tu, 0, &len)) {
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			uprintf("GetTokenInformation (pre) failed: %s", WindowsErrorString());
			return NULL;
		}
		tu = (TOKEN_USER*)calloc(1, len);
	}
	if (tu == NULL) {
		return NULL;
	}

	if (GetTokenInformation(token, TokenUser, tu, len, &len)) {
		/*
		 * now of course, the interesting thing is that if you return tu->User.Sid
		 * but free tu, the PSID pointer becomes invalid after a while.
		 * The workaround? Convert to string then back to PSID
		 */
		if (!ConvertSidToStringSidA(tu->User.Sid, &psid_string)) {
			uprintf("Unable to convert SID to string: %s", WindowsErrorString());
			ret = NULL;
		} else {
			if (!ConvertStringSidToSidA(psid_string, &ret)) {
				uprintf("Unable to convert string back to SID: %s", WindowsErrorString());
				ret = NULL;
			}
			// MUST use LocalFree()
			LocalFree(psid_string);
		}
	} else {
		ret = NULL;
		uprintf("GetTokenInformation (real) failed: %s", WindowsErrorString());
	}
	free(tu);
	return ret;
}

BOOL FileIO(enum file_io_type io_type, char* path, char** buffer, DWORD* size)
{
	SECURITY_ATTRIBUTES s_attr, *sa = NULL;
	SECURITY_DESCRIPTOR s_desc;
	const LARGE_INTEGER liZero = { .QuadPart = 0ULL };
	PSID sid = NULL;
	HANDLE handle;
	DWORD dwDesiredAccess = 0, dwCreationDisposition = 0;
	BOOL r = FALSE;
	BOOL ret = FALSE;

	// Change the owner from admin to regular user
	sid = GetSID();
	if ( (sid != NULL)
	  && InitializeSecurityDescriptor(&s_desc, SECURITY_DESCRIPTOR_REVISION)
	  && SetSecurityDescriptorOwner(&s_desc, sid, FALSE) ) {
		s_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
		s_attr.bInheritHandle = FALSE;
		s_attr.lpSecurityDescriptor = &s_desc;
		sa = &s_attr;
	} else {
		uprintf("Could not set security descriptor: %s", WindowsErrorString());
	}

	switch (io_type) {
	case FILE_IO_READ:
		*buffer = NULL;
		dwDesiredAccess = GENERIC_READ;
		dwCreationDisposition = OPEN_EXISTING;
		break;
	case FILE_IO_WRITE:
		dwDesiredAccess = GENERIC_WRITE;
		dwCreationDisposition = CREATE_ALWAYS;
		break;
	case FILE_IO_APPEND:
		dwDesiredAccess = FILE_APPEND_DATA;
		dwCreationDisposition = OPEN_ALWAYS;
		break;
	default:
		assert(FALSE);
		break;
	}
	handle = CreateFileU(path, dwDesiredAccess, FILE_SHARE_READ, sa,
		dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		uprintf("Could not open '%s': %s", path, WindowsErrorString());
		goto out;
	}

	switch (io_type) {
	case FILE_IO_READ:
		*size = GetFileSize(handle, NULL);
		*buffer = (char*)malloc(*size);
		if (*buffer == NULL) {
			uprintf("Could not allocate buffer for reading file");
			goto out;
		}
		r = ReadFile(handle, *buffer, *size, size, NULL);
		break;
	case FILE_IO_APPEND:
		SetFilePointerEx(handle, liZero, NULL, FILE_END);
		// Fall through
	case FILE_IO_WRITE:
		r = WriteFile(handle, *buffer, *size, size, NULL);
		break;
	}

	if (!r) {
		uprintf("I/O Error: %s", WindowsErrorString());
		goto out;
	}

	//PrintInfoDebug(0, (io_type == FILE_IO_READ) ? MSG_215 : MSG_216, path);
	ret = TRUE;

out:
	CloseHandle(handle);
	if (!ret && (io_type == FILE_IO_READ)) {
		// Only leave the buffer allocated if we were able to read data
		safe_free(*buffer);
		*size = 0;
	}
	return ret;
}

/*
 * Get a resource from the RC. If needed that resource can be duplicated.
 * If duplicate is true and len is non-zero, the a zeroed buffer of 'len'
 * size is allocated for the resource. Else the buffer is allocated for
 * the resource size.
 */
uint8_t* GetResource(HMODULE module, char* name, char* type, const char* desc, DWORD* len, BOOL duplicate)
{
	HGLOBAL res_handle;
	HRSRC res;
	DWORD res_len;
	uint8_t* p = NULL;

	res = FindResourceA(module, name, type);
	if (res == NULL) {
		uprintf("Could not locate resource '%s': %s", desc, WindowsErrorString());
		goto out;
	}
	res_handle = LoadResource(module, res);
	if (res_handle == NULL) {
		uprintf("Could not load resource '%s': %s", desc, WindowsErrorString());
		goto out;
	}
	res_len = SizeofResource(module, res);

	if (duplicate) {
		if (*len == 0)
			*len = res_len;
		p = (uint8_t*)calloc(*len, 1);
		if (p == NULL) {
			uprintf("Could not allocate resource '%s'", desc);
			goto out;
		}
		memcpy(p, LockResource(res_handle), std::min(res_len, *len));
		if (res_len > *len)
			uprintf("WARNING: Resource '%s' was truncated by %d bytes!", desc, res_len - *len);
	} else {
			p = (uint8_t*)LockResource(res_handle);
	}
	*len = res_len;

out:
	return p;
}

static BOOL CALLBACK EnumFontFamExProc(const LOGFONTA *lpelfe,
	const TEXTMETRICA *lpntme, DWORD FontType, LPARAM lParam)
{
	return TRUE;
}

BOOL IsFontAvailable(const char* font_name)
{
	BOOL r;
	LOGFONTA lf = { 0 };
	HDC hDC = GetDC(hMainDialog);

	if (font_name == NULL) {
		safe_release_dc(hMainDialog, hDC);
		return FALSE;
	}

	lf.lfCharSet = DEFAULT_CHARSET;
	safe_strcpy(lf.lfFaceName, LF_FACESIZE, font_name);

	r = EnumFontFamiliesExA(hDC, &lf, EnumFontFamExProc, 0, 0);
	safe_release_dc(hMainDialog, hDC);
	return r;
}

