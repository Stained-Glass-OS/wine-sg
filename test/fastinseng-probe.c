#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* IInstallEngine2 declared by hand: only the methods used here */
typedef struct IE IE;
typedef struct IEVtbl
{
    HRESULT (WINAPI *QueryInterface)(IE *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IE *);
    ULONG (WINAPI *Release)(IE *);
    HRESULT (WINAPI *GetEngineStatus)(IE *, DWORD *);
    HRESULT (WINAPI *SetCifFile)(IE *, const char *, const char *);
    HRESULT (WINAPI *DownloadComponents)(IE *, DWORD);
    HRESULT (WINAPI *InstallComponents)(IE *, DWORD);
    HRESULT (WINAPI *EnumInstallIDs)(IE *, UINT, char **);
    HRESULT (WINAPI *EnumDownloadIDs)(IE *, UINT, char **);
    HRESULT (WINAPI *IsComponentInstalled)(IE *, const char *, DWORD *);
    HRESULT (WINAPI *RegisterCallback)(IE *, IUnknown *);
    HRESULT (WINAPI *UnregisterCallback)(IE *);
    HRESULT (WINAPI *SetAction)(IE *, const char *, DWORD, DWORD);
    HRESULT (WINAPI *GetSizes)(IE *, const char *, void *);
    HRESULT (WINAPI *LaunchExtraCommand)(IE *, const char *, const char *);
    HRESULT (WINAPI *GetDisplayName)(IE *, const char *, const char *);
    HRESULT (WINAPI *SetBaseUrl)(IE *, const char *);
    HRESULT (WINAPI *SetDownloadDir)(IE *, const char *);
    HRESULT (WINAPI *SetInstallDrive)(IE *, char);
    HRESULT (WINAPI *SetInstallOptions)(IE *, DWORD);
    HRESULT (WINAPI *SetHWND)(IE *, HWND);
    HRESULT (WINAPI *SetIStream)(IE *, IStream *);
    HRESULT (WINAPI *Abort)(IE *, DWORD);
    HRESULT (WINAPI *Suspend)(IE *);
    HRESULT (WINAPI *Resume)(IE *);
    HRESULT (WINAPI *SetLocalCif)(IE *, const char *);
} IEVtbl;
struct IE { const IEVtbl *lpVtbl; };

static LONG cb_ref = 1;
static HRESULT WINAPI cb_qi(IUnknown *i, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI cb_ar(IUnknown *i) { return InterlockedIncrement(&cb_ref); }
static ULONG WINAPI cb_rl(IUnknown *i) { return InterlockedDecrement(&cb_ref); }
static IUnknownVtbl cb_vtbl = { cb_qi, cb_ar, cb_rl };
static IUnknown cb = { &cb_vtbl };

int main(void)
{
    static const GUID clsid = {0x6e449686,0xc509,0x11cf,{0xaa,0xfa,0x00,0xaa,0x00,0xb6,0x01,0x5c}};
    static const GUID iid2 = {0x6e449689,0xc509,0x11cf,{0xaa,0xfa,0x00,0xaa,0x00,0xb6,0x01,0x5c}};
    IE *e;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &iid2, (void **)&e);
    CHECK(hr == S_OK, "create %08lx", hr);
    if (hr == S_OK)
    {
        CHECK(e->lpVtbl->SetBaseUrl(e, "http://example.invalid/") == S_OK, "base url");
        CHECK(e->lpVtbl->SetBaseUrl(e, NULL) == E_INVALIDARG, "null url");
        CHECK(e->lpVtbl->SetDownloadDir(e, "C:\\dl") == S_OK, "download dir");
        CHECK(e->lpVtbl->SetInstallDrive(e, 'D') == S_OK, "drive");
        CHECK(e->lpVtbl->SetInstallDrive(e, '1') == E_INVALIDARG, "bad drive");
        CHECK(e->lpVtbl->SetInstallOptions(e, 3) == S_OK, "options");
        CHECK(e->lpVtbl->SetHWND(e, NULL) == S_OK, "hwnd");
        CHECK(e->lpVtbl->SetLocalCif(e, "x.cif") == S_OK, "local cif");
        CHECK(e->lpVtbl->Suspend(e) == S_OK && e->lpVtbl->Resume(e) == S_OK, "suspend/resume");
        hr = e->lpVtbl->RegisterCallback(e, &cb);
        CHECK(hr == S_OK && cb_ref == 2, "register %08lx ref %ld", hr, cb_ref);
        hr = e->lpVtbl->UnregisterCallback(e);
        CHECK(hr == S_OK && cb_ref == 1, "unregister ref %ld", cb_ref);
        e->lpVtbl->Release(e);
    }
    CoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
