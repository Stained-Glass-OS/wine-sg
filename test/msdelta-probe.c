/* msdelta-probe: calls the delta API as Microsoft 365's Click-to-Run does,
 * with a delta that is not one; prints what each returns. The process must
 * live through it, and (32-bit) keep its stack. (patches/sg/0475) */
#include <windows.h>
#include <stdio.h>

typedef struct { const void *start; SIZE_T size; BOOL editable; } DELTA_INPUT_;
typedef struct { void *start; SIZE_T size; } DELTA_OUTPUT_;

int main(void)
{
    HMODULE m = LoadLibraryA("msdelta.dll");
    BOOL (WINAPI *apply_b)(__int64, DELTA_INPUT_, DELTA_INPUT_, DELTA_OUTPUT_ *);
    BOOL (WINAPI *apply_w)(__int64, const WCHAR *, const WCHAR *, const WCHAR *);
    BOOL (WINAPI *info_b)(DELTA_INPUT_, void *);
    BOOL (WINAPI *delta_free)(void *);
    static const char junk[] = "PA30 not really a delta";
    DELTA_INPUT_ source = { junk, sizeof(junk), FALSE }, delta = { junk, sizeof(junk), FALSE };
    DELTA_OUTPUT_ out = { (void *)1, 1 };
    BYTE info[512];
    volatile int canary = 0x5a5a;
    BOOL r;

    if (!m) { printf("no msdelta\n"); return 1; }
    apply_b = (void *)GetProcAddress(m, "ApplyDeltaB");
    apply_w = (void *)GetProcAddress(m, "ApplyDeltaW");
    info_b = (void *)GetProcAddress(m, "GetDeltaInfoB");
    delta_free = (void *)GetProcAddress(m, "DeltaFree");
    SetLastError(0); r = apply_b(0, source, delta, &out);
    printf("ApplyDeltaB %d %lu %s\n", r, GetLastError(), out.start || out.size ? "output-left" : "output-cleared");
    SetLastError(0); r = apply_w(0, L"C:\\nope.src", L"C:\\nope.delta", L"C:\\nope.out");
    printf("ApplyDeltaW %d %lu\n", r, GetLastError());
    SetLastError(0); r = info_b(delta, info);
    printf("GetDeltaInfoB %d %lu\n", r, GetLastError());
    printf("DeltaFree %d\n", delta_free(NULL));
    printf("stack %s\n", canary == 0x5a5a ? "kept" : "broken");
    return 0;
}
