/* Probe for test/hungwake-gate.sh (wine-sg 1128):
 *   hungwake-probe idle   a window whose thread waits for its messages (and
 *                         an event "hdwake"); woken by the event it is at
 *                         work 1.5 s before it reads its messages again
 *   hungwake-probe        wakes it, then SendMessageTimeout(SMTO_ABORTIFHUNG):
 *                         "ok=1 r=77" when it answered, "ok=0" when it was
 *                         taken for hung
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l) { if (m == WM_USER + 7) return 77; return DefWindowProcW(h, m, w, l); }
int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "idle")) {
        WNDCLASSW wc = {0}; MSG msg;
        HANDLE ev = CreateEventW(NULL, FALSE, FALSE, L"hdwake");
        wc.lpfnWndProc = wp; wc.lpszClassName = L"hdwake"; RegisterClassW(&wc);
        CreateWindowExW(0, L"hdwake", L"hdwake", WS_POPUP, 0, 0, 50, 50, 0, 0, 0, 0);
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        for (;;) {
            DWORD r = MsgWaitForMultipleObjects(1, &ev, FALSE, INFINITE, QS_ALLINPUT);
            if (r == WAIT_OBJECT_0) Sleep(1500);   /* woken, at work a moment */
            while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) return 0; DispatchMessageW(&msg); }
        }
    }
    HWND h = FindWindowW(L"hdwake", NULL); DWORD_PTR r = 0; LRESULT ok;
    HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"hdwake");
    if (!h || !ev) { printf("none\n"); return 1; }
    SetEvent(ev); Sleep(200);
    ok = SendMessageTimeoutW(h, WM_USER + 7, 0, 0, SMTO_ABORTIFHUNG, 5000, &r);
    printf("ok=%d r=%d\n", (int)ok, (int)r);
    return 0;
}
