/* dcomp: DCompositionCreateDevice2/3, IDCompositionDevice2/DesktopDevice, surface factory (patch 3615) */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <initguid.h>
#include <dxgiformat.h>
typedef struct { void **vt; } OBJ;
static HRESULT WINAPI dqi(void *t, const GUID *g, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI dar(void *t) { return 1; }
static void *dvt[3] = { dqi, dar, dar };
static OBJ dobj = { dvt };
static void *dc_dummy_unknown(void) { return &dobj; }
#define SLOT(o, n) ((OBJ *)(o))->vt[n]
#define CALL0(o, n, rt) ((rt (WINAPI *)(void *))SLOT(o, n))(o)
#define CALL1(o, n, a) ((HRESULT (WINAPI *)(void *, void *))SLOT(o, n))(o, (void *)(ULONG_PTR)(a))

static int f;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); f++; } } while (0)

DEFINE_GUID(IID_Dev2, 0x75f6468d,0x1b8e,0x447c,0x9b,0xc6,0x75,0xfe,0xa8,0x0b,0x5b,0x25);
DEFINE_GUID(IID_DeskDev, 0x5f4633fe,0x1e08,0x4cb8,0x8c,0x75,0xce,0x24,0x33,0x3f,0x56,0x02);
DEFINE_GUID(IID_Factory, 0xe334bc12,0x3937,0x4e02,0x85,0xeb,0xfc,0xf4,0xeb,0x30,0xd2,0xc8);
DEFINE_GUID(IID_Dev1, 0xc37ea93a,0xe7aa,0x450d,0xb1,0x6f,0x97,0x46,0xcb,0x04,0x07,0xf3);

typedef HRESULT (WINAPI *create_t)(IUnknown *, REFIID, void **);

int main(void)
{
    HMODULE dc = LoadLibraryA("dcomp.dll");
    create_t c2, c3;
    IUnknown *dev = NULL, *x = NULL, *vis = NULL;
    void *dd = NULL, *d2 = NULL, *fac = NULL;
    HRESULT hr;
    ULONG rc;
    HWND hwnd;
    void *tgt = NULL, *anim = NULL;

    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(dc);
    c2 = (create_t)GetProcAddress(dc, "DCompositionCreateDevice2");
    c3 = (create_t)GetProcAddress(dc, "DCompositionCreateDevice3");
    CHECK(c2 && c3);

    /* Device2 with no rendering device */
    hr = c2(NULL, &IID_Dev2, (void **)&d2);
    CHECK(hr == S_OK && d2);
    hr = c2(NULL, &IID_DeskDev, (void **)&dd);
    CHECK(hr == S_OK && dd);
    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(dd, 0))(dd, &IID_Dev2, (void **)&x);
    CHECK(hr == S_OK);
    if (x) IUnknown_Release(x);
    x = NULL;
    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(dd, 0))(dd, &IID_IUnknown, (void **)&x);
    CHECK(hr == S_OK);
    if (x) IUnknown_Release(x);
    hr = c2(NULL, &IID_IUnknown, (void **)&x);
    CHECK(hr == S_OK && x);
    if (x) IUnknown_Release(x);
    hr = c3(NULL, &IID_DeskDev, (void **)&dev);
    CHECK(hr == S_OK && dev);
    if (dev) IUnknown_Release(dev);
    hr = c2(NULL, &IID_IUnknown, NULL);
    CHECK(hr == E_INVALIDARG);

    if (dd)
    {
        hr = CALL1(dd, 6, &vis);
        CHECK(hr == S_OK && vis);
        if (vis) IUnknown_Release(vis);
        hr = CALL1(dd, 23, &anim);
        CHECK(hr == S_OK && anim);
        if (anim) CALL0(anim, 2, ULONG);
        hr = ((HRESULT (WINAPI *)(void *, void *, void **))SLOT(dd, 7))(dd, NULL, &fac);
        CHECK(hr == S_OK && fac);
        if (fac)
        {
            void *s = NULL;
            typedef HRESULT (WINAPI *cs_t)(void *, UINT, UINT, DXGI_FORMAT, int, void **);
            hr = ((cs_t)SLOT(fac, 3))(fac, 0, 16, DXGI_FORMAT_B8G8R8A8_UNORM, 1, &s);
            CHECK(hr == E_INVALIDARG);
            hr = ((cs_t)SLOT(fac, 3))(fac, 16, 16, DXGI_FORMAT_B8G8R8A8_UNORM, 1, &s);
            CHECK(hr == S_OK && s);
            if (s) CALL0(s, 2, ULONG);
            rc = CALL0(fac, 2, ULONG);
            CHECK(rc == 0);
        }
        hwnd = CreateWindowA("static", "t", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
        hr = ((HRESULT (WINAPI *)(void *, HWND, BOOL, void **))SLOT(dd, 24))(dd, hwnd, TRUE, &tgt);
        CHECK(hr == S_OK && tgt);
        if (tgt) CALL0(tgt, 2, ULONG);
        DestroyWindow(hwnd);
        hr = CALL0(dd, 3, HRESULT);
        CHECK(hr == S_OK);
        rc = CALL0(dd, 2, ULONG);
        CHECK(rc == 0);
    }
    if (d2)
    {
        rc = CALL0(d2, 2, ULONG);
        CHECK(rc == 0);
    }

    {
        HMODULE cb = LoadLibraryA("combase.dll");
        HRESULT (WINAPI *create_str)(const WCHAR *, UINT32, void **) = (void *)GetProcAddress(cb, "WindowsCreateString");
        HRESULT (WINAPI *get_factory)(void *, REFIID, void **) = (void *)GetProcAddress(cb, "RoGetActivationFactory");
        const WCHAR *WINAPI (*get_buf)(void *, UINT32 *) = (void *)GetProcAddress(cb, "WindowsGetStringRawBuffer");
        void *cf2 = NULL, *name = NULL, *factory = NULL, *cname = NULL, *comp = NULL;
        ULONG count = 0;
        GUID *iids = NULL;
        UINT32 len;
        static const GUID IID_ActFac = {0x00000035,0,0,{0xc0,0,0,0,0,0,0,0x46}};

        ((HRESULT (WINAPI *)(int))GetProcAddress(cb, "RoInitialize"))(0);
        create_str(L"Windows.UI.Composition.Compositor", 33, &name);
        hr = get_factory(name, &IID_ActFac, &factory);
        CHECK(hr == S_OK && factory);
        if (factory)
        {
            hr = ((HRESULT (WINAPI *)(void *, ULONG *, GUID **))SLOT(factory, 3))(factory, &count, &iids);
            CHECK(hr == S_OK && count == 1 && iids && !memcmp(&iids[0], &IID_ActFac, sizeof(GUID)));
            CoTaskMemFree(iids);
            hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(factory, 4))(factory, &cname);
            CHECK(hr == S_OK && cname);
            if (cname)
            {
                const WCHAR *b = get_buf(cname, &len);
                CHECK(len == 33 && !wcscmp(b, L"Windows.UI.Composition.Compositor"));
            }
            /* a compositor object has more than one runtime interface */
            hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(factory, 6))(factory, &comp);
            if (hr == S_OK && comp)
            {
                count = 0; iids = NULL;
                hr = ((HRESULT (WINAPI *)(void *, ULONG *, GUID **))SLOT(comp, 3))(comp, &count, &iids);
                CHECK(hr == S_OK && count >= 2 && iids);
                CoTaskMemFree(iids);
                CALL0(comp, 2, ULONG);
            }
            else printf("note: ActivateInstance %#lx\n", hr);

        {
            static const GUID IID_Ctl = {0x2d75f35a,0x70a7,0x4395,{0xba,0x2d,0xce,0xf0,0xb1,0x83,0x99,0xf9}};
            static const GUID IID_Info = {0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
            void *cf = NULL, *ctlobj = NULL, *ctl = NULL, *act = NULL, *info = NULL;
            int st = -1;

            create_str(L"Windows.UI.Composition.Core.CompositorController", 48, &cname);
            hr = get_factory(cname, &IID_ActFac, &cf);
            CHECK(hr == S_OK && cf);
            if (cf && SUCCEEDED(((HRESULT (WINAPI *)(void *, void **))SLOT(cf, 6))(cf, &ctlobj)) && ctlobj)
            {
                hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(ctlobj, 0))(ctlobj, &IID_Ctl, &ctl);
                CHECK(hr == S_OK && ctl);
                if (ctl)
                {
                    hr = CALL1(ctl, 8, &act);
                    CHECK(hr == S_OK && act);
                    if (act)
                    {
                        hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(act, 0))(act, &IID_Info, &info);
                        CHECK(hr == S_OK && info);
                        if (info)
                        {
                            hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(info, 7))(info, &st);
                            CHECK(hr == S_OK && st == 1);
                            CALL0(info, 2, ULONG);
                        }
                        CALL0(act, 2, ULONG);
                    }
                    CALL0(ctl, 2, ULONG);
                }
                CALL0(ctlobj, 2, ULONG);
            }
            if (cf) CALL0(cf, 2, ULONG);
        }

        {
            static const GUID IID_CInterop = {0x25297d5c,0x3ad4,0x4c9c,{0xb5,0xcf,0xe3,0x6a,0x38,0x51,0x23,0x30}};
            static const GUID IID_GDInterop = {0xa116ff71,0xf8bf,0x4c8a,{0x9c,0x98,0x70,0x77,0x9a,0x32,0xa9,0xc8}};
            void *cobj = NULL, *cinterop = NULL, *gdev = NULL, *gi = NULL, *got = NULL, *got2 = NULL;
            ID3D11Device *d3d = NULL, *d3d2 = NULL;
            ID2D1Factory1 *f1 = NULL;
            IDXGIDevice *dx = NULL, *dx2 = NULL;
            ID2D1Device *d2a = NULL, *d2b = NULL;
            static const D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

            create_str(L"Windows.UI.Composition.Compositor", 33, &name);
            if (SUCCEEDED(get_factory(name, &IID_ActFac, &cf2)) && cf2
                    && SUCCEEDED(((HRESULT (WINAPI *)(void *, void **))SLOT(cf2, 6))(cf2, &cobj))
                    && SUCCEEDED(((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(cobj, 0))(cobj, &IID_CInterop, &cinterop))
                    && SUCCEEDED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                            lv, 2, D3D11_SDK_VERSION, &d3d, NULL, NULL))
                    && SUCCEEDED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                            lv, 2, D3D11_SDK_VERSION, &d3d2, NULL, NULL)))
            {
                ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dx);
                ID3D11Device_QueryInterface(d3d2, &IID_IDXGIDevice, (void **)&dx2);
                D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &IID_ID2D1Factory1, NULL, (void **)&f1);
                ID2D1Factory1_CreateDevice(f1, dx, &d2a);
                ID2D1Factory1_CreateDevice(f1, dx2, &d2b);
                hr = ((HRESULT (WINAPI *)(void *, void *, void **))SLOT(cinterop, 5))(cinterop, d2a, &gdev);
                CHECK(hr == S_OK && gdev); if (hr != S_OK) printf("hr=%lx\n", hr);
                if (gdev)
                {
                    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(gdev, 0))(gdev, &IID_GDInterop, &gi);
                    CHECK(hr == S_OK && gi);
                    hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(gi, 3))(gi, &got);
                    CHECK(hr == S_OK && got);
                    if (got)
                    {
                        void *u = NULL;
                        ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(got, 0))(got, &IID_IUnknown, &u);
                        CHECK(u == (void *)(IUnknown *)d2a);
                        CALL0(got, 2, ULONG); if (u) CALL0(u, 2, ULONG);
                    }
                    hr = CALL1(gi, 4, NULL);
                    CHECK(hr == E_INVALIDARG);
                    hr = CALL1(gi, 4, d2b);
                    CHECK(hr == S_OK);
                    hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(gi, 3))(gi, &got2);
                    CHECK(hr == S_OK && got2);
                    if (got2)
                    {
                        void *u = NULL;
                        ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(got2, 0))(got2, &IID_IUnknown, &u);
                        CHECK(u == (void *)(IUnknown *)d2b);
                        CALL0(got2, 2, ULONG); if (u) CALL0(u, 2, ULONG);
                    }
                    hr = CALL1(gi, 4, dc_dummy_unknown());
                    CHECK(hr == E_INVALIDARG);
                    CALL0(gi, 2, ULONG);
                    CALL0(gdev, 2, ULONG);
                }
            }
            else printf("note: no d3d11 for the SetRenderingDevice checks\n");
            if (cinterop) CALL0(cinterop, 2, ULONG);
            if (cobj) CALL0(cobj, 2, ULONG);
            if (cf2) CALL0(cf2, 2, ULONG);
        }
            CALL0(factory, 2, ULONG);
        }
    }
    /* the original device does not answer for Device2 */
    hr = ((create_t)GetProcAddress(dc, "DCompositionCreateDevice"))(NULL, &IID_Dev2, (void **)&x);
    CHECK(FAILED(hr));
    puts(f ? "RESULT: FAIL" : "RESULT: PASS");
    return f;
}
