/* Smoke probe for patches/sg/4552 (fast mode), in the DLL named on the
 * command line: exception::_Copy_str and exception::_Tidy (msvcr100 ..
 * msvcr120), the details::_Interruption_exception constructors (message,
 * vtable, run-time type), _SetUnobservedExceptionHandler with
 * _ReportUnobservedException (msvcr110, msvcr120) and msvcr120's
 * CaptureCallstack. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#pragma GCC optimize("no-omit-frame-pointer")
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

typedef struct { const void *vtable; char *name; int do_free; } exc;
static int handled;
static void __cdecl handler(void) { handled++; }

static size_t (__cdecl *p_capture)(void **, size_t, size_t);
static size_t __attribute__((noinline)) capture_here(void **frames, size_t skip)
{
    size_t ret = p_capture(frames, skip, 4);
    __asm__ __volatile__("" ::: "memory");
    return ret;
}

int main(int argc, char **argv)
{
    void (THISCALL *copy_str)(exc *, const char *);
    void (THISCALL *tidy)(exc *);
    HMODULE mod;
    int ver;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ver = atoi(argv[1] + 5);

    copy_str = (void *)GetProcAddress(mod, N("?_Copy_str@exception@std@@AEAAXPEBD@Z", "?_Copy_str@exception@std@@AAEXPBD@Z"));
    tidy = (void *)GetProcAddress(mod, N("?_Tidy@exception@std@@AEAAXXZ", "?_Tidy@exception@std@@AAEXXZ"));
    check(copy_str && tidy, "_Copy_str, _Tidy exported");
    if (copy_str && tidy)
    {
        static const char msg[] = "a message";
        exc e = {(void *)1, NULL, 0};

        copy_str(&e, msg);
        check(e.name && e.name != msg && !strcmp(e.name, msg) && e.do_free == 1 && e.vtable == (void *)1,
              "_Copy_str copies: %s %d", e.name ? e.name : "(null)", e.do_free);
        tidy(&e);
        check(!e.name && !e.do_free, "_Tidy frees");
        e.name = (char *)msg; e.do_free = 0;
        tidy(&e);
        check(!e.name && !e.do_free, "_Tidy of a borrowed message");
        copy_str(&e, NULL);
        check(!e.name && !e.do_free, "_Copy_str(NULL)");
    }

    if (ver >= 110)
    {
        exc *(THISCALL *ctor_str)(exc *, const char *) = (void *)GetProcAddress(mod,
                N("??0_Interruption_exception@details@Concurrency@@QEAA@PEBD@Z", "??0_Interruption_exception@details@Concurrency@@QAE@PBD@Z"));
        exc *(THISCALL *ctor)(exc *) = (void *)GetProcAddress(mod,
                N("??0_Interruption_exception@details@Concurrency@@QEAA@XZ", "??0_Interruption_exception@details@Concurrency@@QAE@XZ"));
        const char *(THISCALL *what)(exc *) = (void *)GetProcAddress(mod,
                N("?what@exception@std@@UEBAPEBDXZ", "?what@exception@std@@UBEPBDXZ"));
        void *(__cdecl *rttypeid)(void *) = (void *)GetProcAddress(mod, "__RTtypeid");
        void (__cdecl *set_handler)(void (__cdecl *)(void)) = (void *)GetProcAddress(mod,
                "?_SetUnobservedExceptionHandler@details@Concurrency@@YAXP6AXXZ@Z");
        void (__cdecl *report)(void) = (void *)GetProcAddress(mod, "?_ReportUnobservedException@details@Concurrency@@YAXXZ");
        exc a, b;

        check(ctor_str && ctor && what && rttypeid && set_handler && report, "_Interruption_exception, handler exported");
        if (ctor_str && ctor && what && rttypeid)
        {
            const char *raw;

            check(ctor_str(&a, "stop") == &a && !strcmp(what(&a), "stop") && a.do_free, "_Interruption_exception(\"stop\"): %s", what(&a));
            check(ctor(&b) == &b && b.vtable == a.vtable && !b.name, "_Interruption_exception()");
            raw = (const char *)rttypeid(&a) + 2 * sizeof(void *);
            check(!strcmp(raw, ".?AV_Interruption_exception@details@Concurrency@@"), "type %s", raw);
        }
        if (set_handler && report)
        {
            set_handler(handler);
            report();
            report();
            check(handled == 2, "_ReportUnobservedException calls the handler: %d", handled);
            set_handler(NULL);
        }
    }

    if (ver >= 120)
    {
        void *frames[8] = {0};
        size_t n;

        p_capture = (void *)GetProcAddress(mod, N("?CaptureCallstack@platform@details@Concurrency@@YA_KPEAPEAX_K1@Z",
                                                  "?CaptureCallstack@platform@details@Concurrency@@YAIPAPAXII@Z"));
        check(p_capture != NULL, "CaptureCallstack exported");
        if (p_capture)
        {
            n = capture_here(frames, 0);
            check(n >= 2 && (char *)frames[0] > (char *)capture_here && (char *)frames[0] < (char *)capture_here + 0x80 &&
                  (char *)frames[1] > (char *)main && (char *)frames[1] < (char *)main + 0x2000,
                  "CaptureCallstack(0): %u frames, %p %p", (unsigned)n, frames[0], frames[1]);
            memset(frames, 0, sizeof(frames));
            n = capture_here(frames, 1);
            check(n >= 1 && (char *)frames[0] > (char *)main && (char *)frames[0] < (char *)main + 0x2000 && !frames[4],
                  "CaptureCallstack(1): %u frames, %p", (unsigned)n, frames[0]);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
