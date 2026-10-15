/* Probe for patches/sg/5212: MNS_AUTODISMISS ends menu tracking after about 10 seconds with the
 * mouse outside the menu; a menu without the style stays until it is ended; the mouse
 * moving inside the menu restarts the time.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HWND hwnd;
static DWORD end_after, move_inside_at;
static HMENU cur_menu;
static POINT inside_pt;

static DWORD WINAPI helper(void *arg)
{
    DWORD start = GetTickCount();
    if (move_inside_at)
    {
        Sleep(move_inside_at);
        /* a synthetic move over the menu */
        SetCursorPos(inside_pt.x, inside_pt.y);
        Sleep(200);
        SetCursorPos(inside_pt.x + 1, inside_pt.y + 1);
        Sleep(200);
        SetCursorPos(5, 5);
    }
    Sleep(end_after > GetTickCount() - start ? end_after - (GetTickCount() - start) : 0);
    PostMessageW(hwnd, WM_CANCELMODE, 0, 0);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    EndMenu();
    return 0;
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return DefWindowProcA(h, m, w, l);
}

static DWORD run_case(DWORD style, DWORD end_ms, DWORD move_ms)
{
    HMENU menu = CreatePopupMenu();
    MENUINFO mi = {sizeof(mi), MIM_STYLE, style};
    DWORD t0;
    HANDLE th;
    BOOL ret;

    AppendMenuA(menu, MF_STRING, 100, "one");
    AppendMenuA(menu, MF_STRING, 101, "two");
    if (style) SetMenuInfo(menu, &mi);
    SetCursorPos(5, 5);
    end_after = end_ms;
    move_inside_at = move_ms;
    inside_pt.x = 405; inside_pt.y = 405;
    cur_menu = menu;
    t0 = GetTickCount();
    th = CreateThread(NULL, 0, helper, NULL, 0, NULL);
    SetForegroundWindow(hwnd);
    ret = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, 400, 400, 0, hwnd, NULL);
    t0 = GetTickCount() - t0;
    WaitForSingleObject(th, 30000);
    CloseHandle(th);
    {   /* the helper's late cancel messages must not hit the next case */
        MSG m;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) ;
    }
    DestroyMenu(menu);
    (void)ret;
    return t0;
}

int main(void)
{
    WNDCLASSA wc = {0};
    DWORD el;

    setvbuf(stdout, NULL, _IONBF, 0);
    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "SgAutoDismiss";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("SgAutoDismiss", "t", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    check(hwnd != NULL, "window");

    /* a plain menu with the mouse away stays until the helper ends it at 13 s */
    el = run_case(0, 13000, 0);
    check(el >= 12500 && el < 16000, "no style: menu lasted %lu ms (ended by the helper at 13000)", el);

    /* MNS_AUTODISMISS: gone after about 10 s on its own */
    el = run_case(MNS_AUTODISMISS, 20000, 0);
    check(el >= 9000 && el < 12500, "MNS_AUTODISMISS: menu lasted %lu ms (about 10000)", el);

    /* the mouse over the menu at 6 s restarts the 10 s (it then moves away again) */
    el = run_case(MNS_AUTODISMISS, 25000, 6000);
    check(el >= 15500 && el < 19000, "MNS_AUTODISMISS with the mouse inside at 6 s: menu lasted %lu ms", el);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
