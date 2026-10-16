/* Probe for patches/sg/5927: a symbolic link is deleted as itself -- relative
 * and absolute links to a file and to a folder, live and dangling, through
 * DeleteFile, RemoveDirectory and FILE_DELETE_ON_CLOSE (CreateFile and
 * NtCreateFile with FILE_OPEN_REPARSE_POINT). A dangling link could not be
 * deleted (ERROR_FILE_NOT_FOUND). Each link must be gone and its target, when
 * it exists, intact; a missing target must stay missing. Table driven; lines
 * "ok"/"FAIL", last "RESULT: PASS|FAIL". The gate also checks from the Unix
 * side that no link is left. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef FILE_OPEN_REPARSE_POINT
#define FILE_OPEN_REPARSE_POINT 0x00200000
#endif
#ifndef FILE_DELETE_ON_CLOSE
#define FILE_DELETE_ON_CLOSE 0x00001000
#endif
#ifndef FILE_OPEN
#define FILE_OPEN 1
#endif

static NTSTATUS (WINAPI *pNtCreateFile)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *, IO_STATUS_BLOCK *,
                                        LARGE_INTEGER *, ULONG, ULONG, ULONG, ULONG, void *, ULONG);
static BOOLEAN (WINAPI *pRtlDosPathNameToNtPathName_U)(const WCHAR *, UNICODE_STRING *, WCHAR **, void *);
static void (WINAPI *pRtlFreeUnicodeString)(UNICODE_STRING *);

enum how { DELETEFILE, REMOVEDIR, CREATEFILE_DOC, NTCREATE_DOC };
static const char *how_name[] = { "DeleteFile", "RemoveDirectory", "CreateFile delete-on-close", "NtCreateFile delete-on-close" };

static DWORD delete_link(const char *link, enum how how, BOOL dir)
{
    HANDLE h;

    SetLastError(0xdeadbeef);
    switch (how)
    {
    case DELETEFILE:
        return DeleteFileA(link) ? 0 : GetLastError();
    case REMOVEDIR:
        return RemoveDirectoryA(link) ? 0 : GetLastError();
    case CREATEFILE_DOC:
        h = CreateFileA(link, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_DELETE_ON_CLOSE | (dir ? FILE_FLAG_BACKUP_SEMANTICS : 0), NULL);
        if (h == INVALID_HANDLE_VALUE) return GetLastError();
        CloseHandle(h);
        return 0;
    case NTCREATE_DOC:
    {
        WCHAR path[MAX_PATH];
        UNICODE_STRING name;
        OBJECT_ATTRIBUTES attr;
        IO_STATUS_BLOCK io;
        NTSTATUS status;

        MultiByteToWideChar(CP_ACP, 0, link, -1, path, MAX_PATH);
        if (!pRtlDosPathNameToNtPathName_U(path, &name, NULL, NULL)) return ERROR_PATH_NOT_FOUND;
        InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, 0, NULL);
        status = pNtCreateFile(&h, DELETE | SYNCHRONIZE, &attr, &io, NULL, 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
                               FILE_OPEN_REPARSE_POINT | FILE_DELETE_ON_CLOSE, NULL, 0);
        pRtlFreeUnicodeString(&name);
        if (status) return 0x80000000 | status;
        CloseHandle(h);
        return 0;
    }
    }
    return ERROR_INVALID_PARAMETER;
}

/* the link itself, whatever it points to: opened as a reparse point. On
 * Windows FindFirstFile would list a dangling link too; Wine's directory
 * listing still skips one (its lstat-then-stat), so it is not asked here */
static BOOL entry_exists(const char *name)
{
    HANDLE h = CreateFileA(name, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (h == INVALID_HANDLE_VALUE) return FALSE;
    CloseHandle(h);
    return TRUE;
}

static BOOL file_intact(const char *name)
{
    char buf[16] = {0};
    DWORD got = 0;
    HANDLE h = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(h, buf, sizeof(buf), &got, NULL);
    CloseHandle(h);
    return got == 5 && !memcmp(buf, "data\n", 5);
}

int main(int argc, char **argv)
{
    /* dir: a folder link (SYMBOLIC_LINK_FLAG_DIRECTORY); tdir: the target is a folder */
    static const struct { const char *target; BOOL dir, tdir, live; } targets[] =
    {
        { "data.txt",    FALSE, FALSE, TRUE },
        { "dir",         TRUE,  TRUE,  TRUE },
        { "dir",         FALSE, TRUE,  TRUE },  /* a file link to a folder: DeleteFile removes it */
        { "missing.txt", FALSE, FALSE, FALSE },
        { "missingdir",  TRUE,  TRUE,  FALSE },
    };
    char base[MAX_PATH], link[MAX_PATH], target[MAX_PATH];
    unsigned int t, abs, h, n = 0;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    DWORD got, err;
    HANDLE f;

    pNtCreateFile = (void *)GetProcAddress(ntdll, "NtCreateFile");
    pRtlDosPathNameToNtPathName_U = (void *)GetProcAddress(ntdll, "RtlDosPathNameToNtPathName_U");
    pRtlFreeUnicodeString = (void *)GetProcAddress(ntdll, "RtlFreeUnicodeString");

    if (argc > 1) strcpy(base, argv[1]);
    else
    {
        GetTempPathA(MAX_PATH, base);
        strcat(base, "linkdel");
    }
    CreateDirectoryA(base, NULL);
    SetCurrentDirectoryA(base);
    CreateDirectoryA("dir", NULL);
    f = CreateFileA("data.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "data\n", 5, &got, NULL);
    CloseHandle(f);
    f = CreateFileA("dir\\inner.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "data\n", 5, &got, NULL);
    CloseHandle(f);

    for (t = 0; t < ARRAYSIZE(targets); t++)
    for (abs = 0; abs < 2; abs++)
    for (h = 0; h < 4; h++)
    {
        const char *kind;

        /* a link to a folder is removed as a folder, a link to a file as a file */
        if (h == DELETEFILE && targets[t].dir) continue;
        if (h == REMOVEDIR && !targets[t].dir) continue;
        kind = targets[t].live ? (targets[t].dir ? "live folder" : targets[t].tdir ? "live file-to-folder" : "live file")
                               : (targets[t].dir ? "dangling folder" : "dangling file");

        sprintf(link, "link%u", n++);
        if (abs) sprintf(target, "%s\\%s", base, targets[t].target);
        else strcpy(target, targets[t].target);
        if (!CreateSymbolicLinkA(link, target, (targets[t].dir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) | 2 /* unprivileged */))
        {
            check(0, "CreateSymbolicLinkA(%s -> %s): %lu", link, target, GetLastError());
            continue;
        }
        check(entry_exists(link), "%s (%s %s link) exists before", link, abs ? "absolute" : "relative", kind);

        err = delete_link(link, h, targets[t].tdir);
        check(!err, "%s %s %s link %s: %#lx", how_name[h], abs ? "absolute" : "relative", kind, link, err);
        check(!entry_exists(link), "%s is gone after %s", link, how_name[h]);

        if (targets[t].live && targets[t].tdir)
            check(file_intact("dir\\inner.txt"), "folder target intact after %s of %s", how_name[h], link);
        else if (targets[t].live)
            check(file_intact("data.txt"), "file target intact after %s of %s", how_name[h], link);
        else
            check(!entry_exists(targets[t].target), "missing target %s still missing after %s of %s",
                  targets[t].target, how_name[h], link);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
