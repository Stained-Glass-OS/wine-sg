/* Probe for patches/sg/4574: ConcRT Context::Yield and Context::Oversubscribe
 * (static and through the current context's vtable) and the context's
 * Alloc/Free slots, in the DLL on the command line (msvcr100, msvcr110,
 * msvcr120, concrt140). Oversubscribe sequences run in child processes; a
 * vectored handler reports the type of a thrown C++ exception.
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

typedef void (__cdecl *fn_void)(void);
typedef void (__cdecl *fn_bool)(unsigned char);
typedef void **(__cdecl *fn_ctx)(void);
typedef void (THISCALL *fn_vt_void)(void *);
typedef void (THISCALL *fn_vt_bool)(void *, unsigned char);
typedef void *(THISCALL *fn_vt_alloc)(void *, size_t);
typedef void (THISCALL *fn_vt_free)(void *, void *);

static HMODULE mod;
static fn_void p_yield;
static fn_bool p_oversubscribe;
static fn_ctx p_current;
static void *(__cdecl *p_alloc)(size_t);
static void (__cdecl *p_free)(void *);

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *rec = ep->ExceptionRecord;

    if (rec->ExceptionCode == 0xe06d7363 && rec->NumberParameters >= 3)
    {
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
        printf("thrown %s", name);
        fflush(stdout);
        ExitProcess(0x42);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* child: run a sequence of Oversubscribe calls, 'T'/'F' static, 't'/'f' through the vtable */
static int child(const char *seq)
{
    void **ctx = p_current();
    fn_vt_bool vt_over = (fn_vt_bool)((void **)*ctx)[9];

    AddVectoredExceptionHandler(1, handler);
    for (; *seq; seq++)
    {
        switch (*seq)
        {
        case 'T': p_oversubscribe(1); break;
        case 'F': p_oversubscribe(0); break;
        case 't': vt_over(ctx, 1); break;
        case 'f': vt_over(ctx, 0); break;
        }
        printf("%c", *seq);
        fflush(stdout);
    }
    printf(" returned");
    return 0;
}

static void run_child(const char *dll, const char *seq, DWORD *code, char *out, DWORD size)
{
    char cmd[2 * MAX_PATH], self[MAX_PATH];
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE rd, wr;
    DWORD got = 0;

    memset(out, 0, size);
    *code = 0xdead;
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = si.hStdError = wr;
    GetModuleFileNameA(NULL, self, sizeof(self));
    sprintf(cmd, "\"%s\" %s %s child", self, dll, seq);
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(wr);
        WaitForSingleObject(pi.hProcess, 30000);
        GetExitCodeProcess(pi.hProcess, code);
        ReadFile(rd, out, size - 1, &got, NULL);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    else CloseHandle(wr);
    CloseHandle(rd);
}

static volatile LONG flag;
static DWORD WINAPI setter(void *arg) { flag = 1; return 0; }

/* on one processor, a thread spinning on Yield lets a ready thread run */
static int yield_lets_other_run(void (*yield)(void))
{
    HANDLE t;
    unsigned int i;

    flag = 0;
    t = CreateThread(NULL, 0, setter, NULL, CREATE_SUSPENDED, NULL);
    ResumeThread(t);
    for (i = 0; i < 20000 && !flag; i++) yield();
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    return i < 20000;
}

static void *vt_ctx;
static void vt_yield(void) { ((fn_vt_void)((void **)*(void **)vt_ctx)[7])(vt_ctx); }
static void static_yield(void) { p_yield(); }

int main(int argc, char **argv)
{
    static const struct { const char *seq; int throws; const char *done; } seqs[] =
    {
        {"TF", 0, "TF"}, {"TTFF", 0, "TTFF"}, {"tf", 0, "tf"}, {"TtfF", 0, "TtfF"},
        {"F", 1, ""}, {"f", 1, ""}, {"TFF", 1, "TF"}, {"tff", 1, "tf"}, {"TFTFF", 1, "TFTF"},
    };
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_yield = (void *)GetProcAddress(mod, "?Yield@Context@Concurrency@@SAXXZ");
    p_oversubscribe = (void *)GetProcAddress(mod, "?Oversubscribe@Context@Concurrency@@SAX_N@Z");
    p_current = (void *)GetProcAddress(mod, N("?CurrentContext@Context@Concurrency@@SAPEAV12@XZ", "?CurrentContext@Context@Concurrency@@SAPAV12@XZ"));
    p_alloc = (void *)GetProcAddress(mod, N("?Alloc@Concurrency@@YAPEAX_K@Z", "?Alloc@Concurrency@@YAPAXI@Z"));
    p_free = (void *)GetProcAddress(mod, N("?Free@Concurrency@@YAXPEAX@Z", "?Free@Concurrency@@YAXPAX@Z"));
    if (argc > 3 && !strcmp(argv[argc - 1], "child")) return child(argv[2]);

    check(p_yield && p_oversubscribe && p_current && p_alloc && p_free, "Context and Alloc exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    vt_ctx = p_current();
    check(vt_ctx != NULL, "CurrentContext");

    for (i = 0; i < ARRAYSIZE(seqs); i++)
    {
        char out[256];
        DWORD code;
        char want[64];

        run_child(argv[1], seqs[i].seq, &code, out, sizeof(out));
        if (seqs[i].throws)
        {
            sprintf(want, "%sthrown .?AVinvalid_oversubscribe_operation@Concurrency@@", seqs[i].done);
            check(code == 0x42 && !strcmp(out, want), "Oversubscribe %s throws after \"%s\": exit %#lx \"%s\"",
                  seqs[i].seq, seqs[i].done, code, out);
        }
        else
        {
            sprintf(want, "%s returned", seqs[i].done);
            check(code == 0 && !strcmp(out, want), "Oversubscribe %s balanced: exit %#lx \"%s\"", seqs[i].seq, code, out);
        }
    }

    /* Yield on one processor */
    SetProcessAffinityMask(GetCurrentProcess(), 1);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    for (i = 0; i < 3; i++)
    {
        check(yield_lets_other_run(static_yield), "Context::Yield lets a ready thread run (round %u)", i);
        check(yield_lets_other_run(vt_yield), "context vtable Yield lets a ready thread run (round %u)", i);
    }

    /* Alloc and Free slots of the context: the Concurrency suballocator */
    {
        static const size_t sizes[] = {1, 16, 100, 1000, 70000};
        fn_vt_alloc vt_alloc = (fn_vt_alloc)((void **)*(void **)vt_ctx)[10];
        fn_vt_free vt_free = (fn_vt_free)((void **)*(void **)vt_ctx)[11];

        for (i = 0; i < ARRAYSIZE(sizes); i++)
        {
            unsigned char *p = vt_alloc(vt_ctx, sizes[i]), *q;

            check(p != NULL, "context Alloc(%Iu) %p", sizes[i], p);
            if (!p) continue;
            memset(p, 0xa5, sizes[i]);
            vt_free(vt_ctx, p);
            /* the suballocator caches small blocks: a freed block comes back from Alloc */
            q = p_alloc(sizes[i]);
            check(q != NULL, "Concurrency::Alloc(%Iu) after the context Free", sizes[i]);
            if (sizes[i] <= 1000) check(q == p, "context Free gave the block to the cache (%p %p)", p, q);
            p_free(q);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
