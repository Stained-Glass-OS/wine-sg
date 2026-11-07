/* Network services Windows has running (patches/sg/6266): DNS Client (Dnscache), IP Helper (iphlpsvc), Network List
 * Service (netprofm), WinHTTP Web Proxy Auto-Discovery Service (WinHttpAutoProxySvc) exist with Windows' display
 * names and start types; a service depending on all four starts, and they run (Tailscale's installer checks them,
 * its service names them as dependencies). cfgmgr32's SwDeviceCreate and DevCreateObjectQuery exist and fail
 * cleanly (Wintun called them through delay loads that ended the process). */
#include <windows.h>
#include <stdio.h>

static SERVICE_STATUS_HANDLE status_handle;
static HANDLE stop;

static DWORD WINAPI handler(DWORD ctrl, DWORD type, void *data, void *ctx)
{
    SERVICE_STATUS st = { SERVICE_WIN32_OWN_PROCESS, SERVICE_STOPPED };
    if (ctrl == SERVICE_CONTROL_STOP) { SetServiceStatus(status_handle, &st); SetEvent(stop); }
    return NO_ERROR;
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    SERVICE_STATUS st = { SERVICE_WIN32_OWN_PROCESS, SERVICE_RUNNING, SERVICE_ACCEPT_STOP };
    stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    status_handle = RegisterServiceCtrlHandlerExW(L"SgNetDepTest", handler, NULL);
    SetServiceStatus(status_handle, &st);
    WaitForSingleObject(stop, 60000);
}

static DWORD state_of(SC_HANDLE scm, const WCHAR *name)
{
    SERVICE_STATUS st = { 0 };
    SC_HANDLE svc = OpenServiceW(scm, name, SERVICE_QUERY_STATUS);
    if (!svc) return 0;
    QueryServiceStatus(svc, &st);
    CloseServiceHandle(svc);
    return st.dwCurrentState;
}

int main(int argc, char **argv)
{
    static const struct { const WCHAR *name, *display; DWORD start; } wanted[] =
    {
        { L"Dnscache", L"DNS Client", SERVICE_AUTO_START },
        { L"iphlpsvc", L"IP Helper", SERVICE_AUTO_START },
        { L"netprofm", L"Network List Service", SERVICE_DEMAND_START },
        { L"WinHttpAutoProxySvc", L"WinHTTP Web Proxy Auto-Discovery Service", SERVICE_DEMAND_START },
    };
    SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)L"SgNetDepTest", service_main }, { NULL, NULL } };
    WCHAR exe[MAX_PATH], path[MAX_PATH + 8];
    SC_HANDLE scm, svc;
    HMODULE cfg;
    int i, running = 0;

    if (argc > 1 && !strcmp(argv[1], "svc")) return !StartServiceCtrlDispatcherW(table);
    setvbuf(stdout, NULL, _IONBF, 0);
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    for (i = 0; i < 4; i++)
    {
        BYTE buf[4096];
        QUERY_SERVICE_CONFIGW *cfgw = (void *)buf;
        DWORD size;
        svc = OpenServiceW(scm, wanted[i].name, SERVICE_QUERY_CONFIG);
        if (svc && QueryServiceConfigW(svc, cfgw, sizeof(buf), &size))
            printf("%ls=%d,%d,%d\n", wanted[i].name, !wcscmp(cfgw->lpDisplayName, wanted[i].display),
                   cfgw->dwStartType == wanted[i].start, cfgw->dwServiceType == SERVICE_WIN32_SHARE_PROCESS);
        else printf("%ls=missing %lu\n", wanted[i].name, GetLastError());
        if (svc) CloseServiceHandle(svc);
    }

    /* a service depending on all four: the service manager starts them first */
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    swprintf(path, ARRAYSIZE(path), L"\"%ls\" svc", exe);
    svc = CreateServiceW(scm, L"SgNetDepTest", L"SgNetDepTest", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path, NULL, NULL,
                         L"Dnscache\0iphlpsvc\0netprofm\0WinHttpAutoProxySvc\0\0", NULL, NULL);
    printf("created=%d\n", svc != NULL);
    if (svc)
    {
        SERVICE_STATUS st;
        printf("started=%d\n", StartServiceW(svc, 0, NULL));
        for (i = 0; i < 50 && state_of(scm, L"SgNetDepTest") != SERVICE_RUNNING; i++) Sleep(100);
        for (i = 0; i < 4; i++) running += state_of(scm, wanted[i].name) == SERVICE_RUNNING;
        printf("running=%d,%d\n", state_of(scm, L"SgNetDepTest") == SERVICE_RUNNING, running);
        ControlService(svc, SERVICE_CONTROL_STOP, &st);
        Sleep(500);
        DeleteService(svc);
        CloseServiceHandle(svc);
    }

    /* a dependency that cannot start: the service is not started (ERROR_SERVICE_DEPENDENCY_FAIL) */
    {
        SC_HANDLE off = CreateServiceW(scm, L"SgNetOff", L"SgNetOff", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                       SERVICE_DISABLED, SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
        SC_HANDLE dep = CreateServiceW(scm, L"SgNetDep2", L"SgNetDep2", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                       SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path, NULL, NULL, L"SgNetOff\0\0", NULL, NULL);
        BOOL ok = dep ? StartServiceW(dep, 0, NULL) : TRUE;
        DWORD err = GetLastError();
        printf("depfail=%d,%d,%lu\n", off != NULL && dep != NULL, ok, err);
        if (dep) { DeleteService(dep); CloseServiceHandle(dep); }
        if (off) { DeleteService(off); CloseServiceHandle(off); }
    }

    /* the device APIs Wintun delay-loads */
    cfg = LoadLibraryW(L"cfgmgr32.dll");
    {
        HRESULT (WINAPI *create)(const WCHAR *, const WCHAR *, const void *, ULONG, const void *, void *, void *, HANDLE *) =
            (void *)GetProcAddress(cfg, "SwDeviceCreate");
        HRESULT (WINAPI *query)(int, ULONG, ULONG, const void *, ULONG, const void *, void *, void *, HANDLE *) =
            (void *)GetProcAddress(cfg, "DevCreateObjectQuery");
        HANDLE h = (HANDLE)1, q = (HANDLE)1;
        HRESULT hc = create ? create(L"Wintun", L"HTREE\\ROOT\\0", NULL, 0, NULL, NULL, NULL, &h) : 0;
        HRESULT hq = query ? query(1, 0, 0, NULL, 0, NULL, NULL, NULL, &q) : 0;
        printf("swdevice=%d,%d,%08lx,%d\n", create != NULL, query != NULL, hc, FAILED(hq) && !h && !q);
    }
    return 0;
}
