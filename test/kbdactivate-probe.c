/* kbdactivate-probe (patches/sg/9156, test/kbdactivate-gate.sh): a window "sgkbdactivate" that logs what
 * the keyboard sends it, one line each, and activates layouts when the gate types its letters:
 *   LIST n hkl... / REQ flags hkl / CHANGE hkl / DOWN vk scan / CHAR xxxx    as kbdgroups-probe
 *   q: ACT old new      ActivateKeyboardLayout(German, 0) from the foreground window's thread
 *   z: BG old new       ActivateKeyboardLayout(US, 0) from another thread, whose window is not the foreground
 *   w: STALL            the window's thread sleeps 1.5 s (two switches queue up meanwhile)
 *   e: NOW hkl, DONE
 * Gives up after 60 s. */
#include <windows.h>
#include <stdio.h>

#define US     ((HKL)(ULONG_PTR)0x04090409)
#define GERMAN ((HKL)(ULONG_PTR)0x04070409)

static unsigned long h(HKL hkl) { return (unsigned long)(ULONG_PTR)hkl; }

static DWORD WINAPI background(void *arg)
{
    /* a thread of the program's with a window of its own, not the foreground one */
    HWND hwnd = CreateWindowW(L"sgkbdactivate", L"sgkbdbackground", WS_OVERLAPPEDWINDOW, 400, 50, 200, 100,
                              NULL, NULL, GetModuleHandleW(NULL), NULL);
    HKL old = ActivateKeyboardLayout(US, 0);
    (void)arg;
    DestroyWindow(hwnd);
    printf("BG %08lx %08lx\n", h(old), h(GetKeyboardLayout(0)));
    fflush(stdout);
    return 0;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HKL old;
    HANDLE thread;
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
        switch (wp)
        {
        case 'q':
            old = ActivateKeyboardLayout(GERMAN, 0);
            printf("ACT %08lx %08lx\n", h(old), h(GetKeyboardLayout(0))); fflush(stdout);
            break;
        case 'z':
            thread = CreateThread(NULL, 0, background, NULL, 0, NULL);
            WaitForSingleObject(thread, 5000);
            CloseHandle(thread);
            break;
        case 'w':
            printf("STALL\n"); fflush(stdout);
            Sleep(1500);
            break;
        case 'e':
            printf("NOW %08lx\nDONE\n", h(GetKeyboardLayout(0))); fflush(stdout);
            PostQuitMessage(0);
            break;
        }
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
    wc.lpszClassName = L"sgkbdactivate";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"sgkbdactivate", L"sgkbdactivate", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 300, 200,
                         NULL, NULL, wc.hInstance, NULL);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetTimer(hwnd, 1, 60000, NULL);
    printf("READY %08lx\n", h(GetKeyboardLayout(0)));
    fflush(stdout);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
