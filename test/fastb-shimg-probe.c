/* shell32 IShellImageData / IShellImageDataFactory (patch 3685) */
#define COBJMACROS
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct SIMG SIMG;
struct SIDVtbl
{
    HRESULT (WINAPI *QueryInterface)(SIMG *, REFIID, void **);
    ULONG (WINAPI *AddRef)(SIMG *);
    ULONG (WINAPI *Release)(SIMG *);
    HRESULT (WINAPI *Decode)(SIMG *, DWORD, ULONG, ULONG);
    HRESULT (WINAPI *Draw)(SIMG *, HDC, RECT *, RECT *);
    HRESULT (WINAPI *NextFrame)(SIMG *);
    HRESULT (WINAPI *NextPage)(SIMG *);
    HRESULT (WINAPI *PrevPage)(SIMG *);
    HRESULT (WINAPI *IsTransparent)(SIMG *);
    HRESULT (WINAPI *IsAnimated)(SIMG *);
    HRESULT (WINAPI *IsVector)(SIMG *);
    HRESULT (WINAPI *IsMultipage)(SIMG *);
    HRESULT (WINAPI *IsEditable)(SIMG *);
    HRESULT (WINAPI *IsPrintable)(SIMG *);
    HRESULT (WINAPI *IsDecoded)(SIMG *);
    HRESULT (WINAPI *GetCurrentPage)(SIMG *, ULONG *);
    HRESULT (WINAPI *GetPageCount)(SIMG *, ULONG *);
    HRESULT (WINAPI *SelectPage)(SIMG *, ULONG);
    HRESULT (WINAPI *GetSize)(SIMG *, SIZE *);
    HRESULT (WINAPI *GetRawDataFormat)(SIMG *, GUID *);
    HRESULT (WINAPI *GetPixelFormat)(SIMG *, int *);
    HRESULT (WINAPI *GetDelay)(SIMG *, DWORD *);
    HRESULT (WINAPI *GetProperties)(SIMG *, DWORD, void **);
    HRESULT (WINAPI *Rotate)(SIMG *, DWORD);
    HRESULT (WINAPI *Scale)(SIMG *, ULONG, ULONG, int);
    HRESULT (WINAPI *DiscardEdit)(SIMG *);
    HRESULT (WINAPI *SetEncoderParams)(SIMG *, void *);
    HRESULT (WINAPI *DisplayName)(SIMG *, WCHAR *, UINT);
    HRESULT (WINAPI *GetResolution)(SIMG *, ULONG *, ULONG *);
    HRESULT (WINAPI *GetEncoderParams)(SIMG *, GUID *, void **);
    HRESULT (WINAPI *RegisterAbort)(SIMG *, void *, void **);
    HRESULT (WINAPI *CloneFrame)(SIMG *, void **);
    HRESULT (WINAPI *ReplaceFrame)(SIMG *, void *);
};
struct SIMG { const struct SIDVtbl *lpVtbl; };

typedef struct SIDF SIDF;
struct SIDFVtbl
{
    HRESULT (WINAPI *QueryInterface)(SIDF *, REFIID, void **);
    ULONG (WINAPI *AddRef)(SIDF *);
    ULONG (WINAPI *Release)(SIDF *);
    HRESULT (WINAPI *CreateIShellImageData)(SIDF *, SIMG **);
    HRESULT (WINAPI *CreateImageFromFile)(SIDF *, const WCHAR *, SIMG **);
    HRESULT (WINAPI *CreateImageFromStream)(SIDF *, IStream *, SIMG **);
    HRESULT (WINAPI *GetDataFormatFromPath)(SIDF *, const WCHAR *, GUID *);
};
struct SIDF { const struct SIDFVtbl *lpVtbl; };

static const CLSID clsid_factory = {0x66e4e4fb, 0xf385, 0x4dd0, {0x8d, 0x74, 0xa2, 0xef, 0xd1, 0xbc, 0x61, 0x78}};
static const GUID iid_factory = {0x9be8ed5c, 0xedab, 0x4d75, {0x90, 0xf3, 0xbd, 0x5b, 0xdb, 0xb2, 0x1c, 0x82}};
static const GUID png_format = {0xb96b3caf, 0x0728, 0x11d3, {0x9d, 0x7b, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
static const GUID jpeg_format = {0xb96b3cae, 0x0728, 0x11d3, {0x9d, 0x7b, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
static const CLSID png_clsid = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};

typedef struct { UINT version; void *cb; BOOL a, b; } startup_t;

static void put16(BYTE *p, WORD v) { p[0] = v; p[1] = v >> 8; }
static void put32(BYTE *p, DWORD v) { put16(p, v); put16(p + 2, v >> 16); }

/* a two page, 8 bit gray TIFF: 8x4 then 4x8 */
static DWORD build_tiff(BYTE *buf)
{
    DWORD pos = 8, ifd[2], i, w[2] = { 8, 4 }, h[2] = { 4, 8 };
    int pg, e;

    memcpy(buf, "II*\0", 4);
    for (pg = 0; pg < 2; pg++)
    {
        DWORD data;
        ifd[pg] = pos;
        data = pos + 2 + 9 * 12 + 4;
        put16(buf + pos, 9);
        for (e = 0; e < 9; e++)
        {
            static const WORD tags[9] = { 256, 257, 258, 259, 262, 273, 277, 278, 279 };
            DWORD val = 0; WORD type = 3;
            switch (tags[e])
            {
            case 256: val = w[pg]; break;
            case 257: val = h[pg]; break;
            case 258: val = 8; break;
            case 259: val = 1; break;
            case 262: val = 1; break;
            case 273: val = data; type = 4; break;
            case 277: val = 1; break;
            case 278: val = h[pg]; break;
            case 279: val = w[pg] * h[pg]; type = 4; break;
            }
            put16(buf + pos + 2 + e * 12, tags[e]); put16(buf + pos + 4 + e * 12, type);
            put32(buf + pos + 6 + e * 12, 1); put32(buf + pos + 10 + e * 12, val);
        }
        /* next IFD follows the pixel data */
        put32(buf + pos + 2 + 9 * 12, pg == 0 ? data + w[pg] * h[pg] : 0);
        for (i = 0; i < w[pg] * h[pg]; i++) buf[data + i] = 40 + pg * 100;
        pos = data + w[pg] * h[pg];
    }
    put32(buf + 4, ifd[0]);
    return pos;
}

static void write_file(const WCHAR *path, const BYTE *data, DWORD len)
{
    DWORD w;
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, data, len, &w, NULL);
    CloseHandle(h);
}

int main(void)
{
    SIDF *factory = NULL;
    SIMG *img = NULL;
    HRESULT hr;
    WCHAR dir[MAX_PATH], tif[MAX_PATH], png[MAX_PATH], name[64];
    BYTE buf[2048];
    DWORD len;
    SIZE size;
    ULONG n, x, y;
    GUID guid;
    HMODULE gp;
    ULONG_PTR token;
    void *bmp = NULL;
    int (WINAPI *pStartup)(ULONG_PTR *, startup_t *, void *);
    int (WINAPI *pCreate)(int, int, int, int, void *, void **);
    int (WINAPI *pSave)(void *, const WCHAR *, const CLSID *, void *);
    int (WINAPI *pDispose)(void *);
    startup_t su = { 1, NULL, FALSE, FALSE };

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(tif, L"%ssgimg_%lu.tif", dir, GetCurrentProcessId());
    wsprintfW(png, L"%ssgimg_%lu.png", dir, GetCurrentProcessId());
    len = build_tiff(buf);
    write_file(tif, buf, len);

    gp = LoadLibraryA("gdiplus.dll");
    pStartup = (void *)GetProcAddress(gp, "GdiplusStartup");
    pCreate = (void *)GetProcAddress(gp, "GdipCreateBitmapFromScan0");
    pSave = (void *)GetProcAddress(gp, "GdipSaveImageToFile");
    pDispose = (void *)GetProcAddress(gp, "GdipDisposeImage");
    pStartup(&token, &su, NULL);
    CHECK(pCreate(8, 4, 0, 0x26200a /* 32bppARGB */, NULL, &bmp) == 0, "bitmap");
    CHECK(pSave(bmp, png, &png_clsid, NULL) == 0, "save png");
    pDispose(bmp);

    hr = CoCreateInstance(&clsid_factory, NULL, CLSCTX_INPROC_SERVER, &iid_factory, (void **)&factory);
    CHECK(hr == S_OK && factory, "factory %#lx", hr);
    if (!factory) return 1;

    /* the format of a file */
    hr = factory->lpVtbl->GetDataFormatFromPath(factory, png, &guid);
    CHECK(hr == S_OK && IsEqualGUID(&guid, &png_format), "format %#lx %s", hr, "guid");
    hr = factory->lpVtbl->GetDataFormatFromPath(factory, L"C:\\nonexistent.png", &guid);
    CHECK(FAILED(hr), "missing file %#lx", hr);

    /* an empty object */
    hr = factory->lpVtbl->CreateIShellImageData(factory, &img);
    CHECK(hr == S_OK && img, "CreateIShellImageData %#lx", hr);
    if (img)
    {
        CHECK(img->lpVtbl->IsDecoded(img) == S_FALSE, "empty is not decoded");
        img->lpVtbl->Release(img);
    }

    /* a multipage TIFF */
    img = NULL;
    hr = factory->lpVtbl->CreateImageFromFile(factory, tif, &img);
    CHECK(hr == S_OK && img, "CreateImageFromFile %#lx", hr);
    if (img)
    {
        CHECK(img->lpVtbl->IsDecoded(img) == S_FALSE, "not decoded before Decode");
        hr = img->lpVtbl->Decode(img, 0, 0, 0);
        CHECK(hr == S_OK, "Decode %#lx", hr);
        CHECK(img->lpVtbl->IsDecoded(img) == S_OK, "decoded");
        CHECK(img->lpVtbl->IsMultipage(img) == S_OK, "multipage");
        CHECK(img->lpVtbl->IsVector(img) == S_FALSE, "not vector");
        CHECK(img->lpVtbl->IsAnimated(img) == S_FALSE, "not animated");
        CHECK(img->lpVtbl->IsPrintable(img) == S_OK, "printable");
        n = 0; hr = img->lpVtbl->GetPageCount(img, &n);
        CHECK(hr == S_OK && n == 2, "page count %lu", n);
        n = 9; hr = img->lpVtbl->GetCurrentPage(img, &n);
        CHECK(hr == S_OK && n == 0, "current page %lu", n);
        hr = img->lpVtbl->GetSize(img, &size);
        CHECK(hr == S_OK && size.cx == 8 && size.cy == 4, "size %ldx%ld", size.cx, size.cy);
        hr = img->lpVtbl->NextPage(img);
        CHECK(hr == S_OK, "NextPage %#lx", hr);
        img->lpVtbl->GetSize(img, &size);
        CHECK(size.cx == 4 && size.cy == 8, "page 2 size %ldx%ld", size.cx, size.cy);
        n = 9; img->lpVtbl->GetCurrentPage(img, &n);
        CHECK(n == 1, "current page %lu", n);
        CHECK(FAILED(img->lpVtbl->NextPage(img)), "no page after the last");
        hr = img->lpVtbl->PrevPage(img);
        CHECK(hr == S_OK, "PrevPage %#lx", hr);
        CHECK(FAILED(img->lpVtbl->PrevPage(img)), "no page before the first");
        CHECK(img->lpVtbl->SelectPage(img, 1) == S_OK, "SelectPage");
        img->lpVtbl->GetSize(img, &size);
        CHECK(size.cx == 4 && size.cy == 8, "selected page size");
        CHECK(img->lpVtbl->SelectPage(img, 5) == E_INVALIDARG, "SelectPage out of range");
        hr = img->lpVtbl->GetRawDataFormat(img, &guid);
        CHECK(hr == S_OK && !IsEqualGUID(&guid, &png_format) && !IsEqualGUID(&guid, &jpeg_format), "tiff format");
        hr = img->lpVtbl->DisplayName(img, name, 64);
        CHECK(hr == S_OK && StrStrIW(name, L"sgimg_") && !wcschr(name, '\\') && StrStrIW(name, L".tif"), "display name %ls", name);
        hr = img->lpVtbl->DisplayName(img, name, 3);
        CHECK(FAILED(hr), "small name buffer %#lx", hr);
        img->lpVtbl->Release(img);
    }

    /* a PNG with alpha: editing */
    img = NULL;
    hr = factory->lpVtbl->CreateImageFromFile(factory, png, &img);
    CHECK(hr == S_OK && img, "png object %#lx", hr);
    if (img)
    {
        void *clone = NULL;
        int pf = 0;

        CHECK(img->lpVtbl->Decode(img, 0, 0, 0) == S_OK, "Decode png");
        CHECK(img->lpVtbl->IsMultipage(img) == S_FALSE, "png is one page");
        n = 0; img->lpVtbl->GetPageCount(img, &n);
        CHECK(n == 1, "png pages %lu", n);
        CHECK(img->lpVtbl->IsEditable(img) == S_OK, "png editable");
        hr = img->lpVtbl->GetRawDataFormat(img, &guid);
        CHECK(hr == S_OK && IsEqualGUID(&guid, &png_format), "png format");
        hr = img->lpVtbl->GetPixelFormat(img, &pf);
        CHECK(hr == S_OK && pf != 0, "pixel format %#x", pf);
        x = y = 0;
        hr = img->lpVtbl->GetResolution(img, &x, &y);
        CHECK(hr == S_OK && x > 0 && y > 0, "resolution %lu x %lu", x, y);
        hr = img->lpVtbl->Rotate(img, 90);
        CHECK(hr == S_OK, "Rotate %#lx", hr);
        img->lpVtbl->GetSize(img, &size);
        CHECK(size.cx == 4 && size.cy == 8, "rotated %ldx%ld", size.cx, size.cy);
        CHECK(img->lpVtbl->Rotate(img, 45) == E_INVALIDARG, "odd angle");
        hr = img->lpVtbl->Scale(img, 16, 12, 0);
        CHECK(hr == S_OK, "Scale %#lx", hr);
        img->lpVtbl->GetSize(img, &size);
        CHECK(size.cx == 16 && size.cy == 12, "scaled %ldx%ld", size.cx, size.cy);
        CHECK(img->lpVtbl->Scale(img, 0, 5, 0) == E_INVALIDARG, "zero scale");
        hr = img->lpVtbl->DiscardEdit(img);
        CHECK(hr == S_OK, "DiscardEdit %#lx", hr);
        img->lpVtbl->GetSize(img, &size);
        CHECK(size.cx == 8 && size.cy == 4, "after DiscardEdit %ldx%ld", size.cx, size.cy);
        hr = img->lpVtbl->CloneFrame(img, &clone);
        CHECK(hr == S_OK && clone, "CloneFrame %#lx", hr);
        if (clone)
        {
            img->lpVtbl->Rotate(img, 90);
            hr = img->lpVtbl->ReplaceFrame(img, clone);
            CHECK(hr == S_OK, "ReplaceFrame %#lx", hr);
            img->lpVtbl->GetSize(img, &size);
            CHECK(size.cx == 8 && size.cy == 4, "after ReplaceFrame %ldx%ld", size.cx, size.cy);
            pDispose(clone);
        }
        hr = img->lpVtbl->GetEncoderParams(img, (GUID *)&jpeg_format, &clone);
        CHECK(hr == S_OK && clone && *(UINT *)clone >= 2, "jpeg encoder params %#lx count %u", hr, clone ? *(UINT *)clone : 0);
        if (clone)
        {
            /* the Quality parameter is among them */
            static const GUID quality = {0x1d5be4b5, 0xfa4a, 0x452d, {0x9c, 0xdd, 0x5d, 0xb3, 0x51, 0x05, 0xe7, 0xeb}};
            UINT count = *(UINT *)clone, i, found = 0;
            struct ep { GUID g; ULONG n, t; void *v; } *p = (struct ep *)((BYTE *)clone + sizeof(void *) * 0 + (sizeof(UINT) + 4 * (sizeof(void *) == 8)));
            for (i = 0; i < count; i++) if (IsEqualGUID(&p[i].g, &quality) && p[i].n == 1 && p[i].t == 6) found++;
            CHECK(found == 1, "quality range parameter");
            CoTaskMemFree(clone);
        }
        img->lpVtbl->Release(img);
    }

    /* from a stream */
    {
        HANDLE h = CreateFileW(png, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        BYTE data[4096];
        DWORD got = 0;
        IStream *stream;

        ReadFile(h, data, sizeof(data), &got, NULL);
        CloseHandle(h);
        stream = SHCreateMemStream(data, got);
        img = NULL;
        hr = factory->lpVtbl->CreateImageFromStream(factory, stream, &img);
        CHECK(hr == S_OK && img, "CreateImageFromStream %#lx", hr);
        if (img)
        {
            CHECK(img->lpVtbl->Decode(img, 0, 0, 0) == S_OK, "Decode stream");
            img->lpVtbl->GetSize(img, &size);
            CHECK(size.cx == 8 && size.cy == 4, "stream size");
            CHECK(img->lpVtbl->IsTransparent(img) == S_OK, "alpha png is transparent");
            CHECK(img->lpVtbl->DisplayName(img, name, 64) == E_FAIL, "stream has no display name");
            img->lpVtbl->Rotate(img, 180);
            CHECK(img->lpVtbl->DiscardEdit(img) == S_OK, "DiscardEdit from stream");
            img->lpVtbl->Release(img);
        }
        IStream_Release(stream);
    }
    factory->lpVtbl->Release(factory);
    DeleteFileW(tif);
    DeleteFileW(png);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
