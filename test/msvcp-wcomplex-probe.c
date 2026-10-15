/* Smoke probe for patches/sg/4548 (fast mode): operator<< and operator>>
 * of complex<float/double/long double> on char, wchar_t ("_W") and
 * unsigned short ("G") streams, every such export of the DLL named on the
 * command line (msvcp60: char only; msvcp70 .. msvcp90). Prints into a
 * basic_ostringstream, reads back from a basic_istringstream made from
 * text put() into an ostringstream. The exports are found by the parts of
 * their mangled names.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef _WIN64
#define THISCALL
#define X64 1
#else
#define THISCALL __attribute__((thiscall))
#define X64 0
#endif

static int failures, checked;
#define check(c, ...) do { checked++; if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HMODULE mod;
static const IMAGE_EXPORT_DIRECTORY *exp_dir;

/* the n-th export whose name starts with pre, contains all of mid (NUL separated, may be "") and ends with suf */
static const char *find_export(const char *pre, const char *mid, const char *suf, unsigned int n)
{
    const DWORD *names = (const DWORD *)((char *)mod + exp_dir->AddressOfNames);
    DWORD i;

    for (i = 0; i < exp_dir->NumberOfNames; i++)
    {
        const char *name = (char *)mod + names[i], *m;
        size_t len = strlen(name);
        int ok = !strncmp(name, pre, strlen(pre)) && len >= strlen(suf) && !strcmp(name + len - strlen(suf), suf);

        for (m = mid; ok && *m; m += strlen(m) + 1) if (!strstr(name, m)) ok = 0;
        if (ok && !n--) return name;
    }
    return NULL;
}

static char *ios_of(void *stream)
{
    const int *vbtable = *(const int **)stream;
    return (char *)stream + vbtable[1];
}

typedef void *(THISCALL *os_ctor_t)(void *, int, int);
typedef void *(THISCALL *is_ctor_t)(void *, void *, int, int);
typedef void *(THISCALL *str_t)(void *, void *);
typedef void *(THISCALL *put_t)(void *, int);
typedef const void *(THISCALL *cstr_t)(void *);
typedef int (THISCALL *rdstate_t)(void *);
typedef void *(__cdecl *op_t)(void *, void *);

struct funcs { os_ctor_t os_ctor; is_ctor_t is_ctor; str_t str; put_t put; cstr_t c_str; rdstate_t rdstate; int narrow; };

static int get_funcs(const char *t, struct funcs *f)
{
    char pre[256], mid[256] = {0}, suf[64];
    const char *n;

#define FIND(var, p, m, s) do { n = find_export(p, m, s, 0); if (!n) { printf("FAIL %s: no %s...%s\n", t, p, s); failures++; return 0; } \
        var = (void *)GetProcAddress(mod, n); } while (0)
    sprintf(pre, "??0?$basic_ostringstream@%sU?$char_traits@%s@std@@V?$allocator@%s@", t, t, t);
    FIND(f->os_ctor, pre, "", X64 ? "QEAA@H@Z" : "QAE@H@Z");
    sprintf(pre, "??0?$basic_istringstream@%sU?$char_traits@%s@std@@V?$allocator@%s@", t, t, t);
    strcpy(mid, X64 ? "AEBV?$basic_string@" : "ABV?$basic_string@"); mid[strlen(mid) + 1] = 0;
    FIND(f->is_ctor, pre, mid, "@H@Z");
    sprintf(pre, "?str@?$basic_ostringstream@%sU?$char_traits@%s@std@@", t, t);
    FIND(f->str, pre, "", "XZ");
    sprintf(pre, "?put@?$basic_ostream@%sU?$char_traits@%s@std@@@std@@", t, t);
    sprintf(suf, "@%s@Z", t);
    FIND(f->put, pre, "", suf);
    sprintf(pre, "?c_str@?$basic_string@%sU?$char_traits@%s@std@@", t, t);
    FIND(f->c_str, pre, "", "XZ");
    FIND(f->rdstate, "?rdstate@ios_base@std@@", "", "XZ");
    return 1;
}

/* a new ostringstream holding text */
static void *new_ostream(struct funcs *f, const WCHAR *text)
{
    void *os = calloc(1, 1024);

    f->os_ctor(os, 2 /* out */, 1);
    for (; text && *text; text++) f->put(os, *text);
    return os;
}

static const WCHAR *contents(struct funcs *f, void *os)
{
    void *s = calloc(1, 256);
    WCHAR *w;

    f->str(os, s);
    if (!f->narrow) return f->c_str(s);
    w = calloc(256, sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, f->c_str(s), -1, w, 256);
    return w;
}

int main(int argc, char **argv)
{
    static const char *const types[] = {"D", "_W", "G"};
    static const struct { const WCHAR *in; int fail; double re, im; } reads[] =
    {
        {L"(1.5,-2)", 0, 1.5, -2}, {L" ( 4 , 5 ) ", 0, 4, 5}, {L"(3)", 0, 3, 0}, {L"7", 0, 7, 0},
        {L"-0.25", 0, -0.25, 0}, {L"(1;2)", 1}, {L"(1,2", 1}, {L"x", 1},
    };
    unsigned int t, i, k, total = 0;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    exp_dir = (const IMAGE_EXPORT_DIRECTORY *)((char *)mod +
        ((IMAGE_NT_HEADERS *)((char *)mod + ((IMAGE_DOS_HEADER *)mod)->e_lfanew))->OptionalHeader.DataDirectory[0].VirtualAddress);

    for (t = 0; t < ARRAYSIZE(types); t++)
    {
        static const char *const kinds[] = {"M", "N", "O"};
        char mid[64];
        struct funcs f;
        const char *name;

        if (t && !strcmp(argv[1], "msvcp60")) break;  /* Wine's msvcp60 has no wide streams */
        memset(mid, 0, sizeof(mid));  /* a list of strings ends with an empty one */
        sprintf(mid, "char_traits@%s@", types[t]);
        if (!find_export("??", mid, "Z", 0)) continue;  /* no such streams here */
        if (!get_funcs(types[t], &f)) continue;
        f.narrow = !t;

        for (k = 0; k < 3; k++)
        {
            for (i = 0; (name = find_export("??", "", "Z", i)); i++)
            {
                char cx[32], tr[32];
                int print = !strncmp(name, "??$?6", 5) || !strncmp(name, "??6", 3);
                int read = !strncmp(name, "??$?5", 5) || !strncmp(name, "??5", 3);
                op_t op;

                sprintf(cx, "?$complex@%s@", kinds[k]);
                sprintf(tr, "char_traits@%s@", types[t]);
                if (!(print || read) || !strstr(name, cx) || !strstr(name, tr)) continue;
                op = (void *)GetProcAddress(mod, name);
                total++;
                if (print)
                {
                    void *os = new_ostream(&f, L"<");
                    float cf[2] = {1.5f, -2};
                    double cd[2] = {0.25, 1e10};
                    const WCHAR *s;

                    check(op(os, k ? (void *)cd : (void *)cf) == os, "%s: returns the stream", name);
                    f.put(os, L'>');
                    s = contents(&f, os);
                    check(!wcscmp(s, k ? L"<(0.25,1e+010)>" : L"<(1.5,-2)>") || !wcscmp(s, k ? L"<(0.25,1e+10)>" : L"<(1.5,-2)>"),
                          "%s writes \"%ls\"", name, s);
                }
                if (read)
                {
                    unsigned int j;

                    for (j = 0; j < ARRAYSIZE(reads); j++)
                    {
                        void *os = new_ostream(&f, reads[j].in), *is = calloc(1, 1024), *s = calloc(1, 256);
                        float cf[2] = {99, 99};
                        double cd[2] = {99, 99};
                        int state;

                        f.str(os, s);
                        f.is_ctor(is, s, 1 /* in */, 1);
                        check(op(is, k ? (void *)cd : (void *)cf) == is, "%s: returns the stream", name);
                        state = f.rdstate(ios_of(is));
                        if (reads[j].fail) check(state & 2, "%s \"%ls\": failbit (%#x)", name, reads[j].in, state);
                        else if (k) check(!(state & 2) && cd[0] == reads[j].re && cd[1] == reads[j].im,
                                          "%s \"%ls\": %g %g (%#x)", name, reads[j].in, cd[0], cd[1], state);
                        else check(!(state & 2) && cf[0] == (float)reads[j].re && cf[1] == (float)reads[j].im,
                                   "%s \"%ls\": %g %g (%#x)", name, reads[j].in, cf[0], cf[1], state);
                    }
                }
            }
        }
    }

    check(total > 0, "%u complex stream operators tested", total);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
