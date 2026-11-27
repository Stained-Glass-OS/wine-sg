/* decoder-order-probe: the video output types of Wine's GStreamer source ("GStreamer splitter", the
 * decodebin parser that plays what no splitter of its own takes), in the order a downstream filter is
 * offered them (patches/sg/9260). Windows' decoders offer 4:2:0 (NV12, YV12, IYUV/I420) first;
 * AYUV (4:4:4) came first here, so a filter that takes several picked AYUV.
 * The probe writes a small uncompressed AVI of its own. Prints key=1|0 lines. */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <stdio.h>
#include <initguid.h>
DEFINE_GUID(MEDIASUBTYPE_I420_, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(CLSID_decodebin_parser_, 0xf9d8d64e, 0xa144, 0x47dc, 0x8e, 0xe0, 0xf5, 0x34, 0x98, 0x37, 0x2c, 0x29);
#define FCC(a,b,c,d) ((DWORD)(a) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))

static FILE *f;
static void dw(DWORD v) { fwrite(&v, 4, 1, f); }
static void wd(WORD v) { fwrite(&v, 2, 1, f); }
static long mark(DWORD id) { long p; dw(id); p = ftell(f); dw(0); return p; }
static void done(long p) { long e = ftell(f); fseek(f, p, SEEK_SET); dw(e - p - 4); fseek(f, e, SEEK_SET); }

static void write_avi(const char *path)
{
    enum { W = 64, H = 48, FRAMES = 3, SIZE = W * H * 3 };
    static BYTE frame[SIZE];
    long riff, hdrl, avih, strl, strh, strf, movi, idx;
    long offs[FRAMES];
    int i, j;

    f = fopen(path, "wb");
    riff = mark(FCC('R','I','F','F')); dw(FCC('A','V','I',' '));
    hdrl = mark(FCC('L','I','S','T')); dw(FCC('h','d','r','l'));
    avih = mark(FCC('a','v','i','h'));
    dw(200000); dw(0); dw(0); dw(0x10); dw(FRAMES); dw(0); dw(1); dw(SIZE); dw(W); dw(H); dw(0); dw(0); dw(0); dw(0);
    done(avih);
    strl = mark(FCC('L','I','S','T')); dw(FCC('s','t','r','l'));
    strh = mark(FCC('s','t','r','h'));
    dw(FCC('v','i','d','s')); dw(0); dw(0); wd(0); wd(0); dw(0); dw(1); dw(5); dw(0); dw(FRAMES); dw(SIZE); dw(~0u); dw(0);
    wd(0); wd(0); wd(W); wd(H);
    done(strh);
    strf = mark(FCC('s','t','r','f'));
    dw(40); dw(W); dw(H); wd(1); wd(24); dw(0); dw(SIZE); dw(0); dw(0); dw(0); dw(0);
    done(strf);
    done(strl);
    done(hdrl);
    movi = mark(FCC('L','I','S','T')); dw(FCC('m','o','v','i'));
    for (i = 0; i < FRAMES; i++)
    {
        for (j = 0; j < SIZE; j++) frame[j] = (BYTE)(j * 7 + i * 40);
        offs[i] = ftell(f) - movi - 4;
        dw(FCC('0','0','d','b')); dw(SIZE); fwrite(frame, SIZE, 1, f);
    }
    done(movi);
    idx = mark(FCC('i','d','x','1'));
    for (i = 0; i < FRAMES; i++) { dw(FCC('0','0','d','b')); dw(0x10); dw(offs[i]); dw(SIZE); }
    done(idx);
    done(riff);
    fclose(f);
}

static IPin *pin_of(IBaseFilter *filter, PIN_DIRECTION want)
{
    IEnumPins *e; IPin *p; PIN_DIRECTION d;
    IBaseFilter_EnumPins(filter, &e);
    while (IEnumPins_Next(e, 1, &p, NULL) == S_OK)
    {
        IPin_QueryDirection(p, &d);
        if (d == want) { IEnumPins_Release(e); return p; }
        IPin_Release(p);
    }
    IEnumPins_Release(e);
    return NULL;
}

static const char *subtype_name(const GUID *g)
{
    static char buf[16];
    if (IsEqualGUID(g, &MEDIASUBTYPE_NV12)) return "NV12";
    if (IsEqualGUID(g, &MEDIASUBTYPE_YV12)) return "YV12";
    if (IsEqualGUID(g, &MEDIASUBTYPE_I420_) || IsEqualGUID(g, &MEDIASUBTYPE_IYUV)) return "I420";
    if (IsEqualGUID(g, &MEDIASUBTYPE_YUY2)) return "YUY2";
    if (IsEqualGUID(g, &MEDIASUBTYPE_UYVY)) return "UYVY";
    if (IsEqualGUID(g, &MEDIASUBTYPE_YVYU)) return "YVYU";
    if (IsEqualGUID(g, &MEDIASUBTYPE_AYUV)) return "AYUV";
    if (IsEqualGUID(g, &MEDIASUBTYPE_ARGB32)) return "ARGB32";
    if (IsEqualGUID(g, &MEDIASUBTYPE_RGB32)) return "RGB32";
    if (IsEqualGUID(g, &MEDIASUBTYPE_RGB24)) return "RGB24";
    if (IsEqualGUID(g, &MEDIASUBTYPE_RGB565)) return "RGB565";
    if (IsEqualGUID(g, &MEDIASUBTYPE_RGB555)) return "RGB555";
    sprintf(buf, "%08lx", g->Data1);
    return buf;
}

int main(void)
{
    IGraphBuilder *graph; IBaseFilter *source, *parser; IPin *out, *in, *video = NULL, *pin;
    IEnumPins *pins; IEnumMediaTypes *types; AM_MEDIA_TYPE *mt; PIN_DIRECTION dir;
    int pos_nv12 = -1, pos_yv12 = -1, pos_i420 = -1, pos_ayuv = -1, pos_rgb = -1, n = 0;
    char list[512] = "";
    HRESULT hr;

    CoInitialize(NULL);
    write_avi("C:\\decoder-order.avi");
    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    hr = IGraphBuilder_AddSourceFilter(graph, L"C:\\decoder-order.avi", L"source", &source);
    printf("source=%d hr %#lx\n", hr == S_OK, hr);
    if (FAILED(hr)) return 1;
    hr = CoCreateInstance(&CLSID_decodebin_parser_, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&parser);
    printf("parser=%d hr %#lx\n", hr == S_OK, hr);
    if (FAILED(hr)) return 1;
    IGraphBuilder_AddFilter(graph, parser, L"parser");
    out = pin_of(source, PINDIR_OUTPUT);
    in = pin_of(parser, PINDIR_INPUT);
    hr = IGraphBuilder_ConnectDirect(graph, out, in, NULL);
    printf("connect=%d hr %#lx\n", hr == S_OK, hr);
    if (FAILED(hr)) return 1;

    IBaseFilter_EnumPins(parser, &pins);
    while (!video && IEnumPins_Next(pins, 1, &pin, NULL) == S_OK)
    {
        IPin_QueryDirection(pin, &dir);
        if (dir == PINDIR_OUTPUT) video = pin;
        else IPin_Release(pin);
    }
    IEnumPins_Release(pins);
    printf("video_pin=%d\n", video != NULL);
    if (!video) return 1;

    IPin_EnumMediaTypes(video, &types);
    while (IEnumMediaTypes_Next(types, 1, &mt, NULL) == S_OK)
    {
        const char *name = subtype_name(&mt->subtype);
        /* index 0 is the stream's own format (no conversion); the offered conversions follow */
        if (n > 0)
        {
            if (!strcmp(name, "NV12") && pos_nv12 < 0) pos_nv12 = n;
            if (!strcmp(name, "YV12") && pos_yv12 < 0) pos_yv12 = n;
            if (!strcmp(name, "I420") && pos_i420 < 0) pos_i420 = n;
            if (!strcmp(name, "AYUV") && pos_ayuv < 0) pos_ayuv = n;
            if (!strcmp(name, "RGB32") && pos_rgb < 0) pos_rgb = n;
        }
        strcat(list, name); strcat(list, " ");
        n++;
        CoTaskMemFree(mt->pbFormat); CoTaskMemFree(mt);
    }
    IEnumMediaTypes_Release(types);
    printf("types: %s\n", list);
    printf("has_all=%d\n", pos_nv12 > 0 && pos_yv12 > 0 && pos_i420 > 0 && pos_ayuv > 0 && pos_rgb > 0);
    printf("nv12_first=%d\n", pos_nv12 == 1);
    printf("420_before_ayuv=%d\n", pos_nv12 < pos_ayuv && pos_yv12 < pos_ayuv && pos_i420 < pos_ayuv);
    printf("yuv_before_rgb=%d\n", pos_ayuv < pos_rgb);
    return 0;
}
