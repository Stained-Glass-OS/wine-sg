/* Probe for patches/sg/4533: _Xoverflow_error throws a std::overflow_error
 * (msvcp100 and later; checked in a child process through a vectored
 * handler: catchable types overflow_error, runtime_error, exception and the
 * message), _Future_error_map/_Get_future_error_what (msvcp110, msvcp120)
 * and std::get_new_handler. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static const char *prefix = "";
#define check(c, ...) do { if (c) printf("%sok   ", prefix); else { printf("%sFAIL ", prefix); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef _WIN64
#define N(a64, a32) a64
#else
#define N(a64, a32) a32
#endif

struct type_info { const void *vtable; char *name; char mangled[64]; };
#ifdef _WIN64
typedef unsigned int ref;
#define REF(base, r) ((const void *)((base) + (r)))
#else
typedef const void *ref;
#define REF(base, r) (r)
#endif
struct catchable_type { unsigned int flags; ref type; int offsets[3]; unsigned int size; ref copy_ctor; };
struct catchable_array { unsigned int count; ref types[1]; };
struct throw_info { unsigned int flags; ref destructor; ref handler; ref array; };

static LONG WINAPI handler(EXCEPTION_POINTERS *ep)
{
    const EXCEPTION_RECORD *rec = ep->ExceptionRecord;
    const struct throw_info *ti;
    const struct catchable_array *array;
    const char *names[3] = {".?AVoverflow_error@std@@", ".?AVruntime_error@std@@", ".?AVexception@std@@"};
    const char *what;
    ULONG_PTR base;
    unsigned int i;

    if (rec->ExceptionCode != 0xe06d7363) return EXCEPTION_CONTINUE_SEARCH;
    base = rec->NumberParameters > 3 ? rec->ExceptionInformation[3] : 0;
    ti = (const struct throw_info *)rec->ExceptionInformation[2];
    array = REF(base, ti->array);
    check(array->count == 3, "%u catchable types", array->count);
    for (i = 0; i < 3 && i < array->count; i++)
    {
        const struct catchable_type *ct = REF(base, array->types[i]);
        const struct type_info *type = REF(base, ct->type);
        check(!strcmp(type->mangled, names[i]), "catchable type %u: %s", i, type->mangled);
    }
    what = *(const char **)(rec->ExceptionInformation[1] + sizeof(void *));
    check(what && !strcmp(what, "value too large"), "what() \"%s\"", what ? what : "(null)");
    fflush(stdout);
    ExitProcess(failures ? 1 : 0x55);
}

static void __cdecl nh(void) {}

int main(int argc, char **argv)
{
    HMODULE mod;
    int v110 = 0, v120 = 0;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    if (argc > 2)
    {
        void (__cdecl *xovf)(const char *) = (void *)GetProcAddress(mod, N("?_Xoverflow_error@std@@YAXPEBD@Z", "?_Xoverflow_error@std@@YAXPBD@Z"));
        prefix = "child: ";
        AddVectoredExceptionHandler(1, handler);
        if (xovf) xovf("value too large");
        printf("child: FAIL nothing thrown\n");
        return 1;
    }
    v110 = !strcmp(argv[1], "msvcp110");
    v120 = !strcmp(argv[1], "msvcp120");

    {
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 64], self[MAX_PATH];
        DWORD code = 0;

        check(GetProcAddress(mod, N("?_Xoverflow_error@std@@YAXPEBD@Z", "?_Xoverflow_error@std@@YAXPBD@Z")) != NULL,
              "_Xoverflow_error exported");
        GetModuleFileNameA(NULL, self, sizeof(self));
        sprintf(cmd, "\"%s\" %s throw", self, argv[1]);
        fflush(stdout);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        check(code == 0x55, "_Xoverflow_error throws an overflow_error (child exit %#lx)", code);
    }

    if (v110 || v120)
    {
        const char *(__cdecl *map)(int) = (void *)GetProcAddress(mod, N("?_Future_error_map@std@@YAPEBDH@Z", "?_Future_error_map@std@@YAPBDH@Z"));
        static const char *msgs[] = {"broken promise", "future already retrieved", "promise already satisfied", "no state"};
        int i, first = v110 ? 0 : 1;

        check(map != NULL, "_Future_error_map exported");
        if (map)
        {
            for (i = 0; i < 4; i++)
            {
                const char *m = map(first + i);
                check(m && !strcmp(m, msgs[i]), "_Future_error_map(%d) = %s", first + i, m ? m : "NULL");
            }
            check(!map(first + 4) && !map(first - 1), "unknown codes give NULL");
        }
        if (v110)
        {
            const char *(__cdecl *what)(int) = (void *)GetProcAddress(mod, N("?_Get_future_error_what@std@@YAPEBDH@Z", "?_Get_future_error_what@std@@YAPBDH@Z"));
            check(what && what(3) && !strcmp(what(3), "no state"), "_Get_future_error_what");
        }
    }

    {
        void *(__cdecl *get)(void) = (void *)GetProcAddress(mod, "?get_new_handler@std@@YAP6AXXZXZ");
        void *(__cdecl *set)(void *) = (void *)GetProcAddress(mod, "?set_new_handler@std@@YAP6AXXZP6AXXZ@Z");

        if (get || v110 || v120 || !strcmp(argv[1], "msvcp_win"))
        {
            check(get && set, "get_new_handler exported");
            if (get && set)
            {
                set(nh);
                check(get() == (void *)nh, "get_new_handler returns the handler");
                set(NULL);
                check(get() == NULL, "and NULL after reset");
            }
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
