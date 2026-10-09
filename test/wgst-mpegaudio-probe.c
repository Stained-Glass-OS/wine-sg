/* winegstreamer's MPEG audio decoder (quartz_transform.c) IMpegAudioDecoder
 * settings (patches/sg/2861), run by test/wgst-mpegaudio-gate.sh: defaults,
 * valid / invalid values of each setting, NULL pointers, and get_AudioFormat
 * unconnected and connected to a fake source pin.
 *
 *   wgst-mpegaudio-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <mmreg.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

DEFINE_GUID(IID_IMpegAudioDecoder_, 0xb45dd570, 0x3c77, 0x11d1, 0xab, 0xe1, 0x00, 0xa0, 0xc9, 0x05, 0xf3, 0x75);

typedef struct IMpegAudioDecoderX IMpegAudioDecoderX;
/* The IMpegAudioDecoder vtable (the toolchain header may lack it); the getters and setters are in pairs. */
typedef struct
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IMpegAudioDecoderX *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IMpegAudioDecoderX *);
    ULONG (STDMETHODCALLTYPE *Release)(IMpegAudioDecoderX *);
    struct { HRESULT (STDMETHODCALLTYPE *get)(IMpegAudioDecoderX *, ULONG *); HRESULT (STDMETHODCALLTYPE *put)(IMpegAudioDecoderX *, ULONG); } s[6];
    HRESULT (STDMETHODCALLTYPE *get_AudioFormat)(IMpegAudioDecoderX *, MPEG1WAVEFORMAT *);
} RealVtbl;
struct IMpegAudioDecoderX { const RealVtbl *lpVtbl; };

/* a source pin that only has to be connected to */
struct fakepin { IPin IPin_iface; LONG ref; };
static struct fakepin *fp(IPin *i) { return CONTAINING_RECORD(i, struct fakepin, IPin_iface); }
static HRESULT WINAPI fp_QI(IPin *i, REFIID iid, void **o)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPin)) { *o = i; IPin_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI fp_AddRef(IPin *i) { return InterlockedIncrement(&fp(i)->ref); }
static ULONG WINAPI fp_Release(IPin *i) { return InterlockedDecrement(&fp(i)->ref); }
static HRESULT WINAPI fp_Connect(IPin *i, IPin *p, const AM_MEDIA_TYPE *m) { return E_NOTIMPL; }
static HRESULT WINAPI fp_Receive(IPin *i, IPin *p, const AM_MEDIA_TYPE *m) { return E_NOTIMPL; }
static HRESULT WINAPI fp_Disconnect(IPin *i) { return S_OK; }
static HRESULT WINAPI fp_ConnectedTo(IPin *i, IPin **p) { return VFW_E_NOT_CONNECTED; }
static HRESULT WINAPI fp_ConnMT(IPin *i, AM_MEDIA_TYPE *m) { return E_NOTIMPL; }
static HRESULT WINAPI fp_Info(IPin *i, PIN_INFO *p) { memset(p, 0, sizeof(*p)); p->dir = PINDIR_OUTPUT; wcscpy(p->achName, L"fake"); return S_OK; }
static HRESULT WINAPI fp_Dir(IPin *i, PIN_DIRECTION *d) { *d = PINDIR_OUTPUT; return S_OK; }
static HRESULT WINAPI fp_Id(IPin *i, WCHAR **id) { return E_NOTIMPL; }
static HRESULT WINAPI fp_Accept(IPin *i, const AM_MEDIA_TYPE *m) { return S_OK; }
static HRESULT WINAPI fp_Enum(IPin *i, IEnumMediaTypes **e) { return E_NOTIMPL; }
static HRESULT WINAPI fp_Internal(IPin *i, IPin **p, ULONG *n) { return E_NOTIMPL; }
static HRESULT WINAPI fp_EOS(IPin *i) { return S_OK; }
static HRESULT WINAPI fp_BF(IPin *i) { return S_OK; }
static HRESULT WINAPI fp_EF(IPin *i) { return S_OK; }
static HRESULT WINAPI fp_NS(IPin *i, REFERENCE_TIME a, REFERENCE_TIME b, double r) { return S_OK; }
static const IPinVtbl fp_vtbl = { fp_QI, fp_AddRef, fp_Release, fp_Connect, fp_Receive, fp_Disconnect, fp_ConnectedTo,
    fp_ConnMT, fp_Info, fp_Dir, fp_Id, fp_Accept, fp_Enum, fp_Internal, fp_EOS, fp_BF, fp_EF, fp_NS };

int main(void)
{
    static const struct
    {
        const char *name;
        ULONG def;
        ULONG valid[4], nvalid;
        ULONG invalid[3];
        unsigned ninvalid;
    } tab[] =
    {
        {"FrequencyDivider", 1, {1, 2, 4}, 3, {0, 3, 8}, 3},
        {"DecoderAccuracy", 0, {0, 1, 2, 3}, 4, {0}, 0},
        {"Stereo", 0, {0, 1, 2}, 3, {0}, 0},
        {"DecoderWordSize", 16, {8, 16}, 2, {0, 12, 32}, 3},
        {"IntegerDecode", 0, {1, 0}, 2, {2, 0xffffffff}, 2},
        {"DualMode", 0, {1, 2, 0}, 3, {3, 100}, 2},
    };
    IBaseFilter *filter = NULL;
    IMpegAudioDecoderX *dec = NULL;
    const RealVtbl *vt;
    MPEG1WAVEFORMAT fmt, in_fmt;
    AM_MEDIA_TYPE mt = {0};
    struct fakepin pin = {{(IPinVtbl *)&fp_vtbl}, 1};
    IPin *sink = NULL;
    ULONG v;
    HRESULT hr;
    unsigned i, j;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_CMpegAudioCodec, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
    if (hr != S_OK) { printf("SKIP  no MPEG audio decoder (hr %#lx)\n", (unsigned long)hr); return 77; }
    hr = IBaseFilter_QueryInterface(filter, &IID_IMpegAudioDecoder_, (void **)&dec);
    check(hr == S_OK, "IMpegAudioDecoder is available");
    if (hr != S_OK) return 1;
    vt = (const RealVtbl *)dec->lpVtbl;

    for (i = 0; i < ARRAY_SIZE(tab); i++)
    {
        v = 0xdeadbeef;
        hr = vt->s[i].get(dec, &v);
        CHECKF(hr == S_OK && v == tab[i].def, "%s: default %lu (hr %#lx, got %lu)", tab[i].name, tab[i].def, (unsigned long)hr, v);
        hr = vt->s[i].get(dec, NULL);
        CHECKF(hr == E_POINTER, "%s: get(NULL) is E_POINTER (hr %#lx)", tab[i].name, (unsigned long)hr);
        for (j = 0; j < tab[i].ninvalid; j++)
        {
            hr = vt->s[i].put(dec, tab[i].invalid[j]);
            CHECKF(hr == E_INVALIDARG, "%s: %lu is rejected with E_INVALIDARG (hr %#lx)", tab[i].name, tab[i].invalid[j], (unsigned long)hr);
        }
        for (j = 0; j < tab[i].nvalid; j++)
        {
            hr = vt->s[i].put(dec, tab[i].valid[j]);
            v = 0xdeadbeef;
            vt->s[i].get(dec, &v);
            CHECKF(hr == S_OK && v == tab[i].valid[j], "%s: %lu is accepted and read back (hr %#lx, got %lu)",
                    tab[i].name, tab[i].valid[j], (unsigned long)hr, v);
        }
        /* a rejected value leaves the setting as it was */
        if (tab[i].ninvalid)
        {
            vt->s[i].put(dec, tab[i].valid[0]);
            vt->s[i].put(dec, tab[i].invalid[0]);
            vt->s[i].get(dec, &v);
            CHECKF(v == tab[i].valid[0], "%s: a rejected value does not change the setting (%lu)", tab[i].name, v);
        }
    }
    /* settings are independent */
    vt->s[0].put(dec, 4);
    vt->s[3].put(dec, 8);
    vt->s[0].get(dec, &v);
    CHECKF(v == 4, "setting the word size leaves the divider alone (%lu)", v);

    memset(&fmt, 0xcc, sizeof(fmt));
    hr = vt->get_AudioFormat(dec, NULL);
    CHECKF(hr == E_POINTER, "get_AudioFormat(NULL) is E_POINTER (hr %#lx)", (unsigned long)hr);
    hr = vt->get_AudioFormat(dec, &fmt);
    CHECKF(hr == VFW_E_NOT_CONNECTED, "get_AudioFormat while unconnected is VFW_E_NOT_CONNECTED (hr %#lx)", (unsigned long)hr);

    {
        IEnumPins *pins = NULL;
        hr = IBaseFilter_EnumPins(filter, &pins);
        if (hr == S_OK) { hr = IEnumPins_Next(pins, 1, &sink, NULL); IEnumPins_Release(pins); }
        check(hr == S_OK && sink, "input pin found");
        if (!sink) return 1;
    }
    memset(&in_fmt, 0, sizeof(in_fmt));
    in_fmt.wfx.wFormatTag = WAVE_FORMAT_MPEG;
    in_fmt.wfx.nChannels = 2;
    in_fmt.wfx.nSamplesPerSec = 44100;
    in_fmt.wfx.nAvgBytesPerSec = 16000;
    in_fmt.wfx.nBlockAlign = 1;
    in_fmt.wfx.cbSize = sizeof(in_fmt) - sizeof(WAVEFORMATEX);
    in_fmt.fwHeadLayer = ACM_MPEG_LAYER2;
    in_fmt.dwHeadBitrate = 128000;
    in_fmt.fwHeadMode = ACM_MPEG_STEREO;
    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_MPEG1Payload;
    mt.formattype = FORMAT_WaveFormatEx;
    mt.cbFormat = sizeof(in_fmt);
    mt.pbFormat = (BYTE *)&in_fmt;
    hr = IPin_ReceiveConnection(sink, &pin.IPin_iface, &mt);
    CHECKF(hr == S_OK, "the input pin accepts the fake source (hr %#lx)", (unsigned long)hr);
    if (hr == S_OK)
    {
        memset(&fmt, 0xcc, sizeof(fmt));
        hr = vt->get_AudioFormat(dec, &fmt);
        CHECKF(hr == S_OK && !memcmp(&fmt, &in_fmt, sizeof(fmt)), "get_AudioFormat returns the connected format (hr %#lx)", (unsigned long)hr);
        IPin_Disconnect(sink);
        hr = vt->get_AudioFormat(dec, &fmt);
        CHECKF(hr == VFW_E_NOT_CONNECTED, "unconnected again after Disconnect (hr %#lx)", (unsigned long)hr);
    }
    if (sink) IPin_Release(sink);
    vt->Release(dec);
    IBaseFilter_Release(filter);

    printf("%s\nRESULT: %s\n", failures ? "FAILURES" : "all checks passed", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
