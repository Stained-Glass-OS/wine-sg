/* Probe for patches/sg/5000: symbolic links in ntdll.
 * - a relative target reads back as it was written (FSCTL_GET_REPARSE_POINT
 *   and the link resolves), also when it climbs out of and back into the
 *   link's own folder;
 * - a directory symbolic link to a folder with files in it is removed by
 *   RemoveDirectoryW, the folder and its files stay (kernel32 volume test);
 * - a file symbolic link is removed by DeleteFileW, its target stays;
 * - a link to a folder held open without delete sharing (C:\\windows) is
 *   removed: the link is an object of its own.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

/* the substitute name of a symbolic link, and whether it is relative */
static BOOL read_link(const WCHAR *path, WCHAR *out, DWORD out_len, BOOL *relative)
{
    BYTE buf[2048];
    DWORD size;
    HANDLE h = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    BOOL ret;
    USHORT off, len;

    if (h == INVALID_HANDLE_VALUE) return FALSE;
    ret = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, sizeof(buf), &size, NULL);
    CloseHandle(h);
    if (!ret || *(DWORD *)buf != IO_REPARSE_TAG_SYMLINK) return FALSE;
    off = *(USHORT *)(buf + 8); len = *(USHORT *)(buf + 10);
    *relative = (*(ULONG *)(buf + 16) & 1) != 0;
    if (len / sizeof(WCHAR) >= out_len) return FALSE;
    memcpy(out, buf + 20 + off, len);
    out[len / sizeof(WCHAR)] = 0;
    return TRUE;
}

static BOOL make_file(const WCHAR *path, const char *text)
{
    DWORD written;
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    WriteFile(h, text, strlen(text), &written, NULL);
    CloseHandle(h);
    return TRUE;
}

static BOOL file_says(const WCHAR *path, const char *text)
{
    char buf[64] = {0};
    DWORD got = 0;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(h, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(h);
    return !strcmp(buf, text);
}

int main(int argc, char **argv)
{
    /* link name (in the base folder), target as written, the file the link reaches */
    static const struct { const WCHAR *link, *target, *reaches; } file_links[] =
    {
        {L"d\\l1", L"a.txt", L"a.txt"},
        {L"d\\l2", L"..\\d\\a.txt", L"a.txt"},             /* out of the link's folder and back in */
        {L"d\\l3", L"..\\other\\b.txt", L"b.txt"},
        {L"d\\l4", L"sub\\c.txt", L"c.txt"},
        {L"d\\l5", L"..\\..\\base\\d\\a.txt", L"a.txt"},
    };
    WCHAR base[MAX_PATH], path[MAX_PATH], got[MAX_PATH], cwd[MAX_PATH];
    BOOL relative;
    unsigned int i;
    BOOL ret;

    GetTempPathW(MAX_PATH, base);
    lstrcatW(base, L"base");
    CreateDirectoryW(base, NULL);
    GetCurrentDirectoryW(MAX_PATH, cwd);
    SetCurrentDirectoryW(base);
    CreateDirectoryW(L"d", NULL);
    CreateDirectoryW(L"other", NULL);
    CreateDirectoryW(L"d\\sub", NULL);
    make_file(L"d\\a.txt", "a");
    make_file(L"other\\b.txt", "b");
    make_file(L"d\\sub\\c.txt", "c");

    for (i = 0; i < ARRAYSIZE(file_links); i++)
    {
        DeleteFileW(file_links[i].link);
        ret = CreateSymbolicLinkW(file_links[i].link, file_links[i].target, SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
        check(ret, "link %u (%ls -> %ls) created: %lu", i, file_links[i].link, file_links[i].target, ret ? 0 : GetLastError());
        if (!ret) continue;
        got[0] = 0; relative = FALSE;
        ret = read_link(file_links[i].link, got, ARRAYSIZE(got), &relative);
        check(ret && relative && !lstrcmpiW(got, file_links[i].target), "link %u reads back \"%ls\" (relative %d)",
              i, got, relative);
        check(file_says(file_links[i].link, file_links[i].reaches[0] == L'a' ? "a" : file_links[i].reaches[0] == L'b' ? "b" : "c"),
              "link %u reaches %ls", i, file_links[i].reaches);
        ret = DeleteFileW(file_links[i].link);
        check(ret && GetFileAttributesW(file_links[i].link) == INVALID_FILE_ATTRIBUTES, "link %u deleted", i);
    }
    check(file_says(L"d\\a.txt", "a") && file_says(L"other\\b.txt", "b") && file_says(L"d\\sub\\c.txt", "c"),
          "the targets are still there");

    /* directory links: removed by RemoveDirectoryW, the folder and its files stay */
    {
        static const WCHAR *const targets[] = {L"d", L"..\\base\\d", NULL};
        for (i = 0; i < ARRAYSIZE(targets); i++)
        {
            const WCHAR *target = targets[i];
            WCHAR absolute[MAX_PATH];

            if (!target) { wsprintfW(absolute, L"%s\\d", base); target = absolute; }
            RemoveDirectoryW(L"dl");
            ret = CreateSymbolicLinkW(L"dl", target, SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
            check(ret, "directory link %u -> %ls created: %lu", i, target, ret ? 0 : GetLastError());
            if (!ret) continue;
            check(file_says(L"dl\\a.txt", "a"), "directory link %u reaches the folder's files", i);
            SetLastError(0xdeadbeef);
            ret = RemoveDirectoryW(L"dl");
            check(ret, "directory link %u removed by RemoveDirectoryW: %lu", i, ret ? 0 : GetLastError());
            check(GetFileAttributesW(L"dl") == INVALID_FILE_ATTRIBUTES, "directory link %u is gone", i);
            check(file_says(L"d\\a.txt", "a") && file_says(L"d\\sub\\c.txt", "c"), "directory link %u: the folder stays", i);
        }
    }

    /* a link to a folder other processes keep open without delete sharing */
    {
        HANDLE held = CreateFileW(L"C:\\windows", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS, NULL);
        RemoveDirectoryW(L"wl");
        ret = CreateSymbolicLinkW(L"wl", L"C:\\windows", SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
        check(ret, "link to C:\\windows created: %lu", ret ? 0 : GetLastError());
        if (ret)
        {
            SetLastError(0xdeadbeef);
            ret = RemoveDirectoryW(L"wl");
            check(ret, "link to a folder held open without delete sharing removed: %lu", ret ? 0 : GetLastError());
            check(GetFileAttributesW(L"wl") == INVALID_FILE_ATTRIBUTES, "and it is gone");
            check(GetFileAttributesW(L"C:\\windows\\system32") != INVALID_FILE_ATTRIBUTES, "C:\\windows is untouched");
        }
        if (held != INVALID_HANDLE_VALUE) CloseHandle(held);
    }

    SetCurrentDirectoryW(cwd);
    wsprintfW(path, L"%s\\d\\sub\\c.txt", base); DeleteFileW(path);
    wsprintfW(path, L"%s\\d\\sub", base); RemoveDirectoryW(path);
    wsprintfW(path, L"%s\\d\\a.txt", base); DeleteFileW(path);
    wsprintfW(path, L"%s\\d", base); RemoveDirectoryW(path);
    wsprintfW(path, L"%s\\other\\b.txt", base); DeleteFileW(path);
    wsprintfW(path, L"%s\\other", base); RemoveDirectoryW(path);
    RemoveDirectoryW(base);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
