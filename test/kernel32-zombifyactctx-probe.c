/* Probe for patches/sg/5022: ZombifyActCtx (RtlZombifyActivationContext)
 * fails ERROR_INVALID_PARAMETER for NULL or a handle that is not an
 * activation context, and succeeds for a valid one - active or not, and
 * twice - which stays queryable and active (kernel32 actctx tests, run on
 * Windows). Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const char manifest[] =
    "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
    "<assemblyIdentity version=\"1.0.0.0\" name=\"Sg.Zombify\" type=\"win32\"/>"
    "</assembly>";

int main(void)
{
    char path[MAX_PATH];
    DWORD junk[16] = {0}, written;
    ACTCTXA ctx = {sizeof(ctx)};
    HANDLE handle, file, current;
    ULONG_PTR cookie;
    unsigned int i;
    BOOL ret;
    struct { const char *what; HANDLE h; } bad[] = {{"NULL", NULL}, {"not a context", junk}, {"INVALID_HANDLE_VALUE", INVALID_HANDLE_VALUE}};

    for (i = 0; i < ARRAYSIZE(bad); i++)
    {
        SetLastError(0xdeadbeef);
        ret = ZombifyActCtx(bad[i].h);
        check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "%s: %d %lu", bad[i].what, ret, GetLastError());
    }

    GetTempPathA(MAX_PATH, path);
    strcat(path, "sg-zombify.manifest");
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, manifest, sizeof(manifest) - 1, &written, NULL);
    CloseHandle(file);
    ctx.lpSource = path;
    handle = CreateActCtxA(&ctx);
    check(handle != INVALID_HANDLE_VALUE, "context created: %lu", handle == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (handle != INVALID_HANDLE_VALUE)
    {
        ret = ZombifyActCtx(handle);
        check(ret, "an inactive context: %d %lu", ret, ret ? 0 : GetLastError());
        ret = ActivateActCtx(handle, &cookie);
        check(ret, "activated after it");
        for (i = 0; i < 2; i++)
        {
            ret = ZombifyActCtx(handle);
            check(ret, "the active context, time %u: %d %lu", i + 1, ret, ret ? 0 : GetLastError());
        }
        current = NULL;
        GetCurrentActCtx(&current);
        check(current == handle, "still the active context (%p)", current);
        if (current) ReleaseActCtx(current);
        ret = DeactivateActCtx(0, cookie);
        check(ret, "deactivated");
        ReleaseActCtx(handle);
    }
    DeleteFileA(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
