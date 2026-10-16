/* Probe for patches/sg/5034: the call_as proxies and stubs of ITypeLib::IsName
 * and FindName (the name comes back as the library spells it) and
 * ITypeInfo::CreateInstance (no aggregation across the call) forward to the
 * type library in another apartment; they returned E_FAIL. Uses stdole2.tlb.
 * (ITypeLib2 does not marshal in a Wine prefix - no proxy registration - so
 * GetLibStatistics is not reached here.)
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <olectl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static IStream *stream;
static HANDLE ready, quit;
static ITypeLib *local_tl;
static HRESULT load_hr, marshal_hr;

static DWORD WINAPI sta_thread(void *arg)
{
    MSG msg;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(load_hr = LoadTypeLib(L"stdole2.tlb", &local_tl))) local_tl = NULL;
    else marshal_hr = CoMarshalInterThreadInterfaceInStream(&IID_ITypeLib, (IUnknown *)local_tl, &stream);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &quit, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    if (local_tl) ITypeLib_Release(local_tl);
    CoUninitialize();
    return 0;
}

int main(void)
{
    static const struct { const WCHAR *ask, *want; BOOL found; } names[] =
    {
        {L"iunknown", L"IUnknown", TRUE}, {L"IDISPATCH", L"IDispatch", TRUE}, {L"sgnosuchname", L"sgnosuchname", FALSE},
    };
    ITypeLib *tl = NULL;
    HANDLE thread;
    unsigned int i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    quit = CreateEventA(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    if (stream) CoGetInterfaceAndReleaseStream(stream, &IID_ITypeLib, (void **)&tl);
    check(tl && (void *)tl != (void *)local_tl, "an ITypeLib proxy (%p, load %#lx, marshal %#lx)", tl, load_hr, marshal_hr);
    if (!tl) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        WCHAR buf[64];
        BOOL found = 2;
        ITypeInfo *ti[2] = {0};
        MEMBERID ids[2];
        USHORT count = 2;
        ULONG hash;

        lstrcpyW(buf, names[i].ask);
        hash = LHashValOfNameSys(sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32, LOCALE_NEUTRAL, buf);
        hr = ITypeLib_IsName(tl, buf, hash, &found);
        check(hr == S_OK && found == names[i].found && !lstrcmpW(buf, names[i].want), "IsName(%ls): %#lx %d \"%ls\"",
              names[i].ask, hr, found, buf);
        lstrcpyW(buf, names[i].ask);
        hr = ITypeLib_FindName(tl, buf, hash, ti, ids, &count);
        check(hr == S_OK && (names[i].found ? count >= 1 && ti[0] != NULL : count == 0) && !lstrcmpW(buf, names[i].want),
              "FindName(%ls): %#lx %u \"%ls\"", names[i].ask, hr, count, buf);
        if (ti[0]) ITypeInfo_Release(ti[0]);
        if (ti[1]) ITypeInfo_Release(ti[1]);
    }

    {
        ITypeInfo *ti = NULL;
        IFont *font = NULL;
        IUnknown *unk = (void *)0xdeadbeef;

        hr = ITypeLib_GetTypeInfoOfGuid(tl, &CLSID_StdFont, &ti);
        check(hr == S_OK && ti, "StdFont's type info: %#lx", hr);
        if (ti)
        {
            hr = ITypeInfo_CreateInstance(ti, NULL, &IID_IFont, (void **)&font);
            check(hr == S_OK && font, "CreateInstance of StdFont through the proxy: %#lx %p", hr, font);
            if (font) IFont_Release(font);
            hr = ITypeInfo_CreateInstance(ti, (IUnknown *)tl, &IID_IUnknown, (void **)&unk);
            check(hr == CLASS_E_NOAGGREGATION && !unk, "CreateInstance with an outer object: %#lx %p", hr, unk);
            ITypeInfo_Release(ti);
        }
    }
    ITypeLib_Release(tl);
    SetEvent(quit);
    WaitForSingleObject(thread, 10000);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
