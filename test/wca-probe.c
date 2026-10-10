/* SetWindowCompositionAttribute (patches/sg/2224): dark-mode colours and accent
 * (blur) policy reach the window like the DWM attributes do; bad calls are
 * refused.  It used to fail every call with ERROR_CALL_NOT_IMPLEMENTED. */
#include <windows.h>
#include <stdio.h>

typedef struct { DWORD attrib; void *data; SIZE_T size; } WCAD;
typedef struct { DWORD state, flags, color, animation; } ACCENT;
static BOOL (WINAPI *pSet)(HWND, WCAD *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    HWND hwnd = CreateWindowA("STATIC", "wca", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    HMODULE dwm = LoadLibraryA("dwmapi.dll");
    HRESULT (WINAPI *pGetAttr)(HWND, DWORD, void *, DWORD) = dwm ? (void *)GetProcAddress(dwm, "DwmGetWindowAttribute") : NULL;
    WCAD d;
    BOOL dark;
    ACCENT acc;
    DWORD value;

    pSet = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "SetWindowCompositionAttribute");
    check(pSet != NULL && hwnd != NULL, "the function and a window exist");

    dark = TRUE;
    d.attrib = 26; d.data = &dark; d.size = sizeof(dark);
    SetLastError(0xdeadbeef);
    check(pSet(hwnd, &d), "WCA_USEDARKMODECOLORS succeeds");
    check(GetPropW(hwnd, L"__wine_dark_caption") != NULL, "...and the window has a dark caption");
    if (pGetAttr)
    {
        BOOL got = FALSE;
        check(pGetAttr(hwnd, 20, &got, sizeof(got)) == S_OK && got, "...which DwmGetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE) reports");
    }
    dark = FALSE;
    check(pSet(hwnd, &d) && GetPropW(hwnd, L"__wine_dark_caption") == NULL, "turning it off removes it");

    acc.state = 3; acc.flags = 0; acc.color = 0; acc.animation = 0;
    d.attrib = 19; d.data = &acc; d.size = sizeof(acc);
    check(pSet(hwnd, &d) && GetPropW(hwnd, L"__wine_dwm_blur") != NULL, "ACCENT_ENABLE_BLURBEHIND blurs behind the window");
    acc.state = 4;
    check(pSet(hwnd, &d) && GetPropW(hwnd, L"__wine_dwm_blur") != NULL, "...acrylic too");
    acc.state = 0;
    check(pSet(hwnd, &d) && GetPropW(hwnd, L"__wine_dwm_blur") == NULL, "ACCENT_DISABLED turns it off");
    acc.state = 1;
    check(pSet(hwnd, &d) && GetPropW(hwnd, L"__wine_dwm_blur") == NULL, "a plain gradient does not blur");

    value = 1;
    d.attrib = 16; d.data = &value; d.size = sizeof(value);
    check(pSet(hwnd, &d), "another attribute of the family (DISALLOW_PEEK) is accepted");

    /* refusals */
    SetLastError(0);
    d.attrib = 26; d.data = &dark; d.size = 2;
    check(!pSet(hwnd, &d) && GetLastError() == ERROR_INVALID_PARAMETER, "a wrong size is ERROR_INVALID_PARAMETER");
    SetLastError(0);
    d.attrib = 777; d.size = sizeof(dark);
    check(!pSet(hwnd, &d) && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown attribute is ERROR_INVALID_PARAMETER");
    SetLastError(0);
    check(!pSet(hwnd, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "no data is ERROR_INVALID_PARAMETER");
    SetLastError(0);
    d.attrib = 26;
    check(!pSet((HWND)0x7ff1, &d) && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "a window that is not there is ERROR_INVALID_WINDOW_HANDLE");
    SetLastError(0);
    check(1, "(done)");

    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
