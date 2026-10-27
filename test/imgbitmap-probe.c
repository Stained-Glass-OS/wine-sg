/* Windows.Graphics.Imaging.SoftwareBitmap (patches/sg/6148): created, locked (BitmapBuffer ->
 * IMemoryBufferReference -> IMemoryBufferByteAccess), converted between formats and alpha modes,
 * read-only views sharing its pixels, CopyToBuffer, made from a WIC bitmap and an icon
 * (ISoftwareBitmapNativeFactory, as NanaZip makes its address bar icon) and back to WIC
 * (ISoftwareBitmapNative); bad sizes, formats and closed bitmaps fail. Interface identity per Wine's
 * windows.graphics.imaging.idl / windows.foundation.idl and windows-rs (MIT) for the interop ones. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <wincodec.h>
#include <stdio.h>

DEFINE_GUID(IID_SBFactory,   0xc99feb69, 0x2d62, 0x4d47, 0xa6, 0xb3, 0x4f, 0xdb, 0x6a, 0x07, 0xfd, 0xf8);
DEFINE_GUID(IID_SBStatics,   0xdf0385db, 0x672f, 0x4a9d, 0x80, 0x6e, 0xc2, 0x44, 0x2f, 0x34, 0x3e, 0x86);
DEFINE_GUID(IID_SBNativeF,   0xc3c181ec, 0x2914, 0x4791, 0xaf, 0x02, 0x02, 0xd2, 0x24, 0xa1, 0x0b, 0x43);
DEFINE_GUID(IID_SBNative,    0x94bc8415, 0x04ea, 0x4b2e, 0xaf, 0x13, 0x4d, 0xe9, 0x5a, 0xa8, 0x98, 0xeb);
DEFINE_GUID(IID_SB,          0x689e0708, 0x7eef, 0x483f, 0x96, 0x3f, 0xda, 0x93, 0x88, 0x18, 0xe0, 0x73);
DEFINE_GUID(IID_BB,          0xa53e04c4, 0x399c, 0x438c, 0xb2, 0x8f, 0xa6, 0x3a, 0x6b, 0x83, 0xd1, 0xa1);
DEFINE_GUID(IID_MemBuf,      0xfbc4dd2a, 0x245b, 0x11e4, 0xaf, 0x98, 0x68, 0x94, 0x23, 0x26, 0x0c, 0xf8);
DEFINE_GUID(IID_MemBytes,    0x5b0d3235, 0x4dba, 0x4d44, 0x86, 0x5e, 0x8f, 0x1d, 0x0e, 0x4f, 0xd0, 0x4d);
DEFINE_GUID(IID_Closable,    0x30d5a829, 0x7fa4, 0x4026, 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e);
DEFINE_GUID(IID_BufFactory,  0x71af914d, 0xc10f, 0x484b, 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27);
DEFINE_GUID(IID_BufBytes,    0x905a0fef, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
enum { BGRA8 = 87, RGBA8 = 30, GRAY8 = 62, NV12 = 103 };
enum { PREMUL = 0, STRAIGHT = 1, IGNORE_ALPHA = 2 };
/* ISoftwareBitmap slots */
enum { SB_Format = 6, SB_Alpha, SB_Width, SB_Height, SB_ReadOnly, SB_PutDpiX, SB_DpiX, SB_PutDpiY, SB_DpiY, SB_Lock,
       SB_CopyTo, SB_CopyFromBuffer, SB_CopyToBuffer, SB_ReadOnlyView };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static int getb(obj *sb, int slot) { boolean v = 2; ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(sb, slot))(sb, &v); return v; }
static INT32 geti(obj *sb, int slot) { INT32 v = -1; ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(sb, slot))(sb, &v); return v; }

/* a bitmap's bytes through LockBuffer */
static BYTE *lock(obj *sb, int mode, UINT32 *cap, HRESULT *hr_out, INT32 *stride)
{
    obj *bb = NULL, *ref = NULL, *bytes;
    BYTE *p = NULL;
    HRESULT hr = ((HRESULT (WINAPI *)(obj *, int, obj **))SLOT(sb, SB_Lock))(sb, mode, &bb);
    if (hr_out) *hr_out = hr;
    if (!bb) return NULL;
    if (stride)
    {
        INT32 desc[4] = { 0 };
        ((HRESULT (WINAPI *)(obj *, INT32, INT32 *))SLOT(bb, 7))(bb, 0, desc);
        *stride = desc[3];
    }
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(as(bb, &IID_MemBuf), 6))(as(bb, &IID_MemBuf), &ref);
    bytes = as(ref, &IID_MemBytes);
    if (bytes) ((HRESULT (WINAPI *)(obj *, BYTE **, UINT32 *))SLOT(bytes, 3))(bytes, &p, cap);
    return p;
}

int main(void)
{
    obj *f, *st, *nf, *sb = NULL, *rgba = NULL, *straight = NULL, *view = NULL, *bad = NULL, *fromwic = NULL, *icon_sb = NULL, *conv = NULL;
    IWICImagingFactory *wic;
    IWICBitmap *wb = NULL, *back = NULL;
    UINT32 cap = 0;
    INT32 stride = 0;
    BYTE *p, buf[64];
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(hs(L"Windows.Graphics.Imaging.SoftwareBitmap"), &IID_SBFactory, (void **)&f);
    printf("factory=%08lx\n", hr);
    if (FAILED(hr)) return 1;
    st = as(f, &IID_SBStatics);
    nf = as(f, &IID_SBNativeF);
    printf("statics=%d native_factory=%d\n", st != NULL, nf != NULL);

    hr = ((HRESULT (WINAPI *)(obj *, int, INT32, INT32, obj **))SLOT(f, 6))(f, BGRA8, 4, 3, &sb);
    printf("create=%08lx format=%d alpha=%d size=%dx%d\n", hr, geti(sb, SB_Format), geti(sb, SB_Alpha), geti(sb, SB_Width), geti(sb, SB_Height));
    p = lock(sb, 1, &cap, NULL, &stride);
    printf("lock=%d capacity=%u stride=%d\n", p != NULL, cap, stride);
    if (!p) return 1;
    p[0] = 10; p[1] = 20; p[2] = 30; p[3] = 255;      /* opaque */
    p[4] = 64; p[5] = 64; p[6] = 64; p[7] = 128;      /* half transparent, premultiplied */

    ((HRESULT (WINAPI *)(obj *, obj *, int, obj **))SLOT(st, 7))(st, sb, RGBA8, &rgba);
    p = lock(rgba, 0, &cap, NULL, NULL);
    printf("convert_rgba=%d,%d,%d,%d\n", p ? p[0] : -1, p ? p[1] : -1, p ? p[2] : -1, p ? p[3] : -1);
    ((HRESULT (WINAPI *)(obj *, obj *, int, int, obj **))SLOT(st, 8))(st, sb, BGRA8, STRAIGHT, &straight);
    p = lock(straight, 0, &cap, NULL, NULL);
    printf("convert_straight=%d,%d\n", p ? p[4] : -1, p ? p[7] : -1);

    /* a read-only view shares the pixels */
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(sb, SB_ReadOnlyView))(sb, &view);
    lock(view, 1, &cap, &hr, NULL);
    p = lock(sb, 1, &cap, NULL, NULL);
    p[8] = 99;
    {
        BYTE *q = lock(view, 0, &cap, NULL, NULL);
        printf("view=%d write=%08lx shared=%d\n", getb(view, SB_ReadOnly), hr, q && q[8] == 99);
    }

    /* CopyToBuffer: a Windows.Storage.Streams.Buffer */
    {
        obj *bf = NULL, *buffer = NULL, *bytes;
        UINT32 len = 0;
        BYTE *bp = NULL;
        RoGetActivationFactory(hs(L"Windows.Storage.Streams.Buffer"), &IID_BufFactory, (void **)&bf);
        if (bf) ((HRESULT (WINAPI *)(obj *, UINT32, obj **))SLOT(bf, 6))(bf, 64, &buffer);
        hr = buffer ? ((HRESULT (WINAPI *)(obj *, obj *))SLOT(sb, SB_CopyToBuffer))(sb, buffer) : E_FAIL;
        if (buffer) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(buffer, 7))(buffer, &len);
        bytes = as(buffer, &IID_BufBytes);
        if (bytes) ((HRESULT (WINAPI *)(obj *, BYTE **))SLOT(bytes, 3))(bytes, &bp);
        printf("copy_to_buffer=%08lx length=%u bytes=%d\n", hr, len, bp && bp[0] == 10 && bp[8] == 99);
    }

    /* from a WIC bitmap, and back */
    CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic);
    IWICImagingFactory_CreateBitmap(wic, 2, 2, &GUID_WICPixelFormat32bppBGRA, WICBitmapCacheOnLoad, &wb);
    {
        WICRect rc = { 0, 0, 2, 2 };
        IWICBitmapLock *l = NULL;
        UINT size = 0, wstride = 0;
        BYTE *data = NULL;
        IWICBitmap_Lock(wb, &rc, WICBitmapLockWrite, &l);
        IWICBitmapLock_GetDataPointer(l, &size, &data);
        IWICBitmapLock_GetStride(l, &wstride);
        data[0] = 1; data[1] = 2; data[2] = 3; data[3] = 200;
        IWICBitmapLock_Release(l);
    }
    hr = ((HRESULT (WINAPI *)(obj *, IWICBitmap *, BOOL, REFIID, void **))SLOT(nf, 6))(nf, wb, TRUE, &IID_SB, (void **)&fromwic);
    p = fromwic ? lock(fromwic, 0, &cap, NULL, NULL) : NULL;
    printf("from_wic=%08lx format=%d alpha=%d readonly=%d pixel=%d,%d,%d,%d\n", hr, geti(fromwic, SB_Format), geti(fromwic, SB_Alpha),
           getb(fromwic, SB_ReadOnly), p ? p[0] : -1, p ? p[1] : -1, p ? p[2] : -1, p ? p[3] : -1);
    hr = ((HRESULT (WINAPI *)(obj *, REFIID, void **))SLOT(as(fromwic, &IID_SBNative), 6))(as(fromwic, &IID_SBNative), &IID_IWICBitmap, (void **)&back);
    memset(buf, 0, sizeof(buf));
    if (back) IWICBitmap_CopyPixels(back, NULL, 8, 16, buf);
    printf("to_wic=%08lx pixel=%d,%d,%d,%d\n", hr, buf[0], buf[1], buf[2], buf[3]);

    /* an icon, as NanaZip makes its address bar's: WIC bitmap of the icon, then premultiplied Bgra8 */
    {
        BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), 16, -16, 1, 32, BI_RGB } };
        DWORD *bits;
        HBITMAP color = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
        HBITMAP mask = CreateBitmap(16, 16, 1, 1, NULL);
        ICONINFO ii = { TRUE, 0, 0, mask, color };
        HICON icon;
        IWICBitmap *iw = NULL;
        int i;
        for (i = 0; i < 256; i++) bits[i] = 0xffff0000;   /* opaque red */
        icon = CreateIconIndirect(&ii);
        IWICImagingFactory_CreateBitmapFromHICON(wic, icon, &iw);
        ((HRESULT (WINAPI *)(obj *, IWICBitmap *, BOOL, REFIID, void **))SLOT(nf, 6))(nf, iw, TRUE, &IID_SB, (void **)&icon_sb);
        ((HRESULT (WINAPI *)(obj *, obj *, int, int, obj **))SLOT(st, 8))(st, icon_sb, BGRA8, PREMUL, &conv);
        p = conv ? lock(conv, 0, &cap, NULL, NULL) : NULL;
        printf("icon=%dx%d pixel=%d,%d,%d,%d\n", geti(conv, SB_Width), geti(conv, SB_Height), p ? p[0] : -1, p ? p[1] : -1,
               p ? p[2] : -1, p ? p[3] : -1);
    }

    /* bad requests */
    printf("bad_size=%08lx\n", ((HRESULT (WINAPI *)(obj *, int, INT32, INT32, obj **))SLOT(f, 6))(f, BGRA8, -1, 3, &bad));
    printf("huge=%d\n", FAILED(((HRESULT (WINAPI *)(obj *, int, INT32, INT32, obj **))SLOT(f, 6))(f, BGRA8, 100000, 100000, &bad)));
    printf("bad_format=%d\n", FAILED(((HRESULT (WINAPI *)(obj *, int, INT32, INT32, obj **))SLOT(f, 6))(f, NV12, 4, 4, &bad)));
    printf("gray_alpha=%d\n", FAILED(((HRESULT (WINAPI *)(obj *, int, INT32, INT32, int, obj **))SLOT(f, 7))(f, GRAY8, 4, 4, STRAIGHT, &bad)));
    printf("copy_mismatch=%d\n", FAILED(((HRESULT (WINAPI *)(obj *, obj *))SLOT(sb, SB_CopyTo))(sb, fromwic)));
    ((HRESULT (WINAPI *)(obj *))SLOT(as(rgba, &IID_Closable), 6))(as(rgba, &IID_Closable));
    printf("closed=%d\n", geti(rgba, SB_Width) == -1);
    return 0;
}
