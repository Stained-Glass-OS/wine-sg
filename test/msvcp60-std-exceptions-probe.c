/* Smoke probe for patches/sg/4568 (fast mode): msvcp60's std::bad_typeid,
 * std::__non_rtti_object and std::bad_exception (constructor from a
 * message, default constructor closure where exported, copy, assignment,
 * destructor, exported vtable, run-time type name) and
 * ios_base::exceptions(short). Table driven; lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
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

int main(void)
{
    static const struct { const char *cls, *def; } classes[] =
    {
        {"bad_typeid", "bad typeid"}, {"__non_rtti_object", NULL}, {"bad_exception", "bad exception"},
    };
    HMODULE mod = LoadLibraryA("msvcp60.dll"), crt = LoadLibraryA("msvcrt.dll");
    void *(__cdecl *rttypeid)(void *) = (void *)GetProcAddress(crt, "__RTtypeid");
    const char *(THISCALL *what)(void *);
    unsigned int i;
    char name[256];

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    what = (void *)GetProcAddress(crt, N("?what@exception@@UEBAPEBDXZ", "?what@exception@@UBEPBDXZ"));  /* the same layout */
    check(what && rttypeid, "exception::what, __RTtypeid");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(classes); i++)
    {
        void *(THISCALL *ctor)(void *, const char *), *(THISCALL *copy)(void *, void *), *(THISCALL *assign)(void *, void *);
        void *(THISCALL *def)(void *);
        void (THISCALL *dtor)(void *);
        void *vtable;
        static char a[64], b[64], c[64];
        const char *raw;

        sprintf(name, N("??0%s@std@@QEAA@PEBD@Z", "??0%s@std@@QAE@PBD@Z"), classes[i].cls); ctor = (void *)GetProcAddress(mod, name);
        sprintf(name, N("??0%s@std@@QEAA@AEBV01@@Z", "??0%s@std@@QAE@ABV01@@Z"), classes[i].cls); copy = (void *)GetProcAddress(mod, name);
        sprintf(name, N("??4%s@std@@QEAAAEAV01@AEBV01@@Z", "??4%s@std@@QAEAAV01@ABV01@@Z"), classes[i].cls); assign = (void *)GetProcAddress(mod, name);
        sprintf(name, N("??1%s@std@@UEAA@XZ", "??1%s@std@@UAE@XZ"), classes[i].cls); dtor = (void *)GetProcAddress(mod, name);
        sprintf(name, N("??_F%s@std@@QEAAXXZ", "??_F%s@std@@QAEXXZ"), classes[i].cls); def = (void *)GetProcAddress(mod, name);
        sprintf(name, "??_7%s@std@@6B@", classes[i].cls); vtable = GetProcAddress(mod, name);
        check(ctor && copy && assign && dtor && vtable && (!classes[i].def || def), "%s exports", classes[i].cls);
        if (!(ctor && copy && assign && dtor && vtable)) continue;

        check(ctor(a, "a message") == a && !strcmp(what(a), "a message") && *(void **)a == vtable, "%s(\"a message\")", classes[i].cls);
        raw = (const char *)rttypeid(a) + 2 * sizeof(void *);
        sprintf(name, ".?AV%s@std@@", classes[i].cls);
        check(!strcmp(raw, name), "type %s", raw);
        check(copy(b, a) == b && !strcmp(what(b), "a message") && *(void **)b == vtable, "copy");
        ctor(c, "other");
        check(assign(c, a) == c && !strcmp(what(c), "a message"), "assignment");
        if (def)
        {
            static char d[64];
            def(d);
            check(!strcmp(what(d), classes[i].def) && *(void **)d == vtable, "default: \"%s\"", what(d));
            dtor(d);
        }
        dtor(a); dtor(b); dtor(c);
    }

    {
        void (THISCALL *exc_s)(void *, short) = (void *)GetProcAddress(mod, N("?exceptions@ios_base@std@@QEAAXF@Z", "?exceptions@ios_base@std@@QAEXF@Z"));
        int (THISCALL *exc_get)(void *) = (void *)GetProcAddress(mod, N("?exceptions@ios_base@std@@QEBAHXZ", "?exceptions@ios_base@std@@QBEHXZ"));
        void *(THISCALL *ss_ctor)(void *, int, int) = (void *)GetProcAddress(mod,
                N("??0?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@H@Z", "??0?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@H@Z"));
        static char ss[1024];

        check(exc_s && exc_get && ss_ctor, "exceptions(short) exports");
        if (exc_s && exc_get && ss_ctor)
        {
            char *ios;

            ss_ctor(ss, 3, 1);
            ios = ss + (*(int **)ss)[1];
            exc_s(ios, 4 /* badbit; the stream is good, nothing thrown */);
            check(exc_get(ios) == 4, "exceptions(short 4): %#x", exc_get(ios));
            exc_s(ios, 0);
            check(exc_get(ios) == 0, "exceptions(short 0): %#x", exc_get(ios));
        }
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
