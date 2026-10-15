/* Probe for patches/sg/5031: IDataObject::SetData through a proxy (call_as
 * RemoteSetData with a FLAG_STGMEDIUM) reaches the object with the format,
 * the medium's data and fRelease; a NULL medium arrives as NULL. It returned
 * E_NOTIMPL. The data object lives in an STA thread, the caller in the MTA.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static char got_text[64];
static CLIPFORMAT got_format;
static DWORD got_tymed;
static BOOL got_release, got_null;

static HRESULT WINAPI do_QueryInterface(IDataObject *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDataObject)) { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI do_AddRef(IDataObject *iface) { return 2; }
static ULONG WINAPI do_Release(IDataObject *iface) { return 1; }
static HRESULT WINAPI do_GetData(IDataObject *iface, FORMATETC *fmt, STGMEDIUM *med) { return E_NOTIMPL; }
static HRESULT WINAPI do_GetDataHere(IDataObject *iface, FORMATETC *fmt, STGMEDIUM *med) { return E_NOTIMPL; }
static HRESULT WINAPI do_QueryGetData(IDataObject *iface, FORMATETC *fmt) { return E_NOTIMPL; }
static HRESULT WINAPI do_GetCanonicalFormatEtc(IDataObject *iface, FORMATETC *in, FORMATETC *out) { return E_NOTIMPL; }
static HRESULT WINAPI do_SetData(IDataObject *iface, FORMATETC *fmt, STGMEDIUM *med, BOOL release)
{
    got_null = !med;
    got_release = release;
    got_format = fmt ? fmt->cfFormat : 0;
    got_text[0] = 0;
    if (!med) return E_INVALIDARG;
    got_tymed = med->tymed;
    if (med->tymed == TYMED_HGLOBAL)
    {
        char *p = GlobalLock(med->hGlobal);
        if (p) { lstrcpynA(got_text, p, sizeof(got_text)); GlobalUnlock(med->hGlobal); }
    }
    if (release) ReleaseStgMedium(med);
    return S_OK;
}
static HRESULT WINAPI do_EnumFormatEtc(IDataObject *iface, DWORD dir, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI do_DAdvise(IDataObject *iface, FORMATETC *fmt, DWORD advf, IAdviseSink *sink, DWORD *conn) { return E_NOTIMPL; }
static HRESULT WINAPI do_DUnadvise(IDataObject *iface, DWORD conn) { return E_NOTIMPL; }
static HRESULT WINAPI do_EnumDAdvise(IDataObject *iface, IEnumSTATDATA **e) { return E_NOTIMPL; }
static IDataObjectVtbl do_vtbl = {do_QueryInterface, do_AddRef, do_Release, do_GetData, do_GetDataHere, do_QueryGetData,
    do_GetCanonicalFormatEtc, do_SetData, do_EnumFormatEtc, do_DAdvise, do_DUnadvise, do_EnumDAdvise};
static IDataObject data_object = {&do_vtbl};

static IStream *stream;
static HANDLE ready, quit;

static DWORD WINAPI sta_thread(void *arg)
{
    MSG msg;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    CoMarshalInterThreadInterfaceInStream(&IID_IDataObject, (IUnknown *)&data_object, &stream);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &quit, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    CoUninitialize();
    return 0;
}

int main(void)
{
    static const struct { const char *text; BOOL release; } tests[] = {{"hello", FALSE}, {"owned", TRUE}};
    IDataObject *proxy = NULL;
    HANDLE thread;
    unsigned int i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    quit = CreateEventA(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    CoGetInterfaceAndReleaseStream(stream, &IID_IDataObject, (void **)&proxy);
    check(proxy && proxy != &data_object, "a proxy (%p)", proxy);
    if (!proxy) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        FORMATETC fmt = {CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM med = {TYMED_HGLOBAL};
        char *p;

        med.hGlobal = GlobalAlloc(GMEM_MOVEABLE, 16);
        p = GlobalLock(med.hGlobal); strcpy(p, tests[i].text); GlobalUnlock(med.hGlobal);
        got_text[0] = 0; got_release = 2;
        hr = IDataObject_SetData(proxy, &fmt, &med, tests[i].release);
        check(hr == S_OK && got_format == CF_TEXT && got_tymed == TYMED_HGLOBAL && !strcmp(got_text, tests[i].text)
              && got_release == tests[i].release, "SetData \"%s\", release %d: %#lx, got \"%s\" release %d",
              tests[i].text, tests[i].release, hr, got_text, got_release);
        if (!tests[i].release) ReleaseStgMedium(&med);
    }
    {
        FORMATETC fmt = {CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        got_null = FALSE;
        hr = IDataObject_SetData(proxy, &fmt, NULL, FALSE);
        check(hr == E_INVALIDARG && got_null, "a NULL medium reaches the object as NULL: %#lx %d", hr, got_null);
    }
    IDataObject_Release(proxy);
    SetEvent(quit);
    WaitForSingleObject(thread, 10000);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
