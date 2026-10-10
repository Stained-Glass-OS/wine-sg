/* The remaining stubs of winegstreamer's video decoder (WMV), H.264 encoder, AAC
 * decoder and video processor objects (patches/sg/2910..2912), run by
 * test/wgvid-{wmvdec,h264enc,aacdec,vproc}-gate.sh:
 *
 *   wgvid-probe.exe wmvdec | h264enc | aacdec | vproc
 *
 * Table-driven: the WMV decoder's IMediaObject (stream flags, current types, size
 * info, type validation, latency, Lock, resources, ProcessInput / ProcessOutput
 * with a real WMV frame), IPropertyBag / IPropertyStore, the attribute objects;
 * the H.264 encoder's ICodecAPI (support, range, values, default, get / set with
 * validation, notification lists, settings stream); the AAC decoder's attributes
 * and messages; the video processor's output status. A component that cannot be
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
#include <codecapi.h>
#include <vfwmsgs.h>
#include <stdio.h>
#include <string.h>

#define G(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    DEFINE_GUID(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)
G(P_CLSID_WMVDec, 0x82d353df, 0x90bd, 0x4382, 0x8b, 0xc2, 0x3f, 0x61, 0x92, 0xb7, 0x6e, 0x34);
G(P_CLSID_H264Enc, 0x6ca50344, 0x051a, 0x4ded, 0x97, 0x79, 0xa4, 0x33, 0x05, 0x16, 0x5e, 0x35);
G(P_CLSID_AACDec, 0x32d186a7, 0x218f, 0x4c75, 0x88, 0x76, 0xdd, 0x77, 0x27, 0x3a, 0x89, 0x99);
G(P_CLSID_VProc, 0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0x0c, 0x44, 0x5c, 0x78, 0xc9, 0x82);
G(P_SUB_WMV1, 0x31564d57, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_NV12, 0x3231564e, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_RGB32, 0xe436eb7e, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
G(P_SUB_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_SUB_RAW_AAC1, 0x000000ff, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_MAJOR_VIDEO, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
G(P_FORMAT_VIDEO, 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

static IMediaObject *dmo;
static IMFTransform *mft;

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


/* ---- media types ------------------------------------------------------- */
union amt
{
    DMO_MEDIA_TYPE mt;
    struct { DMO_MEDIA_TYPE mt; BYTE format[sizeof(VIDEOINFOHEADER) + 64]; } buf;
};

static void init_vid(union amt *a, const GUID *sub, DWORD fourcc, WORD bits, LONG w, LONG h)
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

static IMFMediaType *mk_video(const GUID *sub, UINT w, UINT h)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, sub);
    IMFMediaType_SetUINT64(t, &MF_MT_FRAME_SIZE, (UINT64)w << 32 | h);
    return t;
}

/* the stream and attribute methods a fixed one input one output MFT answers E_NOTIMPL to */
static void test_mft_optional(const char *who)
{
    IMFAttributes *a;
    IMFMediaEvent *event;
    PROPVARIANT pv = {.vt = VT_EMPTY};
    DWORD in = 0xdeadbeef, out = 0xdeadbeef, flags;
    HRESULT hr;

    CHECKF(IMFTransform_GetStreamIDs(mft, 1, &in, 1, &out) == E_NOTIMPL, "%s: GetStreamIDs is E_NOTIMPL", who);
    CHECKF(IMFTransform_DeleteInputStream(mft, 0) == E_NOTIMPL, "%s: DeleteInputStream is E_NOTIMPL", who);
    CHECKF(IMFTransform_AddInputStreams(mft, 1, &in) == E_NOTIMPL, "%s: AddInputStreams is E_NOTIMPL", who);
    CHECKF(IMFTransform_SetOutputBounds(mft, 0, 0) == E_NOTIMPL, "%s: SetOutputBounds is E_NOTIMPL", who);
    CHECKF(IMFTransform_GetInputStreamAttributes(mft, 0, &a) == E_NOTIMPL, "%s: GetInputStreamAttributes is E_NOTIMPL", who);
    hr = MFCreateMediaEvent(MEEndOfStream, &GUID_NULL, S_OK, &pv, &event);
    if (hr == S_OK)
    {
        CHECKF(IMFTransform_ProcessEvent(mft, 0, event) == E_NOTIMPL, "%s: ProcessEvent is E_NOTIMPL", who);
        CHECKF(IMFTransform_ProcessEvent(mft, 1, event) == E_NOTIMPL, "%s: ProcessEvent(1) is E_NOTIMPL", who);
        IMFMediaEvent_Release(event);
    }
    (void)flags;
}

/* ================= WMV decoder ========================================= */
static const BYTE wmv_frame[] =
{
    0x09, 0xae, 0x8c, 0x1c, 0x01, 0x81, 0x81, 0xe0, 0xd0, 0x39, 0x42, 0x82, 0x53, 0xf0, 0x8a, 0x17,
    0xf8, 0xb6, 0x9c, 0x32, 0x5a, 0x67, 0xb4, 0xd1, 0x72, 0x9b, 0x48, 0xeb, 0x05, 0x73, 0xeb, 0x17,
    0xf5, 0x1e, 0xa2, 0xc5, 0xbf, 0x0f, 0xc3, 0xf0, 0xfc, 0x03, 0x08, 0x9b, 0x89, 0x50, 0xa8, 0xd0,
    0x0b, 0xa6, 0xcb, 0x60, 0x25, 0xd1, 0xc9, 0x35, 0xe0, 0x80, 0x13, 0x76, 0x48, 0xf7, 0x82, 0x23,
    0xa1, 0x45, 0xca, 0xb0, 0x5f, 0xdd, 0x2e, 0x0a, 0x7f, 0x5a, 0xa6, 0xe4, 0xff, 0x29, 0x32, 0x50,
    0x65, 0xd4, 0x41, 0x5a, 0x2a, 0xcb, 0x82, 0x50, 0x22, 0x83, 0x99, 0x70, 0x10, 0x48, 0x08, 0x08,
    0xd8, 0x4b, 0x04, 0x45, 0x75, 0xa6, 0x9b, 0x8b, 0x69, 0xf3, 0xe5, 0xb7, 0x6d, 0x22, 0xa1, 0xd4,
    0xf2, 0xdd, 0xc6, 0x0f, 0x81, 0x2f, 0xe3, 0x06, 0x03, 0x0c, 0x18, 0x41, 0x70, 0x62, 0x23, 0xc1,
    0x9c, 0x0d, 0xc1, 0x11, 0x32, 0x0c, 0x01, 0xd0, 0x30, 0x92, 0x80, 0xc4, 0x06, 0x83, 0x39, 0x17,
    0xf8, 0x7e, 0x1f, 0x87, 0xe0, 0x5c, 0x1c, 0xa2, 0x2c, 0x98, 0xd5, 0xc8, 0x34, 0x17, 0x77, 0x25,
    0x44, 0x9d, 0x97, 0x41, 0x27, 0x91, 0x40, 0x8c, 0x55, 0x91, 0x1b, 0x8b, 0xe5, 0xc8, 0xb1, 0x1b,
    0x76, 0xd8, 0x92, 0x4a, 0xe5, 0x24, 0x8a, 0xc2, 0x7a, 0x1e, 0x1f, 0xcb, 0x69, 0xfc, 0xf7, 0xc4,
    0xa3, 0x14, 0x02, 0x62, 0xee, 0xdd, 0xa2, 0x5b, 0xaa, 0xcc, 0x14, 0xf6, 0xd4, 0x8b, 0xbf, 0xea,
    0xc4, 0x2a, 0x09, 0x3e, 0x5c, 0x52, 0x7f, 0xd0, 0xbe, 0x0f, 0xa6, 0xf6, 0x10, 0xde, 0xf8, 0x94,
    0x27, 0x04, 0x8f, 0x64, 0x6e, 0x07, 0x07, 0xe0, 0x15, 0x02, 0x40, 0xe1, 0x3c, 0x1f, 0xc6, 0x0c,
    0x09, 0x18, 0x30, 0x63, 0x20, 0x80, 0x0c, 0x44, 0x40, 0x07, 0x83, 0x37, 0x0f, 0x82, 0x22, 0x5c,
    0x10, 0xc1, 0xdb, 0xfa, 0x28, 0x30, 0xfc, 0x3f, 0x0f, 0xc3, 0xf0, 0x2e, 0x0e, 0x5a, 0xbb, 0xd7,
    0x80, 0x10, 0x4d, 0x97, 0xde, 0x1c, 0xa2, 0x2c, 0x9a, 0x35, 0x67, 0x50, 0x73, 0xdd, 0x2c, 0x0a,
    0x3f, 0x08, 0xfe, 0x50, 0x64, 0x8f, 0x8b, 0x28, 0x11, 0x59, 0x60, 0x4a, 0x44, 0x50, 0x63, 0x00,
    0x02, 0x44, 0x44, 0x6c, 0x46, 0x7f, 0xdb, 0xb7, 0x29, 0xa2, 0xda, 0x7c, 0x7f, 0xf3, 0xed, 0xaa,
    0x01, 0xc0, 0x96, 0xc0, 0xe0, 0xfc, 0x02, 0xa0, 0x48, 0x1c, 0x27, 0xe0, 0x88, 0xe8, 0x06, 0x04,
    0x8c, 0x18, 0x39, 0x90, 0x40, 0x06, 0x22, 0x20, 0x03, 0x81, 0x9b, 0x87, 0xc1, 0x11, 0x1f, 0x8a,
    0x04, 0x30, 0x76, 0xbe, 0x8a, 0x8c, 0x3f, 0x0f, 0xc3, 0xf0, 0xfc, 0x0b, 0x89, 0x7e, 0x38, 0x95,
    0x4e, 0xfc, 0x11, 0x2a, 0x6a, 0xf9, 0xa1, 0xcb, 0x5b, 0x7a, 0xf0, 0x02, 0x09, 0xb2, 0xfb, 0xf3,
    0x13, 0x98, 0xa2, 0xd5, 0x10, 0x4e, 0xba, 0x5c, 0x14, 0xfe, 0x86, 0x4b, 0x73, 0xf2, 0x93, 0x24,
    0x3c, 0x5d, 0x45, 0xd2, 0xcb, 0x2e, 0x09, 0x40, 0x8a, 0x0e, 0x60, 0x10, 0x48, 0x08, 0x0d, 0x80,
    0xcb, 0xc3, 0xf9, 0x4a, 0xc6, 0x4c, 0x22, 0xd8, 0x0c, 0x38, 0x65, 0x53, 0x8c, 0x1f, 0x02, 0x5f,
    0xc6, 0x0c, 0x06, 0x18, 0x30, 0x82, 0xe0, 0xc4, 0x47, 0x83, 0x38, 0x1b, 0x82, 0x22, 0x64, 0x18,
    0x03, 0xa0, 0x61, 0x25, 0x01, 0x88, 0x0d, 0x06, 0x72, 0x2f, 0xf0, 0xfc, 0x3f, 0x0f, 0xc0, 0xb8,
    0x39, 0x44, 0x59, 0x34, 0x6a, 0x4a, 0x5c, 0xa2, 0x55, 0x3b, 0xf0, 0x44, 0xa9, 0xab, 0xe6, 0xb1,
    0x56, 0x44, 0x6e, 0x2f, 0x97, 0x22, 0xcf, 0x6a, 0x4d, 0xb1, 0x24, 0x95, 0xca, 0x49, 0x15, 0x84,
    0xf4, 0x3c, 0x3f, 0x96, 0xd3, 0xf9, 0xef, 0x89, 0x46, 0x28, 0x04, 0xc5, 0x83, 0x5a, 0x40, 0x59,
    0xdc, 0xf7, 0x2a, 0x5a, 0x69, 0x82, 0x8a, 0x54, 0xdb, 0x90, 0xb0, 0x59, 0x8d, 0x0b, 0x72, 0x85,
    0xcf, 0x44, 0x85, 0x81, 0xf3, 0xdb, 0x98, 0xe2, 0xb7, 0xbe, 0x24, 0x50, 0x9f, 0x30, 0x1e, 0xc8,
    0xd8, 0x18, 0x1f, 0x50, 0x94, 0x07, 0x28, 0x94, 0x10, 0x68, 0x94, 0x22, 0x44, 0x94, 0x70, 0x3a,
    0x0c, 0x0f, 0xa9, 0xc1, 0x11, 0x25, 0x72, 0x2a, 0xd3, 0xc1, 0x6b, 0x7d, 0x5c, 0x9c, 0xff, 0x56,
    0x90, 0x33, 0xd6, 0x90, 0x72, 0xb0, 0x56, 0xd2, 0x20, 0x1e, 0xdc, 0xb4, 0x9b, 0x06, 0xfc, 0x3f,
    0x0f, 0xc3, 0xf0, 0x3a, 0xc1, 0x90, 0x41, 0x05, 0x18, 0x41, 0x04, 0x10, 0xf8, 0x1c, 0x0d, 0x97,
    0x82, 0x28, 0x22, 0x0e, 0x41, 0xd6, 0x07, 0x59, 0x33, 0x0a, 0x04, 0x1f, 0x0e, 0x1b, 0x07, 0x0e,
    0x98, 0x6b, 0xbd, 0x2c, 0xe1, 0x5f, 0x78, 0x8e, 0x70, 0xff, 0x87, 0x24, 0x93, 0x24, 0x92, 0x26,
    0x9c, 0x72, 0x56, 0x5e, 0xac, 0x7f, 0xfa, 0xac, 0xbd, 0x5f, 0xcb, 0xd5, 0xd6, 0xaa, 0xb5, 0x7f,
    0xbf, 0xfd, 0xcc, 0x93, 0x7f, 0xfd, 0xb7, 0x26, 0x10, 0x00,
};

static void test_streams(DWORD in_flags, DWORD out_flags)
{
    static const struct { DWORD index; HRESULT want; } rows[] = {{0, S_OK}, {1, DMO_E_INVALIDSTREAMINDEX}, {0xdeadbeef, DMO_E_INVALIDSTREAMINDEX}};
    DWORD in, out, flags;
    int i;

    in = out = 0xdeadbeef;
    EXPECT(IMediaObject_GetStreamCount(dmo, &in, &out), S_OK);
    CHECKF(in == 1 && out == 1, "GetStreamCount gives 1 input and 1 output (%lu, %lu)", in, out);
    for (i = 0; i < 3; i++)
    {
        flags = 0xdeadbeef;
        CHECKF(IMediaObject_GetInputStreamInfo(dmo, rows[i].index, &flags) == rows[i].want, "GetInputStreamInfo(%lu) is %#lx", rows[i].index, (unsigned long)rows[i].want);
        CHECKF(rows[i].want != S_OK || flags == in_flags, "GetInputStreamInfo(%lu) flags are %#lx (got %#lx)", rows[i].index, in_flags, flags);
        CHECKF(rows[i].want == S_OK || flags == 0xdeadbeef, "GetInputStreamInfo(%lu) failing leaves the flags alone", rows[i].index);
        flags = 0xdeadbeef;
        CHECKF(IMediaObject_GetOutputStreamInfo(dmo, rows[i].index, &flags) == rows[i].want, "GetOutputStreamInfo(%lu) is %#lx", rows[i].index, (unsigned long)rows[i].want);
        CHECKF(rows[i].want != S_OK || flags == out_flags, "GetOutputStreamInfo(%lu) flags are %#lx (got %#lx)", rows[i].index, out_flags, flags);
        CHECKF(rows[i].want == S_OK || flags == 0xdeadbeef, "GetOutputStreamInfo(%lu) failing leaves the flags alone", rows[i].index);
    }
    EXPECT(IMediaObject_GetInputStreamInfo(dmo, 0, NULL), E_POINTER);
    EXPECT(IMediaObject_GetOutputStreamInfo(dmo, 0, NULL), E_POINTER);
}

static void test_latency_resources(void)
{
    REFERENCE_TIME lat = 0x1234;

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

static void check_current(const char *what, int output, const GUID *sub, LONG w, LONG h)
{
    DMO_MEDIA_TYPE t;
    HRESULT hr;
    const VIDEOINFOHEADER *v;

    memset(&t, 0, sizeof(t));
    hr = output ? IMediaObject_GetOutputCurrentType(dmo, 0, &t) : IMediaObject_GetInputCurrentType(dmo, 0, &t);
    CHECKF(hr == S_OK, "%s is S_OK (%#lx)", what, (unsigned long)hr);
    if (hr != S_OK) return;
    v = (const VIDEOINFOHEADER *)t.pbFormat;
    CHECKF(IsEqualGUID(&t.majortype, &P_MAJOR_VIDEO) && IsEqualGUID(&t.subtype, sub), "%s returns the major type and subtype set", what);
    CHECKF(IsEqualGUID(&t.formattype, &P_FORMAT_VIDEO) && v && t.cbFormat == sizeof(*v), "%s returns a VIDEOINFOHEADER (%lu bytes)", what, t.cbFormat);
    CHECKF(v && v->bmiHeader.biWidth == w && v->bmiHeader.biHeight == h, "%s returns the size set (%ldx%ld)", what,
            v ? (long)v->bmiHeader.biWidth : 0, v ? (long)v->bmiHeader.biHeight : 0);
    if (v) ((VIDEOINFOHEADER *)t.pbFormat)->bmiHeader.biWidth = 7; /* the copy is the caller's */
    CoTaskMemFree(t.pbFormat);
}

static void test_wmv_types_before(void)
{
    DMO_MEDIA_TYPE t;
    DWORD size, look, align;
    IMFAttributes *a;

    memset(&t, 0, sizeof(t));
    EXPECT(IMediaObject_GetInputCurrentType(dmo, 0, &t), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetOutputCurrentType(dmo, 0, &t), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetInputCurrentType(dmo, 1, &t), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputCurrentType(dmo, 1, &t), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputCurrentType(dmo, 1, NULL), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputCurrentType(dmo, 1, NULL), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputCurrentType(dmo, 0, NULL), E_POINTER);
    EXPECT(IMediaObject_GetOutputCurrentType(dmo, 0, NULL), E_POINTER);

    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 1, &size, &look, &align), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, NULL, &look, &align), E_POINTER);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, NULL, &align), E_POINTER);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, NULL), E_POINTER);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 1, NULL, NULL), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, NULL, NULL), E_POINTER);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, NULL), E_POINTER);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, NULL, &align), E_POINTER);
    EXPECT(IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align), DMO_E_TYPE_NOT_SET);

    EXPECT(IMediaObject_GetOutputType(dmo, 0, 0, NULL), S_OK);
    EXPECT(IMediaObject_GetOutputType(dmo, 0, 0, &t), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_Flush(dmo), DMO_E_TYPE_NOT_SET);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, NULL, 0, 0, 0), E_POINTER);
    EXPECT(IMediaObject_ProcessInput(dmo, 1, NULL, 0, 0, 0), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMFTransform_GetAttributes(mft, &a), S_OK);
    IMFAttributes_Release(a);
}

/* input type validation rows: the compressed type's bitmap header */
enum { M_GOOD, M_SIZE0, M_SIZE1, M_SIZEBIG, M_SIZE44, M_SIZE44SHORT, M_W0, M_WNEG, M_W4097, M_W4096, M_H0, M_H4097, M_H4096, M_HNEG4096,
       M_COMP0, M_COMP1, M_COMP2, M_COMPBIG, M_COUNT };
static const char *const mut_names[M_COUNT] = {"good", "biSize 0", "biSize 1", "biSize 0xdeadbeef", "biSize 44 with room", "biSize 44 without room",
        "width 0", "width -1", "width 4097", "width 4096", "height 0", "height 4097", "height 4096", "height -4096",
        "compression 0", "compression 1", "compression 2", "compression 0xdeadbeef"};
static const HRESULT mut_want[M_COUNT] =
{
    S_OK, DMO_E_TYPE_NOT_ACCEPTED, DMO_E_TYPE_NOT_ACCEPTED, DMO_E_TYPE_NOT_ACCEPTED, S_OK, DMO_E_TYPE_NOT_ACCEPTED,
    DMO_E_TYPE_NOT_ACCEPTED, DMO_E_TYPE_NOT_ACCEPTED, DMO_E_TYPE_NOT_ACCEPTED, S_OK, DMO_E_TYPE_NOT_ACCEPTED, DMO_E_TYPE_NOT_ACCEPTED, S_OK, S_OK,
    DMO_E_TYPE_NOT_ACCEPTED, S_OK, S_OK, S_OK,
};

static void test_wmv_input_validation(void)
{
    union amt in;
    int m;

    for (m = 0; m < M_COUNT; m++)
    {
        VIDEOINFOHEADER *v;
        init_vid(&in, &P_SUB_WMV1, 0x31564d57, 24, 96, 96);
        v = (VIDEOINFOHEADER *)in.buf.format;
        switch (m)
        {
        case M_SIZE0: v->bmiHeader.biSize = 0; break;
        case M_SIZE1: v->bmiHeader.biSize = 1; break;
        case M_SIZEBIG: v->bmiHeader.biSize = 0xdeadbeef; break;
        case M_SIZE44: v->bmiHeader.biSize = 44; in.mt.cbFormat += 4; break;
        case M_SIZE44SHORT: v->bmiHeader.biSize = 44; break;
        case M_W0: v->bmiHeader.biWidth = 0; break;
        case M_WNEG: v->bmiHeader.biWidth = -1; break;
        case M_W4097: v->bmiHeader.biWidth = 4097; break;
        case M_W4096: v->bmiHeader.biWidth = 4096; break;
        case M_H0: v->bmiHeader.biHeight = 0; break;
        case M_H4097: v->bmiHeader.biHeight = 4097; break;
        case M_H4096: v->bmiHeader.biHeight = 4096; break;
        case M_HNEG4096: v->bmiHeader.biHeight = -4096; break;
        case M_COMP0: v->bmiHeader.biCompression = 0; break;
        case M_COMP1: v->bmiHeader.biCompression = 1; break;
        case M_COMP2: v->bmiHeader.biCompression = 2; break;
        case M_COMPBIG: v->bmiHeader.biCompression = 0xdeadbeef; break;
        }
        CHECKF(IMediaObject_SetInputType(dmo, 0, &in.mt, DMO_SET_TYPEF_TEST_ONLY) == mut_want[m], "SetInputType test-only with %s is %#lx",
                mut_names[m], (unsigned long)mut_want[m]);
    }
}

static void test_wmv_output_type_rows(void)
{
    DMO_MEDIA_TYPE t;
    HRESULT hr;
    const DWORD count = 13;

    EXPECT(IMediaObject_GetOutputType(dmo, 1, 0, NULL), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputType(dmo, 1, 0, &t), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputType(dmo, 1, count, &t), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_GetOutputType(dmo, 0, count, &t), DMO_E_NO_MORE_ITEMS);
    EXPECT(IMediaObject_GetOutputType(dmo, 0, count, NULL), S_OK);
    EXPECT(IMediaObject_GetOutputType(dmo, 0, 0xdeadbeef, NULL), S_OK);
    EXPECT(IMediaObject_GetOutputType(dmo, 0, count - 1, NULL), S_OK);
    memset(&t, 0, sizeof(t));
    hr = IMediaObject_GetOutputType(dmo, 0, 0, &t);
    CHECKF(hr == S_OK && IsEqualGUID(&t.subtype, &P_SUB_NV12), "output type 0 is NV12 (%#lx)", (unsigned long)hr);
    CoTaskMemFree(t.pbFormat);
}

static void test_wmv_sizes(const union amt *out, const union amt *rgb)
{
    DWORD size, look, align;
    HRESULT hr;

    size = look = align = 0xdeadbeef;
    hr = IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align);
    CHECKF(hr == S_OK && size == 96 * 96 * 3 / 2 && look == 0 && align == 1,
            "input size info without an output type is the 4:2:0 frame, lookahead 0, alignment 1 (%lu, %lu, %lu)", size, look, align);
    EXPECT(IMediaObject_GetInputSizeInfo(dmo, 1, &size, &look, &align), DMO_E_INVALIDSTREAMINDEX);

    EXPECT(IMediaObject_SetOutputType(dmo, 0, &out->mt, 0), S_OK);
    size = align = 0xdeadbeef;
    hr = IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align);
    CHECKF(hr == S_OK && size == 96 * 96 * 3 / 2 && align == 1, "NV12 output size info is 13824 bytes, alignment 1 (%lu, %lu)", size, align);
    size = look = align = 0xdeadbeef;
    hr = IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align);
    CHECKF(hr == S_OK && size == 96 * 96 * 3 / 2 && look == 0 && align == 1, "input size info follows the NV12 output (%lu, %lu, %lu)", size, look, align);

    EXPECT(IMediaObject_SetOutputType(dmo, 0, &rgb->mt, 0), S_OK);
    size = align = 0xdeadbeef;
    hr = IMediaObject_GetOutputSizeInfo(dmo, 0, &size, &align);
    CHECKF(hr == S_OK && size == 96 * 96 * 4 && align == 1, "RGB32 output size info is 36864 bytes (%lu, %lu)", size, align);
    size = look = align = 0xdeadbeef;
    hr = IMediaObject_GetInputSizeInfo(dmo, 0, &size, &look, &align);
    CHECKF(hr == S_OK && size == 96 * 96 * 4, "input size info follows the RGB32 output (%lu)", size);
    check_current("GetOutputCurrentType with RGB32", 1, &P_SUB_RGB32, 96, 96);
    EXPECT(IMediaObject_SetOutputType(dmo, 0, &out->mt, 0), S_OK);
}

static void test_wmv_data(const union amt *out)
{
    struct mbuf *ib = mb_create(sizeof(wmv_frame)), *ob_buf;
    DMO_OUTPUT_DATA_BUFFER ob;
    DWORD status;
    HRESULT hr;
    int distinct = 0, seen[256] = {0}, i;

    memcpy(ib->data, wmv_frame, sizeof(wmv_frame));
    ib->length = sizeof(wmv_frame);
    ob_buf = mb_create(96 * 96 * 3 / 2);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), S_OK);

    ob.pBuffer = &ob_buf->iface; ob.dwStatus = 0xdeadbeef; ob.rtTimestamp = 0xdeadbeef; ob.rtTimelength = 0xdeadbeef;
    status = 0xdeadbeef;
    hr = IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status);
    CHECKF(hr == S_OK, "ProcessOutput gives the decoded frame (%#lx)", (unsigned long)hr);
    CHECKF(ob_buf->length == 96 * 96 * 3 / 2, "the frame is 13824 bytes (%lu)", ob_buf->length);
    CHECKF(ob.dwStatus == (DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT | DMO_OUTPUT_DATA_BUFFERF_TIME | DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH),
            "the frame is a sync point with a time and a length (%#lx)", ob.dwStatus);
    for (i = 0; i < 96 * 96; i++) if (!seen[ob_buf->data[i]]++) distinct++;
    CHECKF(distinct > 20, "the decoded luma is a picture, not a flat fill (%d levels)", distinct);

    /* the same frame again, flushed before it is read: nothing to output */
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), S_OK);
    EXPECT(IMediaObject_Flush(dmo), S_OK);
    EXPECT(IMediaObject_Flush(dmo), S_OK);
    ob_buf->length = 0; ob.dwStatus = 0xdeadbeef;
    hr = IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status);
    CHECKF(hr == S_FALSE, "ProcessOutput without data is S_FALSE (%#lx)", (unsigned long)hr);
    CHECKF(ob_buf->length == 0 && ob.dwStatus == 0, "ProcessOutput without data writes nothing (%lu, %#lx)", ob_buf->length, ob.dwStatus);

    EXPECT(IMediaObject_ProcessOutput(dmo, 0, 2, &ob, &status), E_INVALIDARG);
    EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, NULL, &status), E_POINTER);
    ob.pBuffer = NULL;
    EXPECT(IMediaObject_ProcessOutput(dmo, 0, 1, &ob, &status), E_INVALIDARG);
    EXPECT(IMediaObject_ProcessInput(dmo, 0, &ib->iface, 0, 0, 0), S_OK);
    hr = IMediaObject_ProcessOutput(dmo, DMO_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER, 1, &ob, &status);
    CHECKF(hr == S_OK && ob.pBuffer == NULL, "a NULL buffer with the discard flag is read and dropped (%#lx)", (unsigned long)hr);

    EXPECT(IMediaObject_Discontinuity(dmo, 1), DMO_E_INVALIDSTREAMINDEX);
    EXPECT(IMediaObject_Discontinuity(dmo, 0), S_OK);
    status = 0;
    EXPECT(IMediaObject_GetInputStatus(dmo, 0, &status), S_OK);
    CHECKF(status == DMO_INPUT_STATUSF_ACCEPT_DATA, "the decoder accepts data (%#lx)", status);
    mb_Release(&ob_buf->iface);
    mb_Release(&ib->iface);
}

static void test_wmv_mft(void)
{
    static const struct { const GUID *key; UINT32 value; const char *name; } rows[] =
    {
        {&MF_LOW_LATENCY, 0, "MF_LOW_LATENCY"}, {&MF_SA_D3D_AWARE, 1, "MF_SA_D3D_AWARE"},
        {&MF_SA_D3D11_AWARE, 1, "MF_SA_D3D11_AWARE"}, {&MFT_DECODER_EXPOSE_OUTPUT_TYPES_IN_NATIVE_ORDER, 0, "MFT_DECODER_EXPOSE_OUTPUT_TYPES_IN_NATIVE_ORDER"},
    };
    IMFAttributes *a, *b;
    UINT32 v, count;
    DWORD flags;
    MFT_INPUT_STREAM_INFO ii;
    MFT_OUTPUT_STREAM_INFO oi;
    HRESULT hr;
    int i;

    EXPECT(IMFTransform_GetAttributes(mft, NULL), E_POINTER);
    EXPECT(IMFTransform_GetAttributes(mft, &a), S_OK);
    for (i = 0; i < 4; i++)
    {
        v = 0xdeadbeef;
        hr = IMFAttributes_GetUINT32(a, rows[i].key, &v);
        CHECKF(hr == S_OK && v == rows[i].value, "the transform attribute %s is %u (%#lx, %u)", rows[i].name, rows[i].value, (unsigned long)hr, v);
    }
    EXPECT(IMFTransform_GetAttributes(mft, &b), S_OK);
    CHECKF(a == b, "GetAttributes returns the same object each time");
    IMFAttributes_Release(b);
    CHECKF(IMFAttributes_Release(a) == 1, "the attributes belong to the transform");

    EXPECT(IMFTransform_GetOutputStreamAttributes(mft, 0, &a), S_OK);
    count = 0xdeadbeef;
    IMFAttributes_GetCount(a, &count);
    CHECKF(count == 0, "the output stream attributes are empty (%u)", count);
    EXPECT(IMFTransform_GetOutputStreamAttributes(mft, 0, &b), S_OK);
    CHECKF(a == b, "GetOutputStreamAttributes returns the same object each time");
    IMFAttributes_Release(b);
    IMFAttributes_Release(a);
    EXPECT(IMFTransform_GetOutputStreamAttributes(mft, 0, NULL), E_POINTER);
    EXPECT(IMFTransform_GetOutputStreamAttributes(mft, 1, &a), MF_E_INVALIDSTREAMNUMBER);
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), E_NOTIMPL);
    test_mft_optional("wmvdec");

    EXPECT(IMFTransform_GetInputStreamInfo(mft, 0, &ii), S_OK);
    EXPECT(IMFTransform_GetOutputStreamInfo(mft, 0, &oi), S_OK);
    CHECKF(ii.dwFlags == 0, "the MFT input stream flags are 0 (%#lx)", ii.dwFlags);
    CHECKF(oi.dwFlags == (MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_OUTPUT_STREAM_DISCARDABLE),
            "the MFT output stream flags are whole samples, single sample, discardable (%#lx)", oi.dwFlags);
}

static void test_wmvdec(void)
{
    union amt in, out, rgb;

    init_vid(&in, &P_SUB_WMV1, 0x31564d57, 24, 96, 96);
    init_vid(&out, &P_SUB_NV12, 0x3231564e, 12, 96, 96);
    init_vid(&rgb, &P_SUB_RGB32, 0, 32, 96, 96);

    test_streams(0, MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_OUTPUT_STREAM_DISCARDABLE);
    test_latency_resources();
    test_wmv_types_before();
    test_wmv_input_validation();

    EXPECT(IMediaObject_SetInputType(dmo, 0, &in.mt, 0), S_OK);
    check_current("GetInputCurrentType", 0, &P_SUB_WMV1, 96, 96);
    test_wmv_output_type_rows();
    test_wmv_sizes(&out, &rgb);
    check_current("GetOutputCurrentType", 1, &P_SUB_NV12, 96, 96);
    test_lock();
    test_wmv_data(&out);
    test_wmv_mft();
    test_property_objects(1);
    EXPECT(IMediaObject_SetInputType(dmo, 0, NULL, DMO_SET_TYPEF_CLEAR), S_OK);
    {
        DMO_MEDIA_TYPE t;
        EXPECT(IMediaObject_GetInputCurrentType(dmo, 0, &t), DMO_E_TYPE_NOT_SET);
    }
}

/* ================= H.264 encoder: ICodecAPI ============================= */
static ICodecAPI *api;

struct param
{
    const GUID *key;
    const char *name;
    VARTYPE vt;
    int enumerated;
    ULONG64 min, max, def;
    int derived;
};
#define P(k, vt, en, min, max, def, der) {&CODECAPI_##k, #k, vt, en, min, max, def, der}
static const struct param params[] =
{
    P(AVEncCommonRateControlMode, VT_UI4, 1, 0, 3, 0, 0),
    P(AVEncCommonQuality, VT_UI4, 0, 0, 100, 65, 0),
    P(AVEncCommonBufferSize, VT_UI4, 0, 0, 0xffffffff, 0, 2),
    P(AVEncCommonMaxBitRate, VT_UI4, 0, 0, 0xffffffff, 0, 0),
    P(AVEncCommonMeanBitRate, VT_UI4, 0, 0, 0xffffffff, 0, 1),
    P(AVEncCommonQualityVsSpeed, VT_UI4, 0, 0, 100, 33, 0),
    P(AVEncH264CABACEnable, VT_UI4, 1, 0, 1, 0, 0),
    P(AVEncH264PPSID, VT_UI4, 0, 0, 255, 0, 0),
    P(AVEncH264SPSID, VT_UI4, 0, 0, 31, 0, 0),
    P(AVEncMPVGOPSize, VT_UI4, 0, 0, 0xffffffff, 0, 0),
    P(AVEncMPVDefaultBPictureCount, VT_UI4, 0, 0, 2, 1, 0),
    P(AVEncVideoEncodeQP, VT_UI8, 0, 0, 51, 26, 0),
    P(AVEncVideoMaxQP, VT_UI4, 0, 0, 51, 51, 0),
    P(AVEncVideoMinQP, VT_UI4, 0, 0, 51, 0, 0),
    P(AVEncVideoMaxNumRefFrame, VT_UI4, 0, 0, 16, 2, 0),
};
#define NPARAMS ((int)(sizeof(params) / sizeof(params[0])))
enum { I_RC = 0, I_QUALITY = 1, I_BUF = 2, I_MAXBR = 3, I_MEANBR = 4, I_QVS = 5, I_CABAC = 6, I_MAXQP = 12, I_QP = 11 };

static ULONG64 var_value(const VARIANT *v)
{
    return V_VT(v) == VT_UI8 ? V_UI8(v) : V_UI4(v);
}

static void set_var(VARIANT *v, VARTYPE vt, ULONG64 value)
{
    VariantInit(v);
    V_VT(v) = vt;
    if (vt == VT_UI8) V_UI8(v) = value;
    else V_UI4(v) = (ULONG)value;
}

static void check_value(const char *what, int i, ULONG64 want)
{
    VARIANT v;
    HRESULT hr;

    memset(&v, 0xcd, sizeof(v));
    hr = ICodecAPI_GetValue(api, params[i].key, &v);
    CHECKF(hr == S_OK && V_VT(&v) == params[i].vt && var_value(&v) == want, "%s: %s is %I64u (hr %#lx, vt %u, value %I64u)", what, params[i].name,
            (unsigned long long)want, (unsigned long)hr, hr == S_OK ? V_VT(&v) : 0, hr == S_OK ? (unsigned long long)var_value(&v) : 0);
}

static void check_default(const char *what, int i, ULONG64 want)
{
    VARIANT v;
    HRESULT hr;

    memset(&v, 0xcd, sizeof(v));
    hr = ICodecAPI_GetDefaultValue(api, params[i].key, &v);
    CHECKF(hr == S_OK && V_VT(&v) == params[i].vt && var_value(&v) == want, "%s: the default of %s is %I64u (hr %#lx, value %I64u)", what, params[i].name,
            (unsigned long long)want, (unsigned long)hr, hr == S_OK ? (unsigned long long)var_value(&v) : 0);
}

static void check_all_defaults(const char *what, ULONG64 bitrate)
{
    int i;
    for (i = 0; i < NPARAMS; i++)
    {
        ULONG64 want = params[i].derived == 1 ? bitrate : params[i].derived == 2 ? bitrate * 3 / 8 : params[i].def;
        check_value(what, i, want);
        check_default(what, i, want);
    }
}

static int guid_list_has(const GUID *list, ULONG count, const GUID *key)
{
    ULONG i;
    for (i = 0; i < count; i++) if (IsEqualGUID(&list[i], key)) return 1;
    return 0;
}

static IStream *new_stream(void)
{
    IStream *s = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    return s;
}

static void rewind_stream(IStream *s)
{
    LARGE_INTEGER zero = {{0}};
    IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
}

static void stream_put(IStream *s, const void *data, ULONG len)
{
    ULONG w;
    IStream_Write(s, data, len, &w);
}

#pragma pack(push, 1)
struct entry { GUID key; ULONG vt; ULONGLONG value; };
#pragma pack(pop)

static void test_codecapi(void)
{
    static const GUID unsupported = {0xa4a0e93d, 0x4cbc, 0x444c, {0x89, 0xf4, 0x82, 0x6d, 0x31, 0x0e, 0x92, 0xa7}};
    VARIANT v, v2, v3, *values;
    GUID *list;
    ULONG count, i, n;
    HRESULT hr;
    int k;

    /* support, modifiability, pointers */
    for (k = 0; k < NPARAMS; k++)
    {
        CHECKF(ICodecAPI_IsSupported(api, params[k].key) == S_OK, "%s is supported", params[k].name);
        CHECKF(ICodecAPI_IsModifiable(api, params[k].key) == S_OK, "%s is modifiable", params[k].name);
    }
    EXPECT(ICodecAPI_IsSupported(api, &unsupported), S_FALSE);
    EXPECT(ICodecAPI_IsModifiable(api, &unsupported), S_FALSE);
    EXPECT(ICodecAPI_IsSupported(api, NULL), E_POINTER);
    EXPECT(ICodecAPI_IsModifiable(api, NULL), E_POINTER);

    /* values before an output type is set */
    check_all_defaults("before any type", 0);
    VariantInit(&v);
    EXPECT(ICodecAPI_GetValue(api, &unsupported, &v), E_NOTIMPL);
    EXPECT(ICodecAPI_GetValue(api, params[0].key, NULL), E_POINTER);
    EXPECT(ICodecAPI_GetValue(api, NULL, &v), E_POINTER);
    EXPECT(ICodecAPI_GetDefaultValue(api, &unsupported, &v), E_NOTIMPL);
    EXPECT(ICodecAPI_GetDefaultValue(api, params[0].key, NULL), E_POINTER);

    /* ranges and value lists */
    for (k = 0; k < NPARAMS; k++)
    {
        VariantInit(&v); VariantInit(&v2); VariantInit(&v3);
        hr = ICodecAPI_GetParameterRange(api, params[k].key, &v, &v2, &v3);
        if (params[k].enumerated)
            CHECKF(hr == VFW_E_CODECAPI_ENUMERATED, "GetParameterRange(%s) says it is enumerated (%#lx)", params[k].name, (unsigned long)hr);
        else
            CHECKF(hr == S_OK && V_VT(&v) == params[k].vt && var_value(&v) == params[k].min && V_VT(&v2) == params[k].vt
                    && var_value(&v2) == params[k].max && V_VT(&v3) == params[k].vt && var_value(&v3) == 1,
                    "GetParameterRange(%s) is %I64u..%I64u step 1 (hr %#lx)", params[k].name, (unsigned long long)params[k].min,
                    (unsigned long long)params[k].max, (unsigned long)hr);
        values = NULL; count = 0xdeadbeef;
        hr = ICodecAPI_GetParameterValues(api, params[k].key, &values, &count);
        if (!params[k].enumerated)
            CHECKF(hr == VFW_E_CODECAPI_LINEAR_RANGE, "GetParameterValues(%s) says it is a linear range (%#lx)", params[k].name, (unsigned long)hr);
        else
        {
            n = (ULONG)(params[k].max - params[k].min + 1);
            CHECKF(hr == S_OK && count == n && values, "GetParameterValues(%s) lists %lu values (hr %#lx, count %lu)", params[k].name, n, (unsigned long)hr, count);
            for (i = 0; hr == S_OK && i < count; i++)
                CHECKF(V_VT(&values[i]) == params[k].vt && var_value(&values[i]) == params[k].min + i, "%s value %lu is %I64u", params[k].name, i,
                        (unsigned long long)(params[k].min + i));
            if (hr == S_OK) CoTaskMemFree(values);
        }
    }
    EXPECT(ICodecAPI_GetParameterRange(api, &unsupported, &v, &v2, &v3), E_NOTIMPL);
    EXPECT(ICodecAPI_GetParameterRange(api, params[1].key, NULL, &v2, &v3), E_POINTER);
    EXPECT(ICodecAPI_GetParameterRange(api, NULL, &v, &v2, &v3), E_POINTER);
    EXPECT(ICodecAPI_GetParameterValues(api, &unsupported, &values, &count), E_NOTIMPL);
    EXPECT(ICodecAPI_GetParameterValues(api, params[0].key, NULL, &count), E_POINTER);
    EXPECT(ICodecAPI_GetParameterValues(api, params[0].key, &values, NULL), E_POINTER);

    /* the output type feeds the mean bit rate and the buffer size */
    {
        IMFMediaType *t = mk_video(&MFVideoFormat_H264, 96, 96);
        IMFMediaType_SetUINT64(t, &MF_MT_FRAME_RATE, (UINT64)30000 << 32 | 1001);
        IMFMediaType_SetUINT32(t, &MF_MT_AVG_BITRATE, 193540);
        IMFMediaType_SetUINT32(t, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        EXPECT(IMFTransform_SetOutputType(mft, 0, t, 0), S_OK);
        IMFMediaType_Release(t);
    }
    check_all_defaults("with the output type", 193540);

    /* SetValue rows: {parameter, type, value, wanted result} */
    {
        static const struct { int p; VARTYPE vt; ULONG64 value; HRESULT want; } rows[] =
        {
            {I_QUALITY, VT_UI4, 80, S_OK}, {I_QUALITY, VT_UI4, 101, E_INVALIDARG}, {I_QUALITY, VT_I4, 5, E_INVALIDARG},
            {I_QUALITY, VT_UI8, 5, E_INVALIDARG}, {I_QUALITY, VT_UI4, 0, S_OK}, {I_QUALITY, VT_UI4, 100, S_OK},
            {I_RC, VT_UI4, 3, S_OK}, {I_RC, VT_UI4, 4, E_INVALIDARG}, {I_CABAC, VT_UI4, 1, S_OK}, {I_CABAC, VT_UI4, 2, E_INVALIDARG},
            {I_QP, VT_UI8, 30, S_OK}, {I_QP, VT_UI4, 31, E_INVALIDARG}, {I_QP, VT_UI8, 52, E_INVALIDARG},
            {I_MEANBR, VT_UI4, 500000, S_OK}, {I_MAXQP, VT_UI4, 40, S_OK}, {I_MAXQP, VT_UI4, 52, E_INVALIDARG},
        };
        ULONG64 cur[NPARAMS];
        int r;
        for (k = 0; k < NPARAMS; k++)
        {
            VariantInit(&v);
            ICodecAPI_GetValue(api, params[k].key, &v);
            cur[k] = var_value(&v);
        }
        for (r = 0; r < (int)(sizeof(rows) / sizeof(rows[0])); r++)
        {
            set_var(&v, rows[r].vt, rows[r].value);
            hr = ICodecAPI_SetValue(api, params[rows[r].p].key, &v);
            CHECKF(hr == rows[r].want, "SetValue(%s, vt %u, %I64u) is %#lx (got %#lx)", params[rows[r].p].name, rows[r].vt,
                    (unsigned long long)rows[r].value, (unsigned long)rows[r].want, (unsigned long)hr);
            if (hr == S_OK) cur[rows[r].p] = rows[r].value;
            check_value("after SetValue", rows[r].p, cur[rows[r].p]);
        }
        check_default("after SetValue", I_MEANBR, 193540);
        check_default("after SetValue", I_QUALITY, 65);
        check_default("after SetValue", I_BUF, 193540 * 3 / 8);
        check_value("an untouched parameter", I_MAXBR, 0);
    }
    set_var(&v, VT_UI4, 1);
    EXPECT(ICodecAPI_SetValue(api, &unsupported, &v), E_NOTIMPL);
    EXPECT(ICodecAPI_SetValue(api, params[1].key, NULL), E_POINTER);
    EXPECT(ICodecAPI_SetValue(api, NULL, &v), E_POINTER);

    /* SetAllDefaults */
    EXPECT(ICodecAPI_SetAllDefaults(api), S_OK);
    check_all_defaults("after SetAllDefaults", 193540);

    /* the notification lists */
    set_var(&v, VT_UI4, 70);
    list = (GUID *)0x1; count = 0xdeadbeef;
    EXPECT(ICodecAPI_SetValueWithNotify(api, params[I_QUALITY].key, &v, &list, &count), S_OK);
    CHECKF(count == 1 && list && IsEqualGUID(&list[0], params[I_QUALITY].key), "SetValueWithNotify lists the parameter that changed (%lu)", count);
    if (count == 1 && list) CoTaskMemFree(list);
    list = (GUID *)0x1; count = 0xdeadbeef;
    EXPECT(ICodecAPI_SetValueWithNotify(api, params[I_QUALITY].key, &v, &list, &count), S_OK);
    CHECKF(count == 0 && !list, "SetValueWithNotify with the same value lists nothing (%lu)", count);
    set_var(&v, VT_UI4, 101);
    list = (GUID *)0x1; count = 0xdeadbeef;
    EXPECT(ICodecAPI_SetValueWithNotify(api, params[I_QUALITY].key, &v, &list, &count), E_INVALIDARG);
    check_value("after a refused SetValueWithNotify", I_QUALITY, 70);
    set_var(&v, VT_UI4, 1);
    EXPECT(ICodecAPI_SetValueWithNotify(api, &unsupported, &v, &list, &count), E_NOTIMPL);
    EXPECT(ICodecAPI_SetValueWithNotify(api, params[I_QUALITY].key, &v, NULL, &count), E_POINTER);
    EXPECT(ICodecAPI_SetValueWithNotify(api, params[I_QUALITY].key, &v, &list, NULL), E_POINTER);

    set_var(&v, VT_UI4, 40);
    ICodecAPI_SetValue(api, params[I_MAXQP].key, &v);
    list = NULL; count = 0xdeadbeef;
    EXPECT(ICodecAPI_SetAllDefaultsWithNotify(api, &list, &count), S_OK);
    CHECKF(count == 2 && list && guid_list_has(list, count, params[I_QUALITY].key) && guid_list_has(list, count, params[I_MAXQP].key),
            "SetAllDefaultsWithNotify lists the two parameters that changed (%lu)", count);
    if (count && list) CoTaskMemFree(list);
    check_all_defaults("after SetAllDefaultsWithNotify", 193540);
    list = (GUID *)0x1; count = 0xdeadbeef;
    EXPECT(ICodecAPI_SetAllDefaultsWithNotify(api, &list, &count), S_OK);
    CHECKF(count == 0 && !list, "SetAllDefaultsWithNotify with nothing changed lists nothing (%lu)", count);
    EXPECT(ICodecAPI_SetAllDefaultsWithNotify(api, NULL, &count), E_POINTER);
    EXPECT(ICodecAPI_SetAllDefaultsWithNotify(api, &list, NULL), E_POINTER);

    /* the settings stream */
    {
        IStream *s = new_stream();
        set_var(&v, VT_UI4, 10); ICodecAPI_SetValue(api, params[I_QUALITY].key, &v);
        set_var(&v, VT_UI4, 1); ICodecAPI_SetValue(api, params[I_RC].key, &v);
        set_var(&v, VT_UI8, 7); ICodecAPI_SetValue(api, params[I_QP].key, &v);
        EXPECT(ICodecAPI_GetAllSettings(api, s), S_OK);
        EXPECT(ICodecAPI_GetAllSettings(api, NULL), E_POINTER);
        ICodecAPI_SetAllDefaults(api);
        check_value("defaults before SetAllSettings", I_QUALITY, 65);
        rewind_stream(s);
        EXPECT(ICodecAPI_SetAllSettings(api, s), S_OK);
        check_value("after SetAllSettings", I_QUALITY, 10);
        check_value("after SetAllSettings", I_RC, 1);
        check_value("after SetAllSettings", I_QP, 7);
        check_value("after SetAllSettings", I_MAXQP, 51);
        ICodecAPI_SetAllDefaults(api);
        rewind_stream(s);
        list = NULL; count = 0xdeadbeef;
        EXPECT(ICodecAPI_SetAllSettingsWithNotify(api, s, &list, &count), S_OK);
        CHECKF(count == 3 && list && guid_list_has(list, count, params[I_QUALITY].key) && guid_list_has(list, count, params[I_RC].key)
                && guid_list_has(list, count, params[I_QP].key), "SetAllSettingsWithNotify lists the three parameters that changed (%lu)", count);
        if (count && list) CoTaskMemFree(list);
        IStream_Release(s);
        ICodecAPI_SetAllDefaults(api);

        /* a stream that cannot be applied changes nothing */
        {
            struct entry e1 = {CODECAPI_AVEncCommonQuality, VT_UI4, 20}, e2 = {CODECAPI_AVEncCommonQuality, VT_UI4, 200};
            struct entry e3 = {unsupported, VT_UI4, 1}, e4 = {CODECAPI_AVEncCommonQuality, VT_UI8, 20};
            const struct { const char *what; ULONG count; const void *e[2]; ULONG nentries; } rows[] =
            {
                {"a stream with a value out of range", 2, {&e1, &e2}, 2}, {"a stream with an unknown parameter", 2, {&e1, &e3}, 2},
                {"a stream with a wrong type", 2, {&e1, &e4}, 2}, {"a stream with too few entries", 2, {&e1}, 1},
                {"an empty stream", 0, {NULL}, 0}, {"a stream with too many entries", 99, {&e1}, 1},
            };
            int r;
            for (r = 0; r < (int)(sizeof(rows) / sizeof(rows[0])); r++)
            {
                ULONG e;
                s = new_stream();
                if (r != 4) stream_put(s, &rows[r].count, sizeof(rows[r].count));
                for (e = 0; e < rows[r].nentries; e++) stream_put(s, rows[r].e[e], sizeof(e1));
                rewind_stream(s);
                hr = ICodecAPI_SetAllSettings(api, s);
                CHECKF(hr == E_INVALIDARG, "%s is refused (%#lx)", rows[r].what, (unsigned long)hr);
                check_value(rows[r].what, I_QUALITY, 65);
                IStream_Release(s);
            }
        }
        EXPECT(ICodecAPI_SetAllSettings(api, NULL), E_POINTER);
        list = NULL;
        EXPECT(ICodecAPI_SetAllSettingsWithNotify(api, NULL, &list, &count), E_POINTER);
    }

    /* no events */
    EXPECT(ICodecAPI_RegisterForEvent(api, params[0].key, 0), E_NOTIMPL);
    EXPECT(ICodecAPI_UnregisterForEvent(api, params[0].key), E_NOTIMPL);
}

static void test_h264enc_mft(void)
{
    DWORD flags;
    IMFMediaType *t;
    HRESULT hr;

    /* the output type is set by test_codecapi; the input type makes the transform */
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_GetInputStatus(mft, 1, &flags), MF_E_INVALIDSTREAMNUMBER);
    EXPECT(IMFTransform_GetInputStatus(mft, 0, NULL), E_POINTER);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_STREAMING, 0), S_OK);
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), E_NOTIMPL);
    test_mft_optional("h264enc");

    t = mk_video(&MFVideoFormat_NV12, 96, 96);
    IMFMediaType_SetUINT64(t, &MF_MT_FRAME_RATE, (UINT64)30000 << 32 | 1001);
    hr = IMFTransform_SetInputType(mft, 0, t, 0);
    IMFMediaType_Release(t);
    CHECKF(hr == S_OK, "the NV12 input type is accepted (%#lx)", (unsigned long)hr);
    if (hr != S_OK) return;
    flags = 0;
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), S_OK);
    CHECKF(flags == MFT_INPUT_STATUS_ACCEPT_DATA, "the encoder accepts data (%#lx)", flags);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), S_OK);
}

/* ================= AAC decoder ========================================== */
static IMFSample *make_sample(DWORD size)
{
    IMFSample *s;
    IMFMediaBuffer *b;
    BYTE *p;
    DWORD i;

    MFCreateSample(&s);
    MFCreateMemoryBuffer(size, &b);
    IMFMediaBuffer_Lock(b, &p, NULL, NULL);
    for (i = 0; i < size; i++) p[i] = (BYTE)(i * 7);
    IMFMediaBuffer_Unlock(b);
    IMFMediaBuffer_SetCurrentLength(b, size);
    IMFSample_AddBuffer(s, b);
    IMFMediaBuffer_Release(b);
    IMFSample_SetSampleTime(s, 0);
    IMFSample_SetSampleDuration(s, 10000000);
    return s;
}

static void test_aacdec(void)
{
    static const BYTE codec_data[] = {0x12, 0x08};
    IMFAttributes *a, *b;
    IMFMediaType *t;
    DWORD flags;
    UINT32 v;
    HRESULT hr;
    IMFSample *s;

    EXPECT(IMFTransform_GetAttributes(mft, NULL), E_POINTER);
    EXPECT(IMFTransform_GetAttributes(mft, &a), S_OK);
    v = 0xdeadbeef;
    hr = IMFAttributes_GetUINT32(a, &MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &v);
    CHECKF(hr == S_OK && v == 0, "MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE is 0 (%#lx, %u)", (unsigned long)hr, v);
    EXPECT(IMFTransform_GetAttributes(mft, &b), S_OK);
    CHECKF(a == b, "GetAttributes returns the same object each time");
    IMFAttributes_Release(b);
    CHECKF(IMFAttributes_Release(a) == 1, "the attributes belong to the transform");
    EXPECT(IMFTransform_GetOutputStreamAttributes(mft, 0, &a), E_NOTIMPL);
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), E_NOTIMPL);
    test_mft_optional("aacdec");

    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_STREAMING, 0), S_OK);

    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &P_SUB_RAW_AAC1);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, 1);
    IMFMediaType_SetBlob(t, &MF_MT_USER_DATA, codec_data, sizeof(codec_data));
    hr = IMFTransform_SetInputType(mft, 0, t, 0);
    IMFMediaType_Release(t);
    CHECKF(hr == S_OK, "the raw AAC input type is accepted (%#lx)", (unsigned long)hr);
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &P_SUB_PCM);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, 1);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    hr = IMFTransform_SetOutputType(mft, 0, t, 0);
    IMFMediaType_Release(t);
    CHECKF(hr == S_OK, "the PCM output type is accepted (%#lx)", (unsigned long)hr);
    if (hr != S_OK) return;

    flags = 0;
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), S_OK);
    CHECKF(flags == MFT_INPUT_STATUS_ACCEPT_DATA, "the decoder accepts data (%#lx)", flags);
    s = make_sample(100);
    EXPECT(IMFTransform_ProcessInput(mft, 0, s, 0), S_OK);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), S_OK);
    CHECKF(flags == 0, "the decoder holds a sample and does not accept more (%#lx)", flags);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), S_OK);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), S_OK);
    CHECKF(flags == MFT_INPUT_STATUS_ACCEPT_DATA, "the flush dropped the held sample (%#lx)", flags);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), S_OK);
    IMFSample_Release(s);
}

/* ================= video processor ====================================== */
static void test_vproc(void)
{
    MFT_OUTPUT_DATA_BUFFER ob = {0};
    MFT_OUTPUT_STREAM_INFO oi;
    IMFMediaType *t;
    IMFSample *in, *out;
    IMFMediaBuffer *buf;
    DWORD flags, status, len;
    HRESULT hr;

    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), MF_E_TRANSFORM_TYPE_NOT_SET);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0), S_OK);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_NOTIFY_END_STREAMING, 0), S_OK);
    test_mft_optional("vproc");

    t = mk_video(&MFVideoFormat_RGB32, 64, 64);
    EXPECT(IMFTransform_SetOutputType(mft, 0, t, 0), S_OK);
    IMFMediaType_Release(t);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), MF_E_TRANSFORM_TYPE_NOT_SET);
    t = mk_video(&MFVideoFormat_NV12, 64, 64);
    hr = IMFTransform_SetInputType(mft, 0, t, 0);
    IMFMediaType_Release(t);
    CHECKF(hr == S_OK, "the NV12 input type is accepted (%#lx)", (unsigned long)hr);
    if (hr != S_OK) return;

    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), S_OK);
    CHECKF(flags == 0, "no output is ready before any input (%#lx)", flags);
    EXPECT(IMFTransform_GetOutputStatus(mft, NULL), E_POINTER);
    flags = 0;
    EXPECT(IMFTransform_GetInputStatus(mft, 0, &flags), S_OK);
    CHECKF(flags == MFT_INPUT_STATUS_ACCEPT_DATA, "the processor accepts data (%#lx)", flags);

    in = make_sample(64 * 64 * 3 / 2);
    EXPECT(IMFTransform_ProcessInput(mft, 0, in, 0), S_OK);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), S_OK);
    CHECKF(flags == MFT_OUTPUT_STATUS_SAMPLE_READY, "an output sample is ready after an input (%#lx)", flags);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_DRAIN, 0), S_OK);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), S_OK);
    CHECKF(flags == MFT_OUTPUT_STATUS_SAMPLE_READY, "the output sample is still ready after a drain (%#lx)", flags);

    EXPECT(IMFTransform_GetOutputStreamInfo(mft, 0, &oi), S_OK);
    MFCreateSample(&out);
    MFCreateMemoryBuffer(oi.cbSize ? oi.cbSize : 64 * 64 * 4, &buf);
    IMFSample_AddBuffer(out, buf);
    IMFMediaBuffer_Release(buf);
    ob.pSample = out;
    hr = IMFTransform_ProcessOutput(mft, 0, 1, &ob, &status);
    CHECKF(hr == S_OK, "ProcessOutput converts the sample (%#lx)", (unsigned long)hr);
    len = 0;
    IMFSample_GetTotalLength(out, &len);
    CHECKF(len == 64 * 64 * 4, "the RGB32 frame is 16384 bytes (%lu)", len);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), S_OK);
    CHECKF(flags == 0, "no output is ready once the sample was read (%#lx)", flags);

    /* a flush drops what was not read */
    EXPECT(IMFTransform_ProcessInput(mft, 0, in, 0), S_OK);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), S_OK);
    CHECKF(flags == MFT_OUTPUT_STATUS_SAMPLE_READY, "an output sample is ready after the second input (%#lx)", flags);
    EXPECT(IMFTransform_ProcessMessage(mft, MFT_MESSAGE_COMMAND_FLUSH, 0), S_OK);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), S_OK);
    CHECKF(flags == 0, "no output is ready after a flush (%#lx)", flags);
    hr = IMFTransform_ProcessOutput(mft, 0, 1, &ob, &status);
    CHECKF(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput after a flush wants more input (%#lx)", (unsigned long)hr);
    EXPECT(IMFTransform_ProcessInput(mft, 0, in, 0), S_OK);
    flags = 0xdeadbeef;
    EXPECT(IMFTransform_GetOutputStatus(mft, &flags), S_OK);
    CHECKF(flags == MFT_OUTPUT_STATUS_SAMPLE_READY, "the processor takes input again after a flush (%#lx)", flags);
    IMFSample_Release(out);
    IMFSample_Release(in);
}

int main(int argc, char **argv)
{
    const char *which = argc > 1 ? argv[1] : "wmvdec";
    const GUID *clsid;
    HRESULT hr;

    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);

    if (!strcmp(which, "wmvdec")) clsid = &P_CLSID_WMVDec;
    else if (!strcmp(which, "h264enc")) clsid = &P_CLSID_H264Enc;
    else if (!strcmp(which, "aacdec")) clsid = &P_CLSID_AACDec;
    else if (!strcmp(which, "vproc")) clsid = &P_CLSID_VProc;
    else { printf("FAIL  unknown component %s\n", which); return 1; }

    hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&mft);
    if (FAILED(hr))
    {
        printf("note  %s cannot be created (%#lx): skipped\n", which, (unsigned long)hr);
        printf("RESULT: SKIP\n");
        return 77;
    }

    if (!strcmp(which, "wmvdec"))
    {
        hr = IMFTransform_QueryInterface(mft, &IID_IMediaObject, (void **)&dmo);
        CHECKF(hr == S_OK, "IMediaObject is offered beside IMFTransform (%#lx)", (unsigned long)hr);
        if (hr == S_OK) test_wmvdec();
        if (dmo) IMediaObject_Release(dmo);
    }
    else if (!strcmp(which, "h264enc"))
    {
        hr = IMFTransform_QueryInterface(mft, &IID_ICodecAPI, (void **)&api);
        CHECKF(hr == S_OK, "ICodecAPI is offered (%#lx)", (unsigned long)hr);
        if (hr == S_OK)
        {
            test_codecapi();
            test_h264enc_mft();
            ICodecAPI_Release(api);
        }
    }
    else if (!strcmp(which, "aacdec")) test_aacdec();
    else test_vproc();

    CHECKF(IMFTransform_Release(mft) == 0, "the object is released");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
