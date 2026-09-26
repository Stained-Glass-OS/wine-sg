/* cryptbase.dll's RtlGenRandom (0303): Mozilla's mozglue.dll imports it as
 * CRYPTBASE.SystemFunction036, so Firefox-140-based programs (Zotero 10,
 * Thunderbird 140) did not start (c0000135). Prints "loaded=1 random=1". */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    BOOLEAN (WINAPI *gen)(void *, ULONG);
    BYTE buf[32] = {0};
    HMODULE mod = LoadLibraryA("CRYPTBASE.dll");   /* as mozglue names it */
    int i, nonzero = 0;

    if (!mod) { printf("loaded=0 error=%lu\n", GetLastError()); return 1; }
    gen = (void *)GetProcAddress(mod, "SystemFunction036");
    if (!gen) { printf("loaded=1 export=0\n"); return 1; }
    if (!gen(buf, sizeof(buf))) { printf("loaded=1 random=0\n"); return 1; }
    for (i = 0; i < sizeof(buf); i++) if (buf[i]) nonzero++;
    printf("loaded=1 random=%d\n", nonzero > 8);
    return 0;
}
