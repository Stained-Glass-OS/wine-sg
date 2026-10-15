/* mscoree ICorDebug: process enumerator, GetProcess, controller state, memory and thread context (patch 3732).
 * Raw vtable slots (cordebug.h is not in mingw). The debuggee is this exe started with "child". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { void **vtbl; } Obj;
#define SLOT(o, i) ((o)->vtbl[i])
#define CALL(o, i, ret, ...) ((ret (__stdcall *)(Obj*, ##__VA_ARGS__))SLOT(o, i))
#define HR(x) ((unsigned)(x))

static const GUID IID_ICorDebug_ = {0x3d6f5f61, 0x7538, 0x11d3, {0x8d, 0x5b, 0x00, 0x10, 0x4b, 0x35, 0xe7, 0xef}};
#define CORDBG_E_PROCESS_NOT_SYNCHRONIZED ((HRESULT)0x80131302)

static const char marker[16] = "SGCORDBGMARKER!";
static char wbuf[16] = "initialwritebuf";

typedef HRESULT (WINAPI *PFN_CDI)(int, LPCWSTR, IUnknown **);

static Obj *launch(Obj *dbg, PROCESS_INFORMATION *pi, WCHAR *self)
{
    STARTUPINFOW si = {sizeof(si)};
    WCHAR cmd[MAX_PATH * 2];
    Obj *proc = NULL;
    HRESULT hr;

    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child", self);
    memset(pi, 0, sizeof(*pi));
    hr = CALL(dbg, 7, HRESULT, LPCWSTR, LPWSTR, void *, void *, BOOL, DWORD, void *, LPCWSTR, LPSTARTUPINFOW,
            LPPROCESS_INFORMATION, DWORD, Obj **)(dbg, self, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, pi, 0, &proc);
    CHECK(hr == S_OK && proc, "CreateProcess %#x", HR(hr));
    return hr == S_OK ? proc : NULL;
}

int main(int argc, char **argv)
{
    WCHAR self[MAX_PATH];
    HMODULE mscoree;
    PFN_CDI create;
    IUnknown *unk = NULL;
    Obj *dbg = NULL, *proc = NULL, *proc2 = NULL, *en = NULL, *en2 = NULL, *got = NULL;
    PROCESS_INFORMATION pi, pi2;
    HRESULT hr;
    ULONG n, cnt;
    BOOL b;
    DWORD id;
    SIZE_T done;
    char buf[32];
    CONTEXT ctx;
    int i;

    if (argc > 1 && !strcmp(argv[1], "child")) { Sleep(60000); return 0; }

    CoInitialize(NULL);
    GetModuleFileNameW(NULL, self, MAX_PATH);
    mscoree = LoadLibraryW(L"mscoree.dll");
    create = mscoree ? (PFN_CDI)GetProcAddress(mscoree, "CreateDebuggingInterfaceFromVersion") : NULL;
    if (!create) { printf("SKIP no CreateDebuggingInterfaceFromVersion\nRESULT: PASS\n"); return 0; }
    hr = create(3, L"v2.0.50727", &unk);
    if (hr != S_OK) { printf("SKIP no runtime (%#x)\nRESULT: PASS\n", HR(hr)); return 0; }
    hr = CALL((Obj *)unk, 0, HRESULT, const GUID *, void **)((Obj *)unk, &IID_ICorDebug_, (void **)&dbg);
    CHECK(hr == S_OK && dbg, "QI ICorDebug %#x", HR(hr));
    if (!dbg) { printf("RESULT: FAIL\n"); return 1; }

    CHECK(CALL(dbg, 3, HRESULT)(dbg) == S_OK, "Initialize");
    CHECK(CALL(dbg, 6, HRESULT, void *)(dbg, NULL) == E_INVALIDARG, "SetUnmanagedHandler NULL");
    CHECK(CALL(dbg, 11, HRESULT, DWORD, BOOL)(dbg, GetCurrentProcessId(), FALSE) == S_OK, "CanLaunchOrAttach");

    /* no processes yet */
    hr = CALL(dbg, 9, HRESULT, Obj **)(dbg, &en);
    CHECK(hr == S_OK && en, "EnumerateProcesses");
    CHECK(CALL(dbg, 9, HRESULT, Obj **)(dbg, NULL) == E_INVALIDARG, "EnumerateProcesses NULL");
    cnt = 99; hr = CALL(en, 6, HRESULT, ULONG *)(en, &cnt);
    CHECK(hr == S_OK && cnt == 0, "empty count %lu", cnt);
    n = 99; hr = CALL(en, 7, HRESULT, ULONG, Obj **, ULONG *)(en, 1, &got, &n);
    CHECK(hr == S_FALSE && n == 0, "Next on empty: %#x %lu", HR(hr), n);
    hr = CALL(dbg, 10, HRESULT, DWORD, Obj **)(dbg, GetCurrentProcessId(), &got);
    CHECK(FAILED(hr), "GetProcess unknown pid %#x", HR(hr));

    /* a debuggee */
    proc = launch(dbg, &pi, self);
    if (!proc) { printf("RESULT: FAIL\n"); return 1; }
    hr = CALL(proc, 13, HRESULT, DWORD *)(proc, &id);
    CHECK(hr == S_OK && id == pi.dwProcessId, "GetID");
    b = 7; hr = CALL(proc, 5, HRESULT, BOOL *)(proc, &b);
    CHECK(hr == S_OK && b == FALSE, "IsRunning while stopped %d", b);
    CHECK(CALL(proc, 5, HRESULT, BOOL *)(proc, NULL) == E_INVALIDARG, "IsRunning NULL");
    b = 7; hr = CALL(proc, 6, HRESULT, void *, BOOL *)(proc, NULL, &b);
    CHECK(hr == S_OK && b == FALSE, "HasQueuedCallbacks");
    b = 7; hr = CALL(proc, 18, HRESULT, DWORD, BOOL *)(proc, pi.dwThreadId, &b);
    CHECK(hr == S_OK && b == FALSE, "IsOSSuspended");
    CHECK(CALL(proc, 11, HRESULT, ULONG, void *, void *)(proc, 0, NULL, NULL) == E_NOTIMPL, "CanCommitChanges is obsolete");

    /* enumerator */
    cnt = 0; en2 = NULL;
    CALL(dbg, 9, HRESULT, Obj **)(dbg, &en2);
    hr = CALL(en2, 6, HRESULT, ULONG *)(en2, &cnt);
    CHECK(hr == S_OK && cnt == 1, "count with one process %lu", cnt);
    got = NULL; n = 99;
    hr = CALL(en2, 7, HRESULT, ULONG, Obj **, ULONG *)(en2, 1, &got, &n);
    CHECK(hr == S_OK && n == 1 && got == proc, "Next: %#x %lu %p vs %p", HR(hr), n, got, proc);
    if (got) CALL(got, 2, ULONG)(got);
    n = 99;
    hr = CALL(en2, 7, HRESULT, ULONG, Obj **, ULONG *)(en2, 1, &got, &n);
    CHECK(hr == S_FALSE && n == 0, "Next at the end: %#x %lu", HR(hr), n);
    CHECK(CALL(en2, 4, HRESULT)(en2) == S_OK, "Reset");
    CHECK(CALL(en2, 3, HRESULT, ULONG)(en2, 1) == S_OK, "Skip(1)");
    hr = CALL(en2, 7, HRESULT, ULONG, Obj **, ULONG *)(en2, 1, &got, &n);
    CHECK(hr == S_FALSE && n == 0, "Next after Skip(1): %#x", HR(hr));
    CALL(en2, 4, HRESULT)(en2);
    CHECK(CALL(en2, 3, HRESULT, ULONG)(en2, 2) == S_FALSE, "Skip past the end");
    CALL(en2, 4, HRESULT)(en2);
    CALL(en2, 3, HRESULT, ULONG)(en2, 1);
    {
        Obj *clone = NULL;
        hr = CALL(en2, 5, HRESULT, Obj **)(en2, &clone);
        CHECK(hr == S_OK && clone && clone != en2, "Clone %#x", HR(hr));
        if (clone)
        {
            /* the clone starts where the original is (past the only process) and then moves alone */
            n = 99; got = NULL;
            hr = CALL(clone, 7, HRESULT, ULONG, Obj **, ULONG *)(clone, 1, &got, &n);
            CHECK(hr == S_FALSE && n == 0, "clone starts at the original's position: %#x %lu", HR(hr), n);
            CALL(clone, 4, HRESULT)(clone);
            CALL(en2, 4, HRESULT)(en2);
            n = 99; got = NULL;
            hr = CALL(en2, 7, HRESULT, ULONG, Obj **, ULONG *)(en2, 1, &got, &n);
            CHECK(hr == S_OK && n == 1, "the original does not move with the clone");
            if (got) CALL(got, 2, ULONG)(got);
            CALL(clone, 2, ULONG)(clone);
        }
    }
    CHECK(CALL(en2, 7, HRESULT, ULONG, Obj **, ULONG *)(en2, 1, NULL, &n) == E_INVALIDARG, "Next NULL array");
    CALL(en2, 2, ULONG)(en2);
    CALL(en, 2, ULONG)(en);

    got = NULL;
    hr = CALL(dbg, 10, HRESULT, DWORD, Obj **)(dbg, pi.dwProcessId, &got);
    CHECK(hr == S_OK && got == proc, "GetProcess %#x", HR(hr));
    if (got) CALL(got, 2, ULONG)(got);

    /* memory: the child is this exe, so the same addresses hold the same image */
    memset(buf, 0, sizeof(buf)); done = 0;
    hr = CALL(proc, 21, HRESULT, ULONGLONG, DWORD, BYTE *, SIZE_T *)(proc, (ULONGLONG)(ULONG_PTR)marker, 16, (BYTE *)buf, &done);
    CHECK(hr == S_OK && done == 16 && !memcmp(buf, marker, 16), "ReadMemory %#x %lu", HR(hr), (unsigned long)done);
    done = 7;
    hr = CALL(proc, 21, HRESULT, ULONGLONG, DWORD, BYTE *, SIZE_T *)(proc, 8, 4, (BYTE *)buf, &done);
    CHECK(FAILED(hr) && done == 0, "ReadMemory at an unmapped address %#x", HR(hr));
    CHECK(CALL(proc, 21, HRESULT, ULONGLONG, DWORD, BYTE *, SIZE_T *)(proc, (ULONGLONG)(ULONG_PTR)marker, 16, NULL, &done) == E_INVALIDARG, "ReadMemory NULL buffer");
    done = 0;
    hr = CALL(proc, 22, HRESULT, ULONGLONG, DWORD, BYTE *, SIZE_T *)(proc, (ULONGLONG)(ULONG_PTR)wbuf, 5, (BYTE *)"HELLO", &done);
    CHECK(hr == S_OK && done == 5, "WriteMemory %#x", HR(hr));
    memset(buf, 0, sizeof(buf));
    hr = CALL(proc, 21, HRESULT, ULONGLONG, DWORD, BYTE *, SIZE_T *)(proc, (ULONGLONG)(ULONG_PTR)wbuf, 15, (BYTE *)buf, &done);
    CHECK(hr == S_OK && !memcmp(buf, "HELLOalwritebuf", 15), "ReadMemory after write: %.15s", buf);
    CHECK(!strcmp(wbuf, "initialwritebuf"), "the debugger's own memory changed");

    /* thread context */
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL;
    hr = CALL(proc, 19, HRESULT, DWORD, ULONG, BYTE *)(proc, pi.dwThreadId, sizeof(ctx), (BYTE *)&ctx);
    CHECK(hr == S_OK, "GetThreadContext %#x", HR(hr));
#ifdef _WIN64
    CHECK(ctx.Rip != 0 && ctx.Rsp != 0, "context empty");
#else
    CHECK(ctx.Eip != 0 && ctx.Esp != 0, "context empty");
#endif
    CHECK(CALL(proc, 19, HRESULT, DWORD, ULONG, BYTE *)(proc, pi.dwThreadId, sizeof(ctx) - 8, (BYTE *)&ctx) == E_INVALIDARG, "context size");
    CHECK(CALL(proc, 19, HRESULT, DWORD, ULONG, BYTE *)(proc, GetCurrentThreadId(), sizeof(ctx), (BYTE *)&ctx) == E_INVALIDARG, "thread of another process");
    CHECK(CALL(proc, 19, HRESULT, DWORD, ULONG, BYTE *)(proc, pi.dwThreadId, sizeof(ctx), NULL) == E_INVALIDARG, "context NULL");
    hr = CALL(proc, 20, HRESULT, DWORD, ULONG, BYTE *)(proc, pi.dwThreadId, sizeof(ctx), (BYTE *)&ctx);
    CHECK(hr == S_OK, "SetThreadContext %#x", HR(hr));
    CHECK(CALL(proc, 20, HRESULT, DWORD, ULONG, BYTE *)(proc, GetCurrentThreadId(), sizeof(ctx), (BYTE *)&ctx) == E_INVALIDARG, "set context of another process");

    /* controller: Continue starts the process once, a second Continue is not synchronised */
    CHECK(CALL(proc, 4, HRESULT, BOOL)(proc, FALSE) == S_OK, "Continue");
    b = 7; CALL(proc, 5, HRESULT, BOOL *)(proc, &b);
    CHECK(b == TRUE, "IsRunning after Continue %d", b);
    CHECK(WaitForSingleObject(pi.hProcess, 300) == WAIT_TIMEOUT, "child is not running");
    CHECK(CALL(proc, 4, HRESULT, BOOL)(proc, FALSE) == CORDBG_E_PROCESS_NOT_SYNCHRONIZED, "second Continue %#x",
            HR(CALL(proc, 4, HRESULT, BOOL)(proc, FALSE)));

    /* Detach: the process keeps running, the debugger forgets it */
    hr = CALL(proc, 9, HRESULT)(proc);
    CHECK(hr == S_OK, "Detach %#x", HR(hr));
    CHECK(WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT, "child died with Detach");
    cnt = 99; en = NULL; CALL(dbg, 9, HRESULT, Obj **)(dbg, &en);
    CALL(en, 6, HRESULT, ULONG *)(en, &cnt);
    CHECK(cnt == 0, "process still listed after Detach: %lu", cnt);
    CALL(en, 2, ULONG)(en);
    CHECK(FAILED(CALL(dbg, 10, HRESULT, DWORD, Obj **)(dbg, pi.dwProcessId, &got)), "GetProcess after Detach");
    CHECK(CALL(proc, 9, HRESULT)(proc) == (HRESULT)0x8013134f, "second Detach");
    b = 7; CALL(proc, 5, HRESULT, BOOL *)(proc, &b);
    CHECK(b == FALSE, "IsRunning after Detach");
    CALL(proc, 2, ULONG)(proc);
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

    /* a second debuggee ends with the debugger */
    proc2 = launch(dbg, &pi2, self);
    if (proc2)
    {
        CHECK(CALL(proc2, 10, HRESULT, UINT)(proc2, 0) == S_OK, "Terminate");
        CHECK(WaitForSingleObject(pi2.hProcess, 5000) == WAIT_OBJECT_0, "Terminate did not end the process");
        CALL(proc2, 2, ULONG)(proc2);
        CloseHandle(pi2.hProcess); CloseHandle(pi2.hThread);
    }
    CALL(dbg, 4, HRESULT)(dbg);
    CALL(dbg, 2, ULONG)(dbg);
    CALL((Obj *)unk, 2, ULONG)((Obj *)unk);
    (void)i;
    printf("%d checks, %d failures\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
