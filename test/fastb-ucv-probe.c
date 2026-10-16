/* windows.security.credentials.ui.userconsentverifier: RequestVerificationAsync, IInspectable members (patch 3716) */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <initguid.h>

typedef struct HSTRING__ *HSTRING;
typedef struct { void **vt; } OBJ;
#define SLOT(o, n) ((OBJ *)(o))->vt[n]
static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

DEFINE_GUID(IID_Statics, 0xaf4f3f91, 0x564c, 0x4ddc, 0xb8,0xb5, 0x97,0x34,0x47,0x62,0x7c,0x65);
DEFINE_GUID(IID_OpResult, 0xfd596ffd, 0x2318, 0x558f, 0x9d,0xbe, 0xd2,0x1d,0xf4,0x37,0x64,0xa5);
DEFINE_GUID(IID_OpAvail, 0xddd384f3, 0xd818, 0x5d83, 0xab,0x4b, 0x32,0x11,0x9c,0x28,0x58,0x7c);
DEFINE_GUID(IID_HdlResult, 0x0cffc6c9, 0x4c2b, 0x5cd4, 0xb3,0x8c, 0x7b,0x8d,0xf3,0xff,0x5a,0xfb);
DEFINE_GUID(IID_HdlAvail, 0x28988174, 0xace2, 0x5c15, 0xa0,0xdf, 0x58,0x0a,0x26,0xd9,0x42,0x94);
DEFINE_GUID(IID_AsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(IID_ActFactory, 0x00000035, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(IID_Agile, 0x94ea2b94, 0xe9cc, 0x49e0, 0xc0,0xff, 0xee,0x64,0xca,0x8f,0x5b,0x90);

struct handler { void **vt; const GUID *iid; HANDLE event; int status; };
static HRESULT WINAPI h_qi(void *t, const GUID *g, void **o)
{
    struct handler *h = t;
    if (IsEqualGUID(g, &IID_IUnknown) || IsEqualGUID(g, &IID_Agile) || IsEqualGUID(g, h->iid)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI h_ar(void *t) { return 2; }
static HRESULT WINAPI h_invoke(void *t, void *async, int status)
{
    struct handler *h = t; h->status = status; SetEvent(h->event); return S_OK;
}
static void *h_vt[4] = { h_qi, h_ar, h_ar, h_invoke };

typedef HRESULT (WINAPI *get_factory_t)(HSTRING, REFIID, void **);
typedef HRESULT (WINAPI *create_string_t)(const WCHAR *, UINT32, HSTRING *);
typedef const WCHAR *(WINAPI *raw_t)(HSTRING, UINT32 *);

static create_string_t pCreate;
static raw_t pRaw;

static void check_class_name(void *obj, const WCHAR *want, const char *what)
{
    HSTRING s = NULL; HRESULT hr = ((HRESULT (WINAPI *)(void *, HSTRING *))SLOT(obj, 4))(obj, &s);
    CHECK(hr == S_OK, "%s GetRuntimeClassName %#lx", what, hr);
    if (hr == S_OK) { const WCHAR *b = pRaw(s, NULL); CHECK(!wcscmp(b, want), "%s class name %ls", what, b); }
}
static void check_iids(void *obj, const GUID *const *want, ULONG n, const char *what)
{
    ULONG c = 99, i, j; GUID *g = NULL;
    HRESULT hr = ((HRESULT (WINAPI *)(void *, ULONG *, GUID **))SLOT(obj, 3))(obj, &c, &g);
    CHECK(hr == S_OK, "%s GetIids %#lx", what, hr);
    if (hr != S_OK) return;
    CHECK(c == n, "%s iid count %lu", what, c);
    for (i = 0; i < n && i < c; i++)
    {
        BOOL found = FALSE;
        for (j = 0; j < c; j++) if (IsEqualGUID(&g[j], want[i])) found = TRUE;
        CHECK(found, "%s missing iid %lu", what, i);
    }
    for (i = 0; i < c; i++) CHECK(!IsEqualGUID(&g[i], &IID_IUnknown), "%s lists IUnknown", what);
    CoTaskMemFree(g);
    {
        int tl = 99; hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(obj, 5))(obj, &tl);
        CHECK(hr == S_OK && tl == 0, "%s GetTrustLevel %#lx %d", what, hr, tl);
    }
}

int main(void)
{
    HMODULE cb, dll; get_factory_t pGet; HRESULT hr; HSTRING cls, msg; void *fac = NULL, *st = NULL, *op = NULL, *info = NULL, *inst = NULL;
    static const WCHAR name[] = L"Windows.Security.Credentials.UI.UserConsentVerifier";
    struct handler h; int res = -1, st2 = -1; DWORD w;
    setvbuf(stdout, NULL, _IONBF, 0);
    cb = LoadLibraryA("combase.dll");
    pGet = (get_factory_t)GetProcAddress(cb, "RoGetActivationFactory");
    pCreate = (create_string_t)GetProcAddress(cb, "WindowsCreateString");
    pRaw = (raw_t)GetProcAddress(cb, "WindowsGetStringRawBuffer");
    CHECK(pGet && pCreate && pRaw, "combase exports");
    hr = ((HRESULT (WINAPI *)(int))GetProcAddress(cb, "RoInitialize"))(1);
    CHECK(hr == S_OK, "RoInitialize %#lx", hr);
    pCreate(name, wcslen(name), &cls);
    for (int try = 0; try < 8; try++)  /* a fresh prefix may still be registering the classes */
    {
        hr = pGet(cls, &IID_ActFactory, &fac);
        if (hr != (HRESULT)0x80040154) break;
        Sleep(1500);
    }
    CHECK(hr == S_OK && fac, "factory %#lx", hr);
    if (!fac) { printf("RESULT: FAIL\n"); return 1; }

    /* the factory */
    { const GUID *w2[] = { &IID_Statics, &IID_ActFactory }; check_iids(fac, w2, 2, "factory"); }
    check_class_name(fac, name, "factory");
    hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(fac, 6))(fac, &inst);
    CHECK(hr == E_NOTIMPL && !inst, "ActivateInstance %#lx", hr);
    hr = ((HRESULT (WINAPI *)(void *, void *, ULONG *))SLOT(fac, 3))(fac, NULL, NULL);
    CHECK(hr == E_POINTER, "GetIids NULL %#lx", hr);
    hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(fac, 5))(fac, NULL);
    CHECK(hr == E_POINTER, "GetTrustLevel NULL %#lx", hr);

    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(fac, 0))(fac, &IID_Statics, &st);
    CHECK(hr == S_OK && st, "statics %#lx", hr);
    if (!st) { printf("RESULT: FAIL\n"); return 1; }

    /* CheckAvailabilityAsync keeps working and now describes itself */
    hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(st, 6))(st, &op);
    CHECK(hr == S_OK && op, "CheckAvailabilityAsync %#lx", hr);
    if (op)
    {
        const GUID *w3[] = { &IID_OpAvail, &IID_AsyncInfo };
        check_iids(op, w3, 2, "availability op");
        check_class_name(op, L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Credentials.UI.UserConsentVerifierAvailability>", "availability op");
        ((ULONG (WINAPI *)(void *))SLOT(op, 2))(op);
    }
    op = NULL;

    /* RequestVerificationAsync: no provider, so DeviceNotPresent (the state CheckAvailabilityAsync reports) */
    pCreate(L"Verify me", 9, &msg);
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, void **))SLOT(st, 7))(st, msg, &op);
    CHECK(hr == S_OK && op, "RequestVerificationAsync %#lx", hr);
    if (op)
    {
        const GUID *w3[] = { &IID_OpResult, &IID_AsyncInfo };
        h.vt = h_vt; h.iid = &IID_HdlResult; h.event = CreateEventW(NULL, FALSE, FALSE, NULL); h.status = -1;
        check_iids(op, w3, 2, "result op");
        check_class_name(op, L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Credentials.UI.UserConsentVerificationResult>", "result op");
        hr = ((HRESULT (WINAPI *)(void *, void *))SLOT(op, 6))(op, &h);
        CHECK(hr == S_OK, "put_Completed %#lx", hr);
        w = WaitForSingleObject(h.event, 3000);
        CHECK(w == WAIT_OBJECT_0, "handler wait %lu", w);
        CHECK(h.status == 1, "handler status %d", h.status);
        hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(op, 8))(op, &res);
        CHECK(hr == S_OK && res == 1, "GetResults %#lx result %d (1 is DeviceNotPresent)", hr, res);
        hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(op, 0))(op, &IID_AsyncInfo, &info);
        CHECK(hr == S_OK && info, "IAsyncInfo %#lx", hr);
        if (info)
        {
            hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(info, 7))(info, &st2);
            CHECK(hr == S_OK && st2 == 1, "status %#lx %d", hr, st2);
            ((ULONG (WINAPI *)(void *))SLOT(info, 2))(info);
        }
        /* a second handler is refused as for every async operation */
        hr = ((HRESULT (WINAPI *)(void *, void *))SLOT(op, 6))(op, &h);
        CHECK(hr == (HRESULT)0x80000018, "second put_Completed %#lx", hr);
        CloseHandle(h.event);
        ((ULONG (WINAPI *)(void *))SLOT(op, 2))(op);
    }
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, void **))SLOT(st, 7))(st, NULL, &op);
    CHECK(hr == S_OK, "RequestVerificationAsync NULL message %#lx", hr);
    if (op) ((ULONG (WINAPI *)(void *))SLOT(op, 2))(op);
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, void **))SLOT(st, 7))(st, msg, NULL);
    CHECK(hr == E_POINTER, "RequestVerificationAsync NULL out %#lx", hr);

    /* the COM entry point of the DLL */
    dll = LoadLibraryA("windows.security.credentials.ui.userconsentverifier.dll");
    CHECK(dll != NULL, "dll");
    if (dll)
    {
        typedef HRESULT (WINAPI *gco_t)(REFCLSID, REFIID, void **);
        gco_t gco = (gco_t)GetProcAddress(dll, "DllGetClassObject");
        void *p = (void *)1;
        CHECK(gco != NULL, "DllGetClassObject export");
        if (gco) { hr = gco(&IID_Statics, &IID_IUnknown, &p); CHECK(hr == CLASS_E_CLASSNOTAVAILABLE && !p, "DllGetClassObject %#lx %p", hr, p); }
    }
    ((ULONG (WINAPI *)(void *))SLOT(st, 2))(st);
    ((ULONG (WINAPI *)(void *))SLOT(fac, 2))(fac);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
