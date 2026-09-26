/* Symbolic links and junctions (wine-sg 0360): CreateSymbolicLinkW, reading a
 * link back, deleting it without touching its target. Each check prints
 * "name=0|1"; the gate requires every one to be 1. */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

typedef struct {
    ULONG tag; USHORT len, reserved;
    USHORT subst_off, subst_len, print_off, print_len;
    ULONG flags; WCHAR path[1];
} SYMLINK_BUF;

static int fails;
static void check(const char *name, int ok) { printf("%s=%d\n", name, ok ? 1 : 0); if (!ok) fails++; fflush(stdout); }

static int read_file(const WCHAR *path, char *out, DWORD size)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ReadFile(h, out, size - 1, &n, NULL); out[n] = 0; CloseHandle(h);
    return 1;
}

static void write_file(const WCHAR *path, const char *s)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD n;
    WriteFile(h, s, strlen(s), &n, NULL); CloseHandle(h);
}

static int get_link(const WCHAR *path, WCHAR *subst, ULONG *tag, ULONG *flags)
{
    BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE]; SYMLINK_BUF *r = (SYMLINK_BUF *)buf; DWORD n;
    HANDLE h = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    BOOL ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, sizeof(buf), &n, NULL);
    if (!ok) printf("  get_reparse(%ls) error %lu\n", path, GetLastError());
    CloseHandle(h);
    if (!ok) return 0;
    *tag = r->tag; *flags = r->flags;
    memcpy(subst, (BYTE *)r->path + r->subst_off, r->subst_len); subst[r->subst_len / 2] = 0;
    return 1;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR base[MAX_PATH], p[MAX_PATH], q[MAX_PATH], subst[MAX_PATH];
    char text[64];
    DWORD attr;
    ULONG tag = 0, flags = 0;
    HANDLE h;

    GetTempPathW(MAX_PATH, base); wcscat(base, L"sg-symlink-probe");
    CreateDirectoryW(base, NULL);
    SetCurrentDirectoryW(base);
    CreateDirectoryW(L"13.0.1", NULL);
    CreateDirectoryW(L"13.0.1\\App Dir", NULL);
    write_file(L"13.0.1\\App Dir\\service.txt", "service");
    write_file(L"target.txt", "target");

    /* a relative directory link, as the EA app makes: "App Dir" -> "13.0.1\App Dir" */
    check("dirlink_created", CreateSymbolicLinkW(L"current", L"13.0.1\\App Dir", SYMBOLIC_LINK_FLAG_DIRECTORY));
    attr = GetFileAttributesW(L"current");
    check("dirlink_attrs", attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) && (attr & FILE_ATTRIBUTE_REPARSE_POINT));
    check("dirlink_follows", read_file(L"current\\service.txt", text, sizeof(text)) && !strcmp(text, "service"));
    check("dirlink_reads_back", get_link(L"current", subst, &tag, &flags) && tag == IO_REPARSE_TAG_SYMLINK &&
          !wcscmp(subst, L"13.0.1\\App Dir") && (flags & 1));

    /* the target's case comes from the file system: the Unix kernel follows the link */
    check("dirlink_case", CreateSymbolicLinkW(L"upper", L"13.0.1\\APP DIR", SYMBOLIC_LINK_FLAG_DIRECTORY) &&
          read_file(L"upper\\SERVICE.TXT", text, sizeof(text)) && !strcmp(text, "service"));

    /* an absolute file link */
    GetFullPathNameW(L"target.txt", MAX_PATH, p, NULL);
    check("filelink_created", CreateSymbolicLinkW(L"flink.txt", p, 0));
    check("filelink_follows", read_file(L"flink.txt", text, sizeof(text)) && !strcmp(text, "target"));
    check("filelink_reads_back", get_link(L"flink.txt", subst, &tag, &flags) && tag == IO_REPARSE_TAG_SYMLINK &&
          !(flags & 1) && !wcsncmp(subst, L"\\??\\", 4) && !lstrcmpiW(subst + 4, p));
    h = CreateFileW(L"flink.txt", FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    {
        FILE_ATTRIBUTE_TAG_INFO ti = { 0 };
        check("filelink_tag", h != INVALID_HANDLE_VALUE && GetFileInformationByHandleEx(h, FileAttributeTagInfo, &ti, sizeof(ti)) &&
              ti.ReparseTag == IO_REPARSE_TAG_SYMLINK && (ti.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT));
        CloseHandle(h);
    }

    /* deleting a link removes the link, never what it points to */
    if (!RemoveDirectoryW(L"current")) printf("  rmdir error %lu\n", GetLastError());
    check("dirlink_removed", 1 && GetFileAttributesW(L"current") == INVALID_FILE_ATTRIBUTES &&
          read_file(L"13.0.1\\App Dir\\service.txt", text, sizeof(text)));
    if (!DeleteFileW(L"flink.txt")) printf("  delete error %lu\n", GetLastError());
    check("filelink_removed", 1 && GetFileAttributesW(L"flink.txt") == INVALID_FILE_ATTRIBUTES &&
          GetFileAttributesW(L"target.txt") != INVALID_FILE_ATTRIBUTES);

    /* a junction, made the way installers make them */
    GetFullPathNameW(L"13.0.1", MAX_PATH, q, NULL);
    CreateDirectoryW(L"junction", NULL);
    h = CreateFileW(L"junction", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    {
        BYTE buf[1024] = { 0 }; DWORD n; int len;
        WCHAR nt[MAX_PATH];
        swprintf(nt, MAX_PATH, L"\\??\\%ls", q);
        len = wcslen(nt) * 2;
        *(ULONG *)buf = IO_REPARSE_TAG_MOUNT_POINT;
        *(USHORT *)(buf + 4) = 8 + len + 2 + 2;
        *(USHORT *)(buf + 8) = 0; *(USHORT *)(buf + 10) = len;
        *(USHORT *)(buf + 12) = len + 2; *(USHORT *)(buf + 14) = 0;
        memcpy(buf + 16, nt, len);
        check("junction_set", h != INVALID_HANDLE_VALUE &&
              DeviceIoControl(h, FSCTL_SET_REPARSE_POINT, buf, 8 + *(USHORT *)(buf + 4), NULL, 0, &n, NULL));
        CloseHandle(h);
    }
    check("junction_follows", read_file(L"junction\\App Dir\\service.txt", text, sizeof(text)) && !strcmp(text, "service"));

    /* bad flags are refused */
    SetLastError(0);
    check("bad_flags", !CreateSymbolicLinkW(L"bad", L"target.txt", 0x100) && GetLastError() == ERROR_INVALID_PARAMETER);

    RemoveDirectoryW(L"upper"); RemoveDirectoryW(L"junction");
    printf("failures=%d\n", fails);
    return fails != 0;
}
