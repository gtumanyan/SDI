bool IsValidHandle(HANDLE);

TempStr GetLastErrorStrTemp(DWORD err = 0);
void LogLastError(DWORD err = 0);

// file and directory operations
TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing = false);
TempStr GetTempDirTemp();
TempStr GetSelfExePathTemp();
TempWStr GetSelfExePathWTemp();
TempStr GetSelfExeDirTemp();

HANDLE LaunchProcessInDir(const char* cmdLine, const char* currDir = nullptr, DWORD flags = 0);
bool CreateProcessHelper(const char* exe, const char* args);

HINSTANCE GetInstance();
