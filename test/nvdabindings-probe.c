/* The functions NVDA binds by name at start are there (patches/sg/5941):
 * kernel32.GetConsoleSelectionInfo, user32.RegisterPointerInputTarget,
 * UnregisterPointerInputTarget, GhostWindowFromHungWindow,
 * oleacc.AccSetRunningUtilityState, AccNotifyTouchInteraction. NVDA's ctypes
 * bindings stopped at the first missing one ("function ... not found"). */
#include <windows.h>
#include <stdio.h>

static int bad;
static FARPROC need(const char *dll, const char *fn)
{
    FARPROC p = GetProcAddress(LoadLibraryA(dll), fn);
    if (!p) { printf("MISSING %s.%s\n", dll, fn); bad++; }
    return p;
}

int main(void)
{
    BOOL (WINAPI *sel)(void *) = (void *)need("kernel32", "GetConsoleSelectionInfo");
    BOOL (WINAPI *reg)(HWND, DWORD) = (void *)need("user32", "RegisterPointerInputTarget");
    BOOL (WINAPI *unreg)(HWND, DWORD) = (void *)need("user32", "UnregisterPointerInputTarget");
    HWND (WINAPI *ghost)(HWND) = (void *)need("user32", "GhostWindowFromHungWindow");
    HRESULT (WINAPI *util)(HWND, DWORD, DWORD) = (void *)need("oleacc", "AccSetRunningUtilityState");
    HRESULT (WINAPI *touch)(HWND, HWND, POINT) = (void *)need("oleacc", "AccNotifyTouchInteraction");
    DWORD info[5] = { 0xdead, 0xdead, 0xdead, 0xdead, 0xdead };
    POINT pt = { 1, 2 };

    AllocConsole();
    if (sel && !(sel(info) && info[0] == 0)) { printf("BAD GetConsoleSelectionInfo: flags %#lx err %lu\n", info[0], GetLastError()); bad++; }
    if (reg && (reg(GetDesktopWindow(), 2) || GetLastError() != ERROR_ACCESS_DENIED)) { printf("BAD RegisterPointerInputTarget\n"); bad++; }
    if (unreg && unreg(GetDesktopWindow(), 2)) { printf("BAD UnregisterPointerInputTarget\n"); bad++; }
    if (ghost && ghost(GetDesktopWindow())) { printf("BAD GhostWindowFromHungWindow\n"); bad++; }
    if (util && util(NULL, 1, 1) != S_OK) { printf("BAD AccSetRunningUtilityState\n"); bad++; }
    if (touch && touch(NULL, NULL, pt) != S_OK) { printf("BAD AccNotifyTouchInteraction\n"); bad++; }
    printf(bad ? "FAILED %d\n" : "ALL-OK\n", bad);
    return bad;
}
