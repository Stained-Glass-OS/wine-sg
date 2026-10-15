/* Probe for patches/sg/5037: IPropertyBag::Read through a proxy passes every
 * requested type - VT_EMPTY (the bag's own type), scalars, BSTR, arrays - not
 * only objects ("E_NOTIMPL is not a valid return code"); and
 * IQuickActivate::QuickActivate's call_as wrappers forward (the control sees
 * cbSize = sizeof(QACONTROL)). Objects in an STA, calls from the MTA.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HRESULT WINAPI pb_QueryInterface(IPropertyBag *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPropertyBag)) { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI pb_AddRef(IPropertyBag *iface) { return 2; }
static ULONG WINAPI pb_Release(IPropertyBag *iface) { return 1; }
static HRESULT WINAPI pb_Read(IPropertyBag *iface, LPCOLESTR name, VARIANT *var, IErrorLog *log)
{
    VARIANT v;
    VARTYPE want = V_VT(var);

    VariantInit(&v);
    if (!lstrcmpW(name, L"num")) { V_VT(&v) = VT_I4; V_I4(&v) = 42; }
    else if (!lstrcmpW(name, L"str")) { V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"hello"); }
    else if (!lstrcmpW(name, L"arr"))
    {
        SAFEARRAYBOUND b = {3, 0};
        LONG i, val;
        V_VT(&v) = VT_ARRAY | VT_I4;
        V_ARRAY(&v) = SafeArrayCreate(VT_I4, 1, &b);
        for (i = 0; i < 3; i++) { val = 10 * (i + 1); SafeArrayPutElement(V_ARRAY(&v), &i, &val); }
    }
    else return E_INVALIDARG;
    VariantClear(var);
    if (want == VT_EMPTY || want == V_VT(&v)) { *var = v; return S_OK; }
    return VariantChangeType(var, &v, 0, want) == S_OK ? (VariantClear(&v), S_OK) : (VariantClear(&v), E_FAIL);
}
static HRESULT WINAPI pb_Write(IPropertyBag *iface, LPCOLESTR name, VARIANT *var) { return E_NOTIMPL; }
static IPropertyBagVtbl pb_vtbl = {pb_QueryInterface, pb_AddRef, pb_Release, pb_Read, pb_Write};
static IPropertyBag bag = {&pb_vtbl};

static ULONG seen_size;
static HRESULT WINAPI qa_QueryInterface(IQuickActivate *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IQuickActivate)) { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI qa_AddRef(IQuickActivate *iface) { return 2; }
static ULONG WINAPI qa_Release(IQuickActivate *iface) { return 1; }
static HRESULT WINAPI qa_QuickActivate(IQuickActivate *iface, QACONTAINER *container, QACONTROL *control)
{
    seen_size = control->cbSize;
    if (container->cbSize != sizeof(*container)) return E_INVALIDARG;
    control->dwMiscStatus = 0x1234;
    control->dwViewStatus = container->dwAmbientFlags;
    return S_OK;
}
static HRESULT WINAPI qa_SetContentExtent(IQuickActivate *iface, SIZEL *size) { return S_OK; }
static HRESULT WINAPI qa_GetContentExtent(IQuickActivate *iface, SIZEL *size) { return E_NOTIMPL; }
static IQuickActivateVtbl qa_vtbl = {qa_QueryInterface, qa_AddRef, qa_Release, qa_QuickActivate, qa_SetContentExtent,
                                     qa_GetContentExtent};
static IQuickActivate qa = {&qa_vtbl};

static IStream *streams[2];
static HANDLE ready, quit;

static DWORD WINAPI sta_thread(void *arg)
{
    MSG msg;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    CoMarshalInterThreadInterfaceInStream(&IID_IPropertyBag, (IUnknown *)&bag, &streams[0]);
    CoMarshalInterThreadInterfaceInStream(&IID_IQuickActivate, (IUnknown *)&qa, &streams[1]);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &quit, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    CoUninitialize();
    return 0;
}

int main(void)
{
    static const struct { const WCHAR *name; VARTYPE ask, got; } reads[] =
    {
        {L"num", VT_EMPTY, VT_I4}, {L"num", VT_BSTR, VT_BSTR}, {L"num", VT_I2, VT_I2},
        {L"str", VT_EMPTY, VT_BSTR}, {L"arr", VT_EMPTY, VT_ARRAY | VT_I4}, {L"arr", VT_ARRAY | VT_I4, VT_ARRAY | VT_I4},
    };
    IPropertyBag *pb = NULL;
    IQuickActivate *qp = NULL;
    HANDLE thread;
    unsigned int i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    quit = CreateEventA(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    CoGetInterfaceAndReleaseStream(streams[0], &IID_IPropertyBag, (void **)&pb);
    CoGetInterfaceAndReleaseStream(streams[1], &IID_IQuickActivate, (void **)&qp);
    check(pb && qp && pb != &bag && qp != &qa, "proxies made (%p %p)", pb, qp);
    if (!pb || !qp) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(reads); i++)
    {
        VARIANT v;
        BOOL value_ok = FALSE;

        VariantInit(&v);
        V_VT(&v) = reads[i].ask;
        hr = IPropertyBag_Read(pb, reads[i].name, &v, NULL);
        if (hr == S_OK && V_VT(&v) == reads[i].got)
        {
            switch (V_VT(&v))
            {
            case VT_I4: value_ok = V_I4(&v) == 42; break;
            case VT_I2: value_ok = V_I2(&v) == 42; break;
            case VT_BSTR: value_ok = !lstrcmpW(V_BSTR(&v), !lstrcmpW(reads[i].name, L"num") ? L"42" : L"hello"); break;
            default:
            {
                LONG ix = 2, val = 0;
                value_ok = SafeArrayGetElement(V_ARRAY(&v), &ix, &val) == S_OK && val == 30;
            }
            }
        }
        check(value_ok, "Read(%ls) asking type %#x: %#lx, type %#x", reads[i].name, reads[i].ask, hr, V_VT(&v));
        VariantClear(&v);
    }

    {
        QACONTAINER container = {sizeof(container)};
        QACONTROL control = {sizeof(control)};

        container.dwAmbientFlags = QACONTAINER_USERMODE;
        hr = IQuickActivate_QuickActivate(qp, &container, &control);
        check(hr == S_OK && seen_size == sizeof(QACONTROL) && control.dwMiscStatus == 0x1234
              && control.dwViewStatus == QACONTAINER_USERMODE, "QuickActivate: %#lx size %lu status %#lx", hr, seen_size,
              control.dwMiscStatus);
    }
    IPropertyBag_Release(pb);
    IQuickActivate_Release(qp);
    SetEvent(quit);
    WaitForSingleObject(thread, 10000);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
