/* gdiplus (patch 9011): saving a 24/32bpp bitmap as GIF. WIC's GIF encoder takes only 8bpp indexed; Windows'
 * GDI+ reduces the picture to the halftone palette and writes a real frame. Wine failed the frame and wrote a
 * 24-byte GIF with no image (Cyberduck then crashed reading it back). Saves, reloads, checks the pixels. */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int Status;
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
typedef struct { CLSID Clsid; GUID FormatID; const WCHAR *CodecName, *DllName, *FormatDescription, *FilenameExtension,
                 *MimeType; DWORD Flags, Version, SigCount, SigSize; const BYTE *SigPattern, *SigMask; } CodecInfo;
static int failures, checks;
static void check(int ok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s  ", ok ? "PASS" : "FAIL");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    fflush(stdout);
    checks++;
    if (!ok) failures++;
}
#define FN(ret, name, args) static ret (WINAPI *p##name) args
FN(Status, GdiplusStartup, (ULONG_PTR *, const StartupInput *, void *));
FN(Status, GdipCreateBitmapFromScan0, (int, int, int, int, BYTE *, void **));
FN(Status, GdipBitmapSetPixel, (void *, int, int, UINT32));
FN(Status, GdipBitmapGetPixel, (void *, int, int, UINT32 *));
FN(Status, GdipDisposeImage, (void *));
FN(Status, GdipGetImageEncodersSize, (UINT *, UINT *));
FN(Status, GdipGetImageEncoders, (UINT, UINT, CodecInfo *));
FN(Status, GdipSaveImageToFile, (void *, const WCHAR *, const CLSID *, const void *));
FN(Status, GdipSaveImageToStream, (void *, IStream *, const CLSID *, const void *));
FN(Status, GdipLoadImageFromFile, (const WCHAR *, void **));
FN(Status, GdipLoadImageFromStream, (IStream *, void **));
FN(Status, GdipGetImagePixelFormat, (void *, int *));
FN(Status, GdipGetImageWidth, (void *, UINT *));
FN(Status, GdipGetImageHeight, (void *, UINT *));

#define W 64
#define H 48
/* colours of the halftone palette: they must come back exactly */
static const UINT32 solid[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };

static UINT32 expect_at(int x, int y, int alpha_case)
{
    if (y < H / 2) return solid[x / (W / 4)];
    if (alpha_case && x < 8) return 0xff000000;          /* transparent pixels: over black */
    return 0xff000000 | ((x * 4) << 16) | ((y * 5) << 8) | (255 - x * 4);    /* a gradient: near */
}

static void fill(void *bmp, int alpha_case)
{
    int x, y;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
        {
            UINT32 c = expect_at(x, y, alpha_case);
            if (alpha_case && y >= H / 2 && x < 8) c = 0x00ff00ff;     /* transparent magenta */
            pGdipBitmapSetPixel(bmp, x, y, c);
        }
}

static void compare(void *img, int alpha_case, const char *what)
{
    int x, y, exact_bad = 0, faroff = 0;
    long long err = 0;
    UINT32 c;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
        {
            UINT32 e = expect_at(x, y, alpha_case);
            c = 0;
            pGdipBitmapGetPixel(img, x, y, &c);
            if (y < H / 2 || (alpha_case && x < 8)) { if ((c | 0xff000000) != e) exact_bad++; continue; }
            {
                int d = abs((int)((c >> 16) & 0xff) - (int)((e >> 16) & 0xff)) + abs((int)((c >> 8) & 0xff) - (int)((e >> 8) & 0xff))
                      + abs((int)(c & 0xff) - (int)(e & 0xff));
                err += d;
                if (d > 200) faroff++;
            }
        }
    check(!exact_bad, "%s: palette colours (and transparent-over-black) come back exactly (%d wrong)", what, exact_bad);
    pGdipBitmapGetPixel(img, 0, 0, &c);
    check(c == 0xffff0000, "%s: pixel (0,0) is red (%08x)", what, c);
    check(err / (W * (H / 2)) < 60 && !faroff, "%s: the gradient is close (mean |error| %lld, %d far off)", what,
          err / (W * (H / 2)), faroff);
}

static void roundtrip(int format, int alpha_case, const CLSID *gif, const char *what)
{
    WCHAR path[MAX_PATH];
    void *bmp = NULL, *img = NULL;
    BYTE head[16];
    DWORD size = 0, got = 0;
    HANDLE f;
    Status st;
    int pf = 0;
    UINT w = 0, h = 0;

    swprintf(path, MAX_PATH, L"C:\\gifsave-%x.gif", format);
    st = pGdipCreateBitmapFromScan0(W, H, 0, format, NULL, &bmp);
    if (st || !bmp) { check(0, "%s: bitmap made (%d)", what, st); return; }
    fill(bmp, alpha_case);
    st = pGdipSaveImageToFile(bmp, path, gif, NULL);
    check(st == 0, "%s: GdipSaveImageToFile as GIF is Ok (%d)", what, st);
    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize(f, NULL);
        ReadFile(f, head, sizeof(head), &got, NULL);
        CloseHandle(f);
    }
    check(size > 200, "%s: the file has an image (%lu bytes; Wine wrote 24)", what, size);
    check(got >= 6 && !memcmp(head, "GIF8", 4), "%s: it is a GIF", what);
    st = pGdipLoadImageFromFile(path, &img);
    check(st == 0 && img, "%s: it loads again (%d)", what, st);
    if (img)
    {
        pGdipGetImagePixelFormat(img, &pf);
        pGdipGetImageWidth(img, &w);
        pGdipGetImageHeight(img, &h);
        check(pf == 0x30803, "%s: reloaded as 8bpp indexed (%#x)", what, pf);
        check(w == W && h == H, "%s: %ux%u", what, w, h);
        compare(img, alpha_case, what);
        pGdipDisposeImage(img);
    }
    /* the stream path too (.NET Image.Save(Stream, ImageFormat.Gif)) */
    {
        IStream *s = NULL;
        STATSTG stat;
        LARGE_INTEGER zero = { { 0 } };
        CreateStreamOnHGlobal(NULL, TRUE, &s);
        st = pGdipSaveImageToStream(bmp, s, gif, NULL);
        memset(&stat, 0, sizeof(stat));
        IStream_Stat(s, &stat, STATFLAG_NONAME);
        check(st == 0 && stat.cbSize.QuadPart > 200, "%s: saved to a stream (%d, %lu bytes)", what, st,
              (unsigned long)stat.cbSize.QuadPart);
        IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
        img = NULL;
        st = pGdipLoadImageFromStream(s, &img);
        if (img)
        {
            UINT32 c = 0;
            pGdipBitmapGetPixel(img, W - 1, 0, &c);
            check(c == 0xffffffff, "%s: stream copy reloads, (%d,0) white (%08x)", what, W - 1, c);
            pGdipDisposeImage(img);
        }
        else check(0, "%s: stream copy reloads (%d)", what, st);
        IStream_Release(s);
    }
    pGdipDisposeImage(bmp);
    DeleteFileW(path);
}

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;
    UINT n = 0, size = 0, i;
    CodecInfo *codecs;
    CLSID gif;
    int found = 0;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipCreateBitmapFromScan0); LOAD(GdipBitmapSetPixel); LOAD(GdipBitmapGetPixel);
    LOAD(GdipDisposeImage); LOAD(GdipGetImageEncodersSize); LOAD(GdipGetImageEncoders); LOAD(GdipSaveImageToFile);
    LOAD(GdipSaveImageToStream); LOAD(GdipLoadImageFromFile); LOAD(GdipLoadImageFromStream); LOAD(GdipGetImagePixelFormat);
    LOAD(GdipGetImageWidth); LOAD(GdipGetImageHeight);
    if (!gp || pGdiplusStartup(&token, &in, NULL) != 0) { printf("RESULT: FAIL (no gdiplus)\n"); return 1; }

    pGdipGetImageEncodersSize(&n, &size);
    codecs = malloc(size);
    pGdipGetImageEncoders(n, size, codecs);
    for (i = 0; i < n; i++)
        if (!wcscmp(codecs[i].MimeType, L"image/gif")) { gif = codecs[i].Clsid; found = 1; }
    check(found, "a GIF encoder");
    if (!found) { printf("RESULT: FAIL\n"); return 1; }

    CoInitialize(NULL);
    roundtrip(0x26200A, 1, &gif, "32bpp ARGB");
    roundtrip(0x21808, 0, &gif, "24bpp RGB");
    roundtrip(0x22009, 0, &gif, "32bpp RGB");
    roundtrip(0xE200B, 1, &gif, "32bpp PARGB");

    if (failures) printf("RESULT: FAIL (%d of %d)\n", failures, checks);
    else printf("RESULT: PASS (%d checks)\n", checks);
    return 0;
}
