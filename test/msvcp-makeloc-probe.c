/* Probe for patches/sg/4576: locale::_Locimp::_Makexloc/_Makewloc/_Makeushloc
 * put every facet of the categories asked for into a locale being made: the
 * messages, money_get, money_put and moneypunct<false/true> facets of char,
 * wchar_t and unsigned short (msvcp80, msvcp90) and time_get<unsigned short>
 * (msvcp80 .. msvcp140), both for a locale made from a name and for one made
 * by taking categories from another locale (msvcp100 and later inline the
 * locale constructors, so there _Makeushloc is called on a new _Locimp).
 * msvcp DLL on the command line.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define N(a64, a32) a64
#else
#define THISCALL __attribute__((thiscall))
#define N(a64, a32) a32
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define ISB(c) "V?$istreambuf_iterator@" c "U?$char_traits@" c "@std@@@std@@"
#define OSB(c) "V?$ostreambuf_iterator@" c "U?$char_traits@" c "@std@@@std@@"

/* facet: the id's symbol (class part), the RTTI name, the category bit, msvcp80/90 only */
static const struct { const char *cls; int cat; int old_only; } facets[] =
{
    {"?$messages@D@std@@", 0x20, 1}, {"?$messages@_W@std@@", 0x20, 1}, {"?$messages@G@std@@", 0x20, 1},
    {"?$money_get@D" ISB("D") "@std@@", 0x04, 1}, {"?$money_get@_W" ISB("_W") "@std@@", 0x04, 1},
    {"?$money_get@G" ISB("G") "@std@@", 0x04, 1},
    {"?$money_put@D" OSB("D") "@std@@", 0x04, 1}, {"?$money_put@_W" OSB("_W") "@std@@", 0x04, 1},
    {"?$money_put@G" OSB("G") "@std@@", 0x04, 1},
    {"?$moneypunct@D$0A@@std@@", 0x04, 1}, {"?$moneypunct@D$00@std@@", 0x04, 1},
    {"?$moneypunct@_W$0A@@std@@", 0x04, 1}, {"?$moneypunct@_W$00@std@@", 0x04, 1},
    {"?$moneypunct@G$0A@@std@@", 0x04, 1}, {"?$moneypunct@G$00@std@@", 0x04, 1},
    {"?$time_get@G" ISB("G") "@std@@", 0x10, 0},
};

static const char *crt_of(const char *dll)
{
    static const struct { const char *p, *r; } map[] =
    {
        {"msvcp80", "msvcr80"}, {"msvcp90", "msvcr90"}, {"msvcp100", "msvcr100"}, {"msvcp110", "msvcr110"},
        {"msvcp120", "msvcr120"}, {"msvcp140", "vcruntime140"},
    };
    unsigned int i;
    for (i = 0; i < ARRAYSIZE(map); i++) if (!strcmp(map[i].p, dll)) return map[i].r;
    return NULL;
}

int main(int argc, char **argv)
{
    void *(THISCALL *ctor_name)(void *, const char *, int);
    void *(THISCALL *ctor_combine)(void *, const void *, const void *, int);
    const void *(THISCALL *getfacet)(const void *, size_t);
    size_t (THISCALL *id_index)(void *);
    const void *(__cdecl *classic)(void);
    void *(__cdecl *rttypeid)(const void *);
    void *(THISCALL *ctor_default)(void *);
    HMODULE mod, crt;
    int is_old;
    unsigned int i;
    static char named[64], combined[64], mixed[64], other[64];

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    is_old = !strcmp(argv[1], "msvcp80") || !strcmp(argv[1], "msvcp90");
    crt = LoadLibraryA(crt_of(argv[1]));
    rttypeid = crt ? (void *)GetProcAddress(crt, "__RTtypeid") : NULL;
    ctor_name = (void *)GetProcAddress(mod, N("??0locale@std@@QEAA@PEBDH@Z", "??0locale@std@@QAE@PBDH@Z"));
    ctor_combine = (void *)GetProcAddress(mod, N("??0locale@std@@QEAA@AEBV01@0H@Z", "??0locale@std@@QAE@ABV01@0H@Z"));
    ctor_default = (void *)GetProcAddress(mod, N("??0locale@std@@QEAA@XZ", "??0locale@std@@QAE@XZ"));
    getfacet = (void *)GetProcAddress(mod, N("?_Getfacet@locale@std@@QEBAPEBVfacet@12@_K@Z", "?_Getfacet@locale@std@@QBEPBVfacet@12@I@Z"));
    id_index = (void *)GetProcAddress(mod, N("??Bid@locale@std@@QEAA_KXZ", "??Bid@locale@std@@QAEIXZ"));
    classic = (void *)GetProcAddress(mod, N("?classic@locale@std@@SAAEBV12@XZ", "?classic@locale@std@@SAABV12@XZ"));
    check(id_index && classic && rttypeid, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    /* _Makeushloc on a new _Locimp, from a _Locinfo and from another locale */
    {
        void *(THISCALL *locimp_ctor)(void *, unsigned char) = (void *)GetProcAddress(mod,
                N("??0_Locimp@locale@std@@AEAA@_N@Z", "??0_Locimp@locale@std@@AAE@_N@Z"));
        void *(THISCALL *locinfo_ctor)(void *, const char *) = (void *)GetProcAddress(mod,
                N("??0_Locinfo@std@@QEAA@PEBD@Z", "??0_Locinfo@std@@QAE@PBD@Z"));
        void (__cdecl *makeushloc)(const void *, int, void *, const void *) = (void *)GetProcAddress(mod,
                N("?_Makeushloc@_Locimp@locale@std@@CAXAEBV_Locinfo@3@HPEAV123@PEBV23@@Z",
                  "?_Makeushloc@_Locimp@locale@std@@CAXABV_Locinfo@3@HPAV123@PBV23@@Z"));
        void *id = GetProcAddress(mod, "?id@?$time_get@G" ISB("G") "@std@@2V0locale@2@A");
        /* _Locimp: facet (vtable, refs), facet vector, facet count, ... */
        struct locimp { void *vtable; size_t refs; void **facetvec; size_t count; } *a, *b;
        static char abuf[256], bbuf[256], info[4096];
        struct { int unused; void *ptr; } other140;  /* msvcp140's locale has an int first */
        void *other;
        size_t idx;
        const char *rtti;

        check(locimp_ctor && locinfo_ctor && makeushloc && id, "_Locimp, _Locinfo, _Makeushloc exports");
        if (locimp_ctor && locinfo_ctor && makeushloc && id)
        {
            locimp_ctor(abuf, 0); a = (void *)abuf;
            locimp_ctor(bbuf, 0); b = (void *)bbuf;
            locinfo_ctor(info, "C");
            idx = id_index(id);
            makeushloc(info, 0x10, a, NULL);
            check(idx < a->count && a->facetvec[idx], "_Makeushloc(time) adds time_get<unsigned short> (%Iu of %Iu)", idx, a->count);
            if (idx < a->count && a->facetvec[idx])
            {
                rtti = (const char *)rttypeid(a->facetvec[idx]) + 2 * sizeof(void *);
                check(!strcmp(rtti, ".?AV?$time_get@G" ISB("G") "@std@@"), "its type %s", rtti);
            }
            other = a;
            other140.unused = 0; other140.ptr = a;
            makeushloc(info, 0x10, b, strcmp(argv[1], "msvcp140") ? (void *)&other : (void *)&other140);
            check(idx < b->count && b->facetvec[idx] && b->facetvec[idx] == a->facetvec[idx],
                  "_Makeushloc(time) from another locale takes its time_get<unsigned short>");
        }
    }

    if (!is_old)
    {
        printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
        return !!failures;
    }
    check(ctor_name && ctor_combine && ctor_default && getfacet, "locale exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    /* a locale from a name: every category */
    ctor_name(named, "C", 0x3f);
    /* the classic locale with the monetary, messages and time categories of another */
    ctor_name(other, "C", 0x3f);
    ctor_combine(combined, classic(), other, 0x34);
    /* only collate and ctype from the other: no money facets come along */
    ctor_combine(mixed, classic(), other, 0x03);

    for (i = 0; i < ARRAYSIZE(facets); i++)
    {
        char sym[512], want[512];
        void *id;
        size_t idx;
        const void *f, *fo, *fc;
        const char *rtti;

        if (facets[i].old_only && !is_old) continue;
        sprintf(sym, "?id@%s2V0locale@2@A", facets[i].cls);
        id = GetProcAddress(mod, sym);
        check(id != NULL, "%s exported", sym);
        if (!id) continue;
        idx = id_index(id);

        f = getfacet(named, idx);
        sprintf(want, ".?AV%s", facets[i].cls);
        rtti = f ? (const char *)rttypeid(f) + 2 * sizeof(void *) : "(none)";
        check(f && !strcmp(rtti, want), "locale(\"C\") has %s: %p %s", want, f, rtti);

        fo = getfacet(other, idx);
        fc = getfacet(combined, idx);
        check(fc && fc == fo, "taking category %#x from another locale takes its %s (%p %p)", facets[i].cat, want, fc, fo);
        fc = getfacet(mixed, idx);
        check(fc != fo, "taking collate and ctype leaves %s (%p)", want, fc);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
