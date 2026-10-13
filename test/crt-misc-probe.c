/* Probe for patches/sg/4505:
 *   probe.exe fma                  _get_FMA3_enable (ucrtbase) / _set_FMA3_enable (msvcr120), 64-bit
 *   probe.exe typeinfo             __clean_type_info_names_internal (msvcr80, msvcr90)
 *   probe.exe secerr DLL CODE      __security_error_handler of msvcr70/msvcr71: must not return */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "fma"))
    {
#ifdef _WIN64
        int (__cdecl *get)(void) = (void *)GetProcAddress(LoadLibraryA("ucrtbase.dll"), "_get_FMA3_enable");
        int (__cdecl *set)(int) = (void *)GetProcAddress(LoadLibraryA("msvcr120.dll"), "_set_FMA3_enable");
        int (__cdecl *set_u)(int) = (void *)GetProcAddress(LoadLibraryA("ucrtbase.dll"), "_set_FMA3_enable");

        check(get && set && set_u, "exports");
        if (get && set && set_u)
        {
            check(set(1) == 0, "msvcr120 _set_FMA3_enable(1): no FMA3 implementations in use");
            check(set_u(1) == 0 && get() == 0, "ucrtbase _set/_get_FMA3_enable");
            check(set_u(0) == 0 && get() == 0, "disabling");
        }
#else
        printf("ok   32-bit has no FMA3 functions\n");
#endif
    }
    else if (!strcmp(argv[1], "typeinfo"))
    {
        static const char *dlls[] = {"msvcr80.dll", "msvcr90.dll"};
        unsigned int i;

        for (i = 0; i < ARRAYSIZE(dlls); i++)
        {
            void (__cdecl *clean)(void *) = (void *)GetProcAddress(LoadLibraryA(dlls[i]), "__clean_type_info_names_internal");
            struct { void *next; } node = {NULL};

            check(clean != NULL, "%s export", dlls[i]);
            if (!clean) continue;
            clean(&node);
            clean(&node);
            check(node.next == NULL, "%s leaves an empty list alone", dlls[i]);
        }
    }
    else if (!strcmp(argv[1], "secerr") && argc >= 4)
    {
        void (__cdecl *handler)(int, void *) = (void *)GetProcAddress(LoadLibraryA(argv[2]), "__security_error_handler");

        if (!handler) { printf("FAIL no export\nRESULT: FAIL\n"); return 1; }
        fflush(stdout);
        handler(atoi(argv[3]), NULL);
        printf("RETURNED\nRESULT: FAIL\n");
        return 0;
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
