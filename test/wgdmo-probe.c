/* The IMediaObject (DirectX Media Object) interface of winegstreamer's audio
 * resampler, color converter and WMA decoder (patches/sg/2880..2882), run by
 * test/wgdmo-{resampler,colorconv,wma}-gate.sh:
 *
 *   wgdmo-probe.exe resampler | colorconv | wma
 *
 * Table-driven: stream counts and flags, type enumeration, SetType flag and
 * error rules, current types, size info, latency, Lock (from a second thread),
 * allocate / free, input status, flush, discontinuity, ProcessInput /
 * ProcessOutput data exchange, the IPropertyBag / IPropertyStore objects and
 * the IMFTransform status / message methods. A component that cannot be
 * created (a missing GStreamer plugin) is skipped, said.
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <oleauto.h>
#include <objidl.h>
#include <propsys.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <mediaobj.h>
#include <dmerror.h>
#include <mediaerr.h>
#include <dshow.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define G(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    DEFINE_GUID(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)
G(P_CLSID_Resampler, 0xf447b69e, 0x1884, 0x4a7e, 0x80, 0x55, 0x34, 0x6f, 0x74, 0xd6, 0xed, 0xb3);
G(P_CLSID_ColorConv, 0x98230571, 0x0087, 0x4204, 0xb0, 0x20, 0x32, 0x82, 0x53, 0x8e, 0x57, 0xd3);
G(P_CLSID_WMADec, 0x2eeb4adf, 0x4578, 0x4d10, 0xbc, 0xa7, 0xbb, 0x95, 0x5f, 0x56, 0x32, 0x0a);
G(P_IID_IWMResamplerProps, 0xe7e9984f, 0xf09f, 0x4da4, 0x90, 0x3f, 0x6e, 0x2e, 0x0e, 0xfe, 0x56, 0xb5);
G(P_SUB_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_MP3, 0x00000055, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_WMA2, 0x00000161, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_NV12, 0x3231564e, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_RGB32, 0xe436eb7e, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
G(P_MAJOR_AUDIO, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_MAJOR_VIDEO, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_FORMAT_WAVE, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
G(P_FORMAT_VIDEO, 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
/* one table row: expression result against an expected HRESULT */
#define EXPECT(expr, want) do { HRESULT _hr = (expr); CHECKF(_hr == (HRESULT)(want), "%s is %#lx (got %#lx)", #expr, (unsigned long)(want), (unsigned long)_hr); } while (0)

/* ---- an IMediaBuffer --------------------------------------------------- */
struct mbuf
{
    IMediaBuffer iface;
    LONG ref;
    BYTE *data;
    DWORD length, max;
};
static struct mbuf *mb_from(IMediaBuffer *i) { return (struct mbuf *)i; }
static HRESULT WINAPI mb_QI(IMediaBuffer *i, REFIID iid, void **o)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMediaBuffer)) { *o = i; IMediaBuffer_AddRef(i); return S_OK; }
    *o = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI mb_AddRef(IMediaBuffer *i) { return InterlockedIncrement(&mb_from(i)->ref); }
static ULONG WINAPI mb_Release(IMediaBuffer *i)
{
    ULONG r = InterlockedDecrement(&mb_from(i)->ref);
    if (!r) { free(mb_from(i)->data); free(mb_from(i)); }
    return r;
}
static HRESULT WINAPI mb_SetLength(IMediaBuffer *i, DWORD l) { if (l > mb_from(i)->max) return E_INVALIDARG; mb_from(i)->length = l; return S_OK; }
static HRESULT WINAPI mb_GetMax(IMediaBuffer *i, DWORD *l) { *l = mb_from(i)->max; return S_OK; }
static HRESULT WINAPI mb_GetBL(IMediaBuffer *i, BYTE **d, DWORD *l) { *d = mb_from(i)->data; *l = mb_from(i)->length; return S_OK; }
static IMediaBufferVtbl mb_vtbl = {mb_QI, mb_AddRef, mb_Release, mb_SetLength, mb_GetMax, mb_GetBL};
static struct mbuf *mb_create(DWORD max)
{
    struct mbuf *b = calloc(1, sizeof(*b));
    b->iface.lpVtbl = &mb_vtbl;
    b->ref = 1;
    b->data = calloc(1, max);
    b->max = max;
    return b;
}

/* ---- media types ------------------------------------------------------- */
union amt
{
    DMO_MEDIA_TYPE mt;
    struct { DMO_MEDIA_TYPE mt; BYTE format[sizeof(VIDEOINFOHEADER) + 64]; } buf;
};

static void init_audio(union amt *a, const GUID *sub, WORD tag, WORD channels, DWORD rate, WORD bits, WORD extra)
{
    WAVEFORMATEX *w = (WAVEFORMATEX *)a->buf.format;
    memset(a, 0, sizeof(*a));
    a->mt.majortype = P_MAJOR_AUDIO;
    a->mt.subtype = *sub;
    a->mt.bFixedSizeSamples = TRUE;
    a->mt.lSampleSize = channels * bits / 8;
    a->mt.formattype = P_FORMAT_WAVE;
    a->mt.cbFormat = sizeof(*w) + extra;
    a->mt.pbFormat = a->buf.format;
    w->wFormatTag = tag;
    w->nChannels = channels;
    w->nSamplesPerSec = rate;
    w->wBitsPerSample = bits;
    w->nBlockAlign = channels * bits / 8;
    w->nAvgBytesPerSec = rate * w->nBlockAlign;
    w->cbSize = extra;
}

static void init_video(union amt *a, const GUID *sub, DWORD fourcc, WORD bits, LONG w, LONG h)
{
    VIDEOINFOHEADER *v = (VIDEOINFOHEADER *)a->buf.format;
    memset(a, 0, sizeof(*a));
    a->mt.majortype = P_MAJOR_VIDEO;
    a->mt.subtype = *sub;
    a->mt.bFixedSizeSamples = TRUE;
    a->mt.formattype = P_FORMAT_VIDEO;
    a->mt.cbFormat = sizeof(*v);
    a->mt.pbFormat = a->buf.format;
    v->rcSource.right = v->rcTarget.right = w;
    v->rcSource.bottom = v->rcTarget.bottom = h;
    v->bmiHeader.biSize = sizeof(v->bmiHeader);
    v->bmiHeader.biWidth = w;
    v->bmiHeader.biHeight = h;
    v->bmiHeader.biPlanes = 1;
    v->bmiHeader.biBitCount = bits;
    v->bmiHeader.biCompression = fourcc;
    v->bmiHeader.biSizeImage = w * h * bits / 8;
    a->mt.lSampleSize = v->bmiHeader.biSizeImage;
}

static const WAVEFORMATEX *wfx_of(const DMO_MEDIA_TYPE *t) { return (const WAVEFORMATEX *)t->pbFormat; }

/* ---- the shared checks ------------------------------------------------- */
static IMediaObject *dmo;
static IMFTransform *mft;
static DWORD g_input_stream_flags;

static void test_streams(void)
{
    static const struct { DWORD index; HRESULT want; } rows[] = {{0, S_OK}, {1, DMO_E_INVALIDSTREAMINDEX}, {0xdeadbeef, DMO_E_INVALIDSTREAMINDEX}};
    DWORD in, out, flags;
    int i;

    in = out = 0xdeadbeef;
    EXPECT(IMediaObject_GetStreamCount(dmo, &in, &out), S_OK);
    CHECKF(in == 1 && out == 1, "GetStreamCount gives 1 input and 1 output (%lu, %lu)", in, out);
    in = out = 0xdeadbeef;
    EXPECT(IMediaObject_GetStreamCount(dmo, NULL, &out), E_POINTER);
    CHECKF(out == 0xdeadbeef, "GetStreamCount with a NULL input count leaves the output count alone");
    EXPECT(IMediaObject_GetStreamCount(dmo, &in, NULL), E_POINTER);
    CHECKF(in == 0xdeadbeef, "GetStreamCount with a NULL output count leaves the input count alone");

    for (i = 0; i < 3; i++)
    {
        flags = 0xdeadbeef;
        CHECKF(IMediaObject_GetInputStreamInfo(dmo, rows[i].index, &flags) == rows[i].want, "GetInputStreamInfo(%lu) is %#lx", rows[i].index, rows[i].want);
        CHECKF(rows[i].want != S_OK || flags == g_input_stream_flags, "GetInputStreamInfo(%lu) flags are %#lx (got %#lx)", rows[i].index, g_input_stream_flags, flags);
        CHECKF(rows[i].want == S_OK || flags == 0xdeadbeef, "GetInputStreamInfo(%lu) failing leaves the flags alone", rows[i].index);
        flags = 0xdeadbeef;
        CHECKF(IMediaObject_GetOutputStreamInfo(dmo, rows[i].index, &flags) == rows[i].want, "GetOutputStreamInfo(%lu) is %#lx", rows[i].index, rows[i].want);
        CHECKF(rows[i].want != S_OK || flags == 0, "GetOutputStreamInfo(%lu) flags are 0 (got %#lx)", rows[i].index, flags);
    }
    EXPECT(IMediaObject_GetInputStreamInfo(dmo, 0, NULL), E_POINTER);
    EXPECT(IMediaObject_GetOutputStreamInfo(dmo, 0, NULL), E_POINTER);
}

static void test_latency_lock_resources(void)
{
    REFERENCE_TIME lat;

    lat = 0x1234;
    EXPECT(IMediaObject_GetInputMaxLatency(dmo, 0, &lat), S_OK);
    CHECKF(lat == 0, "the default maximum latency is 0 (%I64d)", lat);
    EXPECT(IMediaObject_GetInputMaxLatency(dmo, 1, &lat), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputMaxLatency(dmo, 0, NULL), E_POINTER);
    EXPECT(IMediaObject_SetInputMaxLatency(dmo, 1, 5), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_SetInputMaxLatency(dmo, 0, 123456789), S_OK);
    lat = 0;
    EXPECT(IMediaObject_GetInputMaxLatency(dmo, 0, &lat), S_OK);
    CHECKF(lat == 123456789, "the maximum latency set is read back (%I64d)", lat);
    EXPECT(IMediaObject_SetInputMaxLatency(dmo, 0, 0), S_OK);

    EXPECT(IMediaObject_AllocateStreamingResources(dmo), S_OK);
    EXPECT(IMediaObject_FreeStreamingResources(dmo), S_OK);
}

struct lock_ctx { IMediaObject *dmo; volatile LONG done; };
static DWORD WINAPI lock_thread(void *p)
{
    struct lock_ctx *c = p;
    IMediaObject_Flush(c->dmo);
    InterlockedExchange(&c->done, 1);
    return 0;
}

/* Lock(TRUE) holds the object: a call from another thread waits for Lock(FALSE). */
static void test_lock(void)
{
    struct lock_ctx ctx = {dmo, 0};
    HANDLE th;

    EXPECT(IMediaObject_Lock(dmo, 0), S_OK);
    EXPECT(IMediaObject_Lock(dmo, TRUE), S_OK);
    th = CreateThread(NULL, 0, lock_thread, &ctx, 0, NULL);
    Sleep(300);
    CHECKF(!ctx.done, "a Flush from another thread waits while the object is locked");
    EXPECT(IMediaObject_Lock(dmo, FALSE), S_OK);
    CHECKF(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0 && ctx.done, "the Flush finishes after Lock(FALSE)");
    CloseHandle(th);
    EXPECT(IMediaObject_Lock(dmo, FALSE), S_OK);
}

/* GetXType: rows of {stream, index, with a type pointer, wanted result, major, subtype (NULL: not checked)} */
struct type_row { DWORD stream, index; int ptr; HRESULT want; const GUID *major, *sub; int has_format; };

static void run_type_rows(const char *what, int output, const struct type_row *rows, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        DMO_MEDIA_TYPE mt;
        HRESULT hr;
        memset(&mt, 0, sizeof(mt));
        hr = output ? IMediaObject_GetOutputType(dmo, rows[i].stream, rows[i].index, rows[i].ptr ? &mt : NULL)
                    : IMediaObject_GetInputType(dmo, rows[i].stream, rows[i].index, rows[i].ptr ? &mt : NULL);
        CHECKF(hr == rows[i].want, "%s(%lu, %lu%s) is %#lx (got %#lx)", what, rows[i].stream, rows[i].index, rows[i].ptr ? "" : ", NULL", (unsigned long)rows[i].want, (unsigned long)hr);
        if (hr == S_OK && rows[i].ptr)
        {
            CHECKF(IsEqualGUID(&mt.majortype, rows[i].major), "%s(%lu, %lu) major type", what, rows[i].stream, rows[i].index);
            CHECKF(!rows[i].sub || IsEqualGUID(&mt.subtype, rows[i].sub), "%s(%lu, %lu) subtype", what, rows[i].stream, rows[i].index);
            CHECKF(!!mt.pbFormat == rows[i].has_format && (!rows[i].has_format || mt.cbFormat > 0), "%s(%lu, %lu) %s a format block", what, rows[i].stream, rows[i].index, rows[i].has_format ? "has" : "has no");
            if (rows[i].has_format && IsEqualGUID(&rows[i].major[0], &P_MAJOR_AUDIO))
                CHECKF(IsEqualGUID(&mt.formattype, &P_FORMAT_WAVE), "%s(%lu, %lu) format is WAVEFORMATEX", what, rows[i].stream, rows[i].index);
            CoTaskMemFree(mt.pbFormat);
        }
    }
}

/* SetType rows: flag and pointer rules, same for input and output */
struct set_row { DWORD stream; int which; DWORD flags; HRESULT want; };
enum { T_NULL, T_BAD, T_GOOD, T_WRONGSUB, T_VIDEO };

static HRESULT set_type(int output, DWORD stream, const DMO_MEDIA_TYPE *t, DWORD flags)
{
    return output ? IMediaObject_SetOutputType(dmo, stream, t, flags) : IMediaObject_SetInputType(dmo, stream, t, flags);
}

static void run_set_rows(const char *what, int output, const struct set_row *rows, int n,
        const DMO_MEDIA_TYPE *good, const DMO_MEDIA_TYPE *wrongsub, const DMO_MEDIA_TYPE *video)
{
    static const DMO_MEDIA_TYPE bad;
    static const char *const names[] = {"NULL", "empty type", "good type", "wrong subtype", "video type"};
    int i;
    for (i = 0; i < n; i++)
    {
        const DMO_MEDIA_TYPE *t = rows[i].which == T_NULL ? NULL : rows[i].which == T_BAD ? &bad
                : rows[i].which == T_GOOD ? good : rows[i].which == T_WRONGSUB ? wrongsub : video;
        HRESULT hr = set_type(output, rows[i].stream, t, rows[i].flags);
        CHECKF(hr == rows[i].want, "%s(%lu, %s, flags %#lx) is %#lx (got %#lx)", what, rows[i].stream, names[rows[i].which],
                rows[i].flags, (unsigned long)rows[i].want, (unsigned long)hr);
    }
}

static void check_current(const char *what, int output, HRESULT want_noset_hr, const DMO_MEDIA_TYPE *expect)
{
    DMO_MEDIA_TYPE t;
    HRESULT hr;

    memset(&t, 0, sizeof(t));
    hr = output ? IMediaObject_GetOutputCurrentType(dmo, 1, &t) : IMediaObject_GetInputCurrentType(dmo, 1, &t);
    CHECKF(hr == DMO_E_INVALIDSTREAMINDEX, "%s(1) is DMO_E_INVALIDSTREAMINDEX (%#lx)", what, hr);
    hr = output ? IMediaObject_GetOutputCurrentType(dmo, 1, NULL) : IMediaObject_GetInputCurrentType(dmo, 1, NULL);
    CHECKF(hr == DMO_E_INVALIDSTREAMINDEX, "%s(1, NULL) is DMO_E_INVALIDSTREAMINDEX (%#lx)", what, hr);
    hr = output ? IMediaObject_GetOutputCurrentType(dmo, 0, &t) : IMediaObject_GetInputCurrentType(dmo, 0, &t);
    if (!expect)
    {
        CHECKF(hr == want_noset_hr, "%s before any type is DMO_E_TYPE_NOT_SET (%#lx)", what, hr);
        hr = output ? IMediaObject_GetOutputCurrentType(dmo, 0, NULL) : IMediaObject_GetInputCurrentType(dmo, 0, NULL);
        CHECKF(hr == want_noset_hr, "%s(0, NULL) before any type is DMO_E_TYPE_NOT_SET (%#lx)", what, hr);
        return;
    }
    CHECKF(hr == S_OK, "%s is S_OK once a type is set (%#lx)", what, hr);
    if (hr != S_OK) return;
    CHECKF(IsEqualGUID(&t.majortype, &expect->majortype) && IsEqualGUID(&t.subtype, &expect->subtype), "%s returns the type set (major / subtype)", what);
    if (IsEqualGUID(&expect->majortype, &P_MAJOR_AUDIO))
    {
        const WAVEFORMATEX *a = wfx_of(&t), *b = wfx_of(expect);
        CHECKF(a && t.cbFormat >= sizeof(*a) && a->nChannels == b->nChannels && a->nSamplesPerSec == b->nSamplesPerSec
                && a->wBitsPerSample == b->wBitsPerSample && a->nBlockAlign == b->nBlockAlign,
                "%s returns the WAVEFORMATEX set (%u ch, %lu Hz, %u bits)", what, a ? a->nChannels : 0, a ? a->nSamplesPerSec : 0, a ? a->wBitsPerSample : 0);
    }
    else
    {
        const VIDEOINFOHEADER *a = (const VIDEOINFOHEADER *)t.pbFormat, *b = (const VIDEOINFOHEADER *)expect->pbFormat;
        CHECKF(a && t.cbFormat >= sizeof(*a) && a->bmiHeader.biWidth == b->bmiHeader.biWidth && a->bmiHeader.biHeight == b->bmiHeader.biHeight,
                "%s returns the video size set", what);
    }
    CoTaskMemFree(t.pbFormat);
}

/* ---- property bag and property store ----------------------------------- */
static void test_property_objects(int bag_offered)
{
    IPropertyBag *bag;
    IPropertyStore *store;
    VARIANT v, r;
    PROPVARIANT pv, pr;
    PROPERTYKEY key = {{0x12345678, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}}, 7}, k2 = key, got;
    DWORD count;
    HRESULT hr;

    hr = IMediaObject_QueryInterface(dmo, &IID_IPropertyBag, (void **)&bag);
    CHECKF(bag_offered ? hr == S_OK : hr == E_NOINTERFACE, "IPropertyBag is %s (%#lx)", bag_offered ? "offered" : "not offered", hr);
    if (hr == S_OK)
    {
        VariantInit(&r);
        EXPECT(IPropertyBag_Read(bag, L"NoSuchProp", &r, NULL), E_INVALIDARG);
        EXPECT(IPropertyBag_Read(bag, NULL, &r, NULL), E_POINTER);
        EXPECT(IPropertyBag_Read(bag, L"x", NULL, NULL), E_POINTER);
        VariantInit(&v);
        V_VT(&v) = VT_I4; V_I4(&v) = 5;
        EXPECT(IPropertyBag_Write(bag, L"Quality", &v), S_OK);
        EXPECT(IPropertyBag_Write(bag, NULL, &v), E_POINTER);
        VariantInit(&r);
        EXPECT(IPropertyBag_Read(bag, L"QUALITY", &r, NULL), S_OK);
        CHECKF(V_VT(&r) == VT_I4 && V_I4(&r) == 5, "a property written is read back, the name case-insensitive (vt %u)", V_VT(&r));
        VariantClear(&r);
        V_VT(&r) = VT_BSTR; V_BSTR(&r) = NULL;
        EXPECT(IPropertyBag_Read(bag, L"Quality", &r, NULL), S_OK);
        CHECKF(V_VT(&r) == VT_BSTR && V_BSTR(&r) && !wcscmp(V_BSTR(&r), L"5"), "a property is converted to the type asked");
        VariantClear(&r);
        V_I4(&v) = 9;
        EXPECT(IPropertyBag_Write(bag, L"quality", &v), S_OK);
        VariantInit(&r);
        EXPECT(IPropertyBag_Read(bag, L"Quality", &r, NULL), S_OK);
        CHECKF(V_VT(&r) == VT_I4 && V_I4(&r) == 9, "writing the same name again replaces the value (%ld)", V_I4(&r));
        IPropertyBag_Release(bag);
    }

    hr = IMediaObject_QueryInterface(dmo, &IID_IPropertyStore, (void **)&store);
    CHECKF(hr == S_OK, "IPropertyStore is offered (%#lx)", hr);
    if (hr != S_OK) return;
    count = 99;
    EXPECT(IPropertyStore_GetCount(store, &count), S_OK);
    CHECKF(count == 0, "a new property store is empty (%lu)", count);
    EXPECT(IPropertyStore_GetCount(store, NULL), E_POINTER);
    EXPECT(IPropertyStore_GetAt(store, 0, &got), E_INVALIDARG);
    PropVariantInit(&pr); pr.vt = VT_I4;
    EXPECT(IPropertyStore_GetValue(store, &key, &pr), S_OK);
    CHECKF(pr.vt == VT_EMPTY, "a property not set is VT_EMPTY (vt %u)", pr.vt);
    PropVariantInit(&pv); pv.vt = VT_UI4; pv.ulVal = 77;
    EXPECT(IPropertyStore_SetValue(store, &key, &pv), S_OK);
    EXPECT(IPropertyStore_Commit(store), S_OK);
    count = 0;
    IPropertyStore_GetCount(store, &count);
    CHECKF(count == 1, "the store holds the property set (%lu)", count);
    memset(&got, 0, sizeof(got));
    EXPECT(IPropertyStore_GetAt(store, 0, &got), S_OK);
    CHECKF(IsEqualGUID(&got.fmtid, &key.fmtid) && got.pid == 7, "GetAt returns the key set");
    PropVariantInit(&pr);
    EXPECT(IPropertyStore_GetValue(store, &key, &pr), S_OK);
    CHECKF(pr.vt == VT_UI4 && pr.ulVal == 77, "GetValue returns the value set");
    PropVariantClear(&pr);
    pv.ulVal = 78;
    EXPECT(IPropertyStore_SetValue(store, &key, &pv), S_OK);
    k2.pid = 8;
    EXPECT(IPropertyStore_SetValue(store, &k2, &pv), S_OK);
    count = 0;
    IPropertyStore_GetCount(store, &count);
    CHECKF(count == 2, "a second key adds an entry, the same key does not (%lu)", count);
    PropVariantInit(&pr);
    IPropertyStore_GetValue(store, &key, &pr);
    CHECKF(pr.ulVal == 78, "setting a key again replaces its value (%lu)", pr.ulVal);
    IPropertyStore_Release(store);
}

/* ---- IMFTransform status and messages ---------------------------------- */
static void test_mft_methods(int data_flow)
{
    IMFMediaEvent *event;
    PROPVARIANT pv = {.vt = VT_EMPTY};
    DWORD flags;
    HRESULT hr;

    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetInputStatus(mft, 1, &flags), MF_E_INVALIDSTREAMNUMBER);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_STREAMING, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), E_NOTIMPL);
    hr = MFCreateMediaEvent(MEEndOfStream, &GUID_NULL, S_OK, &pv, &event);
    if (hr == S_OK)
    {
        EXPECT(IMFTransform_ProcessEvent(mft, 0, event), E_NOTIMPL);
        IMFMediaEvent_Release(event);
    }
    (void)data_flow;
}

/* ---- audio data flow (resampler) --------------------------------------- */
#define IN_RATE 22050
#define IN_FRAMES 22050

static void feed_float_sine(struct mbuf *b)
{
    float *f = (float *)b->data;
    int i;
    for (i = 0; i < IN_FRAMES; i++)
        f[2 * i] = f[2 * i + 1] = 0.5f * sinf(2 * 3.14159265f * 440 * i / IN_RATE);
    b->length = IN_FRAMES * 8;
}

static DWORD drain_output(DWORD size, int *calls, int *bad_status, short *peak)
{
    DWORD total = 0;
    struct mbuf *out = mb_create(size);
    int guard = 0;

    for (;;)
    {
        DMO_OUTPUT_DATA_BUFFER ob = {&out->iface, 0xdeadbeef, 0, 0};
        DWORD status = 0xdeadbeef;
        HRESULT hr;
        out->length = 0;
        hr = IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status);
        (*calls)++;
        if (hr == S_FALSE)
        {
            if (ob.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE) (*bad_status)++;
            break;
        }
        if (hr != S_OK || ++guard > 200) { (*bad_status) += 100; break; }
        if (status != 0) (*bad_status)++;
        total += out->length;
        if (peak && out->length >= 2)
        {
            DWORD k;
            for (k = 0; k + 1 < out->length / 2; k++)
            {
                short s = ((short *)out->data)[k];
                if (s < 0) s = -s;
                if (s > *peak) *peak = s;
            }
        }
    }
    mb_Release(&out->iface);
    return total;
}

static void test_resampler_flow(void)
{
    union amt in, out, outf;
    struct mbuf *ib;
    DMO_OUTPUT_DATA_BUFFER ob;
    DWORD size, look, align, total, status, flags;
    HRESULT hr;
    int calls, bad, pushed = 1;
    short peak;

    init_audio(&in, &P_SUB_FLOAT, 3, 2, IN_RATE, 32, 0);
    init_audio(&out, &P_SUB_PCM, 1, 2, 44100, 16, 0);
    init_audio(&outf, &P_SUB_FLOAT, 3, 2, 44100, 32, 0);

    /* before the types are set */
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_Flush(dmo), DMO_E_TYPE_NOT_SET);
    ib = mb_create(IN_FRAMES * 8);
    feed_float_sine(ib);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), DMO_E_TYPE_NOT_SET);
    ob.pBuffer = &ib->iface; ob.dwStatus = 0; ob.rtTimestamp = ob.rtTimelength = 0;
    EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_Discontinuity(dmo, 0), S_OK);

    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_SetOutputType(dmo, 0, &out.mt, 0), S_OK);

    size = look = align = 0xdeadbeef;
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align), S_OK);
    CHECKF(size == 8 && align == 1 && look == 0, "input size info is 8 bytes (one frame), lookahead 0, alignment 1 (%lu, %lu, %lu)", size, look, align);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 1, &size, &look, &align), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, NULL, &look, &align), E_POINTER);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, NULL, &align), E_POINTER);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, NULL), E_POINTER);
    size = align = 0xdeadbeef;
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align), S_OK);
    CHECKF(size == 4 && align == 1, "output size info is 4 bytes (one frame), alignment 1 (%lu, %lu)", size, align);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 1, &size, &align), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, NULL, &align), E_POINTER);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, NULL), E_POINTER);

    /* empty: no output, S_FALSE and nothing written */
    {
        struct mbuf *ob_buf = mb_create(0x4000);
        ob.pBuffer = &ob_buf->iface; ob.dwStatus = 0xdeadbeef; ob.rtTimestamp = ob.rtTimelength = 0;
        status = 0xdeadbeef;
        EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), S_FALSE);
        CHECKF(ob_buf->length == 0, "ProcessOutput with nothing to give writes nothing");
        CHECKF(!(ob.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE), "ProcessOutput with nothing to give has no INCOMPLETE flag (%#lx)", ob.dwStatus);
        EXPECT(IMediaObject_ProcessOutput(dmo, 0, 2, &ob, &status), E_INVALIDARG);
        EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, NULL, &status), E_POINTER);
        ob.pBuffer = NULL;
        EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), E_INVALIDARG);
        EXPECT(IMediaObject_ProcessOutput(dmo, DMO_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER, 1, &ob, &status), S_FALSE);
        mb_Release(&ob_buf->iface);
    }

    flags = 0;
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), S_OK);
    CHECKF(flags == DMO_INPUT_STATUSF_ACCEPT_DATA, "an idle transform accepts data (%#lx)", flags);
    EXPECT(IMediaObject_GetInputStatus(dmo, 1, &flags), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, NULL), E_POINTER);
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), S_OK);
    CHECKF(flags == MFT_INPUT_STATUS_ACCEPT_DATA, "the MFT's GetInputStatus agrees (%#lx)", flags);

    EXPECT(IMediaObject_ProcessInput(dmo, 1, &ib->iface, 0, 0, 0), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, NULL, 0, 0, 0), E_POINTER);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, DMO_INPUT_DATA_BUFFERF_TIME | DMO_INPUT_DATA_BUFFERF_TIMELENGTH, 0, 10000000), S_OK);

    /* a second buffer is refused exactly when the input status says so */
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), S_OK);
    hr = IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0);
    CHECKF((flags & DMO_INPUT_STATUSF_ACCEPT_DATA) ? hr == S_OK : hr == DMO_E_NOTACCEPTING,
            "a second ProcessInput agrees with GetInputStatus (status %#lx, hr %#lx)", flags, (unsigned long)hr);
    if (hr == S_OK) pushed++;

    EXPECT(IMediaObject_Discontinuity(dmo, 1), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_Discontinuity(dmo, 0), S_OK);
    calls = bad = 0; peak = 0;
    total = drain_output(0x4000, &calls, &bad, &peak);
    CHECKF(total % 4 == 0 && total >= pushed * 2 * IN_FRAMES * 4 * 9 / 10 && total <= pushed * 2 * IN_FRAMES * 4 * 11 / 10,
            "resampling %d frames of 22050 Hz gives twice as many 44100 Hz frames (%lu bytes in %d calls, %d buffers)", IN_FRAMES, total, calls, pushed);
    CHECKF(peak > 10000 && peak < 20000, "the resampled 16-bit data holds the signal (peak %d)", peak);
    CHECKF(bad == 0, "every output status is 0 and the last call has no INCOMPLETE flag (%d)", bad);
    CHECKF(calls > 1, "the output arrives in several 16 KB buffers (%d calls)", calls);

    /* discard: input is consumed without a buffer */
    ib->length = IN_FRAMES * 8;
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), S_OK);
    EXPECT(IMediaObject_Discontinuity(dmo, 0), S_OK);
    ob.pBuffer = NULL; ob.dwStatus = 0xdeadbeef;
    hr = IMediaObject_ProcessOutput(dmo, DMO_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER, 1, &ob, &status);
    CHECKF(hr == S_OK && ob.pBuffer == NULL, "ProcessOutput with DISCARD_WHEN_NO_BUFFER consumes the output (%#lx)", hr);
    calls = bad = 0;
    total = drain_output(0x4000, &calls, &bad, NULL);
    CHECKF(total < 2 * IN_FRAMES * 4, "discarded output is not returned later (%lu bytes left)", total);

    /* flush drops what is pending */
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), S_OK);
    EXPECT(IMediaObject_Flush(dmo), S_OK);
    EXPECT(IMediaObject_Flush(dmo), S_OK);
    calls = bad = 0;
    total = drain_output(0x4000, &calls, &bad, NULL);
    CHECKF(total == 0, "Flush drops the pending input (%lu bytes after)", total);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), S_OK);
    CHECKF(flags == DMO_INPUT_STATUSF_ACCEPT_DATA, "the transform accepts data again after Flush (%#lx)", flags);

    /* MFT drain and flush messages */
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), S_OK);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), S_OK);
    calls = bad = 0; peak = 0;
    total = drain_output(0x4000, &calls, &bad, &peak);
    CHECKF(total >= 2 * IN_FRAMES * 4 * 9 / 10, "the MFT DRAIN message releases the pending output (%lu bytes)", total);

    /* a float output type works too */
    EXPECT(IMediaObject_SetOutputType(dmo, 0, &outf.mt, 0), S_OK);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align), S_OK);
    CHECKF(size == 8, "float output frame is 8 bytes (%lu)", size);

    /* changing the input type drops the output type */
    init_audio(&in, &P_SUB_FLOAT, 3, 2, IN_RATE * 2, 32, 0);
    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    check_current("GetOutputCurrentType after a new input type", 1, DMO_E_TYPE_NOT_SET, NULL);
    mb_Release(&ib->iface);
}

static void test_resampler(void)
{
    static const struct type_row inputs[] =
    {
        {1, 0, 1, DMO_E_INVALIDSTREAMINDEX}, {1, 0, 0, DMO_E_INVALIDSTREAMINDEX}, {1, 2, 1, DMO_E_INVALIDSTREAMINDEX},
        {0, 0, 1, S_OK, &P_MAJOR_AUDIO, &P_SUB_FLOAT, 0}, {0, 1, 1, S_OK, &P_MAJOR_AUDIO, &P_SUB_PCM, 0}, {0, 1, 0, S_OK},
        {0, 2, 1, DMO_E_NO_MORE_ITEMS}, {0, 2, 0, DMO_E_NO_MORE_ITEMS}, {0, 0xdeadbeef, 0, DMO_E_NO_MORE_ITEMS},
    };
    static const struct type_row outputs[] =
    {
        {1, 0, 1, DMO_E_INVALIDSTREAMINDEX},
        {0, 0, 1, S_OK, &P_MAJOR_AUDIO, &P_SUB_FLOAT, 0}, {0, 1, 1, S_OK, &P_MAJOR_AUDIO, &P_SUB_PCM, 0},
        {0, 2, 1, S_OK, &P_MAJOR_AUDIO, &P_SUB_FLOAT, 1}, {0, 3, 1, S_OK, &P_MAJOR_AUDIO, &P_SUB_PCM, 1},
        {0, 4, 1, DMO_E_NO_MORE_ITEMS}, {0, 4, 0, DMO_E_NO_MORE_ITEMS}, {0, 0xdeadbeef, 0, DMO_E_NO_MORE_ITEMS},
    };
    static const struct set_row in_rows[] =
    {
        {1, T_NULL, 0, DMO_E_INVALIDSTREAMINDEX}, {1, T_GOOD, 0, DMO_E_INVALIDSTREAMINDEX}, {1, T_GOOD, DMO_SET_TYPEF_CLEAR, DMO_E_INVALIDSTREAMINDEX},
        {0, T_NULL, DMO_SET_TYPEF_CLEAR, S_OK}, {0, T_NULL, DMO_SET_TYPEF_CLEAR | DMO_SET_TYPEF_TEST_ONLY, E_INVALIDARG},
        {0, T_NULL, DMO_SET_TYPEF_CLEAR | 4, E_INVALIDARG}, {0, T_NULL, 0, E_POINTER}, {0, T_NULL, DMO_SET_TYPEF_TEST_ONLY, E_POINTER},
        {0, T_NULL, 4, E_POINTER}, {0, T_BAD, 0, DMO_E_TYPE_NOT_ACCEPTED}, {0, T_BAD, DMO_SET_TYPEF_CLEAR, S_OK},
        {0, T_BAD, DMO_SET_TYPEF_TEST_ONLY, DMO_E_TYPE_NOT_ACCEPTED}, {0, T_BAD, 4, E_INVALIDARG},
        {0, T_WRONGSUB, 0, DMO_E_TYPE_NOT_ACCEPTED}, {0, T_VIDEO, 0, DMO_E_TYPE_NOT_ACCEPTED},
        {0, T_GOOD, DMO_SET_TYPEF_TEST_ONLY, S_OK}, {0, T_GOOD, 4, E_INVALIDARG}, {0, T_GOOD, DMO_SET_TYPEF_CLEAR, S_OK},
    };
    static const struct set_row out_rows[] =
    {
        {0, T_GOOD, 0, DMO_E_TYPE_NOT_SET}, {0, T_GOOD, DMO_SET_TYPEF_TEST_ONLY, DMO_E_TYPE_NOT_SET}, {0, T_BAD, 0, DMO_E_TYPE_NOT_ACCEPTED},
        {0, T_NULL, DMO_SET_TYPEF_CLEAR, S_OK}, {0, T_NULL, 0, E_POINTER},
    };
    static const struct set_row out_rows2[] =
    {
        {1, T_NULL, 0, DMO_E_INVALIDSTREAMINDEX}, {1, T_GOOD, 0, DMO_E_INVALIDSTREAMINDEX}, {1, T_GOOD, DMO_SET_TYPEF_CLEAR, DMO_E_INVALIDSTREAMINDEX},
        {0, T_NULL, 0, E_POINTER}, {0, T_NULL, DMO_SET_TYPEF_CLEAR | DMO_SET_TYPEF_TEST_ONLY, E_INVALIDARG}, {0, T_NULL, 4, E_POINTER},
        {0, T_BAD, 0, DMO_E_TYPE_NOT_ACCEPTED}, {0, T_BAD, 4, E_INVALIDARG}, {0, T_WRONGSUB, 0, DMO_E_TYPE_NOT_ACCEPTED},
        {0, T_VIDEO, 0, DMO_E_TYPE_NOT_ACCEPTED}, {0, T_GOOD, DMO_SET_TYPEF_TEST_ONLY, S_OK}, {0, T_GOOD, 4, E_INVALIDARG},
    };
    union amt in, out, wrong, video;

    g_input_stream_flags = 0;
    init_audio(&in, &P_SUB_FLOAT, 3, 2, IN_RATE, 32, 0);
    init_audio(&out, &P_SUB_PCM, 1, 2, 44100, 16, 0);
    init_audio(&wrong, &P_SUB_MP3, 0x55, 2, 44100, 16, 0);
    init_video(&video, &P_SUB_NV12, 0x3231564e, 12, 64, 64);

    test_streams();
    test_latency_lock_resources();
    test_lock();
    run_type_rows("GetInputType", 0, inputs, sizeof(inputs) / sizeof(*inputs));
    run_type_rows("GetOutputType", 1, outputs, sizeof(outputs) / sizeof(*outputs));
    check_current("GetInputCurrentType", 0, DMO_E_TYPE_NOT_SET, NULL);
    check_current("GetOutputCurrentType", 1, DMO_E_TYPE_NOT_SET, NULL);
    run_set_rows("SetInputType", 0, in_rows, sizeof(in_rows) / sizeof(*in_rows), &in.mt, &wrong.mt, &video.mt);
    run_set_rows("SetOutputType", 1, out_rows, sizeof(out_rows) / sizeof(*out_rows), &out.mt, &wrong.mt, &video.mt);
    check_current("GetInputCurrentType", 0, DMO_E_TYPE_NOT_SET, NULL);

    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    check_current("GetInputCurrentType", 0, DMO_E_TYPE_NOT_SET, &in.mt);
    check_current("GetOutputCurrentType", 1, DMO_E_TYPE_NOT_SET, NULL);
    run_set_rows("SetOutputType", 1, out_rows2, sizeof(out_rows2) / sizeof(*out_rows2), &out.mt, &wrong.mt, &video.mt);
    check_current("GetOutputCurrentType (only tested types)", 1, DMO_E_TYPE_NOT_SET, NULL);
    EXPECT(IMediaObject_SetOutputType(dmo, 0, &out.mt, 0), S_OK);
    check_current("GetOutputCurrentType", 1, DMO_E_TYPE_NOT_SET, &out.mt);
    EXPECT(IMediaObject_SetOutputType(dmo, 0, NULL, DMO_SET_TYPEF_CLEAR), S_OK);
    check_current("GetOutputCurrentType after CLEAR", 1, DMO_E_TYPE_NOT_SET, NULL);
    EXPECT(IMediaObject_SetInputType(dmo, 0, NULL, DMO_SET_TYPEF_CLEAR), S_OK);
    check_current("GetInputCurrentType after CLEAR", 0, DMO_E_TYPE_NOT_SET, NULL);

    test_mft_methods(1);
    test_resampler_flow();
    test_property_objects(1);
    {
        IUnknown *props;
        HRESULT hr = IMediaObject_QueryInterface(dmo, &P_IID_IWMResamplerProps, (void **)&props);
        if (hr == S_OK)
        {
            /* IWMResamplerProps: SetHalfFilterLength(LONG) is vtable slot 3, SetUserChannelMtx slot 4 */
            HRESULT (WINAPI *set_len)(IUnknown *, LONG) = ((void ***)props)[0][3];
            HRESULT (WINAPI *set_mtx)(IUnknown *, void *) = ((void ***)props)[0][4];
            float mtx[4] = {1, 0, 0, 1};
            CHECKF(set_len(props, 0) == E_INVALIDARG, "SetHalfFilterLength(0) is E_INVALIDARG");
            CHECKF(set_len(props, 61) == E_INVALIDARG, "SetHalfFilterLength(61) is E_INVALIDARG");
            CHECKF(set_len(props, 30) == S_OK, "SetHalfFilterLength(30) is S_OK");
            CHECKF(set_mtx(props, NULL) == E_POINTER, "SetUserChannelMtx(NULL) is E_POINTER");
            CHECKF(set_mtx(props, mtx) == S_OK, "SetUserChannelMtx is S_OK");
            IUnknown_Release(props);
        }
        else
            printf("note  IWMResamplerProps not offered (%#lx)\n", hr);
    }
}

/* ---- color converter --------------------------------------------------- */
#define CC_W 64
#define CC_H 64

static void test_colorconv(void)
{
    static const struct type_row inputs[] =
    {
        {1, 0, 1, DMO_E_INVALIDSTREAMINDEX}, {1, 0, 0, DMO_E_INVALIDSTREAMINDEX},
        {0, 0, 1, S_OK, &P_MAJOR_VIDEO, NULL, 0}, {0, 19, 1, S_OK, &P_MAJOR_VIDEO, NULL, 0}, {0, 19, 0, S_OK},
        {0, 20, 1, DMO_E_NO_MORE_ITEMS}, {0, 20, 0, DMO_E_NO_MORE_ITEMS}, {0, 0xdeadbeef, 0, DMO_E_NO_MORE_ITEMS},
    };
    static const struct set_row in_rows[] =
    {
        {1, T_NULL, 0, DMO_E_INVALIDSTREAMINDEX}, {1, T_GOOD, DMO_SET_TYPEF_CLEAR, DMO_E_INVALIDSTREAMINDEX},
        {0, T_NULL, DMO_SET_TYPEF_CLEAR, S_OK}, {0, T_NULL, DMO_SET_TYPEF_CLEAR | DMO_SET_TYPEF_TEST_ONLY, E_INVALIDARG},
        {0, T_NULL, 0, E_POINTER}, {0, T_NULL, DMO_SET_TYPEF_TEST_ONLY, E_POINTER}, {0, T_BAD, 0, DMO_E_TYPE_NOT_ACCEPTED},
        {0, T_BAD, DMO_SET_TYPEF_CLEAR, S_OK}, {0, T_BAD, 4, E_INVALIDARG}, {0, T_VIDEO, 0, DMO_E_TYPE_NOT_ACCEPTED},
        {0, T_GOOD, DMO_SET_TYPEF_TEST_ONLY, S_OK}, {0, T_GOOD, 4, E_INVALIDARG},
    };
    static const struct set_row out_before[] =
    {
        {0, T_GOOD, 0, DMO_E_TYPE_NOT_SET}, {0, T_NULL, 0, E_POINTER}, {0, T_BAD, 0, DMO_E_TYPE_NOT_ACCEPTED},
    };
    static const struct set_row out_after[] =
    {
        {1, T_GOOD, 0, DMO_E_INVALIDSTREAMINDEX}, {0, T_NULL, 0, E_POINTER}, {0, T_BAD, 0, DMO_E_TYPE_NOT_ACCEPTED},
        {0, T_WRONGSUB, 0, DMO_E_TYPE_NOT_ACCEPTED}, {0, T_GOOD, DMO_SET_TYPEF_TEST_ONLY, S_OK}, {0, T_GOOD, 4, E_INVALIDARG},
    };
    union amt in, out, audio;
    struct mbuf *ib, *ob_buf;
    DMO_OUTPUT_DATA_BUFFER ob;
    DWORD size, look, align, status, flags;
    DMO_MEDIA_TYPE t;
    HRESULT hr;
    int i, ok;

    g_input_stream_flags = 0;
    init_video(&in, &P_SUB_NV12, 0x3231564e, 12, CC_W, CC_H);
    init_video(&out, &P_SUB_RGB32, BI_RGB, 32, CC_W, CC_H);
    init_audio(&audio, &P_SUB_PCM, 1, 2, 44100, 16, 0);

    test_streams();
    test_latency_lock_resources();
    test_lock();
    test_mft_methods(0);
    run_type_rows("GetInputType", 0, inputs, sizeof(inputs) / sizeof(*inputs));
    EXPECT(IMediaObject_GetOutputType(dmo, 1, 0, NULL), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputType(dmo, 0, 16, NULL), DMO_E_NO_MORE_ITEMS);
    memset(&t, 0, sizeof(t));
    EXPECT(IMediaObject_GetOutputType(dmo, 0, 0, &t), S_OK);
    CHECKF(IsEqualGUID(&t.majortype, &P_MAJOR_VIDEO) && !t.pbFormat, "without an input type the output types have no format block");
    check_current("GetInputCurrentType", 0, DMO_E_TYPE_NOT_SET, NULL);
    check_current("GetOutputCurrentType", 1, DMO_E_TYPE_NOT_SET, NULL);
    run_set_rows("SetInputType", 0, in_rows, sizeof(in_rows) / sizeof(*in_rows), &in.mt, &audio.mt, &audio.mt);
    run_set_rows("SetOutputType", 1, out_before, sizeof(out_before) / sizeof(*out_before), &out.mt, &audio.mt, &audio.mt);

    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_Flush(dmo), DMO_E_TYPE_NOT_SET);

    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    check_current("GetInputCurrentType", 0, DMO_E_TYPE_NOT_SET, &in.mt);
    /* now the output types carry the input's size */
    ok = 1;
    for (i = 0; i < 16; i++)
    {
        memset(&t, 0, sizeof(t));
        if (IMediaObject_GetOutputType(dmo, 0, i, &t) != S_OK || !t.pbFormat || t.cbFormat < sizeof(VIDEOINFOHEADER)
                || ((VIDEOINFOHEADER *)t.pbFormat)->bmiHeader.biWidth != CC_W || ((VIDEOINFOHEADER *)t.pbFormat)->bmiHeader.biHeight == 0)
            ok = 0;
        CoTaskMemFree(t.pbFormat);
    }
    CHECKF(ok, "with an input type set, all 16 output types carry its size in a VIDEOINFOHEADER");
    run_set_rows("SetOutputType", 1, out_after, sizeof(out_after) / sizeof(*out_after), &out.mt, &audio.mt, &audio.mt);
    EXPECT(IMediaObject_SetOutputType(dmo, 0, &out.mt, 0), S_OK);
    check_current("GetOutputCurrentType", 1, DMO_E_TYPE_NOT_SET, &out.mt);

    size = look = align = 0xdeadbeef;
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align), S_OK);
    CHECKF(size == CC_W * CC_H * 3 / 2 && align == 1 && look == 0, "input size info is one NV12 frame (%lu, %lu, %lu)", size, look, align);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align), S_OK);
    CHECKF(size == CC_W * CC_H * 4 && align == 1, "output size info is one RGB32 frame (%lu, %lu)", size, align);

    ib = mb_create(CC_W * CC_H * 3 / 2);
    memset(ib->data, 128, ib->max);
    ib->length = ib->max;
    ob_buf = mb_create(CC_W * CC_H * 4);
    ob.pBuffer = &ob_buf->iface; ob.dwStatus = 0xdeadbeef; ob.rtTimestamp = ob.rtTimelength = 0;
    EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), S_FALSE);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, DMO_INPUT_DATA_BUFFERF_TIME | DMO_INPUT_DATA_BUFFERF_TIMELENGTH, 5000000, 400000), S_OK);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), S_OK);
    hr = IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0);
    CHECKF((flags & DMO_INPUT_STATUSF_ACCEPT_DATA) ? hr == S_OK : hr == DMO_E_NOTACCEPTING,
            "a second ProcessInput agrees with GetInputStatus (status %#lx, hr %#lx)", flags, (unsigned long)hr);
    EXPECT(IMediaObject_Discontinuity(dmo, 0), S_OK);
    ob_buf->length = 0;
    status = 0xdeadbeef;
    hr = IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status);
    CHECKF(hr == S_OK, "ProcessOutput gives the converted frame (%#lx)", hr);
    CHECKF(status == 0, "ProcessOutput's status is 0 (%#lx)", status);
    CHECKF(ob_buf->length == CC_W * CC_H * 4, "the converted frame is %d bytes (%lu)", CC_W * CC_H * 4, ob_buf->length);
    CHECKF((ob.dwStatus & (DMO_OUTPUT_DATA_BUFFERF_TIME | DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH)) == (DMO_OUTPUT_DATA_BUFFERF_TIME | DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH)
            && ob.rtTimestamp == 5000000 && ob.rtTimelength == 400000,
            "the output carries the input's time stamp and length (status %#lx, %I64d, %I64d)", ob.dwStatus, ob.rtTimestamp, ob.rtTimelength);
    if (hr == S_OK && ob_buf->length >= 4)
    {
        BYTE *px = ob_buf->data + (CC_W * 20 + 20) * 4;
        CHECKF(abs(px[0] - 128) < 8 && abs(px[1] - 128) < 8 && abs(px[2] - 128) < 8, "mid grey NV12 converts to mid grey RGB32 (%u, %u, %u)", px[0], px[1], px[2]);
    }
    ob_buf->length = 0;
    EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), S_FALSE);

    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), S_OK);
    EXPECT(IMediaObject_Flush(dmo), S_OK);
    EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), S_FALSE);

    EXPECT(IMediaObject_SetInputType(dmo, 0, NULL, DMO_SET_TYPEF_CLEAR), S_OK);
    check_current("GetInputCurrentType after CLEAR", 0, DMO_E_TYPE_NOT_SET, NULL);
    EXPECT(IMediaObject_Flush(dmo), DMO_E_TYPE_NOT_SET);
    mb_Release(&ib->iface);
    mb_Release(&ob_buf->iface);
    test_property_objects(0);
}

/* ---- WMA decoder ------------------------------------------------------- */
static void test_wma(void)
{
    union amt in, out;
    DWORD size, look, align, flags;
    HRESULT hr;
    IMFMediaType *mt;

    g_input_stream_flags = 0;
    init_audio(&in, &P_SUB_WMA2, 0x161, 2, 22050, 16, 10);
    ((WAVEFORMATEX *)in.buf.format)->nBlockAlign = 640;
    ((WAVEFORMATEX *)in.buf.format)->nAvgBytesPerSec = 2000;
    init_audio(&out, &P_SUB_PCM, 1, 2, 22050, 16, 0);

    test_streams();
    test_latency_lock_resources();
    test_lock();

    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_Flush(dmo), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, NULL, 0, 0, 0), E_POINTER);
    EXPECT(IMediaObject_ProcessInput(dmo, 1, NULL, 0, 0, 0), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_Discontinuity(dmo, 1), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_Discontinuity(dmo, 0), S_OK);

    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    size = look = align = 0xdeadbeef;
    hr = IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align);
    CHECKF(hr == S_OK && size == 640 && align == 1 && look == 0, "input size info is the block alignment 640, lookahead 0, alignment 1 (%lu, %lu, %lu)", size, look, align);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 1, &size, &look, &align), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, NULL, &look, &align), E_POINTER);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, NULL, &align), E_POINTER);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, NULL), E_POINTER);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), DMO_E_TYPE_NOT_SET); /* no output type yet: no transform */
    EXPECT(IMediaObject_SetOutputType(dmo, 0, &out.mt, 0), S_OK);
    flags = 0;
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &flags), S_OK);
    CHECKF(flags == DMO_INPUT_STATUSF_ACCEPT_DATA, "the decoder accepts data (%#lx)", flags);
    EXPECT(IMediaObject_GetInputStatus(dmo, 1, &flags), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, NULL), E_POINTER);
    {
        MFT_INPUT_STREAM_INFO ii;
        MFT_OUTPUT_STREAM_INFO oi;
        DWORD osize = 0, oalign = 0;

        EXPECT(IMFTransform_GetInputStreamInfo(mft, 0, &ii), S_OK);
        EXPECT(IMFTransform_GetOutputStreamInfo(mft, 0, &oi), S_OK);
        CHECKF(ii.cbSize == 640 && oi.cbSize == 8192, "the MFT's stream sizes follow the DMO types (%lu, %lu)", ii.cbSize, oi.cbSize);
        EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &osize, &oalign), S_OK);
        CHECKF(osize == 8192 && oalign == 1, "output size info is 8192 bytes, alignment 1 (%lu, %lu)", osize, oalign);
    }
    EXPECT(IMediaObject_Flush(dmo), S_OK);
    {
        struct mbuf *ob_buf = mb_create(8192);
        DMO_OUTPUT_DATA_BUFFER ob = {&ob_buf->iface, 0xdeadbeef, 0, 0};
        DWORD status = 0xdeadbeef;
        EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), S_FALSE);
        CHECKF(ob_buf->length == 0, "ProcessOutput with no input writes nothing");
        mb_Release(&ob_buf->iface);
    }

    /* changing the input type drops the output type; the same type keeps it */
    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    check_current("GetOutputCurrentType after the same input type again", 1, DMO_E_TYPE_NOT_SET, &out.mt);
    init_audio(&in, &P_SUB_WMA2, 0x161, 2, 44100, 16, 10);
    ((WAVEFORMATEX *)in.buf.format)->nBlockAlign = 640;
    ((WAVEFORMATEX *)in.buf.format)->nAvgBytesPerSec = 2000;
    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    check_current("GetOutputCurrentType after another input type", 1, DMO_E_TYPE_NOT_SET, NULL);

    /* the MFT side: input types, current types, status, messages */
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_GetInputStatus(mft, 1, &flags), MF_E_INVALIDSTREAMNUMBER);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), S_OK);
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), E_NOTIMPL);
    {
        int i;
        GUID sub;
        for (i = 0; i < 4; i++)
        {
            hr = IMFTransform_GetInputAvailableType(mft, 0, i, &mt);
            CHECKF(hr == S_OK, "MFT GetInputAvailableType(%d) is S_OK (%#lx)", i, hr);
            if (hr == S_OK)
            {
                CHECKF(SUCCEEDED(IMFMediaType_GetGUID(mt, &MF_MT_SUBTYPE, &sub)), "available input type %d has a subtype", i);
                IMFMediaType_Release(mt);
            }
        }
        EXPECT(IMFTransform_GetInputAvailableType(mft, 0, 4, &mt), MF_E_NO_MORE_TYPES);
    }
    EXPECT(IMFTransform_GetInputCurrentType(mft, 1, &mt), MF_E_INVALIDSTREAMNUMBER);
    hr = IMFTransform_GetInputCurrentType(mft, 0, &mt);
    CHECKF(hr == S_OK, "MFT GetInputCurrentType after a DMO SetInputType (%#lx)", hr);
    if (hr == S_OK)
    {
        UINT32 rate = 0;
        IMFMediaType_GetUINT32(mt, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        CHECKF(rate == 44100, "the MFT's current input type is the one set through the DMO (%u Hz)", rate);
        IMFMediaType_Release(mt);
    }
    EXPECT(IMFTransform_GetOutputCurrentType(mft, 0, &mt), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_GetOutputCurrentType(mft, 1, &mt), MF_E_INVALIDSTREAMNUMBER);
    {
        IMFMediaEvent *event;
        PROPVARIANT pv = {.vt = VT_EMPTY};
        if (MFCreateMediaEvent(MEEndOfStream, &GUID_NULL, S_OK, &pv, &event) == S_OK)
        {
            EXPECT(IMFTransform_ProcessEvent(mft, 0, event), E_NOTIMPL);
            IMFMediaEvent_Release(event);
        }
    }
    test_property_objects(1);
}

int main(int argc, char **argv)
{
    const GUID *clsid;
    const char *which = argc > 1 ? argv[1] : "resampler";
    HRESULT hr;

    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);

    if (!strcmp(which, "resampler")) clsid = &P_CLSID_Resampler;
    else if (!strcmp(which, "colorconv")) clsid = &P_CLSID_ColorConv;
    else if (!strcmp(which, "wma")) clsid = &P_CLSID_WMADec;
    else { printf("FAIL  unknown component %s\n", which); return 1; }

    hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMediaObject, (void **)&dmo);
    if (FAILED(hr))
    {
        printf("note  %s cannot be created (%#lx): skipped\n", which, hr);
        printf("RESULT: SKIP\n");
        return 77;
    }
    hr = IMediaObject_QueryInterface(dmo, &IID_IMFTransform, (void **)&mft);
    CHECKF(hr == S_OK, "IMFTransform is offered beside IMediaObject (%#lx)", hr);

    if (!strcmp(which, "resampler")) test_resampler();
    else if (!strcmp(which, "colorconv")) test_colorconv();
    else test_wma();

    IMFTransform_Release(mft);
    CHECKF(IMediaObject_Release(dmo) == 0, "the object is released");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
