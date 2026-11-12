/* imagecopy (wine-sg 7750): an image whose sections are not page-aligned in the file (or that has a
 * shared writable section) loads from a copy the server makes once per file. The copy lives in the
 * prefix, not tmpfs /tmp, and when the server cannot write it (a full disk or /tmp) the image still
 * loads, mapped privately in each process.
 *   built as DLL (-DPROBE_DLL): exports probe_value() and probe_shared()
 *   imagecopy-probe.exe DLL MODE [SECONDS]
 *     MODE load:   load DLL, check its code and data, print "load=ok"
 *     MODE set N:  write N to the shared section, print it, stay SECONDS
 *     MODE get:    print the shared section's value */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PROBE_DLL
__attribute__((section(".shared"))) volatile LONG shared_counter = 7;
static const char padding[96 * 1024] = { 1 };   /* a copy bigger than a page or two */
static int private_data = 123;

__declspec(dllexport) int probe_value(void) { return private_data + padding[0] + 1000; }
__declspec(dllexport) volatile LONG *probe_shared(void) { return &shared_counter; }

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved) { return TRUE; }
#else
int wmain(int argc, WCHAR **argv)
{
    int (*value)(void);
    volatile LONG *(*shared)(void);
    HMODULE mod;

    if (argc < 3) return 2;
    if (!(mod = LoadLibraryW(argv[1]))) { printf("load failed %lu\n", GetLastError()); return 1; }
    value = (void *)GetProcAddress(mod, "probe_value");
    shared = (void *)GetProcAddress(mod, "probe_shared");
    if (!value || !shared) { printf("exports missing\n"); return 1; }
    if (!wcscmp(argv[2], L"load"))
    {
        volatile LONG *s = shared();
        LONG before = *s;
        *s = before + 1;
        printf("value=%d shared=%ld written=%ld\n", value(), before, *s);
        printf("load=%s\n", value() == 1124 && *s == before + 1 ? "ok" : "bad");
        return 0;
    }
    if (!wcscmp(argv[2], L"set") && argc >= 4)
    {
        *shared() = _wtoi(argv[3]);
        printf("set=%ld\n", *shared());
        fflush(stdout);
        Sleep(argc >= 5 ? _wtoi(argv[4]) * 1000 : 10000);
        return 0;
    }
    if (!wcscmp(argv[2], L"get")) { printf("get=%ld\n", *shared()); return 0; }
    return 2;
}
#endif
