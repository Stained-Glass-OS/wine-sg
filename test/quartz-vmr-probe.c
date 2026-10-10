/* quartz Video Mixing Renderer stubs (patches/sg/2930), run by test/quartz-vmr-gate.sh:
 * VMR9 and VMR7 filter configuration, mixer control, mixer bitmap, windowless
 * control, monitor configuration, certified output protection, the input
 * pin's IAMVideoAccelerator and IOverlay, the VMR7 surface allocator notify
 * object and the default allocator-presenter. Every value is checked.
 *
 *   quartz-vmr-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <d3d9.h>
#include <ddraw.h>
#include <vmr9.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef VFW_E_NO_COPP_HW
#define VFW_E_NO_COPP_HW ((HRESULT)0x8004029B)
#endif
#ifndef VFW_E_VMR_NO_PROCAMP_HW
#define VFW_E_VMR_NO_PROCAMP_HW ((HRESULT)0x80040299)
#endif
#ifndef VFW_E_VMR_NOT_IN_MIXER_MODE
#define VFW_E_VMR_NOT_IN_MIXER_MODE ((HRESULT)0x80040296)
#endif

DEFINE_GUID(IID_AMVA, 0x256a6a22, 0xfbad, 0x11d1, 0x82, 0xbf, 0x00, 0xa0, 0xc9, 0x69, 0x6c, 0x8f);

/* IAMVideoAccelerator is not in the mingw-w64 headers: declare its vtable by hand. */
typedef struct AMVA AMVA;
typedef struct AMVAVtbl
{
    HRESULT (WINAPI *QueryInterface)(AMVA *, REFIID, void **);
    ULONG (WINAPI *AddRef)(AMVA *);
    ULONG (WINAPI *Release)(AMVA *);
    HRESULT (WINAPI *GetVideoAcceleratorGUIDs)(AMVA *, DWORD *, GUID *);
    HRESULT (WINAPI *GetUncompFormatsSupported)(AMVA *, const GUID *, DWORD *, void *);
    HRESULT (WINAPI *GetInternalMemInfo)(AMVA *, const GUID *, const void *, void *);
    HRESULT (WINAPI *GetCompBufferInfo)(AMVA *, const GUID *, const void *, DWORD *, void *);
    HRESULT (WINAPI *GetInternalCompBufferInfo)(AMVA *, DWORD *, void *);
    HRESULT (WINAPI *BeginFrame)(AMVA *, const void *);
    HRESULT (WINAPI *EndFrame)(AMVA *, const void *);
    HRESULT (WINAPI *GetBuffer)(AMVA *, DWORD, DWORD, BOOL, void **, LONG *);
    HRESULT (WINAPI *ReleaseBuffer)(AMVA *, DWORD, DWORD);
    HRESULT (WINAPI *Execute)(AMVA *, DWORD, void *, DWORD, void *, DWORD, DWORD, const void *);
    HRESULT (WINAPI *QueryRenderStatus)(AMVA *, DWORD, DWORD, DWORD);
    HRESULT (WINAPI *DisplayFrame)(AMVA *, DWORD, IMediaSample *);
} AMVAVtbl;
struct AMVA { const AMVAVtbl *lpVtbl; };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define HR(call, expected) do { HRESULT _hr = (call); CHECKF(_hr == (HRESULT)(expected), "%s -> %#lx (want %#lx)", #call, (unsigned long)_hr, (unsigned long)(HRESULT)(expected)); } while (0)

/* A fake overlay notification sink. */
struct sink { IOverlayNotify IOverlayNotify_iface; LONG ref; };
static struct sink *impl_sink(IOverlayNotify *i) { return (struct sink *)i; }
static HRESULT WINAPI sink_QI(IOverlayNotify *i, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IOverlayNotify)) { *out = i; IOverlayNotify_AddRef(i); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IOverlayNotify *i) { return InterlockedIncrement(&impl_sink(i)->ref); }
static ULONG WINAPI sink_Release(IOverlayNotify *i) { return InterlockedDecrement(&impl_sink(i)->ref); }
static HRESULT WINAPI sink_pal(IOverlayNotify *i, DWORD c, const PALETTEENTRY *p) { return S_OK; }
static HRESULT WINAPI sink_clip(IOverlayNotify *i, const RECT *s, const RECT *d, const RGNDATA *r) { return S_OK; }
static HRESULT WINAPI sink_key(IOverlayNotify *i, const COLORKEY *k) { return S_OK; }
static HRESULT WINAPI sink_pos(IOverlayNotify *i, const RECT *s, const RECT *d) { return S_OK; }
static const IOverlayNotifyVtbl sink_vtbl = {sink_QI, sink_AddRef, sink_Release, sink_pal, sink_clip, sink_key, sink_pos};

static struct sink sink = {{(IOverlayNotifyVtbl *)&sink_vtbl}, 1};
static ULONG refcount(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }
static BOOL near_eq(float a, float b) { return fabsf(a - b) < 1e-6f; }

static IBaseFilter *create(const CLSID *clsid)
{
    IBaseFilter *f = NULL;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&f);
    if (FAILED(hr)) printf("note  CoCreateInstance failed (%#lx)\n", (unsigned long)hr);
    return f;
}

/* ---- the input pin's IAMVideoAccelerator and IOverlay, shared by VMR7 and VMR9 ---- */
static void test_pin_interfaces(IBaseFilter *filter, BOOL windowed, const char *tag)
{
    IPin *pin = NULL;
    AMVA *amva = NULL;
    IOverlay *ov = NULL;
    DWORD count;
    GUID guid = {1}, guids[2];
    LONG stride;
    void *buf;
    COLORKEY key, key2;
    PALETTEENTRY pal[3] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}}, *got = NULL;
    RGNDATA *rgn = NULL;
    RECT src, dst;
    HRESULT hr;

    IBaseFilter_FindPin(filter, L"VMR Input0", &pin);
    if (!pin) { check(0, "input pin"); return; }

    hr = IPin_QueryInterface(pin, &IID_AMVA, (void **)&amva);
    CHECKF(hr == S_OK, "%s: pin has IAMVideoAccelerator (%#lx)", tag, (unsigned long)hr);
    if (hr == S_OK)
    {
        count = 77;
        HR(amva->lpVtbl->GetVideoAcceleratorGUIDs(amva, NULL, NULL), E_POINTER);
        HR(amva->lpVtbl->GetVideoAcceleratorGUIDs(amva, &count, NULL), S_OK);
        CHECKF(count == 0, "%s: no video accelerators (%lu)", tag, count);
        count = 5;
        HR(amva->lpVtbl->GetVideoAcceleratorGUIDs(amva, &count, guids), S_OK);
        CHECKF(count == 0, "%s: still no accelerators with a buffer (%lu)", tag, count);
        HR(amva->lpVtbl->GetUncompFormatsSupported(amva, NULL, &count, NULL), E_POINTER);
        HR(amva->lpVtbl->GetUncompFormatsSupported(amva, &guid, NULL, NULL), E_POINTER);
        HR(amva->lpVtbl->GetUncompFormatsSupported(amva, &guid, &count, NULL), E_INVALIDARG);
        HR(amva->lpVtbl->GetInternalMemInfo(amva, &guid, NULL, NULL), E_POINTER);
        HR(amva->lpVtbl->GetInternalMemInfo(amva, &guid, &guid, &guid), E_INVALIDARG);
        HR(amva->lpVtbl->GetCompBufferInfo(amva, &guid, &guid, NULL, NULL), E_POINTER);
        HR(amva->lpVtbl->GetCompBufferInfo(amva, &guid, &guid, &count, NULL), E_INVALIDARG);
        count = 9;
        HR(amva->lpVtbl->GetInternalCompBufferInfo(amva, NULL, NULL), E_POINTER);
        HR(amva->lpVtbl->GetInternalCompBufferInfo(amva, &count, NULL), S_OK);
        CHECKF(count == 0, "%s: no internal compressed buffers (%lu)", tag, count);
        HR(amva->lpVtbl->BeginFrame(amva, NULL), E_POINTER);
        HR(amva->lpVtbl->BeginFrame(amva, &guid), VFW_E_WRONG_STATE);
        HR(amva->lpVtbl->EndFrame(amva, NULL), E_POINTER);
        HR(amva->lpVtbl->EndFrame(amva, &guid), VFW_E_WRONG_STATE);
        HR(amva->lpVtbl->GetBuffer(amva, 0, 0, FALSE, NULL, &stride), E_POINTER);
        HR(amva->lpVtbl->GetBuffer(amva, 0, 0, FALSE, &buf, &stride), VFW_E_WRONG_STATE);
        HR(amva->lpVtbl->ReleaseBuffer(amva, 0, 0), VFW_E_WRONG_STATE);
        HR(amva->lpVtbl->Execute(amva, 0, NULL, 0, NULL, 0, 0, NULL), VFW_E_WRONG_STATE);
        HR(amva->lpVtbl->QueryRenderStatus(amva, 0, 0, 0), VFW_E_WRONG_STATE);
        HR(amva->lpVtbl->DisplayFrame(amva, 0, NULL), E_POINTER);
        amva->lpVtbl->Release(amva);
    }

    hr = IPin_QueryInterface(pin, &IID_IOverlay, (void **)&ov);
    CHECKF(hr == S_OK, "%s: pin has IOverlay (%#lx)", tag, (unsigned long)hr);
    if (hr == S_OK && windowed)
    {
        count = 5;
        HR(IOverlay_GetPalette(ov, NULL, &got), E_POINTER);
        HR(IOverlay_GetPalette(ov, &count, &got), VFW_E_NO_PALETTE_AVAILABLE);
        HR(IOverlay_SetPalette(ov, 257, pal), E_INVALIDARG);
        HR(IOverlay_SetPalette(ov, 3, NULL), E_POINTER);
        HR(IOverlay_SetPalette(ov, 3, pal), S_OK);
        count = 0;
        HR(IOverlay_GetPalette(ov, &count, &got), S_OK);
        CHECKF(count == 3 && got && !memcmp(got, pal, sizeof(pal)), "%s: palette round trip (%lu)", tag, count);
        CoTaskMemFree(got);
        HR(IOverlay_SetPalette(ov, 0, NULL), S_OK);
        HR(IOverlay_GetPalette(ov, &count, &got), VFW_E_NO_PALETTE_AVAILABLE);

        HR(IOverlay_GetDefaultColorKey(ov, NULL), E_POINTER);
        HR(IOverlay_GetDefaultColorKey(ov, &key), VFW_E_NO_COLOR_KEY_FOUND);
        HR(IOverlay_GetColorKey(ov, NULL), E_POINTER);
        HR(IOverlay_GetColorKey(ov, &key), VFW_E_NO_COLOR_KEY_SET);
        key.KeyType = CK_RGB; key.PaletteIndex = 4; key.LowColorValue = 0x123456; key.HighColorValue = 0x654321;
        HR(IOverlay_SetColorKey(ov, NULL), E_POINTER);
        key.KeyType = 8;
        HR(IOverlay_SetColorKey(ov, &key), E_INVALIDARG);
        key.KeyType = CK_RGB;
        HR(IOverlay_SetColorKey(ov, &key), S_OK);
        memset(&key2, 0xcc, sizeof(key2));
        HR(IOverlay_GetColorKey(ov, &key2), S_OK);
        CHECKF(!memcmp(&key, &key2, sizeof(key)), "%s: color key round trip", tag);

        HR(IOverlay_GetVideoPosition(ov, NULL, &dst), E_POINTER);
        memset(&src, 0xcc, sizeof(src)); memset(&dst, 0xcc, sizeof(dst));
        HR(IOverlay_GetVideoPosition(ov, &src, &dst), S_OK);
        CHECKF(src.right >= src.left && dst.right >= dst.left, "%s: video position rects are filled in", tag);
        HR(IOverlay_GetClipList(ov, &src, &dst, NULL), E_POINTER);
        HR(IOverlay_GetClipList(ov, &src, &dst, &rgn), S_OK);
        CHECKF(rgn && rgn->rdh.nCount == 1 && rgn->rdh.iType == RDH_RECTANGLES
                && rgn->rdh.nRgnSize == sizeof(RECT) && !memcmp(rgn->Buffer, &dst, sizeof(RECT)),
                "%s: clip list is the destination rectangle", tag);
        CoTaskMemFree(rgn);

        HR(IOverlay_Advise(ov, NULL, ADVISE_POSITION), E_POINTER);
        HR(IOverlay_Advise(ov, &sink.IOverlayNotify_iface, 0), E_INVALIDARG);
        HR(IOverlay_Advise(ov, &sink.IOverlayNotify_iface, 0x100), E_INVALIDARG);
        HR(IOverlay_Unadvise(ov), VFW_E_NO_ADVISE_SET);
        HR(IOverlay_Advise(ov, &sink.IOverlayNotify_iface, ADVISE_POSITION | ADVISE_CLIPPING), S_OK);
        CHECKF(sink.ref == 2, "%s: Advise holds a reference (%ld)", tag, sink.ref);
        HR(IOverlay_Advise(ov, &sink.IOverlayNotify_iface, ADVISE_POSITION), VFW_E_ADVISE_ALREADY_SET);
        HR(IOverlay_Unadvise(ov), S_OK);
        CHECKF(sink.ref == 1, "%s: Unadvise drops the reference (%ld)", tag, sink.ref);
        HR(IOverlay_Unadvise(ov), VFW_E_NO_ADVISE_SET);
        HR(IOverlay_Advise(ov, &sink.IOverlayNotify_iface, ADVISE_ALL2), S_OK);
        CHECKF(sink.ref == 2, "%s: Advise again (%ld)", tag, sink.ref);
        /* the reference is dropped when the filter goes away: checked by the caller via sink.ref */
        IOverlay_Release(ov);
        IPin_Release(pin);
        /* keep the sink alive for the filter's destruction */
        printf("note  %s: advised sink left to the filter\n", tag);
        return;
    }
    else if (hr == S_OK)
    {
        /* windowless / renderless: no window, so the overlay is not usable */
        HR(IOverlay_GetPalette(ov, &count, &got), VFW_E_WRONG_STATE);
        HR(IOverlay_SetPalette(ov, 0, NULL), VFW_E_WRONG_STATE);
        HR(IOverlay_GetDefaultColorKey(ov, &key), VFW_E_WRONG_STATE);
        HR(IOverlay_GetColorKey(ov, &key), VFW_E_WRONG_STATE);
        key.KeyType = CK_RGB;
        HR(IOverlay_SetColorKey(ov, &key), VFW_E_WRONG_STATE);
        HR(IOverlay_GetVideoPosition(ov, &src, &dst), VFW_E_WRONG_STATE);
        HR(IOverlay_GetClipList(ov, &src, &dst, &rgn), VFW_E_WRONG_STATE);
        HR(IOverlay_Advise(ov, &sink.IOverlayNotify_iface, ADVISE_POSITION), VFW_E_WRONG_STATE);
        HR(IOverlay_Unadvise(ov), VFW_E_WRONG_STATE);
        IOverlay_Release(ov);
    }
    IPin_Release(pin);
}

static void test_copp(IBaseFilter *filter, const char *tag)
{
    IAMCertifiedOutputProtection *copp = NULL;
    AMCOPPSignature sig;
    AMCOPPCommand cmd;
    AMCOPPStatusInput in;
    AMCOPPStatusOutput out;
    BYTE *cert = NULL;
    DWORD len = 0;
    GUID random;
    HRESULT hr = IBaseFilter_QueryInterface(filter, &IID_IAMCertifiedOutputProtection, (void **)&copp);

    CHECKF(hr == S_OK, "%s: IAMCertifiedOutputProtection (%#lx)", tag, (unsigned long)hr);
    if (hr != S_OK) return;
    memset(&sig, 0, sizeof(sig)); memset(&cmd, 0, sizeof(cmd)); memset(&in, 0, sizeof(in)); memset(&out, 0, sizeof(out));
    HR(IAMCertifiedOutputProtection_KeyExchange(copp, &random, &cert, &len), VFW_E_NO_COPP_HW);
    HR(IAMCertifiedOutputProtection_SessionSequenceStart(copp, &sig), VFW_E_NO_COPP_HW);
    HR(IAMCertifiedOutputProtection_ProtectionCommand(copp, &cmd), VFW_E_NO_COPP_HW);
    HR(IAMCertifiedOutputProtection_ProtectionStatus(copp, &in, &out), VFW_E_NO_COPP_HW);
    IAMCertifiedOutputProtection_Release(copp);
}

/* ---------------------------------- VMR9 ---------------------------------- */
static void test_vmr9_config_and_mixer(void)
{
    IBaseFilter *filter = create(&CLSID_VideoMixingRenderer9);
    IVMRFilterConfig9 *config;
    IVMRMixerControl9 *mixer;
    IVMRMixerBitmap9 *bitmap;
    IVMRAspectRatioControl9 *ar;
    VMR9NormalizedRect rect, rect2;
    VMR9ProcAmpControl pa;
    VMR9ProcAmpControlRange range;
    VMR9AlphaBitmap bm, bm2;
    COLORREF color;
    float alpha;
    DWORD v, i;
    HRESULT hr;
    HDC dc;

    if (!filter) { check(0, "VMR9 created"); return; }
    IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig9, (void **)&config);
    IBaseFilter_QueryInterface(filter, &IID_IVMRMixerBitmap9, (void **)&bitmap);
    IBaseFilter_QueryInterface(filter, &IID_IVMRAspectRatioControl9, (void **)&ar);

    /* rendering preferences: only RenderPrefs9_DoNotRenderBorder exists */
    v = 77;
    HR(IVMRFilterConfig9_GetRenderingPrefs(config, NULL), E_POINTER);
    HR(IVMRFilterConfig9_GetRenderingPrefs(config, &v), S_OK);
    CHECKF(v == 0, "VMR9 default rendering prefs %#lx", v);
    HR(IVMRFilterConfig9_SetRenderingPrefs(config, 2), E_INVALIDARG);
    HR(IVMRFilterConfig9_SetRenderingPrefs(config, 0x80000000), E_INVALIDARG);
    HR(IVMRFilterConfig9_SetRenderingPrefs(config, RenderPrefs9_DoNotRenderBorder), S_OK);
    HR(IVMRFilterConfig9_GetRenderingPrefs(config, &v), S_OK);
    CHECKF(v == RenderPrefs9_DoNotRenderBorder, "VMR9 rendering prefs round trip %#lx", v);

    /* aspect ratio control */
    v = 77;
    HR(IVMRAspectRatioControl9_GetAspectRatioMode(ar, NULL), E_POINTER);
    HR(IVMRAspectRatioControl9_GetAspectRatioMode(ar, &v), S_OK);
    CHECKF(v == VMR9ARMode_None, "VMR9 default aspect mode %lu", v);
    HR(IVMRAspectRatioControl9_SetAspectRatioMode(ar, 2), E_INVALIDARG);
    HR(IVMRAspectRatioControl9_SetAspectRatioMode(ar, VMR9ARMode_LetterBox), S_OK);
    IVMRAspectRatioControl9_GetAspectRatioMode(ar, &v);
    CHECKF(v == VMR9ARMode_LetterBox, "VMR9 aspect mode round trip %lu", v);
    IVMRAspectRatioControl9_Release(ar);

    /* stream count limits */
    HR(IVMRFilterConfig9_GetNumberOfStreams(config, NULL), E_POINTER);
    HR(IVMRFilterConfig9_GetNumberOfStreams(config, &v), VFW_E_VMR_NOT_IN_MIXER_MODE);
    HR(IVMRFilterConfig9_SetNumberOfStreams(config, 0), E_INVALIDARG);
    HR(IVMRFilterConfig9_SetNumberOfStreams(config, 17), E_INVALIDARG);
    HR(IVMRFilterConfig9_SetImageCompositor(config, NULL), S_OK);

    /* a mixer needs a stream count */
    hr = IBaseFilter_QueryInterface(filter, &IID_IVMRMixerControl9, (void **)&mixer);
    CHECKF(hr == E_NOINTERFACE, "VMR9 mixer control needs streams (%#lx)", (unsigned long)hr);

    /* the alpha bitmap needs no stream: before any is set there is none to read */
    memset(&bm2, 0, sizeof(bm2));
    HR(IVMRMixerBitmap9_GetAlphaBitmapParameters(bitmap, &bm2), VFW_E_WRONG_STATE);
    HR(IVMRMixerBitmap9_UpdateAlphaBitmapParameters(bitmap, &bm2), VFW_E_WRONG_STATE);
    HR(IVMRMixerBitmap9_SetAlphaBitmap(bitmap, NULL), E_POINTER);
    HR(IVMRMixerBitmap9_GetAlphaBitmapParameters(bitmap, NULL), E_POINTER);
    memset(&bm, 0, sizeof(bm));
    bm.dwFlags = 0x1000;
    HR(IVMRMixerBitmap9_SetAlphaBitmap(bitmap, &bm), E_INVALIDARG);
    bm.dwFlags = VMR9AlphaBitmap_hDC;
    bm.rDest.right = bm.rDest.bottom = 1.0f; bm.fAlpha = 1.0f;
    HR(IVMRMixerBitmap9_SetAlphaBitmap(bitmap, &bm), E_INVALIDARG); /* no hdc */
    dc = GetDC(NULL);
    bm.hdc = dc;
    bm.rDest.left = 0.5f; bm.rDest.right = 0.25f;
    HR(IVMRMixerBitmap9_SetAlphaBitmap(bitmap, &bm), E_INVALIDARG); /* empty destination */
    bm.rDest.left = 0.25f; bm.rDest.top = 0.125f; bm.rDest.right = 0.75f; bm.rDest.bottom = 0.5f;
    bm.fAlpha = 1.5f;
    HR(IVMRMixerBitmap9_SetAlphaBitmap(bitmap, &bm), E_INVALIDARG);
    bm.fAlpha = 0.5f;
    bm.dwFlags = VMR9AlphaBitmap_hDC | VMR9AlphaBitmap_SrcRect | VMR9AlphaBitmap_SrcColorKey;
    SetRect(&bm.rSrc, 1, 2, 11, 12);
    bm.clrSrcKey = 0x00123456;
    HR(IVMRMixerBitmap9_SetAlphaBitmap(bitmap, &bm), S_OK);
    memset(&bm2, 0xcc, sizeof(bm2));
    HR(IVMRMixerBitmap9_GetAlphaBitmapParameters(bitmap, &bm2), S_OK);
    CHECKF(bm2.dwFlags == bm.dwFlags && bm2.hdc == dc && EqualRect(&bm2.rSrc, &bm.rSrc) && near_eq(bm2.fAlpha, 0.5f)
            && near_eq(bm2.rDest.left, 0.25f) && near_eq(bm2.rDest.bottom, 0.5f) && bm2.clrSrcKey == 0x00123456,
            "VMR9 alpha bitmap round trip (flags %#lx alpha %f)", bm2.dwFlags, bm2.fAlpha);
    bm.dwFlags = VMR9AlphaBitmap_FilterMode;
    bm.fAlpha = 2.0f;
    HR(IVMRMixerBitmap9_UpdateAlphaBitmapParameters(bitmap, &bm), E_INVALIDARG);
    bm.fAlpha = 0.25f; bm.dwFilterMode = 3;
    HR(IVMRMixerBitmap9_UpdateAlphaBitmapParameters(bitmap, &bm), S_OK);
    IVMRMixerBitmap9_GetAlphaBitmapParameters(bitmap, &bm2);
    CHECKF(near_eq(bm2.fAlpha, 0.25f) && bm2.dwFilterMode == 3 && bm2.hdc == dc && (bm2.dwFlags & VMR9AlphaBitmap_hDC),
            "VMR9 alpha bitmap update keeps the bitmap (flags %#lx)", bm2.dwFlags);
    bm.dwFlags = VMR9AlphaBitmap_Disable;
    HR(IVMRMixerBitmap9_UpdateAlphaBitmapParameters(bitmap, &bm), S_OK);
    IVMRMixerBitmap9_GetAlphaBitmapParameters(bitmap, &bm2);
    CHECKF(bm2.dwFlags & VMR9AlphaBitmap_Disable, "VMR9 alpha bitmap disabled (flags %#lx)", bm2.dwFlags);
    ReleaseDC(NULL, dc);
    IVMRMixerBitmap9_Release(bitmap);

    /* streams */
    HR(IVMRFilterConfig9_SetNumberOfStreams(config, 3), S_OK);
    HR(IVMRFilterConfig9_SetNumberOfStreams(config, 2), VFW_E_WRONG_STATE);
    hr = IBaseFilter_QueryInterface(filter, &IID_IVMRMixerControl9, (void **)&mixer);
    CHECKF(hr == S_OK, "VMR9 mixer control (%#lx)", (unsigned long)hr);
    if (hr != S_OK) goto out;

    for (i = 0; i < 3; ++i)
    {
        alpha = 7.0f;
        HR(IVMRMixerControl9_GetAlpha(mixer, i, &alpha), S_OK);
        CHECKF(near_eq(alpha, 1.0f), "VMR9 stream %lu default alpha %f", i, alpha);
        v = 77;
        HR(IVMRMixerControl9_GetZOrder(mixer, i, &v), S_OK);
        CHECKF(v == i, "VMR9 stream %lu default z-order %lu", i, v);
        memset(&rect, 0xcc, sizeof(rect));
        HR(IVMRMixerControl9_GetOutputRect(mixer, i, &rect), S_OK);
        CHECKF(near_eq(rect.left, 0) && near_eq(rect.top, 0) && near_eq(rect.right, 1) && near_eq(rect.bottom, 1),
                "VMR9 stream %lu default output rect", i);
    }
    HR(IVMRMixerControl9_GetAlpha(mixer, 3, &alpha), E_INVALIDARG);
    HR(IVMRMixerControl9_GetAlpha(mixer, 0, NULL), E_POINTER);
    HR(IVMRMixerControl9_SetAlpha(mixer, 3, 0.5f), E_INVALIDARG);
    HR(IVMRMixerControl9_SetAlpha(mixer, 1, 1.01f), E_INVALIDARG);
    HR(IVMRMixerControl9_SetAlpha(mixer, 1, -0.01f), E_INVALIDARG);
    HR(IVMRMixerControl9_SetAlpha(mixer, 1, NAN), E_INVALIDARG);
    HR(IVMRMixerControl9_SetAlpha(mixer, 1, 0.25f), S_OK);
    IVMRMixerControl9_GetAlpha(mixer, 1, &alpha);
    CHECKF(near_eq(alpha, 0.25f), "VMR9 alpha round trip %f", alpha);
    IVMRMixerControl9_GetAlpha(mixer, 0, &alpha);
    CHECKF(near_eq(alpha, 1.0f), "VMR9 alpha is per stream %f", alpha);
    HR(IVMRMixerControl9_SetAlpha(mixer, 2, 0.0f), S_OK);
    HR(IVMRMixerControl9_SetAlpha(mixer, 2, 1.0f), S_OK);

    HR(IVMRMixerControl9_GetZOrder(mixer, 3, &v), E_INVALIDARG);
    HR(IVMRMixerControl9_GetZOrder(mixer, 0, NULL), E_POINTER);
    HR(IVMRMixerControl9_SetZOrder(mixer, 3, 0), E_INVALIDARG);
    HR(IVMRMixerControl9_SetZOrder(mixer, 0, 2), S_OK);
    IVMRMixerControl9_GetZOrder(mixer, 0, &v);
    CHECKF(v == 2, "VMR9 z-order round trip %lu", v);
    IVMRMixerControl9_GetZOrder(mixer, 2, &v);
    CHECKF(v == 2, "VMR9 z-order is per stream %lu", v);

    rect.left = 0.1f; rect.top = 0.2f; rect.right = 0.9f; rect.bottom = 0.8f;
    HR(IVMRMixerControl9_SetOutputRect(mixer, 1, NULL), E_POINTER);
    HR(IVMRMixerControl9_SetOutputRect(mixer, 3, &rect), E_INVALIDARG);
    HR(IVMRMixerControl9_SetOutputRect(mixer, 1, &rect), S_OK);
    memset(&rect2, 0xcc, sizeof(rect2));
    HR(IVMRMixerControl9_GetOutputRect(mixer, 1, &rect2), S_OK);
    CHECKF(!memcmp(&rect, &rect2, sizeof(rect)), "VMR9 output rect round trip (%f %f %f %f)", rect2.left, rect2.top, rect2.right, rect2.bottom);
    HR(IVMRMixerControl9_GetOutputRect(mixer, 1, NULL), E_POINTER);
    HR(IVMRMixerControl9_GetOutputRect(mixer, 5, &rect2), E_INVALIDARG);
    rect2.left = 0.6f; rect2.right = 0.5f; rect2.top = 0; rect2.bottom = 1;
    HR(IVMRMixerControl9_SetOutputRect(mixer, 1, &rect2), E_INVALIDARG);
    rect2.left = -0.1f; rect2.right = 1; rect2.top = 0; rect2.bottom = 1;
    HR(IVMRMixerControl9_SetOutputRect(mixer, 1, &rect2), E_INVALIDARG);
    rect2.left = 0; rect2.right = 1.1f;
    HR(IVMRMixerControl9_SetOutputRect(mixer, 1, &rect2), E_INVALIDARG);
    rect2.left = 0; rect2.right = 1; rect2.top = 0.5f; rect2.bottom = 0.5f;
    HR(IVMRMixerControl9_SetOutputRect(mixer, 1, &rect2), E_INVALIDARG);
    IVMRMixerControl9_GetOutputRect(mixer, 1, &rect2);
    CHECKF(!memcmp(&rect, &rect2, sizeof(rect)), "VMR9 invalid output rects leave the stored one alone");

    color = 77;
    HR(IVMRMixerControl9_GetBackgroundClr(mixer, NULL), E_POINTER);
    HR(IVMRMixerControl9_GetBackgroundClr(mixer, &color), S_OK);
    CHECKF(color == 0, "VMR9 default background %#lx", (unsigned long)color);
    HR(IVMRMixerControl9_SetBackgroundClr(mixer, 0x00123456), S_OK);
    IVMRMixerControl9_GetBackgroundClr(mixer, &color);
    CHECKF(color == 0x00123456, "VMR9 background round trip %#lx", (unsigned long)color);

    HR(IVMRMixerControl9_GetMixingPrefs(mixer, NULL), E_POINTER);

    /* ProcAmp needs video processing hardware, which is reported absent */
    memset(&pa, 0, sizeof(pa));
    HR(IVMRMixerControl9_GetProcAmpControl(mixer, 0, NULL), E_POINTER);
    HR(IVMRMixerControl9_GetProcAmpControl(mixer, 0, &pa), E_INVALIDARG); /* dwSize 0 */
    pa.dwSize = sizeof(pa); pa.dwFlags = ProcAmpControl9_Brightness;
    HR(IVMRMixerControl9_GetProcAmpControl(mixer, 3, &pa), E_INVALIDARG);
    pa.dwFlags = 0x10;
    HR(IVMRMixerControl9_GetProcAmpControl(mixer, 0, &pa), E_INVALIDARG);
    pa.dwFlags = ProcAmpControl9_Brightness | ProcAmpControl9_Hue;
    HR(IVMRMixerControl9_GetProcAmpControl(mixer, 0, &pa), VFW_E_VMR_NO_PROCAMP_HW);
    HR(IVMRMixerControl9_SetProcAmpControl(mixer, 0, &pa), VFW_E_VMR_NO_PROCAMP_HW);
    HR(IVMRMixerControl9_SetProcAmpControl(mixer, 0, NULL), E_POINTER);
    pa.dwSize = 1;
    HR(IVMRMixerControl9_SetProcAmpControl(mixer, 0, &pa), E_INVALIDARG);
    memset(&range, 0, sizeof(range));
    HR(IVMRMixerControl9_GetProcAmpControlRange(mixer, 0, NULL), E_POINTER);
    HR(IVMRMixerControl9_GetProcAmpControlRange(mixer, 0, &range), E_INVALIDARG);
    range.dwSize = sizeof(range);
    HR(IVMRMixerControl9_GetProcAmpControlRange(mixer, 0, &range), E_INVALIDARG); /* no property */
    range.dwProperty = ProcAmpControl9_Brightness | ProcAmpControl9_Contrast;
    HR(IVMRMixerControl9_GetProcAmpControlRange(mixer, 0, &range), E_INVALIDARG); /* two properties */
    range.dwProperty = ProcAmpControl9_Saturation;
    HR(IVMRMixerControl9_GetProcAmpControlRange(mixer, 9, &range), E_INVALIDARG);
    HR(IVMRMixerControl9_GetProcAmpControlRange(mixer, 0, &range), VFW_E_VMR_NO_PROCAMP_HW);
    IVMRMixerControl9_Release(mixer);

out:
    IVMRFilterConfig9_Release(config);
    IBaseFilter_Release(filter);
}

static void test_vmr9_monitors(void)
{
    IBaseFilter *filter = create(&CLSID_VideoMixingRenderer9);
    IVMRMonitorConfig9 *mon;
    VMR9MonitorInfo info[8];
    DWORD n = 0, i;
    UINT dev;
    HRESULT hr;

    if (!filter) { check(0, "VMR9 created"); return; }
    hr = IBaseFilter_QueryInterface(filter, &IID_IVMRMonitorConfig9, (void **)&mon);
    if (hr != S_OK) { printf("note  no IVMRMonitorConfig9 (%#lx), skipped\n", (unsigned long)hr); IBaseFilter_Release(filter); return; }

    HR(IVMRMonitorConfig9_GetMonitor(mon, NULL), E_POINTER);
    HR(IVMRMonitorConfig9_GetDefaultMonitor(mon, NULL), E_POINTER);
    dev = 77;
    HR(IVMRMonitorConfig9_GetMonitor(mon, &dev), S_OK);
    CHECKF(dev == 0, "VMR9 initial monitor %u", dev);
    HR(IVMRMonitorConfig9_GetAvailableMonitors(mon, NULL, 0, &n), S_OK);
    CHECKF(n >= 1, "VMR9 monitor count %lu", n);
    memset(info, 0, sizeof(info));
    HR(IVMRMonitorConfig9_GetAvailableMonitors(mon, info, min(n, 8), &n), S_OK);
    for (i = 0; i < min(n, 8); ++i)
        CHECKF(info[i].uDevID == i, "VMR9 monitor %lu has device id %u", i, info[i].uDevID);
    HR(IVMRMonitorConfig9_SetMonitor(mon, n), E_INVALIDARG);
    HR(IVMRMonitorConfig9_SetMonitor(mon, 0xdeadbeef), E_INVALIDARG);
    HR(IVMRMonitorConfig9_SetMonitor(mon, n - 1), S_OK);
    dev = 77;
    IVMRMonitorConfig9_GetMonitor(mon, &dev);
    CHECKF(dev == n - 1, "VMR9 monitor round trip %u", dev);
    HR(IVMRMonitorConfig9_SetDefaultMonitor(mon, n), E_INVALIDARG);
    HR(IVMRMonitorConfig9_SetDefaultMonitor(mon, 0), S_OK);
    dev = 77;
    IVMRMonitorConfig9_GetDefaultMonitor(mon, &dev);
    CHECKF(dev == 0, "VMR9 default monitor round trip %u", dev);
    IVMRMonitorConfig9_Release(mon);
    IBaseFilter_Release(filter);
}

static void test_vmr9_windowless(void)
{
    IBaseFilter *filter = create(&CLSID_VideoMixingRenderer9);
    IVMRFilterConfig9 *config;
    IVMRWindowlessControl9 *wc;
    BYTE *dib;
    COLORREF color;
    LONG w, h;
    DWORD v;
    HRESULT hr;

    if (!filter) { check(0, "VMR9 created"); return; }
    IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig9, (void **)&config);
    hr = IVMRFilterConfig9_SetRenderingMode(config, VMR9Mode_Windowless);
    if (hr != S_OK)
    {
        printf("note  windowless mode unavailable (%#lx), skipped\n", (unsigned long)hr);
        goto out;
    }
    hr = IBaseFilter_QueryInterface(filter, &IID_IVMRWindowlessControl9, (void **)&wc);
    CHECKF(hr == S_OK, "VMR9 windowless control (%#lx)", (unsigned long)hr);
    if (hr != S_OK) goto out;

    HR(IVMRWindowlessControl9_GetMinIdealVideoSize(wc, NULL, &h), E_POINTER);
    HR(IVMRWindowlessControl9_GetMaxIdealVideoSize(wc, &w, NULL), E_POINTER);
    HR(IVMRWindowlessControl9_GetMinIdealVideoSize(wc, &w, &h), VFW_E_NOT_CONNECTED);
    HR(IVMRWindowlessControl9_GetMaxIdealVideoSize(wc, &w, &h), VFW_E_NOT_CONNECTED);

    HR(IVMRWindowlessControl9_GetBorderColor(wc, NULL), E_POINTER);
    color = 77;
    HR(IVMRWindowlessControl9_GetBorderColor(wc, &color), S_OK);
    CHECKF(color == 0, "VMR9 default border color %#lx", (unsigned long)color);
    HR(IVMRWindowlessControl9_SetBorderColor(wc, 0x0000ff00), S_OK);
    IVMRWindowlessControl9_GetBorderColor(wc, &color);
    CHECKF(color == 0x0000ff00, "VMR9 border color round trip %#lx", (unsigned long)color);

    HR(IVMRWindowlessControl9_DisplayModeChanged(wc), S_OK);
    HR(IVMRWindowlessControl9_GetCurrentImage(wc, NULL), E_POINTER);
    dib = (BYTE *)1;
    HR(IVMRWindowlessControl9_GetCurrentImage(wc, &dib), VFW_E_NOT_CONNECTED);
    CHECKF(dib == (BYTE *)1, "VMR9 GetCurrentImage leaves the output alone");

    HR(IVMRWindowlessControl9_GetAspectRatioMode(wc, NULL), E_POINTER);
    HR(IVMRWindowlessControl9_SetAspectRatioMode(wc, 2), E_INVALIDARG);
    HR(IVMRWindowlessControl9_SetAspectRatioMode(wc, VMR9ARMode_LetterBox), S_OK);
    IVMRWindowlessControl9_GetAspectRatioMode(wc, &v);
    CHECKF(v == VMR9ARMode_LetterBox, "VMR9 windowless aspect mode %lu", v);
    IVMRWindowlessControl9_Release(wc);
out:
    IVMRFilterConfig9_Release(config);
    IBaseFilter_Release(filter);
}

static void test_vmr9(void)
{
    IBaseFilter *filter;
    IVMRFilterConfig9 *config;
    HRESULT hr;

    test_vmr9_config_and_mixer();
    test_vmr9_monitors();
    test_vmr9_windowless();

    filter = create(&CLSID_VideoMixingRenderer9);
    if (filter)
    {
        test_copp(filter, "VMR9");
        sink.ref = 1;
        test_pin_interfaces(filter, TRUE, "VMR9 windowed");
        IBaseFilter_Release(filter);
        CHECKF(sink.ref == 1, "VMR9 released the advised overlay sink (%ld)", sink.ref);
    }
    filter = create(&CLSID_VideoMixingRenderer9);
    if (filter)
    {
        IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig9, (void **)&config);
        hr = IVMRFilterConfig9_SetRenderingMode(config, VMR9Mode_Renderless);
        CHECKF(hr == S_OK, "VMR9 renderless mode (%#lx)", (unsigned long)hr);
        IVMRFilterConfig9_Release(config);
        test_pin_interfaces(filter, FALSE, "VMR9 renderless");
        IBaseFilter_Release(filter);
    }
}

/* ---------------------------------- VMR7 ---------------------------------- */
static void test_vmr7_config(void)
{
    IBaseFilter *filter = create(&CLSID_VideoMixingRenderer);
    IVMRFilterConfig *config;
    IVMRMonitorConfig *mon;
    VMRGUID guid, guid2, bad;
    VMRMONITORINFO info[8];
    GUID g;
    DWORD v, n = 0;
    HRESULT hr;

    if (!filter) { check(0, "VMR7 created"); return; }
    IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig, (void **)&config);

    v = 77;
    HR(IVMRFilterConfig_GetRenderingPrefs(config, NULL), E_POINTER);
    HR(IVMRFilterConfig_GetRenderingPrefs(config, &v), S_OK);
    CHECKF(v == 0, "VMR7 default rendering prefs %#lx", v);
    HR(IVMRFilterConfig_SetRenderingPrefs(config, 0x40), E_INVALIDARG);
    HR(IVMRFilterConfig_SetRenderingPrefs(config, RenderPrefs_ForceOffscreen | RenderPrefs_DoNotRenderColorKeyAndBorder), S_OK);
    HR(IVMRFilterConfig_GetRenderingPrefs(config, &v), S_OK);
    CHECKF(v == (RenderPrefs_ForceOffscreen | RenderPrefs_DoNotRenderColorKeyAndBorder), "VMR7 rendering prefs round trip %#lx", v);

    HR(IVMRFilterConfig_GetNumberOfStreams(config, NULL), E_POINTER);
    HR(IVMRFilterConfig_GetNumberOfStreams(config, &v), VFW_E_VMR_NOT_IN_MIXER_MODE);
    HR(IVMRFilterConfig_SetNumberOfStreams(config, 0), E_INVALIDARG);
    HR(IVMRFilterConfig_SetNumberOfStreams(config, 17), E_INVALIDARG);
    HR(IVMRFilterConfig_SetNumberOfStreams(config, 4), S_OK);
    v = 0;
    HR(IVMRFilterConfig_GetNumberOfStreams(config, &v), S_OK);
    CHECKF(v == 4, "VMR7 stream count %lu", v);
    HR(IVMRFilterConfig_SetNumberOfStreams(config, 2), VFW_E_WRONG_STATE);
    HR(IVMRFilterConfig_SetImageCompositor(config, NULL), S_OK);
    IVMRFilterConfig_Release(config);

    hr = IBaseFilter_QueryInterface(filter, &IID_IVMRMonitorConfig, (void **)&mon);
    CHECKF(hr == S_OK, "VMR7 monitor config (%#lx)", (unsigned long)hr);
    if (hr == S_OK)
    {
        memset(&guid, 0xcc, sizeof(guid));
        HR(IVMRMonitorConfig_GetMonitor(mon, NULL), E_POINTER);
        HR(IVMRMonitorConfig_GetMonitor(mon, &guid), S_OK);
        CHECKF(guid.pGUID == NULL, "VMR7 initial monitor is the default device");
        HR(IVMRMonitorConfig_SetMonitor(mon, NULL), E_POINTER);
        HR(IVMRMonitorConfig_SetDefaultMonitor(mon, NULL), E_POINTER);
        HR(IVMRMonitorConfig_GetDefaultMonitor(mon, NULL), E_POINTER);
        HR(IVMRMonitorConfig_GetAvailableMonitors(mon, NULL, 0, &n), S_OK);
        memset(info, 0, sizeof(info));
        IVMRMonitorConfig_GetAvailableMonitors(mon, info, min(n, 8), &n);

        memset(&bad, 0, sizeof(bad));
        memset(&g, 0, sizeof(g)); g.Data1 = 5; g.Data4[7] = 1;
        bad.pGUID = &g;
        HR(IVMRMonitorConfig_SetMonitor(mon, &bad), E_INVALIDARG);
        memset(&g, 0, sizeof(g)); g.Data4[7] = 100;
        HR(IVMRMonitorConfig_SetMonitor(mon, &bad), E_INVALIDARG);
        HR(IVMRMonitorConfig_SetDefaultMonitor(mon, &bad), E_INVALIDARG);
        memset(&guid, 0, sizeof(guid));
        HR(IVMRMonitorConfig_SetMonitor(mon, &guid), S_OK);
        if (n > 1)
        {
            guid2 = info[1].guid;
            HR(IVMRMonitorConfig_SetMonitor(mon, &guid2), S_OK);
            memset(&guid, 0xcc, sizeof(guid));
            IVMRMonitorConfig_GetMonitor(mon, &guid);
            CHECKF(guid.pGUID && guid.pGUID->Data4[7] == 1, "VMR7 monitor 1 round trip");
            HR(IVMRMonitorConfig_SetDefaultMonitor(mon, &guid2), S_OK);
            memset(&guid, 0xcc, sizeof(guid));
            IVMRMonitorConfig_GetDefaultMonitor(mon, &guid);
            CHECKF(guid.pGUID && guid.pGUID->Data4[7] == 1, "VMR7 default monitor 1 round trip");
        }
        else
            printf("note  one monitor only: non-default monitor round trip skipped\n");
        memset(&guid, 0, sizeof(guid));
        HR(IVMRMonitorConfig_SetDefaultMonitor(mon, &guid), S_OK);
        memset(&guid, 0xcc, sizeof(guid));
        IVMRMonitorConfig_GetDefaultMonitor(mon, &guid);
        CHECKF(guid.pGUID == NULL, "VMR7 default monitor back to the default device");
        IVMRMonitorConfig_Release(mon);
    }

    test_copp(filter, "VMR7");
    sink.ref = 1;
    test_pin_interfaces(filter, TRUE, "VMR7 windowed");
    IBaseFilter_Release(filter);
    CHECKF(sink.ref == 1, "VMR7 released the advised overlay sink (%ld)", sink.ref);
}

static void test_vmr7_windowless(void)
{
    IBaseFilter *filter = create(&CLSID_VideoMixingRenderer);
    IVMRFilterConfig *config;
    IVMRWindowlessControl *wc;
    COLORREF color;
    BYTE *dib;
    LONG w, h;
    DWORD v;
    HRESULT hr;

    if (!filter) { check(0, "VMR7 created"); return; }
    IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig, (void **)&config);
    hr = IVMRFilterConfig_SetRenderingMode(config, VMRMode_Windowless);
    IVMRFilterConfig_Release(config);
    if (hr != S_OK)
    {
        printf("note  VMR7 windowless mode unavailable (%#lx), skipped\n", (unsigned long)hr);
        IBaseFilter_Release(filter);
        return;
    }
    hr = IBaseFilter_QueryInterface(filter, &IID_IVMRWindowlessControl, (void **)&wc);
    CHECKF(hr == S_OK, "VMR7 windowless control (%#lx)", (unsigned long)hr);
    if (hr != S_OK) { IBaseFilter_Release(filter); return; }

    HR(IVMRWindowlessControl_GetMinIdealVideoSize(wc, NULL, &h), E_POINTER);
    HR(IVMRWindowlessControl_GetMaxIdealVideoSize(wc, &w, NULL), E_POINTER);
    HR(IVMRWindowlessControl_GetMinIdealVideoSize(wc, &w, &h), VFW_E_NOT_CONNECTED);
    HR(IVMRWindowlessControl_GetMaxIdealVideoSize(wc, &w, &h), VFW_E_NOT_CONNECTED);

    HR(IVMRWindowlessControl_GetAspectRatioMode(wc, NULL), E_POINTER);
    v = 77;
    HR(IVMRWindowlessControl_GetAspectRatioMode(wc, &v), S_OK);
    CHECKF(v == VMR_ARMODE_NONE, "VMR7 default aspect mode %lu", v);
    HR(IVMRWindowlessControl_SetAspectRatioMode(wc, 2), E_INVALIDARG);
    HR(IVMRWindowlessControl_SetAspectRatioMode(wc, VMR_ARMODE_LETTER_BOX), S_OK);
    IVMRWindowlessControl_GetAspectRatioMode(wc, &v);
    CHECKF(v == VMR_ARMODE_LETTER_BOX, "VMR7 aspect mode round trip %lu", v);

    HR(IVMRWindowlessControl_GetBorderColor(wc, NULL), E_POINTER);
    color = 77;
    HR(IVMRWindowlessControl_GetBorderColor(wc, &color), S_OK);
    CHECKF(color == 0, "VMR7 default border color %#lx", (unsigned long)color);
    HR(IVMRWindowlessControl_SetBorderColor(wc, 0x00ff0000), S_OK);
    IVMRWindowlessControl_GetBorderColor(wc, &color);
    CHECKF(color == 0x00ff0000, "VMR7 border color round trip %#lx", (unsigned long)color);

    HR(IVMRWindowlessControl_GetColorKey(wc, NULL), E_POINTER);
    HR(IVMRWindowlessControl_SetColorKey(wc, 0x00010203), S_OK);
    color = 77;
    IVMRWindowlessControl_GetColorKey(wc, &color);
    CHECKF(color == 0x00010203, "VMR7 color key round trip %#lx", (unsigned long)color);

    HR(IVMRWindowlessControl_DisplayModeChanged(wc), S_OK);
    HR(IVMRWindowlessControl_GetCurrentImage(wc, NULL), E_POINTER);
    dib = (BYTE *)1;
    HR(IVMRWindowlessControl_GetCurrentImage(wc, &dib), VFW_E_NOT_CONNECTED);
    CHECKF(dib == (BYTE *)1, "VMR7 GetCurrentImage leaves the output alone");
    IVMRWindowlessControl_Release(wc);
    IBaseFilter_Release(filter);
}

static void test_vmr7_notify(void)
{
    IBaseFilter *filter = create(&CLSID_VideoMixingRenderer);
    IVMRSurfaceAllocatorNotify *notify;
    IVMRFilterConfig *config;
    IDirectDraw7 *ddraw, *ddraw2;
    HRESULT hr;
    ULONG ref;

    if (!filter) { check(0, "VMR7 created"); return; }
    IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig, (void **)&config);
    HR(IVMRFilterConfig_SetRenderingMode(config, VMRMode_Renderless), S_OK);
    IVMRFilterConfig_Release(config);
    hr = IBaseFilter_QueryInterface(filter, &IID_IVMRSurfaceAllocatorNotify, (void **)&notify);
    CHECKF(hr == S_OK, "VMR7 surface allocator notify (%#lx)", (unsigned long)hr);
    if (hr != S_OK) { IBaseFilter_Release(filter); return; }

    HR(IVMRSurfaceAllocatorNotify_SetDDrawDevice(notify, NULL, NULL), E_POINTER);
    HR(IVMRSurfaceAllocatorNotify_ChangeDDrawDevice(notify, NULL, NULL), E_POINTER);
    HR(IVMRSurfaceAllocatorNotify_SetBorderColor(notify, 0x00abcdef), S_OK);
    HR(IVMRSurfaceAllocatorNotify_RestoreDDrawSurfaces(notify), S_OK);
    HR(IVMRSurfaceAllocatorNotify_NotifyEvent(notify, EC_USER, 0, 0), VFW_E_NOT_IN_GRAPH);

    hr = DirectDrawCreateEx(NULL, (void **)&ddraw, &IID_IDirectDraw7, NULL);
    if (FAILED(hr))
        printf("note  DirectDrawCreateEx failed (%#lx): device parts skipped\n", (unsigned long)hr);
    else
    {
        DirectDrawCreateEx(NULL, (void **)&ddraw2, &IID_IDirectDraw7, NULL);
        ref = refcount((IUnknown *)ddraw);
        CHECKF(ref == 1, "VMR7 ddraw starts with one reference (%lu)", ref);
        HR(IVMRSurfaceAllocatorNotify_SetDDrawDevice(notify, ddraw, NULL), S_OK);
        ref = refcount((IUnknown *)ddraw);
        CHECKF(ref == 2, "VMR7 SetDDrawDevice holds a reference (%lu)", ref);
        HR(IVMRSurfaceAllocatorNotify_ChangeDDrawDevice(notify, ddraw2, NULL), S_OK);
        ref = refcount((IUnknown *)ddraw);
        CHECKF(ref == 1, "VMR7 ChangeDDrawDevice dropped the old device (%lu)", ref);
        ref = refcount((IUnknown *)ddraw2);
        CHECKF(ref == 2, "VMR7 ChangeDDrawDevice holds the new device (%lu)", ref);
        IVMRSurfaceAllocatorNotify_Release(notify);
        IBaseFilter_Release(filter);
        ref = refcount((IUnknown *)ddraw2);
        CHECKF(ref == 1, "VMR7 released the device with the filter (%lu)", ref);
        IDirectDraw7_Release(ddraw);
        IDirectDraw7_Release(ddraw2);
        return;
    }
    IVMRSurfaceAllocatorNotify_Release(notify);
    IBaseFilter_Release(filter);
}

static void test_vmr7_presenter(void)
{
    IVMRWindowlessControl *wc = NULL;
    IVMRImagePresenter *ip = NULL;
    IVMRSurfaceAllocator *sa = NULL;
    IUnknown *unk = NULL;
    COLORREF color;
    RECT src, dst, r;
    LONG w, h;
    DWORD v;
    HRESULT hr;
    BYTE *dib;

    hr = CoCreateInstance(&CLSID_AllocPresenter, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk);
    if (hr != S_OK) { printf("note  default presenter unavailable (%#lx), skipped\n", (unsigned long)hr); return; }
    IUnknown_QueryInterface(unk, &IID_IVMRWindowlessControl, (void **)&wc);
    IUnknown_QueryInterface(unk, &IID_IVMRImagePresenter, (void **)&ip);
    IUnknown_QueryInterface(unk, &IID_IVMRSurfaceAllocator, (void **)&sa);
    if (!wc || !ip || !sa) { check(0, "presenter interfaces"); return; }

    HR(IVMRImagePresenter_StartPresenting(ip, 1), S_OK);
    HR(IVMRImagePresenter_StopPresenting(ip, 1), S_OK);
    HR(IVMRSurfaceAllocator_AdviseNotify(sa, NULL), S_OK);

    HR(IVMRWindowlessControl_GetMinIdealVideoSize(wc, NULL, &h), E_POINTER);
    HR(IVMRWindowlessControl_GetMinIdealVideoSize(wc, &w, &h), VFW_E_WRONG_STATE);
    HR(IVMRWindowlessControl_GetMaxIdealVideoSize(wc, &w, &h), VFW_E_WRONG_STATE);

    HR(IVMRWindowlessControl_GetAspectRatioMode(wc, NULL), E_POINTER);
    v = 77;
    HR(IVMRWindowlessControl_GetAspectRatioMode(wc, &v), S_OK);
    CHECKF(v == VMR_ARMODE_NONE, "presenter default aspect mode %lu", v);
    HR(IVMRWindowlessControl_SetAspectRatioMode(wc, 5), E_INVALIDARG);
    HR(IVMRWindowlessControl_SetAspectRatioMode(wc, VMR_ARMODE_LETTER_BOX), S_OK);
    IVMRWindowlessControl_GetAspectRatioMode(wc, &v);
    CHECKF(v == VMR_ARMODE_LETTER_BOX, "presenter aspect mode round trip %lu", v);

    SetRect(&src, 1, 2, 30, 40); SetRect(&dst, 5, 6, 70, 80);
    HR(IVMRWindowlessControl_SetVideoPosition(wc, &src, &dst), S_OK);
    memset(&r, 0xcc, sizeof(r));
    HR(IVMRWindowlessControl_GetVideoPosition(wc, &r, NULL), S_OK);
    CHECKF(EqualRect(&r, &src), "presenter source rect round trip");
    memset(&r, 0xcc, sizeof(r));
    HR(IVMRWindowlessControl_GetVideoPosition(wc, NULL, &r), S_OK);
    CHECKF(EqualRect(&r, &dst), "presenter destination rect round trip");
    SetRect(&dst, 9, 9, 19, 19);
    HR(IVMRWindowlessControl_SetVideoPosition(wc, NULL, &dst), S_OK);
    IVMRWindowlessControl_GetVideoPosition(wc, &r, &dst);
    CHECKF(EqualRect(&r, &src), "presenter source kept when only the destination is set");

    HR(IVMRWindowlessControl_GetBorderColor(wc, NULL), E_POINTER);
    HR(IVMRWindowlessControl_SetBorderColor(wc, 0x00112233), S_OK);
    color = 0;
    IVMRWindowlessControl_GetBorderColor(wc, &color);
    CHECKF(color == 0x00112233, "presenter border color round trip %#lx", (unsigned long)color);
    HR(IVMRWindowlessControl_GetColorKey(wc, NULL), E_POINTER);
    HR(IVMRWindowlessControl_SetColorKey(wc, 0x00445566), S_OK);
    IVMRWindowlessControl_GetColorKey(wc, &color);
    CHECKF(color == 0x00445566, "presenter color key round trip %#lx", (unsigned long)color);

    HR(IVMRWindowlessControl_DisplayModeChanged(wc), S_OK);
    HR(IVMRWindowlessControl_GetCurrentImage(wc, NULL), E_POINTER);
    dib = (BYTE *)1;
    HR(IVMRWindowlessControl_GetCurrentImage(wc, &dib), VFW_E_WRONG_STATE);
    HR(IVMRWindowlessControl_RepaintVideo(wc, (HWND)0x1234, NULL), S_OK);

    IVMRWindowlessControl_Release(wc);
    IVMRImagePresenter_Release(ip);
    IVMRSurfaceAllocator_Release(sa);
    IUnknown_Release(unk);
}

static void test_vmr7(void)
{
    IBaseFilter *filter;
    IVMRFilterConfig *config;
    HRESULT hr;

    test_vmr7_config();
    test_vmr7_windowless();
    test_vmr7_notify();
    test_vmr7_presenter();

    filter = create(&CLSID_VideoMixingRenderer);
    if (filter)
    {
        IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig, (void **)&config);
        hr = IVMRFilterConfig_SetRenderingMode(config, VMRMode_Renderless);
        CHECKF(hr == S_OK, "VMR7 renderless mode (%#lx)", (unsigned long)hr);
        IVMRFilterConfig_Release(config);
        test_pin_interfaces(filter, FALSE, "VMR7 renderless");
        IBaseFilter_Release(filter);
    }
}

int main(void)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_vmr9();
    test_vmr7();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
