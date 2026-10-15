/* DWM thumbnails (patches/sg/5500): registers a thumbnail of a source window
 * in a destination window and reads the destination's pixels back.
 *
 *   dwmthumb-probe.exe             run every check (prints "name=ok|FAIL ...")
 *   dwmthumb-probe.exe other HWND  try to register with another process's window
 *   dwmthumb-probe.exe srcwin EVENT FILE  a source window of this process
 */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>
#include <string.h>

#define RED RGB(255, 0, 0)
#define GREEN RGB(0, 255, 0)
#define BLUE RGB(0, 0, 255)

static COLORREF left_color = RED, right_color = RED;
static int failures;

static void check(const char *name, int ok, const char *fmt, ...)
{
    char text[256] = "";
    va_list args;

    if (fmt)
    {
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);
    }
    printf("%s=%s %s\n", name, ok ? "ok" : "FAIL", text);
    fflush(stdout);
    if (!ok) failures++;
}

static LRESULT CALLBACK dest_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND)
    {
        RECT rc;
        HBRUSH brush = CreateSolidBrush(BLUE);

        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, brush);
        DeleteObject(brush);
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK src_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND)
    {
        RECT rc, half;
        HBRUSH left = CreateSolidBrush(left_color), right = CreateSolidBrush(right_color);

        GetClientRect(hwnd, &rc);
        half = rc;
        half.right = rc.right / 2;
        FillRect((HDC)wp, &half, left);
        half.left = half.right;
        half.right = rc.right;
        FillRect((HDC)wp, &half, right);
        DeleteObject(left);
        DeleteObject(right);
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;

    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

static void register_classes(void)
{
    WNDCLASSW cls = {0};

    cls.hInstance = GetModuleHandleW(NULL);
    cls.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    cls.lpfnWndProc = dest_proc;
    cls.lpszClassName = L"SgThumbDest";
    RegisterClassW(&cls);
    cls.lpfnWndProc = src_proc;
    cls.lpszClassName = L"SgThumbSrc";
    RegisterClassW(&cls);
}

static COLORREF pixel(HWND hwnd, int x, int y)
{
    HDC dc = GetDC(hwnd);
    COLORREF c = GetPixel(dc, x, y);

    ReleaseDC(hwnd, dc);
    return c;
}

static int near_color(COLORREF c, int r, int g, int b, int tolerance)
{
    return abs((int)GetRValue(c) - r) <= tolerance && abs((int)GetGValue(c) - g) <= tolerance
            && abs((int)GetBValue(c) - b) <= tolerance;
}

static void update(HTHUMBNAIL thumbnail, DWORD flags, RECT dest, RECT source, BYTE opacity, BOOL visible)
{
    DWM_THUMBNAIL_PROPERTIES props = {0};
    HRESULT hr;

    props.dwFlags = flags;
    props.rcDestination = dest;
    props.rcSource = source;
    props.opacity = opacity;
    props.fVisible = visible;
    hr = DwmUpdateThumbnailProperties(thumbnail, &props);
    if (hr != S_OK) check("update", 0, "hr=%#lx", hr);
}

static int run_other(HWND dest)
{
    HTHUMBNAIL thumbnail = NULL;
    HWND src = CreateWindowW(L"static", L"x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    HRESULT hr = DwmRegisterThumbnail(dest, src, &thumbnail);

    printf("other=%#lx\n", hr);
    hr = DwmRegisterThumbnail(GetDesktopWindow(), src, &thumbnail);
    printf("desktop=%#lx\n", hr);
    if (hr == S_OK) DwmUnregisterThumbnail(thumbnail);
    return 0;
}

static int run_srcwin(const char *event_name, const char *file)
{
    HANDLE event = OpenEventA(SYNCHRONIZE, FALSE, event_name);
    HWND src;
    FILE *f;

    register_classes();
    left_color = right_color = RED;
    src = CreateWindowW(L"SgThumbSrc", L"remote source", WS_POPUP | WS_VISIBLE, 700, 20, 200, 150,
                        NULL, NULL, NULL, NULL);
    f = fopen(file, "w");
    fprintf(f, "%p\n", src);
    fclose(f);
    while (WaitForSingleObject(event, 20) == WAIT_TIMEOUT) pump(10);
    return 0;
}

static void spawn_wait(char *cmdline, char *output, size_t size)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HANDLE read, write;
    DWORD got = 0;

    output[0] = 0;
    CreatePipe(&read, &write, &sa, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write;
    si.hStdError = write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(read);
        CloseHandle(write);
        return;
    }
    CloseHandle(write);
    WaitForSingleObject(pi.hProcess, 60000);
    ReadFile(read, output, size - 1, &got, NULL);
    output[got] = 0;
    CloseHandle(read);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main(int argc, char **argv)
{
    HTHUMBNAIL thumbnail = NULL;
    RECT all = {0}, dest_rect, src_rect;
    char self[MAX_PATH], cmd[2 * MAX_PATH + 64], out[512];
    HWND dest, src;
    COLORREF c;
    HRESULT hr;

    if (argc > 2 && !strcmp(argv[1], "other")) return run_other((HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16));
    if (argc > 3 && !strcmp(argv[1], "srcwin")) return run_srcwin(argv[2], argv[3]);

    register_classes();
    dest = CreateWindowW(L"SgThumbDest", L"dest", WS_POPUP | WS_VISIBLE, 20, 20, 300, 300, NULL, NULL, NULL, NULL);
    src = CreateWindowW(L"SgThumbSrc", L"src", WS_POPUP | WS_VISIBLE, 400, 20, 200, 150, NULL, NULL, NULL, NULL);
    pump(300);
    c = pixel(dest, 100, 100);
    check("setup", near_color(c, 0, 0, 255, 4), "dest pixel %06lx", c);

    hr = DwmRegisterThumbnail(dest, src, &thumbnail);
    check("register", hr == S_OK && thumbnail, "hr=%#lx", hr);

    /* "will not be rendered to the destination window until [update] is called" */
    pump(400);
    c = pixel(dest, 100, 100);
    check("not-before-update", near_color(c, 0, 0, 255, 4), "pixel %06lx", c);

    SetRect(&dest_rect, 50, 50, 150, 150);
    update(thumbnail, DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE, dest_rect, all, 255, TRUE);
    pump(600);
    c = pixel(dest, 100, 100);
    check("visible-inside", near_color(c, 255, 0, 0, 6), "pixel %06lx", c);
    c = pixel(dest, 200, 200);
    check("outside-untouched", near_color(c, 0, 0, 255, 4), "pixel %06lx", c);
    c = pixel(dest, 30, 30);
    check("outside-untouched-2", near_color(c, 0, 0, 255, 4), "pixel %06lx", c);

    /* a source that changes: the right half of the picture turns green */
    right_color = GREEN;
    InvalidateRect(src, NULL, TRUE);
    pump(600);
    c = pixel(dest, 70, 100);
    check("live-left", near_color(c, 255, 0, 0, 8), "pixel %06lx", c);
    c = pixel(dest, 130, 100);
    check("live-right", near_color(c, 0, 255, 0, 8), "pixel %06lx", c);

    /* only the right half of the source shown: all green */
    SetRect(&src_rect, 100, 0, 200, 150);
    update(thumbnail, DWM_TNP_RECTSOURCE, all, src_rect, 255, TRUE);
    pump(600);
    c = pixel(dest, 70, 100);
    check("source-rect", near_color(c, 0, 255, 0, 8), "pixel %06lx", c);

    /* opacity: half of green over blue, and not more after many passes */
    update(thumbnail, DWM_TNP_OPACITY, all, all, 128, TRUE);
    pump(600);
    c = pixel(dest, 70, 100);
    check("opacity-half", near_color(c, 0, 128, 127, 8), "pixel %06lx", c);
    pump(800);
    c = pixel(dest, 70, 100);
    check("opacity-stable", near_color(c, 0, 128, 127, 8), "pixel %06lx", c);

    /* hidden: the destination shows only its own content again */
    update(thumbnail, DWM_TNP_VISIBLE, all, all, 255, FALSE);
    pump(600);
    c = pixel(dest, 70, 100);
    check("hidden", near_color(c, 0, 0, 255, 4), "pixel %06lx", c);

    /* shown again, then unregistered: the thumbnail goes */
    update(thumbnail, DWM_TNP_VISIBLE | DWM_TNP_OPACITY, all, all, 255, TRUE);
    pump(600);
    c = pixel(dest, 70, 100);
    check("shown-again", near_color(c, 0, 255, 0, 8), "pixel %06lx", c);
    hr = DwmUnregisterThumbnail(thumbnail);
    check("unregister", hr == S_OK, "hr=%#lx", hr);
    pump(600);
    c = pixel(dest, 70, 100);
    check("gone-after-unregister", near_color(c, 0, 0, 255, 4), "pixel %06lx", c);
    pump(400);
    c = pixel(dest, 70, 100);
    check("stays-gone", near_color(c, 0, 0, 255, 4), "pixel %06lx", c);

    /* a destination of another process is refused; the desktop is not */
    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(cmd, sizeof(cmd), "\"%s\" other %p", self, dest);
    spawn_wait(cmd, out, sizeof(out));
    check("other-process-dest", strstr(out, "other=0x80070005") != NULL, "%s", out[0] ? out : "(no output)");
    check("desktop-dest", strstr(out, "desktop=0") != NULL, "%s", out[0] ? out : "(no output)");

    /* a source window of another process */
    {
        char file[MAX_PATH], event_name[64];
        HANDLE event, proc_event;
        PROCESS_INFORMATION pi;
        STARTUPINFOA si = { sizeof(si) };
        HWND remote = NULL;
        int tries;
        FILE *f;

        GetTempPathA(sizeof(file), file);
        snprintf(file + strlen(file), 40, "sgthumb%lu.txt", GetCurrentProcessId());
        snprintf(event_name, sizeof(event_name), "sgthumb-%lu", GetCurrentProcessId());
        DeleteFileA(file);
        event = CreateEventA(NULL, TRUE, FALSE, event_name);
        proc_event = event;
        snprintf(cmd, sizeof(cmd), "\"%s\" srcwin %s \"%s\"", self, event_name, file);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            for (tries = 0; tries < 200 && !remote; tries++)
            {
                f = fopen(file, "r");
                if (f)
                {
                    void *ptr = NULL;

                    if (fscanf(f, "%p", &ptr) == 1) remote = ptr;
                    fclose(f);
                }
                Sleep(50);
            }
            hr = remote ? DwmRegisterThumbnail(dest, remote, &thumbnail) : E_FAIL;
            if (hr == S_OK)
            {
                SetRect(&dest_rect, 150, 150, 250, 250);
                update(thumbnail, DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE, dest_rect, all, 255, TRUE);
                pump(800);
                c = pixel(dest, 200, 200);
                check("remote-source", near_color(c, 255, 0, 0, 8), "pixel %06lx", c);
                DwmUnregisterThumbnail(thumbnail);
            }
            else check("remote-source", 0, "register hr=%#lx remote=%p", hr, remote);
            SetEvent(proc_event);
            WaitForSingleObject(pi.hProcess, 10000);
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else check("remote-source", 0, "no child process");
        CloseHandle(event);
        DeleteFileA(file);
    }

    printf("probe-failures=%d\n", failures);
    return failures != 0;
}
