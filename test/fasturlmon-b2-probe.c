/* urlmon batch 2 probe: runs under Wine, prints "ok"/"FAIL" lines and a RESULT line. */
#define __USE_MINGW_ANSI_STDIO 0
#define COBJMACROS
#define CONST_VTABLE
#include <winsock2.h>
#include <windows.h>
#include <ole2.h>
#include <urlmon.h>
#include <hlink.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>

#define INTERNET_MAX_URL_LENGTH 2084
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (cond) printf("ok   %s\n", #cond); \
    else { failures++; printf("FAIL %s:%d %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 20, QS_ALLINPUT);
    }
}

/* ---------------------------------------------------------------- generic helpers */
static WCHAR tmpdir[MAX_PATH];

static void make_file(const WCHAR *name, const char *data, DWORD len, WCHAR *path)
{
    HANDLE h;
    DWORD w;
    lstrcpyW(path, tmpdir);
    lstrcatW(path, L"\\");
    lstrcatW(path, name);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, data, len, &w, NULL);
    CloseHandle(h);
}

static void file_url(const WCHAR *path, WCHAR *url)
{
    DWORD len = INTERNET_MAX_URL_LENGTH;
    UrlCreateFromPathW(path, url, &len, 0);
}

/* ---------------------------------------------------------------- sink / bind info */
typedef struct {
    IInternetProtocolSink sink;
    IInternetBindInfo info;
    IServiceProvider sp;
    IHttpNegotiate neg;
    DWORD bindf;
    HRESULT result;
    volatile int reported_result;
    volatile int got_data;
} TestSink;

static HRESULT WINAPI sink_QI(IInternetProtocolSink *iface, REFIID riid, void **ppv)
{
    TestSink *s = (TestSink *)iface;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IInternetProtocolSink)) *ppv = &s->sink;
    else if (IsEqualGUID(riid, &IID_IInternetBindInfo)) *ppv = &s->info;
    else if (IsEqualGUID(riid, &IID_IServiceProvider)) *ppv = &s->sp;
    else { *ppv = NULL; return E_NOINTERFACE; }
    return S_OK;
}
static ULONG WINAPI sink_AddRef(IInternetProtocolSink *iface) { return 2; }
static ULONG WINAPI sink_Release(IInternetProtocolSink *iface) { return 1; }
static HRESULT WINAPI sink_Switch(IInternetProtocolSink *iface, PROTOCOLDATA *d) { return S_OK; }
static HRESULT WINAPI sink_Progress(IInternetProtocolSink *iface, ULONG c, LPCWSTR t) { return S_OK; }
static HRESULT WINAPI sink_Data(IInternetProtocolSink *iface, DWORD f, ULONG p, ULONG m)
{
    ((TestSink *)iface)->got_data = 1;
    return S_OK;
}
static HRESULT WINAPI sink_Result(IInternetProtocolSink *iface, HRESULT hr, DWORD err, LPCWSTR s)
{
    TestSink *ts = (TestSink *)iface;
    ts->result = hr;
    ts->reported_result = 1;
    return S_OK;
}
static IInternetProtocolSinkVtbl sink_vtbl = { sink_QI, sink_AddRef, sink_Release, sink_Switch,
                                               sink_Progress, sink_Data, sink_Result };

static HRESULT WINAPI info_QI(IInternetBindInfo *iface, REFIID riid, void **ppv)
{
    TestSink *s = CONTAINING_RECORD(iface, TestSink, info);
    return sink_QI(&s->sink, riid, ppv);
}
static ULONG WINAPI info_AddRef(IInternetBindInfo *iface) { return 2; }
static ULONG WINAPI info_Release(IInternetBindInfo *iface) { return 1; }
static HRESULT WINAPI info_GetBindInfo(IInternetBindInfo *iface, DWORD *bindf, BINDINFO *bi)
{
    TestSink *s = CONTAINING_RECORD(iface, TestSink, info);
    DWORD size = bi->cbSize;
    memset(bi, 0, size);
    bi->cbSize = size;
    *bindf = s->bindf;
    return S_OK;
}
static HRESULT WINAPI info_GetBindString(IInternetBindInfo *iface, ULONG t, LPOLESTR *p, ULONG c, ULONG *f)
{
    if (t == BINDSTRING_ACCEPT_MIMES)
    {
        p[0] = CoTaskMemAlloc(4 * sizeof(WCHAR));
        lstrcpyW(p[0], L"*/*");
        *f = 1;
        return S_OK;
    }
    return E_NOTIMPL;
}
static IInternetBindInfoVtbl info_vtbl = { info_QI, info_AddRef, info_Release, info_GetBindInfo, info_GetBindString };

static HRESULT WINAPI sp_QI(IServiceProvider *iface, REFIID riid, void **ppv)
{
    TestSink *s = CONTAINING_RECORD(iface, TestSink, sp);
    return sink_QI(&s->sink, riid, ppv);
}
static HRESULT WINAPI neg_BeginningTransaction(IHttpNegotiate *iface, LPCWSTR url, LPCWSTR hdrs, DWORD r, LPWSTR *add)
{
    *add = NULL;
    return S_OK;
}
static HRESULT WINAPI neg_OnResponse(IHttpNegotiate *iface, DWORD code, LPCWSTR resp, LPCWSTR req, LPWSTR *add)
{
    return S_OK;
}
static HRESULT WINAPI neg_QI(IHttpNegotiate *iface, REFIID riid, void **ppv)
{
    TestSink *s = CONTAINING_RECORD(iface, TestSink, neg);
    return sink_QI(&s->sink, riid, ppv);
}
static IHttpNegotiateVtbl neg_vtbl = { neg_QI, (void *)info_AddRef, (void *)info_Release, neg_BeginningTransaction, neg_OnResponse };

static HRESULT WINAPI sp_QueryService(IServiceProvider *iface, REFGUID service, REFIID riid, void **ppv)
{
    TestSink *s = CONTAINING_RECORD(iface, TestSink, sp);
    if (IsEqualGUID(service, &IID_IHttpNegotiate) && IsEqualGUID(riid, &IID_IHttpNegotiate))
    {
        *ppv = &s->neg;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static IServiceProviderVtbl sp_vtbl = { sp_QI, (void *)info_AddRef, (void *)info_Release, sp_QueryService };

static void init_sink(TestSink *s, DWORD bindf)
{
    memset(s, 0, sizeof(*s));
    s->sp.lpVtbl = &sp_vtbl;
    s->neg.lpVtbl = &neg_vtbl;
    s->sink.lpVtbl = &sink_vtbl;
    s->info.lpVtbl = &info_vtbl;
    s->bindf = bindf;
}

/* ---------------------------------------------------------------- foreign IUri */
typedef struct { IUri iface; } ForeignUri;
static HRESULT WINAPI furi_QI(IUri *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IUri)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI furi_AddRef(IUri *iface) { return 2; }
static ULONG WINAPI furi_Release(IUri *iface) { return 1; }
static HRESULT WINAPI furi_GetPropertyBSTR(IUri *i, Uri_PROPERTY p, BSTR *b, DWORD f) { *b = NULL; return E_ACCESSDENIED; }
static HRESULT WINAPI furi_GetPropertyLength(IUri *i, Uri_PROPERTY p, DWORD *d, DWORD f) { return E_ACCESSDENIED; }
static HRESULT WINAPI furi_GetPropertyDWORD(IUri *i, Uri_PROPERTY p, DWORD *d, DWORD f) { return E_ACCESSDENIED; }
static HRESULT WINAPI furi_HasProperty(IUri *i, Uri_PROPERTY p, BOOL *b) { return E_ACCESSDENIED; }
static HRESULT WINAPI furi_Str(IUri *i, BSTR *b) { *b = NULL; return E_ACCESSDENIED; }
static HRESULT WINAPI furi_Dword(IUri *i, DWORD *d) { return E_ACCESSDENIED; }
static HRESULT WINAPI furi_IsEqual(IUri *i, IUri *o, BOOL *b) { return E_ACCESSDENIED; }
static IUriVtbl furi_vtbl = {
    furi_QI, furi_AddRef, furi_Release, furi_GetPropertyBSTR, furi_GetPropertyLength, furi_GetPropertyDWORD,
    furi_HasProperty, furi_Str, furi_Str, furi_Str, furi_Str, furi_Str, furi_Str, furi_Str, furi_Str,
    furi_Str, furi_Str, furi_Str, furi_Str, furi_Str, furi_Str, furi_Str, furi_Dword, furi_Dword, furi_Dword,
    furi_Dword, furi_Dword, furi_IsEqual
};

static void test_uri(void)
{
    ForeignUri foreign = { { &furi_vtbl } };
    IUri *result, *base;
    IUriBuilder *builder;
    WCHAR buf[300];
    DWORD len, len2;
    HRESULT hr, hr2;

    /* a foreign IUri that fails: the error comes back, not E_NOTIMPL */
    result = (void *)1;
    hr = CoInternetCombineUrlEx(&foreign.iface, L"rel.htm", 0, &result, 0);
    CHECK(hr == E_ACCESSDENIED, "CombineUrlEx foreign: %08lx", hr);

    hr = CreateUri(L"http://www.example.com/dir/", 0, 0, &base);
    CHECK(hr == S_OK, "CreateUri %08lx", hr);
    hr = CoInternetCombineIUri(base, &foreign.iface, 0, &result, 0);
    CHECK(hr == E_ACCESSDENIED, "CombineIUri foreign relative: %08lx", hr);

    len = 0;
    hr = CoInternetParseIUri(&foreign.iface, PARSE_CANONICALIZE, 0, buf, ARRAY_SIZE(buf), &len, 0);
    CHECK(hr == E_ACCESSDENIED, "ParseIUri foreign: %08lx", hr);

    hr = CreateIUriBuilder(&foreign.iface, 0, 0, &builder);
    CHECK(hr == E_ACCESSDENIED, "CreateIUriBuilder foreign: %08lx", hr);

    hr = CreateIUriBuilder(NULL, 0, 0, &builder);
    CHECK(hr == S_OK, "CreateIUriBuilder %08lx", hr);
    hr = IUriBuilder_SetIUri(builder, &foreign.iface);
    CHECK(hr == E_ACCESSDENIED, "SetIUri foreign: %08lx", hr);
    IUriBuilder_Release(builder);

    /* the encoding mask on a modified builder is E_NOTIMPL on Windows too */
    hr = CreateIUriBuilder(base, 0, 0, &builder);
    IUriBuilder_SetFragment(builder, L"x");
    result = (void *)1;
    hr = IUriBuilder_CreateUri(builder, 0, 1, 0, &result);
    CHECK(hr == E_NOTIMPL, "builder CreateUri with mask: %08lx", hr);
    IUriBuilder_Release(builder);

    /* the escaping actions of the IUri version match the string version */
    {
        static const WCHAR *urls[] = { L"http://www.example.com/a%20b/c d?x=%41", L"http://www.example.com/p%61th" };
        static const PARSEACTION actions[] = { PARSE_ESCAPE, PARSE_UNESCAPE, PARSE_ENCODE, PARSE_DECODE };
        int i, j;

        for (i = 0; i < 2; i++)
        {
            IUri *uri;
            BSTR canon;

            hr = CreateUri(urls[i], 0, 0, &uri);
            CHECK(hr == S_OK, "CreateUri %08lx", hr);
            IUri_GetAbsoluteUri(uri, &canon);
            for (j = 0; j < 4; j++)
            {
                WCHAR b1[300], b2[300];
                len = len2 = 0;
                hr = CoInternetParseIUri(uri, actions[j], 0, b1, ARRAY_SIZE(b1), &len, 0);
                hr2 = CoInternetParseUrl(canon, actions[j], 0, b2, ARRAY_SIZE(b2), &len2, 0);
                CHECK(hr == hr2 && hr == S_OK && len == len2 && !lstrcmpW(b1, b2),
                      "action %d on %d: %08lx %08lx %lu %lu", actions[j], i, hr, hr2, len, len2);
            }
            SysFreeString(canon);
            IUri_Release(uri);
        }
    }
    IUri_Release(base);
}

/* ---------------------------------------------------------------- IStorage binding, stream, pull stream */
typedef struct {
    IBindStatusCallback iface;
    int stopped;
    HRESULT stop_hr;
    int data_calls;
    int got_stream;
    char data[64];
    DWORD data_len;
    DWORD progress[128];
    int progress_count;
    int saw_gzip;
    int start_called;
    IBinding *binding;
    IStorage *storage;
    DWORD bindf;
    int stg_calls;
} TestBSC;

static HRESULT WINAPI bsc_QI(IBindStatusCallback *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IBindStatusCallback)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI bsc_AddRef(IBindStatusCallback *iface) { return 2; }
static ULONG WINAPI bsc_Release(IBindStatusCallback *iface) { return 1; }
static HRESULT WINAPI bsc_OnStart(IBindStatusCallback *iface, DWORD r, IBinding *b)
{
    TestBSC *t = (TestBSC *)iface;
    t->start_called++;
    if (b) { IBinding_AddRef(b); t->binding = b; }
    return S_OK;
}
static HRESULT WINAPI bsc_GetPriority(IBindStatusCallback *iface, LONG *p) { return E_NOTIMPL; }
static HRESULT WINAPI bsc_OnLow(IBindStatusCallback *iface, DWORD r) { return E_NOTIMPL; }
static HRESULT WINAPI bsc_OnProgress(IBindStatusCallback *iface, ULONG p, ULONG m, ULONG code, LPCWSTR text)
{
    TestBSC *t = (TestBSC *)iface;
    if (t->progress_count < 128) t->progress[t->progress_count++] = code;
    if (code == BINDSTATUS_ENCODING && text && !lstrcmpW(text, L"gzip")) t->saw_gzip = 1;
    return S_OK;
}
static HRESULT WINAPI bsc_OnStop(IBindStatusCallback *iface, HRESULT hr, LPCWSTR e)
{
    TestBSC *t = (TestBSC *)iface;
    t->stopped = 1;
    t->stop_hr = hr;
    return S_OK;
}
static HRESULT WINAPI bsc_GetBindInfo(IBindStatusCallback *iface, DWORD *f, BINDINFO *bi)
{
    TestBSC *t = (TestBSC *)iface;
    DWORD size = bi->cbSize;
    memset(bi, 0, size);
    bi->cbSize = size;
    *f = t->bindf;
    return S_OK;
}
static HRESULT WINAPI bsc_OnData(IBindStatusCallback *iface, DWORD bscf, DWORD size, FORMATETC *fe, STGMEDIUM *sm)
{
    TestBSC *t = (TestBSC *)iface;
    t->data_calls++;
    if (sm->tymed == TYMED_ISTREAM && sm->pstm)
    {
        ULONG n = 0;
        t->got_stream = 1;
        IStream_Read(sm->pstm, t->data + t->data_len, sizeof(t->data) - t->data_len, &n);
        t->data_len += n;
    }
    else if (sm->tymed == TYMED_ISTORAGE && sm->pstg)
    {
        t->stg_calls++;
        if (!t->storage) { t->storage = sm->pstg; IStorage_AddRef(t->storage); }
    }
    return S_OK;
}
static HRESULT WINAPI bsc_OnObject(IBindStatusCallback *iface, REFIID r, IUnknown *u) { return S_OK; }
static IBindStatusCallbackVtbl bsc_vtbl = { bsc_QI, bsc_AddRef, bsc_Release, bsc_OnStart, bsc_GetPriority, bsc_OnLow,
                                            bsc_OnProgress, bsc_OnStop, bsc_GetBindInfo, bsc_OnData, bsc_OnObject };

static void init_bsc(TestBSC *t, DWORD bindf)
{
    memset(t, 0, sizeof(*t));
    t->iface.lpVtbl = &bsc_vtbl;
    t->bindf = bindf;
}

static void test_binding(void)
{
    WCHAR path[MAX_PATH], url[INTERNET_MAX_URL_LENGTH];
    IStorage *stg;
    IStream *stream, *sub;
    IMoniker *mon;
    IBindCtx *bc;
    ULONG n;
    char buf[16];
    HRESULT hr;
    ULARGE_INTEGER size;
    static TestBSC bsc;
    int i;

    /* IStorage: the file is a compound file */
    lstrcpyW(path, tmpdir);
    lstrcatW(path, L"\\doc.stg");
    hr = StgCreateDocfile(path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    CHECK(hr == S_OK, "StgCreateDocfile %08lx", hr);
    hr = IStorage_CreateStream(stg, L"hello", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &sub);
    IStream_Write(sub, "abcdef", 6, &n);
    IStream_Release(sub);
    IStorage_Commit(stg, STGC_DEFAULT);
    IStorage_Release(stg);

    file_url(path, url);
    hr = CreateURLMoniker(NULL, url, &mon);
    CHECK(hr == S_OK, "CreateURLMoniker %08lx", hr);
    hr = CreateBindCtx(0, &bc);
    stg = NULL;
    hr = IMoniker_BindToStorage(mon, bc, NULL, &IID_IStorage, (void **)&stg);
    CHECK(hr == S_OK && stg, "BindToStorage(IStorage) %08lx %p", hr, stg);
    if (stg)
    {
        memset(buf, 0, sizeof(buf));
        hr = IStorage_OpenStream(stg, L"hello", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &sub);
        CHECK(hr == S_OK, "OpenStream %08lx", hr);
        if (hr == S_OK) { IStream_Read(sub, buf, 6, &n); IStream_Release(sub); }
        CHECK(!strcmp(buf, "abcdef"), "stream data '%s'", buf);
        IStorage_Release(stg);
    }
    IBindCtx_Release(bc);
    IMoniker_Release(mon);

    /* a file that is not a compound file is refused */
    make_file(L"plain.txt", "just text", 9, path);
    file_url(path, url);
    CreateURLMoniker(NULL, url, &mon);
    CreateBindCtx(0, &bc);
    stg = (void *)1;
    hr = IMoniker_BindToStorage(mon, bc, NULL, &IID_IStorage, (void **)&stg);
    CHECK(FAILED(hr), "plain file as storage: %08lx", hr);
    if (SUCCEEDED(hr) && stg) IStorage_Release(stg);
    IBindCtx_Release(bc);
    IMoniker_Release(mon);

    /* the downloaded stream is read-only */
    make_file(L"data.bin", "0123456789", 10, path);
    file_url(path, url);
    init_bsc(&bsc, 0);
    hr = URLOpenBlockingStreamW(NULL, url, &stream, 0, &bsc.iface);
    CHECK(hr == S_OK && stream, "URLOpenBlockingStream %08lx", hr);
    if (stream)
    {
        size.QuadPart = 5;
        hr = IStream_SetSize(stream, size);
        CHECK(hr == STG_E_ACCESSDENIED, "SetSize %08lx", hr);
        IStream_Release(stream);
    }

    /* progress statuses that urlmon itself does not use reach the client (custom protocol below) */
    (void)i;

    /* pull stream */
    init_bsc(&bsc, 0);
    bsc.bindf = BINDF_ASYNCHRONOUS | BINDF_PULLDATA;
    hr = URLOpenPullStreamW(NULL, url, 0, &bsc.iface);
    CHECK(SUCCEEDED(hr), "URLOpenPullStream %08lx", hr);
    for (i = 0; i < 100 && !bsc.stopped; i++) pump(100);
    CHECK(bsc.stopped && bsc.stop_hr == S_OK, "pull stopped=%d hr=%08lx", bsc.stopped, bsc.stop_hr);
    CHECK(bsc.got_stream && bsc.data_len == 10 && !memcmp(bsc.data, "0123456789", 10),
          "pull data len %lu", bsc.data_len);
}

/* ---------------------------------------------------------------- protocol seek / abort */
static void test_file_protocol(void)
{
    WCHAR path[MAX_PATH], url[INTERNET_MAX_URL_LENGTH];
    IInternetProtocol *prot, *bindprot;
    IInternetSession *session;
    TestSink sink;
    LARGE_INTEGER move;
    ULARGE_INTEGER pos;
    char buf[8];
    ULONG n;
    HRESULT hr;

    make_file(L"seek.bin", "0123456789", 10, path);
    file_url(path, url);

    hr = CoCreateInstance(&CLSID_FileProtocol, NULL, CLSCTX_INPROC_SERVER, &IID_IInternetProtocol, (void **)&prot);
    CHECK(hr == S_OK, "create file protocol %08lx", hr);
    hr = IInternetProtocol_Abort(prot, E_ABORT, 0);
    CHECK(hr == S_OK, "file protocol early abort %08lx", hr);
    IInternetProtocol_Release(prot);

    hr = CoCreateInstance(&CLSID_MkProtocol, NULL, CLSCTX_INPROC_SERVER, &IID_IInternetProtocol, (void **)&prot);
    CHECK(hr == S_OK, "create mk protocol %08lx", hr);
    hr = IInternetProtocol_Abort(prot, E_ABORT, 0);
    CHECK(hr == S_OK, "mk protocol early abort %08lx", hr);
    IInternetProtocol_Release(prot);

    /* the file protocol itself */
    hr = CoCreateInstance(&CLSID_FileProtocol, NULL, CLSCTX_INPROC_SERVER, &IID_IInternetProtocol, (void **)&prot);
    init_sink(&sink, 0);
    hr = IInternetProtocol_Start(prot, url, &sink.sink, &sink.info, 0, 0);
    CHECK(hr == S_OK, "file Start %08lx", hr);
    move.QuadPart = 4;
    pos.QuadPart = 99;
    hr = IInternetProtocol_Seek(prot, move, FILE_BEGIN, &pos);
    CHECK(hr == S_OK && pos.QuadPart == 4, "file Seek begin %08lx %I64u", hr, pos.QuadPart);
    memset(buf, 0, sizeof(buf));
    hr = IInternetProtocol_Read(prot, buf, 3, &n);
    CHECK(n == 3 && !memcmp(buf, "456", 3), "read after seek: %08lx '%s'", hr, buf);
    move.QuadPart = -2;
    hr = IInternetProtocol_Seek(prot, move, FILE_END, &pos);
    CHECK(hr == S_OK && pos.QuadPart == 8, "file Seek end %08lx %I64u", hr, pos.QuadPart);
    move.QuadPart = -5;
    hr = IInternetProtocol_Seek(prot, move, FILE_CURRENT, &pos);
    CHECK(hr == S_OK && pos.QuadPart == 3, "file Seek current %08lx %I64u", hr, pos.QuadPart);
    hr = IInternetProtocol_Seek(prot, move, 7, &pos);
    CHECK(FAILED(hr), "file Seek bad origin %08lx", hr);
    IInternetProtocol_Terminate(prot, 0);
    IInternetProtocol_Release(prot);

    /* through the bind protocol */
    hr = CoInternetGetSession(0, &session, 0);
    CHECK(hr == S_OK, "CoInternetGetSession %08lx", hr);
    hr = IInternetSession_CreateBinding(session, NULL, url, NULL, NULL, &bindprot, 0);
    CHECK(hr == S_OK, "CreateBinding %08lx", hr);
    if (hr == S_OK)
    {
        init_sink(&sink, 0);
        hr = IInternetProtocol_Start(bindprot, url, &sink.sink, &sink.info, 0, 0);
        CHECK(hr == S_OK, "bind Start %08lx", hr);
        memset(buf, 0, sizeof(buf));
        hr = IInternetProtocol_Read(bindprot, buf, 2, &n);
        CHECK(n == 2 && !memcmp(buf, "01", 2), "bind read %08lx '%s'", hr, buf);
        move.QuadPart = 6;
        hr = IInternetProtocol_Seek(bindprot, move, FILE_BEGIN, &pos);
        CHECK(hr == S_OK && pos.QuadPart == 6, "bind Seek %08lx %I64u", hr, pos.QuadPart);
        memset(buf, 0, sizeof(buf));
        hr = IInternetProtocol_Read(bindprot, buf, 2, &n);
        CHECK(n == 2 && !memcmp(buf, "67", 2), "bind read after seek %08lx '%s'", hr, buf);
        IInternetProtocol_Terminate(bindprot, 0);
        IInternetProtocol_Release(bindprot);
    }
    IInternetSession_Release(session);
}

/* ---------------------------------------------------------------- http server for the network seek */
static SOCKET listener;
static volatile int server_done;

static DWORD WINAPI server_thread(void *arg)
{
    SOCKET c = accept(listener, NULL, NULL);
    char req[2048];
    static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n"
                                "Connection: close\r\n\r\n0123456789";
    if (c != INVALID_SOCKET)
    {
        recv(c, req, sizeof(req), 0);
        send(c, reply, sizeof(reply) - 1, 0);
        Sleep(200);
        closesocket(c);
    }
    server_done = 1;
    return 0;
}

static void test_http_seek(void)
{
    WSADATA wsa;
    struct sockaddr_in addr;
    int len = sizeof(addr);
    WCHAR url[100];
    IInternetProtocol *prot;
    TestSink sink;
    LARGE_INTEGER move;
    ULARGE_INTEGER pos;
    char buf[16];
    ULONG n;
    HRESULT hr;
    HANDLE thread;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 1);
    getsockname(listener, (struct sockaddr *)&addr, &len);
    wsprintfW(url, L"http://127.0.0.1:%u/seek.txt", ntohs(addr.sin_port));
    thread = CreateThread(NULL, 0, server_thread, NULL, 0, NULL);

    hr = CoCreateInstance(&CLSID_HttpProtocol, NULL, CLSCTX_INPROC_SERVER, &IID_IInternetProtocol, (void **)&prot);
    CHECK(hr == S_OK, "create http protocol %08lx", hr);
    init_sink(&sink, 0);
    hr = IInternetProtocol_Start(prot, url, &sink.sink, &sink.info, 0, 0);
    CHECK(hr == S_OK, "http Start %08lx", hr);
    for (n = 0; n < 100 && !sink.got_data; n++) pump(100);
    CHECK(sink.got_data, "http data arrived");

    move.QuadPart = -1;
    hr = IInternetProtocol_Seek(prot, move, FILE_BEGIN, &pos);
    CHECK(FAILED(hr), "http Seek to a negative position %08lx", hr);
    move.QuadPart = 3;
    pos.QuadPart = 0;
    hr = IInternetProtocol_Seek(prot, move, FILE_BEGIN, &pos);
    CHECK(hr == S_OK && pos.QuadPart == 3, "http Seek forward %08lx %I64u", hr, pos.QuadPart);
    memset(buf, 0, sizeof(buf));
    hr = IInternetProtocol_Read(prot, buf, 4, &n);
    CHECK(n == 4 && !memcmp(buf, "3456", 4), "http read after seek %08lx %lu '%s'", hr, n, buf);
    move.QuadPart = 1;
    hr = IInternetProtocol_Seek(prot, move, FILE_BEGIN, &pos);
    CHECK(FAILED(hr), "http Seek back %08lx", hr);
    move.QuadPart = -1;
    pos.QuadPart = 0;
    hr = IInternetProtocol_Seek(prot, move, FILE_END, &pos);
    CHECK(hr == S_OK && pos.QuadPart == 9, "http Seek from end %08lx %I64u", hr, pos.QuadPart);
    memset(buf, 0, sizeof(buf));
    hr = IInternetProtocol_Read(prot, buf, 4, &n);
    CHECK(n == 1 && buf[0] == '9', "http read last %08lx %lu", hr, n);
    IInternetProtocol_Read(prot, buf, 4, &n);
    for (n = 0; n < 50 && !sink.reported_result; n++) pump(100);

    IInternetProtocol_Terminate(prot, 0);
    IInternetProtocol_Release(prot);
    closesocket(listener);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
}

/* ---------------------------------------------------------------- ftp error ends the bind */
static void test_ftp_error(void)
{
    IStream *stream = NULL;
    DWORD start = GetTickCount();
    HRESULT hr;

    hr = URLOpenBlockingStreamW(NULL, L"ftp://127.0.0.1:1/none.txt", &stream, 0, NULL);
    CHECK(FAILED(hr), "ftp to a closed port fails: %08lx", hr);
    CHECK(GetTickCount() - start < 60000, "ftp failure was prompt: %lu ms", GetTickCount() - start);
    if (stream) IStream_Release(stream);
}

/* ---------------------------------------------------------------- custom protocol reporting statuses */
typedef struct { IInternetProtocol iface; LONG ref; int done; } SgProtocol;

static HRESULT WINAPI sgp_QI(IInternetProtocol *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IInternetProtocolRoot)
        || IsEqualGUID(riid, &IID_IInternetProtocol)) { *ppv = iface; InterlockedIncrement(&((SgProtocol *)iface)->ref); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sgp_AddRef(IInternetProtocol *iface) { return InterlockedIncrement(&((SgProtocol *)iface)->ref); }
static ULONG WINAPI sgp_Release(IInternetProtocol *iface)
{
    SgProtocol *p = (SgProtocol *)iface;
    LONG ref = InterlockedDecrement(&p->ref);
    if (!ref) free(p);
    return ref;
}
static HRESULT WINAPI sgp_Start(IInternetProtocol *iface, LPCWSTR url, IInternetProtocolSink *sink,
                                IInternetBindInfo *info, DWORD flags, HANDLE_PTR reserved)
{
    IInternetProtocolSink_ReportProgress(sink, BINDSTATUS_ENCODING, L"gzip");
    IInternetProtocolSink_ReportProgress(sink, BINDSTATUS_COOKIE_SENT, NULL);
    IInternetProtocolSink_ReportProgress(sink, BINDSTATUS_MIMETYPEAVAILABLE, L"text/plain");
    IInternetProtocolSink_ReportData(sink, BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION |
                                     BSCF_DATAFULLYAVAILABLE, 4, 4);
    IInternetProtocolSink_ReportResult(sink, S_OK, 0, NULL);
    return S_OK;
}
static HRESULT WINAPI sgp_Continue(IInternetProtocol *iface, PROTOCOLDATA *d) { return S_OK; }
static HRESULT WINAPI sgp_Abort(IInternetProtocol *iface, HRESULT r, DWORD o) { return S_OK; }
static HRESULT WINAPI sgp_Terminate(IInternetProtocol *iface, DWORD o) { return S_OK; }
static HRESULT WINAPI sgp_Suspend(IInternetProtocol *iface) { return E_NOTIMPL; }
static HRESULT WINAPI sgp_Resume(IInternetProtocol *iface) { return E_NOTIMPL; }
static HRESULT WINAPI sgp_Read(IInternetProtocol *iface, void *pv, ULONG cb, ULONG *read)
{
    SgProtocol *p = (SgProtocol *)iface;
    if (p->done) { *read = 0; return S_FALSE; }
    p->done = 1;
    memcpy(pv, "abcd", 4);
    *read = 4;
    return S_OK;
}
static HRESULT WINAPI sgp_Seek(IInternetProtocol *iface, LARGE_INTEGER m, DWORD o, ULARGE_INTEGER *p) { return E_NOTIMPL; }
static HRESULT WINAPI sgp_Lock(IInternetProtocol *iface, DWORD o) { return S_OK; }
static HRESULT WINAPI sgp_Unlock(IInternetProtocol *iface) { return S_OK; }
static IInternetProtocolVtbl sgp_vtbl = { sgp_QI, sgp_AddRef, sgp_Release, sgp_Start, sgp_Continue, sgp_Abort,
    sgp_Terminate, sgp_Suspend, sgp_Resume, sgp_Read, sgp_Seek, sgp_Lock, sgp_Unlock };

static HRESULT WINAPI cf_QI(IClassFactory *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_Create(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    SgProtocol *p = calloc(1, sizeof(*p));
    HRESULT hr;
    p->iface.lpVtbl = &sgp_vtbl;
    p->ref = 1;
    hr = IInternetProtocol_QueryInterface(&p->iface, riid, ppv);
    IInternetProtocol_Release(&p->iface);
    return hr;
}
static HRESULT WINAPI cf_Lock(IClassFactory *iface, BOOL l) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_Create, cf_Lock };
static IClassFactory sg_cf = { &cf_vtbl };

static void test_status_forwarding(void)
{
    IInternetSession *session;
    IStream *stream = NULL;
    static TestBSC bsc;
    HRESULT hr;
    int i, seen_cookie = 0;

    hr = CoInternetGetSession(0, &session, 0);
    hr = IInternetSession_RegisterNameSpace(session, &sg_cf, &IID_NULL, L"sgtest", 0, NULL, 0);
    CHECK(hr == S_OK, "RegisterNameSpace %08lx", hr);

    init_bsc(&bsc, BINDF_ASYNCHRONOUS | BINDF_PULLDATA);
    hr = URLOpenPullStreamW(NULL, L"sgtest:payload", 0, &bsc.iface);
    CHECK(SUCCEEDED(hr), "open sgtest stream %08lx", hr);
    for (i = 0; i < 100 && !bsc.stopped; i++) pump(100);
    CHECK(bsc.stopped && bsc.stop_hr == S_OK, "sgtest stopped %d %08lx", bsc.stopped, bsc.stop_hr);
    for (i = 0; i < bsc.progress_count; i++)
        if (bsc.progress[i] == BINDSTATUS_COOKIE_SENT) seen_cookie = 1;
    CHECK(bsc.saw_gzip, "BINDSTATUS_ENCODING reached the client");
    CHECK(seen_cookie, "BINDSTATUS_COOKIE_SENT reached the client");
    if (stream) IStream_Release(stream);
    IInternetSession_UnregisterNameSpace(session, &sg_cf, L"sgtest");
    IInternetSession_Release(session);
}

/* ---------------------------------------------------------------- GetBindResult across apartments */
static IStream *binding_stream;
static HRESULT remote_hr;
static DWORD remote_result;

static DWORD WINAPI marshal_thread(void *arg)
{
    IBinding *b = NULL;
    CLSID clsid;
    DWORD res = 0;
    LPOLESTR str = NULL;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    remote_hr = CoGetInterfaceAndReleaseStream(binding_stream, &IID_IBinding, (void **)&b);
    if (SUCCEEDED(remote_hr))
    {
        remote_hr = IBinding_GetBindResult(b, &clsid, &res, &str, NULL);
        remote_result = res;
        IBinding_Release(b);
    }
    CoUninitialize();
    return 0;
}

static void test_bindresult_proxy(void)
{
    WCHAR path[MAX_PATH], url[INTERNET_MAX_URL_LENGTH];
    IStream *stream = NULL;
    TestBSC bsc;
    HRESULT hr;
    HANDLE thread;
    MSG msg;
    DWORD r;

    make_file(L"bind.bin", "xyz", 3, path);
    file_url(path, url);
    init_bsc(&bsc, 0);
    hr = URLOpenBlockingStreamW(NULL, url, &stream, 0, &bsc.iface);
    CHECK(hr == S_OK && bsc.binding, "got IBinding %08lx %p", hr, bsc.binding);
    if (stream) IStream_Release(stream);
    if (!bsc.binding) return;

    hr = CoMarshalInterThreadInterfaceInStream(&IID_IBinding, (IUnknown *)bsc.binding, &binding_stream);
    CHECK(hr == S_OK, "marshal IBinding %08lx", hr);
    if (hr != S_OK) return;
    remote_hr = E_FAIL;
    thread = CreateThread(NULL, 0, marshal_thread, NULL, 0, NULL);
    for (;;)
    {
        r = MsgWaitForMultipleObjects(1, &thread, FALSE, 20000, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0 + 1) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg); continue; }
        break;
    }
    CHECK(remote_hr == S_OK, "remote GetBindResult %08lx", remote_hr);
    CloseHandle(thread);
    IBinding_Release(bsc.binding);
}

/* ---------------------------------------------------------------- wrapper callbacks */
typedef struct {
    IBindStatusCallback bsc;
    IAuthenticate auth;
    IInternetBindInfo info;
} FwdBSC;

static HRESULT WINAPI fwd_QI(IBindStatusCallback *iface, REFIID riid, void **ppv)
{
    FwdBSC *f = (FwdBSC *)iface;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IBindStatusCallback)) *ppv = &f->bsc;
    else if (IsEqualGUID(riid, &IID_IAuthenticate)) *ppv = &f->auth;
    else if (IsEqualGUID(riid, &IID_IInternetBindInfo)) *ppv = &f->info;
    else { *ppv = NULL; return E_NOINTERFACE; }
    return S_OK;
}
static HRESULT WINAPI fwd_auth_QI(IAuthenticate *iface, REFIID riid, void **ppv)
{ return fwd_QI(&CONTAINING_RECORD(iface, FwdBSC, auth)->bsc, riid, ppv); }
static HRESULT WINAPI fwd_info_QI(IInternetBindInfo *iface, REFIID riid, void **ppv)
{ return fwd_QI(&CONTAINING_RECORD(iface, FwdBSC, info)->bsc, riid, ppv); }
static HRESULT WINAPI fwd_Authenticate(IAuthenticate *iface, HWND *hwnd, LPWSTR *user, LPWSTR *pass)
{
    *hwnd = (HWND)(ULONG_PTR)0x1234;
    *user = CoTaskMemAlloc(4 * sizeof(WCHAR)); lstrcpyW(*user, L"usr");
    *pass = CoTaskMemAlloc(4 * sizeof(WCHAR)); lstrcpyW(*pass, L"pwd");
    return S_OK;
}
static HRESULT WINAPI fwd_GetBindInfo(IInternetBindInfo *iface, DWORD *bindf, BINDINFO *bi)
{
    *bindf = 0x12345;
    return S_OK;
}
static IBindStatusCallbackVtbl fwd_bsc_vtbl = { fwd_QI, bsc_AddRef, bsc_Release, bsc_OnStart, bsc_GetPriority,
    bsc_OnLow, bsc_OnProgress, bsc_OnStop, bsc_GetBindInfo, bsc_OnData, bsc_OnObject };
static IAuthenticateVtbl fwd_auth_vtbl = { fwd_auth_QI, (void *)bsc_AddRef, (void *)bsc_Release, fwd_Authenticate };
static IInternetBindInfoVtbl fwd_info_vtbl = { fwd_info_QI, (void *)bsc_AddRef, (void *)bsc_Release, fwd_GetBindInfo, info_GetBindString };

static void test_wrapper(void)
{
    FwdBSC f;
    IBindCtx *bc;
    IUnknown *holder = NULL;
    IAuthenticate *auth = NULL;
    IInternetBindInfo *info = NULL;
    HWND hwnd = NULL;
    LPWSTR user = NULL, pass = NULL;
    DWORD bindf = 0;
    BINDINFO bi;
    HRESULT hr;

    f.bsc.lpVtbl = &fwd_bsc_vtbl;
    f.auth.lpVtbl = &fwd_auth_vtbl;
    f.info.lpVtbl = &fwd_info_vtbl;

    CreateBindCtx(0, &bc);
    hr = RegisterBindStatusCallback(bc, &f.bsc, NULL, 0);
    CHECK(hr == S_OK, "RegisterBindStatusCallback %08lx", hr);
    hr = IBindCtx_GetObjectParam(bc, (LPOLESTR)L"_BSCB_Holder_", &holder);
    CHECK(hr == S_OK && holder, "holder %08lx", hr);
    if (!holder) return;

    hr = IUnknown_QueryInterface(holder, &IID_IAuthenticate, (void **)&auth);
    CHECK(hr == S_OK && auth, "holder IAuthenticate %08lx", hr);
    if (auth)
    {
        hr = IAuthenticate_Authenticate(auth, &hwnd, &user, &pass);
        CHECK(hr == S_OK && hwnd == (HWND)(ULONG_PTR)0x1234 && user && !lstrcmpW(user, L"usr")
              && pass && !lstrcmpW(pass, L"pwd"), "Authenticate forwarded: %08lx %p", hr, hwnd);
        CoTaskMemFree(user);
        CoTaskMemFree(pass);
        IAuthenticate_Release(auth);
    }

    hr = IUnknown_QueryInterface(holder, &IID_IInternetBindInfo, (void **)&info);
    CHECK(hr == S_OK && info, "holder IInternetBindInfo %08lx", hr);
    if (info)
    {
        memset(&bi, 0, sizeof(bi));
        bi.cbSize = sizeof(bi);
        hr = IInternetBindInfo_GetBindInfo(info, &bindf, &bi);
        CHECK(hr == S_OK && bindf == 0x12345, "GetBindInfo forwarded: %08lx %lx", hr, bindf);
        IInternetBindInfo_Release(info);
    }
    IUnknown_Release(holder);
    IBindCtx_Release(bc);
}

/* ---------------------------------------------------------------- URL moniker */
static void test_moniker(void)
{
    IMoniker *mon, *same, *other, *parsed = NULL, *prefix = NULL;
    IBindCtx *bc;
    IRunningObjectTable *rot;
    IUnknown *unk;
    DWORD cookie;
    FILETIME ft;
    LPOLESTR name;
    ULONG eaten = 0;
    HRESULT hr;

    CreateURLMoniker(NULL, L"http://foo.com/pub/list.html", &mon);
    CreateURLMoniker(NULL, L"http://foo.com/pub/list.html", &same);
    CreateURLMoniker(NULL, L"http://foo.com/other.html", &other);
    CreateBindCtx(0, &bc);

    hr = IMoniker_IsRunning(mon, bc, NULL, same);
    CHECK(hr == S_OK, "IsRunning equal newly running %08lx", hr);
    hr = IMoniker_IsRunning(mon, bc, NULL, other);
    CHECK(hr == S_FALSE, "IsRunning other newly running %08lx", hr);
    hr = IMoniker_IsRunning(mon, bc, NULL, NULL);
    CHECK(hr == S_FALSE, "IsRunning not registered %08lx", hr);

    GetRunningObjectTable(0, &rot);
    IMoniker_QueryInterface(mon, &IID_IUnknown, (void **)&unk);
    hr = IRunningObjectTable_Register(rot, 0, unk, mon, &cookie);
    CHECK(SUCCEEDED(hr), "ROT register %08lx", hr);
    hr = IMoniker_IsRunning(mon, bc, NULL, NULL);
    CHECK(hr == S_OK, "IsRunning registered %08lx", hr);
    {
        FILETIME note = { 2, 1 };
        IRunningObjectTable_NoteChangeTime(rot, cookie, &note);
    }
    memset(&ft, 0, sizeof(ft));
    hr = IMoniker_GetTimeOfLastChange(mon, bc, NULL, &ft);
    CHECK(hr == S_OK && ft.dwLowDateTime == 2 && ft.dwHighDateTime == 1, "GetTimeOfLastChange %08lx %lx", hr, ft.dwLowDateTime);
    IRunningObjectTable_Revoke(rot, cookie);
    hr = IMoniker_IsRunning(mon, bc, NULL, NULL);
    CHECK(hr == S_FALSE, "IsRunning revoked %08lx", hr);
    hr = IMoniker_GetTimeOfLastChange(mon, bc, NULL, &ft);
    CHECK(FAILED(hr), "GetTimeOfLastChange not running %08lx", hr);
    IRunningObjectTable_Release(rot);
    IUnknown_Release(unk);

    hr = IMoniker_CommonPrefixWith(mon, other, &prefix);
    CHECK(hr == E_NOTIMPL, "CommonPrefixWith %08lx", hr);
    hr = IMoniker_RelativePathTo(mon, other, &prefix);
    CHECK(hr == E_NOTIMPL, "RelativePathTo %08lx", hr);

    /* relative name against the moniker itself */
    hr = IMoniker_ParseDisplayName(mon, bc, NULL, (LPOLESTR)L"../default.html", &eaten, &parsed);
    CHECK(hr == S_OK && parsed && eaten == 15, "ParseDisplayName relative %08lx %lu", hr, eaten);
    if (parsed)
    {
        name = NULL;
        IMoniker_GetDisplayName(parsed, bc, NULL, &name);
        CHECK(name && !lstrcmpiW(name, L"http://foo.com/default.html"), "parsed -> %ls", name ? name : L"(null)");
        CoTaskMemFree(name);
        IMoniker_Release(parsed);
    }
    /* a full URL stays as it is */
    parsed = NULL;
    hr = IMoniker_ParseDisplayName(mon, bc, NULL, (LPOLESTR)L"http://bar.org/x.html", &eaten, &parsed);
    CHECK(hr == S_OK && parsed, "ParseDisplayName full %08lx", hr);
    if (parsed)
    {
        name = NULL;
        IMoniker_GetDisplayName(parsed, bc, NULL, &name);
        CHECK(name && !lstrcmpiW(name, L"http://bar.org/x.html"), "parsed full -> %ls", name ? name : L"(null)");
        CoTaskMemFree(name);
        IMoniker_Release(parsed);
    }
    IBindCtx_Release(bc);
    IMoniker_Release(other);
    IMoniker_Release(same);
    IMoniker_Release(mon);
}

/* ---------------------------------------------------------------- zone manager reset */
static void test_zones(void)
{
    IInternetZoneManagerEx2 *zm = NULL;
    DWORD policy = 0;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_InternetZoneManager, NULL, CLSCTX_INPROC_SERVER, &IID_IInternetZoneManagerEx2, (void **)&zm);
    CHECK(hr == S_OK, "create zone manager %08lx", hr);
    if (hr != S_OK) return;

    policy = URLPOLICY_DISALLOW;
    hr = IInternetZoneManagerEx2_SetZoneActionPolicy(zm, URLZONE_INTERNET, URLACTION_SCRIPT_RUN, (BYTE *)&policy,
                                                     sizeof(policy), URLZONEREG_HKCU);
    CHECK(hr == S_OK, "SetZoneActionPolicy %08lx", hr);
    policy = 0xdead;
    hr = IInternetZoneManagerEx2_GetZoneActionPolicy(zm, URLZONE_INTERNET, URLACTION_SCRIPT_RUN, (BYTE *)&policy,
                                                     sizeof(policy), URLZONEREG_DEFAULT);
    CHECK(hr == S_OK && policy == URLPOLICY_DISALLOW, "policy set: %08lx %lx", hr, policy);

    hr = IInternetZoneManagerEx2_FixUnsecureSettings(zm);
    CHECK(hr == S_OK, "FixUnsecureSettings %08lx", hr);
    policy = 0xdead;
    hr = IInternetZoneManagerEx2_GetZoneActionPolicy(zm, URLZONE_INTERNET, URLACTION_SCRIPT_RUN, (BYTE *)&policy,
                                                     sizeof(policy), URLZONEREG_DEFAULT);
    CHECK(hr == S_OK && policy != URLPOLICY_DISALLOW, "policy after reset: %08lx %lx", hr, policy);
    IInternetZoneManagerEx2_Release(zm);
}

/* ---------------------------------------------------------------- features, session option, class objects */
static volatile HRESULT thread_view[4];

static DWORD WINAPI feature_thread(void *arg)
{
    thread_view[0] = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD);
    thread_view[1] = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD_INTERNET);
    return 0;
}

static void test_features(void)
{
    HANDLE thread;
    HRESULT hr;

    hr = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD);
    CHECK(hr == S_FALSE, "thread feature off by default %08lx", hr);

    hr = CoInternetSetFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, SET_FEATURE_ON_THREAD, TRUE);
    CHECK(hr == S_OK, "set on thread %08lx", hr);
    hr = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD);
    CHECK(hr == S_OK, "thread feature on %08lx", hr);
    hr = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_PROCESS);
    CHECK(hr == S_FALSE, "process feature untouched %08lx", hr);
    hr = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD_INTERNET);
    CHECK(hr == S_FALSE, "internet zone scope untouched %08lx", hr);

    hr = CoInternetSetFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, SET_FEATURE_ON_THREAD_INTERNET, TRUE);
    CHECK(hr == S_OK, "set on internet zone %08lx", hr);
    hr = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD_INTERNET);
    CHECK(hr == S_OK, "internet zone on %08lx", hr);
    hr = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD_TRUSTED);
    CHECK(hr == S_FALSE, "trusted zone untouched %08lx", hr);

    thread = CreateThread(NULL, 0, feature_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    CHECK(thread_view[0] == S_FALSE && thread_view[1] == S_FALSE, "other thread sees %08lx %08lx",
          thread_view[0], thread_view[1]);

    hr = CoInternetSetFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, SET_FEATURE_ON_THREAD, FALSE);
    hr = CoInternetIsFeatureEnabled(FEATURE_PROTOCOL_LOCKDOWN, GET_FEATURE_FROM_THREAD);
    CHECK(hr == S_FALSE, "thread feature off again %08lx", hr);

    /* the registry value, whatever the process was told */
    CoInternetSetFeatureEnabled(FEATURE_XMLHTTP, SET_FEATURE_ON_PROCESS, FALSE);
    hr = CoInternetIsFeatureEnabled(FEATURE_XMLHTTP, GET_FEATURE_FROM_PROCESS);
    CHECK(hr == S_FALSE, "process override %08lx", hr);
    hr = CoInternetIsFeatureEnabled(FEATURE_XMLHTTP, GET_FEATURE_FROM_REGISTRY);
    CHECK(hr == S_OK, "registry/default value %08lx", hr);
}

static void test_misc(void)
{
    IInternetSession *session;
    IClassFactory *cf;
    char agent[256];
    DWORD size = 0;
    HRESULT hr;
    HKEY key;
    WCHAR clsid_str[64];
    CLSID random_clsid = { 0x5a6b7c8d, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };

    hr = CoInternetGetSession(0, &session, 0);
    hr = IInternetSession_SetSessionOption(session, URLMON_OPTION_USERAGENT, (void *)"SG Test Agent/1.0", 17, 0);
    CHECK(hr == S_OK, "SetSessionOption %08lx", hr);
    memset(agent, 0, sizeof(agent));
    hr = UrlMkGetSessionOption(URLMON_OPTION_USERAGENT, agent, sizeof(agent), &size, 0);
    /* Windows answers E_OUTOFMEMORY when the agent is returned (see the urlmon tests) */
    CHECK(hr == E_OUTOFMEMORY && !strcmp(agent, "SG Test Agent/1.0"), "user agent now '%s' (%08lx)", agent, hr);
    IInternetSession_Release(session);

    cf = NULL;
    hr = CoGetClassObjectFromURL(&CLSID_FileProtocol, NULL, 0, 0, NULL, NULL, CLSCTX_INPROC_SERVER, NULL,
                                 &IID_IClassFactory, (void **)&cf);
    CHECK(hr == S_OK && cf, "CoGetClassObjectFromURL installed class %08lx", hr);
    if (cf) IClassFactory_Release(cf);

    StringFromGUID2(&CLSID_FileProtocol, clsid_str, ARRAY_SIZE(clsid_str));
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\MIME\\Database\\Content Type\\application/x-sgtest", 0, NULL,
                        0, KEY_ALL_ACCESS, NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, L"CLSID", 0, REG_SZ, (BYTE *)clsid_str, (lstrlenW(clsid_str) + 1) * sizeof(WCHAR));
        RegCloseKey(key);
    }
    cf = NULL;
    hr = CoGetClassObjectFromURL(&CLSID_NULL, NULL, 0, 0, L"application/x-sgtest", NULL, CLSCTX_INPROC_SERVER, NULL,
                                 &IID_IClassFactory, (void **)&cf);
    CHECK(hr == S_OK && cf, "CoGetClassObjectFromURL by content type %08lx", hr);
    if (cf) IClassFactory_Release(cf);

    {
        uCLSSPEC spec;

        spec.tyspec = TYSPEC_CLSID;
        spec.tagged_union.clsid = CLSID_FileProtocol;
        hr = FaultInIEFeature(NULL, &spec, NULL, 0);
        CHECK(hr == S_OK, "FaultInIEFeature installed class %08lx", hr);
        spec.tagged_union.clsid = random_clsid;
        hr = FaultInIEFeature(NULL, &spec, NULL, 0);
        CHECK(hr == S_FALSE, "FaultInIEFeature unknown class %08lx", hr);
        spec.tyspec = TYSPEC_MIMETYPE;
        spec.tagged_union.pMimeType = (LPOLESTR)L"application/x-sgtest";
        hr = FaultInIEFeature(NULL, &spec, NULL, FIEF_FLAG_PEEK);
        CHECK(hr == S_OK, "FaultInIEFeature by content type %08lx", hr);
    }

    cf = (void *)1;
    hr = CoGetClassObjectFromURL(&random_clsid, NULL, 0, 0, NULL, NULL, CLSCTX_INPROC_SERVER, NULL,
                                 &IID_IClassFactory, (void **)&cf);
    CHECK(hr == E_NOINTERFACE && !cf, "CoGetClassObjectFromURL unknown class %08lx", hr);
}

/* ---------------------------------------------------------------- HlinkSimpleNavigateToString with a frame */
typedef struct { IHlinkFrame iface; int navigated; DWORD flags; } TestFrame;

static HRESULT WINAPI fr_QI(IHlinkFrame *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IHlinkFrame)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI fr_AddRef(IHlinkFrame *iface) { return 2; }
static ULONG WINAPI fr_Release(IHlinkFrame *iface) { return 1; }
static HRESULT WINAPI fr_SetBC(IHlinkFrame *iface, IHlinkBrowseContext *bc) { return E_NOTIMPL; }
static HRESULT WINAPI fr_GetBC(IHlinkFrame *iface, IHlinkBrowseContext **bc) { return E_NOTIMPL; }
static HRESULT WINAPI fr_Navigate(IHlinkFrame *iface, DWORD flags, LPBC pbc, IBindStatusCallback *pbsc, IHlink *hl)
{
    TestFrame *f = (TestFrame *)iface;
    f->navigated++;
    f->flags = flags;
    return S_OK;
}
static HRESULT WINAPI fr_OnNavigate(IHlinkFrame *iface, DWORD f, IMoniker *m, LPCWSTR l, LPCWSTR n, DWORD r) { return S_OK; }
static HRESULT WINAPI fr_Update(IHlinkFrame *iface, DWORD f, IMoniker *m, LPCWSTR l, LPCWSTR n) { return S_OK; }
static IHlinkFrameVtbl fr_vtbl = { fr_QI, fr_AddRef, fr_Release, fr_SetBC, fr_GetBC, fr_Navigate, fr_OnNavigate, fr_Update };

static void test_hlink(void)
{
    TestFrame frame = { { &fr_vtbl }, 0, 0 };
    HRESULT hr;

    hr = HlinkSimpleNavigateToString(L"http://www.example.com/page.htm", L"top", NULL, (IUnknown *)&frame.iface,
                                     NULL, NULL, HLNF_INTERNALJUMP, 0);
    CHECK(hr == S_OK && frame.navigated == 1 && frame.flags == HLNF_INTERNALJUMP, "frame navigation %08lx %d", hr, frame.navigated);
    hr = HlinkSimpleNavigateToString(NULL, NULL, NULL, NULL, NULL, NULL, 0, 0);
    CHECK(hr == E_INVALIDARG, "no target %08lx", hr);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    GetTempPathW(MAX_PATH, tmpdir);
    lstrcatW(tmpdir, L"sgurlmonb2");
    CreateDirectoryW(tmpdir, NULL);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    test_uri();
    test_binding();
    test_file_protocol();
    test_http_seek();
    test_ftp_error();
    test_status_forwarding();
    test_bindresult_proxy();
    test_wrapper();
    test_moniker();
    test_zones();
    test_features();
    test_misc();
    test_hlink();

    printf("checks %d failures %d\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
