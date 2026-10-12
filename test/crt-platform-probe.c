/* Probe for patches/sg/4501:
 *   probe.exe info DLL        __GetPlatformExceptionInfo of DLL (ucrtbase, vcruntime140, msvcr120)
 *                             against C++ exception records made current through
 *                             ucrtbase's __current_exception
 *   probe.exe unobserved DLL  Concurrency::details::_ReportUnobservedException must not return
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define CXX_EXCEPTION 0xe06d7363
#define CXX_MAGIC_VC8 0x19930522

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

/* type_info, catchable type, catchable type array and throw info as the
 * compiler lays them out; on 64-bit the references are offsets from a base */
struct type_info { const void *vtable; char *name; char mangled[32]; };
#ifdef _WIN64
typedef unsigned int ref;
#else
typedef const void *ref;
#endif
struct catchable_type { unsigned int flags; ref type; int offsets[3]; unsigned int size; ref copy_ctor; };
struct catchable_array { unsigned int count; ref types[2]; };
struct throw_info { unsigned int flags; ref destructor; ref handler; ref array; };

static struct
{
    struct type_info ti[2];
    struct catchable_type ct[2];
    struct catchable_array array;
    struct throw_info info;
} data;

static ref make_ref(const void *p)
{
#ifdef _WIN64
    return (unsigned int)((const char *)p - (const char *)&data);
#else
    return p;
#endif
}

static void build(const char *first, const char *second, unsigned int flags)
{
    memset(&data, 0, sizeof(data));
    strcpy(data.ti[0].mangled, first);
    data.ct[0].type = make_ref(&data.ti[0]);
    data.array.types[data.array.count++] = make_ref(&data.ct[0]);
    if (second)
    {
        strcpy(data.ti[1].mangled, second);
        data.ct[1].type = make_ref(&data.ti[1]);
        data.array.types[data.array.count++] = make_ref(&data.ct[1]);
    }
    data.info.flags = flags;
    data.info.array = make_ref(&data.array);
}

static void make_record(EXCEPTION_RECORD *rec, DWORD code)
{
    static int object;

    memset(rec, 0, sizeof(*rec));
    rec->ExceptionCode = code;
    rec->ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    rec->NumberParameters = sizeof(void *) == 8 ? 4 : 3;
    rec->ExceptionInformation[0] = CXX_MAGIC_VC8;
    rec->ExceptionInformation[1] = (ULONG_PTR)&object;
    rec->ExceptionInformation[2] = (ULONG_PTR)&data.info;
    if (sizeof(void *) == 8) rec->ExceptionInformation[3] = (ULONG_PTR)&data;
}

int main(int argc, char **argv)
{
    void *(__cdecl *get_info)(int *);
    void **(__cdecl *current_exception)(void);
    EXCEPTION_RECORD rec;
    HMODULE mod;
    unsigned int i;
    int bad;
    void *ret;

    if (argc < 3 || !(mod = LoadLibraryA(argv[2])))
    {
        printf("FAIL usage or cannot load\nRESULT: FAIL\n");
        return 1;
    }

    if (!strcmp(argv[1], "unobserved"))
    {
        void (__cdecl *report)(void) = (void *)GetProcAddress(mod, "?_ReportUnobservedException@details@Concurrency@@YAXXZ");

        if (!report) { printf("FAIL no export\nRESULT: FAIL\n"); return 1; }
        fflush(stdout);
        report();
        printf("RETURNED\nRESULT: FAIL\n");
        return 0;
    }

    get_info = (void *)GetProcAddress(mod, "__GetPlatformExceptionInfo");
    current_exception = (void *)GetProcAddress(LoadLibraryA("ucrtbase.dll"), "__current_exception");
    check(get_info && current_exception, "exports");
    if (!get_info || !current_exception) { printf("RESULT: FAIL\n"); return 1; }

    bad = 7;
    ret = get_info(&bad);
    check(!ret && bad == 0, "no exception: %p, %d", ret, bad);

    if (strcmp(argv[2], "msvcr120"))  /* msvcr120 keeps its own current exception */
    {
        static const struct { const char *first, *second; unsigned int flags; DWORD code; int bad; } tests[] =
        {
            {".?AVbad_alloc@std@@", ".?AVexception@std@@", 0, CXX_EXCEPTION, 1},
            {".?AVbad_array_new_length@std@@", ".?AVbad_alloc@std@@", 0, CXX_EXCEPTION, 1},
            {".?AVruntime_error@std@@", ".?AVexception@std@@", 0, CXX_EXCEPTION, 0},
            {".H", NULL, 0, CXX_EXCEPTION, 0},
            {".?AVbad_alloc@std@@", NULL, 0, 0xc0000005, 0},
            {".?AVException@Platform@@", NULL, 0x10, CXX_EXCEPTION, 0},
        };
        void **current = current_exception(), *saved = *current;

        for (i = 0; i < ARRAYSIZE(tests); i++)
        {
            build(tests[i].first, tests[i].second, tests[i].flags);
            make_record(&rec, tests[i].code);
            *current = &rec;
            bad = 7;
            ret = get_info(&bad);
            check(!ret && bad == tests[i].bad, "%s%s (code %#lx): %p, is_bad_alloc %d", tests[i].first,
                  tests[i].flags ? " WinRT" : "", tests[i].code, ret, bad);
        }
        *current = NULL;
        ret = get_info(NULL);
        check(!ret, "NULL flag pointer");
        *current = saved;
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
