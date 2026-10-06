/* fileoprace-probe: for fileoprace-gate.sh (patches/sg/0980).
 *   fileoprace-probe.exe ROUNDS MS
 * Copies a folder of small files with SHFileOperation (progress shown, as
 * File Explorer's paste does) ROUNDS times, the folder sized so the copy
 * takes about MS ms (measured first), each from a thread of its own, and
 * prints "ROUND n ms T" for each; one that does not return within 20 s
 * prints "HANG n" and the probe ends with 1. After each, no progress window
 * may be left ("LEFT n"). */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

static SHFILEOPSTRUCTW op;

static void make_files(const WCHAR *dir, int from, int to)
{
    WCHAR path[MAX_PATH];
    DWORD put;
    int i;
    CreateDirectoryW(dir, NULL);
    for (i = from; i < to; i++)
    {
        HANDLE h;
        swprintf(path, MAX_PATH, L"%ls\\file%05d.txt", dir, i);
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(h, "0123456789abcdef0123456789abcdef", 32, &put, NULL);
        CloseHandle(h);
    }
}

static void remove_tree(const WCHAR *dir)
{
    SHFILEOPSTRUCTW del = { 0 };
    WCHAR from[MAX_PATH + 2] = { 0 };
    lstrcpyW(from, dir);
    del.wFunc = FO_DELETE;
    del.pFrom = from;
    del.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    SHFileOperationW(&del);
}

static DWORD WINAPI copier(void *arg)
{
    return SHFileOperationW(&op);
}

static DWORD timed_copy(BOOL silent)
{
    DWORD t0 = GetTickCount();
    op.wFunc = FO_COPY;
    op.pFrom = L"C:\\race-src\0";
    op.pTo = L"C:\\race-dst\0";
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | (silent ? FOF_SILENT : 0);
    if (SHFileOperationW(&op)) printf("CALIBRATE failed\n");
    return GetTickCount() - t0;
}

int main(int argc, char **argv)
{
    int rounds = argc > 1 ? atoi(argv[1]) : 10, target = argc > 2 ? atoi(argv[2]) : 1000, n = 400, i, bad = 0;
    DWORD ms;

    /* how fast this machine copies them */
    make_files(L"C:\\race-src", 0, n);
    remove_tree(L"C:\\race-dst");
    ms = timed_copy(TRUE);
    remove_tree(L"C:\\race-dst");
    if (ms < 20) ms = 20;
    n = (int)((double)n * target / ms);
    if (n < 50) n = 50;
    if (n > 20000) n = 20000;
    make_files(L"C:\\race-src", 400, n);
    printf("FILES %d (400 in %lu ms)\n", n, ms);
    for (i = 0; i < rounds; i++)
    {
        HANDLE t;
        DWORD t0 = GetTickCount(), w;
        HWND left;
        op.wFunc = FO_COPY;
        op.pFrom = L"C:\\race-src\0";
        op.pTo = L"C:\\race-dst\0";
        op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR;
        t = CreateThread(NULL, 0, copier, NULL, 0, NULL);
        w = WaitForSingleObject(t, 20000);
        if (w != WAIT_OBJECT_0)
        {
            printf("HANG %d\n", i);
            fflush(stdout);
            ExitProcess(1);
        }
        GetExitCodeThread(t, &w);
        CloseHandle(t);
        printf("ROUND %d ms %lu result %lu\n", i, GetTickCount() - t0, w);
        Sleep(400);
        if ((left = FindWindowW(L"SGFileOperation", NULL)) && IsWindowVisible(left)) { printf("LEFT %d\n", i); bad = 1; }
        remove_tree(L"C:\\race-dst");
        fflush(stdout);
    }
    return bad;
}
