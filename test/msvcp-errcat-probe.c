/* Probe for patches/sg/4507: the error_category virtuals default_error_condition
 * and both equivalent() of the msvcp DLL named on the command line.
 *   probe.exe exported DLL   categories from generic_category()/iostream_category()/
 *                            system_category() (msvcp100)
 *   probe.exe generic DLL    the category of the system_error _Throw_C_error throws
 *                            (msvcp110 .. msvcp140, msvcp_win)
 *   probe.exe system DLL     the category of the system_error _XGetLastError throws
 *                            (msvcp140, msvcp_win): Windows errors map to errno */
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

struct cond { int value; const void *category; };
typedef const void *(__cdecl *fn_cat)(void);
typedef struct cond *(THISCALL *fn_default)(const void *, struct cond *, int);
typedef unsigned char (THISCALL *fn_equiv)(const void *, int, const struct cond *);
typedef unsigned char (THISCALL *fn_equiv_code)(const void *, const struct cond *, int);

/* system_error: exception (vtable, name, do_free), then the error_code */
struct system_error { const void *vtable; char *name; int do_free; struct cond code; };

static int failures;
static const char *mode;
static const int alien_category; /* any other category object */
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void *slot(const void *obj, int i) { return (*(void *const *const *)obj)[i]; }

static void test_plain(const void *cat, const char *name)
{
    fn_default def = slot(cat, 3);
    fn_equiv equiv = slot(cat, 4);
    fn_equiv_code equiv_code = slot(cat, 5);
    struct cond c;

    memset(&c, 0xcc, sizeof(c));
    check(def(cat, &c, 7) == &c && c.value == 7 && c.category == cat, "%s default_error_condition(7)", name);
    c.value = 7; c.category = cat;
    check(equiv(cat, 7, &c), "%s equivalent(7, own condition 7)", name);
    c.value = 8;
    check(!equiv(cat, 7, &c), "%s not equivalent(7, own condition 8)", name);
    c.value = 7; c.category = &alien_category;
    check(!equiv(cat, 7, &c), "%s not equivalent(7, condition 7 of another category)", name);
    c.value = 7; c.category = cat;
    check(equiv_code(cat, &c, 7), "%s equivalent(own code 7, 7)", name);
    check(!equiv_code(cat, &c, 8), "%s not equivalent(own code 7, 8)", name);
    c.category = &alien_category;
    check(!equiv_code(cat, &c, 7), "%s not equivalent(code 7 of another category, 7)", name);
}

static void test_system_mapped(const void *system)
{
    static const struct { int err; int value; int generic; } map[] =
    {
        {ERROR_FILE_NOT_FOUND, 2 /* ENOENT */, 1},
        {ERROR_ACCESS_DENIED, 13 /* EACCES */, 1},
        {ERROR_ALREADY_EXISTS, 17 /* EEXIST */, 1},
        {0x2345678, 0x2345678, 0},
    };
    fn_default def = slot(system, 3);
    fn_equiv equiv = slot(system, 4);
    struct cond c, generic13;
    const void *generic;
    unsigned int i;

    def(system, &c, ERROR_ACCESS_DENIED);
    generic = c.category;
    check(generic && generic != system, "ERROR_ACCESS_DENIED maps to another (generic) category");
    for (i = 0; i < ARRAYSIZE(map); i++)
    {
        memset(&c, 0xcc, sizeof(c));
        def(system, &c, map[i].err);
        check(c.value == map[i].value && c.category == (map[i].generic ? generic : system),
              "system default_error_condition(%#x) = %d, %s", map[i].err, c.value,
              c.category == generic ? "generic" : c.category == system ? "system" : "?");
    }
    generic13.value = 13; generic13.category = generic;
    check(equiv(system, ERROR_ACCESS_DENIED, &generic13), "ERROR_ACCESS_DENIED is errc::permission_denied");
    generic13.value = 2;
    check(!equiv(system, ERROR_ACCESS_DENIED, &generic13), "ERROR_ACCESS_DENIED is not errc::no_such_file_or_directory");
}

static LONG WINAPI handler(EXCEPTION_POINTERS *ep)
{
    const struct system_error *se;

    if (ep->ExceptionRecord->ExceptionCode != 0xe06d7363) return EXCEPTION_CONTINUE_SEARCH;
    se = (const struct system_error *)ep->ExceptionRecord->ExceptionInformation[1];
    check(se && se->code.category, "%s: thrown system_error with code %d", mode, se ? se->code.value : -1);
    if (se && se->code.category)
    {
        if (!strcmp(mode, "system")) test_system_mapped(se->code.category);
        else test_plain(se->code.category, "generic (thrown)");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(!!failures);
}

int main(int argc, char **argv)
{
    HMODULE mod;

    if (argc < 3 || !(mod = LoadLibraryA(argv[2]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    mode = argv[1];
    if (!strcmp(mode, "exported"))
    {
        fn_cat get_generic = (void *)GetProcAddress(mod, N("?generic_category@std@@YAAEBVerror_category@1@XZ", "?generic_category@std@@YAABVerror_category@1@XZ"));
        fn_cat get_iostream = (void *)GetProcAddress(mod, N("?iostream_category@std@@YAAEBVerror_category@1@XZ", "?iostream_category@std@@YAABVerror_category@1@XZ"));
        fn_cat get_system = (void *)GetProcAddress(mod, N("?system_category@std@@YAAEBVerror_category@1@XZ", "?system_category@std@@YAABVerror_category@1@XZ"));

        check(get_generic && get_iostream && get_system, "exports");
        if (!failures)
        {
            test_plain(get_generic(), "generic");
            test_plain(get_iostream(), "iostream");
            test_plain(get_system(), "system");
        }
    }
    else
    {
        void (__cdecl *throw_c)(int) = (void *)GetProcAddress(mod, "?_Throw_C_error@std@@YAXH@Z");
        void (__cdecl *throw_last)(void) = (void *)GetProcAddress(mod, "?_XGetLastError@std@@YAXXZ");

        AddVectoredExceptionHandler(1, handler);
        if (!strcmp(mode, "system") && throw_last)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            throw_last();
        }
        else if (!strcmp(mode, "generic") && throw_c)
            throw_c(1);
        check(0, "%s: nothing thrown", mode);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
