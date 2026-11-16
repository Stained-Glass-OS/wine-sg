/* ICorDebug over Wine Mono's soft debugger agent (wine-sg 7792), for test/cordbgsdb-gate.sh: the debugger.
 * Raw vtable slots (cordebug.h is not in mingw); 64-bit only (one calling convention for the generic callback).
 *
 *   cordbgsdb-probe.exe C:\App.exe   prints KEY=VALUE lines: launch, stop, threads, continue, attach, detach
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

typedef struct { void **vtbl; } Obj;
#define CALL(o, i, ret, ...) ((ret (__stdcall *)(Obj*, ##__VA_ARGS__))((o)->vtbl[i]))
#define HR(x) ((unsigned)(x))

static const GUID IID_ICorDebug_ = {0x3d6f5f61, 0x7538, 0x11d3, {0x8d, 0x5b, 0x00, 0x10, 0x4b, 0x35, 0xe7, 0xef}};
static const GUID IID_IUnknown_ = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
#define CORDBG_E_PROCESS_NOT_SYNCHRONIZED ((HRESULT)0x80131302)
#define CORDBG_E_DEBUGGING_NOT_POSSIBLE ((HRESULT)0x80131346)

/* the managed callback: every method answers S_OK; CreateProcess (slot 9) is counted */
static LONG created;
static void *cb_vtbl[32];
static Obj cb = { cb_vtbl };
static HRESULT __stdcall cb_qi(Obj *o, const GUID *iid, void **out) { *out = o; return S_OK; }
static ULONG __stdcall cb_ref(Obj *o) { return 1; }
static HRESULT __stdcall cb_any(Obj *o, void *a, void *b, void *c, void *d, void *e) { return S_OK; }
static HRESULT __stdcall cb_createprocess(Obj *o, Obj *proc) { InterlockedIncrement(&created); return S_OK; }

static long counter(DWORD pid)
{
    char name[64], buf[32] = "";
    FILE *f;
    sprintf(name, "C:\\counter-%lu.txt", pid);
    if (!(f = fopen(name, "r"))) return -1;
    fgets(buf, sizeof(buf), f);
    fclose(f);
    return atol(buf);
}

static BOOL thread_of(DWORD tid, DWORD pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = { sizeof(te) };
    BOOL found = FALSE;
    if (snap == INVALID_HANDLE_VALUE) return FALSE;
    if (Thread32First(snap, &te))
        do if (te.th32ThreadID == tid && te.th32OwnerProcessID == pid) found = TRUE;
        while (Thread32Next(snap, &te));
    CloseHandle(snap);
    return found;
}

/* stopped: threads enumerated, each the debuggee's; "background" if a thread has USER_BACKGROUND */
static void threads(const char *tag, Obj *proc, DWORD pid, DWORD *first)
{
    Obj *en = NULL, *th = NULL;
    ULONG n = 0, fetched, owned = 0, bg = 0;
    DWORD tid, state;
    HRESULT hr = CALL(proc, 7, HRESULT, Obj **)(proc, &en);

    printf("%s.EnumerateThreads=%#x\n", tag, HR(hr));
    if (FAILED(hr)) return;
    CALL(en, 6, HRESULT, ULONG *)(en, &n);
    while (CALL(en, 7, HRESULT, ULONG, Obj **, ULONG *)(en, 1, &th, &fetched) == S_OK && fetched == 1)
    {
        if (CALL(th, 4, HRESULT, DWORD *)(th, &tid) == S_OK && thread_of(tid, pid)) owned++;
        if (first && !*first) *first = tid;
        if (CALL(th, 9, HRESULT, DWORD *)(th, &state) == S_OK && (state & 4)) bg++;
        CALL(th, 2, ULONG)(th);
    }
    CALL(en, 2, ULONG)(en);
    printf("%s.Threads=%d %d %d\n", tag, n >= 2, owned == n, bg >= 1);
}

static void stopped_holds(const char *tag, DWORD pid)
{
    long a, b;
    Sleep(400);
    a = counter(pid);
    Sleep(1500);
    b = counter(pid);
    printf("%s.Holds=%d\n", tag, a >= 0 && a == b);
}

static void runs(const char *tag, DWORD pid)
{
    long a = counter(pid), b;
    Sleep(1500);
    b = counter(pid);
    printf("%s.Runs=%d\n", tag, b > a);
}

int main(int argc, char **argv)
{
    typedef HRESULT (WINAPI *PFN_CDI)(int, LPCWSTR, IUnknown **);
    WCHAR app[MAX_PATH], cmd[MAX_PATH + 8];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    IUnknown *unk = NULL;
    Obj *dbg = NULL, *proc = NULL, *th = NULL;
    PFN_CDI create;
    DWORD first = 0, tid;
    BOOL running;
    HRESULT hr;
    int i;

    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, app, MAX_PATH);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", app);
    for (i = 0; i < 32; i++) cb_vtbl[i] = cb_any;
    cb_vtbl[0] = cb_qi; cb_vtbl[1] = cb_ref; cb_vtbl[2] = cb_ref; cb_vtbl[9] = cb_createprocess;

    CoInitialize(NULL);
    create = (PFN_CDI)GetProcAddress(LoadLibraryW(L"mscoree.dll"), "CreateDebuggingInterfaceFromVersion");
    if (!create || (create(3, L"v4.0.30319", &unk) != S_OK && create(3, L"v2.0.50727", &unk) != S_OK))
    {
        printf("Debugger=0\n");
        return 1;
    }
    CALL((Obj *)unk, 0, HRESULT, const GUID *, void **)((Obj *)unk, &IID_ICorDebug_, (void **)&dbg);
    CALL(dbg, 3, HRESULT)(dbg);
    CALL(dbg, 5, HRESULT, Obj *)(dbg, &cb);
    printf("Debugger=1\n");
    fflush(stdout);

    /* launched by the debugger */
    memset(&pi, 0, sizeof(pi));
    hr = CALL(dbg, 7, HRESULT, LPCWSTR, LPWSTR, void *, void *, BOOL, DWORD, void *, LPCWSTR, LPSTARTUPINFOW,
              LPPROCESS_INFORMATION, DWORD, Obj **)(dbg, app, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi, 0, &proc);
    printf("Launch=%#x %d\n", HR(hr), (int)created);
    if (FAILED(hr)) return 1;
    printf("Launch.Continue=%#x\n", HR(CALL(proc, 4, HRESULT, BOOL)(proc, FALSE)));
    Sleep(2000);
    printf("Launch.Stop=%#x\n", HR(CALL(proc, 3, HRESULT, DWORD)(proc, 0)));
    stopped_holds("Launch", pi.dwProcessId);
    threads("Launch", proc, pi.dwProcessId, &first);
    hr = CALL(proc, 15, HRESULT, DWORD, Obj **)(proc, first, &th);
    tid = 0;
    if (SUCCEEDED(hr)) { CALL(th, 4, HRESULT, DWORD *)(th, &tid); CALL(th, 2, ULONG)(th); }
    printf("Launch.GetThread=%#x %d\n", HR(hr), tid == first && tid);
    printf("Launch.Continue2=%#x\n", HR(CALL(proc, 4, HRESULT, BOOL)(proc, FALSE)));
    running = FALSE;
    CALL(proc, 5, HRESULT, BOOL *)(proc, &running);
    printf("Launch.IsRunning=%d\n", running);
    runs("Launch", pi.dwProcessId);
    hr = CALL(proc, 7, HRESULT, Obj **)(proc, &th);
    printf("Launch.EnumWhileRunning=%d\n", hr == CORDBG_E_PROCESS_NOT_SYNCHRONIZED);
    CALL(proc, 10, HRESULT, UINT)(proc, 0);
    CALL(proc, 2, ULONG)(proc);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    fflush(stdout);

    /* a program running its agent (server, not suspended): attached to */
    SetEnvironmentVariableW(L"WINE_MONO_DEBUGGER_AGENT", L"transport=dt_unix,server=y,suspend=n");
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(app, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("Start=0\n"); return 1; }
    SetEnvironmentVariableW(L"WINE_MONO_DEBUGGER_AGENT", NULL);
    Sleep(4000);
    runs("Before", pi.dwProcessId);
    proc = NULL;
    hr = CALL(dbg, 8, HRESULT, DWORD, BOOL, Obj **)(dbg, pi.dwProcessId, FALSE, &proc);
    printf("Attach=%#x %d\n", HR(hr), (int)created);
    if (SUCCEEDED(hr))
    {
        stopped_holds("Attach", pi.dwProcessId);
        threads("Attach", proc, pi.dwProcessId, NULL);
        printf("Attach.Detach=%#x\n", HR(CALL(proc, 9, HRESULT)(proc)));
        runs("Detached", pi.dwProcessId);
        printf("Detached.Alive=%d\n", WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT);
        CALL(proc, 2, ULONG)(proc);
    }
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    fflush(stdout);

    /* no agent: attaching is not possible */
    memset(&pi, 0, sizeof(pi));
    CreateProcessW(app, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    Sleep(3000);
    proc = NULL;
    hr = CALL(dbg, 8, HRESULT, DWORD, BOOL, Obj **)(dbg, pi.dwProcessId, FALSE, &proc);
    printf("NoAgent=%d\n", hr == CORDBG_E_DEBUGGING_NOT_POSSIBLE);
    TerminateProcess(pi.hProcess, 0);
    printf("done=1\n");
    return 0;
}
