/* winegstreamer media_source.c / quartz_parser.c / mfplat.c remaining stubs
 * (patches/sg/2922), run by test/wgrest-source-gate.sh:
 * IMFByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution and the
 * resolution flags, and IAMStreamSelect of the MPEG splitter.
 *
 *   wgrest-source-probe.exe FILE.mpg */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <stdio.h>

DEFINE_GUID(CLSID_GStreamerByteStreamHandler, 0x317df618, 0x5e5a, 0x468a, 0x9f, 0x15, 0xd8, 0x27, 0xa9, 0xa0, 0x81, 0x62);
DEFINE_GUID(CLSID_MPEG1Splitter_, 0xa8edbf98, 0x2442, 0x42c5, 0x85, 0xa1, 0xab, 0x05, 0xa5, 0x80, 0xdf, 0x53);
DEFINE_GUID(CLSID_AsyncReader_, 0xe436ebb5, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);

/* The mingw headers do not declare IMFByteStreamHandler. */
DEFINE_GUID(IID_IMFByteStreamHandler_, 0xbb420aa4, 0x765b, 0x4a1f, 0x91, 0xfe, 0xd6, 0xa8, 0xa1, 0x43, 0x92, 0x4c);
typedef struct IMFByteStreamHandler_ IMFByteStreamHandler_;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFByteStreamHandler_ *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFByteStreamHandler_ *);
    ULONG (WINAPI *Release)(IMFByteStreamHandler_ *);
    void *BeginCreateObject, *EndCreateObject, *CancelObjectCreation;
    HRESULT (WINAPI *GetMaxNumberOfBytesRequiredForResolution)(IMFByteStreamHandler_ *, QWORD *);
} IMFByteStreamHandlerVtbl_;
struct IMFByteStreamHandler_ { const IMFByteStreamHandlerVtbl_ *lpVtbl; };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECKHR(hr, exp, what) CHECKF((hr) == (HRESULT)(exp), "%s: hr %#lx (want %#lx)", what, (unsigned long)(hr), (unsigned long)(HRESULT)(exp))

static WCHAR mpg_path[MAX_PATH];

/* ---- byte stream handler ----------------------------------------------- */
static void test_handler(void)
{
    IMFByteStreamHandler_ *handler;
    IMFSourceResolver *resolver;
    MF_OBJECT_TYPE type;
    IUnknown *obj;
    QWORD bytes;
    HRESULT hr;
    static const DWORD flag_sets[] = {
        MF_RESOLUTION_MEDIASOURCE,
        MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ,
        MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE,
        MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_KEEP_BYTE_STREAM_ALIVE_ON_FAIL | MF_RESOLUTION_READ,
    };
    unsigned i;

    hr = CoCreateInstance(&CLSID_GStreamerByteStreamHandler, NULL, CLSCTX_INPROC_SERVER, &IID_IMFByteStreamHandler_, (void **)&handler);
    CHECKHR(hr, S_OK, "create the byte stream handler");
    if (hr != S_OK) return;
    hr = handler->lpVtbl->GetMaxNumberOfBytesRequiredForResolution(handler, NULL);
    CHECKHR(hr, E_POINTER, "GetMaxNumberOfBytesRequiredForResolution(NULL)");
    bytes = 0;
    hr = handler->lpVtbl->GetMaxNumberOfBytesRequiredForResolution(handler, &bytes);
    CHECKF(hr == S_OK && bytes == 0x4000, "bytes needed for resolution (hr %#lx, %I64u)", (unsigned long)hr, bytes);
    handler->lpVtbl->Release(handler);

    hr = MFCreateSourceResolver(&resolver);
    CHECKHR(hr, S_OK, "MFCreateSourceResolver");
    for (i = 0; i < sizeof(flag_sets) / sizeof(flag_sets[0]); i++)
    {
        obj = NULL;
        hr = IMFSourceResolver_CreateObjectFromURL(resolver, mpg_path, flag_sets[i], NULL, &type, &obj);
        CHECKF(hr == S_OK && type == MF_OBJECT_MEDIASOURCE && obj, "resolve with flags %#lx: a media source (hr %#lx)", flag_sets[i], (unsigned long)hr);
        if (obj)
        {
            IMFMediaSource_Shutdown((IMFMediaSource *)obj);
            IUnknown_Release(obj);
        }
    }
    IMFSourceResolver_Release(resolver);
}

/* ---- IAMStreamSelect --------------------------------------------------- */
static IPin *find_pin(IBaseFilter *f, PIN_DIRECTION want, int nth)
{
    IEnumPins *e;
    IPin *p;
    IBaseFilter_EnumPins(f, &e);
    while (IEnumPins_Next(e, 1, &p, NULL) == S_OK)
    {
        PIN_DIRECTION d;
        IPin_QueryDirection(p, &d);
        if (d == want && !nth--) { IEnumPins_Release(e); return p; }
        IPin_Release(p);
    }
    IEnumPins_Release(e);
    return NULL;
}

static void check_info(IAMStreamSelect *sel, LONG index, DWORD want_flags, DWORD want_group, const WCHAR *want_name, IUnknown *want_pin, const char *what)
{
    AM_MEDIA_TYPE *mt = (void *)1;
    DWORD flags = 0xdead, group = 0xdead;
    LCID lcid = 0xdead;
    WCHAR *name = (void *)1;
    IUnknown *obj = (void *)1, *unk = (void *)1;
    HRESULT hr = IAMStreamSelect_Info(sel, index, &mt, &flags, &lcid, &group, &name, &obj, &unk);

    CHECKF(hr == S_OK && mt && mt != (void *)1, "%s: Info succeeds with a media type (hr %#lx)", what, (unsigned long)hr);
    CHECKF(flags == want_flags, "%s: flags %#lx (want %#lx)", what, flags, want_flags);
    CHECKF(group == want_group && lcid == 0, "%s: group %lu (want %lu), lcid %lu", what, group, want_group, lcid);
    CHECKF(name && name != (void *)1 && !wcscmp(name, want_name), "%s: name %ls (want %ls)", what, name && name != (void *)1 ? name : L"(none)", want_name);
    CHECKF(obj == want_pin, "%s: the object is the output pin", what);
    CHECKF(!unk, "%s: no unknown", what);
    if (mt && mt != (void *)1) CoTaskMemFree(mt->pbFormat), CoTaskMemFree(mt);
    if (name && name != (void *)1) CoTaskMemFree(name);
    if (obj && obj != (void *)1) IUnknown_Release(obj);
}

static void test_stream_select(void)
{
    IGraphBuilder *graph;
    IBaseFilter *source, *splitter;
    IAMStreamSelect *sel;
    IPin *src_out, *spl_in, *pin_v, *pin_a;
    IUnknown *unk_v, *unk_a;
    IFileSourceFilter *fs;
    DWORD count;
    HRESULT hr;

    if (FAILED(CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph)))
    { check(0, "create a filter graph"); return; }
    hr = CoCreateInstance(&CLSID_AsyncReader_, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&source);
    CHECKHR(hr, S_OK, "create the file source");
    hr = CoCreateInstance(&CLSID_MPEG1Splitter_, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&splitter);
    CHECKHR(hr, S_OK, "create the MPEG splitter");
    if (FAILED(hr)) return;
    IBaseFilter_QueryInterface(source, &IID_IFileSourceFilter, (void **)&fs);
    IFileSourceFilter_Load(fs, mpg_path, NULL);
    IFileSourceFilter_Release(fs);
    IGraphBuilder_AddFilter(graph, source, L"source");
    IGraphBuilder_AddFilter(graph, splitter, L"splitter");
    IBaseFilter_QueryInterface(splitter, &IID_IAMStreamSelect, (void **)&sel);

    count = 0xdead;
    hr = IAMStreamSelect_Count(sel, &count);
    CHECKF(hr == S_OK && count == 0, "no streams before the input is connected (%lu)", count);
    hr = IAMStreamSelect_Info(sel, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECKHR(hr, VFW_E_NOT_CONNECTED, "Info before the input is connected");
    hr = IAMStreamSelect_Enable(sel, 0, AMSTREAMSELECTENABLE_ENABLE);
    CHECKHR(hr, VFW_E_NOT_CONNECTED, "Enable before the input is connected");

    src_out = find_pin(source, PINDIR_OUTPUT, 0);
    spl_in = find_pin(splitter, PINDIR_INPUT, 0);
    hr = IGraphBuilder_ConnectDirect(graph, src_out, spl_in, NULL);
    CHECKHR(hr, S_OK, "connect the file source to the splitter");
    IPin_Release(src_out);
    IPin_Release(spl_in);

    IBaseFilter_FindPin(splitter, L"Video", &pin_v);
    IBaseFilter_FindPin(splitter, L"Audio", &pin_a);
    IPin_QueryInterface(pin_v, &IID_IUnknown, (void **)&unk_v);
    IPin_QueryInterface(pin_a, &IID_IUnknown, (void **)&unk_a);

    hr = IAMStreamSelect_Count(sel, &count);
    CHECKF(hr == S_OK && count == 2, "two streams (%lu)", count);
    check_info(sel, 0, AMSTREAMSELECTINFO_ENABLED, 0, L"Stream(E0)", unk_v, "video stream");
    check_info(sel, 1, AMSTREAMSELECTINFO_ENABLED, 1, L"Stream(C0)", unk_a, "audio stream");
    hr = IAMStreamSelect_Info(sel, 2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECKHR(hr, S_FALSE, "Info past the end");
    hr = IAMStreamSelect_Info(sel, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECKHR(hr, S_FALSE, "Info(-1)");
    /* all out parameters are optional */
    hr = IAMStreamSelect_Info(sel, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECKHR(hr, S_OK, "Info with no out parameters");

    /* Enable */
    hr = IAMStreamSelect_Enable(sel, 5, AMSTREAMSELECTENABLE_ENABLE);
    CHECKHR(hr, E_INVALIDARG, "Enable of stream 5");
    hr = IAMStreamSelect_Enable(sel, -1, AMSTREAMSELECTENABLE_ENABLE);
    CHECKHR(hr, E_INVALIDARG, "Enable of stream -1");
    hr = IAMStreamSelect_Enable(sel, 0, 0x10);
    CHECKHR(hr, E_INVALIDARG, "Enable with unknown flags");
    hr = IAMStreamSelect_Enable(sel, 1, 0);
    CHECKHR(hr, S_OK, "Enable(1, 0) deselects the audio stream");
    check_info(sel, 1, 0, 1, L"Stream(C0)", unk_a, "deselected audio stream");
    check_info(sel, 0, AMSTREAMSELECTINFO_ENABLED, 0, L"Stream(E0)", unk_v, "video stream is not affected");
    hr = IAMStreamSelect_Enable(sel, 1, AMSTREAMSELECTENABLE_ENABLE);
    CHECKHR(hr, S_OK, "Enable(1, ENABLE)");
    check_info(sel, 1, AMSTREAMSELECTINFO_ENABLED, 1, L"Stream(C0)", unk_a, "audio stream enabled again");
    check_info(sel, 0, AMSTREAMSELECTINFO_ENABLED, 0, L"Stream(E0)", unk_v, "video stream is still enabled (own group)");
    IAMStreamSelect_Enable(sel, 0, 0);
    IAMStreamSelect_Enable(sel, 1, 0);
    hr = IAMStreamSelect_Enable(sel, 0, AMSTREAMSELECTENABLE_ENABLEALL);
    CHECKHR(hr, S_OK, "Enable(ENABLEALL)");
    check_info(sel, 0, AMSTREAMSELECTINFO_ENABLED, 0, L"Stream(E0)", unk_v, "video enabled by ENABLEALL");
    check_info(sel, 1, AMSTREAMSELECTINFO_ENABLED, 1, L"Stream(C0)", unk_a, "audio enabled by ENABLEALL");

    IUnknown_Release(unk_v);
    IUnknown_Release(unk_a);
    IPin_Release(pin_v);
    IPin_Release(pin_a);
    IAMStreamSelect_Release(sel);
    IBaseFilter_Release(splitter);
    IBaseFilter_Release(source);
    IGraphBuilder_Release(graph);
}

int main(int argc, char **argv)
{
    HRESULT hr;

    if (argc < 2) { printf("usage: %s file.mpg\n", argv[0]); return 2; }
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, mpg_path, MAX_PATH);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (hr != S_OK) { printf("SKIP  MFStartup %#lx\n", (unsigned long)hr); return 77; }

    test_handler();
    test_stream_select();

    MFShutdown();
    printf("%s\nRESULT: %s\n", failures ? "FAILURES" : "all checks passed", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
