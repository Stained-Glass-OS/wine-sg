/* dwmapi: thumbnails (register, properties, source size, unregister) and the deprecated entry points */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>
#include <tlhelp32.h>
#ifndef DWM_E_COMPOSITIONDISABLED
#define DWM_E_COMPOSITIONDISABLED ((HRESULT)0x80263001)
#endif
#include <string.h>

static int failures;
#ifndef IDLE_MS
#define IDLE_MS 6000
#endif
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static COLORREF src_left = RGB(255, 0, 0), src_right = RGB(0, 0, 255);

static LRESULT CALLBACK src_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT)
    {
        PAINTSTRUCT ps;
        HDC dc = msg == WM_PAINT ? BeginPaint(hwnd, &ps) : (HDC)wp;
        RECT r;
        HBRUSH b1 = CreateSolidBrush(src_left), b2 = CreateSolidBrush(src_right);
        GetClientRect(hwnd, &r);
        r.right /= 2;
        FillRect(dc, &r, b1);
        GetClientRect(hwnd, &r);
        r.left = r.right / 2;
        FillRect(dc, &r, b2);
        DeleteObject(b1);
        DeleteObject(b2);
        if (msg == WM_PAINT) EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int dest_paints;
static LRESULT CALLBACK dest_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        dest_paints++;
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        FillRect(dc, &r, GetStockObject(WHITE_BRUSH));
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        Sleep(10);
    }
}

static COLORREF pixel(HWND hwnd, int x, int y)
{
    HDC dc = GetDC(hwnd);
    COLORREF c = GetPixel(dc, x, y);
    ReleaseDC(hwnd, dc);
    return c;
}

static int is_near(COLORREF c, int r, int g, int b, int tol)
{
    return abs((int)GetRValue(c) - r) <= tol && abs((int)GetGValue(c) - g) <= tol && abs((int)GetBValue(c) - b) <= tol;
}

static int thread_count(void)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = { sizeof(te) };
    int n = 0;
    if (snap == INVALID_HANDLE_VALUE) return -1;
    if (Thread32First(snap, &te))
        do { if (te.th32OwnerProcessID == GetCurrentProcessId()) n++; } while (Thread32Next(snap, &te));
    CloseHandle(snap);
    return n;
}

static ULONGLONG cpu_ms(void)
{
    FILETIME c, e, k, u;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    return (((ULONGLONG)k.dwHighDateTime << 32 | k.dwLowDateTime) + ((ULONGLONG)u.dwHighDateTime << 32 | u.dwLowDateTime)) / 10000;
}

static void idle_wait(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
}

static int child_main(void)
{
    WNDCLASSA wc = { 0 };
    HWND w;
    MSG msg;
    wc.lpfnWndProc = dest_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "SgThumbChild";
    RegisterClassA(&wc);
    w = CreateWindowA("SgThumbChild", "child", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    printf("%p\n", w);
    fflush(stdout);
    SetEvent(OpenEventA(EVENT_MODIFY_STATE, FALSE, "SgThumbChildReady"));
    {
        DWORD end = GetTickCount() + 20000;
        while (GetTickCount() < end && !GetPropA(w, "quit"))
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
            Sleep(20);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    WNDCLASSA wc = { 0 };
    HWND src, dest, child_win;
    HTHUMBNAIL t, t2;
    DWM_THUMBNAIL_PROPERTIES props;
    HRESULT hr;
    SIZE size;
    RECT r;
    UUID uuid;
    MilMatrix3x2D m;
    HANDLE ev;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 16];

    if (argc > 1) return child_main();

    wc.lpfnWndProc = src_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "SgThumbSrc";
    RegisterClassA(&wc);
    wc.lpfnWndProc = dest_proc;
    wc.lpszClassName = "SgThumbDest";
    RegisterClassA(&wc);
    src = CreateWindowA("SgThumbSrc", "src", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 300, 20, 200, 120, NULL, NULL, wc.hInstance, NULL);
    dest = CreateWindowA("SgThumbDest", "dest", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 20, 200, 300, 250, NULL, NULL, wc.hInstance, NULL);
    pump(300);

    /* the deprecated entry points */
    hr = DwmAttachMilContent(dest);
    CHECK(hr == DWM_E_COMPOSITIONDISABLED, "attach mil %#lx", hr);
    hr = DwmDetachMilContent(dest);
    CHECK(hr == DWM_E_COMPOSITIONDISABLED, "detach mil %#lx", hr);
    hr = DwmGetGraphicsStreamClient(0, &uuid);
    CHECK(hr == DWM_E_COMPOSITIONDISABLED, "stream client %#lx", hr);
    hr = DwmGetGraphicsStreamTransformHint(0, &m);
    CHECK(hr == DWM_E_COMPOSITIONDISABLED, "transform hint %#lx", hr);
    hr = DwmSetPresentParameters(dest, NULL);
    CHECK(hr == E_NOTIMPL, "present parameters %#lx", hr);

    /* registration */
    hr = DwmRegisterThumbnail(dest, src, NULL);
    CHECK(hr == E_INVALIDARG, "NULL out %#lx", hr);
    hr = DwmRegisterThumbnail(dest, (HWND)0xdead, &t);
    CHECK(hr == E_INVALIDARG, "bad source %#lx", hr);
    hr = DwmRegisterThumbnail((HWND)0xdead, src, &t);
    CHECK(hr == E_INVALIDARG, "bad destination %#lx", hr);
    {
        HWND kid = CreateWindowA("SgThumbDest", "kid", WS_CHILD | WS_VISIBLE, 0, 0, 20, 20, dest, NULL, wc.hInstance, NULL);
        hr = DwmRegisterThumbnail(dest, kid, &t);
        CHECK(hr == E_INVALIDARG, "child source %#lx", hr);
        hr = DwmRegisterThumbnail(kid, src, &t);
        CHECK(hr == E_INVALIDARG, "child destination %#lx", hr);
        DestroyWindow(kid);
    }
    hr = DwmRegisterThumbnail(GetDesktopWindow(), src, &t2);
    CHECK(hr == S_OK && t2, "desktop as destination %#lx", hr);
    if (hr == S_OK) CHECK(DwmUnregisterThumbnail(t2) == S_OK, "unregister desktop one");

    /* a window of another process is not ours to put thumbnails in */
    ev = CreateEventA(NULL, FALSE, FALSE, "SgThumbChildReady");
    sprintf(cmd, "\"%s\" child", argv[0]);
    {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        sprintf(cmd, "\"%s\" child", path);
    }
    si.dwFlags = STARTF_USESTDHANDLES;
    {
        HANDLE rd, wr;
        char line[64] = "";
        DWORD n = 0;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        CreatePipe(&rd, &wr, &sa, 0);
        si.hStdOutput = wr;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(ev, 20000);
            CloseHandle(wr);
            ReadFile(rd, line, sizeof(line) - 1, &n, NULL);
            child_win = (HWND)(ULONG_PTR)strtoull(line, NULL, 16);
            hr = DwmRegisterThumbnail(child_win, src, &t2);
            CHECK(hr == E_ACCESSDENIED, "destination of another process %#lx", hr);
            if (hr == S_OK) DwmUnregisterThumbnail(t2);
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else CHECK(0, "no child process");
        CloseHandle(rd);
    }

    hr = DwmRegisterThumbnail(dest, src, &t);
    CHECK(hr == S_OK && t, "register %#lx", hr);
    if (hr != S_OK) return 1;
    CHECK(DwmRegisterThumbnail(dest, src, &t2) == S_OK && t2 != t, "second registration gets its own handle");
    CHECK(DwmUnregisterThumbnail(t2) == S_OK, "unregister second");
    CHECK(DwmUnregisterThumbnail(t2) == E_INVALIDARG, "unregister twice");
    CHECK(DwmUnregisterThumbnail(NULL) == E_INVALIDARG, "unregister NULL");

    /* nothing is drawn before DwmUpdateThumbnailProperties */
    pump(300);
    CHECK(is_near(pixel(dest, 50, 50), 255, 255, 255, 2), "drawn before any properties: %06lx", pixel(dest, 50, 50));

    GetWindowRect(src, &r);
    hr = DwmQueryThumbnailSourceSize(t, &size);
    CHECK(hr == S_OK && size.cx == r.right - r.left && size.cy == r.bottom - r.top, "source size %#lx %ldx%ld (window %ldx%ld)", hr, size.cx, size.cy,
          r.right - r.left, r.bottom - r.top);
    CHECK(DwmQueryThumbnailSourceSize(NULL, &size) == E_INVALIDARG, "size of NULL");
    CHECK(DwmQueryThumbnailSourceSize(t, NULL) == E_INVALIDARG, "size to NULL");

    memset(&props, 0, sizeof(props));
    props.dwFlags = 0x20;
    CHECK(DwmUpdateThumbnailProperties(t, &props) == E_INVALIDARG, "unknown flag");
    CHECK(DwmUpdateThumbnailProperties(t, NULL) == E_INVALIDARG, "NULL properties");
    props.dwFlags = DWM_TNP_VISIBLE;
    props.fVisible = TRUE;
    CHECK(DwmUpdateThumbnailProperties(NULL, &props) == E_INVALIDARG, "properties of NULL");
    CHECK(DwmUpdateThumbnailProperties((HTHUMBNAIL)0x77, &props) == E_INVALIDARG, "properties of an unknown handle");

    /* visible, at 10,10-110,110, the whole source: red on the left half, blue on the right */
    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
    SetRect(&props.rcDestination, 10, 10, 210, 130);
    props.opacity = 255;
    props.fVisible = TRUE;
    hr = DwmUpdateThumbnailProperties(t, &props);
    CHECK(hr == S_OK, "update %#lx", hr);
    pump(600);
    CHECK(is_near(pixel(dest, 40, 70), 255, 0, 0, 12), "left half of the thumbnail %06lx", pixel(dest, 40, 70));
    CHECK(is_near(pixel(dest, 180, 70), 0, 0, 255, 12), "right half of the thumbnail %06lx", pixel(dest, 180, 70));
    CHECK(is_near(pixel(dest, 240, 200), 255, 255, 255, 2), "outside the rectangle %06lx", pixel(dest, 240, 200));

    /* the source changes: the thumbnail follows */
    src_left = RGB(0, 255, 0);
    InvalidateRect(src, NULL, TRUE);
    pump(1700);
    CHECK(is_near(pixel(dest, 40, 70), 0, 255, 0, 12), "thumbnail follows the source %06lx", pixel(dest, 40, 70));

    /* source rectangle: the right half only */
    props.dwFlags = DWM_TNP_RECTSOURCE;
    GetClientRect(src, &r);
    props.rcSource.left = r.right / 2 + 5;
    props.rcSource.right = r.right;
    props.rcSource.top = 0;
    props.rcSource.bottom = r.bottom;
    CHECK(DwmUpdateThumbnailProperties(t, &props) == S_OK, "source rect");
    pump(600);
    CHECK(is_near(pixel(dest, 40, 70), 0, 0, 255, 12), "source rectangle %06lx", pixel(dest, 40, 70));

    /* opacity */
    props.dwFlags = DWM_TNP_RECTSOURCE;
    props.rcSource.left = 0;
    props.rcSource.right = r.right / 2 - 5;
    DwmUpdateThumbnailProperties(t, &props);
    props.dwFlags = DWM_TNP_OPACITY;
    props.opacity = 128;
    CHECK(DwmUpdateThumbnailProperties(t, &props) == S_OK, "opacity");
    InvalidateRect(dest, NULL, TRUE);
    pump(700);
    {
        COLORREF c = pixel(dest, 40, 70);
        /* green at half opacity over white */
        CHECK(GetRValue(c) > 100 && GetRValue(c) < 160 && GetGValue(c) > 230 && GetBValue(c) > 100 && GetBValue(c) < 160, "half opacity %06lx", c);
    }

    /* a translucent picture that changes is blended over the background again, not over itself */
    src_left = RGB(255, 0, 0);
    InvalidateRect(src, NULL, TRUE);
    pump(1700);
    {
        COLORREF c = pixel(dest, 40, 70);
        CHECK(GetRValue(c) > 230 && GetGValue(c) > 100 && GetGValue(c) < 160 && GetBValue(c) > 100 && GetBValue(c) < 160, "half opacity, changed picture %06lx", c);
    }

    /* hiding gives the area back to the destination */
    props.dwFlags = DWM_TNP_VISIBLE;
    props.fVisible = FALSE;
    CHECK(DwmUpdateThumbnailProperties(t, &props) == S_OK, "hide");
    pump(500);
    CHECK(is_near(pixel(dest, 40, 70), 255, 255, 255, 2), "area after hiding %06lx", pixel(dest, 40, 70));
    props.fVisible = TRUE;
    props.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
    props.opacity = 255;
    DwmUpdateThumbnailProperties(t, &props);
    pump(400);
    CHECK(is_near(pixel(dest, 40, 70), 255, 0, 0, 12), "shown again %06lx", pixel(dest, 40, 70));

    /* the client area only changes the source size */
    props.dwFlags = DWM_TNP_SOURCECLIENTAREAONLY;
    props.fSourceClientAreaOnly = TRUE;
    CHECK(DwmUpdateThumbnailProperties(t, &props) == S_OK, "client area only");
    GetClientRect(src, &r);
    hr = DwmQueryThumbnailSourceSize(t, &size);
    CHECK(hr == S_OK && size.cx == r.right && size.cy == r.bottom, "client source size %ldx%ld vs %ldx%ld", size.cx, size.cy, r.right, r.bottom);

    /* unregistering gives it back too */
    pump(1500);
    CHECK(!is_near(pixel(dest, 40, 70), 255, 255, 255, 30), "thumbnail on screen before unregistering: %06lx", pixel(dest, 40, 70));
    CHECK(DwmUnregisterThumbnail(t) == S_OK, "unregister");
    pump(500);
    CHECK(is_near(pixel(dest, 40, 70), 255, 255, 255, 2), "area after unregistering %06lx", pixel(dest, 40, 70));
    CHECK(DwmUpdateThumbnailProperties(t, &props) == E_INVALIDARG, "properties after unregistering");
    CHECK(DwmQueryThumbnailSourceSize(t, &size) == E_INVALIDARG, "size after unregistering");

    /* cost: thumbnails kept registered and visible while nothing changes */
    {
        HTHUMBNAIL th[3];
        HWND dest2 = CreateWindowA("SgThumbDest", "dest2", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 400, 200, 300, 250, NULL, NULL, wc.hInstance, NULL);
        int base, with, i, paints0;
        ULONGLONG cpu0, used;

        idle_wait(500);
        base = thread_count();
        memset(&props, 0, sizeof(props));
        props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
        props.fVisible = TRUE;
        props.opacity = 255;
        for (i = 0; i < 3; i++)
        {
            SetRect(&props.rcDestination, 5 + i * 95, 10, 5 + i * 95 + 90, 80);
            CHECK(DwmRegisterThumbnail(dest2, src, &th[i]) == S_OK, "register %d", i);
            CHECK(DwmUpdateThumbnailProperties(th[i], &props) == S_OK, "show %d", i);
        }
        idle_wait(2000);
        with = thread_count();
        CHECK(base >= 0 && with == base + 1, "one painting thread for any number of thumbnails: %d -> %d", base, with);
        paints0 = dest_paints;
        cpu0 = cpu_ms();
        idle_wait(IDLE_MS);
        used = cpu_ms() - cpu0;
        printf("note: %d ms idle with 3 visible thumbnails: %I64u ms of CPU, %d destination repaints\n", IDLE_MS, used, dest_paints - paints0);
        CHECK(used < IDLE_MS / 20, "3 idle thumbnails used %I64u ms of CPU in %d ms", used, IDLE_MS);
        CHECK(dest_paints - paints0 <= 2, "the destination was repainted %d times while nothing changed", dest_paints - paints0);
        for (i = 0; i < 3; i++) CHECK(DwmUnregisterThumbnail(th[i]) == S_OK, "unregister %d", i);
        idle_wait(1500);
        CHECK(thread_count() == base, "the painting thread ends with the last thumbnail: %d vs %d", thread_count(), base);

        /* and with the destination window */
        CHECK(DwmRegisterThumbnail(dest2, src, &th[0]) == S_OK, "register again");
        SetRect(&props.rcDestination, 10, 10, 110, 90);
        DwmUpdateThumbnailProperties(th[0], &props);
        idle_wait(500);
        CHECK(thread_count() == base + 1, "thread running again: %d", thread_count());
        DestroyWindow(dest2);
        idle_wait(2500);
        CHECK(thread_count() == base, "the thread ends with the destination window: %d vs %d", thread_count(), base);
        CHECK(DwmUpdateThumbnailProperties(th[0], &props) == E_INVALIDARG, "thumbnail of a destroyed destination");
    }

    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
