/* kbdgroups-probe (patches/sg/9154, 9155, test/kbdgroups-gate.sh): a window "sgkbdgroups" that logs what the
 * keyboard sends it, one line each, while the gate types and switches the session's layouts:
 *   LIST n hkl...                    GetKeyboardLayoutList at start
 *   REQ flags hkl                    WM_INPUTLANGCHANGEREQUEST (then DefWindowProc: the layout is activated)
 *   CHANGE hkl                       WM_INPUTLANGCHANGE
 *   DOWN vk scan / CHAR xxxx         WM_KEYDOWN, WM_CHAR
 * On the character q: NOW hkl, then NEXT/NEXT2/PREV old new for ActivateKeyboardLayout(HKL_NEXT, HKL_NEXT,
 * HKL_PREV), and DONE. Gives up after 40 s. */
#include <windows.h>
#include <stdio.h>

static unsigned long h(HKL hkl) { return (unsigned long)(ULONG_PTR)hkl; }

static void hkl_tests(void)
{
    HKL old;
    printf("NOW %08lx\n", h(GetKeyboardLayout(0)));
    old = ActivateKeyboardLayout((HKL)HKL_NEXT, 0);
    printf("NEXT %08lx %08lx\n", h(old), h(GetKeyboardLayout(0)));
    old = ActivateKeyboardLayout((HKL)HKL_NEXT, 0);
    printf("NEXT2 %08lx %08lx\n", h(old), h(GetKeyboardLayout(0)));
    old = ActivateKeyboardLayout((HKL)HKL_PREV, 0);
    printf("PREV %08lx %08lx\n", h(old), h(GetKeyboardLayout(0)));
    printf("DONE\n");
    fflush(stdout);
    PostQuitMessage(0);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INPUTLANGCHANGEREQUEST:
        printf("REQ %lx %08lx\n", (unsigned long)wp, (unsigned long)lp); fflush(stdout);
        break;
    case WM_INPUTLANGCHANGE:
        printf("CHANGE %08lx\n", (unsigned long)lp); fflush(stdout);
        break;
    case WM_KEYDOWN:
        printf("DOWN %02lx %02lx\n", (unsigned long)wp, (unsigned long)((lp >> 16) & 0xff)); fflush(stdout);
        break;
    case WM_CHAR:
        printf("CHAR %04lx\n", (unsigned long)wp); fflush(stdout);
        if (wp == 'q') hkl_tests();
        return 0;
    case WM_TIMER:
        printf("TIMEOUT\n"); fflush(stdout);
        PostQuitMessage(1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(void)
{
    WNDCLASSW wc = {0};
    HKL list[16];
    HWND hwnd;
    MSG msg;
    UINT n, i;

    n = GetKeyboardLayoutList(16, list);
    printf("LIST %u", n);
    for (i = 0; i < n; i++) printf(" %08lx", h(list[i]));
    printf("\n");
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"sgkbdgroups";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"sgkbdgroups", L"sgkbdgroups", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 300, 200,
                         NULL, NULL, wc.hInstance, NULL);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetTimer(hwnd, 1, 40000, NULL);
    printf("READY %08lx\n", h(GetKeyboardLayout(0)));
    fflush(stdout);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
