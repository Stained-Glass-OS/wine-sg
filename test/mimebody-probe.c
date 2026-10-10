/* inetcomm batch (patches/sg/2033), run by test/mimebody-gate.sh. Families
 * (from Wine's todo_wine blocks, which record Windows): the content type
 * follows SetData, GetCharset never returns a null charset; and the body's
 * data (EmptyData, GetEstimatedSize, GetDataHere, SaveToFile) and charset
 * (SetCharset) methods. IMimeBody is called through its vtable slots.
 *
 *   mimebody-probe.exe */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MIME_E_NO_DATA ((HRESULT)0x800cce05)
enum { IET_BINARY = 0, IET_BASE64 = 1, IET_UUENCODE = 2, IET_QP = 3, IET_7BIT = 4, IET_8BIT = 5 };

typedef struct { char szName[128]; void *hCharset; UINT cpiWindows; UINT cpiInternet; DWORD reserved; } INETCSETINFO;
typedef struct { LPSTR pszName; LPSTR pszData; } PARAMINFO;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define SLOT(obj, n) ((*(void ***)(obj))[n])
#define BodyRelease(o) ((ULONG (WINAPI *)(void *))SLOT(o, 2))(o)
#define Load(o, s) ((HRESULT (WINAPI *)(void *, IStream *))SLOT(o, 5))(o, s)
#define GetCharset(o, h) ((HRESULT (WINAPI *)(void *, void **))SLOT(o, 19))(o, h)
#define SetCharset(o, h, t) ((HRESULT (WINAPI *)(void *, void *, int))SLOT(o, 20))(o, h, t)
#define GetParameters(o, n, c, p) ((HRESULT (WINAPI *)(void *, const char *, ULONG *, PARAMINFO **))SLOT(o, 21))(o, n, c, p)
#define IsContentType(o, a, b) ((HRESULT (WINAPI *)(void *, const char *, const char *))SLOT(o, 22))(o, a, b)
#define GetEstimatedSize(o, e, s) ((HRESULT (WINAPI *)(void *, int, ULONG *))SLOT(o, 34))(o, e, s)
#define GetDataHere(o, e, s) ((HRESULT (WINAPI *)(void *, int, IStream *))SLOT(o, 35))(o, e, s)
#define GetData(o, e, s) ((HRESULT (WINAPI *)(void *, int, IStream **))SLOT(o, 36))(o, e, s)
#define SetData(o, e, a, b, iid, p) ((HRESULT (WINAPI *)(void *, int, const char *, const char *, const GUID *, void *))SLOT(o, 37))(o, e, a, b, iid, p)
#define EmptyData(o) ((HRESULT (WINAPI *)(void *))SLOT(o, 38))(o)
#define SaveToFile(o, e, p) ((HRESULT (WINAPI *)(void *, int, const char *))SLOT(o, 41))(o, e, p)

static IStream *stream_of(const char *text)
{
    IStream *s = NULL;
    ULONG n;
    LARGE_INTEGER zero = { 0 };
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    s->lpVtbl->Write(s, text, strlen(text), &n);
    s->lpVtbl->Seek(s, zero, STREAM_SEEK_SET, NULL);
    return s;
}

static int stream_is(IStream *s, const char *want)
{
    char buf[512];
    ULONG n = 0;
    LARGE_INTEGER zero = { 0 };
    s->lpVtbl->Seek(s, zero, STREAM_SEEK_SET, NULL);
    s->lpVtbl->Read(s, buf, sizeof(buf) - 1, &n);
    buf[n] = 0;
    if (strcmp(buf, want)) printf("   stream is [%s]\n", buf);
    return !strcmp(buf, want);
}

static const GUID IID_IStream_ = { 0x0000000c, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };

int main(void)
{
    HMODULE lib;
    HRESULT (WINAPI *create_body)(void **);
    HRESULT (WINAPI *charset_info)(void *, INETCSETINFO *);
    HRESULT (WINAPI *find_charset)(const char *, void **);
    void *body = NULL, *hcs = NULL, *hcs2 = NULL;
    IStream *in, *out;
    ULONG size, count;
    PARAMINFO *params;
    INETCSETINFO info;
    HRESULT hr;
    int i;

    CoInitialize(NULL);
    lib = LoadLibraryA("inetcomm.dll");
    create_body = (void *)GetProcAddress(lib, "MimeOleCreateBody");
    charset_info = (void *)GetProcAddress(lib, "MimeOleGetCharsetInfo");
    find_charset = (void *)GetProcAddress(lib, "MimeOleFindCharset");
    if (!create_body || !charset_info || !find_charset || FAILED(create_body(&body)) || !body)
    {
        check(0, "MimeOleCreateBody");
        printf("RESULT: FAIL\n");
        return 1;
    }

    /* charset: the default when untagged, the parameter when tagged */
    hr = GetCharset(body, &hcs);
    check(hr == S_OK && hcs != NULL, "an untagged body has a charset (the default)");

    in = stream_of("Content-Type: text/plain; charset=iso-8859-2\r\nSubject: x\r\n\r\n");
    Load(body, in);
    in->lpVtbl->Release(in);
    hr = GetCharset(body, &hcs);
    memset(&info, 0, sizeof(info));
    if (hcs) charset_info(hcs, &info);
    check(hr == S_OK && hcs && !strcasecmp(info.szName, "iso-8859-2"), "the charset parameter gives the charset");

    find_charset("utf-8", &hcs2);
    check(SetCharset(body, hcs2, 0) == S_OK, "SetCharset untagged on a tagged body");
    GetCharset(body, &hcs);
    memset(&info, 0, sizeof(info));
    if (hcs) charset_info(hcs, &info);
    check(!strcasecmp(info.szName, "iso-8859-2"), "untagged apply leaves a tagged body alone");
    check(SetCharset(body, hcs2, 1) == S_OK, "SetCharset all");
    GetCharset(body, &hcs);
    memset(&info, 0, sizeof(info));
    if (hcs) charset_info(hcs, &info);
    check(!strcasecmp(info.szName, "utf-8"), "apply all replaces the charset");
    count = 0; params = NULL;
    GetParameters(body, "Content-Type", &count, &params);
    check(count == 1 && params && !strcasecmp(params[0].pszName, "charset") && !strcasecmp(params[0].pszData, "utf-8"),
          "one charset parameter after two sets");
    check(SetCharset(body, NULL, 1) == E_INVALIDARG, "SetCharset(NULL) is invalid");
    BodyRelease(body);

    /* content type */
    create_body(&body);
    in = stream_of("raw bytes");
    check(SetData(body, IET_BINARY, "image", "png", &IID_IStream_, in) == S_OK, "SetData image/png");
    check(IsContentType(body, "image", "png") == S_OK, "the content type follows SetData");
    check(IsContentType(body, "text", "plain") == S_FALSE, "and is not text/plain any more");
    SetData(body, IET_BINARY, NULL, NULL, &IID_IStream_, in);
    check(IsContentType(body, "application", "octet-stream") == S_OK, "no type is application/octet-stream");

    /* size */
    size = 77;
    check(GetEstimatedSize(body, IET_BINARY, &size) == S_OK && size == 9, "estimated size as stored");
    size = 0;
    check(GetEstimatedSize(body, IET_BASE64, &size) == S_OK && size >= 12, "estimated size as base64 is larger");
    check(GetEstimatedSize(body, IET_BINARY, NULL) == E_INVALIDARG, "estimated size needs a pointer");

    /* data out */
    out = stream_of("");
    check(GetDataHere(body, IET_BINARY, out) == S_OK && stream_is(out, "raw bytes"), "GetDataHere copies the data");
    out->lpVtbl->Release(out);
    {
        char path[MAX_PATH];
        char buf[64] = "";
        DWORD n = 0;
        HANDLE f;
        GetTempPathA(sizeof(path), path);
        strcat(path, "sg-mimebody.bin");
        check(SaveToFile(body, IET_BINARY, path) == S_OK, "SaveToFile");
        f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) { ReadFile(f, buf, sizeof(buf) - 1, &n, NULL); CloseHandle(f); }
        check(n == 9 && !memcmp(buf, "raw bytes", 9), "the file holds the data");
        DeleteFileA(path);
        check(FAILED(SaveToFile(body, IET_BINARY, "Z:\\no\\such\\dir\\x.bin")), "SaveToFile to a bad path fails");
    }
    in->lpVtbl->Release(in);

    /* base64 data decoded on the way out */
    in = stream_of("aGVsbG8=");
    SetData(body, IET_BASE64, "text", "plain", &IID_IStream_, in);
    out = stream_of("");
    check(GetDataHere(body, IET_BINARY, out) == S_OK && stream_is(out, "hello"), "base64 data is decoded");
    out->lpVtbl->Release(out);
    in->lpVtbl->Release(in);

    /* emptied */
    check(EmptyData(body) == S_OK, "EmptyData");
    out = NULL;
    hr = GetData(body, IET_BINARY, &out);
    check(hr == MIME_E_NO_DATA && out == NULL, "no data after EmptyData");
    size = 5;
    check(GetEstimatedSize(body, IET_BINARY, &size) == MIME_E_NO_DATA && size == 0, "no size after EmptyData");
    out = stream_of("");
    check(GetDataHere(body, IET_BINARY, out) == MIME_E_NO_DATA, "GetDataHere has nothing");
    out->lpVtbl->Release(out);
    check(EmptyData(body) == S_OK, "EmptyData twice");
    BodyRelease(body);

    (void)i;
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
