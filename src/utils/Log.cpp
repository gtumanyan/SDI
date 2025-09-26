#include "BaseUtil.h"
#include "ThreadUtil.h"
#include "ScopedWin.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "Log.h"

constexpr const WCHAR* kPipeName = L"\\\\.\\pipe\\LOCAL\\ArsLexis-Logger";

const char* gLogAppName = "SDI";

Mutex gLogMutex;

// we use HeapAllocator because we can do logging during crash handling
// where we want to avoid allocator deadlocks by calling malloc()
HeapAllocator* gLogAllocator = nullptr;

str::Str* gLogBuf = nullptr;
bool gLogToConsole = true;
// we always log if IsDebuggerPresent()
// this forces logging to debugger always
bool gLogToDebugger = false;
// meant to avoid doing stuff during crash reporting
// will log to debugger (if no need for formatting)
bool gReducedLogging = false;
// when main thread exists other threads might still
// try to log. when true, this stops logging
bool gDestroyedLogging = false;

// if true, doesn't log if the same text has already been logged
// reduces logging but also can be confusing i.e. log lines are not showing up
bool gSkipDuplicateLines = true;

bool gLogToPipe = true;
HANDLE hLogPipe = INVALID_HANDLE_VALUE;

WCHAR* gLogFilePath = nullptr;

// 1 MB - 128 to stay under 1 MB even after appending (an estimate)
constexpr int kMaxLogBuf = 1024 * 1024 - 128;

//{ Error handling
static const char* errno_str() {
    switch (errno) {
        case EPERM:               return "Operation not permitted";
        case ENOENT:              return "No such file or directory";
        case ESRCH:               return "No such process";
        case EINTR:               return "Interrupted function";
        case EIO:                 return "I/O error";
        case ENXIO:               return "No such device or address";
        case E2BIG:               return "Argument list too long";
        case ENOEXEC:             return "Exec format error";
        case EBADF:               return "Bad file number";
        case ECHILD:              return "No spawned processes";
        case EAGAIN:              return "No more processes or not enough memory or maximum nesting level reached";
        case ENOMEM:              return "Not enough memory";
        case EACCES:              return "Permission denied";
        case EFAULT:              return "Bad address";
        case EBUSY:               return "Device or resource busy";
        case EEXIST:              return "File exists";
        case EXDEV:               return "Cross-device link";
        case ENODEV:              return "No such device";
        case ENOTDIR:             return "Not a directory";
        case EISDIR:              return "Is a directory";
        case EINVAL:              return "Invalid argument";
        case ENFILE:              return "Too many files open in system";
        case EMFILE:              return "Too many open files";
        case ENOTTY:              return "Inappropriate I/O control operation";
        case EFBIG:               return "File too large";
        case ENOSPC:              return "No space left on device";
        case ESPIPE:              return "Invalid seek";
        case EROFS:               return "Read-only file system";
        case EMLINK:              return "Too many links";
        case EPIPE:               return "Broken pipe";
        case EDOM:                return "Math argument";
        case ERANGE:              return "Result too large";
        case EDEADLK:             return "Resource deadlock would occur";
        case ENAMETOOLONG:        return "Filename too long";
        case ENOLCK:              return "No locks available";
        case ENOSYS:              return "Function not supported";
        case ENOTEMPTY:           return "Directory not empty";
        case EILSEQ:              return "Illegal byte sequence";
    }
    return "Unknown error";
}

static void logToPipe(const char* s, size_t n = 0) {
    if (!gLogToPipe) {
        return;
    }
    if (!s || (*s == 0)) {
        return;
    }
    if (n == 0) {
        n = str::Len(s);
    }

    DWORD cbWritten = 0;
    BOOL ok = false;
    bool didConnect = false;
    if (!IsValidHandle(hLogPipe)) {
        // try open pipe for logging
        hLogPipe = CreateFileW(kPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!IsValidHandle(hLogPipe)) {
            // TODO: retry if ERROR_PIPE_BUSY ?
            // TODO: maybe remember when last we tried to open it and don't try to open for the
            // next 10 secs, to minimize CreateFileW() calls
            return;
        }
        didConnect = true;
    }

    // TODO: do I need this if I don't read from the pipe?
    DWORD mode = PIPE_READMODE_MESSAGE;
    ok = SetNamedPipeHandleState(hLogPipe, &mode, nullptr, nullptr);
    if (!ok) {
        OutputDebugStringA("logPipe: SetNamedPipeHandleState() failed\n");
    }

    if (didConnect) {
        // logview accepts logging from anyone, so announce ourselves
        TempStr initialMsg = str::FormatTemp("app: %s\n", gLogAppName);
        WriteFile(hLogPipe, initialMsg, (DWORD)str::Len(initialMsg), &cbWritten, nullptr);
    }

    DWORD cb = (DWORD)n;
    // TODO: what happens when we write more than the server can read?
    // should I loop if cbWritten < cb?
    ok = WriteFile(hLogPipe, s, cb, &cbWritten, nullptr);
    if (!ok) {
#if 0
        DWORD err = GetLastError();
        OutputDebugStringA("logPipe: WriteFile() failed with error: ");
        char buf[256]{};
        snprintf(buf, sizeof(buf) - 1, "%d %s\n", (int)err, getWinError(err));
        OutputDebugStringA(buf);
#endif
        CloseHandle(hLogPipe);
        hLogPipe = INVALID_HANDLE_VALUE;
    }
}

void log(const char* s, bool always) {
    bool skipLog = !always && gSkipDuplicateLines && gLogBuf && gLogBuf->Contains(s);

    if (!skipLog) {
        // in reduced logging mode, we do want to log to at least the debugger
        if (gLogToDebugger || IsDebuggerPresent() || gReducedLogging) {
            OutputDebugStringA(s);
        }
    }
    if (gDestroyedLogging) {
        return;
    }
    if (gReducedLogging) {
        // if the pipe already connected, do log to it even if disabled
        // we do want easy logging, just want to reduce doing stuff
        // that can break crash handling
        if (gLogToPipe && IsValidHandle(hLogPipe)) {
            logToPipe(s);
        }
        return;
    }
    gLogMutex.Lock();

    InterlockedIncrement(&gAllowAllocFailure);
    defer {
        InterlockedDecrement(&gAllowAllocFailure);
    };

    if (!gLogBuf) {
        gLogAllocator = new HeapAllocator();
        gLogBuf = new str::Str(32 * 1024, gLogAllocator);
    } else {
        if (gLogBuf->Size() > kMaxLogBuf) {
            // TODO: use gLogBuf->Clear(), which doesn't free the allocated space
            gLogBuf->Reset();
        }
    }

    size_t n = str::Len(s);

    // when skipping, we skip buf (crash reports) and console
    // but write to file and logview
    if (!skipLog) {
        gLogBuf->Append(s, n);
    }

    if (!skipLog && gLogToConsole) {
        fwrite(s, 1, n, stdout);
        fflush(stdout);
    }

    if (gLogFilePath) {
        auto f = _wfopen(gLogFilePath, L"a");
        if (f != nullptr) {
            fwrite(s, 1, n, f);
            fflush(f);
            fclose(f);
        }
    }
    logToPipe(s, n);
    gLogMutex.Unlock();
}
void loga(const char* s) {
    if (gDestroyedLogging) {
        return;
    }
    log(s, true);
}

void logf(const char* fmt, ...) {
    if (gReducedLogging || gDestroyedLogging) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    AutoFreeStr s = str::FmtV(fmt, args);
    log(s.Get(), false);
    va_end(args);
}

void logfa(const char* fmt, ...) {
    if (gDestroyedLogging) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char* s = str::FmtV(fmt, args);
    log(s, true);
    str::Free(s);
    va_end(args);
}

void StartLogToFile(const WCHAR* path, bool removeIfExists) {
    ReportIf(gLogFilePath);
    gLogFilePath = str::Dup(path);
    if (removeIfExists) {
        _wremove(path);
    }
}

bool WriteCurrentLogToFile(const char* path) {
    ByteSlice slice = gLogBuf->AsByteSlice();
    if (slice.empty()) {
        return false;
    }
    bool ok = dir::CreateForFile(path);
    if (!ok) {
        logf("WriteCurrentLogToFile: dir::CreateForFile('%s') failed\n", path);
        return false;
    }
    ok = file::WriteFile(path, slice);
    if (!ok) {
        logf("WriteCurrentLogToFile: file::WriteFile('%s') failed\n", path);
    }
    return ok;
}

void DestroyLogging() {
    gDestroyedLogging = true;
    gLogMutex.Lock();
    delete gLogBuf;
    gLogBuf = nullptr;
    delete gLogAllocator;
    gLogAllocator = nullptr;
    gLogMutex.Unlock();
    str::FreePtr(&gLogFilePath);
}
