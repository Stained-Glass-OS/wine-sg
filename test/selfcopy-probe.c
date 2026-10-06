/* selfcopy-probe: SHFileOperation FO_COPY of items into the folder they are
 * in, as File Explorer's paste does (wine-sg 1176), for test/selfcopy-gate.sh.
 *
 *   selfcopy-probe DIR NAME...   copies DIR\NAME... into DIR (no flags: the
 *                                user would be asked), prints rc= and aborted=
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    WCHAR from[4096] = { 0 }, to[MAX_PATH + 2] = { 0 }, *p = from;
    SHFILEOPSTRUCTW op = { 0 };
    int i, rc;

    if (argc < 3) return 2;
    for (i = 2; i < argc; i++)
    {
        swprintf(p, 4096 - (p - from), L"%ls\\%ls", argv[1], argv[i]);
        p += lstrlenW(p) + 1;
    }
    lstrcpyW(to, argv[1]);
    op.wFunc = FO_COPY;
    op.pFrom = from;
    op.pTo = to;
    op.fFlags = FOF_ALLOWUNDO;
    rc = SHFileOperationW(&op);
    printf("rc=%d aborted=%d\n", rc, op.fAnyOperationsAborted);
    return 0;
}
