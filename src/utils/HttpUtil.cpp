#include "BaseUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "HttpUtil.h"

#include "Log.h"

// per RFC 1945 10.15 and 3.7, a user agent product token shouldn't contain whitespace
constexpr const WCHAR* kUserAgent = L"SdiHTTP";

bool IsHttpRspOk(const HttpRsp* rsp) {
    if (rsp->error != ERROR_SUCCESS) {
        logf("HttpRspOk: rsp->error %d, should be %d (ERROR_SUCCESS)\n", (int)rsp->error, (int)ERROR_SUCCESS);
        return false;
    }
    if (rsp->httpStatusCode >= 300) {
        logf("HttpRspOk: rsp->httpStatusCode: %d\n", (int)rsp->httpStatusCode);
        return false;
    }
    return true;
}

constexpr const int kBufSize = 256 * 1024;

// Download content of a url to a file
bool HttpGetToFile(const char* urlA, const char* destFilePath) {
    logf("HttpGetToFile: url: '%s', file: '%s'\n", urlA, destFilePath);
    bool ok = false;
    HINTERNET hReq = nullptr, hInet = nullptr;
    DWORD dwRead = 0;
    DWORD headerBuffSize = sizeof(DWORD);
    DWORD statusCode = 0;
    WCHAR* url = ToWStrTemp(urlA);
    char* buf = nullptr;

    WCHAR* pathW = ToWStrTemp(destFilePath);
    HANDLE hf =
        CreateFileW(pathW, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (INVALID_HANDLE_VALUE == hf) {
        logf("HttpGetToFile: CreateFileW('%s') failed\n", destFilePath);
        LogLastError();
        goto Exit;
    }

    buf = AllocArray<char>(kBufSize);
    if (!buf) {
        goto Exit;
    }

    hInet = InternetOpenW(kUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        goto Exit;
    }

    hReq = InternetOpenUrlW(hInet, url, nullptr, 0, 0, 0);
    if (!hReq) {
        goto Exit;
    }

    if (!HttpQueryInfoW(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &headerBuffSize, nullptr)) {
        goto Exit;
    }

    if (statusCode != 200) {
        goto Exit;
    }

    for (;;) {
        if (!InternetReadFile(hReq, buf, kBufSize, &dwRead)) {
            goto Exit;
        }
        if (dwRead == 0) {
            break;
        }
        DWORD size;
        BOOL wroteOk = WriteFile(hf, buf, (DWORD)dwRead, &size, nullptr);
        if (!wroteOk) {
            goto Exit;
        }

        if (size != dwRead) {
            goto Exit;
        }
    }

    ok = true;
Exit:
    CloseHandle(hf);
    if (hReq) {
        InternetCloseHandle(hReq);
    }
    if (hInet) {
        InternetCloseHandle(hInet);
    }
    if (!ok) {
        file::Delete(destFilePath);
    }
    free(buf);
    return ok;
}
