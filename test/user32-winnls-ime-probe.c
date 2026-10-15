/* Probe for patches/sg/5213: WINNLSEnableIME returns the previous state (hwnd must be NULL) and
 * WINNLSGetEnableStatus returns the state it left.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef BOOL (WINAPI *enable_t)(HWND, BOOL);
typedef BOOL (WINAPI *status_t)(HWND);

int main(void)
{
    static const struct { BOOL set; BOOL want_prev; BOOL want_status; } steps[] = {
        { FALSE, TRUE, FALSE },   /* enabled at start */
        { FALSE, FALSE, FALSE },
        { TRUE, FALSE, TRUE },
        { TRUE, TRUE, TRUE },
        { 5, TRUE, TRUE },        /* any non-zero value is TRUE */
        { FALSE, TRUE, FALSE },
        { TRUE, FALSE, TRUE },
    };
    HMODULE u32 = GetModuleHandleA("user32.dll");
    enable_t pEnable = (enable_t)GetProcAddress(u32, "WINNLSEnableIME");
    status_t pStatus = (status_t)GetProcAddress(u32, "WINNLSGetEnableStatus");
    HWND hwnd;
    unsigned int i;
    BOOL ret;

    if (!pEnable || !pStatus) { printf("ok   not available\nRESULT: PASS\n"); return 0; }
    for (i = 0; i < sizeof(steps) / sizeof(*steps); i++)
    {
        ret = pEnable(NULL, steps[i].set);
        check(ret == steps[i].want_prev, "step %u: enable(%d) returned %d, want %d", i, steps[i].set, ret, steps[i].want_prev);
        ret = pStatus(NULL);
        check(ret == steps[i].want_status, "step %u: status %d, want %d", i, ret, steps[i].want_status);
    }

    hwnd = CreateWindowA("static", "t", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    SetLastError(0xdeadbeef);
    ret = pEnable(hwnd, FALSE);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "a window handle is refused (%d, %lu)", ret, GetLastError());
    ret = pStatus(NULL);
    check(ret == TRUE, "a refused call changes nothing (%d)", ret);
    SetLastError(0xdeadbeef);
    ret = pStatus(hwnd);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "status with a window handle is refused (%d, %lu)", ret, GetLastError());
    DestroyWindow(hwnd);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
