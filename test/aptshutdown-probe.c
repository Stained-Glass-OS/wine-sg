/* Apartment identifiers and shutdown callbacks (patches/sg/1696), run by
 * test/aptshutdown-gate.sh: RoGetApartmentIdentifier gives each apartment
 * its own identifier (it was 0xdeadbeef for all), RoRegisterForApartmentShutdown
 * calls back when the apartment goes and RoUnregisterForApartmentShutdown
 * (an "@ stub") takes that back; IGlobalOptions::Set keeps what it is given. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <objidl.h>
#include <stdio.h>

typedef void *APARTMENT_SHUTDOWN_REGISTRATION_COOKIE_T;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HRESULT (WINAPI *pRoGetApartmentIdentifier)(UINT64 *);
static HRESULT (WINAPI *pRoRegisterForApartmentShutdown)(IUnknown *, UINT64 *, void **);
static HRESULT (WINAPI *pRoUnregisterForApartmentShutdown)(void *);

static const IID iid_shutdown = { 0xa2f05a09, 0x27a2, 0x42b0, { 0x8a, 0xb5, 0xe3, 0x1c, 0x43, 0x3d, 0x96, 0x17 } };
static volatile LONG calls;
static volatile UINT64 told;

static HRESULT WINAPI cb_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &iid_shutdown)) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cb_AddRef(IUnknown *iface) { return 2; }
static ULONG WINAPI cb_Release(IUnknown *iface) { return 1; }
static void WINAPI cb_OnUninitialize(IUnknown *iface, UINT64 id)
{
    told = id;
    InterlockedIncrement(&calls);
}
static void *cb_vtbl[] = { cb_QueryInterface, cb_AddRef, cb_Release, cb_OnUninitialize };
static IUnknown callback = { (IUnknownVtbl *)cb_vtbl };

static UINT64 thread_id1, thread_reg_id;
static HRESULT thread_hr;
static int unregister_first;

static DWORD WINAPI sta_thread(void *arg)
{
    void *cookie = NULL;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    pRoGetApartmentIdentifier((UINT64 *)&thread_id1);
    thread_hr = pRoRegisterForApartmentShutdown(&callback, (UINT64 *)&thread_reg_id, &cookie);
    if (unregister_first) thread_hr = pRoUnregisterForApartmentShutdown(cookie);
    CoUninitialize();
    return 0;
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    UINT64 id_main = 0;
    HANDLE thread;
    IGlobalOptions *options;
    ULONG_PTR value = 0;
    HRESULT hr;

    pRoGetApartmentIdentifier = (void *)GetProcAddress(combase, "RoGetApartmentIdentifier");
    pRoRegisterForApartmentShutdown = (void *)GetProcAddress(combase, "RoRegisterForApartmentShutdown");
    pRoUnregisterForApartmentShutdown = (void *)GetProcAddress(combase, "RoUnregisterForApartmentShutdown");
    check(pRoUnregisterForApartmentShutdown != NULL, "RoUnregisterForApartmentShutdown is there");

    check(pRoGetApartmentIdentifier(&id_main) == CO_E_NOTINITIALIZED, "no apartment: CO_E_NOTINITIALIZED");
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    check(pRoGetApartmentIdentifier(&id_main) == S_OK && id_main && id_main != 0xdeadbeef,
          "RoGetApartmentIdentifier: the MTA's");

    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    check(thread_id1 && thread_id1 != id_main, "an STA has an identifier of its own");
    check(thread_hr == S_OK && thread_reg_id == thread_id1, "RoRegisterForApartmentShutdown gives it");
    check(calls == 1 && told == thread_id1, "CoUninitialize: OnUninitialize with the identifier");

    calls = 0;
    unregister_first = 1;
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    check(thread_hr == S_OK && calls == 0, "unregistered first: no call");

    hr = CoCreateInstance(&CLSID_GlobalOptions, NULL, CLSCTX_INPROC_SERVER, &IID_IGlobalOptions, (void **)&options);
    check(hr == S_OK, "IGlobalOptions");
    if (hr == S_OK)
    {
        check(IGlobalOptions_Set(options, COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE_ANY) == S_OK &&
              IGlobalOptions_Query(options, COMGLB_EXCEPTION_HANDLING, &value) == S_OK &&
              value == COMGLB_EXCEPTION_DONOT_HANDLE_ANY, "Set keeps COMGLB_EXCEPTION_DONOT_HANDLE_ANY (was dropped)");
        check(IGlobalOptions_Set(options, COMGLB_EXCEPTION_HANDLING, 7) == E_INVALIDARG, "an invalid value: E_INVALIDARG");
        IGlobalOptions_Release(options);
    }
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
