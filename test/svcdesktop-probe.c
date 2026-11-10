/* svcdesktop gate probe (wine-sg 7724).
 *   probe.exe service   -- run by services.exe: ServiceMain creates a window
 *   probe.exe check     -- run on WinSta0 after the service has started
 * A service's ServiceMain thread must be on the service's own desktop, not on
 * the user's: it used to come up on WinSta0\Default and force a desktop window
 * there that had no display, and the user's programs then failed with "The
 * explorer process failed to start". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS status;
static HDESK main_desktop;

static void write_log(const char *line)
{
    FILE *f = fopen("C:\\svcdesktop.log", "a");
    if (f) { fputs(line, f); fclose(f); }
}

static DWORD WINAPI ctrl(DWORD code, DWORD type, void *data, void *ctx)
{
    if (code == SERVICE_CONTROL_STOP)
    {
        status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(status_handle, &status);
    }
    return NO_ERROR;
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    char line[256];
    WCHAR ws[64] = L"?";
    DWORD n;
    HWND desk, msg;
    HDESK sm_desktop;

    status_handle = RegisterServiceCtrlHandlerExW(L"sgsvcdesk", ctrl, NULL);
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_RUNNING;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    SetServiceStatus(status_handle, &status);

    GetUserObjectInformationW(GetProcessWindowStation(), UOI_NAME, ws, sizeof(ws), &n);
    sm_desktop = GetThreadDesktop(GetCurrentThreadId());
    desk = GetDesktopWindow();
    msg = CreateWindowExW(0, L"Static", L"svc", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    snprintf(line, sizeof(line), "service %d-bit station=%ls main=%p servicemain=%p deskwnd=%p msgwnd=%d\n",
             (int)sizeof(void *) * 8, ws, main_desktop, sm_desktop, desk, msg != NULL);
    write_log(line);
    while (status.dwCurrentState == SERVICE_RUNNING) Sleep(200);
}

static int check(void)
{
    char buf[4096] = "", *p;
    void *svc_main = NULL, *svc_thread = NULL, *svc_wnd = NULL;
    int failures = 0, have = 0;
    FILE *f;
    HWND desk, w;
    WCHAR ws[64] = L"?";
    DWORD n;

    for (int i = 0; i < 100 && !have; i++)
    {
        if ((f = fopen("C:\\svcdesktop.log", "r")))
        {
            size_t len = fread(buf, 1, sizeof(buf) - 1, f);
            buf[len] = 0;
            fclose(f);
            have = strstr(buf, "service ") != NULL;
        }
        if (!have) Sleep(200);
    }
    if (!have) { printf("FAIL the service never ran\nRESULT: FAIL\n"); return 1; }
    printf("info %s", buf);
    p = strstr(buf, "main=");
    sscanf(p, "main=%p servicemain=%p deskwnd=%p", &svc_main, &svc_thread, &svc_wnd);

    if (!strstr(buf, "station=__wineservice_winstation"))
    { printf("FAIL the service is not on the service station\n"); failures++; }
    else printf("ok   the service is on the service station\n");

    if (svc_main != svc_thread)
    { printf("FAIL ServiceMain's desktop %p is not the one its process was started on, %p\n", svc_thread, svc_main); failures++; }
    else printf("ok   ServiceMain is on the service's desktop\n");

    GetUserObjectInformationW(GetProcessWindowStation(), UOI_NAME, ws, sizeof(ws), &n);
    desk = GetDesktopWindow();
    if (desk == (HWND)svc_wnd)
    { printf("FAIL %ls shares the service's desktop window %p\n", ws, desk); failures++; }
    else printf("ok   %ls has a desktop window of its own\n", ws);

    if (!GetPropW(desk, L"__wine_display_device_guid"))
    { printf("FAIL the user's desktop window has no display device (no explorer)\n"); failures++; }
    else printf("ok   the user's desktop has a display device\n");

    w = CreateWindowExW(0, L"Static", L"probe", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, 0, 0, 0, 0);
    if (!w) { printf("FAIL a window cannot be created on the user's desktop (%lu)\n", GetLastError()); failures++; }
    else { printf("ok   a window can be created on the user's desktop\n"); DestroyWindow(w); }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

int main(int argc, char **argv)
{
    SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)L"sgsvcdesk", service_main }, { 0 } };

    if (argc > 1 && !strcmp(argv[1], "check")) return check();
    main_desktop = GetThreadDesktop(GetCurrentThreadId());  /* no window: that would mend it */
    StartServiceCtrlDispatcherW(table);
    return 0;
}
