/* Smoke probe for patches/sg/4569 (fast mode): msvcp60's
 * locale::operator()(string, string), whether the first string collates
 * before the second in the locale (the classic one here), against a
 * table. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define STR "?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@"

int main(void)
{
    static const struct { const char *a, *b; int before; } tests[] =
    {
        {"a", "b", 1}, {"b", "a", 0}, {"abc", "abc", 0}, {"", "a", 1}, {"a", "", 0}, {"abc", "abd", 1}, {"abcd", "abc", 0},
        {"apple", "banana", 1}, {"zebra", "apple", 0},
        /* case is left out: Wine's classic collate is not byte order (escalate-5.md) */
    };
    HMODULE mod = LoadLibraryA("msvcp60.dll");
    void *(THISCALL *str_ctor)(void *, const char *, void *);
    void *(__cdecl *classic)(void);
    int (THISCALL *call)(void *, void *, void *);
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    str_ctor = (void *)GetProcAddress(mod, N("??0" STR "std@@QEAA@PEBDAEBV?$allocator@D@1@@Z", "??0" STR "std@@QAE@PBDABV?$allocator@D@1@@Z"));
    classic = (void *)GetProcAddress(mod, N("?classic@locale@std@@SAAEBV12@XZ", "?classic@locale@std@@SAABV12@XZ"));
    call = (void *)GetProcAddress(mod, N("??Rlocale@std@@QEBA_NAEBV" STR "1@0@Z", "??Rlocale@std@@QBE_NABV" STR "1@0@Z"));
    check(str_ctor && classic && call, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        static char a[64], b[64];
        char alloc = 0;
        int r;

        str_ctor(a, tests[i].a, &alloc);
        str_ctor(b, tests[i].b, &alloc);
        r = call(classic(), a, b) & 0xff;
        check(r == tests[i].before, "locale()(\"%s\", \"%s\") = %d", tests[i].a, tests[i].b, r);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
