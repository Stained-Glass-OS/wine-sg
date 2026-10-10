/* Probe for the quartz stubs of patches 2941-2949: enumerator Skip/Clone of
 * the filter mapper, the video renderer's IOverlay, IVideoWindow / IBasicVideo
 * helpers, the DirectSound renderer's IAMDirectSound / IQualityControl. */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <amaudio.h>
#include <stdio.h>

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_HR(hr, exp, what) CHECK((hr) == (exp), "%s: got %#lx, expected %#lx", what, (long)(hr), (long)(exp))

static int nb_palette, nb_color;
static HRESULT WINAPI n_qi(IOverlayNotify *i, REFIID r, void **o)
{ if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IOverlayNotify)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI n_addref(IOverlayNotify *i) { return 2; }
static ULONG WINAPI n_release(IOverlayNotify *i) { return 1; }
static HRESULT WINAPI n_pal(IOverlayNotify *i, DWORD c, const PALETTEENTRY *p) { nb_palette += c; return S_OK; }
static HRESULT WINAPI n_clip(IOverlayNotify *i, const RECT *a, const RECT *b, const RGNDATA *c) { return S_OK; }
static HRESULT WINAPI n_key(IOverlayNotify *i, const COLORKEY *k) { nb_color++; return S_OK; }
static HRESULT WINAPI n_pos(IOverlayNotify *i, const RECT *a, const RECT *b) { return S_OK; }
static IOverlayNotifyVtbl notify_vtbl = { n_qi, n_addref, n_release, n_pal, n_clip, n_key, n_pos };
static IOverlayNotify notify = { &notify_vtbl };

static void test_mapper(void)
{
    IFilterMapper2 *mapper2;
    IFilterMapper *mapper;
    IEnumMoniker *em, *clone;
    IEnumRegFilters *er, *rclone;
    IMoniker *m[8], *mo;
    REGFILTER *rf;
    ULONG n, total, i;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper2, (void **)&mapper2);
    CHECK_HR(hr, S_OK, "create FilterMapper2");
    if (hr != S_OK) return;
    hr = IFilterMapper2_EnumMatchingFilters(mapper2, &em, 0, FALSE, 0, FALSE, 0, NULL, NULL, &GUID_NULL,
            FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
    CHECK_HR(hr, S_OK, "EnumMatchingFilters");
    total = 0;
    while (IEnumMoniker_Next(em, 1, &mo, &n) == S_OK) { if (total < 8) m[total] = mo; else IMoniker_Release(mo); total++; }
    CHECK(total >= 4, "enumerated %lu monikers", total);
    if (total >= 4)
    {
        hr = IEnumMoniker_Reset(em);
        CHECK_HR(hr, S_OK, "Reset");
        hr = IEnumMoniker_Skip(em, 2);
        CHECK_HR(hr, S_OK, "Skip(2)");
        hr = IEnumMoniker_Next(em, 1, &mo, &n);
        CHECK_HR(hr, S_OK, "Next after Skip");
        CHECK(IMoniker_IsEqual(mo, m[2]) == S_OK, "Next after Skip(2) is not the third moniker");
        IMoniker_Release(mo);
        clone = NULL;
        hr = IEnumMoniker_Clone(em, &clone);
        CHECK_HR(hr, S_OK, "Clone");
        if (clone)
        {
            hr = IEnumMoniker_Next(clone, 1, &mo, &n);
            CHECK_HR(hr, S_OK, "clone Next");
            CHECK(IMoniker_IsEqual(mo, m[3]) == S_OK, "clone did not keep the position");
            IMoniker_Release(mo);
            hr = IEnumMoniker_Next(em, 1, &mo, &n);
            CHECK(hr == S_OK && IMoniker_IsEqual(mo, m[3]) == S_OK, "original moved by the clone");
            if (hr == S_OK) IMoniker_Release(mo);
            IEnumMoniker_Release(em);
            em = clone;   /* the clone outlives the original: it owns references */
        }
        hr = IEnumMoniker_Reset(em);
        hr = IEnumMoniker_Skip(em, total);
        CHECK_HR(hr, S_OK, "Skip(total)");
        hr = IEnumMoniker_Skip(em, 1);
        CHECK_HR(hr, S_FALSE, "Skip past the end");
        hr = IEnumMoniker_Next(em, 1, &mo, &n);
        CHECK_HR(hr, S_FALSE, "Next at the end");
        hr = IEnumMoniker_Clone(em, NULL);
        CHECK_HR(hr, E_POINTER, "Clone(NULL)");
    }
    for (i = 0; i < total && i < 8; i++) IMoniker_Release(m[i]);
    IEnumMoniker_Release(em);
    IFilterMapper2_Release(mapper2);

    hr = CoCreateInstance(&CLSID_FilterMapper, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper, (void **)&mapper);
    CHECK_HR(hr, S_OK, "create FilterMapper");
    if (hr != S_OK) return;
    hr = IFilterMapper_EnumMatchingFilters(mapper, &er, 0, FALSE, GUID_NULL, GUID_NULL, FALSE, FALSE, GUID_NULL, GUID_NULL);
    CHECK_HR(hr, S_OK, "legacy EnumMatchingFilters");
    total = 0;
    while (IEnumRegFilters_Next(er, 1, &rf, &n) == S_OK) total++;
    CHECK(total >= 4, "legacy enumerated %lu", total);
    if (total >= 4)
    {
        CLSID c2, c3;
        IEnumRegFilters_Reset(er);
        IEnumRegFilters_Next(er, 1, &rf, &n); IEnumRegFilters_Next(er, 1, &rf, &n);
        IEnumRegFilters_Next(er, 1, &rf, &n); c2 = rf->Clsid; CoTaskMemFree(rf);
        IEnumRegFilters_Next(er, 1, &rf, &n); c3 = rf->Clsid; CoTaskMemFree(rf);
        IEnumRegFilters_Reset(er);
        hr = IEnumRegFilters_Skip(er, 2);
        CHECK_HR(hr, S_OK, "legacy Skip(2)");
        hr = IEnumRegFilters_Next(er, 1, &rf, &n);
        CHECK(hr == S_OK && IsEqualGUID(&rf->Clsid, &c2), "legacy Next after Skip");
        if (hr == S_OK) CoTaskMemFree(rf);
        rclone = NULL;
        hr = IEnumRegFilters_Clone(er, &rclone);
        CHECK_HR(hr, S_OK, "legacy Clone");
        if (rclone)
        {
            hr = IEnumRegFilters_Next(rclone, 1, &rf, &n);
            CHECK(hr == S_OK && IsEqualGUID(&rf->Clsid, &c3), "legacy clone keeps position");
            if (hr == S_OK) CoTaskMemFree(rf);
            hr = IEnumRegFilters_Next(er, 1, &rf, &n);
            CHECK(hr == S_OK && IsEqualGUID(&rf->Clsid, &c3), "legacy original independent");
            if (hr == S_OK) CoTaskMemFree(rf);
            IEnumRegFilters_Release(rclone);
        }
        IEnumRegFilters_Reset(er);
        hr = IEnumRegFilters_Skip(er, total);
        CHECK_HR(hr, S_OK, "legacy Skip(total)");
        hr = IEnumRegFilters_Skip(er, 1);
        CHECK_HR(hr, S_FALSE, "legacy Skip past the end");
        hr = IEnumRegFilters_Clone(er, NULL);
        CHECK_HR(hr, E_POINTER, "legacy Clone(NULL)");
    }
    IEnumRegFilters_Release(er);
    IFilterMapper_Release(mapper);
}

static void test_video(void)
{
    IBaseFilter *filter;
    IPin *pin;
    IOverlay *overlay;
    IVideoWindow *window;
    IBasicVideo *video;
    PALETTEENTRY pal[2] = {{1,2,3,4},{5,6,7,8}}, *got;
    COLORKEY key = {CK_RGB, 0, 0x112233, 0x445566}, key2, def;
    DWORD count;
    RGNDATA *rgn;
    RECT src, dst;
    LONG l, a, b, c, d;
    HWND hwnd;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
    CHECK_HR(hr, S_OK, "create VideoRenderer");
    if (hr != S_OK) return;
    IBaseFilter_FindPin(filter, L"In", &pin);
    hr = IPin_QueryInterface(pin, &IID_IOverlay, (void **)&overlay);
    CHECK_HR(hr, S_OK, "QI IOverlay");

    count = 77; got = (void *)1;
    hr = IOverlay_GetPalette(overlay, &count, &got);
    CHECK_HR(hr, VFW_E_NO_PALETTE_AVAILABLE, "GetPalette empty");
    CHECK(count == 0 && !got, "GetPalette empty outputs %lu %p", count, got);
    hr = IOverlay_GetPalette(overlay, NULL, &got);
    CHECK_HR(hr, E_POINTER, "GetPalette(NULL count)");
    hr = IOverlay_SetPalette(overlay, 257, pal);
    CHECK_HR(hr, E_INVALIDARG, "SetPalette(257)");
    hr = IOverlay_SetPalette(overlay, 2, NULL);
    CHECK_HR(hr, E_POINTER, "SetPalette(NULL)");
    hr = IOverlay_SetPalette(overlay, 2, pal);
    CHECK_HR(hr, S_OK, "SetPalette");
    hr = IOverlay_GetPalette(overlay, &count, &got);
    CHECK_HR(hr, S_OK, "GetPalette");
    CHECK(count == 2 && got && !memcmp(got, pal, sizeof(pal)), "GetPalette values (count %lu)", count);
    CoTaskMemFree(got);
    hr = IOverlay_SetPalette(overlay, 0, NULL);
    CHECK_HR(hr, S_OK, "SetPalette(0)");
    hr = IOverlay_GetPalette(overlay, &count, &got);
    CHECK_HR(hr, VFW_E_NO_PALETTE_AVAILABLE, "GetPalette after reset");

    hr = IOverlay_GetColorKey(overlay, &key2);
    CHECK_HR(hr, VFW_E_NO_COLOR_KEY_SET, "GetColorKey unset");
    hr = IOverlay_GetDefaultColorKey(overlay, &def);
    CHECK_HR(hr, S_OK, "GetDefaultColorKey");
    CHECK(def.KeyType == CK_RGB && def.LowColorValue == def.HighColorValue && def.LowColorValue == RGB(255, 0, 255),
            "default key %#lx %#lx %#lx", (long)def.KeyType, (long)def.LowColorValue, (long)def.HighColorValue);
    hr = IOverlay_GetDefaultColorKey(overlay, NULL);
    CHECK_HR(hr, E_POINTER, "GetDefaultColorKey(NULL)");
    hr = IOverlay_SetColorKey(overlay, NULL);
    CHECK_HR(hr, E_POINTER, "SetColorKey(NULL)");
    hr = IOverlay_SetColorKey(overlay, &key);
    CHECK_HR(hr, S_OK, "SetColorKey");
    memset(&key2, 0, sizeof(key2));
    hr = IOverlay_GetColorKey(overlay, &key2);
    CHECK_HR(hr, S_OK, "GetColorKey");
    CHECK(!memcmp(&key, &key2, sizeof(key)), "color key roundtrip");

    hr = IOverlay_GetWindowHandle(overlay, &hwnd);
    CHECK(hr == S_OK && IsWindow(hwnd), "window handle");
    hr = IOverlay_GetVideoPosition(overlay, &src, &dst);
    CHECK_HR(hr, S_OK, "GetVideoPosition");
    hr = IOverlay_GetVideoPosition(overlay, NULL, &dst);
    CHECK_HR(hr, E_POINTER, "GetVideoPosition(NULL)");
    rgn = NULL;
    hr = IOverlay_GetClipList(overlay, &src, &dst, &rgn);
    CHECK(hr == S_OK && rgn && rgn->rdh.dwSize == sizeof(RGNDATAHEADER) && rgn->rdh.iType == RDH_RECTANGLES, "GetClipList %#lx", (long)hr);
    if (rgn) { CHECK(rgn->rdh.nCount == 0, "hidden window has %lu clip rects", (long)rgn->rdh.nCount); CoTaskMemFree(rgn); }
    hr = IOverlay_GetClipList(overlay, &src, &dst, NULL);
    CHECK_HR(hr, E_POINTER, "GetClipList(NULL)");

    hr = IOverlay_Unadvise(overlay);
    CHECK_HR(hr, VFW_E_NO_ADVISE_SET, "Unadvise unset");
    hr = IOverlay_Advise(overlay, NULL, ADVISE_PALETTE);
    CHECK_HR(hr, E_POINTER, "Advise(NULL)");
    hr = IOverlay_Advise(overlay, &notify, 0x100);
    CHECK_HR(hr, E_INVALIDARG, "Advise(bad flags)");
    hr = IOverlay_Advise(overlay, &notify, ADVISE_PALETTE);
    CHECK_HR(hr, S_OK, "Advise");
    hr = IOverlay_Advise(overlay, &notify, ADVISE_PALETTE);
    CHECK_HR(hr, VFW_E_ADVISE_ALREADY_SET, "Advise twice");
    IOverlay_SetPalette(overlay, 2, pal);
    IOverlay_SetColorKey(overlay, &key);
    CHECK(nb_palette == 2, "palette notification count %d", nb_palette);
    CHECK(nb_color == 0, "colour key notification without ADVISE_COLORKEY: %d", nb_color);
    hr = IOverlay_Unadvise(overlay);
    CHECK_HR(hr, S_OK, "Unadvise");
    IOverlay_SetPalette(overlay, 2, pal);
    CHECK(nb_palette == 2, "notified after Unadvise");
    hr = IOverlay_Advise(overlay, &notify, ADVISE_COLORKEY);
    CHECK_HR(hr, S_OK, "Advise colorkey");
    IOverlay_SetColorKey(overlay, &key);
    CHECK(nb_color == 1, "colour key notifications %d", nb_color);
    IOverlay_SetPalette(overlay, 2, pal);
    CHECK(nb_palette == 2, "palette notification without ADVISE_PALETTE: %d", nb_palette);
    IOverlay_Release(overlay);
    IPin_Release(pin);

    hr = IBaseFilter_QueryInterface(filter, &IID_IVideoWindow, (void **)&window);
    CHECK_HR(hr, S_OK, "QI IVideoWindow");
    l = 7;
    hr = IVideoWindow_get_BackgroundPalette(window, &l);
    CHECK(hr == S_OK && l == OAFALSE, "default BackgroundPalette %#lx %ld", (long)hr, l);
    hr = IVideoWindow_put_BackgroundPalette(window, 5);
    CHECK_HR(hr, E_INVALIDARG, "put_BackgroundPalette(5)");
    hr = IVideoWindow_put_BackgroundPalette(window, OATRUE);
    CHECK_HR(hr, S_OK, "put_BackgroundPalette(OATRUE)");
    IVideoWindow_get_BackgroundPalette(window, &l);
    CHECK(l == OATRUE, "BackgroundPalette %ld", l);
    hr = IVideoWindow_get_BackgroundPalette(window, NULL);
    CHECK_HR(hr, E_POINTER, "get_BackgroundPalette(NULL)");
    hr = IVideoWindow_put_BorderColor(window, 0x123456);
    CHECK_HR(hr, S_OK, "put_BorderColor");
    l = 0;
    hr = IVideoWindow_get_BorderColor(window, &l);
    CHECK(hr == S_OK && l == 0x123456, "BorderColor %#lx %#lx", (long)hr, l);
    hr = IVideoWindow_get_BorderColor(window, NULL);
    CHECK_HR(hr, E_POINTER, "get_BorderColor(NULL)");
    l = 9;
    hr = IVideoWindow_IsCursorHidden(window, &l);
    CHECK(hr == S_OK && l == OAFALSE, "cursor default %#lx %ld", (long)hr, l);
    hr = window->lpVtbl->HideCursor(window, 3);
    CHECK_HR(hr, E_INVALIDARG, "HideCursor(3)");
    hr = window->lpVtbl->HideCursor(window, OATRUE);
    CHECK_HR(hr, S_OK, "HideCursor");
    IVideoWindow_IsCursorHidden(window, &l);
    CHECK(l == OATRUE, "cursor hidden %ld", l);
    window->lpVtbl->HideCursor(window, OAFALSE);
    IVideoWindow_IsCursorHidden(window, &l);
    CHECK(l == OAFALSE, "cursor shown %ld", l);
    hr = IVideoWindow_IsCursorHidden(window, NULL);
    CHECK_HR(hr, E_POINTER, "IsCursorHidden(NULL)");
    a = b = c = d = -1;
    hr = IVideoWindow_GetRestorePosition(window, &a, &b, &c, &d);
    CHECK(hr == S_OK && c >= 0 && d >= 0, "GetRestorePosition %#lx %ld %ld %ld %ld", (long)hr, a, b, c, d);
    hr = IVideoWindow_GetRestorePosition(window, &a, &b, &c, NULL);
    CHECK_HR(hr, E_POINTER, "GetRestorePosition(NULL)");
    IVideoWindow_Release(window);

    hr = IBaseFilter_QueryInterface(filter, &IID_IBasicVideo, (void **)&video);
    CHECK_HR(hr, S_OK, "QI IBasicVideo");
    hr = IBasicVideo_GetVideoPaletteEntries(video, 0, 1, NULL, &l);
    CHECK_HR(hr, E_POINTER, "GetVideoPaletteEntries(NULL count)");
    l = 5;
    hr = IBasicVideo_GetVideoPaletteEntries(video, 0, 1, &l, NULL);
    CHECK_HR(hr, VFW_E_NOT_CONNECTED, "GetVideoPaletteEntries unconnected");
    CHECK(l == 0, "ret_count %ld", l);
    IBasicVideo_Release(video);
    IBaseFilter_Release(filter);
}

static void test_dsound(void)
{
    IBaseFilter *filter;
    IAMDirectSound *ds;
    IQualityControl *qc;
    HRESULT hr;
    HWND hwnd = NULL;
    BOOL bg;
    IClassFactory *cf;

    hr = CoCreateInstance(&CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
    CHECK_HR(hr, S_OK, "create DSoundRender");
    if (hr != S_OK) return;
    hr = IBaseFilter_QueryInterface(filter, &IID_IAMDirectSound, (void **)&ds);
    CHECK_HR(hr, S_OK, "QI IAMDirectSound");
    if (hr == S_OK)
    {
        IDirectSound *s = NULL; IDirectSoundBuffer *buf = NULL;
        CHECK_HR(ds->lpVtbl->GetDirectSoundInterface(ds, &s), E_NOTIMPL, "GetDirectSoundInterface");
        CHECK_HR(ds->lpVtbl->GetPrimaryBufferInterface(ds, &buf), E_NOTIMPL, "GetPrimaryBufferInterface");
        CHECK_HR(ds->lpVtbl->GetSecondaryBufferInterface(ds, &buf), E_NOTIMPL, "GetSecondaryBufferInterface");
        CHECK_HR(ds->lpVtbl->ReleaseDirectSoundInterface(ds, s), E_NOTIMPL, "ReleaseDirectSoundInterface");
        CHECK_HR(ds->lpVtbl->ReleasePrimaryBufferInterface(ds, buf), E_NOTIMPL, "ReleasePrimaryBufferInterface");
        CHECK_HR(ds->lpVtbl->ReleaseSecondaryBufferInterface(ds, buf), E_NOTIMPL, "ReleaseSecondaryBufferInterface");
        CHECK_HR(ds->lpVtbl->SetFocusWindow(ds, GetDesktopWindow(), TRUE), E_NOTIMPL, "SetFocusWindow");
        CHECK_HR(ds->lpVtbl->GetFocusWindow(ds, &hwnd, &bg), E_NOTIMPL, "GetFocusWindow");
        ds->lpVtbl->Release(ds);
    }
    hr = IBaseFilter_QueryInterface(filter, &IID_IQualityControl, (void **)&qc);
    CHECK_HR(hr, S_OK, "QI IQualityControl");
    if (hr == S_OK)
    {
        Quality q = {0};
        hr = IQualityControl_SetSink(qc, qc);
        CHECK_HR(hr, S_OK, "SetSink");
        hr = IQualityControl_SetSink(qc, NULL);
        CHECK_HR(hr, S_OK, "SetSink(NULL)");
        hr = IQualityControl_Notify(qc, filter, q);
        CHECK_HR(hr, E_NOTIMPL, "Notify");
        IQualityControl_Release(qc);
    }
    IBaseFilter_Release(filter);

    hr = CoGetClassObject(&CLSID_DSoundRender, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    CHECK_HR(hr, S_OK, "class factory");
    if (hr == S_OK)
    {
        CHECK_HR(IClassFactory_LockServer(cf, TRUE), S_OK, "LockServer(TRUE)");
        CHECK_HR(IClassFactory_LockServer(cf, FALSE), S_OK, "LockServer(FALSE)");
        IClassFactory_Release(cf);
    }
}

int main(void)
{
    CoInitialize(NULL);
    test_mapper();
    test_video();
    test_dsound();
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
