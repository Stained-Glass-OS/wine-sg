/* trashcancel-probe.c: delete a file to the Recycle Bin that has to be copied (another filesystem),
 * watch the progress window, press Cancel; for test/trashcancel-gate.sh.
 *   trashcancel-probe.exe PATH
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static WCHAR path[MAX_PATH + 2];
static volatile LONG done;
static int ret_code, aborted;

static DWORD WINAPI delete_thread(void *arg)
{
    SHFILEOPSTRUCTW op = {0};
    op.wFunc = FO_DELETE;
    op.pFrom = path;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    ret_code = SHFileOperationW(&op);
    aborted = op.fAnyOperationsAborted;
    InterlockedExchange(&done, 1);
    return 0;
}

struct find { const WCHAR *cls; HWND found; };
static BOOL CALLBACK find_proc(HWND hwnd, LPARAM lp)
{
    struct find *f = (struct find *)lp;
    WCHAR cls[64];
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
    GetClassNameW(hwnd, cls, 64);
    if (!lstrcmpW(cls, f->cls)) { f->found = hwnd; return FALSE; }
    return TRUE;
}
static HWND find_window(const WCHAR *cls)
{
    struct find f = { cls, NULL };
    EnumWindows(find_proc, (LPARAM)&f);
    return f.found;
}

static int window_percent(HWND hwnd)
{
    WCHAR title[128];
    int pct = -1;
    if (GetWindowTextW(hwnd, title, 128) && swscanf(title, L"%d%% complete", &pct) == 1) return pct;
    return -1;
}

int wmain(int argc, WCHAR **argv)
{
    HWND hwnd = NULL, button, box;
    int i, pct, first = -1, last = -1, dialog = 0;
    HANDLE thread;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) return 1;
    lstrcpyW(path, argv[1]);
    path[lstrlenW(path) + 1] = 0;
    thread = CreateThread(NULL, 0, delete_thread, NULL, 0, NULL);

    for (i = 0; i < 200 && !done && !(hwnd = find_window(L"SGFileOperation")); i++) Sleep(50);
    printf("Shown=%d\n", hwnd != NULL);
    if (hwnd)
    {
        /* watch the bar for a while: it moves with the bytes, and stays under 100% */
        for (i = 0; i < 30 && !done; i++)
        {
            if ((pct = window_percent(hwnd)) >= 0) { if (first < 0) first = pct; last = pct; }
            Sleep(100);
        }
        printf("Percent=%d %d\n", last >= 0 && last < 100, last > first);
        printf("Running=%d\n", !done);
        if ((button = GetDlgItem(hwnd, IDCANCEL))) SendMessageW(button, BM_CLICK, 0, 0);
        printf("Cancel=%d\n", button != NULL);
    }
    /* the operation ends, and no message box ("delete it permanently?") follows */
    for (i = 0; i < 400; i++)
    {
        if ((box = find_window(L"#32770")))
        {
            dialog = 1;
            PostMessageW(box, WM_COMMAND, IDNO, 0);   /* (never delete for good) */
        }
        if (done && i >= 40) break;
        Sleep(50);
    }
    WaitForSingleObject(thread, 5000);
    printf("Result=%d %d %d\n", done, ret_code != 0, aborted);
    printf("Dialog=%d\n", dialog);
    printf("Done=1\n");
    return 0;
}
