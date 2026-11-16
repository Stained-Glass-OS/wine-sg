/* icu.dll's locale list (wine-sg 7806): no en_US_POSIX, as Windows' icu.dll,
 * and every other host locale once, in order, ending at the count. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    HMODULE icu = LoadLibraryA("icu.dll");
    int (__cdecl *count)(void) = icu ? (void *)GetProcAddress(icu, "uloc_countAvailable") : NULL;
    const char *(__cdecl *get)(int) = icu ? (void *)GetProcAddress(icu, "uloc_getAvailable") : NULL;
    int i, n, posix = 0, enus = 0, nulls = 0, unordered = 0;
    const char *prev = NULL;

    if (!count || !get) { printf("load=0\n"); return 1; }
    n = count();
    for (i = 0; i < n; i++)
    {
        const char *s = get(i);
        if (!s) { nulls++; continue; }
        if (!strcmp(s, "en_US_POSIX")) posix++;
        if (!strcmp(s, "en_US")) enus++;
        if (prev && strcmp(prev, s) >= 0) unordered++;
        prev = s;
    }
    printf("load=1\ncount_big=%d\nposix=%d\nenus=%d\nnulls=%d\nunordered=%d\n",
           n > 100, posix, enus, nulls, unordered);
    return 0;
}
