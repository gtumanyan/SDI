
struct HttpRsp {
    AutoFreeStr url;
    str::Str data;
    DWORD error = (DWORD)-1;
    DWORD httpStatusCode = (DWORD)-1;

    HttpRsp() = default;
};

bool IsHttpRspOk(const HttpRsp*);

bool HttpGetToFile(const char* url, const char* destFilePath);
