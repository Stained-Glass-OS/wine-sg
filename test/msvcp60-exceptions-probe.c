/* Smoke probe for patches/sg/4564 (fast mode): msvcp60's overflow_error,
 * underflow_error and domain_error: construction from a basic_string, the
 * message, the exported vtable, the run-time type name, copy, assignment
 * and destruction. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
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

#define STR "V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@"

int main(int argc, char **argv)
{
    static const struct { const char *cls, *base; } classes[] =
    {
        {"overflow_error", "runtime_error"}, {"underflow_error", "runtime_error"}, {"domain_error", "logic_error"},
    };
    HMODULE mod = LoadLibraryA("msvcp60.dll"), crt = LoadLibraryA("msvcrt.dll");
    void *(THISCALL *str_ctor)(void *, const char *, void *);
    void *(__cdecl *rttypeid)(void *) = (void *)GetProcAddress(crt, "__RTtypeid");
    unsigned int i;
    char name[256];

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    str_ctor = (void *)GetProcAddress(mod, N("??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@PEBDAEBV?$allocator@D@1@@Z",
                                             "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBDABV?$allocator@D@1@@Z"));
    check(str_ctor && rttypeid, "basic_string, __RTtypeid");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(classes); i++)
    {
        void *(THISCALL *ctor)(void *, void *), *(THISCALL *copy)(void *, void *), *(THISCALL *assign)(void *, void *);
        void (THISCALL *dtor)(void *);
        const char *(THISCALL *what)(void *);
        void *vtable;
        static char s[64], a[64], b[64], c[64];
        char alloc = 0;
        const char *raw;

        sprintf(name, N("??0%s@std@@QEAA@AEB" STR "@Z", "??0%s@std@@QAE@AB" STR "@Z"), classes[i].cls);
        ctor = (void *)GetProcAddress(mod, name);
        sprintf(name, N("??0%s@std@@QEAA@AEBV01@@Z", "??0%s@std@@QAE@ABV01@@Z"), classes[i].cls);
        copy = (void *)GetProcAddress(mod, name);
        sprintf(name, N("??4%s@std@@QEAAAEAV01@AEBV01@@Z", "??4%s@std@@QAEAAV01@ABV01@@Z"), classes[i].cls);
        assign = (void *)GetProcAddress(mod, name);
        sprintf(name, N("??1%s@std@@UEAA@XZ", "??1%s@std@@UAE@XZ"), classes[i].cls);
        dtor = (void *)GetProcAddress(mod, name);
        sprintf(name, N("?what@%s@std@@UEBAPEBDXZ", "?what@%s@std@@UBEPBDXZ"), classes[i].base);
        what = (void *)GetProcAddress(mod, name);
        sprintf(name, "??_7%s@std@@6B@", classes[i].cls);
        vtable = GetProcAddress(mod, name);
        check(ctor && copy && assign && dtor && what && vtable, "%s exports", classes[i].cls);
        if (!(ctor && copy && assign && dtor && what && vtable)) continue;

        str_ctor(s, classes[i].cls, &alloc);
        check(ctor(a, s) == a && !strcmp(what(a), classes[i].cls) && *(void **)a == vtable, "%s(string): \"%s\"",
              classes[i].cls, what(a));
        raw = (const char *)rttypeid(a) + 2 * sizeof(void *);
        sprintf(name, ".?AV%s@std@@", classes[i].cls);
        check(!strcmp(raw, name), "type %s", raw);
        check(copy(b, a) == b && !strcmp(what(b), classes[i].cls) && what(b) != what(a) && *(void **)b == vtable, "copy");
        str_ctor(s, "other", &alloc);
        ctor(c, s);
        check(assign(c, a) == c && !strcmp(what(c), classes[i].cls), "assignment");
        dtor(a); dtor(b); dtor(c);
        check(1, "destroyed");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
