/* comsvc-gate.sh's probe (patches/sg/1302): one program, three parts.
 *   comsvc.exe sleep          keeps the machine's services.exe up (the owner's)
 *   comsvc.exe svc            the service: registers CLSID_SgComSvc's class
 *                             object (CLSCTX_LOCAL_SERVER) while it runs
 *   comsvc.exe setup          as the prefix owner (SYSTEM): the service
 *                             (default descriptor: users may not start it),
 *                             the class with AppID LocalService = SgComSvc,
 *                             and two classes no server can serve
 *   comsvc.exe get {CLSID}    CoGetClassObject(CLSCTX_LOCAL_SERVER): prints
 *                             "RESULT hr" and "MS milliseconds"
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static const CLSID CLSID_SgComSvc = {0x6a1b2c3d,0x0001,0x4e5f,{0x9a,0x9a,0x5a,0xb0,0xc0,0x55,0x01,0x01}};

/* a class factory that is its own object */
static HRESULT WINAPI cf_qi(IClassFactory *cf, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IClassFactory)) { *out = cf; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref(IClassFactory *cf) { return 2; }
static ULONG WINAPI cf_release(IClassFactory *cf) { return 1; }
static HRESULT WINAPI cf_create(IClassFactory *cf, IUnknown *outer, REFIID iid, void **out) { return cf_qi(cf, iid, out); }
static HRESULT WINAPI cf_lock(IClassFactory *cf, BOOL lock) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static IClassFactory factory = { &cf_vtbl };

static SERVICE_STATUS_HANDLE handle;
static SERVICE_STATUS status;
static HANDLE stop_event;

static void set_state(DWORD state)
{
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    SetServiceStatus(handle, &status);
}

static DWORD WINAPI control(DWORD code, DWORD type, void *data, void *ctx)
{
    if (code == SERVICE_CONTROL_STOP || code == SERVICE_CONTROL_SHUTDOWN)
    {
        set_state(SERVICE_STOP_PENDING);
        SetEvent(stop_event);
        return NO_ERROR;
    }
    return code == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    DWORD cookie;
    stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!(handle = RegisterServiceCtrlHandlerExW(L"SgComSvc", control, NULL))) return;
    set_state(SERVICE_START_PENDING);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoRegisterClassObject(&CLSID_SgComSvc, (IUnknown *)&factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie);
    set_state(SERVICE_RUNNING);
    WaitForSingleObject(stop_event, INFINITE);
    CoRevokeClassObject(cookie);
    CoUninitialize();
    set_state(SERVICE_STOPPED);
}

static void set(const WCHAR *key, const WCHAR *name, const WCHAR *value)
{
    HKEY k;
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW(value) + 1) * sizeof(WCHAR));
    RegCloseKey(k);
}

int wmain(int argc, WCHAR **argv)
{
    if (argc >= 2 && !lstrcmpW(argv[1], L"sleep"))
    {
        Sleep(INFINITE);
        return 0;
    }
    if (argc >= 2 && !lstrcmpW(argv[1], L"svc"))
    {
        SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)L"SgComSvc", service_main }, { NULL, NULL } };
        return StartServiceCtrlDispatcherW(table) ? 0 : 1;
    }
    if (argc >= 2 && !lstrcmpW(argv[1], L"setup"))
    {
        SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS), svc;
        svc = CreateServiceW(scm, L"SgComSvc", L"SG COM service test", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                             SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, L"C:\\comsvc.exe svc", NULL, NULL, NULL, NULL, NULL);
        printf("RESULT setup %lu\n", svc ? 0 : GetLastError());
        if (svc) CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        /* served by the service */
        set(L"Software\\Classes\\CLSID\\{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550101}", L"AppID", L"{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550101}");
        set(L"Software\\Classes\\AppID\\{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550101}", L"LocalService", L"SgComSvc");
        /* an AppID without LocalService or DllSurrogate, and no server at all */
        set(L"Software\\Classes\\CLSID\\{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550102}", L"AppID", L"{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550102}");
        set(L"Software\\Classes\\AppID\\{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550102}", NULL, L"no server");
        /* a surrogate class (DllSurrogate "": dllhost) whose DLL is not there */
        set(L"Software\\Classes\\CLSID\\{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550103}", L"AppID", L"{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550103}");
        set(L"Software\\Classes\\CLSID\\{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550103}\\InprocServer32", NULL, L"C:\\nothere\\sgnone.dll");
        set(L"Software\\Classes\\AppID\\{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550103}", L"DllSurrogate", L"");
        return 0;
    }
    if (argc >= 3 && !lstrcmpW(argv[1], L"get"))
    {
        CLSID clsid;
        IClassFactory *cf = NULL;
        DWORD t0;
        HRESULT hr;
        CLSIDFromString(argv[2], &clsid);
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        t0 = GetTickCount();
        hr = CoGetClassObject(&clsid, CLSCTX_LOCAL_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
        if (SUCCEEDED(hr))
        {
            IUnknown *unk = NULL;
            hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
            if (SUCCEEDED(hr)) IUnknown_Release(unk);
            IClassFactory_Release(cf);
        }
        printf("RESULT get %08lx\nMS %lu\n", hr, GetTickCount() - t0);
        CoUninitialize();
        return 0;
    }
    return 2;
}
