/* SetThreadIdealProcessor range (patches/sg/2245): up to 64 accepted, 65 is invalid;
 * the previous ideal processor is the return value. */
#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void)
{
    DWORD ret;
    SYSTEM_INFO si;

    GetSystemInfo(&si);
    SetLastError(0xdeadbeef);
    ret = SetThreadIdealProcessor(GetCurrentThread(), 0);
    CHECK(ret != ~0u, "set 0: %lu", ret);
    ret = SetThreadIdealProcessor(GetCurrentThread(), 0);
    CHECK(ret == 0, "previous is 0: %lu", ret);
    if (si.dwNumberOfProcessors > 1)
    {
        SetThreadIdealProcessor(GetCurrentThread(), 1);
        ret = SetThreadIdealProcessor(GetCurrentThread(), 0);
        CHECK(ret == 1, "previous is 1: %lu", ret);
    }
    ret = SetThreadIdealProcessor(GetCurrentThread(), 33);
    CHECK(ret != ~0u, "33 is accepted: %lu err %lu", ret, GetLastError());
    ret = SetThreadIdealProcessor(GetCurrentThread(), MAXIMUM_PROCESSORS);
    CHECK(ret != ~0u, "MAXIMUM_PROCESSORS: %lu", ret);
    ret = SetThreadIdealProcessor(GetCurrentThread(), 64);
    CHECK(ret != ~0u, "64: %lu", ret);
    SetLastError(0xdeadbeef);
    ret = SetThreadIdealProcessor(GetCurrentThread(), 65);
    CHECK(ret == ~0u && GetLastError() == ERROR_INVALID_PARAMETER, "65: %lu err %lu", ret, GetLastError());
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
