/* dbgeng smoke probe: one executable, the debugger when run bare and the debuggee when run with "child". */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "windef.h"
#include "winbase.h"
#include "initguid.h"
#include "dbgeng.h"
#include "dbghelp.h"
#include <wchar.h>

static int failures, checks;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } else printf("ok    " __FILE__ ":%d\n", __LINE__); } while (0)

struct sample
{
    int a;
    char name[8];
    void *p;
    short s;
};

__declspec(dllexport) struct sample g_sample = { 0x1234, "wine", (void *)0x5678, 9 };
__declspec(dllexport) volatile int g_result;
__declspec(dllexport) unsigned char g_jit_area[4096];

__declspec(dllexport) __attribute__((noinline)) void target_function(int x)
{
    g_result = x * 2;
}

static int sleeping_child(void)
{
    Sleep(20000);
    return 0;
}

static int child_main(void)
{
    OutputDebugStringA("child says hi\n");
    target_function(42);
    ExitProcess(7);
    return 0;
}

/* ---- callbacks ---- */
static struct { int create, load, exit_proc, breakpoint, exception, exit_code; char out[8192]; } ev;

static HRESULT STDMETHODCALLTYPE cb_qi(IDebugEventCallbacks *This, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDebugEventCallbacks))
    {
        *out = This;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE cb_addref(IDebugEventCallbacks *This) { return 2; }
static ULONG STDMETHODCALLTYPE cb_release(IDebugEventCallbacks *This) { return 1; }
static HRESULT STDMETHODCALLTYPE cb_mask(IDebugEventCallbacks *This, ULONG *mask) { *mask = ~0u; return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_bp(IDebugEventCallbacks *This, PDEBUG_BREAKPOINT bp) { ++ev.breakpoint; return DEBUG_STATUS_BREAK; }
static HRESULT STDMETHODCALLTYPE cb_exc(IDebugEventCallbacks *This, PEXCEPTION_RECORD64 r, ULONG first) { ++ev.exception; return DEBUG_STATUS_NO_CHANGE; }
static HRESULT STDMETHODCALLTYPE cb_ct(IDebugEventCallbacks *This, ULONG64 h, ULONG64 d, ULONG64 s) { return DEBUG_STATUS_NO_CHANGE; }
static HRESULT STDMETHODCALLTYPE cb_et(IDebugEventCallbacks *This, ULONG code) { return DEBUG_STATUS_NO_CHANGE; }
static HRESULT STDMETHODCALLTYPE cb_cp(IDebugEventCallbacks *This, ULONG64 ih, ULONG64 h, ULONG64 base, ULONG size, PCSTR mn,
        PCSTR in, ULONG cs, ULONG ts, ULONG64 th, ULONG64 td, ULONG64 so)
{
    ++ev.create;
    return DEBUG_STATUS_NO_CHANGE;
}
static HRESULT STDMETHODCALLTYPE cb_ep(IDebugEventCallbacks *This, ULONG code) { ++ev.exit_proc; ev.exit_code = code; return DEBUG_STATUS_NO_CHANGE; }
static HRESULT STDMETHODCALLTYPE cb_lm(IDebugEventCallbacks *This, ULONG64 ih, ULONG64 base, ULONG size, PCSTR mn, PCSTR in,
        ULONG cs, ULONG ts)
{
    ++ev.load;
    return DEBUG_STATUS_NO_CHANGE;
}
static HRESULT STDMETHODCALLTYPE cb_um(IDebugEventCallbacks *This, PCSTR n, ULONG64 base) { return DEBUG_STATUS_NO_CHANGE; }
static HRESULT STDMETHODCALLTYPE cb_se(IDebugEventCallbacks *This, ULONG e, ULONG l) { return DEBUG_STATUS_NO_CHANGE; }
static HRESULT STDMETHODCALLTYPE cb_ss(IDebugEventCallbacks *This, ULONG s) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_cd(IDebugEventCallbacks *This, ULONG f, ULONG64 a) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_ce(IDebugEventCallbacks *This, ULONG f, ULONG64 a) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_cs(IDebugEventCallbacks *This, ULONG f, ULONG64 a) { return S_OK; }

static IDebugEventCallbacksVtbl event_vtbl =
{
    cb_qi, cb_addref, cb_release, cb_mask, cb_bp, cb_exc, cb_ct, cb_et, cb_cp, cb_ep, cb_lm, cb_um, cb_se, cb_ss, cb_cd,
    cb_ce, cb_cs,
};
static IDebugEventCallbacks event_callbacks = { &event_vtbl };

static HRESULT STDMETHODCALLTYPE ocb_qi(IDebugOutputCallbacks *This, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDebugOutputCallbacks))
    {
        *out = This;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE ocb_addref(IDebugOutputCallbacks *This) { return 2; }
static ULONG STDMETHODCALLTYPE ocb_release(IDebugOutputCallbacks *This) { return 1; }
static HRESULT STDMETHODCALLTYPE ocb_output(IDebugOutputCallbacks *This, ULONG mask, PCSTR text)
{
    strncat(ev.out, text, sizeof(ev.out) - strlen(ev.out) - 1);
    return S_OK;
}
static IDebugOutputCallbacksVtbl output_vtbl = { ocb_qi, ocb_addref, ocb_release, ocb_output };
static IDebugOutputCallbacks output_callbacks = { &output_vtbl };

static void clear_output(void) { ev.out[0] = 0; }

static HRESULT run_to_event(IDebugControl4 *control, ULONG timeout)
{
    return control->lpVtbl->WaitForEvent(control, 0, timeout);
}

static ULONG64 eval(IDebugControl4 *control, const char *expr)
{
    DEBUG_VALUE v;
    HRESULT hr = control->lpVtbl->Evaluate(control, expr, DEBUG_VALUE_INT64, &v, NULL);

    if (FAILED(hr))
    {
        printf("note  Evaluate(%s) failed %#lx\n", expr, hr);
        return ~0ull;
    }
    return v.I64;
}


static WCHAR implicit_cmd[MAX_PATH + 32];

static void test_gap(IDebugClient7 *client, IDebugControl4 *control, IDebugSymbols3 *symbols,
        IDebugDataSpaces *dataspaces, IDebugSystemObjects *sysobj, IDebugAdvanced3 *advanced)
{
    char path[MAX_PATH + 32], name[256], expect[64], stream[64];
    ULONG64 ip, target_addr, base;
    DEBUG_MODULE_AND_ID id;
    ULONG opts[4], out_size, size, status, nprocs, i;
    IDebugClient *client2;
    IDebugSystemObjects *sysobj2;
    HRESULT hr;
    WCHAR *p;

    /* kernel targets cannot be connected: no current kernel target */
    hr = client->lpVtbl->GetKernelConnectionOptions(client, name, sizeof(name), &size);
    CHECK(hr == E_UNEXPECTED, "GetKernelConnectionOptions %#lx", hr);
    hr = client->lpVtbl->SetKernelConnectionOptions(client, "resync");
    CHECK(hr == E_UNEXPECTED, "SetKernelConnectionOptions %#lx", hr);
    hr = client->lpVtbl->AttachKernel(client, 99, "x");
    CHECK(hr == E_INVALIDARG, "AttachKernel with bad flags %#lx", hr);

    /* default creation options */
    memset(opts, 0xaa, sizeof(opts));
    hr = advanced->lpVtbl->Request(advanced, 4 /* GET_ADDITIONAL_CREATE_OPTIONS */, NULL, 0, opts, sizeof(opts), &out_size);
    CHECK(hr == S_OK && out_size == sizeof(opts) && opts[1] == 0 && opts[2] == 0, "get create options %#lx", hr);
    hr = advanced->lpVtbl->Request(advanced, 4, NULL, 0, opts, 4, &out_size);
    CHECK(hr == E_INVALIDARG, "get create options with a small buffer %#lx", hr);

    GetModuleFileNameA(NULL, path, MAX_PATH);
    strcat(path, " child");
    for (p = implicit_cmd, i = 0; path[i]; i++) *p++ = path[i];
    *p = 0;
    hr = advanced->lpVtbl->Request(advanced, 9 /* SET_LOCAL_IMPLICIT_COMMAND_LINE */, implicit_cmd, 0, NULL, 0, NULL);
    CHECK(hr == S_OK, "set implicit command line %#lx", hr);
    opts[0] = 0; opts[1] = 4 /* USE_IMPLICIT_COMMAND_LINE */; opts[2] = 0x55; opts[3] = 0;
    hr = advanced->lpVtbl->Request(advanced, 5 /* SET_ADDITIONAL_CREATE_OPTIONS */, opts, sizeof(opts), NULL, 0, NULL);
    CHECK(hr == S_OK, "set create options %#lx", hr);
    memset(opts, 0xaa, sizeof(opts));
    hr = advanced->lpVtbl->Request(advanced, 4, NULL, 0, opts, sizeof(opts), &out_size);
    CHECK(hr == S_OK && opts[0] == 0 && opts[1] == 4 && opts[2] == 0x55, "create options read back %#lx: %lx %lx %lx", hr,
            opts[0], opts[1], opts[2]);
    hr = advanced->lpVtbl->Request(advanced, 5, opts, 8, NULL, 0, NULL);
    CHECK(hr == E_INVALIDARG, "set create options with a wrong size %#lx", hr);

    /* the supplied command line is replaced by the implicit one */
    hr = control->lpVtbl->AddEngineOptions(control, DEBUG_ENGOPT_INITIAL_BREAK);
    CHECK(hr == S_OK, "AddEngineOptions");
    hr = client->lpVtbl->CreateProcess(client, 0, "nonexistent_program_xyz.exe", DEBUG_ONLY_THIS_PROCESS);
    CHECK(hr == S_OK, "CreateProcess with the implicit command line %#lx", hr);
    if (FAILED(hr))
        return;
    hr = run_to_event(control, 30000);
    CHECK(hr == S_OK, "WaitForEvent %#lx", hr);

    /* the code at the event's instruction pointer */
    hr = advanced->lpVtbl->Request(advanced, 10 /* GET_CAPTURED_EVENT_CODE_OFFSET */, NULL, 0, &ip, sizeof(ip), &out_size);
    CHECK(hr == S_OK && ip, "captured event code offset %#lx %I64x", hr, ip);
    memset(stream, 0, sizeof(stream));
    out_size = 0;
    hr = advanced->lpVtbl->Request(advanced, 11 /* READ_CAPTURED_EVENT_CODE_STREAM */, NULL, 0, stream, sizeof(stream), &out_size);
    CHECK(hr == S_OK && out_size > 0 && out_size <= 64, "captured event code stream %#lx size %lu", hr, out_size);
    memset(expect, 0, sizeof(expect));
    hr = dataspaces->lpVtbl->ReadVirtual(dataspaces, ip, expect, out_size, &size);
    CHECK(hr == S_OK && size == out_size && !memcmp(stream, expect, out_size), "the snapshot matches the code %#lx", hr);
    hr = advanced->lpVtbl->Request(advanced, 11, NULL, 0, stream, 2, &out_size);
    CHECK(hr == S_FALSE && out_size == 2, "captured stream into a small buffer %#lx size %lu", hr, out_size);

    /* a second client works on the same targets */
    hr = client->lpVtbl->CreateClient(client, &client2);
    CHECK(hr == S_OK && client2, "CreateClient %#lx", hr);
    if (SUCCEEDED(hr))
    {
        hr = client2->lpVtbl->QueryInterface(client2, &IID_IDebugSystemObjects, (void **)&sysobj2);
        CHECK(hr == S_OK, "QI system objects of the new client %#lx", hr);
        nprocs = 0;
        hr = sysobj2->lpVtbl->GetNumberProcesses(sysobj2, &nprocs);
        CHECK(hr == S_OK && nprocs == 1, "the new client sees the target: %#lx %lu", hr, nprocs);
        sysobj2->lpVtbl->Release(sysobj2);
        client2->lpVtbl->Release(client2);
    }

    /* synthetic symbols can be removed */
    target_addr = eval(control, "probe!target_function");
    hr = symbols->lpVtbl->AddSyntheticSymbol(symbols, target_addr + 2, 2, "synthetic_gap_sym", 0, &id);
    CHECK(hr == S_OK, "AddSyntheticSymbol %#lx", hr);
    if (SUCCEEDED(hr))
    {
        name[0] = 0;
        symbols->lpVtbl->GetNameByOffset(symbols, target_addr + 2, name, sizeof(name), NULL, NULL);
        CHECK(strstr(name, "synthetic_gap_sym") != NULL, "the synthetic symbol resolves: %s", name);
        hr = symbols->lpVtbl->RemoveSyntheticSymbol(symbols, &id);
        CHECK(hr == S_OK, "RemoveSyntheticSymbol %#lx", hr);
        name[0] = 0;
        symbols->lpVtbl->GetNameByOffset(symbols, target_addr + 2, name, sizeof(name), NULL, NULL);
        CHECK(strstr(name, "synthetic_gap_sym") == NULL, "the removed symbol is gone: %s", name);
        hr = symbols->lpVtbl->RemoveSyntheticSymbol(symbols, &id);
        CHECK(hr == E_INVALIDARG, "removing it again %#lx", hr);
    }
    hr = symbols->lpVtbl->RemoveSyntheticSymbol(symbols, NULL);
    CHECK(hr == E_INVALIDARG, "RemoveSyntheticSymbol(NULL) %#lx", hr);


    /* GetSymbolInformation: the documented queries */
    {
        IDebugBreakpoint *bp = NULL;
        ULONG bp_id = 0, line = 0, info_size2 = 0, str_size = 0, offsets[512];
        IMAGEHLP_MODULEW64 modinfo;
        WCHAR wname[128];
        char names[16384], file[512];
        BOOL found = FALSE;
        const char *n;

        hr = control->lpVtbl->AddBreakpoint(control, DEBUG_BREAKPOINT_CODE, DEBUG_ANY_ID, &bp);
        CHECK(hr == S_OK && bp, "AddBreakpoint %#lx", hr);
        if (bp)
        {
            bp->lpVtbl->GetId(bp, &bp_id);
            bp->lpVtbl->SetOffset(bp, target_addr);
            memset(file, 0, sizeof(file));
            hr = advanced->lpVtbl->GetSymbolInformation(advanced, 0 /* BREAKPOINT_SOURCE_LINE */, 0, bp_id, &line, sizeof(line),
                    &info_size2, file, sizeof(file), &str_size);
            CHECK(hr == S_OK && line > 0 && info_size2 == sizeof(ULONG) && strstr(file, "probe.c"),
                    "breakpoint source line %#lx: line %lu file %s", hr, line, file);
            hr = advanced->lpVtbl->GetSymbolInformation(advanced, 0, 0, bp_id + 100, &line, sizeof(line), NULL, file, sizeof(file), NULL);
            CHECK(hr == E_NOINTERFACE, "unknown breakpoint %#lx", hr);
        }
        hr = advanced->lpVtbl->GetSymbolInformation(advanced, 1 /* IMAGEHLP_MODULEW64 */, target_addr, 0, &modinfo, sizeof(modinfo),
                &info_size2, NULL, 0, NULL);
        CHECK(hr == S_OK && info_size2 == sizeof(modinfo), "module info %#lx %lu", hr, info_size2);
        hr = advanced->lpVtbl->GetSymbolInformation(advanced, 2 /* NAME_BY_OFFSET_AND_TAG_WIDE */, target_addr, 0, wname, sizeof(wname),
                &info_size2, NULL, 0, NULL);
        CHECK(hr == S_OK && !wcscmp(wname, L"target_function") && info_size2 == 16 * sizeof(WCHAR), "name by offset %#lx %ls size %lu", hr, wname, info_size2);
        hr = advanced->lpVtbl->GetSymbolInformation(advanced, 2, target_addr, 0, wname, 4, &info_size2, NULL, 0, NULL);
        CHECK(hr == S_FALSE && info_size2 == 16 * sizeof(WCHAR), "name by offset in a small buffer %#lx %lu", hr, info_size2);
        hr = advanced->lpVtbl->GetSymbolInformation(advanced, 2, target_addr, 2 /* SymTagCompiland */, wname, sizeof(wname), NULL, NULL, 0, NULL);
        CHECK(hr == E_NOINTERFACE, "name by offset with another tag %#lx", hr);
        memset(offsets, 0, sizeof(offsets));
        memset(names, 0, sizeof(names));
        hr = advanced->lpVtbl->GetSymbolInformation(advanced, 3 /* MODULE_SYMBOL_NAMES_AND_OFFSETS */, target_addr, 0, offsets, sizeof(offsets),
                &info_size2, names, sizeof(names), &str_size);
        CHECK(hr == S_OK && info_size2 >= sizeof(ULONG) && str_size > 2, "module symbol names %#lx %lu %lu", hr, info_size2, str_size);
        for (n = names; n < names + str_size && *n; n += strlen(n) + 2)
            if (!strcmp(n, "target_function")) found = TRUE;
        CHECK(found, "target_function is among the module symbols");
        hr = advanced->lpVtbl->GetSymbolInformation(advanced, 3, target_addr, 0, offsets, 4, &info_size2, names, sizeof(names), &str_size);
        CHECK(hr == S_FALSE, "module symbols into a small buffer %#lx", hr);
    }


    /* the scope can be set from the JIT_DEBUG_INFO a just-in-time debugger is given */
    {
        struct { DWORD size, arch, tid, reserved; ULONG64 address, exception, context; } jit;
        CONTEXT fake;
        ULONG64 area = eval(control, "probe!g_jit_area"), scope_ip = 0;
        DEBUG_STACK_FRAME fr;

        memset(&fake, 0, sizeof(fake));
#ifdef _WIN64
        fake.Rip = target_addr + 4; fake.Rsp = 0x1000; fake.Rbp = 0x2000;
#else
        fake.Eip = target_addr + 4; fake.Esp = 0x1000; fake.Ebp = 0x2000;
#endif
        memset(&jit, 0, sizeof(jit));
        jit.size = sizeof(jit);
#ifdef _WIN64
        jit.arch = 9; /* PROCESSOR_ARCHITECTURE_AMD64 */
#else
        jit.arch = 0; /* PROCESSOR_ARCHITECTURE_INTEL */
#endif
        jit.context = area + 64;
        hr = dataspaces->lpVtbl->WriteVirtual(dataspaces, area, &jit, sizeof(jit), &size);
        CHECK(hr == S_OK, "write the JIT info %#lx", hr);
        hr = dataspaces->lpVtbl->WriteVirtual(dataspaces, area + 64, &fake, sizeof(fake), &size);
        CHECK(hr == S_OK, "write the context %#lx", hr);
        hr = symbols->lpVtbl->SetScopeFromJitDebugInfo(symbols, DEBUG_OUTCTL_IGNORE, area);
        CHECK(hr == S_OK, "SetScopeFromJitDebugInfo %#lx", hr);
        memset(&fr, 0, sizeof(fr));
        hr = symbols->lpVtbl->GetScope(symbols, &scope_ip, &fr, NULL, 0);
        CHECK(hr == S_OK && scope_ip == target_addr + 4 && fr.StackOffset == 0x1000 && fr.FrameOffset == 0x2000,
                "the scope follows the JIT context %#lx %I64x", hr, scope_ip);
        hr = symbols->lpVtbl->SetScopeFromJitDebugInfo(symbols, DEBUG_OUTCTL_IGNORE, area + 2048);
        CHECK(hr == E_INVALIDARG, "empty JIT info %#lx", hr);
        symbols->lpVtbl->ResetScope(symbols);
    }

    /* the child was started from the implicit command line: it exits with its own exit code */
    ev.exit_proc = 0;
    hr = control->lpVtbl->SetExecutionStatus(control, DEBUG_STATUS_GO);
    for (i = 0; i < 20 && !ev.exit_proc; i++)
        run_to_event(control, 30000);
    CHECK(ev.exit_proc == 1 && ev.exit_code == 7, "the implicit command line ran the child: exit %d code %d", ev.exit_proc, ev.exit_code);
    client->lpVtbl->EndSession(client, DEBUG_END_PASSIVE);
}

static void test_attach_existing(IDebugClient7 *client, IDebugControl4 *control, IDebugSystemObjects *sysobj,
        IDebugDataSpaces *dataspaces)
{
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { sizeof(si) };
    char path[MAX_PATH + 32], mz[2];
    ULONG a, size;
    HRESULT hr;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    strcat(path, " sleep");
    CHECK(CreateProcessA(NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi), "create sleeping child");
    Sleep(1500);
    hr = client->lpVtbl->AttachProcess(client, 0, pi.dwProcessId, DEBUG_ATTACH_EXISTING);
    CHECK(hr == S_OK, "AttachProcess existing %#lx", hr);
    hr = run_to_event(control, 30000);
    CHECK(hr == S_OK, "WaitForEvent after the re-attach %#lx", hr);
    hr = sysobj->lpVtbl->GetCurrentProcessSystemId(sysobj, &a);
    CHECK(hr == S_OK && a == pi.dwProcessId, "process id %lu", a);
    memset(mz, 0, sizeof(mz));
    hr = dataspaces->lpVtbl->ReadVirtual(dataspaces, (ULONG64)(ULONG_PTR)GetModuleHandleA(NULL), mz, 2, &size);
    CHECK(hr == S_OK && size == 2 && mz[0] == 'M' && mz[1] == 'Z', "memory of the re-attached process %#lx", hr);
    client->lpVtbl->EndSession(client, DEBUG_END_PASSIVE);
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main(int argc, char **argv)
{
    IDebugClient7 *client;
    IDebugControl4 *control;
    IDebugSymbols3 *symbols;
    IDebugDataSpaces *dataspaces;
    IDebugSystemObjects *sysobj;
    IDebugAdvanced3 *advanced;
    HRESULT hr;

    if (argc > 1 && !strcmp(argv[1], "child"))
        return child_main();
    if (argc > 1 && !strcmp(argv[1], "sleep"))
        return sleeping_child();

    hr = DebugCreate(&IID_IDebugClient7, (void **)&client);
    if (FAILED(hr))
    {
        printf("FAIL  DebugCreate %#lx\nRESULT: FAIL\n", hr);
        return 1;
    }
    client->lpVtbl->QueryInterface(client, &IID_IDebugControl4, (void **)&control);
    client->lpVtbl->QueryInterface(client, &IID_IDebugSymbols3, (void **)&symbols);
    client->lpVtbl->QueryInterface(client, &IID_IDebugDataSpaces, (void **)&dataspaces);
    client->lpVtbl->QueryInterface(client, &IID_IDebugSystemObjects, (void **)&sysobj);
    client->lpVtbl->QueryInterface(client, &IID_IDebugAdvanced3, (void **)&advanced);
    client->lpVtbl->SetEventCallbacks(client, &event_callbacks);
    client->lpVtbl->SetOutputCallbacks(client, &output_callbacks);

    test_gap(client, control, symbols, dataspaces, sysobj, advanced);
    test_attach_existing(client, control, sysobj, dataspaces);

    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
