/* Probe for patches/sg/4582: the Windows Media Player control's
 * IOleObject::GetMoniker gives the client site's moniker, and E_UNEXPECTED
 * with a NULL moniker when it has no client site. Table driven; lines
 * "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const GUID clsid_wmp = {0x6bf52a52,0x394a,0x11d3,{0xb1,0x53,0x00,0xc0,0x4f,0x79,0xfa,0xa6}};

static IMoniker *site_moniker;
static DWORD got_assign, got_which;
static HRESULT site_hr;

static HRESULT WINAPI site_qi(IOleClientSite *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IOleClientSite)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI site_addref(IOleClientSite *iface) { return 2; }
static ULONG WINAPI site_release(IOleClientSite *iface) { return 1; }
static HRESULT WINAPI site_save(IOleClientSite *iface) { return E_NOTIMPL; }
static HRESULT WINAPI site_getmoniker(IOleClientSite *iface, DWORD assign, DWORD which, IMoniker **mk)
{
    got_assign = assign; got_which = which;
    *mk = site_moniker;
    if (site_moniker) IMoniker_AddRef(site_moniker);
    return site_hr;
}
static HRESULT WINAPI site_container(IOleClientSite *iface, IOleContainer **c) { *c = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI site_show(IOleClientSite *iface) { return S_OK; }
static HRESULT WINAPI site_onshow(IOleClientSite *iface, BOOL show) { return S_OK; }
static HRESULT WINAPI site_layout(IOleClientSite *iface) { return S_OK; }
static IOleClientSiteVtbl site_vtbl = {site_qi, site_addref, site_release, site_save, site_getmoniker, site_container,
                                       site_show, site_onshow, site_layout};
static IOleClientSite site = {&site_vtbl};

int main(int argc, char **argv)
{
    IOleObject *ole;
    IMoniker *mk;
    HRESULT hr;
    unsigned int i;

    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid_wmp, NULL, CLSCTX_INPROC_SERVER, &IID_IOleObject, (void **)&ole);
    check(hr == S_OK, "player control %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }

    mk = (void *)0xdead;
    hr = IOleObject_GetMoniker(ole, OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_OBJFULL, &mk);
    check(hr == E_UNEXPECTED && !mk, "no client site: %#lx %p", hr, mk);

    CreateItemMoniker(L"!", L"player", &site_moniker);
    hr = IOleObject_SetClientSite(ole, &site);
    check(hr == S_OK, "SetClientSite %#lx", hr);
    {
        static const struct { DWORD assign, which; HRESULT site_hr; int has; } cases[] =
        {
            {OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_OBJFULL, S_OK, 1},
            {OLEGETMONIKER_FORCEASSIGN, OLEWHICHMK_CONTAINER, S_OK, 1},
            {OLEGETMONIKER_TEMPFORUSER, OLEWHICHMK_OBJREL, E_FAIL, 0},
        };
        for (i = 0; i < ARRAYSIZE(cases); i++)
        {
            site_hr = cases[i].site_hr;
            got_assign = got_which = 0xdead;
            mk = (void *)0xdead;
            if (!cases[i].has) { IMoniker *keep = site_moniker; site_moniker = NULL;
                hr = IOleObject_GetMoniker(ole, cases[i].assign, cases[i].which, &mk); site_moniker = keep; }
            else hr = IOleObject_GetMoniker(ole, cases[i].assign, cases[i].which, &mk);
            check(hr == cases[i].site_hr && got_assign == cases[i].assign && got_which == cases[i].which &&
                  mk == (cases[i].has ? site_moniker : NULL), "case %u: the site's answer: %#lx %p", i, hr, mk);
            if (hr == S_OK && mk) IMoniker_Release(mk);
        }
    }
    IOleObject_SetClientSite(ole, NULL);
    mk = (void *)0xdead;
    hr = IOleObject_GetMoniker(ole, OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_OBJFULL, &mk);
    check(hr == E_UNEXPECTED && !mk, "client site removed: %#lx %p", hr, mk);

    IOleObject_Release(ole);
    IMoniker_Release(site_moniker);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
