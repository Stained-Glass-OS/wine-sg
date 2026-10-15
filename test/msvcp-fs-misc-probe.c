/* Probe for patches/sg/4555: msvcp140's (and msvcp_win's) experimental
 * filesystem helpers _Hard_links (link count, -1 on error), _Statvfs
 * (volume space against GetDiskFreeSpaceExW, all -1 on error), _Chmod (the
 * read-only attribute, 0 on success) and _Symlink_get (the text of a
 * symbolic link). Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { ULONGLONG capacity, free, available; } space_info;

int main(int argc, char **argv)
{
    ULONGLONG (__cdecl *p_links)(const WCHAR *);
    space_info *(__cdecl *p_statvfs)(space_info *, const WCHAR *);
    int (__cdecl *p_chmod)(const WCHAR *, int);
    WCHAR *(__cdecl *p_symlink_get)(WCHAR *, const WCHAR *);
    WCHAR dir[MAX_PATH], a[MAX_PATH], b[MAX_PATH], link[MAX_PATH], dlink[MAX_PATH], buf[MAX_PATH];
    HMODULE mod;
    HANDLE h;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_links = (void *)GetProcAddress(mod, "_Hard_links");
    p_statvfs = (void *)GetProcAddress(mod, "_Statvfs");
    p_chmod = (void *)GetProcAddress(mod, "_Chmod");
    p_symlink_get = (void *)GetProcAddress(mod, "_Symlink_get");
    check(p_links && p_statvfs && p_chmod && p_symlink_get, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    GetTempPathW(MAX_PATH, dir);
    wcscat(dir, L"fs-misc");
    CreateDirectoryW(dir, NULL);
    swprintf(a, MAX_PATH, L"%ls\\a.txt", dir);
    swprintf(b, MAX_PATH, L"%ls\\b.txt", dir);
    swprintf(link, MAX_PATH, L"%ls\\link", dir);
    /* Wine can't remove a directory link: a name of its own each run */
    swprintf(dlink, MAX_PATH, L"%ls\\dlink%lu", dir, GetTickCount());
    h = CreateFileW(a, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);

    /* _Hard_links */
    check(p_links(a) == 1, "_Hard_links(a) = %I64u", p_links(a));
    check(CreateHardLinkW(b, a, NULL), "CreateHardLinkW");
    check(p_links(a) == 2 && p_links(b) == 2, "_Hard_links after a second link: %I64u %I64u", p_links(a), p_links(b));
    check(p_links(L"C:\\no\\such\\file") == ~0ull, "_Hard_links(missing) = -1");
    check(p_links(dir) >= 1 && p_links(dir) != ~0ull, "_Hard_links(directory) = %I64u", p_links(dir));
    DeleteFileW(b);
    check(p_links(a) == 1, "_Hard_links after removing it: %I64u", p_links(a));

    /* _Statvfs */
    {
        static const WCHAR *const paths[] = {L"C:\\", NULL, L"C:\\windows"};
        ULARGE_INTEGER av, tot, fr;
        space_info si;

        for (i = 0; i < ARRAYSIZE(paths); i++)
        {
            const WCHAR *p = paths[i] ? paths[i] : dir;
            memset(&si, 0xcc, sizeof(si));
            check(p_statvfs(&si, p) == &si && GetDiskFreeSpaceExW(p, &av, &tot, &fr) && si.capacity == tot.QuadPart &&
                  si.available <= si.free + (1 << 30) && si.free <= si.capacity && si.capacity &&
                  (si.available >> 24) == (av.QuadPart >> 24) && (si.free >> 24) == (fr.QuadPart >> 24),
                  "_Statvfs(%ls): %I64u %I64u %I64u", p, si.capacity, si.free, si.available);
        }
        memset(&si, 0, sizeof(si));
        p_statvfs(&si, L"Q:\\no\\such\\volume");
        check(si.capacity == ~0ull && si.free == ~0ull && si.available == ~0ull, "_Statvfs(missing volume): all -1");
    }

    /* _Chmod */
    {
        static const struct { int perms; BOOL readonly; } modes[] =
        {
            {0444, TRUE}, {0666, FALSE}, {0400, TRUE}, {0200, FALSE}, {0, TRUE}, {0777, FALSE}, {0555, TRUE}, {02, FALSE},
        };

        for (i = 0; i < ARRAYSIZE(modes); i++)
        {
            int r = p_chmod(a, modes[i].perms);
            DWORD attr = GetFileAttributesW(a);

            check(!r && attr != INVALID_FILE_ATTRIBUTES && !!(attr & FILE_ATTRIBUTE_READONLY) == modes[i].readonly,
                  "_Chmod(%#o) = %d, attributes %#lx", modes[i].perms, r, attr);
        }
        check(p_chmod(L"C:\\no\\such\\file", 0666) != 0, "_Chmod(missing) fails");
        SetFileAttributesW(a, FILE_ATTRIBUTE_NORMAL);
    }

    /* _Symlink_get */
    {
        static const struct { const WCHAR *target; DWORD flags; } links[] =
        {
            {L"a.txt", 0}, {L"C:\\windows\\win.ini", 0}, {L"C:\\windows", SYMBOLIC_LINK_FLAG_DIRECTORY},
        };

        for (i = 0; i < ARRAYSIZE(links); i++)
        {
            const WCHAR *l = (links[i].flags & SYMBOLIC_LINK_FLAG_DIRECTORY) ? dlink : link;
            BOOLEAN made;

            if (!RemoveDirectoryW(l)) DeleteFileW(l);  /* left by an earlier run */
            made = CreateSymbolicLinkW(l, links[i].target, links[i].flags);

            check(made, "CreateSymbolicLinkW(%ls) %lu", links[i].target, GetLastError());
            buf[0] = 'x';
            check(p_symlink_get(buf, l) == buf && !wcscmp(buf, links[i].target), "_Symlink_get: \"%ls\" (%ls)", buf, links[i].target);
            if (!RemoveDirectoryW(l)) DeleteFileW(l);
        }
        buf[0] = 'x';
        check(p_symlink_get(buf, a) == buf && !buf[0], "_Symlink_get(not a link): empty");
        buf[0] = 'x';
        check(p_symlink_get(buf, L"C:\\no\\such\\link") == buf && !buf[0], "_Symlink_get(missing): empty");
    }

    DeleteFileW(a);
    RemoveDirectoryW(dir);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
