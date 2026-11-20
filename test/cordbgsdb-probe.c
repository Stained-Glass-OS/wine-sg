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
static const GUID IID_ICorDebugILFrame_ = {0x03e26311, 0x4f76, 0x11d3, {0x88, 0xc6, 0x00, 0x60, 0x97, 0x94, 0x54, 0x18}};
#define CORDBG_E_PROCESS_NOT_SYNCHRONIZED ((HRESULT)0x80131302)
#define CORDBG_E_DEBUGGING_NOT_POSSIBLE ((HRESULT)0x80131346)

/* the managed callback: every method answers S_OK; CreateProcess (slot 9), Breakpoint (3) and StepComplete
 * (4) are counted, the thread of the last breakpoint kept */
static LONG created, bp_hits, steps_done;
static Obj *bp_thread;
static void *cb_vtbl[32];
static Obj cb = { cb_vtbl };
static HRESULT __stdcall cb_qi(Obj *o, const GUID *iid, void **out) { *out = o; return S_OK; }
static ULONG __stdcall cb_ref(Obj *o) { return 1; }
static HRESULT __stdcall cb_any(Obj *o, void *a, void *b, void *c, void *d, void *e) { return S_OK; }
static HRESULT __stdcall cb_createprocess(Obj *o, Obj *proc) { InterlockedIncrement(&created); return S_OK; }
static HRESULT __stdcall cb_breakpoint(Obj *o, Obj *domain, Obj *thread, Obj *bp)
{
    if (!bp_thread && thread) { bp_thread = thread; CALL(thread, 1, ULONG)(thread); }
    InterlockedIncrement(&bp_hits);
    return S_OK;
}
static HRESULT __stdcall cb_stepcomplete(Obj *o, Obj *domain, Obj *thread, Obj *stepper, int reason)
{
    InterlockedIncrement(&steps_done);
    return S_OK;
}
/* the load callbacks (wine-sg 8209): each stops the process; a debugger continues it */
static LONG domains_created, assemblies_loaded, modules_loaded, xml_loaded;
static HRESULT __stdcall cb_createappdomain(Obj *o, Obj *proc, Obj *domain)
{
    InterlockedIncrement(&domains_created);
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    return S_OK;
}
static HRESULT __stdcall cb_loadassembly(Obj *o, Obj *domain, Obj *assembly)
{
    InterlockedIncrement(&assemblies_loaded);
    CALL(domain, 4, HRESULT, BOOL)(domain, FALSE);
    return S_OK;
}
static HRESULT __stdcall cb_loadmodule(Obj *o, Obj *domain, Obj *module)
{
    WCHAR name[MAX_PATH];
    ULONG32 len = 0;
    InterlockedIncrement(&modules_loaded);
    if (CALL(module, 6, HRESULT, ULONG32, ULONG32 *, WCHAR *)(module, MAX_PATH, &len, name) == S_OK && len > 14 &&
        !_wcsicmp(name + wcslen(name) - 14, L"System.Xml.dll"))
        InterlockedIncrement(&xml_loaded);
    CALL(domain, 4, HRESULT, BOOL)(domain, FALSE);
    return S_OK;
}
static Obj *g_il, *g_thread, *g_callee;   /* App.Inspect's frame, thread, and the frame it called (App.Tick) */
static unsigned int g_ip;

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

/* a value's type and, for a primitive, its bytes; for a string reference, its text (UTF-8) */
static void show_value(const char *name, Obj *v)
{
    int type = 0;
    unsigned char raw[8] = {0};
    BOOL isnull = TRUE;
    Obj *str = NULL;
    WCHAR w[64];
    char u[128] = "";
    ULONG32 n = 0;
    long long ll;
    double d;
    int i32;

    if (!v) { printf("Value.%s=none\n", name); return; }
    CALL(v, 3, HRESULT, int *)(v, &type);
    switch (type)
    {
    case 0x2: case 0x8:
        CALL(v, 7, HRESULT, void *)(v, raw);
        memcpy(&i32, raw, 4);
        printf("Value.%s=%#x %d\n", name, type, type == 2 ? raw[0] : i32);
        break;
    case 0xa:
        CALL(v, 7, HRESULT, void *)(v, raw);
        memcpy(&ll, raw, 8);
        printf("Value.%s=%#x %lld\n", name, type, ll);
        break;
    case 0xd:
        CALL(v, 7, HRESULT, void *)(v, raw);
        memcpy(&d, raw, 8);
        printf("Value.%s=%#x %.2f\n", name, type, d);
        break;
    case 0x12:
        CALL(v, 7, HRESULT, BOOL *)(v, &isnull);
        printf("Value.%s=%#x %s\n", name, type, isnull ? "null" : "object");
        break;
    case 0xe:
        CALL(v, 7, HRESULT, BOOL *)(v, &isnull);
        if (!isnull && CALL(v, 10, HRESULT, Obj **)(v, &str) == S_OK && str)
        {
            CALL(str, 10, HRESULT, ULONG32, ULONG32 *, WCHAR *)(str, 63, &n, w);
            w[n] = 0;
            WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL);
            CALL(str, 2, ULONG)(str);
        }
        printf("Value.%s=%#x %s\n", name, type, u);
        break;
    default:
        printf("Value.%s=%#x\n", name, type);
    }
    CALL(v, 2, ULONG)(v);
}

/* stopped: the thread whose frame has argument 1 == 12345 (App.Inspect, argument 0 "this"); its arguments
 * and locals */
static void inspect(Obj *proc)
{
    Obj *en = NULL, *th = NULL, *chain = NULL, *fen = NULL, *fr = NULL, *il = NULL, *v = NULL;
    ULONG fetched;
    unsigned char raw[8];
    int type, found = 0, frames = 0, i32;
    unsigned int token = 0, ip = 0, map = 0;
    HRESULT hr;

    if (CALL(proc, 7, HRESULT, Obj **)(proc, &en) != S_OK) { printf("Inspect=0\n"); return; }
    while (!found && CALL(en, 7, HRESULT, ULONG, Obj **, ULONG *)(en, 1, &th, &fetched) == S_OK && fetched == 1)
    {
        if (CALL(th, 14, HRESULT, Obj **)(th, &chain) == S_OK && chain)
        {
            if (CALL(chain, 11, HRESULT, Obj **)(chain, &fen) == S_OK)
            {
                while (!found && CALL(fen, 7, HRESULT, ULONG, Obj **, ULONG *)(fen, 1, &fr, &fetched) == S_OK && fetched == 1)
                {
                    frames++;
                    if (CALL(fr, 0, HRESULT, const GUID *, void **)(fr, &IID_ICorDebugILFrame_, (void **)&il) == S_OK)
                    {
                        v = NULL;
                        if (CALL(il, 16, HRESULT, DWORD, Obj **)(il, 1, &v) == S_OK && v)
                        {
                            type = 0; CALL(v, 3, HRESULT, int *)(v, &type);
                            if (type == 8 && CALL(v, 7, HRESULT, void *)(v, raw) == S_OK)
                            {
                                memcpy(&i32, raw, 4);
                                if (i32 == 12345) found = 1;
                            }
                            CALL(v, 2, ULONG)(v);
                        }
                        if (found)
                        {
                            const char *names[] = { "big", "ratio", "flag", "text" };
                            int k;
                            CALL(il, 6, HRESULT, unsigned int *)(il, &token);
                            CALL(il, 11, HRESULT, unsigned int *, unsigned int *)(il, &ip, &map);
                            g_ip = ip;
                            g_il = il;
                            CALL(il, 1, ULONG)(il);
                            g_thread = th;
                            CALL(th, 1, ULONG)(th);
                            CALL(il, 9, HRESULT, Obj **)(il, &g_callee);   /* App.Tick's frame */
                            printf("Inspect.Token=%d\n", (token >> 24) == 6);
                            for (k = 0; k < 4; k++) { v = NULL; CALL(il, 14, HRESULT, DWORD, Obj **)(il, k, &v); show_value(names[k], v); }
                            v = NULL; CALL(il, 16, HRESULT, DWORD, Obj **)(il, 2, &v); show_value("label", v);
                            v = NULL; CALL(il, 16, HRESULT, DWORD, Obj **)(il, 0, &v); show_value("this", v);
                            v = NULL; hr = CALL(il, 14, HRESULT, DWORD, Obj **)(il, 99, &v);
                            printf("Inspect.BadLocal=%d\n", hr == E_INVALIDARG);
                            v = NULL; hr = CALL(il, 16, HRESULT, DWORD, Obj **)(il, 3, &v);
                            printf("Inspect.BadArg=%d\n", hr == E_INVALIDARG);
                        }
                        CALL(il, 2, ULONG)(il);
                    }
                    CALL(fr, 2, ULONG)(fr);
                }
                CALL(fen, 2, ULONG)(fen);
            }
            CALL(chain, 2, ULONG)(chain);
        }
        CALL(th, 2, ULONG)(th);
    }
    CALL(en, 2, ULONG)(en);
    printf("Inspect=%d\n", found);
}

static int wait_count(LONG *what, LONG want, int ms)
{
    for (; ms > 0 && *what < want; ms -= 50) Sleep(50);
    return *what >= want;
}

static unsigned int active_ip(Obj *thread)
{
    Obj *fr = NULL, *il = NULL;
    unsigned int ip = ~0u, map;
    if (CALL(thread, 15, HRESULT, Obj **)(thread, &fr) == S_OK && fr)
    {
        if (CALL(fr, 0, HRESULT, const GUID *, void **)(fr, &IID_ICorDebugILFrame_, (void **)&il) == S_OK)
        {
            CALL(il, 11, HRESULT, unsigned int *, unsigned int *)(il, &ip, &map);
            CALL(il, 2, ULONG)(il);
        }
        CALL(fr, 2, ULONG)(fr);
    }
    return ip;
}

/* stopped in App.Tick (called by App.Inspect every round): a breakpoint at Tick's first IL offset, continued,
 * is hit (the Breakpoint callback, the program stopped); from there a step over completes at another IL
 * offset (StepComplete); the breakpoint off, the program runs on, stopped by nothing */
static void breakpoint_and_step(Obj *proc, DWORD pid)
{
    Obj *fn = NULL, *code = NULL, *bp = NULL, *st = NULL;
    unsigned int ip1, ip2;
    long a, b;
    HRESULT hr;

    if (!g_callee) { printf("BP=0\n"); return; }
    hr = CALL(g_callee, 5, HRESULT, Obj **)(g_callee, &fn);
    if (SUCCEEDED(hr)) hr = CALL(fn, 6, HRESULT, Obj **)(fn, &code);
    if (SUCCEEDED(hr)) hr = CALL(code, 7, HRESULT, unsigned int, Obj **)(code, 0, &bp);
    printf("BP.Create=%#x\n", HR(hr));
    if (FAILED(hr)) return;
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    printf("BP.Hit=%d\n", wait_count(&bp_hits, 1, 10000));
    a = counter(pid); Sleep(1200); b = counter(pid);
    printf("BP.Holds=%d\n", a >= 0 && a == b);
    if (!bp_thread) return;
    CALL(bp, 3, HRESULT, BOOL)(bp, FALSE);   /* off before stepping on */
    ip1 = active_ip(bp_thread);
    hr = CALL(bp_thread, 12, HRESULT, Obj **)(bp_thread, &st);
    if (SUCCEEDED(hr)) hr = CALL(st, 7, HRESULT, BOOL)(st, FALSE);
    printf("Step.Start=%#x\n", HR(hr));
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    printf("Step.Done=%d\n", wait_count(&steps_done, 1, 10000));
    ip2 = active_ip(bp_thread);
    printf("Step.Moved=%d\n", ip1 != ~0u && ip2 != ~0u && ip1 != ip2);
    {
        BOOL active = TRUE;
        hr = CALL(st, 3, HRESULT, BOOL *)(st, &active);
        printf("Step.Ended=%d\n", hr == S_OK && !active);
    }
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    a = counter(pid); Sleep(1500); b = counter(pid);
    printf("BP.Off=%d\n", b > a && bp_hits == 1 && steps_done == 1);
    CALL(proc, 3, HRESULT, DWORD)(proc, 0);   /* stopped again, for what follows */
    if (st) CALL(st, 2, ULONG)(st);
    CALL(bp, 2, ULONG)(bp);
    CALL(code, 2, ULONG)(code);
    CALL(fn, 2, ULONG)(fn);
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
    cb_vtbl[3] = cb_breakpoint; cb_vtbl[4] = cb_stepcomplete;
    cb_vtbl[13] = cb_loadmodule; cb_vtbl[20] = cb_createappdomain; cb_vtbl[22] = cb_loadassembly;

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
    inspect(proc);
    breakpoint_and_step(proc, pi.dwProcessId);
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
