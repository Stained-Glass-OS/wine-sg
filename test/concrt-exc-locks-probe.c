/* Probe for patches/sg/4546: the Concurrency Runtime exception classes
 * (message constructors, RTTI names through __RTtypeid,
 * scheduler_worker_creation_error's HRESULT) and the _ReentrantLock and
 * _ReaderWriterLock spin locks between threads, in the DLL named on the
 * command line (msvcr100 .. msvcr120, concrt140).
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
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

static HMODULE mod;
struct exc { const void *vtable; const char *name; int do_free; LONG hr; };
struct type_info { const void *vtable; char *name; char mangled[64]; };

static void (THISCALL *p_lock)(void *), (THISCALL *p_unlock)(void *);
static unsigned char (THISCALL *p_try)(void *);
static LONG rlock[2], rw[2];
static volatile LONG phase;

static DWORD WINAPI try_other(void *arg) { return p_try(rlock) & 1; }

static void (THISCALL *p_rd)(void *), (THISCALL *p_rd_rel)(void *), (THISCALL *p_wr)(void *), (THISCALL *p_wr_rel)(void *);
static unsigned char (THISCALL *p_try_wr)(void *);
static DWORD WINAPI writer(void *arg) { p_wr(rw); phase = 1; Sleep(100); p_wr_rel(rw); return 0; }

int main(int argc, char **argv)
{
    static const char *names[] = {"bad_target", "context_self_unblock", "context_unblock_unbalanced", "default_scheduler_exists",
        "improper_scheduler_reference", "invalid_link_target", "invalid_operation", "invalid_oversubscribe_operation",
        "message_not_found", "nested_scheduler_missing_detach", "operation_timed_out", "scheduler_not_attached", "task_canceled",
        "unsupported_os"};
    const struct type_info *(__cdecl *rttypeid)(const void *);
    unsigned int i, found = 0;
    char name[128];

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    rttypeid = (void *)GetProcAddress(mod, "__RTtypeid");
    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        void *(THISCALL *ctor)(void *, const char *);
        struct exc e = {0};

        sprintf(name, N("??0%s@Concurrency@@QEAA@PEBD@Z", "??0%s@Concurrency@@QAE@PBD@Z"), names[i]);
        if (!(ctor = (void *)GetProcAddress(mod, name))) continue;
        found++;
        ctor(&e, "the message");
        check(e.vtable && e.name && !strcmp(e.name, "the message"), "%s(\"the message\")", names[i]);
        if (rttypeid)
        {
            const struct type_info *ti = rttypeid(&e);
            char want[96];
            sprintf(want, ".?AV%s@Concurrency@@", names[i]);
            check(ti && !strcmp(ti->mangled, want), "%s RTTI %s", names[i], ti ? ti->mangled : "(none)");
        }
    }
    check(found >= 10, "%u exception classes exported", found);
    {
        void *(THISCALL *ctor)(void *, const char *, LONG) = (void *)GetProcAddress(mod,
                N("??0scheduler_worker_creation_error@Concurrency@@QEAA@PEBDJ@Z", "??0scheduler_worker_creation_error@Concurrency@@QAE@PBDJ@Z"));
        LONG (THISCALL *get_hr)(const void *) = (void *)GetProcAddress(mod,
                N("?get_error_code@scheduler_resource_allocation_error@Concurrency@@QEBAJXZ", "?get_error_code@scheduler_resource_allocation_error@Concurrency@@QBEJXZ"));
        struct exc e = {0};
        /* msvcr100 has no scheduler_worker_creation_error */
        if (ctor && get_hr)
        {
            ctor(&e, "workers", E_OUTOFMEMORY);
            check(get_hr(&e) == E_OUTOFMEMORY && !strcmp(e.name, "workers"), "scheduler_worker_creation_error HRESULT %#lx", get_hr(&e));
        }
    }

    /* _ReentrantLock */
    {
        void *(THISCALL *ctor)(void *) = (void *)GetProcAddress(mod, N("??0_ReentrantLock@details@Concurrency@@QEAA@XZ", "??0_ReentrantLock@details@Concurrency@@QAE@XZ"));
        HANDLE th;
        DWORD code;

        p_lock = (void *)GetProcAddress(mod, N("?_Acquire@_ReentrantLock@details@Concurrency@@QEAAXXZ", "?_Acquire@_ReentrantLock@details@Concurrency@@QAEXXZ"));
        p_unlock = (void *)GetProcAddress(mod, N("?_Release@_ReentrantLock@details@Concurrency@@QEAAXXZ", "?_Release@_ReentrantLock@details@Concurrency@@QAEXXZ"));
        p_try = (void *)GetProcAddress(mod, N("?_TryAcquire@_ReentrantLock@details@Concurrency@@QEAA_NXZ", "?_TryAcquire@_ReentrantLock@details@Concurrency@@QAE_NXZ"));
        check(ctor && p_lock && p_unlock && p_try, "_ReentrantLock exports");
        if (ctor && p_lock && p_unlock && p_try)
        {
            ctor(rlock);
            p_lock(rlock);
            p_lock(rlock);
            th = CreateThread(NULL, 0, try_other, NULL, 0, NULL);
            WaitForSingleObject(th, 5000); GetExitCodeThread(th, &code); CloseHandle(th);
            check(!code, "another thread cannot take it");
            p_unlock(rlock);
            th = CreateThread(NULL, 0, try_other, NULL, 0, NULL);
            WaitForSingleObject(th, 5000); GetExitCodeThread(th, &code); CloseHandle(th);
            check(!code, "still held after one of two releases");
            p_unlock(rlock);
            th = CreateThread(NULL, 0, try_other, NULL, 0, NULL);
            WaitForSingleObject(th, 5000); GetExitCodeThread(th, &code); CloseHandle(th);
            check(code == 1, "free after the second release");
        }
    }

    /* _ReaderWriterLock */
    {
        void *(THISCALL *ctor)(void *) = (void *)GetProcAddress(mod, N("??0_ReaderWriterLock@details@Concurrency@@QEAA@XZ", "??0_ReaderWriterLock@details@Concurrency@@QAE@XZ"));
        HANDLE th;

        p_rd = (void *)GetProcAddress(mod, N("?_AcquireRead@_ReaderWriterLock@details@Concurrency@@QEAAXXZ", "?_AcquireRead@_ReaderWriterLock@details@Concurrency@@QAEXXZ"));
        p_rd_rel = (void *)GetProcAddress(mod, N("?_ReleaseRead@_ReaderWriterLock@details@Concurrency@@QEAAXXZ", "?_ReleaseRead@_ReaderWriterLock@details@Concurrency@@QAEXXZ"));
        p_wr = (void *)GetProcAddress(mod, N("?_AcquireWrite@_ReaderWriterLock@details@Concurrency@@QEAAXXZ", "?_AcquireWrite@_ReaderWriterLock@details@Concurrency@@QAEXXZ"));
        p_wr_rel = (void *)GetProcAddress(mod, N("?_ReleaseWrite@_ReaderWriterLock@details@Concurrency@@QEAAXXZ", "?_ReleaseWrite@_ReaderWriterLock@details@Concurrency@@QAEXXZ"));
        p_try_wr = (void *)GetProcAddress(mod, N("?_TryAcquireWrite@_ReaderWriterLock@details@Concurrency@@QEAA_NXZ", "?_TryAcquireWrite@_ReaderWriterLock@details@Concurrency@@QAE_NXZ"));
        check(ctor && p_rd && p_rd_rel && p_wr && p_wr_rel && p_try_wr, "_ReaderWriterLock exports");
        if (ctor && p_rd && p_rd_rel && p_wr && p_wr_rel && p_try_wr)
        {
            ctor(rw);
            p_rd(rw);
            p_rd(rw);
            check(!(p_try_wr(rw) & 1), "no writer while two readers");
            p_rd_rel(rw);
            p_rd_rel(rw);
            check(p_try_wr(rw) & 1, "a writer when the readers left");
            p_wr_rel(rw);
            phase = 0;
            th = CreateThread(NULL, 0, writer, NULL, 0, NULL);
            while (!phase) Sleep(1);
            p_rd(rw);   /* waits for the writer */
            check(rw[0] == 1, "the reader got in after the writer left (state %ld)", rw[0]);
            p_rd_rel(rw);
            WaitForSingleObject(th, 5000);
            CloseHandle(th);
        }
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
