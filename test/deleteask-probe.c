/* The delete question's details (patches/sg/9066): Windows shows, under "Are
 * you sure you want to move this file to the Recycle Bin?", the item's own
 * icon, its name, and for a file its type, size and date modified, for a
 * folder its date created; we showed its path.
 *
 *   deleteask-probe.exe trash|delete PATH [PATH...]
 * deletes (FOF_ALLOWUNDO for trash) with the question shown, answers No and
 * prints what was asked: the caption, the question (WINE_CONFIRM), the
 * details (WINE_CONFIRM_ITEM, lines joined by " / "), whether the question
 * icon is shown, and whether an icon is drawn left below the question. */
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static WCHAR caption[256], question[512], details[1024];
static int qicon = -1, drawn = -1, dlg_found;
static volatile LONG done;

static BOOL CALLBACK find_dialog(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    DWORD pid;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
    GetClassNameW(hwnd, cls, ARRAY_SIZE(cls));
    if (lstrcmpiW(cls, L"#32770")) return TRUE;
    *(HWND *)lp = hwnd;
    return FALSE;
}

/* an icon drawn in the dialog's left part, below the first line of text:
 * pixels that are not the dialog's face, in the column the icon sits in */
static int icon_pixels(HWND dlg)
{
    COLORREF face = GetSysColor(COLOR_BTNFACE);
    HWND yes = GetDlgItem(dlg, IDYES);
    RECT rc, yr;
    int x, y, n = 0;
    HDC hdc;

    GetClientRect(dlg, &rc);
    GetWindowRect(yes, &yr);
    MapWindowPoints(0, dlg, (POINT *)&yr, 2);
    hdc = GetDC(dlg);
    /* below the question's first line, above the buttons, in the left 90 px */
    for (y = 34; y < yr.top - 2; y++)
        for (x = 0; x < 90 && x < rc.right; x++)
        {
            COLORREF c = GetPixel(hdc, x, y);
            if (c != CLR_INVALID && c != face) n++;
        }
    ReleaseDC(dlg, hdc);
    return n;
}

static DWORD WINAPI answer_thread(void *arg)
{
    HWND dlg = NULL;
    const WCHAR *p;
    int i;

    for (i = 0; i < 160 && !dlg; i++) { EnumWindows(find_dialog, (LPARAM)&dlg); if (!dlg) Sleep(50); }
    if (dlg)
    {
        dlg_found = 1;
        Sleep(400);  /* painted */
        {
            WCHAR hold[16];
            if (GetEnvironmentVariableW(L"DA_HOLD_MS", hold, ARRAY_SIZE(hold))) Sleep(_wtoi(hold));
        }
        UpdateWindow(dlg);
        GetWindowTextW(dlg, caption, ARRAY_SIZE(caption));
        if ((p = GetPropW(dlg, L"WINE_CONFIRM"))) lstrcpynW(question, p, ARRAY_SIZE(question));
        if ((p = GetPropW(dlg, L"WINE_CONFIRM_ITEM")))
        {
            WCHAR *d = details;
            for (; *p && d < details + ARRAY_SIZE(details) - 4; p++)
            {
                if (*p == '\n') { *d++ = ' '; *d++ = '/'; *d++ = ' '; }
                else *d++ = *p;
            }
            *d = 0;
        }
        qicon = IsWindowVisible(GetDlgItem(dlg, 0x4300 /* IDD_ICON */)) ? 1 : 0;
        drawn = icon_pixels(dlg);
        PostMessageW(dlg, WM_COMMAND, IDNO, 0);
    }
    InterlockedExchange(&done, 1);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    SHFILEOPSTRUCTW op = { 0 };
    SHELLSTATEW state = { 0 };
    WCHAR from[4096] = { 0 }, *f = from;
    int i, ret;

    if (argc < 3) return 2;
    /* "Display delete confirmation dialog" on, so moving to the bin asks */
    state.fNoConfirmRecycle = FALSE;
    SHGetSetSettings((SHELLSTATE *)&state, SSF_NOCONFIRMRECYCLE, TRUE);
    for (i = 2; i < argc; i++)
    {
        lstrcpyW(f, argv[i]);
        f += lstrlenW(argv[i]) + 1;
    }
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = !lstrcmpiW(argv[1], L"trash") ? FOF_ALLOWUNDO : 0;
    CloseHandle(CreateThread(NULL, 0, answer_thread, NULL, 0, NULL));
    ret = SHFileOperationW(&op);
    for (i = 0; i < 200 && !done; i++) Sleep(50);
    wprintf(L"dialog=%d ret=%d aborted=%d\n", dlg_found, ret, op.fAnyOperationsAborted);
    wprintf(L"caption: %ls\n", caption);
    wprintf(L"question: %ls\n", question);
    wprintf(L"details: %ls\n", details);
    wprintf(L"qicon=%d drawn=%d\n", qicon, drawn);
    return 0;
}
