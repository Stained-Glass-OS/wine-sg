/* Standard picture and font (patches/sg/1674), run by test/oapict-gate.sh:
 * enhanced metafile pictures get their size; metafile and enhanced
 * metafile pictures save and load again; SaveAsFile writes the picture's
 * file (or fails without a memory copy); a picture loaded from a PNG
 * keeps its format when saved; IsDirty; EnumConnectionPoints;
 * OleLoadPictureEx's icon size; OleSavePictureFile; the font's SetHdc,
 * property bag Save and class ID; OleLoadPicturePath of nothing.
 * These were stubs and FIXMEs. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <olectl.h>
#include <wincodec.h>
#include <stdio.h>

DEFINE_GUID(CLSID_StdFont, 0x0be35203, 0x8f91, 0x11ce, 0x9d, 0xe3, 0x00, 0xaa, 0x00, 0x4b, 0xb8, 0x51);
DEFINE_GUID(CLSID_StdPicture, 0x0be35204, 0x8f91, 0x11ce, 0x9d, 0xe3, 0x00, 0xaa, 0x00, 0x4b, 0xb8, 0x51);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static IPicture *save_and_load(IPicture *pic, BYTE *first, int nfirst)
{
    IPersistStream *ps;
    IStream *stream;
    IPicture *back = NULL;
    LARGE_INTEGER zero = {{0}};
    HRESULT hr;

    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IPicture_QueryInterface(pic, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Save(ps, stream, TRUE);
    printf("      Save %08lx\n", hr);
    IPersistStream_Release(ps);
    if (FAILED(hr)) { IStream_Release(stream); return NULL; }
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    if (first)
    {
        DWORD header[2];
        IStream_Read(stream, header, 8, NULL);
        IStream_Read(stream, first, nfirst, NULL);
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    }
    hr = OleLoadPicture(stream, 0, FALSE, &IID_IPicture, (void **)&back);
    printf("      OleLoadPicture %08lx\n", hr);
    IStream_Release(stream);
    return back;
}

/* the colour at the middle of the picture's icon */
static DWORD icon_colour(IPicture *pic)
{
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), 32, -32, 1, 32, BI_RGB }};
    OLE_HANDLE handle = 0;
    DWORD *bits, colour;
    HDC dc = CreateCompatibleDC(0);
    HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);

    SelectObject(dc, bmp);
    IPicture_get_Handle(pic, &handle);
    DrawIconEx(dc, 0, 0, (HICON)(ULONG_PTR)handle, 16, 16, 0, NULL, DI_NORMAL);
    colour = bits[8 * 32 + 8] & 0xffffff;
    DeleteDC(dc);
    DeleteObject(bmp);
    return colour;
}

static short pic_type(IPicture *pic)
{
    short type = -1;
    if (pic) IPicture_get_Type(pic, &type);
    return type;
}

/* a PNG of a 4x4 red square, made with WIC */
static IStream *make_png(void)
{
    IWICImagingFactory *factory;
    IWICBitmapEncoder *encoder;
    IWICBitmapFrameEncode *frame;
    IStream *stream;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    DWORD pixels[16];
    LARGE_INTEGER zero = {{0}};
    int i;

    for (i = 0; i < 16; i++) pixels[i] = 0xffff0000;
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                                (void **)&factory)))
        return NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder);
    IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
    IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
    IWICBitmapFrameEncode_Initialize(frame, NULL);
    IWICBitmapFrameEncode_SetSize(frame, 4, 4);
    IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
    IWICBitmapFrameEncode_WritePixels(frame, 4, 16, sizeof(pixels), (BYTE *)pixels);
    IWICBitmapFrameEncode_Commit(frame);
    IWICBitmapEncoder_Commit(encoder);
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(encoder);
    IWICImagingFactory_Release(factory);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    return stream;
}

/* an icon file with a 16x16 and a 32x32 image */
static IStream *make_ico(void)
{
    static const int sizes[2] = { 16, 32 };
    BYTE buf[16384];
    WORD *dir = (WORD *)buf;
    DWORD offset = 6 + 2 * 16;
    IStream *stream;
    int i;

    memset(buf, 0, sizeof(buf));
    dir[0] = 0; dir[1] = 1; dir[2] = 2;
    for (i = 0; i < 2; i++)
    {
        int n = sizes[i];
        BYTE *entry = buf + 6 + 16 * i;
        BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)(buf + offset);
        DWORD size = sizeof(*bih) + n * n * 4 + n * ((n + 31) / 32 * 4);

        entry[0] = n; entry[1] = n;
        *(WORD *)(entry + 4) = 1;
        *(WORD *)(entry + 6) = 32;
        *(DWORD *)(entry + 8) = size;
        *(DWORD *)(entry + 12) = offset;
        bih->biSize = sizeof(*bih);
        bih->biWidth = n;
        bih->biHeight = 2 * n;
        bih->biPlanes = 1;
        bih->biBitCount = 32;
        {
            /* the 16 pixel image blue, the 32 pixel one red */
            DWORD *px = (DWORD *)(bih + 1);
            int k;
            for (k = 0; k < n * n; k++) px[k] = n == 16 ? 0xff0000ff : 0xffff0000;
        }
        offset += size;
    }
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IStream_Write(stream, buf, offset, NULL);
    {
        LARGE_INTEGER zero = {{0}};
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    }
    return stream;
}

/* a property bag that remembers what is written */
struct bag { IPropertyBag iface; int writes; WCHAR name[64]; CY size; };
static HRESULT WINAPI bag_qi(IPropertyBag *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI bag_addref(IPropertyBag *iface) { return 2; }
static ULONG WINAPI bag_release(IPropertyBag *iface) { return 1; }
static HRESULT WINAPI bag_read(IPropertyBag *iface, LPCOLESTR name, VARIANT *v, IErrorLog *log) { return E_INVALIDARG; }
static HRESULT WINAPI bag_write(IPropertyBag *iface, LPCOLESTR name, VARIANT *v)
{
    struct bag *bag = (struct bag *)iface;
    bag->writes++;
    if (!lstrcmpW(name, L"Name") && V_VT(v) == VT_BSTR) lstrcpynW(bag->name, V_BSTR(v), 64);
    if (!lstrcmpW(name, L"Size") && V_VT(v) == VT_CY) bag->size = V_CY(v);
    return S_OK;
}
static IPropertyBagVtbl bag_vtbl = { bag_qi, bag_addref, bag_release, bag_read, bag_write };

int main(void)
{
    PICTDESC desc = { sizeof(desc) };
    IPicture *pic, *back;
    IConnectionPointContainer *cpc;
    IEnumConnectionPoints *cps;
    IConnectionPoint *points[4];
    IPersistStream *ps;
    IStream *stream, *out;
    IDispatch *disp;
    IFont *font;
    IPersistPropertyBag *ppb;
    struct bag bag = { { &bag_vtbl } };
    FONTDESC fd = { sizeof(fd), (LPOLESTR)L"Arial", {{ 120000, 0 }}, FW_NORMAL, DEFAULT_CHARSET };
    HENHMETAFILE emf;
    HMETAFILE wmf;
    HDC dc;
    RECT frame = { 0, 0, 2000, 1000 };
    LONG cx = 0, cy = 0, size;
    ULONG got;
    BYTE first[8];
    HRESULT hr;
    HFONT hfont;
    LOGFONTW lf;
    CLSID clsid;
    WCHAR path[MAX_PATH];
    HANDLE file;
    DWORD read;
    IClassFactory *cf = NULL;
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **);

    CoInitialize(NULL);

    /* an enhanced metafile picture */
    dc = CreateEnhMetaFileW(NULL, NULL, &frame, NULL);
    Rectangle(dc, 10, 10, 100, 50);
    emf = CloseEnhMetaFile(dc);
    desc.picType = PICTYPE_ENHMETAFILE;
    desc.emf.hemf = emf;
    OleCreatePictureIndirect(&desc, &IID_IPicture, TRUE, (void **)&pic);
    IPicture_get_Width(pic, &cx);
    IPicture_get_Height(pic, &cy);
    printf("      emf %ld x %ld\n", cx, cy);
    check(cx > 1500 && cy > 500, "an enhanced metafile picture: its frame's size");
    back = save_and_load(pic, NULL, 0);
    check(pic_type(back) == PICTYPE_ENHMETAFILE, "saved and loaded again");
    if (back) { IPicture_get_Width(back, &size); check(size == cx, "the same size"); IPicture_Release(back); }
    IPicture_Release(pic);

    /* a metafile picture */
    dc = CreateMetaFileW(NULL);
    Rectangle(dc, 0, 0, 100, 100);
    wmf = CloseMetaFile(dc);
    desc.picType = PICTYPE_METAFILE;
    desc.wmf.hmeta = wmf;
    desc.wmf.xExt = 2540;
    desc.wmf.yExt = 1270;
    OleCreatePictureIndirect(&desc, &IID_IPicture, TRUE, (void **)&pic);
    back = save_and_load(pic, first, 4);
    check(*(DWORD *)first == 0x9ac6cdd7, "a metafile saves as a placeable metafile");
    check(pic_type(back) == PICTYPE_METAFILE, "and loads again");
    if (back)
    {
        IPicture_get_Width(back, &cx);
        IPicture_get_Height(back, &cy);
        printf("      wmf back %ld x %ld\n", cx, cy);
        check(cx == 2540 && cy == 1270, "its size kept");
        IPicture_Release(back);
    }
    IPicture_Release(pic);

    /* SaveAsFile of a bitmap made in memory */
    desc.picType = PICTYPE_BITMAP;
    desc.bmp.hbitmap = CreateBitmap(4, 4, 1, 1, NULL);
    desc.bmp.hpal = 0;
    OleCreatePictureIndirect(&desc, &IID_IPicture, TRUE, (void **)&pic);
    CreateStreamOnHGlobal(NULL, TRUE, &out);
    size = -1;
    check(IPicture_SaveAsFile(pic, out, FALSE, &size) == E_FAIL && size == -1,
          "SaveAsFile without a memory copy of a made picture: E_FAIL");
    size = -1;
    check(IPicture_SaveAsFile(pic, out, TRUE, &size) == S_OK && size > 54, "with one: a BMP file");
    IStream_Release(out);

    /* connection points */
    IPicture_QueryInterface(pic, &IID_IConnectionPointContainer, (void **)&cpc);
    got = 0;
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &cps);
    if (hr == S_OK) { IEnumConnectionPoints_Next(cps, 4, points, &got); }
    check(hr == S_OK && got == 1, "the picture's EnumConnectionPoints: one");
    if (got)
    {
        IID iid;
        IConnectionPoint_GetConnectionInterface(points[0], &iid);
        check(IsEqualIID(&iid, &IID_IPropertyNotifySink), "IPropertyNotifySink's");
        IConnectionPoint_Release(points[0]);
    }
    if (hr == S_OK) IEnumConnectionPoints_Release(cps);
    IConnectionPointContainer_Release(cpc);
    IPicture_Release(pic);

    /* a picture loaded from a PNG */
    stream = make_png();
    check(stream != NULL, "a PNG");
    pic = NULL;
    if (stream) OleLoadPicture(stream, 0, FALSE, &IID_IPicture, (void **)&pic);
    check(pic_type(pic) == PICTYPE_BITMAP, "loaded");
    if (pic)
    {
        BOOL keep = FALSE;
        IPicture_get_KeepOriginalFormat(pic, &keep);
        check(keep, "fRunmode FALSE: the original format kept");
        IPicture_QueryInterface(pic, &IID_IPersistStream, (void **)&ps);
        check(IPersistStream_IsDirty(ps) == S_FALSE, "IsDirty: not after loading");
        IPicture_PictureChanged(pic);
        check(IPersistStream_IsDirty(ps) == S_OK, "after PictureChanged it is");
        IPersistStream_Release(ps);
        back = save_and_load(pic, first, 4);
        printf("      saved: %02x %02x %02x %02x\n", first[0], first[1], first[2], first[3]);
        check(!memcmp(first, "\x89PNG", 4), "saved again as a PNG");
        check(pic_type(back) == PICTYPE_BITMAP, "which loads");
        if (back) IPicture_Release(back);
        IPicture_Release(pic);
    }
    if (stream) IStream_Release(stream);

    /* icons of the size asked for */
    stream = make_ico();
    pic = NULL;
    hr = OleLoadPictureEx(stream, 0, FALSE, &IID_IPicture, 16, 16, LP_DEFAULT, (void **)&pic);
    cx = 0;
    {
        DWORD colour = 0;
        if (pic) { IPicture_get_Width(pic, &cx); colour = icon_colour(pic); IPicture_Release(pic); }
        dc = GetDC(0);
        printf("      icon 16: %08lx width %ld colour %06lx\n", hr, cx, colour);
        check(cx == MulDiv(16, 2540, GetDeviceCaps(dc, LOGPIXELSX)) && colour == 0x0000ff,
              "OleLoadPictureEx 16 by 16: the 16 pixel image");
    }
    IStream_Release(stream);
    stream = make_ico();
    pic = NULL;
    OleLoadPictureEx(stream, 0, FALSE, &IID_IPicture, 32, 32, LP_DEFAULT, (void **)&pic);
    cx = 0;
    {
        DWORD colour = 0;
        if (pic) { IPicture_get_Width(pic, &cx); colour = icon_colour(pic); IPicture_Release(pic); }
        printf("      icon 32: width %ld colour %06lx\n", cx, colour);
        check(cx == MulDiv(32, 2540, GetDeviceCaps(dc, LOGPIXELSX)) && colour == 0xff0000,
              "32 by 32: the 32 pixel image");
    }
    IStream_Release(stream);

    /* OleSavePictureFile and back */
    desc.picType = PICTYPE_BITMAP;
    desc.bmp.hbitmap = CreateBitmap(8, 8, 1, 1, NULL);
    OleCreatePictureIndirect(&desc, &IID_IPictureDisp, TRUE, (void **)&disp);
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"oapict-probe.bmp");
    {
        BSTR name = SysAllocString(path);
        hr = OleSavePictureFile(disp, name);
        SysFreeString(name);
    }
    memset(first, 0, sizeof(first));
    file = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (file != INVALID_HANDLE_VALUE) { ReadFile(file, first, 2, &read, NULL); CloseHandle(file); }
    check(hr == S_OK && !memcmp(first, "BM", 2), "OleSavePictureFile: a BMP file");
    {
        VARIANT name;
        IDispatch *loaded = NULL;
        V_VT(&name) = VT_BSTR;
        V_BSTR(&name) = SysAllocString(path);
        check(OleLoadPictureFile(name, &loaded) == S_OK && loaded, "which OleLoadPictureFile loads");
        if (loaded) IDispatch_Release(loaded);
        VariantClear(&name);
    }
    DeleteFileW(path);
    IDispatch_Release(disp);
    check(OleLoadPicturePath((LPOLESTR)L"", NULL, 0, 0, &IID_IPicture, (void **)&pic) == INET_E_UNKNOWN_PROTOCOL,
          "OleLoadPicturePath of nothing: INET_E_UNKNOWN_PROTOCOL");

    get_class = (void *)GetProcAddress(GetModuleHandleA("oleaut32.dll"), "DllGetClassObject");
    if (get_class) get_class(&CLSID_StdPicture, &IID_IClassFactory, (void **)&cf);
    if (cf)
    {
        IUnknown *unk = NULL;
        check(IClassFactory_QueryInterface(cf, &IID_IUnknown, (void **)&unk) == S_OK && unk,
              "the picture class factory answers IUnknown");
        if (unk) IUnknown_Release(unk);
        IClassFactory_Release(cf);
    }
    else check(0, "the picture class factory");

    /* the font */
    OleCreateFontIndirect(&fd, &IID_IFont, (void **)&font);
    IFont_get_hFont(font, &hfont);
    GetObjectW(hfont, sizeof(lf), &lf);
    printf("      screen lfHeight %ld\n", lf.lfHeight);
    {
        HDC twips = CreateCompatibleDC(dc);
        SetMapMode(twips, MM_TWIPS);
        check(IFont_SetHdc(font, twips) == S_OK, "IFont::SetHdc");
        IFont_get_hFont(font, &hfont);
        GetObjectW(hfont, sizeof(lf), &lf);
        printf("      twips lfHeight %ld\n", lf.lfHeight);
        check(abs(lf.lfHeight) >= 235 && abs(lf.lfHeight) <= 245, "the font sized in the DC's units: 12pt = 240 twips");
        DeleteDC(twips);
    }
    IFont_QueryInterface(font, &IID_IConnectionPointContainer, (void **)&cpc);
    got = 0;
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &cps);
    if (hr == S_OK) { IEnumConnectionPoints_Next(cps, 4, points, &got); IEnumConnectionPoints_Release(cps); }
    check(hr == S_OK && got == 2, "the font's EnumConnectionPoints: two");
    while (got) IConnectionPoint_Release(points[--got]);
    IConnectionPointContainer_Release(cpc);
    IFont_QueryInterface(font, &IID_IPersistPropertyBag, (void **)&ppb);
    check(IPersistPropertyBag_GetClassID(ppb, &clsid) == S_OK && IsEqualCLSID(&clsid, &CLSID_StdFont),
          "the font's property bag class: StdFont");
    hr = IPersistPropertyBag_Save(ppb, &bag.iface, TRUE, TRUE);
    printf("      Save %08lx, %d writes, %ls, %I64d\n", hr, bag.writes, bag.name, bag.size.int64);
    check(hr == S_OK && bag.writes == 7 && bag.name[0] && bag.size.int64 == 120000,
          "the font saved to a property bag");
    IPersistPropertyBag_Release(ppb);
    IFont_Release(font);
    ReleaseDC(0, dc);

    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
