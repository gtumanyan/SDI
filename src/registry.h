#pragma once

#include "SDI.h"
#include "versionEx.h"
#ifdef __cplusplus
extern "C" {
#endif

#define REGKEY_HKLM                 HKEY_LOCAL_MACHINE
		
/*
 * Read a generic registry key value. If a short key_name is used, assume that
 * it belongs to the application and create the app subkey if required
 */
static __inline BOOL _GetRegistryKey(HKEY key_root, const char* key_name, DWORD reg_type,
	LPBYTE dest, DWORD dest_size)
{
	char long_key_name[MAX_PATH] = { 0 };
	BOOL r = FALSE;
	size_t i;
	LONG s;
	HKEY hSoftware = NULL, hApp = NULL;
	DWORD dwDisp, dwType = -1, dwSize = dest_size;

	memset(dest, 0, dest_size);

	if (key_name == NULL)
		return FALSE;

	for (i = safe_strlen(key_name); i > 0; i--) {
		if (key_name[i] == '\\')
			break;
	}

	if (i > 0) {
		// For a read operation, allow access to any long key
		if (i >= sizeof(long_key_name))
			return FALSE;
		static_strcpy(long_key_name, key_name);
		long_key_name[i] = 0;
		i++;
		if (RegOpenKeyExA(key_root, long_key_name, 0, KEY_READ, &hApp) != ERROR_SUCCESS) {
			hApp = NULL;
			goto out;
		}
	} else {
		if (RegOpenKeyExA(key_root, "SOFTWARE", 0, KEY_READ|KEY_CREATE_SUB_KEY, &hSoftware) != ERROR_SUCCESS) {
			hSoftware = NULL;
			goto out;
		}
		if (RegCreateKeyExA(hSoftware, APPLICATION_NAME, 0, NULL, 0,
			KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_CREATE_SUB_KEY, NULL, &hApp, &dwDisp) != ERROR_SUCCESS) {
			hApp = NULL;
			goto out;
		}
	}

	s = RegQueryValueExA(hApp, &key_name[i], NULL, &dwType, (LPBYTE)dest, &dwSize);
	// No key means default value of 0 or empty string
	if ((s == ERROR_FILE_NOT_FOUND) || ((s == ERROR_SUCCESS) && (dwType == reg_type) && (dwSize > 0))) {
		r = TRUE;
	}
out:
	if (hSoftware != NULL)
		RegCloseKey(hSoftware);
	if (hApp != NULL)
		RegCloseKey(hApp);
	return r;
}

/* Helpers for 32 bit registry operations */
#define GetRegistryKey32(root, key, pval) _GetRegistryKey(root, key, REG_DWORD, (LPBYTE)pval, sizeof(DWORD))
static __inline int32_t ReadRegistryKey32(HKEY root, const char* key) {
	DWORD val;
	GetRegistryKey32(root, key, &val);
	return (int32_t)val;
}

#ifdef __cplusplus
}
#endif
