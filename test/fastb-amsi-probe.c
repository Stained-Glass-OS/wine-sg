/* amsi: providers registered under HKLM\Software\Microsoft\AMSI\Providers see the scanned content through IAmsiStream (patch 3723) */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef HRESULT (WINAPI *init_t)(const WCHAR *, void **);
typedef void (WINAPI *uninit_t)(void *);
typedef HRESULT (WINAPI *open_t)(void *, void **);
typedef void (WINAPI *close_t)(void *, void *);
typedef HRESULT (WINAPI *scanbuf_t)(void *, void *, ULONG, const WCHAR *, void *, int *);
typedef HRESULT (WINAPI *scanstr_t)(void *, const WCHAR *, const WCHAR *, void *, int *);
static init_t pInit; static uninit_t pUninit; static open_t pOpen; static close_t pClose; static scanbuf_t pScanBuf; static scanstr_t pScanStr;

static const GUID IID_Provider = {0xb2cabfe3, 0xfe04, 0x42b1, {0xa5, 0xdf, 0x08, 0xd4, 0x83, 0xd4, 0xd1, 0x25}};
static const GUID CLSID_A = {0x6f3a0001, 0x1111, 0x4444, {0x88, 0x88, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
static const GUID CLSID_B = {0x6f3a0002, 0x1111, 0x4444, {0x88, 0x88, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};

struct prov { void **vt; LONG ref; int blocked; /* B: always 0x4000 */ };
struct seen { WCHAR app[64], name[64]; ULONGLONG size; const BYTE *addr; void *session; int calls, named; BYTE content[64]; ULONG content_len; int small_buf_hr_ok; } seen;
static LONG closes[4]; static int nclose;
static LONG live;

typedef struct { void **vt; } OBJ;
#define SLOT(o, n) ((OBJ *)(o))->vt[n]

static HRESULT WINAPI p_qi(struct prov *p, REFIID iid, void **o)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_Provider)) { *o = p; InterlockedIncrement(&p->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI p_addref(struct prov *p) { return InterlockedIncrement(&p->ref); }
static ULONG WINAPI p_release(struct prov *p) { return InterlockedDecrement(&p->ref); }
static HRESULT WINAPI p_scan(struct prov *p, void *stream, int *result)
{
    ULONG got = 0; BYTE tiny[2];
    HRESULT (WINAPI *get)(void *, int, ULONG, BYTE *, ULONG *) = SLOT(stream, 3);
    HRESULT (WINAPI *read)(void *, ULONGLONG, ULONG, BYTE *, ULONG *) = SLOT(stream, 4);
    HRESULT hr;

    if (p->blocked) { *result = 0x4000; return S_OK; }
    seen.calls++;
    memset(seen.app, 0, sizeof(seen.app)); memset(seen.name, 0, sizeof(seen.name));
    get(stream, 0, sizeof(seen.app), (BYTE *)seen.app, &got);
    seen.named = SUCCEEDED(get(stream, 1, sizeof(seen.name), (BYTE *)seen.name, &got));
    get(stream, 2, sizeof(seen.size), (BYTE *)&seen.size, &got);
    get(stream, 3, sizeof(seen.addr), (BYTE *)&seen.addr, &got);
    seen.session = (void *)1;
    get(stream, 4, sizeof(seen.session), (BYTE *)&seen.session, &got);
    hr = get(stream, 2, 2, tiny, &got);
    seen.small_buf_hr_ok = hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && got == 8;
    memset(seen.content, 0, sizeof(seen.content));
    read(stream, 0, sizeof(seen.content) - 1, seen.content, &seen.content_len);
    *result = strstr((char *)seen.content, "EICAR") ? 0x8000 : 0;
    return S_OK;
}
static void WINAPI p_close(struct prov *p, ULONGLONG id) { if (!p->blocked && nclose < 4) closes[nclose++] = (LONG)id; }
static HRESULT WINAPI p_name(struct prov *p, WCHAR **n) { *n = NULL; return E_NOTIMPL; }
static void *prov_vt[] = { p_qi, p_addref, p_release, p_scan, p_close, p_name };

struct factory { IClassFactory iface; int blocked; };
static HRESULT WINAPI f_qi(IClassFactory *i, REFIID iid, void **o) { if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IClassFactory)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI f_ar(IClassFactory *i) { return 2; }
static HRESULT WINAPI f_create(IClassFactory *i, IUnknown *outer, REFIID iid, void **o)
{
    struct prov *p = calloc(1, sizeof(*p));
    p->vt = prov_vt; p->ref = 1; p->blocked = ((struct factory *)i)->blocked;
    InterlockedIncrement(&live);
    *o = p;
    return S_OK;
}
static HRESULT WINAPI f_lock(IClassFactory *i, BOOL l) { return S_OK; }
static IClassFactoryVtbl fvt = { f_qi, f_ar, f_ar, f_create, f_lock };

static void set_key(const GUID *g, BOOL on)
{
    WCHAR name[128], buf[64];
    HKEY k;
    StringFromGUID2(g, buf, 64);
    swprintf(name, 128, L"Software\\Microsoft\\AMSI\\Providers\\%ls", buf);
    if (on) { RegCreateKeyExW(HKEY_LOCAL_MACHINE, name, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL); RegCloseKey(k); }
    else RegDeleteKeyW(HKEY_LOCAL_MACHINE, name);
}

int main(void)
{
    HMODULE m = LoadLibraryA("amsi.dll");
    void *ctx = NULL, *ctx2 = NULL, *s1 = NULL, *s2 = NULL;
    int res = -1; HRESULT hr; DWORD cookie_a, cookie_b;
    struct factory fa = { { &fvt }, 0 }, fb = { { &fvt }, 1 };
    char dirty[] = "xx EICAR yy", clean[] = "nothing here";
    WCHAR wdirty[] = L"say EICAR now";

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    pInit = (void *)GetProcAddress(m, "AmsiInitialize"); pUninit = (void *)GetProcAddress(m, "AmsiUninitialize");
    pOpen = (void *)GetProcAddress(m, "AmsiOpenSession"); pClose = (void *)GetProcAddress(m, "AmsiCloseSession");
    pScanBuf = (void *)GetProcAddress(m, "AmsiScanBuffer"); pScanStr = (void *)GetProcAddress(m, "AmsiScanString");

    hr = pInit(NULL, &ctx); CHECK(hr == E_INVALIDARG, "NULL app name %#lx", hr);
    hr = pInit(L"fastb app", NULL); CHECK(hr == E_INVALIDARG, "NULL context %#lx", hr);

    /* no providers registered */
    set_key(&CLSID_A, FALSE); set_key(&CLSID_B, FALSE);
    hr = pInit(L"fastb app", &ctx); CHECK(hr == S_OK && ctx, "Initialize %#lx", hr);
    hr = pScanBuf(ctx, dirty, sizeof(dirty), L"file.txt", NULL, &res);
    CHECK(hr == S_OK && res == 1, "no provider: %#lx result %d", hr, res);
    hr = pOpen(ctx, &s1); CHECK(hr == S_OK && s1, "OpenSession %#lx", hr);
    pClose(ctx, s1);
    pUninit(ctx);

    /* argument checks */
    hr = pInit(L"fastb app", &ctx);
    res = 77; hr = pScanBuf(ctx, dirty, 0, NULL, NULL, &res); CHECK(hr == E_INVALIDARG, "zero length %#lx", hr);
    hr = pScanBuf(ctx, NULL, 4, NULL, NULL, &res); CHECK(hr == E_INVALIDARG, "NULL buffer %#lx", hr);
    hr = pScanBuf(ctx, dirty, 4, NULL, NULL, NULL); CHECK(hr == E_INVALIDARG, "NULL result %#lx", hr);
    hr = pScanBuf((void *)0x1234, dirty, 4, NULL, NULL, &res); CHECK(hr == E_INVALIDARG, "bad context %#lx", hr);
    hr = pScanStr(ctx, NULL, NULL, NULL, &res); CHECK(hr == E_INVALIDARG, "NULL string %#lx", hr);
    hr = pScanStr(ctx, L"", NULL, NULL, &res); CHECK(hr == E_INVALIDARG, "empty string %#lx", hr);
    hr = pOpen(ctx, NULL); CHECK(hr == E_INVALIDARG, "NULL session out %#lx", hr);
    hr = pOpen((void *)0x1234, &s1); CHECK(hr == E_INVALIDARG, "OpenSession bad context %#lx", hr);
    pUninit(ctx);

    /* providers */
    set_key(&CLSID_A, TRUE);
    CoRegisterClassObject(&CLSID_A, (IUnknown *)&fa.iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie_a);
    hr = pInit(L"fastb app", &ctx); CHECK(hr == S_OK, "Initialize with a provider %#lx", hr);
    CHECK(live == 1, "the provider was created by AmsiInitialize (%ld)", live);
    memset(&seen, 0, sizeof(seen));
    hr = pScanBuf(ctx, clean, sizeof(clean), L"clean.txt", NULL, &res);
    CHECK(hr == S_OK && res == 0, "clean buffer: %#lx result %d", hr, res);
    CHECK(seen.calls == 1 && !wcscmp(seen.app, L"fastb app") && !wcscmp(seen.name, L"clean.txt"), "attributes: calls %d app '%ls' name '%ls'", seen.calls, seen.app, seen.name);
    CHECK(seen.size == sizeof(clean) && seen.addr == (BYTE *)clean && seen.session == NULL, "size %llu addr %p/%p session %p", seen.size, seen.addr, clean, seen.session);
    CHECK(seen.small_buf_hr_ok, "a too small attribute buffer gives ERROR_INSUFFICIENT_BUFFER and the size");
    CHECK(seen.content_len == sizeof(clean), "Read gave %lu bytes", seen.content_len);
    CHECK(!memcmp(seen.content, clean, sizeof(clean)), "Read content");
    hr = pScanBuf(ctx, dirty, sizeof(dirty), NULL, NULL, &res);
    CHECK(hr == S_OK && res == 0x8000, "detected: %#lx result %#x", hr, res);
    CHECK(!seen.named, "no content name given: attribute not available");
    hr = pScanStr(ctx, wdirty, L"script", NULL, &res);
    CHECK(hr == S_OK && res == 0 && seen.size == wcslen(wdirty) * 2, "scan string (UTF-16 bytes, no ASCII EICAR): %#lx %#x size %llu", hr, res, seen.size);
    CHECK(seen.content[0] == 's' && seen.content[1] == 0 && seen.content_len == wcslen(wdirty) * 2, "the string is scanned as its UTF-16 bytes (%lu)", seen.content_len);
    hr = pScanStr(ctx, L"E", L"script2", NULL, &res); CHECK(hr == S_OK && !wcscmp(seen.name, L"script2"), "scan string passes the name");

    /* sessions */
    hr = pOpen(ctx, &s1); hr |= pOpen(ctx, &s2); CHECK(hr == S_OK && s1 != s2, "two sessions");
    pScanBuf(ctx, clean, sizeof(clean), L"a", s1, &res);
    CHECK(seen.session == s1, "the session handle reaches the provider: %p vs %p", seen.session, s1);
    nclose = 0;
    pClose(ctx, s1); pClose(ctx, s2);
    CHECK(nclose == 2 && closes[0] != closes[1] && closes[0] > 0, "providers told of both sessions (%d: %ld %ld)", nclose, closes[0], closes[1]);

    /* second provider: the worst answer wins */
    set_key(&CLSID_B, TRUE);
    CoRegisterClassObject(&CLSID_B, (IUnknown *)&fb.iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie_b);
    pUninit(ctx);
    CHECK(live == 1 || live == 2, "live providers after Uninitialize: %ld", live);
    hr = pInit(L"fastb app", &ctx2);
    hr = pScanBuf(ctx2, clean, sizeof(clean), NULL, NULL, &res);
    CHECK(hr == S_OK && res == 0x4000, "blocked-by-admin beats clean: %#x", res);
    hr = pScanBuf(ctx2, dirty, sizeof(dirty), NULL, NULL, &res);
    CHECK(hr == S_OK && res == 0x8000, "detected beats blocked: %#x", res);
    pUninit(ctx2);

    CoRevokeClassObject(cookie_a); CoRevokeClassObject(cookie_b);
    set_key(&CLSID_A, FALSE); set_key(&CLSID_B, FALSE);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
