/* Probe for patches/sg/1491: a bitmap that CreateSharedBitmap makes from
 * another Direct2D bitmap, with bitmap properties given, can be drawn.
 * Word's ribbon draws its font name and size boxes from such views of its
 * atlas; Wine made every such view a target that cannot be drawn, so the
 * boxes came out black.
 *
 * Prints name=1 / name=0 lines; the gate checks them.
 *
 *   x86_64-w64-mingw32-gcc -O2 -o d2dsharedview-probe.exe d2dsharedview-probe.c -ld2d1 -lwindowscodecs -lole32 -luuid
 */
#define COBJMACROS
#include <windows.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <stdio.h>

static void report(const char *name, int ok, const char *fmt, ...)
{
    va_list ap;
    printf("%s=%d", name, ok ? 1 : 0);
    if (fmt)
    {
        printf(" ");
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
    printf("\n");
    fflush(stdout);
}

static DWORD pixel(IWICBitmap *bitmap, UINT x, UINT y)
{
    WICRect all = { 0, 0, 64, 32 };
    IWICBitmapLock *lock;
    UINT size, stride;
    BYTE *data;
    DWORD ret = 0xdeadbeef;

    if (FAILED(IWICBitmap_Lock(bitmap, &all, WICBitmapLockRead, &lock))) return ret;
    if (SUCCEEDED(IWICBitmapLock_GetStride(lock, &stride)) && SUCCEEDED(IWICBitmapLock_GetDataPointer(lock, &size, &data)))
        ret = *(DWORD *)(data + y * stride + x * 4);
    IWICBitmapLock_Release(lock);
    return ret;
}

int main(void)
{
    IWICImagingFactory *wic = NULL;
    IWICBitmap *target_bitmap = NULL;
    ID2D1Factory *factory = NULL;
    ID2D1RenderTarget *rt = NULL;
    ID2D1Bitmap *source = NULL, *view = NULL, *view_ignore = NULL;
    ID2D1Bitmap1 *view1 = NULL, *source1 = NULL;
    D2D1_RENDER_TARGET_PROPERTIES props = { D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 0, 0,
            D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT };
    D2D1_BITMAP_PROPERTIES bp = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f };
    D2D1_BITMAP_PROPERTIES bp_ignore = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE }, 96.0f, 96.0f };
    D2D1_COLOR_F clear = { 0, 0, 1.0f, 1.0f };
    D2D1_SIZE_U size = { 16, 16 };
    D2D1_RECT_F dst1 = { 0, 0, 16, 16 }, dst2 = { 20, 0, 36, 16 }, dst3 = { 40, 0, 56, 16 };
    DWORD pixels[16 * 16], p1, p2, p3, p4;
    HRESULT hr;
    int i;

    for (i = 0; i < 16 * 16; i++) pixels[i] = 0xffff0000; /* opaque red, premultiplied */

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic);
    if (SUCCEEDED(hr))
        hr = IWICImagingFactory_CreateBitmap(wic, 64, 32, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &target_bitmap);
    if (SUCCEEDED(hr))
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&factory);
    if (SUCCEEDED(hr))
        hr = ID2D1Factory_CreateWicBitmapRenderTarget(factory, target_bitmap, &props, &rt);
    report("target", SUCCEEDED(hr), "%#lx", hr);
    if (!rt) { printf("done=0\n"); return 1; }

    hr = ID2D1RenderTarget_CreateBitmap(rt, size, pixels, 16 * 4, &bp, &source);
    report("source", SUCCEEDED(hr), "%#lx", hr);
    hr = ID2D1RenderTarget_CreateSharedBitmap(rt, &IID_ID2D1Bitmap, source, &bp, &view);
    report("shared_view", SUCCEEDED(hr), "%#lx", hr);
    hr = ID2D1RenderTarget_CreateSharedBitmap(rt, &IID_ID2D1Bitmap, source, &bp_ignore, &view_ignore);
    report("shared_view_ignore", SUCCEEDED(hr), "%#lx", hr);

    /* through the vtable: mingw's C macros for ID2D1Bitmap1 do not build. GetOptions is slot 12
     * (IUnknown 3, ID2D1Resource 1, ID2D1Bitmap 7, GetColorContext) */
    if (view && SUCCEEDED(((IUnknown *)view)->lpVtbl->QueryInterface((IUnknown *)view, &IID_ID2D1Bitmap1, (void **)&view1))
            && SUCCEEDED(((IUnknown *)source)->lpVtbl->QueryInterface((IUnknown *)source, &IID_ID2D1Bitmap1, (void **)&source1)))
    {
        typedef D2D1_BITMAP_OPTIONS (WINAPI *get_options_fn)(void *);
        D2D1_BITMAP_OPTIONS o = ((get_options_fn)(*(void ***)view1)[12])(view1);
        D2D1_BITMAP_OPTIONS so = ((get_options_fn)(*(void ***)source1)[12])(source1);
        report("view_options", o == so, "view %#x source %#x", o, so);
        ((IUnknown *)view1)->lpVtbl->Release((IUnknown *)view1);
        ((IUnknown *)source1)->lpVtbl->Release((IUnknown *)source1);
    }
    else report("view_options", 0, "no ID2D1Bitmap1");

    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_Clear(rt, &clear);
    ID2D1RenderTarget_DrawBitmap(rt, source, &dst1, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, NULL);
    if (view) ID2D1RenderTarget_DrawBitmap(rt, view, &dst2, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, NULL);
    if (view_ignore) ID2D1RenderTarget_DrawBitmap(rt, view_ignore, &dst3, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, NULL);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    report("enddraw", SUCCEEDED(hr), "%#lx", hr);

    p1 = pixel(target_bitmap, 8, 8);
    p2 = pixel(target_bitmap, 28, 8);
    p3 = pixel(target_bitmap, 48, 8);
    p4 = pixel(target_bitmap, 60, 8);
    report("source_drawn", (p1 & 0xffffff) == 0xff0000, "%08lx", p1);
    report("view_drawn", (p2 & 0xffffff) == 0xff0000, "%08lx", p2);
    report("view_ignore_drawn", (p3 & 0xffffff) == 0xff0000, "%08lx", p3);
    report("background_kept", (p4 & 0xffffff) == 0x0000ff, "%08lx", p4);

    printf("done=1\n");
    return 0;
}
