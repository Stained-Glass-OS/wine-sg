/* xregex-probe: msvcp140's std::_Xregex_error (patches/sg/1520) throws a
 * std::regex_error carrying the error code, as Windows does. The probe calls
 * it and inspects the C++ exception it raises (code 0xe06d7363) from a
 * vectored handler: the catchable types, what() and the stored code.
 * Prints one line: "type=<1|0> base=<1|0> code=<n> what=<text>". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int want_code;

#ifdef _WIN64
#define RVA(base, rva) ((char *)(base) + (rva))
#else
#define RVA(base, rva) ((char *)(rva))
#endif

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *rec = ep->ExceptionRecord;
    int has_type = 0, has_base = 0, code, n, i;
    const char *what;
    ULONG_PTR base = 0;
    char *obj, *info, *array;

    if (rec->ExceptionCode != 0xe06d7363) return EXCEPTION_CONTINUE_SEARCH;
    obj = (char *)rec->ExceptionInformation[1];
    info = (char *)rec->ExceptionInformation[2];
#ifdef _WIN64
    base = rec->ExceptionInformation[3];
#endif
    array = RVA(base, *(int *)(info + 12));
    n = *(int *)array;
    for (i = 0; i < n; i++)
    {
        char *ct = RVA(base, ((int *)(array + 4))[i]);
        char *td = RVA(base, *(int *)(ct + 4));
        const char *name = td + 2 * sizeof(void *);
        if (!strcmp(name, ".?AVregex_error@std@@")) has_type = 1;
        if (!strcmp(name, ".?AVruntime_error@std@@")) has_base = 1;
    }
    /* layout: vtable, what string, do_free (std::exception), then the code */
    what = *(const char **)(obj + sizeof(void *));
    code = *(int *)(obj + 3 * sizeof(void *));
    printf("type=%d base=%d code=%d what=%s\n", has_type, has_base, code, what ? what : "(null)");
    fflush(stdout);
    ExitProcess(code == want_code ? 0 : 2);
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA(argc > 2 ? argv[2] : "msvcp140.dll");
    void (__cdecl *pXregex)(int);

    want_code = argc > 1 ? atoi(argv[1]) : 4;
    if (!mod) { printf("no dll\n"); return 3; }
    pXregex = (void *)GetProcAddress(mod, "?_Xregex_error@std@@YAXW4error_type@regex_constants@1@@Z");
    if (!pXregex) { printf("no export\n"); return 4; }
    AddVectoredExceptionHandler(1, handler);
    pXregex(want_code);
    printf("returned\n");
    return 5;
}
