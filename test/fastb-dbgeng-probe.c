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

static void test_static(IDebugClient7 *client, IDebugControl4 *control)
{
    char buffer[256];
    WCHAR wbuffer[256];
    ULONG size, a, b, c, i;
    DEBUG_VALUE v, out;
    HRESULT hr;
    BOOL append = TRUE;
    HANDLE file;
    char data[256];
    DWORD read;

    /* log file */
    hr = control->lpVtbl->GetLogFile(control, buffer, sizeof(buffer), &size, &append);
    CHECK(hr == E_NOINTERFACE, "GetLogFile without a log: %#lx", hr);
    hr = control->lpVtbl->OpenLogFile(control, "C:\\dbg.log", FALSE);
    CHECK(hr == S_OK, "OpenLogFile %#lx", hr);
    hr = control->lpVtbl->GetLogFile(control, buffer, sizeof(buffer), &size, &append);
    CHECK(hr == S_OK && !strcmp(buffer, "C:\\dbg.log") && !append && size == 11, "GetLogFile %#lx '%s' %lu", hr, buffer, size);
    hr = control->lpVtbl->GetLogFileWide(control, wbuffer, 256, &size, &append);
    CHECK(hr == S_OK && !wcscmp(wbuffer, L"C:\\dbg.log"), "GetLogFileWide %#lx", hr);
    control->lpVtbl->Output(control, DEBUG_OUTPUT_NORMAL, "logged-line %d\n", 5);
    hr = control->lpVtbl->CloseLogFile(control);
    CHECK(hr == S_OK, "CloseLogFile %#lx", hr);
    file = CreateFileA("C:\\dbg.log", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    memset(data, 0, sizeof(data));
    if (file != INVALID_HANDLE_VALUE) ReadFile(file, data, sizeof(data) - 1, &read, NULL);
    CHECK(strstr(data, "logged-line 5"), "log content '%s'", data);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    hr = control->lpVtbl->GetLogFile2(control, buffer, sizeof(buffer), &size, &a);
    CHECK(hr == E_NOINTERFACE, "GetLogFile2 closed %#lx", hr);
    hr = control->lpVtbl->OpenLogFile2(control, "C:\\dbg2.log", DEBUG_LOG_APPEND);
    CHECK(hr == S_OK, "OpenLogFile2 %#lx", hr);
    hr = control->lpVtbl->GetLogFile2(control, buffer, sizeof(buffer), &size, &a);
    CHECK(hr == S_OK && a == DEBUG_LOG_APPEND, "GetLogFile2 %#lx flags %lu", hr, a);
    control->lpVtbl->CloseLogFile(control);

    /* text macros and replacements */
    hr = control->lpVtbl->SetTextMacro(control, 3, "macro three");
    CHECK(hr == S_OK, "SetTextMacro %#lx", hr);
    hr = control->lpVtbl->GetTextMacro(control, 3, buffer, sizeof(buffer), &size);
    CHECK(hr == S_OK && !strcmp(buffer, "macro three") && size == 12, "GetTextMacro '%s'", buffer);
    hr = control->lpVtbl->GetTextMacroWide(control, 3, wbuffer, 256, &size);
    CHECK(hr == S_OK && !wcscmp(wbuffer, L"macro three"), "GetTextMacroWide");
    hr = control->lpVtbl->SetTextMacro(control, 12, "x");
    CHECK(hr == E_INVALIDARG, "SetTextMacro slot 12 %#lx", hr);
    hr = control->lpVtbl->SetTextReplacement(control, "$myalias", "1+2");
    CHECK(hr == S_OK, "SetTextReplacement %#lx", hr);
    hr = control->lpVtbl->GetNumberTextReplacements(control, &a);
    CHECK(hr == S_OK && a == 1, "GetNumberTextReplacements %lu", a);
    {
        char dst[64];
        ULONG dsize;

        hr = control->lpVtbl->GetTextReplacement(control, "$myalias", 0, NULL, 0, NULL, dst, sizeof(dst), &dsize);
        CHECK(hr == S_OK && !strcmp(dst, "1+2"), "GetTextReplacement '%s'", dst);
    }
    clear_output();
    hr = control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "? $myalias", DEBUG_EXECUTE_DEFAULT);
    CHECK(hr == S_OK && strstr(ev.out, "Evaluate expression: 3 ="), "alias expansion in Execute: %#lx '%s'", hr, ev.out);
    control->lpVtbl->RemoveTextReplacements(control);
    control->lpVtbl->GetNumberTextReplacements(control, &a);
    CHECK(a == 0, "replacements removed %lu", a);

    /* event filters */
    hr = control->lpVtbl->GetNumberEventFilters(control, &a, &b, &c);
    CHECK(hr == S_OK && a == 10 && b > 5 && c == 0, "GetNumberEventFilters %lu %lu %lu", a, b, c);
    hr = control->lpVtbl->GetEventFilterText(control, 0, buffer, sizeof(buffer), NULL);
    CHECK(hr == S_OK && !strcmp(buffer, "Create thread"), "filter text '%s'", buffer);
    {
        DEBUG_SPECIFIC_FILTER_PARAMETERS sp[2];
        DEBUG_EXCEPTION_FILTER_PARAMETERS ex = { 0 };
        ULONG code = 0xc0000005;

        hr = control->lpVtbl->GetSpecificFilterParameters(control, 0, 2, sp);
        CHECK(hr == S_OK && sp[0].ExecutionOption == DEBUG_FILTER_IGNORE && sp[1].TextSize == 12, "specific params %#lx", hr);
        sp[0].ExecutionOption = DEBUG_FILTER_BREAK;
        hr = control->lpVtbl->SetSpecificFilterParameters(control, 0, 1, sp);
        CHECK(hr == S_OK, "SetSpecificFilterParameters %#lx", hr);
        control->lpVtbl->GetSpecificFilterParameters(control, 0, 1, sp);
        CHECK(sp[0].ExecutionOption == DEBUG_FILTER_BREAK, "filter changed");
        hr = control->lpVtbl->GetExceptionFilterParameters(control, 1, &code, 0, &ex);
        CHECK(hr == S_OK && ex.ExecutionOption == DEBUG_FILTER_BREAK && ex.ExceptionCode == 0xc0000005, "exception params %#lx", hr);
        ex.ExceptionCode = 0xe0000001;
        ex.ExecutionOption = DEBUG_FILTER_OUTPUT;
        hr = control->lpVtbl->SetExceptionFilterParameters(control, 1, &ex);
        CHECK(hr == S_OK, "add arbitrary exception %#lx", hr);
        control->lpVtbl->GetNumberEventFilters(control, &a, &b, &c);
        CHECK(c == 1, "arbitrary exceptions %lu", c);
        hr = control->lpVtbl->SetEventFilterCommand(control, 3, "g");
        CHECK(hr == S_OK, "SetEventFilterCommand");
        hr = control->lpVtbl->GetEventFilterCommand(control, 3, buffer, sizeof(buffer), &size);
        CHECK(hr == S_OK && !strcmp(buffer, "g") && size == 2, "GetEventFilterCommand '%s'", buffer);
        hr = control->lpVtbl->GetEventFilterCommandWide(control, 3, wbuffer, 256, &size);
        CHECK(hr == S_OK && !wcscmp(wbuffer, L"g"), "GetEventFilterCommandWide");
        hr = control->lpVtbl->SetSpecificFilterArgument(control, DEBUG_FILTER_LOAD_MODULE, "ntdll");
        CHECK(hr == S_OK, "SetSpecificFilterArgument %#lx", hr);
        hr = control->lpVtbl->GetSpecificFilterArgument(control, DEBUG_FILTER_LOAD_MODULE, buffer, sizeof(buffer), &size);
        CHECK(hr == S_OK && !strcmp(buffer, "ntdll"), "GetSpecificFilterArgument '%s'", buffer);
        hr = control->lpVtbl->SetSpecificFilterArgument(control, 0, "x");
        CHECK(hr == E_INVALIDARG, "argument on ct %#lx", hr);
        ex.ExecutionOption = DEBUG_FILTER_REMOVE;
        hr = control->lpVtbl->SetExceptionFilterParameters(control, 1, &ex);
        CHECK(hr == S_OK, "remove arbitrary exception %#lx", hr);
    }

    /* expressions */
    CHECK(eval(control, "1+2*3") == 7, "1+2*3");
    CHECK(eval(control, "0n10+1") == 11, "0n10+1");
    CHECK(eval(control, "(10 + 20) / 4") == 0x30 / 4, "parenthesis");
    CHECK(eval(control, "0x10 << 4") == 0x100, "shift");
    CHECK(eval(control, "ff & 0f") == 0xf, "and");
    CHECK(eval(control, "not_a_symbol_xyz") == ~0ull, "unknown symbol fails");
    hr = control->lpVtbl->Evaluate(control, "5 junk", DEBUG_VALUE_INT64, &v, &a);
    CHECK(hr == S_OK && v.I64 == 5 && a == 2, "remainder index %#lx %lu", hr, a);
    hr = control->lpVtbl->Evaluate(control, "1234", DEBUG_VALUE_INT8, &v, NULL);
    CHECK(hr == S_OK && v.Type == DEBUG_VALUE_INT8 && v.I8 == 0x34, "coerced to int8");
    memset(&v, 0, sizeof(v));
    v.I32 = 100;
    v.Type = DEBUG_VALUE_INT32;
    hr = control->lpVtbl->CoerceValue(control, v, DEBUG_VALUE_FLOAT64, &out);
    CHECK(hr == S_OK && out.Type == DEBUG_VALUE_FLOAT64 && out.F64 == 100.0, "coerce int32 to float64");
    hr = control->lpVtbl->CoerceValue(control, out, DEBUG_VALUE_INT16, &v);
    CHECK(hr == S_OK && v.I16 == 100, "coerce float64 to int16");
    hr = control->lpVtbl->SetRadix(control, 10);
    CHECK(hr == S_OK, "SetRadix");
    CHECK(eval(control, "10+1") == 11, "decimal radix");
    control->lpVtbl->SetRadix(control, 16);
    hr = control->lpVtbl->EvaluateWide(control, L"3+4", DEBUG_VALUE_INT64, &v, NULL);
    CHECK(hr == S_OK && v.I64 == 7, "EvaluateWide");

    /* commands */
    clear_output();
    hr = control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, ".echo hello world", DEBUG_EXECUTE_DEFAULT);
    CHECK(hr == S_OK && !strcmp(ev.out, "hello world\n"), ".echo: '%s'", ev.out);
    clear_output();
    hr = control->lpVtbl->ExecuteWide(control, DEBUG_OUTCTL_ALL_CLIENTS, L"? 1+1; .echo two", 0);
    CHECK(hr == S_OK && strstr(ev.out, "Evaluate expression: 2 =") && strstr(ev.out, "two"), "ExecuteWide: '%s'", ev.out);
    clear_output();
    hr = control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "nosuchcommand", 0);
    CHECK(FAILED(hr), "unknown command fails %#lx", hr);
    clear_output();
    control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "as myname some text", 0);
    control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "al", 0);
    CHECK(strstr(ev.out, "myname"), "al lists alias: '%s'", ev.out);
    clear_output();
    control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "sx", 0);
    CHECK(strstr(ev.out, "av") && strstr(ev.out, "Access violation"), "sx lists filters");

    /* misc state */
    hr = control->lpVtbl->GetPromptText(control, buffer, sizeof(buffer), &size);
    CHECK(hr == S_OK && !strcmp(buffer, "NoTarget> "), "prompt '%s'", buffer);
    hr = control->lpVtbl->GetCurrentTimeDate(control, &a);
    CHECK(hr == S_OK && a > 1700000000, "time date %lu", a);
    hr = control->lpVtbl->GetCurrentSystemUpTime(control, &a);
    CHECK(hr == S_OK, "uptime");
    hr = control->lpVtbl->SetSystemErrorControl(control, 2, 1);
    CHECK(hr == S_OK, "SetSystemErrorControl");
    hr = control->lpVtbl->GetSystemErrorControl(control, &a, &b);
    CHECK(hr == S_OK && a == 2 && b == 1, "GetSystemErrorControl %lu %lu", a, b);
    hr = control->lpVtbl->SetSystemErrorControl(control, 9, 1);
    CHECK(hr == E_INVALIDARG, "bad level");
    hr = control->lpVtbl->GetSystemVersion(control, &a, &b, &c, buffer, sizeof(buffer), NULL, &i, buffer + 100, 100, NULL);
    CHECK(hr == S_OK && a == VER_PLATFORM_WIN32_NT && b >= 6, "GetSystemVersion %lu %lu", a, b);
    hr = control->lpVtbl->SetExpressionSyntaxByName(control, "C++");
    CHECK(hr == S_OK, "SetExpressionSyntaxByName");
    control->lpVtbl->GetExpressionSyntax(control, &a);
    CHECK(a == DEBUG_EXPR_CPLUSPLUS, "syntax");
    control->lpVtbl->SetExpressionSyntaxByName(control, "MASM");
    hr = control->lpVtbl->SetExpressionSyntaxByName(control, "nonsense");
    CHECK(hr == E_INVALIDARG, "bad syntax name");
    {
        ULONG64 handle1, handle2;
        IDebugClient *base = NULL;

        hr = client->lpVtbl->PushOutputLinePrefix(client, "> ", &handle1);
        CHECK(hr == S_OK, "PushOutputLinePrefix");
        client->lpVtbl->GetOutputLinePrefix(client, buffer, sizeof(buffer), NULL);
        CHECK(!strcmp(buffer, "> "), "prefix '%s'", buffer);
        client->lpVtbl->PushOutputLinePrefix(client, "# ", &handle2);
        hr = client->lpVtbl->PopOutputLinePrefix(client, handle1);
        CHECK(hr == S_OK, "PopOutputLinePrefix");
        client->lpVtbl->GetOutputLinePrefix(client, buffer, sizeof(buffer), NULL);
        CHECK(buffer[0] == 0, "prefix restored '%s'", buffer);
        (void)base;
    }
}

static void test_live(IDebugClient7 *client, IDebugControl4 *control, IDebugSymbols3 *symbols,
        IDebugDataSpaces *dataspaces, IDebugSystemObjects *sysobj, IDebugAdvanced3 *advanced)
{
    char path[MAX_PATH + 32], buffer[256];
    ULONG64 offset, disp, ntdll_base, target_addr, probe_base;
    WCHAR wbuffer[256];
    ULONG status, a, b, size, id;
    HRESULT hr;
    DEBUG_STACK_FRAME frames[16];
    IDebugBreakpoint *bp;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    strcat(path, " child");
    hr = control->lpVtbl->AddEngineOptions(control, DEBUG_ENGOPT_INITIAL_BREAK);
    CHECK(hr == S_OK, "AddEngineOptions");
    hr = client->lpVtbl->CreateProcess(client, 0, path, DEBUG_ONLY_THIS_PROCESS);
    CHECK(hr == S_OK, "CreateProcess %#lx", hr);
    if (FAILED(hr))
        return;
    hr = control->lpVtbl->GetExecutionStatus(control, &status);
    CHECK(hr == S_OK, "GetExecutionStatus before wait %#lx", hr);

    /* the first event is the one for the initial breakpoint, after the create and load events */
    hr = run_to_event(control, 30000);
    CHECK(hr == S_OK, "WaitForEvent %#lx", hr);
    CHECK(ev.create == 1 && ev.load >= 2, "events: create %d load %d", ev.create, ev.load);
    hr = control->lpVtbl->GetExecutionStatus(control, &status);
    CHECK(hr == S_OK && status == DEBUG_STATUS_BREAK, "status after break %lu", status);
    {
        ULONG type, pid, tid, used;

        hr = control->lpVtbl->GetLastEventInformation(control, &type, &pid, &tid, NULL, 0, &used, buffer, sizeof(buffer), NULL);
        CHECK(hr == S_OK && type == DEBUG_EVENT_EXCEPTION, "last event %#lx type %#lx '%s'", hr, type, buffer);
    }
    hr = sysobj->lpVtbl->GetNumberThreads(sysobj, &a);
    CHECK(hr == S_OK && a >= 1, "threads %lu", a);

    /* symbols */
    hr = symbols->lpVtbl->GetModuleByModuleName(symbols, "ntdll", 0, &id, &ntdll_base);
    CHECK(hr == S_OK && ntdll_base, "ntdll module");
    CHECK(eval(control, "ntdll") == ntdll_base, "module name as expression");
    hr = symbols->lpVtbl->GetOffsetByName(symbols, "ntdll!NtClose", &offset);
    CHECK(hr == S_OK && offset > ntdll_base, "GetOffsetByName NtClose %#lx", hr);
    hr = symbols->lpVtbl->GetNameByOffset(symbols, offset, buffer, sizeof(buffer), &size, &disp);
    CHECK(hr == S_OK && (!strcmp(buffer, "ntdll!NtClose") || !strcmp(buffer, "ntdll!ZwClose")) && !disp && size == 14, "GetNameByOffset '%s' %I64u", buffer, disp);
    hr = symbols->lpVtbl->GetNameByOffset(symbols, offset + 3, buffer, sizeof(buffer), &size, &disp);
    CHECK(hr == S_OK && (!strcmp(buffer, "ntdll!NtClose") || !strcmp(buffer, "ntdll!ZwClose")) && disp == 3, "displacement %I64u", disp);
    hr = symbols->lpVtbl->GetNameByOffsetWide(symbols, offset, wbuffer, 256, &size, &disp);
    CHECK(hr == S_OK && (!wcscmp(wbuffer, L"ntdll!NtClose") || !wcscmp(wbuffer, L"ntdll!ZwClose")), "GetNameByOffsetWide");
    hr = symbols->lpVtbl->GetOffsetByNameWide(symbols, L"ntdll!NtClose", &disp);
    CHECK(hr == S_OK && disp == offset, "GetOffsetByNameWide");
    hr = symbols->lpVtbl->GetNearNameByOffset(symbols, offset, 1, buffer, sizeof(buffer), NULL, &disp);
    CHECK(hr == S_OK && strncmp(buffer, "ntdll!NtClose", 13) && strncmp(buffer, "ntdll!ZwClose", 13), "near name '%s'", buffer);
    CHECK(eval(control, "ntdll!NtClose+10") == offset + 0x10, "symbol arithmetic");
    hr = symbols->lpVtbl->GetSymbolModule(symbols, "ntdll!NtClose", &disp);
    CHECK(hr == S_OK && disp == ntdll_base, "GetSymbolModule");
    {
        ULONG64 handle;
        ULONG64 match_offset;

        hr = symbols->lpVtbl->StartSymbolMatch(symbols, "ntdll!NtClos*", &handle);
        CHECK(hr == S_OK, "StartSymbolMatch %#lx", hr);
        hr = symbols->lpVtbl->GetNextSymbolMatch(symbols, handle, buffer, sizeof(buffer), NULL, &match_offset);
        CHECK(hr == S_OK && !strncmp(buffer, "ntdll!NtClos", 12), "GetNextSymbolMatch '%s'", buffer);
        symbols->lpVtbl->EndSymbolMatch(symbols, handle);
    }
    hr = symbols->lpVtbl->GetOffsetByName(symbols, "probe!target_function", &target_addr);
    if (FAILED(hr))
        hr = symbols->lpVtbl->GetOffsetByName(symbols, "target_function", &target_addr);
    CHECK(hr == S_OK, "target_function symbol %#lx", hr);
    hr = symbols->lpVtbl->GetModuleByModuleName(symbols, "probe", 0, &id, &probe_base);
    CHECK(hr == S_OK, "probe module");

    /* registers, context, stack, disassembly */
    {
        CONTEXT ctx;
        char text[256];
        ULONG used;
        ULONG64 end;

        memset(&ctx, 0, sizeof(ctx));
        hr = advanced->lpVtbl->GetThreadContext(advanced, &ctx, sizeof(ctx));
        CHECK(hr == S_OK, "GetThreadContext %#lx", hr);
#ifdef _WIN64
        CHECK(eval(control, "@rip") == ctx.Rip && ctx.Rip, "rip evaluates");
        CHECK(eval(control, "$ip") == ctx.Rip, "$ip evaluates");
        CHECK(eval(control, "@eax") == (ctx.Rax & 0xffffffff), "eax evaluates");
#else
        CHECK(eval(control, "@eip") == ctx.Eip && ctx.Eip, "eip evaluates");
        CHECK(eval(control, "@eax") == ctx.Eax, "eax evaluates");
#endif
        hr = control->lpVtbl->GetStackTrace(control, 0, 0, 0, frames, 16, &a);
        CHECK(hr == S_OK && a >= 1, "stack trace %#lx %lu", hr, a);
        hr = control->lpVtbl->Disassemble(control, offset, 0, text, sizeof(text), &used, &end);
        CHECK(hr == S_OK && end > offset && strlen(text) > 12, "Disassemble: %#lx '%s'", hr, text);
        hr = control->lpVtbl->DisassembleWide(control, offset, 0, wbuffer, 256, &used, &end);
        CHECK(hr == S_OK && wcslen(wbuffer) > 12, "DisassembleWide");
        hr = control->lpVtbl->GetNearInstruction(control, offset, 2, &end);
        CHECK(hr == S_OK && end > offset && end < offset + 32, "GetNearInstruction");
        clear_output();
        control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "u ntdll!NtClose L2", 0);
        CHECK(strstr(ev.out, "ntdll!NtClose:") || strstr(ev.out, "ntdll!ZwClose:"), "u output with symbol: '%s'", ev.out);
        clear_output();
        control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "r", 0);
#ifdef _WIN64
        CHECK(strstr(ev.out, "rip="), "r output");
#else
        CHECK(strstr(ev.out, "eip="), "r output");
#endif
        clear_output();
        control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "lm", 0);
        CHECK(strstr(ev.out, "ntdll") && strstr(ev.out, "probe"), "lm output");
        clear_output();
        control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "k", 0);
        CHECK(strlen(ev.out) > 20, "k output '%s'", ev.out);
        clear_output();
        control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "db ntdll L10", 0);
        CHECK(strstr(ev.out, "4d 5a"), "db output '%s'", ev.out);
        clear_output();
        control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "ln ntdll!NtClose+2", 0);
        CHECK(strstr(ev.out, "ntdll!NtClose+0x2") || strstr(ev.out, "ntdll!ZwClose+0x2"), "ln output '%s'", ev.out);
        clear_output();
        control->lpVtbl->Execute(control, DEBUG_OUTCTL_ALL_CLIENTS, "x ntdll!NtClos*", 0);
        CHECK(strstr(ev.out, "ntdll!NtClose"), "x output");
    }

    /* data and types (the debug info of the main image may be absent) */
    {
        ULONG type_id = 0, field_off = 0, tsize = 0;
        ULONG64 g_sample_addr = 0;

        hr = symbols->lpVtbl->GetOffsetByName(symbols, "probe!g_sample", &g_sample_addr);
        CHECK(hr == S_OK, "g_sample symbol %#lx", hr);
        if (SUCCEEDED(hr))
        {
            struct sample s = { 0 };
            ULONG got;

            hr = dataspaces->lpVtbl->ReadVirtual(dataspaces, g_sample_addr, &s, sizeof(s), &got);
            CHECK(hr == S_OK && s.a == 0x1234 && !strcmp(s.name, "wine"), "g_sample contents");
            hr = symbols->lpVtbl->GetTypeId(symbols, probe_base, "sample", &type_id);
            if (SUCCEEDED(hr))
            {
                hr = symbols->lpVtbl->GetTypeSize(symbols, probe_base, type_id, &tsize);
                CHECK(hr == S_OK && tsize == sizeof(struct sample), "type size %lu", tsize);
                hr = symbols->lpVtbl->GetFieldOffset(symbols, probe_base, type_id, "p", &field_off);
                CHECK(hr == S_OK && field_off == offsetof(struct sample, p), "field offset %lu", field_off);
                hr = symbols->lpVtbl->GetTypeName(symbols, probe_base, type_id, buffer, sizeof(buffer), NULL);
                CHECK(hr == S_OK && strstr(buffer, "sample"), "type name '%s'", buffer);
                memset(&s, 0, sizeof(s));
                hr = symbols->lpVtbl->ReadTypedDataVirtual(symbols, g_sample_addr, probe_base, type_id, &s, sizeof(s), &got);
                CHECK(hr == S_OK && s.a == 0x1234 && s.s == 9, "typed read");
                clear_output();
                hr = symbols->lpVtbl->OutputTypedDataVirtual(symbols, 0, g_sample_addr, probe_base, type_id, 0);
                CHECK(hr == S_OK && strstr(ev.out, "+0x000 a") && strstr(ev.out, "0n4660"), "typed output '%s'", ev.out);
                {
                    IDebugSymbolGroup *group;
                    ULONG n;

                    hr = symbols->lpVtbl->CreateSymbolGroup(symbols, &group);
                    CHECK(hr == S_OK, "CreateSymbolGroup");
                    if (SUCCEEDED(hr))
                    {
                        n = DEBUG_ANY_ID;
                        hr = group->lpVtbl->AddSymbol(group, "probe!g_sample", &n);
                        CHECK(hr == S_OK && n == 0, "group AddSymbol %#lx", hr);
                        hr = group->lpVtbl->GetNumberSymbols(group, &n);
                        CHECK(hr == S_OK && n == 1, "one symbol");
                        hr = group->lpVtbl->ExpandSymbol(group, 0, TRUE);
                        group->lpVtbl->GetNumberSymbols(group, &n);
                        CHECK(hr == S_OK && n == 5, "expanded to %lu", n);
                        group->lpVtbl->GetSymbolName(group, 2, buffer, sizeof(buffer), NULL);
                        CHECK(!strcmp(buffer, "name"), "member name '%s'", buffer);
                        group->lpVtbl->Release(group);
                    }
                }
            }
            else
                printf("note  no type information for the main image (%#lx)\n", hr);
        }
    }

    /* a breakpoint in the debuggee */
    hr = control->lpVtbl->AddBreakpoint(control, DEBUG_BREAKPOINT_CODE, DEBUG_ANY_ID, &bp);
    CHECK(hr == S_OK, "AddBreakpoint %#lx", hr);
    if (SUCCEEDED(hr))
    {
        ULONG flags, count;

        hr = bp->lpVtbl->GetOffset(bp, &offset);
        CHECK(hr == E_NOINTERFACE, "offset not set %#lx", hr);
        hr = bp->lpVtbl->SetOffset(bp, target_addr);
        CHECK(hr == S_OK, "SetOffset");
        hr = bp->lpVtbl->AddFlags(bp, DEBUG_BREAKPOINT_ENABLED);
        CHECK(hr == S_OK, "AddFlags");
        bp->lpVtbl->GetFlags(bp, &flags);
        CHECK(flags == DEBUG_BREAKPOINT_ENABLED, "flags %#lx", flags);
        hr = control->lpVtbl->GetNumberBreakpoints(control, &count);
        CHECK(hr == S_OK && count == 1, "breakpoint count %lu", count);
        hr = control->lpVtbl->AddBreakpoint(control, DEBUG_BREAKPOINT_CODE, 0, &bp);
        CHECK(hr == E_INVALIDARG, "duplicate id");
        control->lpVtbl->GetBreakpointByIndex(control, 0, &bp);
        hr = bp->lpVtbl->SetCommand(bp, ".echo hit");
        CHECK(hr == S_OK, "SetCommand");
        hr = bp->lpVtbl->GetCommand(bp, buffer, sizeof(buffer), &size);
        CHECK(hr == S_OK && !strcmp(buffer, ".echo hit") && size == 10, "GetCommand");
        bp->lpVtbl->SetCommand(bp, NULL);
    }
    clear_output();
    hr = control->lpVtbl->SetExecutionStatus(control, DEBUG_STATUS_GO);
    CHECK(hr == S_OK, "SetExecutionStatus GO");
    hr = run_to_event(control, 30000);
    CHECK(hr == S_OK, "WaitForEvent for the breakpoint %#lx", hr);
    {
        struct { EXCEPTION_RECORD64 rec; ULONG first; } info;

        memset(&info, 0, sizeof(info));
        control->lpVtbl->GetLastEventInformation(control, NULL, NULL, NULL, &info, sizeof(info), NULL, buffer, sizeof(buffer), NULL);
        printf("note  last event: %s at %I64x (target %I64x)\n", buffer, info.rec.ExceptionAddress, target_addr);
    }
    CHECK(ev.breakpoint == 1, "breakpoint callback %d", ev.breakpoint);
    CHECK(eval(control, "$ip") == target_addr, "stopped at target_function");
#ifdef _WIN64
    CHECK(eval(control, "@rcx") == 42, "argument in rcx");
#else
    CHECK(eval(control, "poi(@esp+4)") == 42, "argument on the stack");
#endif
    hr = control->lpVtbl->GetStackTrace(control, 0, 0, 0, frames, 16, &a);
    CHECK(hr == S_OK && a >= 2 && frames[0].InstructionOffset == target_addr, "stack at breakpoint %lu", a);
    /* data before and after */
    {
        int result = -1;

        dataspaces->lpVtbl->ReadVirtual(dataspaces, 0, &result, 0, NULL);
        hr = symbols->lpVtbl->GetOffsetByName(symbols, "probe!g_result", &offset);
        if (SUCCEEDED(hr))
        {
            dataspaces->lpVtbl->ReadVirtual(dataspaces, offset, &result, sizeof(result), NULL);
            CHECK(result == 0, "g_result before the call");
        }
    }
    CHECK(strstr(ev.out, "child says hi"), "debuggee output reached the output callbacks: '%s'", ev.out);
    {
        IDebugSymbolGroup *group = NULL;
        ULONG64 ip = 0;
        DEBUG_STACK_FRAME frame;

        hr = symbols->lpVtbl->GetScope(symbols, &ip, &frame, NULL, 0);
        CHECK(hr == S_OK && ip == target_addr && frame.InstructionOffset == target_addr, "GetScope %#lx %I64x", hr, ip);
        hr = symbols->lpVtbl->GetScopeSymbolGroup(symbols, 3, NULL, &group);
        CHECK(hr == S_OK && group, "GetScopeSymbolGroup %#lx", hr);
        if (group)
        {
            hr = group->lpVtbl->GetNumberSymbols(group, &a);
            CHECK(hr == S_OK, "scope group symbols %lu", a);
            group->lpVtbl->Release(group);
        }
        hr = symbols->lpVtbl->SetScope(symbols, target_addr + 4, NULL, NULL, 0);
        CHECK(hr == S_OK, "SetScope");
        symbols->lpVtbl->GetScope(symbols, &ip, NULL, NULL, 0);
        CHECK(ip == target_addr + 4, "scope set");
        symbols->lpVtbl->ResetScope(symbols);
        symbols->lpVtbl->GetScope(symbols, &ip, NULL, NULL, 0);
        CHECK(ip == target_addr, "scope reset");
    }
    {
        HANDLE f;
        DWORD fsize = 0;

        DeleteFileA("C:\\test.dmp");
        hr = client->lpVtbl->WriteDumpFile(client, "C:\\test.dmp", DEBUG_DUMP_SMALL);
        CHECK(hr == S_OK, "WriteDumpFile %#lx", hr);
        f = CreateFileA("C:\\test.dmp", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f != INVALID_HANDLE_VALUE)
        {
            fsize = GetFileSize(f, NULL);
            CloseHandle(f);
        }
        CHECK(fsize > 100, "dump file size %lu", fsize);
        DeleteFileA("C:\\test.dmp");
    }
    /* step: the next event is the single step exception */
    hr = control->lpVtbl->SetExecutionStatus(control, DEBUG_STATUS_STEP_INTO);
    CHECK(hr == S_OK, "SetExecutionStatus STEP_INTO");
    ev.exception = 0;
    hr = run_to_event(control, 30000);
    CHECK(hr == S_OK && ev.exception == 1, "step event %#lx %d", hr, ev.exception);
    {
        ULONG64 ip = eval(control, "$ip");

        CHECK(ip > target_addr && ip < target_addr + 16, "single step moved to %I64x", ip);
    }
    /* remove the breakpoint and run to the end */
    hr = control->lpVtbl->GetBreakpointByIndex(control, 0, &bp);
    hr = control->lpVtbl->RemoveBreakpoint(control, bp);
    CHECK(hr == S_OK, "RemoveBreakpoint");
    hr = control->lpVtbl->SetExecutionStatus(control, DEBUG_STATUS_GO);
    hr = run_to_event(control, 30000);
    CHECK(hr == S_OK, "WaitForEvent to the end %#lx", hr);
    hr = control->lpVtbl->GetExecutionStatus(control, &status);
    CHECK(hr == S_OK && status == DEBUG_STATUS_NO_DEBUGGEE, "status at the end %lu", status);
    CHECK(ev.exit_proc == 1 && ev.exit_code == 7, "exit callback %d code %d", ev.exit_proc, ev.exit_code);
    hr = run_to_event(control, 1000);
    CHECK(hr == E_UNEXPECTED, "WaitForEvent after the end %#lx", hr);
    client->lpVtbl->EndSession(client, DEBUG_END_ACTIVE_TERMINATE);
}

static void test_attach(IDebugClient7 *client, IDebugControl4 *control, IDebugSystemObjects *sysobj)
{
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { sizeof(si) };
    char path[MAX_PATH + 32];
    ULONG status, a, exit_code;
    HRESULT hr;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    strcat(path, " sleep");
    CHECK(CreateProcessA(NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi), "create sleeping child");
    Sleep(1500);
    memset(&ev, 0, sizeof(ev));
    hr = client->lpVtbl->AttachProcess(client, 0, pi.dwProcessId, DEBUG_ATTACH_DEFAULT);
    CHECK(hr == S_OK, "AttachProcess %#lx", hr);
    hr = run_to_event(control, 30000);
    CHECK(hr == S_OK, "WaitForEvent after attach %#lx", hr);
    CHECK(ev.create == 1 && ev.load >= 2, "attach events: create %d load %d", ev.create, ev.load);
    hr = control->lpVtbl->GetExecutionStatus(control, &status);
    CHECK(hr == S_OK && status == DEBUG_STATUS_BREAK, "status %lu", status);
    hr = sysobj->lpVtbl->GetCurrentProcessSystemId(sysobj, &a);
    CHECK(hr == S_OK && a == pi.dwProcessId, "process id %lu", a);
    hr = sysobj->lpVtbl->GetCurrentProcessId(sysobj, &a);
    CHECK(hr == S_OK && a == 0, "engine process id %lu", a);
    hr = control->lpVtbl->GetNumberBreakpoints(control, &a);
    CHECK(hr == S_OK && a == 0, "no breakpoints");
    hr = client->lpVtbl->TerminateProcesses(client);
    CHECK(hr == S_OK, "TerminateProcesses %#lx", hr);
    WaitForSingleObject(pi.hProcess, 10000);
    GetExitCodeProcess(pi.hProcess, (DWORD *)&exit_code);
    CHECK(exit_code != STILL_ACTIVE, "child ended");
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    client->lpVtbl->EndSession(client, DEBUG_END_PASSIVE);
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

    test_static(client, control);
    test_live(client, control, symbols, dataspaces, sysobj, advanced);
    test_attach(client, control, sysobj);

    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
