/* fastshdocvw-probe: shdocvw's SetQueryNetSessionCount is ieframe's (one count for both) */
#include <windows.h>
#include <stdio.h>

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef LONG (WINAPI *count_fn)(DWORD);

int main(void)
{
    HMODULE sh = LoadLibraryA("shdocvw.dll"), ie = LoadLibraryA("ieframe.dll");
    count_fn psh, pie;
    LONG base;

    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(sh && ie, "load %p %p", sh, ie);
    if (!sh || !ie) goto done;
    psh = (count_fn)GetProcAddress(sh, "SetQueryNetSessionCount");
    pie = (count_fn)GetProcAddress(ie, "SetQueryNetSessionCount");
    CHECK(psh && pie, "exports");
    if (!psh || !pie) goto done;

    base = psh(0);
    CHECK(pie(0) == base, "same start %ld %ld", base, pie(0));
    CHECK(psh(1) == base + 1, "increment through shdocvw");
    CHECK(pie(0) == base + 1, "ieframe sees it: %ld", pie(0));
    CHECK(pie(1) == base + 2, "increment through ieframe");
    CHECK(psh(0) == base + 2, "shdocvw sees it: %ld", psh(0));
    CHECK(psh(2) == base + 1, "decrement through shdocvw");
    CHECK(pie(2) == base, "decrement through ieframe");
    CHECK(psh(0) == base && pie(0) == base, "back at the start");
    CHECK(psh(7) == 0, "unknown operation");
done:
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
