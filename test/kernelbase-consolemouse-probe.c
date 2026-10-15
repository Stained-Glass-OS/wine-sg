/* Probe for patches/sg/5071: GetNumberOfConsoleMouseButtons asks the console.
 * - in a console it is the number of buttons of the mouse (SM_CMOUSEBUTTONS), as
 *   conhost answers on Windows (it reported a fixed 2); started with no console host
 *   (Wine's console without a window, where every console call fails) it fails too;
 * - in a console of its own (AllocConsole) the same;
 * - with no console it fails; a NULL pointer is ERROR_INVALID_PARAMETER.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    int expect = GetSystemMetrics(SM_CMOUSEBUTTONS);
    DWORD count;
    BOOL ret;

    BOOL hosted = GetConsoleOutputCP() != 0 && GetConsoleWindow() != NULL;

    count = 0xdead;
    ret = GetNumberOfConsoleMouseButtons(&count);
    if (hosted)
        check(ret && count == (DWORD)expect, "in the console it was started in: %d %lu (SM_CMOUSEBUTTONS %d)", ret, count, expect);
    else
        check(!ret, "started with no console host: %d %lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = GetNumberOfConsoleMouseButtons(NULL);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL: %d %lu", ret, GetLastError());

    /* stdout is the gate's pipe, not the console: it stays */
    fflush(stdout);
    FreeConsole();
    count = 0xdead;
    SetLastError(0xdeadbeef);
    ret = GetNumberOfConsoleMouseButtons(&count);
    check(!ret, "no console: %d %lu", ret, GetLastError());
    ret = AllocConsole();
    check(ret, "AllocConsole %lu", ret ? 0 : GetLastError());
    if (ret)
    {
        count = 0xdead;
        ret = GetNumberOfConsoleMouseButtons(&count);
        check(ret && count == (DWORD)expect, "a new console: %d %lu error %lu", ret, count, ret ? 0 : GetLastError());
        FreeConsole();
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
