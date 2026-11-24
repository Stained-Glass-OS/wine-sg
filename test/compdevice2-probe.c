/* Windows.UI.Composition ICompositionGraphicsDevice2 (patches/sg/9013): CreateDrawingSurface2 makes a surface
 * sized in pixels. Avalonia 11 (ShareX 22.4, ImageGlass 10) draws its windows into such a surface: an empty one,
 * Resized through ICompositionDrawingSurfaceInterop, then BeginDraw/EndDraw each frame. Wine had no
 * ICompositionGraphicsDevice2: Avalonia made a new graphics device every frame and the windows stayed black.
 * Interface identity per windows-rs (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdarg.h>

DEFINE_GUID(IID_CompositorInterop_, 0x25297d5c, 0x3ad4, 0x4c9c, 0xb5, 0xcf, 0xe3, 0x6a, 0x38, 0x51, 0x23, 0x30);
DEFINE_GUID(IID_GraphicsDevice2_, 0x0fb8bdf6, 0xc0f0, 0x4bcc, 0x9f, 0xb8, 0x08, 0x49, 0x82, 0x49, 0x0d, 0x7d);
DEFINE_GUID(IID_DrawingSurface_, 0xa166c300, 0xfad0, 0x4d11, 0x9e, 0x67, 0xe4, 0x33, 0x16, 0x2f, 0xf4, 0x9e);
DEFINE_GUID(IID_SurfaceInterop_, 0xfd04e6e3, 0xfe0c, 0x4c3c, 0xab, 0x19, 0xa0, 0x76, 0x01, 0xa5, 0x76, 0xee);
DEFINE_GUID(IID_D2DDeviceContext_, 0xe8f7fe7a, 0x191c, 0x466d, 0xad, 0x95, 0x97, 0x56, 0x78, 0xbd, 0xa9, 0x98);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef struct { INT32 Width, Height; } SizeInt32;
typedef struct { FLOAT Width, Height; } SizeF;

static int failures, checks;
static void check(int ok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s  ", ok ? "PASS" : "FAIL");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    checks++;
    if (!ok) failures++;
}
static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }

int main(void)
{
    obj *comp = NULL, *interop, *device = NULL, *device2, *surface = NULL, *ds, *si, *dc = NULL, *ctx;
    ID3D11Device *d3d = NULL;
    SizeInt32 zero = { 0, 0 }, bad = { -1, 4 };
    SizeF size = { -1, -1 };
    SIZE px = { 64, 32 };
    POINT offset = { -1, -1 };
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                           D3D11_SDK_VERSION, &d3d, NULL, NULL);
    if (FAILED(hr)) { printf("SKIP no D3D11 device (%08lx)\nRESULT: PASS\n", hr); return 0; }
    RoActivateInstance(hs(L"Windows.UI.Composition.Compositor"), (IInspectable **)&comp);
    interop = as(comp, &IID_CompositorInterop_);
    if (interop) hr = ((HRESULT (WINAPI *)(obj *, IUnknown *, obj **))SLOT(interop, 5))(interop, (IUnknown *)d3d, &device);
    check(interop && device, "a graphics device for a D3D11 device (%08lx)", hr);
    if (!device) { printf("RESULT: FAIL\n"); return 1; }

    device2 = as(device, &IID_GraphicsDevice2_);
    check(device2 != NULL, "the graphics device is an ICompositionGraphicsDevice2");
    if (!device2) { printf("RESULT: FAIL (%d of %d)\n", failures, checks); return 1; }
    hr = ((HRESULT (WINAPI *)(obj *, SizeInt32, int, int, obj **))SLOT(device2, 6))(device2, bad, 87, 1, &surface);
    check(hr == E_INVALIDARG && !surface, "CreateDrawingSurface2 of a negative size is E_INVALIDARG (%08lx)", hr);
    hr = ((HRESULT (WINAPI *)(obj *, SizeInt32, int, int, obj **))SLOT(device2, 6))(device2, zero, 87, 1, &surface);
    check(hr == S_OK && surface, "CreateDrawingSurface2 of 0x0 pixels, BGRA premultiplied (%08lx)", hr);
    if (!surface) { printf("RESULT: FAIL (%d of %d)\n", failures, checks); return 1; }
    ds = as(surface, &IID_DrawingSurface_);
    if (ds) ((HRESULT (WINAPI *)(obj *, SizeF *))SLOT(ds, 8))(ds, &size);
    check(ds && size.Width == 0 && size.Height == 0, "its Size is 0x0 (%.0fx%.0f)", size.Width, size.Height);

    si = as(surface, &IID_SurfaceInterop_);
    check(si != NULL, "it has ICompositionDrawingSurfaceInterop");
    if (!si) { printf("RESULT: FAIL (%d of %d)\n", failures, checks); return 1; }
    hr = ((HRESULT (WINAPI *)(obj *, SIZE))SLOT(si, 5))(si, px);
    if (ds) ((HRESULT (WINAPI *)(obj *, SizeF *))SLOT(ds, 8))(ds, &size);
    check(hr == S_OK && size.Width == 64 && size.Height == 32, "Resize to 64x32 (%08lx, %.0fx%.0f)", hr, size.Width, size.Height);
    hr = ((HRESULT (WINAPI *)(obj *, const RECT *, REFIID, void **, POINT *))SLOT(si, 3))(si, NULL, &IID_D2DDeviceContext_,
            (void **)&dc, &offset);
    check(hr == S_OK && dc, "BeginDraw gives a Direct2D device context (%08lx), offset %ld,%ld", hr, offset.x, offset.y);
    ctx = as(dc, &IID_D2DDeviceContext_);
    check(ctx != NULL, "which is an ID2D1DeviceContext");
    hr = ((HRESULT (WINAPI *)(obj *))SLOT(si, 4))(si);
    check(hr == S_OK, "EndDraw (%08lx)", hr);

    if (failures) printf("RESULT: FAIL (%d of %d)\n", failures, checks);
    else printf("RESULT: PASS (%d checks)\n", checks);
    return 0;
}
