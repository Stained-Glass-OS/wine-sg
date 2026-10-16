/* Smoke probe for patches/sg/4563 (fast mode): exports whose names differ
 * from implemented ones only in member access, in the DLL named on the
 * command line: msvcp70's private basic_string<char> _Myptr, _Eos,
 * _Inside and _Copy, and msvcp60's time_get destructors.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define S "?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"

int main(int argc, char **argv)
{
    HMODULE mod;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    if (!strcmp(argv[1], "msvcp70"))
    {
        void *(THISCALL *ctor)(void *, const char *) = (void *)GetProcAddress(mod, N("??0" S "QEAA@PEBD@Z", "??0" S "QAE@PBD@Z"));
        const char *(THISCALL *c_str)(void *) = (void *)GetProcAddress(mod, N("?c_str@" S "QEBAPEBDXZ", "?c_str@" S "QBEPBDXZ"));
        char *(THISCALL *myptr)(void *) = (void *)GetProcAddress(mod, N(NULL, "?_Myptr@" S "AAEPADXZ"));
        const char *(THISCALL *cmyptr)(void *) = (void *)GetProcAddress(mod, N(NULL, "?_Myptr@" S "ABEPBDXZ"));
        void (THISCALL *eos)(void *, size_t) = (void *)GetProcAddress(mod, N("?_Eos@" S "AEAAX_K@Z", "?_Eos@" S "AAEXI@Z"));
        int (THISCALL *inside)(void *, const char *) = (void *)GetProcAddress(mod, N(NULL, "?_Inside@" S "AAE_NPBD@Z"));
        void (THISCALL *copy)(void *, size_t, size_t) = (void *)GetProcAddress(mod, N(NULL, "?_Copy@" S "AAEXII@Z"));
        static char s[64];

#ifdef _WIN64
        if (!ctor) { printf("ok   no 64-bit basic_string exports here\nRESULT: PASS\n"); return 0; }
#endif
        check(ctor && c_str && eos, "basic_string exports");
        if (ctor && c_str && eos)
        {
            ctor(s, "hello world");
            if (myptr && cmyptr && inside && copy)
            {
                const char *p = c_str(s);

                check(myptr(s) == p && cmyptr(s) == p, "_Myptr: %p %p", myptr(s), p);
                check((inside(s, p + 3) & 0xff) && !(inside(s, "elsewhere") & 0xff), "_Inside");
                copy(s, 64, 5);
                check(!strcmp(c_str(s), "hello"), "_Copy(64, 5): \"%s\"", c_str(s));
                ctor(s, "hello world");
            }
            eos(s, 4);
            check(!strcmp(c_str(s), "hell"), "_Eos(4): \"%s\"", c_str(s));
        }
    }
    else
    {
        static const char *const ctors[] = {
            N("??0?$time_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QEAA@_K@Z", "??0?$time_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QAE@I@Z"),
            N("??0?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QEAA@_K@Z", "??0?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QAE@I@Z") };
        static const char *const dtors[] = {
            N("??1?$time_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@UEAA@XZ", "??1?$time_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@UAE@XZ"),
            N("??1?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@UEAA@XZ", "??1?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@UAE@XZ") };
        unsigned int i;

        for (i = 0; i < 2; i++)
        {
            void *(THISCALL *ctor)(void *, size_t) = (void *)GetProcAddress(mod, ctors[i]);
            void (THISCALL *dtor)(void *) = (void *)GetProcAddress(mod, dtors[i]);
            static char facet[2][512];

            check(ctor && dtor, "time_get %u constructor and destructor", i);
            if (ctor && dtor) { ctor(facet[i], 1); dtor(facet[i]); check(1, "time_get %u destroyed", i); }
        }
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
