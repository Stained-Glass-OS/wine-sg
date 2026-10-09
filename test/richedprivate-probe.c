/* richedprivate-probe MODE: load riched20 and say whose it is.
 *   path:  LoadLibrary("<dir>\\riched20.dll") by full path (Office's way)
 *   name:  LoadLibrary("riched20.dll") -- the exe's directory is searched first
 *   system: LoadLibrary("C:\\windows\\system32\\riched20.dll")
 *   msftedit: LoadLibrary("msftedit.dll") from a directory holding a private riched20.dll
 * Prints "private=<1|0> system=<1|0>": the private copy's export / the system riched20's CreateTextServices. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
/* which riched20 the process has: the system's (CreateTextServices) or the stand-in */
static const char *whose(void)
{
    HMODULE r = GetModuleHandleA("riched20.dll");
    if (!r) return "none";
    if (GetProcAddress(r, "SgPrivateRichEdit")) return "private";
    return GetProcAddress(r, "CreateTextServices") ? "system" : "other";
}

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    HMODULE m;
    if (argc > 1 && !strcmp(argv[1], "msftedit"))
    {
        /* Wine's msftedit imports the system riched20: a private copy beside
         * the program must not be taken for it */
        WNDCLASSW wc;
        m = LoadLibraryA("msftedit.dll");
        printf("msftedit=%d class=%d textservices=%d riched20=%s\n", m != NULL, GetClassInfoW(m, L"RICHEDIT50W", &wc) != 0,
               m && GetProcAddress(m, "CreateTextServices") != NULL, whose());
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "riched32"))
    {
        /* Wine's riched32 imports the system riched20 (its RichEdit 1.0
         * window procedure) */
        WNDCLASSA wc;
        m = LoadLibraryA("riched32.dll");
        printf("riched32=%d class=%d riched20=%s\n", m != NULL, GetClassInfoA(m, "RichEdit", &wc) != 0, whose());
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "system"))
    {
        /* C:\windows\system32\riched20.dll by full path: always the system's */
        m = LoadLibraryA("C:\\windows\\system32\\riched20.dll");
        printf("private=%d system=%d\n", m && GetProcAddress(m, "SgPrivateRichEdit") != NULL,
               m && GetProcAddress(m, "CreateTextServices") != NULL);
        return 0;
    }
    if (argc > 2) { snprintf(path, sizeof(path), "%s\\riched20.dll", argv[2]); m = LoadLibraryA(path); }
    else m = LoadLibraryA("riched20.dll");
    printf("private=%d system=%d\n", m && GetProcAddress(m, "SgPrivateRichEdit") != NULL,
           m && GetProcAddress(m, "CreateTextServices") != NULL);
    return 0;
}
