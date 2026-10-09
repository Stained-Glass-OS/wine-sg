/* d2d3dxform-probe: Direct2D's 3D Transform effect moves and scales its
 * input (patches/sg/1525). Word draws its text cursor so: a one-pixel image
 * through a 3D Transform (a 4x4 matrix that scales it to the caret's size
 * and moves it to the caret), a Color Matrix, then DrawImage with
 * D2D1_COMPOSITE_MODE_MASK_INVERT. Wine drew the 3D Transform as its input,
 * one pixel at the origin: Word showed no caret.
 *
 * A 1x1 opaque white bitmap goes through a 3D Transform scaling it 4x20 and
 * moving it to (30,40), drawn with MASK_INVERT over a white 100x100 target.
 * Prints "bounds=l,t,r,b in=<pixel at 31,50> out=<pixel at 40,50> origin=<pixel at 0,0>".
 *   over: a red pixel drawn source-over instead
 *   colormatrix: a red pixel through a Color Matrix that makes red green
 *   (the built-in effects' matrices are for row vectors, as Direct2D's) */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <initguid.h>

DEFINE_GUID(probe_CLSID_3DTransform, 0xe8467b04,0xec61,0x4b8a,0xb5,0xde,0xd4,0xd7,0x3d,0xeb,0xea,0x5a);
DEFINE_GUID(probe_CLSID_ColorMatrix, 0x921f03d6,0x641c,0x47df,0x85,0x2d,0xb4,0xbb,0x61,0x53,0xae,0x11);
DEFINE_GUID(probe_CLSID_2DAffineTransform, 0x6aa97485,0x6354,0x4cfc,0x90,0x8c,0xe4,0xa7,0x4f,0x62,0xc9,0x6c);
DEFINE_GUID(probe_IID_ID2D1Factory1, 0xbb12d362,0xdaee,0x4b9a,0xaa,0x1d,0x14,0xba,0x40,0x1c,0xfa,0x1f);
DEFINE_GUID(probe_IID_ID2D1DeviceContext, 0xe8f7fe7a,0x191c,0x466d,0xad,0x95,0x97,0x56,0x78,0xbd,0xa9,0x98);

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
#define DC_CREATE_BITMAP1         57
#define DC_CREATE_EFFECT          63
#define DC_GET_IMAGE_LOCAL_BOUNDS 70
#define DC_DRAW_IMAGE             83
#define EFFECT_SET_VALUE          9
#define EFFECT_SET_INPUT          14
#define EFFECT_GET_OUTPUT         18

int main(int argc, char **argv)
{
    IWICImagingFactory *wic;
    IWICBitmap *target;
    ID2D1Factory *factory;
    ID2D1RenderTarget *rt;
    void *ctx, *white, *fx;
    ID2D1Image *out;
    D2D1_RENDER_TARGET_PROPERTIES rtp = { D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f, 0, 0 };
    D2D1_BITMAP_PROPERTIES1 bp = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f,
            D2D1_BITMAP_OPTIONS_NONE, NULL };
    D2D1_SIZE_U one = { 1, 1 };
    D2D1_COLOR_F bg = { 1.0f, 1.0f, 1.0f, 1.0f };
    D2D1_RECT_F bounds = { 0 };
    DWORD px = argc > 1 && strcmp(argv[1], "mask") ? 0xffff0000 : 0xffffffff, pixels[100 * 100];
    WICRect all = { 0, 0, 100, 100 };
    float m[16] = { 4,0,0,0,  0,20,0,0,  0,0,1,0,  30,40,0,1 };
    HRESULT hr;

    CoInitialize(NULL);
    if (FAILED(hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic)))
    { printf("wic=%#lx\n", hr); return 1; }
    IWICImagingFactory_CreateBitmap(wic, 100, 100, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &target);
    if (FAILED(hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &probe_IID_ID2D1Factory1, NULL, (void **)&factory)))
    { printf("factory=%#lx\n", hr); return 1; }
    if (FAILED(hr = ID2D1Factory_CreateWicBitmapRenderTarget(factory, target, &rtp, &rt)))
    { printf("rt=%#lx\n", hr); return 1; }
    if (FAILED(hr = ID2D1RenderTarget_QueryInterface(rt, &probe_IID_ID2D1DeviceContext, (void **)&ctx)))
    { printf("ctx=%#lx\n", hr); return 1; }
    if (FAILED(hr = SLOT(ctx, DC_CREATE_BITMAP1, HRESULT (WINAPI *)(void *, D2D1_SIZE_U, const void *, UINT32,
            const D2D1_BITMAP_PROPERTIES1 *, void **))(ctx, one, &px, 4, &bp, &white)))
    { printf("bitmap=%#lx\n", hr); return 1; }
    if (argc > 1 && !strcmp(argv[1], "colormatrix"))
    {
        /* red to green: row r, column g of a matrix for row vectors */
        float cm[20] = { 0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,1,  0,0,0,0 };
        void *cfx; ID2D1Image *cout;
        if (FAILED(hr = SLOT(ctx, DC_CREATE_EFFECT, HRESULT (WINAPI *)(void *, REFCLSID, void **))(ctx, &probe_CLSID_ColorMatrix, &cfx)))
        { printf("effect=%#lx\n", hr); return 1; }
        SLOT(cfx, EFFECT_SET_INPUT, void (WINAPI *)(void *, UINT32, void *, BOOL))(cfx, 0, white, TRUE);
        SLOT(cfx, EFFECT_SET_VALUE, HRESULT (WINAPI *)(void *, UINT32, D2D1_PROPERTY_TYPE, const BYTE *, UINT32))(
                cfx, 0, D2D1_PROPERTY_TYPE_MATRIX_5X4, (const BYTE *)cm, sizeof(cm));
        SLOT(cfx, EFFECT_GET_OUTPUT, void (WINAPI *)(void *, ID2D1Image **))(cfx, &cout);
        ID2D1RenderTarget_BeginDraw(rt);
        ID2D1RenderTarget_Clear(rt, &bg);
        SLOT(ctx, DC_DRAW_IMAGE, void (WINAPI *)(void *, ID2D1Image *, const D2D1_POINT_2F *, const D2D1_RECT_F *,
                D2D1_INTERPOLATION_MODE, D2D1_COMPOSITE_MODE))(ctx, cout, NULL, NULL,
                D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
        ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
        IWICBitmap_CopyPixels(target, &all, 400, sizeof(pixels), (BYTE *)pixels);
        printf("colormatrix=%06lx\n", pixels[0] & 0xffffff);
        return 0;
    }
    if (FAILED(hr = SLOT(ctx, DC_CREATE_EFFECT, HRESULT (WINAPI *)(void *, REFCLSID, void **))(ctx, &probe_CLSID_3DTransform, &fx)))
    { printf("effect=%#lx\n", hr); return 1; }
    SLOT(fx, EFFECT_SET_INPUT, void (WINAPI *)(void *, UINT32, void *, BOOL))(fx, 0, white, TRUE);
    SLOT(fx, EFFECT_SET_VALUE, HRESULT (WINAPI *)(void *, UINT32, D2D1_PROPERTY_TYPE, const BYTE *, UINT32))(
            fx, 2, D2D1_PROPERTY_TYPE_MATRIX_4X4, (const BYTE *)m, sizeof(m));
    {
        UINT32 nearest = 0; /* D2D1_3DTRANSFORM_INTERPOLATION_MODE_NEAREST_NEIGHBOR */
        SLOT(fx, EFFECT_SET_VALUE, HRESULT (WINAPI *)(void *, UINT32, D2D1_PROPERTY_TYPE, const BYTE *, UINT32))(
                fx, 0, D2D1_PROPERTY_TYPE_ENUM, (const BYTE *)&nearest, sizeof(nearest));
    }
    SLOT(fx, EFFECT_GET_OUTPUT, void (WINAPI *)(void *, ID2D1Image **))(fx, &out);

    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_Clear(rt, &bg);
    SLOT(ctx, DC_GET_IMAGE_LOCAL_BOUNDS, HRESULT (WINAPI *)(void *, ID2D1Image *, D2D1_RECT_F *))(ctx, out, &bounds);
    SLOT(ctx, DC_DRAW_IMAGE, void (WINAPI *)(void *, ID2D1Image *, const D2D1_POINT_2F *, const D2D1_RECT_F *,
            D2D1_INTERPOLATION_MODE, D2D1_COMPOSITE_MODE))(ctx, out, NULL, NULL,
            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, argc > 1 && !strcmp(argv[1], "over") ? D2D1_COMPOSITE_MODE_SOURCE_OVER : D2D1_COMPOSITE_MODE_MASK_INVERT);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    IWICBitmap_CopyPixels(target, &all, 400, sizeof(pixels), (BYTE *)pixels);
    printf("bounds=%.0f,%.0f,%.0f,%.0f in=%06lx out=%06lx origin=%06lx\n", bounds.left, bounds.top, bounds.right,
           bounds.bottom, pixels[50 * 100 + 31] & 0xffffff, pixels[50 * 100 + 40] & 0xffffff, pixels[0] & 0xffffff);
    return 0;
}
