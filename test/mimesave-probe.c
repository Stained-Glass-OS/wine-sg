/* inetcomm batch (patches/sg/2034), run by test/mimesave-gate.sh. Families:
 * the dirty flag (IsDirty), Save (headers with parameters, an empty line, the
 * data as stored), GetSizeMax, and the round trip through Load.
 * IMimeBody is called through its vtable slots.
 *
 *   mimesave-probe.exe */
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
#define IsDirty(o) ((HRESULT (WINAPI *)(void *))SLOT(o, 4))(o)
#define Save(o, s, c) ((HRESULT (WINAPI *)(void *, IStream *, BOOL))SLOT(o, 6))(o, s, c)
#define GetSizeMax(o, p) ((HRESULT (WINAPI *)(void *, ULARGE_INTEGER *))SLOT(o, 7))(o, p)
#define InitNew(o) ((HRESULT (WINAPI *)(void *))SLOT(o, 8))(o)
#define BSetProp(o, n, f, v) ((HRESULT (WINAPI *)(void *, const char *, DWORD, const PROPVARIANT *))SLOT(o, 12))(o, n, f, v)
#define DeleteProp(o, n) ((HRESULT (WINAPI *)(void *, const char *))SLOT(o, 14))(o, n)
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

static char *saved(void *body, BOOL clear, HRESULT *hr)
{
    static char buf[1024];
    IStream *s = stream_of("");
    ULONG n = 0;
    LARGE_INTEGER zero = { 0 };
    *hr = Save(body, s, clear);
    s->lpVtbl->Seek(s, zero, STREAM_SEEK_SET, NULL);
    s->lpVtbl->Read(s, buf, sizeof(buf) - 1, &n);
    buf[n] = 0;
    s->lpVtbl->Release(s);
    return buf;
}

int main(void)
{
    HMODULE lib;
    HRESULT (WINAPI *create_body)(void **);
    void *body = NULL, *body2 = NULL;
    IStream *in;
    ULARGE_INTEGER max;
    PROPVARIANT pv;
    HRESULT hr;
    char *text;
    ULONG count;
    PARAMINFO *params;

    CoInitialize(NULL);
    lib = LoadLibraryA("inetcomm.dll");
    create_body = (void *)GetProcAddress(lib, "MimeOleCreateBody");
    if (!create_body || FAILED(create_body(&body)) || !body) { check(0, "MimeOleCreateBody"); printf("RESULT: FAIL\n"); return 1; }

    check(IsDirty(body) == S_FALSE, "a new body is clean");
    InitNew(body);
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR; pv.pszVal = (char *)"hello";
    check(BSetProp(body, "Subject", 0, &pv) == S_OK, "SetProp");
    check(IsDirty(body) == S_OK, "SetProp makes the body dirty");
    BodyRelease(body);

    create_body(&body);
    in = stream_of("Content-Type: text/plain; charset=iso-8859-2; name=\"a b.txt\"\r\nSubject: hi\r\n\r\n");
    check(Load(body, in) == S_OK, "Load");
    in->lpVtbl->Release(in);
    check(IsDirty(body) == S_FALSE, "a loaded body is clean");

    in = stream_of("<b>x</b>");
    SetData(body, IET_8BIT, "text", "html", &IID_IStream_, in);
    in->lpVtbl->Release(in);
    check(IsDirty(body) == S_OK, "SetData makes the body dirty");

    text = saved(body, FALSE, &hr);
    check(hr == S_OK && !strcmp(text, "Content-Type: text/html; charset=iso-8859-2; name=\"a b.txt\"\r\nSubject: hi\r\n\r\n<b>x</b>"),
          "Save writes the headers, a blank line and the data");
    if (strcmp(text, "Content-Type: text/html; charset=iso-8859-2; name=\"a b.txt\"\r\nSubject: hi\r\n\r\n<b>x</b>")) printf("   saved [%s]\n", text);
    check(IsDirty(body) == S_OK, "Save keeps the body dirty");
    {
        char again[1024];
        strcpy(again, text);
        text = saved(body, TRUE, &hr);
        check(!strcmp(again, text), "a second Save writes the same text");
    }
    check(IsDirty(body) == S_FALSE, "Save with clear makes it clean");

    max.QuadPart = 0;
    check(GetSizeMax(body, &max) == S_OK && max.QuadPart == strlen(text), "GetSizeMax is the saved size");
    check(GetSizeMax(body, NULL) == E_INVALIDARG, "GetSizeMax(NULL)");
    check(Save(body, NULL, FALSE) == E_INVALIDARG, "Save(NULL)");

    /* a header added and a header removed */
    pv.vt = VT_LPSTR; pv.pszVal = (char *)"yes";
    BSetProp(body, "X-Custom", 0, &pv);
    DeleteProp(body, "Subject");
    check(IsDirty(body) == S_OK, "SetProp and DeleteProp make it dirty again");
    text = saved(body, TRUE, &hr);
    check(strstr(text, "X-Custom: yes\r\n") && !strstr(text, "Subject"), "the added header is there, the deleted one gone");

    /* round trip */
    create_body(&body2);
    in = stream_of(text);
    Load(body2, in);
    in->lpVtbl->Release(in);
    count = 0; params = NULL;
    GetParameters(body2, "Content-Type", &count, &params);
    check(count == 2 && params && !strcasecmp(params[0].pszName, "charset") && !strcmp(params[1].pszData, "a b.txt"),
          "the parameters survive a round trip, quoted value included");
    check(IsContentType(body2, "text", "html") == S_OK, "and the content type");
    BodyRelease(body2);

    /* no data: headers and the blank line only */
    BodyRelease(body);
    create_body(&body);
    pv.vt = VT_LPSTR; pv.pszVal = (char *)"only";
    BSetProp(body, "Subject", 0, &pv);
    text = saved(body, FALSE, &hr);
    check(hr == S_OK && !strcmp(text, "Subject: only\r\n\r\n"), "a body without data saves its headers");
    BodyRelease(body);

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
