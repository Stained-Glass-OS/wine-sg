/* noexcept-gate.sh's probe (0510): vcruntime140's __C_specific_handler_noexcept.
 *   probe unwind : an unwind passing the frame continues (no scope of ours)
 *   probe escape : a C++ exception leaving the noexcept function ends the process
 *   probe seh    : an SEH exception (not C++) passes the frame: the search goes on (0535) */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef EXCEPTION_DISPOSITION (WINAPI *handler_fn)(EXCEPTION_RECORD *, void *, CONTEXT *, DISPATCHER_CONTEXT *);

int main(int argc, char **argv)
{
    HMODULE vcr = LoadLibraryA("vcruntime140.dll");
    handler_fn handler = vcr ? (handler_fn)GetProcAddress(vcr, "__C_specific_handler_noexcept") : NULL;
    static DWORD empty_scope_table[2];   /* Count = 0: no __try scope here */
    EXCEPTION_RECORD rec;
    DISPATCHER_CONTEXT dispatch;
    CONTEXT ctx;
    EXCEPTION_DISPOSITION ret;

    if (!handler) { printf("missing\n"); return 1; }
    memset(&rec, 0, sizeof(rec));
    memset(&dispatch, 0, sizeof(dispatch));
    memset(&ctx, 0, sizeof(ctx));
    rec.ExceptionCode = 0xe06d7363;
    dispatch.ImageBase = (ULONG64)GetModuleHandleA(NULL);
    dispatch.ControlPc = dispatch.ImageBase + 0x1000;
    dispatch.HandlerData = empty_scope_table;
    dispatch.ContextRecord = &ctx;

    if (argc > 1 && !strcmp(argv[1], "unwind"))
    {
        rec.ExceptionFlags = EXCEPTION_UNWINDING;
        ret = handler(&rec, (void *)0x1000, &ctx, &dispatch);
        printf("unwind %d\n", ret);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "seh"))
    {
        rec.ExceptionCode = 0xe0000002;   /* a program's own SEH code */
        ret = handler(&rec, (void *)0x1000, &ctx, &dispatch);
        printf("seh %d\n", ret);
        return 0;
    }
    ret = handler(&rec, (void *)0x1000, &ctx, &dispatch);
    printf("escape returned %d\n", ret);   /* must not be reached */
    return 0;
}
