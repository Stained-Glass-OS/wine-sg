/* actfilter-gate.sh's probe (0493): a process's activation filter
 * (CoRegisterActivationFilter). Prints one line per step: "NAME HR". */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <msxml6.h>
#include <stdio.h>

typedef struct filter filter;
struct filter_vtbl
{
    HRESULT (WINAPI *QueryInterface)(filter *, REFIID, void **);
    ULONG (WINAPI *AddRef)(filter *);
    ULONG (WINAPI *Release)(filter *);
    HRESULT (WINAPI *HandleActivation)(filter *, DWORD, REFCLSID, CLSID *);
};
struct filter { const struct filter_vtbl *lpVtbl; };
DEFINE_GUID(IID_IActivationFilter, 0x00000017, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
HRESULT WINAPI CoRegisterActivationFilter(filter *);

static int calls, inner;

static HRESULT WINAPI f_qi(filter *f, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IActivationFilter)) { *out = f; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_addref(filter *f) { return 2; }
static ULONG WINAPI f_release(filter *f) { return 1; }
static HRESULT WINAPI f_handle(filter *f, DWORD type, REFCLSID clsid, CLSID *out)
{
    calls++;
    if (IsEqualGUID(clsid, &CLSID_ShellLink)) { *out = CLSID_DOMDocument60; return S_OK; }
    if (IsEqualGUID(clsid, &CLSID_DOMDocument)) return E_ACCESSDENIED;
    if (IsEqualGUID(clsid, &CLSID_FreeThreadedDOMDocument60))
    {
        /* an activation of its own is not filtered again */
        IUnknown *unk;
        int before = calls;
        if (SUCCEEDED(CoCreateInstance(&CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk)))
            IUnknown_Release(unk);
        inner = calls - before;
    }
    *out = GUID_NULL;   /* no class named: the one asked for */
    return S_OK;
}
static const struct filter_vtbl vtbl = { f_qi, f_addref, f_release, f_handle };
static filter the_filter = { &vtbl }, other_filter = { &vtbl };

int main(void)
{
    IUnknown *unk;
    HRESULT hr;

    CoInitialize(NULL);
    printf("register %08lx\n", CoRegisterActivationFilter(&the_filter));
    printf("register-again %08lx\n", CoRegisterActivationFilter(&other_filter));
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void **)&unk);
    printf("replaced %08lx\n", hr);
    if (SUCCEEDED(hr)) IUnknown_Release(unk);
    printf("refused %08lx\n", CoCreateInstance(&CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk));
    hr = CoCreateInstance(&CLSID_FreeThreadedDOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk);
    printf("unchanged %08lx\n", hr);
    if (SUCCEEDED(hr)) IUnknown_Release(unk);
    printf("inner %d\n", inner);
    printf("calls %d\n", calls);
    return 0;
}
