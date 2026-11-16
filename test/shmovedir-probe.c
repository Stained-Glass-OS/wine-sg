/* shmovedir-probe.c: SHFileOperation FO_MOVE of a folder, for test/shmovedir-gate.sh:
 *   shmovedir-probe.exe FROM TO      prints ret=N aborted=N */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
int wmain(int argc, WCHAR **argv)
{
    WCHAR from[MAX_PATH + 2] = {0}, to[MAX_PATH + 2] = {0};
    SHFILEOPSTRUCTW op = {0};
    int r;
    if (argc < 3) return 1;
    lstrcpyW(from, argv[1]);
    lstrcpyW(to, argv[2]);
    op.wFunc = FO_MOVE;
    op.pFrom = from;
    op.pTo = to;
    op.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    r = SHFileOperationW(&op);
    printf("ret=%d aborted=%d\n", r, op.fAnyOperationsAborted);
    return 0;
}
