/* inputind-probe (patches/sg/9160, 9162, test/inputind-gate.sh): a window "sginputprobe" in the foreground
 * that logs, one line each, what the taskbar's input indicator shows and what the keyboard sends it:
 *   LIST n hkl...                     GetKeyboardLayoutList at start
 *   INDRECT l t r b                   the indicator (TrayInputIndicatorWClass) on the screen
 *   IND visible "text" hkl            the indicator, at start and whenever it changes (its SgInputLayout)
 *   FLY visible "title" l t r b       the list (SgInputSwitcher), whenever it comes or goes
 *   REQ flags hkl / CHANGE hkl        WM_INPUTLANGCHANGEREQUEST, WM_INPUTLANGCHANGE
 *   DOWN vk                           WM_KEYDOWN
 *   SHELL hkl                         HSHELL_LANGUAGE to the probe's own shell hook window (RegisterShellHookWindow)
 * On the character a: ACT hkl -- the probe (in the foreground) activates the first layout; on b a thread
 * of its own without windows (in the background) activates the second (BACK hkl); on q DONE.
 * Gives up after 60 s. */
#include <windows.h>
#include <stdio.h>

static unsigned long h(HKL hkl) { return (unsigned long)(ULONG_PTR)hkl; }
static HKL list[16];
static UINT count;
static UINT shellhook;

static DWORD WINAPI background(void *arg)
{
    ActivateKeyboardLayout(list[1], 0);
    printf("BACK %08lx\n", h(GetKeyboardLayout(0))); fflush(stdout);
    return 0;
}

static void watch(void)
{
    static char last_ind[128] = "", last_fly[160] = "";
    char buf[160];
    WCHAR text[64] = L"";
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    HWND ind = tray ? FindWindowExW(tray, NULL, L"TrayInputIndicatorWClass", NULL) : NULL;
    HWND fly = FindWindowW(L"SgInputSwitcher", NULL);
    RECT r = {0};

    if (ind) GetWindowTextW(ind, text, 64);
    snprintf(buf, sizeof(buf), "IND %d \"%ls\" %08lx", ind && IsWindowVisible(ind), text,
             ind ? h(GetPropW(ind, L"SgInputLayout")) : 0ul);
    if (strcmp(buf, last_ind)) { strcpy(last_ind, buf); printf("%s\n", buf); fflush(stdout); }
    text[0] = 0;
    if (fly) { GetWindowTextW(fly, text, 64); GetWindowRect(fly, &r); }
    snprintf(buf, sizeof(buf), "FLY %d \"%ls\" %ld %ld %ld %ld", fly && IsWindowVisible(fly), text,
             r.left, r.top, r.right, r.bottom);
    if (strcmp(buf, last_fly)) { strcpy(last_fly, buf); printf("%s\n", buf); fflush(stdout); }
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (shellhook && msg == shellhook)
    {
        if (wp == HSHELL_LANGUAGE) { printf("SHELL %08lx\n", (unsigned long)lp); fflush(stdout); }
        return 0;
    }
    switch (msg)
    {
    case WM_INPUTLANGCHANGEREQUEST:
        printf("REQ %lx %08lx\n", (unsigned long)wp, (unsigned long)lp); fflush(stdout);
        break;
    case WM_INPUTLANGCHANGE:
        printf("CHANGE %08lx\n", (unsigned long)lp); fflush(stdout);
        break;
    case WM_KEYDOWN:
        printf("DOWN %02lx\n", (unsigned long)wp); fflush(stdout);
        break;
    case WM_CHAR:
        if (wp == 'a')
        {
            ActivateKeyboardLayout(list[0], 0);
            printf("ACT %08lx\n", h(GetKeyboardLayout(0))); fflush(stdout);
        }
        else if (wp == 'b') CloseHandle(CreateThread(NULL, 0, background, NULL, 0, NULL));
        else if (wp == 'q') { printf("DONE\n"); fflush(stdout); PostQuitMessage(0); }
        return 0;
    case WM_TIMER:
        if (wp == 2) { watch(); return 0; }
        printf("TIMEOUT\n"); fflush(stdout);
        PostQuitMessage(1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd, tray, ind;
    RECT r;
    MSG msg;
    UINT i;
    int tries;

    count = GetKeyboardLayoutList(16, list);
    printf("LIST %u", count);
    for (i = 0; i < count; i++) printf(" %08lx", h(list[i]));
    printf("\n");
    for (tries = 0; tries < 100; tries++)
    {
        tray = FindWindowW(L"Shell_TrayWnd", NULL);
        ind = tray ? FindWindowExW(tray, NULL, L"TrayInputIndicatorWClass", NULL) : NULL;
        if (ind && IsWindowVisible(ind)) break;
        Sleep(100);
    }
    if (ind && GetWindowRect(ind, &r)) printf("INDRECT %ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"sginputprobe";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"sginputprobe", L"sginputprobe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 300, 200,
                         NULL, NULL, wc.hInstance, NULL);
    shellhook = RegisterWindowMessageW(L"SHELLHOOK");
    RegisterShellHookWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetTimer(hwnd, 1, 60000, NULL);
    SetTimer(hwnd, 2, 50, NULL);
    watch();
    printf("READY %08lx\n", h(GetKeyboardLayout(0)));
    fflush(stdout);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
