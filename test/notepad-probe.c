/*
 * Probe for test/notepad-gate.sh: looks inside a running Notepad.
 *
 *   notepad-probe wait                     wait for a Notepad window; hwnd=
 *   notepad-probe tabs                     tabs=N active=K
 *   notepad-probe dump TAB FILE            the tab's text, UTF-8, into FILE
 *   notepad-probe enc [TAB]                enc=E eol=L (ENCODING / EOLTYPE numbers)
 *   notepad-probe modified [TAB]           modified=0|1
 *   notepad-probe class                    the editor's window class
 *   notepad-probe style POS                style at POS of the active tab
 *   notepad-probe lang                     lang= of the active tab
 *   notepad-probe screenpos POS            x= y= of character POS on the screen
 *   notepad-probe cmd ID                   WM_COMMAND to the main window
 *   notepad-probe findbar FIND REPLACE     fill the find bar's two boxes
 *   notepad-probe click ID                 press a find bar button
 *   notepad-probe settext TEXT             WM_SETTEXT on the active editor (the EDIT protocol)
 *   notepad-probe getsel                   EM_GETSEL: a= b=
 *   notepad-probe launch CMDLINE           CreateProcess(NULL, CMDLINE) the way a program would
 *                                          (search path, not a full path); then the image and
 *                                          the class of its editor: image= editor=
 *   notepad-probe close                    WM_CLOSE to every Notepad
 *
 * Built 64- and 32-bit by the gate.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SGN_TABCOUNT   (WM_APP + 0x51)
#define SGN_ACTIVETAB  (WM_APP + 0x52)
#define SGN_GETEDITOR  (WM_APP + 0x53)
#define SGN_ISMODIFIED (WM_APP + 0x54)
#define SGN_GETENC     (WM_APP + 0x55)
#define SGE_GETSTYLEAT (WM_USER + 0x500)
#define SGE_GETLANG    (WM_USER + 0x501)
#define SGE_POSFROMCHAR (WM_USER + 0x503)

static DWORD want_pid;

static BOOL CALLBACK find_proc(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    DWORD pid;
    GetClassNameW(hwnd, cls, 64);
    GetWindowThreadProcessId(hwnd, &pid);
    if (!lstrcmpW(cls, L"Notepad") && IsWindowVisible(hwnd) && (!want_pid || pid == want_pid))
    {
        *(HWND *)lp = hwnd;
        return FALSE;
    }
    return TRUE;
}

static HWND find_notepad(int timeout_ms)
{
    HWND h = NULL;
    DWORD start = GetTickCount();
    do
    {
        EnumWindows(find_proc, (LPARAM)&h);
        if (h) return h;
        Sleep(200);
    } while ((int)(GetTickCount() - start) < timeout_ms);
    return NULL;
}

static HWND editor(HWND main, int tab)
{
    return (HWND)SendMessageW(main, SGN_GETEDITOR, (WPARAM)tab, 0);
}

static BOOL CALLBACK close_proc(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    GetClassNameW(hwnd, cls, 64);
    if (!lstrcmpW(cls, L"Notepad")) PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

int wmain(int argc, WCHAR **argv)
{
    HWND main;
    if (argc < 2) return 2;

    if (!lstrcmpW(argv[1], L"launch") && argc >= 3)
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[1024], image[MAX_PATH], cls[64] = L"none";
        DWORD n = MAX_PATH;
        HWND w, child;
        lstrcpynW(cmd, argv[2], 1024);
        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            printf("launch failed %lu\n", GetLastError());
            return 1;
        }
        WaitForInputIdle(pi.hProcess, 20000);
        if (!QueryFullProcessImageNameW(pi.hProcess, 0, image, &n)) lstrcpyW(image, L"?");
        want_pid = pi.dwProcessId;
        w = find_notepad(20000);
        if (w && (child = editor(w, -1))) GetClassNameW(child, cls, 64);
        else if (w && (child = FindWindowExW(w, NULL, NULL, NULL))) { GetClassNameW(child, cls, 64); }
        printf("image=%ls editor=%ls bits=%d\n", image, cls, (int)(sizeof(void *) * 8));
        return 0;
    }
    if (!lstrcmpW(argv[1], L"close"))
    {
        EnumWindows(close_proc, 0);
        return 0;
    }

    main = find_notepad(!lstrcmpW(argv[1], L"wait") ? 30000 : 5000);
    if (!main) { printf("no notepad\n"); return 1; }

    if (!lstrcmpW(argv[1], L"wait")) printf("hwnd=%p\n", main);
    else if (!lstrcmpW(argv[1], L"tabs"))
        printf("tabs=%ld active=%ld\n", (long)SendMessageW(main, SGN_TABCOUNT, 0, 0), (long)SendMessageW(main, SGN_ACTIVETAB, 0, 0));
    else if (!lstrcmpW(argv[1], L"dump") && argc >= 4)
    {
        HWND ed = editor(main, _wtoi(argv[2]));
        LRESULT len = ed ? SendMessageW(ed, WM_GETTEXTLENGTH, 0, 0) : -1;
        WCHAR *buf;
        char *u8;
        int n;
        FILE *f;
        if (len < 0) { printf("no editor\n"); return 1; }
        buf = malloc((len + 1) * sizeof(WCHAR));
        len = SendMessageW(ed, WM_GETTEXT, len + 1, (LPARAM)buf);
        n = WideCharToMultiByte(CP_UTF8, 0, buf, (int)len, NULL, 0, NULL, NULL);
        u8 = malloc(n + 1);
        WideCharToMultiByte(CP_UTF8, 0, buf, (int)len, u8, n, NULL, NULL);
        f = _wfopen(argv[3], L"wb");
        if (!f) return 1;
        fwrite(u8, 1, n, f);
        fclose(f);
        printf("chars=%ld\n", (long)len);
    }
    else if (!lstrcmpW(argv[1], L"enc"))
    {
        LRESULT r = SendMessageW(main, SGN_GETENC, argc > 2 ? _wtoi(argv[2]) : -1, 0);
        printf("enc=%d eol=%d\n", (int)LOWORD(r), (int)HIWORD(r));
    }
    else if (!lstrcmpW(argv[1], L"modified"))
        printf("modified=%ld\n", (long)SendMessageW(main, SGN_ISMODIFIED, argc > 2 ? _wtoi(argv[2]) : -1, 0));
    else if (!lstrcmpW(argv[1], L"class"))
    {
        WCHAR cls[64] = L"none";
        HWND ed = editor(main, -1);
        if (ed) GetClassNameW(ed, cls, 64);
        printf("class=%ls\n", cls);
    }
    else if (!lstrcmpW(argv[1], L"style") && argc >= 3)
        printf("style=%ld\n", (long)SendMessageW(editor(main, -1), SGE_GETSTYLEAT, _wtoi(argv[2]), 0));
    else if (!lstrcmpW(argv[1], L"lang"))
        printf("lang=%ld\n", (long)SendMessageW(editor(main, -1), SGE_GETLANG, 0, 0));
    else if (!lstrcmpW(argv[1], L"screenpos") && argc >= 3)
    {
        HWND ed = editor(main, -1);
        LRESULT r = SendMessageW(ed, SGE_POSFROMCHAR, _wtoi(argv[2]), 0);
        POINT pt = { (short)LOWORD(r), (short)HIWORD(r) };
        ClientToScreen(ed, &pt);
        printf("x=%ld y=%ld\n", pt.x, pt.y);
    }
    else if (!lstrcmpW(argv[1], L"cmd") && argc >= 3)
        PostMessageW(main, WM_COMMAND, _wtoi(argv[2]), 0);
    else if (!lstrcmpW(argv[1], L"findbar") && argc >= 4)
    {
        HWND bar = FindWindowExW(main, NULL, L"#32770", NULL);
        if (!bar) { printf("no find bar\n"); return 1; }
        SetDlgItemTextW(bar, 0x400, argv[2]);
        SetDlgItemTextW(bar, 0x401, argv[3]);
        printf("ok\n");
    }
    else if (!lstrcmpW(argv[1], L"click") && argc >= 3)
    {
        HWND bar = FindWindowExW(main, NULL, L"#32770", NULL);
        if (!bar) { printf("no find bar\n"); return 1; }
        SendMessageW(bar, WM_COMMAND, MAKEWPARAM(_wtoi(argv[2]), BN_CLICKED), (LPARAM)GetDlgItem(bar, _wtoi(argv[2])));
        printf("ok\n");
    }
    else if (!lstrcmpW(argv[1], L"settext") && argc >= 3)
        printf("r=%ld\n", (long)SendMessageW(editor(main, -1), WM_SETTEXT, 0, (LPARAM)argv[2]));
    else if (!lstrcmpW(argv[1], L"getsel"))
    {
        DWORD a = 0, b = 0;
        SendMessageW(editor(main, -1), EM_GETSEL, (WPARAM)&a, (LPARAM)&b);
        printf("a=%lu b=%lu\n", a, b);
    }
    else return 2;
    return 0;
}
