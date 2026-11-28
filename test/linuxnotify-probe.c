/* linuxnotify-gate.sh's helper (patches/sg/9512).
 *
 *   linuxnotify-probe toasts              the toasts on screen: toast=l,t,r,b a line
 *   linuxnotify-probe activate SEQ [ARGS] the notification centre's click on history entry SEQ
 */
#include <windows.h>
#include <stdio.h>

static BOOL CALLBACK each(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    RECT rc;
    GetClassNameW(hwnd, cls, 64);
    if (!lstrcmpW(cls, L"WineToastNotification") && IsWindowVisible(hwnd))
    {
        GetWindowRect(hwnd, &rc);
        printf("toast=%ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom);
        (*(int *)lp)++;
    }
    return TRUE;
}

int wmain(int argc, WCHAR **argv)
{
    if (argc >= 2 && !lstrcmpW(argv[1], L"toasts"))
    {
        int n = 0;
        EnumWindows(each, (LPARAM)&n);
        printf("toasts=%d\n", n);
        return 0;
    }
    if (argc >= 3 && !lstrcmpW(argv[1], L"activate"))
    {
        HRESULT (WINAPI *activate)(DWORD, const WCHAR *) =
            (void *)GetProcAddress(LoadLibraryW(L"windows.ui.dll"), "SgActivateNotification");
        if (!activate) return 2;
        printf("hr=%#lx\n", activate(_wtoi(argv[2]), argc >= 4 ? argv[3] : NULL));
        return 0;
    }
    fprintf(stderr, "usage: linuxnotify-probe toasts | activate SEQ [ARGS]\n");
    return 1;
}
