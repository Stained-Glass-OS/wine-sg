/* gdipbmpsave-gate.sh: what GDI+ writes for Image.Save(stream, Bmp) (patches/sg/9463).
 * Prints one line per pixel format:
 *   FMT <name> len <stream bytes> bfsize <bfSize> off <bfOffBits> hdr <biSize> bits <n> comp <n> clr <n> alpha <4th byte of the first pixel>
 */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

typedef struct { UINT32 GdiplusVersion; void *cb; BOOL nobg, noext; } GdiplusStartupInput;
typedef void GpBitmap;
typedef int GpStatus;
typedef DWORD ARGB;
GpStatus WINAPI GdiplusStartup(ULONG_PTR *, const GdiplusStartupInput *, void *);
GpStatus WINAPI GdipCreateBitmapFromScan0(INT, INT, INT, INT, BYTE *, GpBitmap **);
GpStatus WINAPI GdipBitmapSetPixel(GpBitmap *, INT, INT, ARGB);
GpStatus WINAPI GdipSaveImageToStream(GpBitmap *, IStream *, const CLSID *, const void *);
GpStatus WINAPI GdipDisposeImage(GpBitmap *);

static const CLSID bmp_encoder = { 0x557cf400, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
#define PixelFormat1bppIndexed  0x00030101
#define PixelFormat4bppIndexed  0x00030402
#define PixelFormat8bppIndexed  0x00030803
#define PixelFormat24bppRGB     0x00021808
#define PixelFormat32bppARGB    0x0026200a
#define PixelFormat32bppPARGB   0x000e200b

static DWORD d32(const BYTE *b) { return b[0] | b[1] << 8 | b[2] << 16 | (DWORD)b[3] << 24; }

static void one(const char *name, INT fmt)
{
    GpBitmap *bmp = NULL;
    IStream *st;
    HGLOBAL h;
    STATSTG stat;
    BYTE *b;
    GpStatus s;

    if ((s = GdipCreateBitmapFromScan0(37, 21, 0, fmt, NULL, &bmp))) { printf("FMT %s create %d\n", name, s); return; }
    if (fmt == PixelFormat32bppARGB || fmt == PixelFormat32bppPARGB)
    {
        INT x, y;
        for (y = 0; y < 21; y++) for (x = 0; x < 37; x++) GdipBitmapSetPixel(bmp, x, y, 0x80ff0000);
    }
    CreateStreamOnHGlobal(NULL, TRUE, &st);
    s = GdipSaveImageToStream(bmp, st, &bmp_encoder, NULL);
    if (s) { printf("FMT %s save %d\n", name, s); return; }
    IStream_Stat(st, &stat, STATFLAG_NONAME);
    GetHGlobalFromStream(st, &h);
    b = GlobalLock(h);
    printf("FMT %s len %lu bfsize %lu off %lu hdr %lu bits %u comp %lu clr %lu alpha %u\n", name,
           (ULONG)stat.cbSize.QuadPart, d32(b + 2), d32(b + 10), d32(b + 14), b[28] | b[29] << 8, d32(b + 30),
           d32(b + 46), b[d32(b + 10) + 3]);
    GlobalUnlock(h);
    IStream_Release(st);
    GdipDisposeImage(bmp);
}

int main(void)
{
    GdiplusStartupInput in = { 1 };
    ULONG_PTR tok;
    GdiplusStartup(&tok, &in, NULL);
    one("1bpp", PixelFormat1bppIndexed);
    one("4bpp", PixelFormat4bppIndexed);
    one("8bpp", PixelFormat8bppIndexed);
    one("24bpp", PixelFormat24bppRGB);
    one("32bppARGB", PixelFormat32bppARGB);
    one("32bppPARGB", PixelFormat32bppPARGB);
    return 0;
}
