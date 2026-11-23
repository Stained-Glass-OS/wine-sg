/* tiffmeta-probe.c: test/tiffmeta-gate.sh (wine-sg 9506). The TIFF encoder writes the frame's IFD metadata:
 * GDI+ property items saved as TIFF (were dropped: "the TIFF encoder writes no metadata blocks") and tags
 * set through a WIC frame's metadata query writer ("/ifd/{ushort=N}") are in the file's directory and
 * read back.
 *   tiffmeta-probe.exe OUT-GDIPLUS.tif OUT-WIC.tif
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <stdio.h>
#include <string.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;
typedef struct { PROPID id; ULONG length; WORD type; void *value; } PropItem;

static GpStatus (WINAPI *pGdiplusStartup)(ULONG_PTR *, const StartupInput *, void *);
static GpStatus (WINAPI *pGdipCreateBitmapFromScan0)(INT, INT, INT, INT, BYTE *, void **);
static GpStatus (WINAPI *pGdipSetPropertyItem)(void *, const PropItem *);
static GpStatus (WINAPI *pGdipSaveImageToFile)(void *, const WCHAR *, const CLSID *, const void *);
static GpStatus (WINAPI *pGdipLoadImageFromFile)(const WCHAR *, void **);
static GpStatus (WINAPI *pGdipGetPropertyItemSize)(void *, PROPID, UINT *);
static GpStatus (WINAPI *pGdipGetPropertyItem)(void *, PROPID, UINT, PropItem *);
static GpStatus (WINAPI *pGdipDisposeImage)(void *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static const CLSID tiff_encoder_clsid = { 0x557cf405, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };

static void set_item(void *bitmap, PROPID id, WORD type, const void *value, ULONG length)
{
    PropItem item = { id, length, type, (void *)value };
    pGdipSetPropertyItem(bitmap, &item);
}

#define LOAD(name) (p##name = (void *)GetProcAddress(gdip, #name))

int main(int argc, char **argv)
{
    HMODULE gdip = LoadLibraryA("gdiplus.dll");
    StartupInput si = { 1 };
    ULONG_PTR token;
    WCHAR path[MAX_PATH], path2[MAX_PATH];
    void *bitmap = NULL, *loaded = NULL;
    static const char description[] = "SG TIFF description", artist[] = "g2d", taken[] = "2026:11:23 10:00:00";
    WORD orientation = 3;
    DWORD custom[2] = { 7, 9 };
    BYTE buffer[512];
    PropItem *got = (PropItem *)buffer;
    UINT size;
    char buf[300];

    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, path, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, path2, MAX_PATH);
    LOAD(GdiplusStartup); LOAD(GdipCreateBitmapFromScan0); LOAD(GdipSetPropertyItem); LOAD(GdipSaveImageToFile);
    LOAD(GdipLoadImageFromFile); LOAD(GdipGetPropertyItemSize); LOAD(GdipGetPropertyItem); LOAD(GdipDisposeImage);
    pGdiplusStartup(&token, &si, NULL);

    /* GDI+: property items saved as TIFF */
    pGdipCreateBitmapFromScan0(8, 8, 0, 0x26200a, NULL, &bitmap);
    set_item(bitmap, 0x010e, 2, description, sizeof(description));
    set_item(bitmap, 0x013b, 2, artist, sizeof(artist));
    set_item(bitmap, 0x0112, 3, &orientation, sizeof(orientation));
    set_item(bitmap, 0xc350, 4, custom, sizeof(custom));
    set_item(bitmap, 0x9003, 2, taken, sizeof(taken)); /* an Exif tag: its directory is not written */
    check(!pGdipSaveImageToFile(bitmap, path, &tiff_encoder_clsid, NULL), "GDI+ saves the bitmap as TIFF");
    pGdipDisposeImage(bitmap);
    check(!pGdipLoadImageFromFile(path, &loaded), "and loads it again");
    if (loaded)
    {
        size = 0;
        pGdipGetPropertyItemSize(loaded, 0x010e, &size);
        memset(buffer, 0, sizeof(buffer));
        if (size && size <= sizeof(buffer)) pGdipGetPropertyItem(loaded, 0x010e, size, got);
        sprintf(buf, "its ImageDescription property item is back (%u bytes, \"%s\")", size,
                size ? (const char *)got->value : "");
        check(size && got->type == 2 && !strcmp(got->value, description), buf);
        size = 0;
        pGdipGetPropertyItemSize(loaded, 0x0112, &size);
        if (size && size <= sizeof(buffer)) pGdipGetPropertyItem(loaded, 0x0112, size, got);
        sprintf(buf, "its Orientation is 3 (%u bytes, %u)", size, size ? *(WORD *)got->value : 0);
        check(size && got->type == 3 && *(WORD *)got->value == 3, buf);
        size = 0;
        pGdipGetPropertyItemSize(loaded, 0xc350, &size);
        if (size && size <= sizeof(buffer)) pGdipGetPropertyItem(loaded, 0xc350, size, got);
        sprintf(buf, "a private tag (0xC350, two LONGs) is back (%u bytes)", size);
        check(size && got->type == 4 && got->length == 8 && ((DWORD *)got->value)[1] == 9, buf);
        pGdipDisposeImage(loaded);
    }

    /* WIC: a tag set through the frame's metadata query writer */
    {
        IWICImagingFactory *factory = NULL;
        IWICBitmapEncoder *encoder = NULL;
        IWICBitmapFrameEncode *frame = NULL;
        IWICMetadataQueryWriter *writer = NULL;
        IWICBitmapDecoder *decoder = NULL;
        IWICBitmapFrameDecode *dframe = NULL;
        IWICMetadataQueryReader *reader = NULL;
        IWICStream *stream = NULL;
        WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
        PROPVARIANT value;
        BYTE pixels[8 * 8 * 3] = { 0 };
        HRESULT hr;

        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory);
        IWICImagingFactory_CreateStream(factory, &stream);
        IWICStream_InitializeFromFilename(stream, path2, GENERIC_WRITE);
        IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatTiff, NULL, &encoder);
        IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache);
        IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
        IWICBitmapFrameEncode_Initialize(frame, NULL);
        IWICBitmapFrameEncode_SetSize(frame, 8, 8);
        IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
        hr = IWICBitmapFrameEncode_GetMetadataQueryWriter(frame, &writer);
        check(hr == S_OK, "a TIFF frame has a metadata query writer");
        if (writer)
        {
            value.vt = VT_LPSTR;
            value.pszVal = (char *)"SG Software";
            hr = IWICMetadataQueryWriter_SetMetadataByName(writer, L"/ifd/{ushort=305}", &value);
            sprintf(buf, "Software set at /ifd/{ushort=305} (%#lx)", hr);
            check(hr == S_OK, buf);
            IWICMetadataQueryWriter_Release(writer);
        }
        IWICBitmapFrameEncode_WritePixels(frame, 8, 24, sizeof(pixels), pixels);
        IWICBitmapFrameEncode_Commit(frame);
        IWICBitmapEncoder_Commit(encoder);
        IWICBitmapFrameEncode_Release(frame);
        IWICBitmapEncoder_Release(encoder);
        IWICStream_Release(stream);

        hr = IWICImagingFactory_CreateDecoderFromFilename(factory, path2, NULL, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand, &decoder);
        if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(decoder, 0, &dframe);
        if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_GetMetadataQueryReader(dframe, &reader);
        PropVariantInit(&value);
        if (SUCCEEDED(hr)) hr = IWICMetadataQueryReader_GetMetadataByName(reader, L"/ifd/{ushort=305}", &value);
        sprintf(buf, "and read back from the file (%#lx, \"%s\")", hr, value.vt == VT_LPSTR ? value.pszVal : "");
        check(hr == S_OK && value.vt == VT_LPSTR && !strcmp(value.pszVal, "SG Software"), buf);
        PropVariantClear(&value);
        if (reader) IWICMetadataQueryReader_Release(reader);
        if (dframe) IWICBitmapFrameDecode_Release(dframe);
        if (decoder) IWICBitmapDecoder_Release(decoder);
        if (factory) IWICImagingFactory_Release(factory);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
