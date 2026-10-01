/* A COM proxy's security blanket (patches/sg/0627): an object in a
 * single-threaded apartment, called from the multithreaded one through a
 * proxy, whose blanket is set, read back and copied. Prints key=value lines
 * for test/proxyblanket-gate.sh:
 *   setdefault=<hr of CoSetProxyBlanket as Omaha's updaters call it>
 *   query=<hr> authn=<n> level=<n> imp=<n>   (after it)
 *   querynull=<hr of a query with no outputs>
 *   call=<hr of a call through the proxy afterwards (the object's E_NOINTERFACE)>
 *   calls=<calls the object had>
 *   local=<hr of setting it on the proxy's own IMarshal>
 *   invalid=<hr with an unknown authentication service>
 *   copy=<hr of CoCopyProxy> copied=<0|1>
 */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static LONG calls;

static HRESULT WINAPI cf_QueryInterface(IClassFactory *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out)
{
    InterlockedIncrement(&calls);
    *out = NULL;
    return E_NOINTERFACE;
}
static HRESULT WINAPI cf_LockServer(IClassFactory *iface, BOOL lock) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_QueryInterface, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer };
static IClassFactory cf = { &cf_vtbl };

static IStream *marshalled;
static HANDLE ready, quit;

static DWORD WINAPI host(void *arg)
{
    MSG msg;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    CoMarshalInterThreadInterfaceInStream(&IID_IClassFactory, (IUnknown *)&cf, &marshalled);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &quit, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    CoUninitialize();
    return 0;
}

int main(void)
{
    DWORD authn = 0, authz = 0, level = 0, imp = 0, caps = 0;
    IClientSecurity *sec = NULL;
    IClassFactory *proxy = NULL;
    IUnknown *copy = NULL;
    IMarshal *marshal = NULL;
    OLECHAR *princ = NULL;
    HANDLE thread;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    quit = CreateEventW(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, host, NULL, 0, NULL);
    WaitForSingleObject(ready, INFINITE);
    if (FAILED(hr = CoGetInterfaceAndReleaseStream(marshalled, &IID_IClassFactory, (void **)&proxy)))
    {
        printf("unmarshal=%08lx\n", hr);
        return 1;
    }

    /* what Omaha's updaters ask before calling their COM servers */
    hr = CoSetProxyBlanket((IUnknown *)proxy, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL,
                           RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_DYNAMIC_CLOAKING);
    printf("setdefault=%08lx\n", hr);
    hr = CoQueryProxyBlanket((IUnknown *)proxy, &authn, &authz, &princ, &level, &imp, NULL, &caps);
    printf("query=%08lx authn=%lu level=%lu imp=%lu\n", hr, authn, level, imp);
    CoTaskMemFree(princ);
    hr = CoQueryProxyBlanket((IUnknown *)proxy, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    printf("querynull=%08lx\n", hr);
    {
        void *unused;
        hr = IClassFactory_CreateInstance(proxy, NULL, &IID_IStream, &unused);
        printf("call=%08lx calls=%ld\n", hr, calls);
    }

    IClassFactory_QueryInterface(proxy, &IID_IClientSecurity, (void **)&sec);
    IClassFactory_QueryInterface(proxy, &IID_IMarshal, (void **)&marshal);
    hr = sec && marshal ? IClientSecurity_SetBlanket(sec, (IUnknown *)marshal, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                                                    RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_IMP_LEVEL_IDENTIFY, NULL, EOAC_NONE)
                        : E_FAIL;
    printf("local=%08lx\n", hr);
    hr = CoSetProxyBlanket((IUnknown *)proxy, 0xdeadbeef, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CONNECT,
                           RPC_C_IMP_LEVEL_IDENTIFY, NULL, EOAC_NONE);
    printf("invalid=%08lx\n", hr);
    hr = CoCopyProxy((IUnknown *)proxy, &copy);
    printf("copy=%08lx copied=%d\n", hr, copy != NULL);

    if (copy) IUnknown_Release(copy);
    if (marshal) IMarshal_Release(marshal);
    if (sec) IClientSecurity_Release(sec);
    IClassFactory_Release(proxy);
    SetEvent(quit);
    WaitForSingleObject(thread, 5000);
    CoUninitialize();
    return 0;
}
