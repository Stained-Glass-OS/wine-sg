/* staleclass-probe: a local server that ended without revoking its class (patches/sg/6225).
 *
 * rpcss kept a class object registered by a server that had ended (killed, crashed, an idle exit
 * without CoRevokeClassObject); activation was handed that object, failed with
 * RPC_S_SERVER_UNAVAILABLE (0x800706ba) and never started the server again: OneDrive could not
 * reach FileCoAuth's notification class after FileCoAuth had exited. Now the registration goes
 * with its process and the class's LocalServer32 starts again.
 *
 * Prints name=value lines; see test/staleclass-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <stdio.h>

DEFINE_GUID(CLSID_Probe, 0x5a6e1d2c, 0x3b4f, 0x4c21, 0x9e, 0x8a, 0x61, 0x7f, 0x12, 0x34, 0x56, 0x78);

/* the server's objects: one IUnknown, made by a class factory */
static LONG objects;
static HRESULT WINAPI unk_QI(IUnknown *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown)) { *out = iface; IUnknown_AddRef(iface); return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI unk_AddRef(IUnknown *iface) { return InterlockedIncrement(&objects); }
static ULONG WINAPI unk_Release(IUnknown *iface) { return InterlockedDecrement(&objects); }
static IUnknownVtbl unk_vtbl = { unk_QI, unk_AddRef, unk_Release };
static IUnknown unk = { &unk_vtbl };

static HRESULT WINAPI cf_QI(IClassFactory *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IClassFactory)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(&unk, iid, out);
}
static HRESULT WINAPI cf_LockServer(IClassFactory *iface, BOOL lock) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer };
static IClassFactory cf = { &cf_vtbl };

static int serve(DWORD ms, const WCHAR *ready)
{
    DWORD cookie;
    HANDLE event;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoRegisterClassObject(&CLSID_Probe, (IUnknown *)&cf, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie)))
        return 1;
    if (ready && (event = OpenEventW(EVENT_MODIFY_STATE, FALSE, ready))) SetEvent(event);
    Sleep(ms);
    CoRevokeClassObject(cookie);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR exe[MAX_PATH], key[128], cmd[MAX_PATH + 32];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    IUnknown *obj = NULL;
    HANDLE ready;
    HRESULT hr;
    DWORD start;

    if (argc > 1 && !wcsicmp(argv[1], L"-Embedding")) return serve(8000, NULL);
    if (argc > 1 && !wcscmp(argv[1], L"stale")) return serve(INFINITE, L"StaleClassReady");

    setvbuf(stdout, NULL, _IONBF, 0);
    GetModuleFileNameW(NULL, exe, ARRAYSIZE(exe));
    wcscpy(key, L"Software\\Classes\\CLSID\\{5a6e1d2c-3b4f-4c21-9e8a-617f12345678}\\LocalServer32");
    RegSetKeyValueW(HKEY_CURRENT_USER, key, NULL, REG_SZ, exe, (wcslen(exe) + 1) * sizeof(WCHAR));
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    /* a server registers the class, then is killed without revoking it */
    ready = CreateEventW(NULL, TRUE, FALSE, L"StaleClassReady");
    wcscpy(cmd, L"\"");
    wcscat(cmd, exe);
    wcscat(cmd, L"\" stale");
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("spawn=0\n"); return 1; }
    printf("registered=%d\n", WaitForSingleObject(ready, 30000) == WAIT_OBJECT_0);
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 10000);
    printf("killed=1\n");

    /* activation: the server started again, not the dead one's object */
    start = GetTickCount();
    hr = CoCreateInstance(&CLSID_Probe, NULL, CLSCTX_LOCAL_SERVER, &IID_IUnknown, (void **)&obj);
    printf("activate=%08lx fast=%d\n", hr, GetTickCount() - start < 20000);
    if (obj) IUnknown_Release(obj);
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5a6e1d2c-3b4f-4c21-9e8a-617f12345678}");
    printf("done=1\n");
    return 0;
}
