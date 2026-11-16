/* renamemount-probe.c: renames across two mounts of one filesystem (wine-sg 7814), for
 * test/renamemount-gate.sh. Each argument pair: OP SRC DST, OP one of
 *   move   MoveFileExW(SRC, DST, 0)
 *   copyok MoveFileExW(SRC, DST, MOVEFILE_COPY_ALLOWED)
 *   info   SetFileInformationByHandle(FileRenameInfo)
 * prints one line per operation: OK or the error code */
#include <windows.h>
#include <stdio.h>

static void info_rename(const WCHAR *src, const WCHAR *dst)
{
    BYTE buf[sizeof(FILE_RENAME_INFO) + MAX_PATH * sizeof(WCHAR)];
    FILE_RENAME_INFO *ri = (FILE_RENAME_INFO *)buf;
    HANDLE h = CreateFileW(src, DELETE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("open %lu\n", GetLastError()); return; }
    memset(buf, 0, sizeof(buf));
    ri->FileNameLength = lstrlenW(dst) * sizeof(WCHAR);
    memcpy(ri->FileName, dst, ri->FileNameLength);
    if (SetFileInformationByHandle(h, FileRenameInfo, ri, sizeof(buf))) printf("OK\n");
    else printf("%lu\n", GetLastError());
    CloseHandle(h);
}

int wmain(int argc, WCHAR **argv)
{
    int i;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 1; i + 2 < argc + 0 || i + 2 == argc; i += 3)
    {
        const WCHAR *op = argv[i], *src = argv[i + 1], *dst = argv[i + 2];
        if (!lstrcmpW(op, L"info")) { info_rename(src, dst); continue; }
        if (MoveFileExW(src, dst, !lstrcmpW(op, L"copyok") ? MOVEFILE_COPY_ALLOWED : 0)) printf("OK\n");
        else printf("%lu\n", GetLastError());
    }
    return 0;
}
