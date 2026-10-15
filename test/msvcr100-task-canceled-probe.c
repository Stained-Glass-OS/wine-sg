/* Smoke probe for patches/sg/4572 (fast mode): msvcr100's
 * details::task_canceled constructors (message, vtable, run-time type
 * name in the details namespace). Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
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

int main(void)
{
    HMODULE mod = LoadLibraryA("msvcr100.dll");
    void *(THISCALL *ctor_str)(void *, const char *), *(THISCALL *ctor)(void *);
    const char *(THISCALL *what)(void *);
    void *(__cdecl *rttypeid)(void *);
    static char a[64], b[64];

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ctor_str = (void *)GetProcAddress(mod, N("??0task_canceled@details@Concurrency@@QEAA@PEBD@Z", "??0task_canceled@details@Concurrency@@QAE@PBD@Z"));
    ctor = (void *)GetProcAddress(mod, N("??0task_canceled@details@Concurrency@@QEAA@XZ", "??0task_canceled@details@Concurrency@@QAE@XZ"));
    what = (void *)GetProcAddress(mod, N("?what@exception@std@@UEBAPEBDXZ", "?what@exception@std@@UBEPBDXZ"));
    rttypeid = (void *)GetProcAddress(mod, "__RTtypeid");
    check(ctor_str && ctor && what && rttypeid, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    check(ctor_str(a, "canceled") == a && !strcmp(what(a), "canceled"), "task_canceled(\"canceled\"): %s", what(a));
    check(ctor(b) == b && *(void **)b == *(void **)a, "task_canceled(): same vtable");
    check(!strcmp((const char *)rttypeid(a) + 2 * sizeof(void *), ".?AVtask_canceled@details@Concurrency@@"), "type %s",
          (const char *)rttypeid(a) + 2 * sizeof(void *));
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
