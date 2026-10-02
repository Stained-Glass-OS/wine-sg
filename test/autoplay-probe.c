/* AutoPlay's notification for a drive that arrives (patches/sg/0748).
 *   autoplay-probe.exe LETTER SECONDS
 * waits for a balloon (a visible tooltip with the balloon style) to come up,
 * prints "balloon=1", clicks it as the person would (a button release on
 * it), then waits for a window whose title names the drive ("<letter>:") and
 * prints "opened=<its title>". */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>

static HWND found;
static WCHAR want[4], title[256];

static BOOL CALLBACK find_balloon(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    if (IsWindowVisible(hwnd) && GetClassNameW(hwnd, cls, 64) && !wcscmp(cls, TOOLTIPS_CLASSW) &&
        (GetWindowLongW(hwnd, GWL_STYLE) & TTS_BALLOON)) { found = hwnd; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK find_drive_window(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    if (!IsWindowVisible(hwnd) || !GetWindowTextW(hwnd, title, 256)) return TRUE;
    if (GetClassNameW(hwnd, cls, 64) && !wcscmp(cls, TOOLTIPS_CLASSW)) return TRUE;
    if (wcsstr(title, want)) { found = hwnd; return FALSE; }
    return TRUE;
}

int main(int argc, char **argv)
{
    DWORD end;
    if (argc < 3) return 2;
    swprintf(want, 4, L"%c:", argv[1][0]);
    end = GetTickCount() + atoi(argv[2]) * 1000;
    while (!found && (int)(end - GetTickCount()) > 0) { EnumWindows(find_balloon, 0); if (!found) Sleep(200); }
    printf("balloon=%d\n", found != NULL);
    fflush(stdout);
    if (!found) return 1;
    SendMessageW(found, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
    SendMessageW(found, WM_LBUTTONUP, 0, MAKELPARAM(10, 10));
    found = NULL;
    end = GetTickCount() + 20000;
    while (!found && (int)(end - GetTickCount()) > 0) { EnumWindows(find_drive_window, 0); if (!found) Sleep(300); }
    printf("opened=%ls\n", found ? title : L"");
    return 0;
}
