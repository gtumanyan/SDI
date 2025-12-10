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
	static char unknown_edition_str[22];

	// From: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getproductinfo
	// These values can be found in the winnt.h header.
	switch (ProductType) {
	case PRODUCT_UNDEFINED:                              return	"";	//  Undefined
	case PRODUCT_ULTIMATE:                               return "Ultimate";
	case PRODUCT_HOME_BASIC:                             return "Home Basic";
	case PRODUCT_HOME_PREMIUM:                           return "Home Premium";
	case PRODUCT_ENTERPRISE:                             return "Enterprise";
	case PRODUCT_HOME_BASIC_N:                           return "Home Basic N";
	case PRODUCT_BUSINESS:                               return "Business";
	case PRODUCT_STANDARD_SERVER:                        return "Server Standard";
	case PRODUCT_DATACENTER_SERVER:                      return "Server Datacenter (full installation)";
	case PRODUCT_SMALLBUSINESS_SERVER:                   return "Small Business  Server";
	case PRODUCT_ENTERPRISE_SERVER:                      return "Server Enterprise (full installation)";
	case PRODUCT_STARTER:                                return "Starter";
	case PRODUCT_DATACENTER_SERVER_CORE:                 return "Server Datacenter (Core)";
	case PRODUCT_STANDARD_SERVER_CORE:                   return "Server Standard (Core)";
	case PRODUCT_ENTERPRISE_SERVER_CORE:                 return "Server Enterprise (Core)";
	case PRODUCT_BUSINESS_N:                             return "Business N";
	case PRODUCT_WEB_SERVER:                             return "Web Server";
	case PRODUCT_CLUSTER_SERVER:                         return "Server Hyper Core";
	case PRODUCT_HOME_SERVER:                            return "Home Server";
	case PRODUCT_STORAGE_EXPRESS_SERVER:                 return "Storage Server Express";
	case PRODUCT_STORAGE_STANDARD_SERVER:                return "Storage Server Standard";
	case PRODUCT_STORAGE_WORKGROUP_SERVER:               return "Storage Server Workgroup";
	case PRODUCT_STORAGE_ENTERPRISE_SERVER:              return "Storage Server Enterprise";
	case PRODUCT_SERVER_FOR_SMALLBUSINESS:               return "Windows Server 2008 for Windows Essential Server Solutions";
	case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:           return "Small Business Server Premium";
	case PRODUCT_HOME_PREMIUM_N:                         return "Home Premium N";
	case PRODUCT_ENTERPRISE_N:                           return "Enterprise N";
	case PRODUCT_ULTIMATE_N:                             return "Ultimate N";
	case PRODUCT_WEB_SERVER_CORE:                        return "Web Server (core installation)";
	case PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT:       return "Windows Essential Business Server Management Server";
	case PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY:         return "Windows Essential Business Server Security Server";
	case PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING:        return "Windows Essential Business Server Messaging Server";
	case PRODUCT_SERVER_FOUNDATION:                      return "Server Foundation";
	case PRODUCT_HOME_PREMIUM_SERVER:                    return "Home Server";
	case PRODUCT_SERVER_FOR_SMALLBUSINESS_V:             return "Windows Server 2008 without Hyper-V for Windows Essential Server Solutions";
	case PRODUCT_STANDARD_SERVER_V:                      return "Server Standard without Hyper-V";
	case PRODUCT_DATACENTER_SERVER_V:                    return "Server Datacenter without Hyper-V";
	case PRODUCT_ENTERPRISE_SERVER_V:                    return "Server Enterprise without Hyper-V";
	case PRODUCT_DATACENTER_SERVER_CORE_V:               return "Server Datacenter without Hyper-V (Core)";
	case PRODUCT_STANDARD_SERVER_CORE_V:                 return "Server Standard without Hyper-V (Core)";
	case PRODUCT_ENTERPRISE_SERVER_CORE_V:               return "Server Enterprise without Hyper-V (Core)";
	case PRODUCT_HYPERV:                                 return "Hyper-V Server";
	case PRODUCT_STORAGE_EXPRESS_SERVER_CORE:            return "Storage Server Express (core installation)";
	case PRODUCT_STORAGE_STANDARD_SERVER_CORE:           return "Storage Server Standard (core installation)";
    case PRODUCT_STORAGE_WORKGROUP_SERVER_CORE:          return "Storage Server Workgroup (core installation)";
    case PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE:         return "Storage Server Enterprise (core installation)";
	case PRODUCT_STARTER_N:                              return "Starter N";
	case PRODUCT_PROFESSIONAL:                           return "Pro";
	case PRODUCT_PROFESSIONAL_N:                         return "Pro N";
    case PRODUCT_SB_SOLUTION_SERVER:                     return "Windows Small Business Server 2011 Essentials";
    case PRODUCT_SERVER_FOR_SB_SOLUTIONS:                return "Server For SB Solutions";
	case PRODUCT_STANDARD_SERVER_SOLUTIONS:              return "Server Solutions Premium";
	case PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE:         return "Server Solutions Premium (Core)";
    case PRODUCT_SB_SOLUTION_SERVER_EM:                  return "Server For SB Solutions EM";
    case PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM:             return "Server For SB Solutions EM";
    case PRODUCT_SOLUTION_EMBEDDEDSERVER:                return "Windows MultiPoint Server";
	case PRODUCT_SOLUTION_EMBEDDEDSERVER_CORE:           return "Solution Embedded Server (core installation)";
    case PRODUCT_PROFESSIONAL_EMBEDDED:                  return "Professional Embedded";
    case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT:          return " Essential Server Solution Management";
    case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL:       	 return " Essential Server Solution Additional";
    case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC:    	 return " Essential Server Solution Management SVC";
    case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC:    	 return " Essential Server Solution Additional SVC";
    case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE:   	 return " Small Business Server Premium (core installation)";
	case PRODUCT_CLUSTER_SERVER_V:                       return "Server Hyper Core V";
    case PRODUCT_EMBEDDED:                            	 return " Embedded";
	case PRODUCT_STARTER_E:                              return "Starter E";
	case PRODUCT_HOME_BASIC_E:                           return "Home Basic E";
	case PRODUCT_HOME_PREMIUM_E:                         return "Premium E";
	case PRODUCT_PROFESSIONAL_E:                         return "Pro E";
	case PRODUCT_ENTERPRISE_E:                           return "Enterprise E";
	case PRODUCT_ULTIMATE_E:                             return "Ultimate E";
	case PRODUCT_ENTERPRISE_EVALUATION:                  return "Enterprise (Eval)";
    case PRODUCT_MULTIPOINT_STANDARD_SERVER:          	 return  "MultiPoint Server Standard";
    case PRODUCT_MULTIPOINT_PREMIUM_SERVER:           	 return "MultiPoint Server Premium";
	case PRODUCT_STANDARD_EVALUATION_SERVER:             return "Server Standard (Eval)";
	case PRODUCT_DATACENTER_EVALUATION_SERVER:           return "Server Datacenter (Eval)";
	case PRODUCT_ENTERPRISE_N_EVALUATION:                return "Enterprise N (Eval)";
    case PRODUCT_EMBEDDED_AUTOMOTIVE:                 	 return" Embedded Automotive";
    case PRODUCT_EMBEDDED_INDUSTRY_A:                 	 return " Embedded Industry A";
	case PRODUCT_THINPC:                                 return "Thin PC";
    case PRODUCT_EMBEDDED_A:                          	 return " Embedded A";
    case PRODUCT_EMBEDDED_INDUSTRY:                   	 return " Embedded Industry";
    case PRODUCT_EMBEDDED_E:                          	 return " Embedded E";
    case PRODUCT_EMBEDDED_INDUSTRY_E:                 	 return " Embedded Industry E";
    case PRODUCT_EMBEDDED_INDUSTRY_A_E:               	 return " Embedded Industry A E";
    case PRODUCT_STORAGE_WORKGROUP_EVALUATION_SERVER: 	 return " Storage Server Workgroup (evaluation installation)";
    case PRODUCT_STORAGE_STANDARD_EVALUATION_SERVER:  	 return " Storage Server Standard (evaluation installation)";
    case PRODUCT_CORE_ARM:                            	 return " RT";
	case PRODUCT_CORE_N:                                 return "Home N";
	case PRODUCT_CORE_COUNTRYSPECIFIC:                   return "Home China";
	case PRODUCT_CORE_SINGLELANGUAGE:                    return "Home Single Language";
	case PRODUCT_CORE:                                   return "Home";
	case PRODUCT_PROFESSIONAL_WMC:                       return "Pro with Media Center";
    case PRODUCT_EMBEDDED_INDUSTRY_EVAL:              	 return "Embedded Industry (evaluation installation)";
    case PRODUCT_EMBEDDED_INDUSTRY_E_EVAL:            	 return " Embedded Industry E (evaluation installation)";
    case PRODUCT_EMBEDDED_EVAL:                       	 return " Embedded (evaluation installation)";
    case PRODUCT_EMBEDDED_E_EVAL:                     	 return  "Embedded E (evaluation installation)";
    case PRODUCT_NANO_SERVER:                         	 return "Nano Server";
    case PRODUCT_CLOUD_STORAGE_SERVER:                	 return " Server Could Storage";
	case PRODUCT_CORE_CONNECTED:                         return "Home Connected";
	case PRODUCT_PROFESSIONAL_STUDENT:                   return "Pro Student";
	case PRODUCT_CORE_CONNECTED_N:                       return "Home Connected N";
	case PRODUCT_PROFESSIONAL_STUDENT_N:                 return "Pro Student N";
	case PRODUCT_CORE_CONNECTED_SINGLELANGUAGE:          return "Home Connected Single Language";
	case PRODUCT_CORE_CONNECTED_COUNTRYSPECIFIC:         return "Home Connected China";
    case PRODUCT_CONNECTED_CAR:                       	 return " Connected Car";
    case PRODUCT_INDUSTRY_HANDHELD:                   	 return " Industry Handheld";
    case PRODUCT_PPI_PRO:                             	 return " PPI Pro";
    case PRODUCT_ARM64_SERVER:                        	 return " ARM64 Server";
	case PRODUCT_EDUCATION:                              return "Education";
	case PRODUCT_EDUCATION_N:                            return "Education N";
    case PRODUCT_IOTUAP:                              	 return " IoT Core";
    case PRODUCT_CLOUD_HOST_INFRASTRUCTURE_SERVER:    	 return " Cloud Host Infrastructure Server";
	case PRODUCT_ENTERPRISE_S:                           return "Enterprise LTSB";
	case PRODUCT_ENTERPRISE_S_N:                         return "Enterprise LTSB N";
	case PRODUCT_PROFESSIONAL_S:                         return "Pro S";
	case PRODUCT_PROFESSIONAL_S_N:                       return "Pro S N";
	case PRODUCT_ENTERPRISE_S_EVALUATION:                return "Enterprise LTSB (Eval)";
	case PRODUCT_ENTERPRISE_S_N_EVALUATION:              return "Enterprise LTSB N (Eval)";
    case PRODUCT_HOLOGRAPHIC:                         	 return " Holographic";
    case PRODUCT_HOLOGRAPHIC_BUSINESS:                	 return "HOLOGRAPHIC BUSINESS";
	case PRODUCT_PRO_SINGLE_LANGUAGE:                    return "Pro Single Language";
	case PRODUCT_PRO_CHINA:                              return "Pro China";
	case PRODUCT_ENTERPRISE_SUBSCRIPTION:                return "Enterprise Subscription";
	case PRODUCT_ENTERPRISE_SUBSCRIPTION_N:              return "Enterprise Subscription N";
    case PRODUCT_DATACENTER_NANO_SERVER:              	 return " Datacenter Nano Server";
    case PRODUCT_STANDARD_NANO_SERVER:                	 return " Standard Nano Server";
	case PRODUCT_DATACENTER_A_SERVER_CORE:               return "Server Datacenter SA (Core)";
	case PRODUCT_STANDARD_A_SERVER_CORE:                 return "Server Standard SA (Core)";
    case PRODUCT_DATACENTER_WS_SERVER_CORE:           	 return " Datacenter WS Server Core";
    case PRODUCT_STANDARD_WS_SERVER_CORE:             	 return " Standard WS Server Core";
	case PRODUCT_UTILITY_VM:                             return "Utility VM";
    case PRODUCT_DATACENTER_EVALUATION_SERVER_CORE:   	 return " Datacenter_evaluation_server_core";
    case PRODUCT_STANDARD_EVALUATION_SERVER_CORE:     	 return " Standard Evaluation Server Core";
	case PRODUCT_PRO_WORKSTATION:                        return "Pro for Workstations";
	case PRODUCT_PRO_WORKSTATION_N:                      return "Pro for Workstations N";
	case PRODUCT_PRO_FOR_EDUCATION:                      return "Pro for Education";
	case PRODUCT_PRO_FOR_EDUCATION_N:                    return "Pro for Education N";
    case PRODUCT_AZURE_SERVER_CORE:                      return " Azure Server Core";
    case PRODUCT_AZURE_NANO_SERVER:                   	 return " Azure Nano Server";
	case PRODUCT_ENTERPRISEG:                            return "Enterprise G";
	case PRODUCT_ENTERPRISEGN:                           return "Enterprise G N";
    case PRODUCT_SERVERRDSH:                          	 return " Server RDSH";
	case PRODUCT_CLOUD:                                  return "Cloud";
	case PRODUCT_CLOUDN:                                 return "Cloud N";
    case PRODUCT_HUBOS:                                	 return "Windows 10 S (Hub OS)";
	case PRODUCT_ONECOREUPDATEOS:                        return "Home OS";
	case PRODUCT_CLOUDE:                                 return "Cloud E";
	case PRODUCT_IOTOS:                                  return "IoT OS";
	case PRODUCT_CLOUDEN:                                return "Cloud E N";
	case PRODUCT_IOTEDGEOS:                              return "IoT Edge OS";
	case PRODUCT_IOTENTERPRISE:                          return "IoT Enterprise";
	case PRODUCT_LITE:                                   return "Lite";
	case PRODUCT_IOTENTERPRISES:                         return "IoT Enterprise S";
	case PRODUCT_XBOX_SYSTEMOS:                          return "XBox";
	case PRODUCT_XBOX_GAMEOS:                            return "Xbox OS";
	case PRODUCT_XBOX_ERAOS:                             return "Xbox Era OS";
	case PRODUCT_XBOX_DURANGOHOSTOS:                     return "Xbox Durango Host OS";
	case PRODUCT_XBOX_SCARLETTHOSTOS:                    return "Xbox Scarlett Host OS";
	case PRODUCT_XBOX_KEYSTONE:                          return "Xbox Keystone";
	case PRODUCT_AZURE_SERVER_CLOUDHOST:                 return "Azure Stack HCI Cloud Host";
	case PRODUCT_AZURE_SERVER_CLOUDMOS:                  return "Azure Stack HCI Cloud MOS";
	case PRODUCT_CLOUDEDITIONN:                          return "Windows Cloud Edition N";
	case PRODUCT_CLOUDEDITION:                           return "Windows Cloud Edition";
	case PRODUCT_VALIDATION:                             return "Windows Validation OS";
	case PRODUCT_IOTENTERPRISESK:                        return "Windows IoT Enterprise LTSC";
	case PRODUCT_IOTENTERPRISEK:                         return "Windows IoT Enterprise";
	case PRODUCT_IOTENTERPRISESEVAL:                     return "Windows IoT Enterprise S (Evaluation)";
	case PRODUCT_AZURE_SERVER_AGENTBRIDGE:               return "Azure Stack HCI Agent Bridge";
	case PRODUCT_AZURE_SERVER_NANOHOST:                  return "Azure Stack HCI Nano Host";
	case PRODUCT_WNC:                                    return "Windows National Cloud";
	case PRODUCT_AZURESTACKHCI_SERVER_CORE:              return "Azure Stack HCI";
	case PRODUCT_DATACENTER_SERVER_AZURE_EDITION:        return "Windows Server Datacenter Azure Edition";
	case PRODUCT_DATACENTER_SERVER_CORE_AZURE_EDITION:   return "Windows Server Datacenter Azure Edition (Server Core)";
	case PRODUCT_DATACENTER_WS_SERVER_CORE_AZURE_EDITION:return "Windows Server Datacenter Azure Edition with Desktop Experience (Server Core)";


	case PRODUCT_UNLICENSED:                             return "(Unlicensed)";
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
	}
	else {
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
	char* vptr;
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
	if (!GetVersionExA((OSVERSIONINFOA*)&vi)) {
		memset(&vi, 0, sizeof(vi));
		vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
		if (!GetVersionExA((OSVERSIONINFOA*)&vi))
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
	}
	else {
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
		}
		else {
			if (!ConvertStringSidToSidA(psid_string, &ret)) {
				uprintf("Unable to convert string back to SID: %s", WindowsErrorString());
				ret = NULL;
			}
			// MUST use LocalFree()
			LocalFree(psid_string);
		}
	}
	else {
		ret = NULL;
		uprintf("GetTokenInformation (real) failed: %s", WindowsErrorString());
	}
	free(tu);
	return ret;
}

BOOL FileIO(enum file_io_type io_type, char* path, char** buffer, DWORD* size)
{
	SECURITY_ATTRIBUTES s_attr, * sa = NULL;
	SECURITY_DESCRIPTOR s_desc;
	const LARGE_INTEGER liZero = { .QuadPart = 0ULL };
	PSID sid = NULL;
	HANDLE handle;
	DWORD dwDesiredAccess = 0, dwCreationDisposition = 0;
	BOOL r = FALSE;
	BOOL ret = FALSE;

	// Change the owner from admin to regular user
	sid = GetSID();
	if ((sid != NULL)
		&& InitializeSecurityDescriptor(&s_desc, SECURITY_DESCRIPTOR_REVISION)
		&& SetSecurityDescriptorOwner(&s_desc, sid, FALSE)) {
		s_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
		s_attr.bInheritHandle = FALSE;
		s_attr.lpSecurityDescriptor = &s_desc;
		sa = &s_attr;
	}
	else {
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
	}
	else {
		p = (uint8_t*)LockResource(res_handle);
	}
	*len = res_len;

out:
	return p;
}

static BOOL CALLBACK EnumFontFamExProc(const LOGFONTA* lpelfe,
	const TEXTMETRICA* lpntme, DWORD FontType, LPARAM lParam)
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

