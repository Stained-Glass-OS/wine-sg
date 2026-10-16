/* Probe for patches/sg/4525: __report_gsfailure (the process ends with
 * STATUS_STACK_BUFFER_OVERRUN), the set_terminate(int)/set_unexpected(int)
 * overloads (msvcr80 .. msvcr120), and ucrtbase/vcruntime140's unexpected
 * and _is_exception_typeof. The ending cases run in a child process.
 *   probe.exe DLL     lines "ok"/"FAIL", last "RESULT: PASS|FAIL" */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define CXX_EXCEPTION 0xe06d7363

static DWORD run_child(const char *mode, const char *dll)
{
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 64], self[MAX_PATH];
    DWORD code = 0xdeadbeef;

    GetModuleFileNameA(NULL, self, sizeof(self));
    sprintf(cmd, "\"%s\" %s %s", self, dll, mode);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return code;
    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return code;
}

static void __cdecl handler(void) { ExitProcess(5); }
static void __cdecl handler2(void) { }

/* a C++ exception record as the compiler lays it out (see crt-platform-probe.c) */
struct type_info { const void *vtable; char *name; char mangled[32]; };
#ifdef _WIN64
typedef unsigned int ref;
#else
typedef const void *ref;
#endif
struct catchable_type { unsigned int flags; ref type; int offsets[3]; unsigned int size; ref copy_ctor; };
struct catchable_array { unsigned int count; ref types[2]; };
struct throw_info { unsigned int flags; ref destructor; ref handler; ref array; };
static struct { struct type_info ti[2]; struct catchable_type ct[2]; struct catchable_array array; struct throw_info info; } data;
static ref make_ref(const void *p)
{
#ifdef _WIN64
    return (unsigned int)((const char *)p - (const char *)&data);
#else
    return p;
#endif
}

int main(int argc, char **argv)
{
    HMODULE mod;
    int crt14;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    crt14 = !strcmp(argv[1], "ucrtbase") || !strcmp(argv[1], "vcruntime140");

    if (argc > 2)  /* child */
    {
        if (!strcmp(argv[2], "gs"))
        {
            void (__cdecl *p)(ULONG_PTR) = (void *)GetProcAddress(mod, "__report_gsfailure");
            p(0x1234);
            return 0x1001;  /* returned */
        }
        if (!strcmp(argv[2], "unexpected"))
        {
            void *(__cdecl *p_set)(void *) = (void *)GetProcAddress(LoadLibraryA("ucrtbase.dll"), "set_unexpected");
            void (__cdecl *p)(void) = (void *)GetProcAddress(mod, "unexpected");
            p_set(handler);
            p();
            return 0x1002;
        }
        return 0x1003;
    }

    check(GetProcAddress(mod, "__report_gsfailure") != NULL, "__report_gsfailure exported");
    check(run_child("gs", argv[1]) == STATUS_STACK_BUFFER_OVERRUN, "__report_gsfailure ends the process with %#lx",
          (DWORD)STATUS_STACK_BUFFER_OVERRUN);

    if (!crt14)
    {
        void *(__cdecl *p_term)(void *) = (void *)GetProcAddress(mod, "?set_terminate@@YAP6AXXZP6AXXZ@Z");
        void *(__cdecl *p_term0)(int) = (void *)GetProcAddress(mod, "?set_terminate@@YAP6AXXZH@Z");
        void *(__cdecl *p_unex)(void *) = (void *)GetProcAddress(mod, "?set_unexpected@@YAP6AXXZP6AXXZ@Z");
        void *(__cdecl *p_unex0)(int) = (void *)GetProcAddress(mod, "?set_unexpected@@YAP6AXXZH@Z");

        check(p_term && p_term0 && p_unex && p_unex0, "set_terminate/set_unexpected overloads exported");
        if (p_term && p_term0 && p_unex && p_unex0)
        {
            p_term(handler2);
            check(p_term0(0) == (void *)handler2 && p_term(NULL) == NULL, "set_terminate(0) resets the handler");
            p_unex(handler2);
            check(p_unex0(0) == (void *)handler2 && p_unex(NULL) == NULL, "set_unexpected(0) resets the handler");
        }
    }
    else
    {
        int (__cdecl *p_typeof)(const struct type_info *, EXCEPTION_POINTERS *) = (void *)GetProcAddress(mod, "_is_exception_typeof");
        static struct type_info other = {NULL, NULL, ".?AVlogic_error@std@@"};
        EXCEPTION_RECORD rec;
        EXCEPTION_POINTERS ep = {&rec, NULL};
        static int object;

        check(GetProcAddress(mod, "unexpected") != NULL, "unexpected exported");
        check(run_child("unexpected", argv[1]) == 5, "unexpected calls the unexpected handler");

        check(p_typeof != NULL, "_is_exception_typeof exported");
        if (p_typeof)
        {
            strcpy(data.ti[0].mangled, ".?AVruntime_error@std@@");
            strcpy(data.ti[1].mangled, ".?AVexception@std@@");
            data.ct[0].type = make_ref(&data.ti[0]);
            data.ct[1].type = make_ref(&data.ti[1]);
            data.array.count = 2;
            data.array.types[0] = make_ref(&data.ct[0]);
            data.array.types[1] = make_ref(&data.ct[1]);
            data.info.array = make_ref(&data.array);
            memset(&rec, 0, sizeof(rec));
            rec.ExceptionCode = CXX_EXCEPTION;
            rec.NumberParameters = sizeof(void *) == 8 ? 4 : 3;
            rec.ExceptionInformation[0] = 0x19930520;
            rec.ExceptionInformation[1] = (ULONG_PTR)&object;
            rec.ExceptionInformation[2] = (ULONG_PTR)&data.info;
            if (sizeof(void *) == 8) rec.ExceptionInformation[3] = (ULONG_PTR)&data;
            check(p_typeof(&data.ti[0], &ep) == 1, "the thrown type");
            {
                struct type_info same = {NULL, NULL, ".?AVexception@std@@"};
                check(p_typeof(&same, &ep) == 1, "a base type, by name");
            }
            check(p_typeof(&other, &ep) == 0, "another type");
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
