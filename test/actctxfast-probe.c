/* RtlActivateActivationContextUnsafeFast / DeactivateUnsafeFast and
 * RtlQueryInformationActiveActivationContext (patches/sg/2237). */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

struct fakeframe { SIZE_T size; ULONG format; void *previous; void *ctx; ULONG flags; void *extra[3]; };
typedef void *(__fastcall *activate_t)(struct fakeframe *, void *);
typedef void *(__fastcall *deactivate_t)(struct fakeframe *);
typedef LONG (WINAPI *query_t)(ULONG, void *, SIZE_T, SIZE_T *);

static const char manifest[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n"
"<assemblyIdentity version=\"1.0.0.0\" name=\"sg.test.fast\" type=\"win32\"/>\n"
"</assembly>\n";

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    activate_t activate = (void *)GetProcAddress(ntdll, "RtlActivateActivationContextUnsafeFast");
    deactivate_t deactivate = (void *)GetProcAddress(ntdll, "RtlDeactivateActivationContextUnsafeFast");
    query_t query = (void *)GetProcAddress(ntdll, "RtlQueryInformationActiveActivationContext");
    char path[MAX_PATH];
    ACTCTXA act = { sizeof(act) };
    HANDLE ctx, cur;
    HANDLE f;
    DWORD n;
    struct fakeframe frame;
    ULONG_PTR cookie;
    void *ret;
    ACTIVATION_CONTEXT_BASIC_INFORMATION info;
    SIZE_T len;
    LONG st;

    CHECK(activate && deactivate && query, "exports %p %p %p", activate, deactivate, query);
    if (!activate || !deactivate || !query) { printf("RESULT: FAIL\n"); return 1; }

    GetTempPathA(MAX_PATH, path);
    strcat(path, "sg-actctxfast.manifest");
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, manifest, sizeof(manifest) - 1, &n, NULL);
    CloseHandle(f);
    act.lpSource = path;
    ctx = CreateActCtxA(&act);
    DeleteFileA(path);
    CHECK(ctx != INVALID_HANDLE_VALUE, "CreateActCtx %lu", GetLastError());

    GetCurrentActCtx(&cur);
    CHECK(cur == NULL, "no active context to start with: %p", cur);

    memset(&frame, 0xcc, sizeof(frame));
    ret = activate(&frame, ctx);
    CHECK(ret == &frame.previous, "returns the embedded frame: %p / %p", ret, &frame.previous);
    CHECK(frame.size == sizeof(frame) && frame.format == 1, "size %Iu format %lu", frame.size, frame.format);
    CHECK(frame.ctx == ctx && frame.previous == NULL && frame.flags == 0, "ctx %p prev %p flags %lx", frame.ctx, frame.previous, frame.flags);
    ret = NULL;
    GetCurrentActCtx(&ret);
    CHECK(ret == ctx, "active context %p / %p", ret, ctx);
    if (ret) ReleaseActCtx(ret);

    memset(&info, 0, sizeof(info));
    len = 0xdead;
    st = query(ActivationContextBasicInformation, &info, sizeof(info), &len);
    CHECK(!st && info.hActCtx == ctx && len == sizeof(info), "query st %#lx ctx %p len %Iu", st, info.hActCtx, len);
    if (!st && info.hActCtx) ReleaseActCtx(info.hActCtx);
    len = 0xdead;
    st = query(ActivationContextBasicInformation, NULL, 0, &len);
    CHECK(st == (LONG)0xC0000023 && len == sizeof(info), "small buffer st %#lx len %Iu", st, len);

    ret = deactivate(&frame);
    CHECK(ret == NULL, "deactivate returns the previous frame: %p", ret);
    ret = (void *)1;
    GetCurrentActCtx(&ret);
    CHECK(ret == NULL, "nothing active again: %p", ret);

    /* popping a caller frame through a forced early deactivation of a frame below it must not free it */
    ActivateActCtx(ctx, &cookie);
    memset(&frame, 0xcc, sizeof(frame));
    activate(&frame, ctx);
    ret = NULL;
    GetCurrentActCtx(&ret);
    CHECK(ret == ctx, "stacked: active %p", ret);
    if (ret) ReleaseActCtx(ret);
    CHECK(DeactivateActCtx(DEACTIVATE_ACTCTX_FLAG_FORCE_EARLY_DEACTIVATION, cookie), "force early deactivation %lu", GetLastError());
    ret = (void *)1;
    GetCurrentActCtx(&ret);
    CHECK(ret == NULL, "stack empty: %p", ret);
    CHECK(frame.size == sizeof(frame) && frame.ctx == ctx, "caller frame untouched");
    /* the context lost no reference to the caller frame: it still works */
    {
        ACTIVATION_CONTEXT_BASIC_INFORMATION bi;
        SIZE_T rl = 0;
        memset(&bi, 0, sizeof(bi));
        ret = QueryActCtxW(QUERY_ACTCTX_FLAG_NO_ADDREF, ctx, NULL, ActivationContextBasicInformation, &bi, sizeof(bi), &rl) ? (void *)1 : NULL;
        CHECK(ret && bi.hActCtx == ctx, "context still valid after the pop (%lu)", GetLastError());
        ReleaseActCtx(ctx);
    }

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
