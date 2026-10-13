/* Probe for patches/sg/4575: msvcp60's exception classes' _Doraise (the
 * exported member and the vtable slot after what) throws a copy of the object
 * as its own class. A child process calls it; a vectored handler reports the
 * thrown type and the copy's message. Table driven; lines "ok"/"FAIL", last
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

#define STR "V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@"

/* ctor kind: 's' from a basic_string, 'c' from a const char *; whether
 * msvcp60 exports _Doraise; the run-time type name (msvcp60's logic_error
 * has no namespace in its type name, as its RTTI says) */
static const struct { const char *cls; char kind; int exported; const char *type; } classes[] =
{
    {"logic_error", 's', 1, ".?AVlogic_error@@"}, {"length_error", 's', 1}, {"out_of_range", 's', 1},
    {"domain_error", 's', 1}, {"runtime_error", 's', 1}, {"range_error", 's', 1}, {"overflow_error", 's', 1},
    {"underflow_error", 's', 1}, {"bad_alloc", 'c', 1}, {"bad_cast", 'c', 1}, {"bad_typeid", 'c', 1},
    {"__non_rtti_object", 'c', 0}, {"bad_exception", 'c', 1},
};

static HMODULE mod;

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *rec = ep->ExceptionRecord;

    if (rec->ExceptionCode == 0xe06d7363 && rec->NumberParameters >= 3)
    {
        void *obj = (void *)rec->ExceptionInformation[1];
        const char *(THISCALL *what)(void *) = (*(void ***)obj)[1];
#ifdef _WIN64
        char *base = (char *)rec->ExceptionInformation[3];
        const int *ti = (const int *)rec->ExceptionInformation[2];
        const int *cta = (const int *)(base + ti[3]);
        const int *ct = (const int *)(base + cta[1]);
        const char *name = base + ct[1] + 16;
#else
        const int *ti = (const int *)rec->ExceptionInformation[2];
        const int *cta = (const int *)ti[3];
        const int *ct = (const int *)cta[1];
        const char *name = (const char *)ct[1] + 8;
#endif
        printf("thrown %s %d \"%s\"", name, cta[0], what(obj));
        fflush(stdout);
        ExitProcess(0x42);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void *make(unsigned int i, void *buf, const char *msg)
{
    static char s[64];
    char name[512], alloc = 0;

    if (classes[i].kind == 's')
    {
        void *(THISCALL *str_ctor)(void *, const char *, void *) = (void *)GetProcAddress(mod,
                N("??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@PEBDAEBV?$allocator@D@1@@Z",
                  "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBDABV?$allocator@D@1@@Z"));
        void *(THISCALL *ctor)(void *, void *);

        sprintf(name, N("??0%s@std@@QEAA@AEB" STR "@Z", "??0%s@std@@QAE@AB" STR "@Z"), classes[i].cls);
        ctor = (void *)GetProcAddress(mod, name);
        if (!str_ctor || !ctor) return NULL;
        str_ctor(s, msg, &alloc);
        return ctor(buf, s);
    }
    else
    {
        void *(THISCALL *ctor)(void *, const char *);

        sprintf(name, N("??0%s@std@@QEAA@PEBD@Z", "??0%s@std@@QAE@PBD@Z"), classes[i].cls);
        ctor = (void *)GetProcAddress(mod, name);
        return ctor ? ctor(buf, msg) : NULL;
    }
}

static int child(unsigned int i, int slot)
{
    static char obj[128];
    void (THISCALL *doraise)(void *);
    char name[256];

    if (!make(i, obj, "the message")) { printf("no object"); return 1; }
    if (slot) doraise = (*(void ***)obj)[2];
    else
    {
        sprintf(name, N("?_Doraise@%s@std@@MEBAXXZ", "?_Doraise@%s@std@@MBEXXZ"), classes[i].cls);
        doraise = (void *)GetProcAddress(mod, name);
        if (!doraise) { printf("no export"); return 1; }
    }
    AddVectoredExceptionHandler(1, handler);
    doraise(obj);
    printf("returned");
    return 0;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH];
    unsigned int i, slot;

    if (!(mod = LoadLibraryA("msvcp60.dll"))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    if (argc > 3 && !strcmp(argv[argc - 1], "child")) return child(atoi(argv[argc - 3]), atoi(argv[argc - 2]));

    GetModuleFileNameA(NULL, self, sizeof(self));
    for (i = 0; i < ARRAYSIZE(classes); i++)
    {
        for (slot = !classes[i].exported; slot < 2; slot++)
        {
            char cmd[2 * MAX_PATH], out[512] = {0}, want[256];
            STARTUPINFOA si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
            HANDLE rd, wr;
            DWORD code = 0xdead, got = 0;

            CreatePipe(&rd, &wr, &sa, 0);
            SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = si.hStdError = wr;
            sprintf(cmd, "\"%s\" msvcp60 %u %u child", self, i, slot);
            if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
            {
                CloseHandle(wr);
                WaitForSingleObject(pi.hProcess, 30000);
                GetExitCodeProcess(pi.hProcess, &code);
                ReadFile(rd, out, sizeof(out) - 1, &got, NULL);
                CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            }
            else CloseHandle(wr);
            CloseHandle(rd);
            /* the thrown type comes first among the catchable types */
            if (classes[i].type) sprintf(want, "thrown %s ", classes[i].type);
            else sprintf(want, "thrown .?AV%s@std@@ ", classes[i].cls);
            check(code == 0x42 && !strncmp(out, want, strlen(want)) && strstr(out, "\"the message\""),
                  "%s %s throws itself: exit %#lx %s", classes[i].cls, slot ? "vtable _Doraise" : "_Doraise", code, out);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
