/* Probe for patches/sg/4532: the C threads layer of the DLL named on the
 * command line: _Mtxinit/_Mtxlock/_Mtxunlock/_Mtxdst (msvcp100 and later),
 * _Mtx_timedlock, _Thrd_exit and _Thrd_abort (msvcp110 and later), _Tss_*
 * (msvcp110, msvcp120) and _Once (msvcp100 .. msvcp120), with threads that
 * contend. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { __int64 sec; long nsec; } xtime;
static HMODULE mod;
static int ver;

static void (__cdecl *p_lock)(void *), (__cdecl *p_unlock)(void *);
static CRITICAL_SECTION cs_buf;  /* storage only */
static volatile LONG got_at;

static DWORD WINAPI mtx_locker(void *arg)
{
    p_lock(&cs_buf);
    got_at = GetTickCount();
    p_unlock(&cs_buf);
    return 0;
}

static void *mtx;  /* _Mtx_t */
static int (__cdecl *p_mtx_lock)(void *), (__cdecl *p_mtx_unlock)(void *);
static DWORD WINAPI holder(void *arg)
{
    p_mtx_lock(ver >= 140 ? mtx : (void *)&mtx);
    Sleep(600);
    p_mtx_unlock(ver >= 140 ? mtx : (void *)&mtx);
    return 0;
}

static void (__cdecl *p_thrd_exit)(int);
static DWORD WINAPI exiter(void *arg)
{
    p_thrd_exit(42);
    return 7;
}

static int (__cdecl *p_tss_set)(int, void *);
static void *(__cdecl *p_tss_get)(int);
static int tss_key;
static LONG dtor_calls;
static void *dtor_value;
static void __cdecl tss_dtor(void *v) { InterlockedIncrement(&dtor_calls); dtor_value = v; }
static DWORD WINAPI tss_thread(void *arg)
{
    p_tss_set(tss_key, arg);
    return p_tss_get(tss_key) == arg ? 0 : 1;
}

static void (__cdecl *p_once)(LONG *, void (__cdecl *)(void));
static LONG once_flag, once_calls;
static void __cdecl once_func(void) { InterlockedIncrement(&once_calls); Sleep(100); }
static DWORD WINAPI once_thread(void *arg) { p_once(&once_flag, once_func); return once_calls; }

int main(int argc, char **argv)
{
    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    sscanf(argv[1] + strlen("msvcp"), "%d", &ver);
    if (!strcmp(argv[1], "msvcp_win")) ver = 140;

    if (argc > 2)  /* child: _Thrd_abort */
    {
        void (__cdecl *p_abort)(const char *) = (void *)GetProcAddress(mod, "_Thrd_abort");
        if (p_abort) p_abort("thread failure");
        return 0x1001;
    }

    /* the library's critical section mutex */
    {
        void (__cdecl *p_init)(void *) = (void *)GetProcAddress(mod, "_Mtxinit");
        void (__cdecl *p_dst)(void *) = (void *)GetProcAddress(mod, "_Mtxdst");
        HANDLE th;
        DWORD released;

        p_lock = (void *)GetProcAddress(mod, "_Mtxlock");
        p_unlock = (void *)GetProcAddress(mod, "_Mtxunlock");
        check(p_init && p_dst && p_lock && p_unlock, "_Mtx* exported");
        if (p_init && p_dst && p_lock && p_unlock)
        {
            p_init(&cs_buf);
            p_lock(&cs_buf);
            p_lock(&cs_buf);  /* recursive */
            th = CreateThread(NULL, 0, mtx_locker, NULL, 0, NULL);
            Sleep(200);
            p_unlock(&cs_buf);
            Sleep(100);
            check(!got_at, "held after one of two unlocks");
            released = GetTickCount();
            p_unlock(&cs_buf);
            check(!WaitForSingleObject(th, 5000) && got_at >= released, "the waiter gets it after the last unlock");
            CloseHandle(th);
            p_dst(&cs_buf);
        }
    }

    if (ver >= 110)
    {
        int (__cdecl *p_init)(void **, int) = (void *)GetProcAddress(mod, "_Mtx_init");
        int (__cdecl *p_timed)(void *, const xtime *) = (void *)GetProcAddress(mod, "_Mtx_timedlock");
        int (__cdecl *p_xtime)(xtime *, int) = (void *)GetProcAddress(mod, "xtime_get");
        void (__cdecl *p_destroy)(void *) = (void *)GetProcAddress(mod, "_Mtx_destroy");
        HANDLE th;
        DWORD start, took, code = 0;
        xtime xt;
        int r;

        if (!p_xtime) p_xtime = (void *)GetProcAddress(mod, "_Xtime_get");
        p_mtx_lock = (void *)GetProcAddress(mod, "_Mtx_lock");
        p_mtx_unlock = (void *)GetProcAddress(mod, "_Mtx_unlock");
        p_thrd_exit = (void *)GetProcAddress(mod, "_Thrd_exit");
        check(p_init && p_timed && p_xtime && p_destroy && p_mtx_lock && p_mtx_unlock && p_thrd_exit, "_Mtx_timedlock, _Thrd_exit exported");
        if (!failures)
        {
            p_init(&mtx, 4 /* timed */);
            th = CreateThread(NULL, 0, holder, NULL, 0, NULL);
            Sleep(150);
            p_xtime(&xt, 1);
            xt.nsec += 100000000; if (xt.nsec >= 1000000000) { xt.sec++; xt.nsec -= 1000000000; }
            start = GetTickCount();
            r = p_timed(ver >= 140 ? mtx : (void *)&mtx, &xt);
            took = GetTickCount() - start;
            check(r == 2 && took >= 60 && took < 450, "_Mtx_timedlock times out: %d after %lu ms", r, took);
            p_xtime(&xt, 1);
            xt.sec += 3;
            start = GetTickCount();
            r = p_timed(ver >= 140 ? mtx : (void *)&mtx, &xt);
            took = GetTickCount() - start;
            check(r == 0 && took < 2500, "_Mtx_timedlock gets it when released: %d after %lu ms", r, took);
            p_mtx_unlock(ver >= 140 ? mtx : (void *)&mtx);
            WaitForSingleObject(th, 5000);
            CloseHandle(th);
            p_destroy(ver >= 140 ? mtx : (void *)&mtx);

            th = CreateThread(NULL, 0, exiter, NULL, 0, NULL);
            WaitForSingleObject(th, 5000);
            GetExitCodeThread(th, &code);
            CloseHandle(th);
            check(code == 42, "_Thrd_exit(42): exit code %lu", code);
        }
        {
            STARTUPINFOA si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            char cmd[MAX_PATH + 64], self[MAX_PATH];

            GetModuleFileNameA(NULL, self, sizeof(self));
            sprintf(cmd, "\"%s\" %s abort", self, argv[1]);
            code = 0;
            if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            {
                WaitForSingleObject(pi.hProcess, 30000);
                GetExitCodeProcess(pi.hProcess, &code);
                CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            }
            check(code == 3, "_Thrd_abort aborts: exit %lu", code);
        }
    }

    if (ver == 110 || ver == 120)
    {
        int (__cdecl *p_create)(int *, void (__cdecl *)(void *)) = (void *)GetProcAddress(mod, "_Tss_create");
        int (__cdecl *p_delete)(int) = (void *)GetProcAddress(mod, "_Tss_delete");
        HANDLE th;
        DWORD code = 1;

        p_tss_set = (void *)GetProcAddress(mod, "_Tss_set");
        p_tss_get = (void *)GetProcAddress(mod, "_Tss_get");
        check(p_create && p_delete && p_tss_set && p_tss_get, "_Tss_* exported");
        if (p_create && p_delete && p_tss_set && p_tss_get)
        {
            check(!p_create(&tss_key, tss_dtor), "_Tss_create");
            check(!p_tss_get(tss_key), "a new key holds NULL");
            check(!p_tss_set(tss_key, (void *)0x1234) && p_tss_get(tss_key) == (void *)0x1234, "_Tss_set/_Tss_get");
            th = CreateThread(NULL, 0, tss_thread, (void *)0x5678, 0, NULL);
            WaitForSingleObject(th, 5000);
            GetExitCodeThread(th, &code);
            CloseHandle(th);
            check(!code && p_tss_get(tss_key) == (void *)0x1234, "per thread values");
            check(dtor_calls == 1 && dtor_value == (void *)0x5678, "the destructor ran at the thread's exit (%ld)", dtor_calls);
            check(!p_delete(tss_key) && dtor_calls == 1, "_Tss_delete runs no destructor");
        }
    }

    if (ver >= 100 && ver <= 120)
    {
        HANDLE th[4];
        int i;

        p_once = (void *)GetProcAddress(mod, "_Once");
        check(p_once != NULL, "_Once exported");
        if (p_once)
        {
            for (i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, once_thread, NULL, 0, NULL);
            WaitForMultipleObjects(4, th, TRUE, 5000);
            for (i = 0; i < 4; i++) CloseHandle(th[i]);
            p_once(&once_flag, once_func);
            check(once_calls == 1, "_Once calls once (%ld)", once_calls);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
