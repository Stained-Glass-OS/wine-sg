/* Probe for patches/sg/5208: RegisterTouchHitTestingWindow validates the window and the value
 * (TOUCH_HIT_TESTING_DEFAULT/CLIENT/NONE) and records the registration on the window.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef BOOL (WINAPI *reg_t)(HWND, ULONG);

int main(void)
{
    static const struct { ULONG value; BOOL ok; DWORD err; } tests[] = {
        { 0, TRUE, 0 }, { 1, TRUE, 0 }, { 2, TRUE, 0 },
        { 3, FALSE, ERROR_INVALID_PARAMETER }, { 0xff, FALSE, ERROR_INVALID_PARAMETER },
        { 0xffffffff, FALSE, ERROR_INVALID_PARAMETER },
    };
    reg_t pRegister = (reg_t)GetProcAddress(GetModuleHandleA("user32.dll"), "RegisterTouchHitTestingWindow");
    HWND hwnd;
    unsigned int i;
    BOOL ret;
    HANDLE prop;

    if (!pRegister) { printf("ok   not available\nRESULT: PASS\n"); return 0; }
    hwnd = CreateWindowA("static", "t", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    check(hwnd != NULL, "window created");

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        SetLastError(0xdeadbeef);
        ret = pRegister(hwnd, tests[i].value);
        check(ret == tests[i].ok && (ret || GetLastError() == tests[i].err),
              "value %#lx: ret %d err %lu (want %d / %lu)", tests[i].value, ret, GetLastError(), tests[i].ok, tests[i].err);
    }

    /* the last good registration (NONE = 2) is still recorded after a bad one */
    pRegister(hwnd, 2);
    pRegister(hwnd, 7);
    prop = GetPropW(hwnd, L"__wine_sg_touch_hit_testing");
    check((ULONG_PTR)prop == (2 | 0x10000), "registration recorded on the window (%p)", prop);
    pRegister(hwnd, 1);
    prop = GetPropW(hwnd, L"__wine_sg_touch_hit_testing");
    check((ULONG_PTR)prop == (1 | 0x10000), "re-registration replaces it (%p)", prop);

    SetLastError(0xdeadbeef);
    ret = pRegister(NULL, 1);
    check(!ret && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "NULL window: ret %d err %lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = pRegister((HWND)0xdeadbee0, 1);
    check(!ret && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "bad window: ret %d err %lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = pRegister((HWND)0xdeadbee0, 9);
    check(!ret && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "bad window and bad value: window checked first (err %lu)", GetLastError());

    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
