/* Probe for patches/sg/5078: IViewObject::Freeze / Unfreeze on the OLE data cache.
 * - a cached aspect freezes (S_OK, a key); freezing it again gives VIEW_S_ALREADY_FROZEN and
 *   the same key; an aspect the cache holds nothing for is OLE_E_BLANK (ole32 ole2 tests);
 *   lindex other than -1 is DV_E_LINDEX, an unknown aspect DV_E_DVASPECT (documented);
 * - while frozen no view notification goes out; Unfreeze sends the one held back;
 * - Unfreeze of a key that is not frozen is OLE_E_NOCONNECTION.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int view_changes;
static HRESULT WINAPI s_qi(IAdviseSink *i, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IAdviseSink)) { *out = i; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI s_addref(IAdviseSink *i) { return 2; }
static ULONG WINAPI s_release(IAdviseSink *i) { return 1; }
static void WINAPI s_ondatachange(IAdviseSink *i, FORMATETC *f, STGMEDIUM *m) {}
static void WINAPI s_onviewchange(IAdviseSink *i, DWORD aspect, LONG index) { view_changes++; }
static void WINAPI s_onrename(IAdviseSink *i, IMoniker *m) {}
static void WINAPI s_onsave(IAdviseSink *i) {}
static void WINAPI s_onclose(IAdviseSink *i) {}
static IAdviseSinkVtbl sink_vtbl = { s_qi, s_addref, s_release, s_ondatachange, s_onviewchange, s_onrename, s_onsave, s_onclose };
static IAdviseSink sink = { &sink_vtbl };

static HRESULT set_icon(IOleCache2 *cache)
{
    FORMATETC fmt = { CF_METAFILEPICT, NULL, DVASPECT_ICON, -1, TYMED_MFPICT };
    STGMEDIUM med;
    med.tymed = TYMED_MFPICT;
    med.hMetaFilePict = OleMetafilePictFromIconAndLabel(LoadIconA(NULL, (LPSTR)IDI_APPLICATION), (WCHAR *)L"x", (WCHAR *)L"x", 0);
    med.pUnkForRelease = NULL;
    return IOleCache2_SetData(cache, &fmt, &med, TRUE);
}

int main(void)
{
    FORMATETC fmt = { CF_METAFILEPICT, NULL, DVASPECT_ICON, -1, TYMED_MFPICT };
    IOleCache2 *cache = NULL;
    IViewObject *view = NULL;
    DWORD conn, key = 0, key2 = 0;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CreateDataCache(NULL, &CLSID_NULL, &IID_IOleCache2, (void **)&cache);
    check(hr == S_OK, "CreateDataCache %#lx", hr);
    if (!cache) goto done;
    IOleCache2_Cache(cache, &fmt, 0, &conn);
    hr = set_icon(cache);
    check(hr == S_OK, "icon cached %#lx", hr);
    IOleCache2_QueryInterface(cache, &IID_IViewObject, (void **)&view);
    IViewObject_SetAdvise(view, DVASPECT_ICON, 0, &sink);

    hr = IViewObject_Freeze(view, DVASPECT_ICON, -1, NULL, &key);
    check(hr == S_OK, "Freeze(ICON) %#lx key %lu", hr, key);
    hr = IViewObject_Freeze(view, DVASPECT_ICON, -1, NULL, &key2);
    check(hr == VIEW_S_ALREADY_FROZEN && key2 == key, "Freeze(ICON) again %#lx key %lu", hr, key2);
    hr = IViewObject_Freeze(view, DVASPECT_CONTENT, -1, NULL, &key2);
    check(hr == OLE_E_BLANK, "Freeze(CONTENT), nothing cached %#lx", hr);
    hr = IViewObject_Freeze(view, DVASPECT_ICON, 0, NULL, &key2);
    check(hr == DV_E_LINDEX, "lindex 0 %#lx", hr);
    hr = IViewObject_Freeze(view, 3, -1, NULL, &key2);
    check(hr == DV_E_DVASPECT, "aspect 3 %#lx", hr);

    view_changes = 0;
    hr = set_icon(cache);
    check(hr == S_OK && view_changes == 0, "new data while frozen: %#lx, %d view notifications", hr, view_changes);
    hr = IViewObject_Unfreeze(view, key);
    check(hr == S_OK && view_changes == 1, "Unfreeze %#lx: the notification held back went out (%d)", hr, view_changes);
    hr = IViewObject_Unfreeze(view, key);
    check(hr == OLE_E_NOCONNECTION, "Unfreeze again %#lx", hr);
    view_changes = 0;
    set_icon(cache);
    check(view_changes == 1, "notifications again once unfrozen: %d", view_changes);

    IViewObject_SetAdvise(view, DVASPECT_ICON, 0, NULL);
    IViewObject_Release(view);
    IOleCache2_Release(cache);
done:
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
