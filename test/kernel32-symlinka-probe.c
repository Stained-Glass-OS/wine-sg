/* Probe for patches/sg/5001: CreateSymbolicLinkA creates the link (it
 * reported success and created nothing): file and directory links, relative
 * and absolute targets, and its failures. Table driven; lines "ok"/"FAIL",
 * last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    static const struct { const char *link, *target; DWORD flags; const char *through; } cases[] =
    {
        {"la1", "data.txt", 0, "la1"},
        {"la2", ".\\data.txt", 0, "la2"},
        {"dla", "dir", SYMBOLIC_LINK_FLAG_DIRECTORY, "dla\\inner.txt"},
    };
    char base[MAX_PATH], cwd[MAX_PATH], buf[16];
    unsigned int i;
    DWORD got;
    HANDLE h;
    BOOLEAN ret;

    GetTempPathA(MAX_PATH, base);
    strcat(base, "symlinka");
    CreateDirectoryA(base, NULL);
    GetCurrentDirectoryA(MAX_PATH, cwd);
    SetCurrentDirectoryA(base);
    CreateDirectoryA("dir", NULL);
    h = CreateFileA("data.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(h, "x", 1, &got, NULL); CloseHandle(h);
    h = CreateFileA("dir\\inner.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(h, "x", 1, &got, NULL); CloseHandle(h);

    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        DWORD attrs;

        ret = CreateSymbolicLinkA(cases[i].link, cases[i].target, cases[i].flags | 2 /* unprivileged */);
        check(ret, "CreateSymbolicLinkA(%s -> %s): %lu", cases[i].link, cases[i].target, ret ? 0 : GetLastError());
        attrs = GetFileAttributesA(cases[i].link);
        check(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT), "%s is a reparse point (%#lx)",
              cases[i].link, attrs);
        h = CreateFileA(cases[i].through, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        got = 0; memset(buf, 0, sizeof(buf));
        if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, sizeof(buf), &got, NULL); CloseHandle(h); }
        check(got == 1 && buf[0] == 'x', "the target is reached through %s", cases[i].through);
        if (cases[i].flags) RemoveDirectoryA(cases[i].link); else DeleteFileA(cases[i].link);
    }
    SetLastError(0xdeadbeef);
    ret = CreateSymbolicLinkA("nosuchdir\\l", "data.txt", 2);
    check(!ret && GetLastError() == ERROR_PATH_NOT_FOUND, "a link in a missing folder fails: %lu", GetLastError());

    DeleteFileA("dir\\inner.txt"); RemoveDirectoryA("dir"); DeleteFileA("data.txt");
    SetCurrentDirectoryA(cwd);
    RemoveDirectoryA(base);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
