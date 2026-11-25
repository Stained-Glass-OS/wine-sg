/* regmerge-probe: for regmerge-gate.sh (issue #13, .reg files merge on a double-click).
 *   regmerge-probe.exe assoc            the .reg association: verbs, their commands, icon, type name
 *   regmerge-probe.exe answer K...      for each K (Y yes, N no, O OK): wait for regedit's next
 *                                       message box, print "BOX <its text>", press K
 *   regmerge-probe.exe nobox SECS       print "NOBOX" when no regedit message box shows in SECS
 *   regmerge-probe.exe create CMDLINE   CreateProcess, print "CREATE ok N" or "CREATE err N"
 *   regmerge-probe.exe shell FILE       ShellExecuteEx "open" FILE (its default verb), print "SHELL ok|err N"
 * Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later */
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>

static HWND find_box(DWORD ms)
{
    DWORD t0 = GetTickCount();
    do
    {
        HWND hwnd = NULL;
        while ((hwnd = FindWindowExW(NULL, hwnd, L"#32770", L"Registry Editor")))
            if (IsWindowVisible(hwnd)) return hwnd;
        Sleep(100);
    } while (GetTickCount() - t0 < ms);
    return NULL;
}

static void print_w(const char *tag, const WCHAR *s)
{
    char buf[4096];
    WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, sizeof(buf), NULL, NULL);
    for (char *p = buf; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
    printf("%s %s\n", tag, buf);
    fflush(stdout);
}

static void assoc(const WCHAR *verb)
{
    WCHAR out[1024], line[1100];
    DWORD n = ARRAYSIZE(out);
    if (AssocQueryStringW(0, ASSOCSTR_COMMAND, L".reg", verb, out, &n) != S_OK) lstrcpyW(out, L"(none)");
    wsprintfW(line, L"%s %s", verb, out);
    print_w("VERB", line);
}

int wmain(int argc, WCHAR **argv)
{
    if (argc < 2) return 2;
    if (!lstrcmpW(argv[1], L"assoc"))
    {
        WCHAR out[1024], name[256];
        DWORD n = ARRAYSIZE(out);
        LONG size;
        if (AssocQueryStringW(0, ASSOCSTR_FRIENDLYDOCNAME, L".reg", NULL, out, &n) == S_OK) print_w("TYPE", out);
        n = ARRAYSIZE(out);
        if (AssocQueryStringW(0, ASSOCSTR_DEFAULTICON, L".reg", NULL, out, &n) == S_OK) print_w("ICON", out);
        assoc(L"open");
        assoc(L"edit");
        assoc(L"print");
        n = ARRAYSIZE(out);
        if (AssocQueryStringW(0, ASSOCSTR_COMMAND, L".reg", NULL, out, &n) == S_OK) print_w("DEFAULT", out);
        size = sizeof(name);
        if (!RegQueryValueW(HKEY_CLASSES_ROOT, L"regfile\\shell\\open", name, &size)) print_w("OPENNAME", name);
        printf("ICONS %u\n", ExtractIconExW(L"C:\\windows\\regedit.exe", -1, NULL, NULL, 0));
        {
            HICON big = NULL, app = NULL;
            ICONINFO ii;
            BITMAP bm;
            ExtractIconExW(L"C:\\windows\\regedit.exe", 1, &big, NULL, 1);
            ExtractIconExW(L"C:\\windows\\regedit.exe", 0, &app, NULL, 1);
            /* index 1 is its own picture, not the program's icon again */
            if (big && app && GetIconInfo(big, &ii) && GetObjectW(ii.hbmColor, sizeof(bm), &bm))
            {
                BYTE a[32 * 32 * 4], b[32 * 32 * 4];
                HDC dc = CreateCompatibleDC(NULL);
                HBITMAP bmp = CreateBitmap(32, 32, 1, 32, NULL);
                HGDIOBJ old = SelectObject(dc, bmp);
                RECT r = {0, 0, 32, 32};
                FillRect(dc, &r, GetStockObject(WHITE_BRUSH));
                DrawIconEx(dc, 0, 0, big, 32, 32, 0, NULL, DI_NORMAL);
                GetBitmapBits(bmp, sizeof(a), a);
                FillRect(dc, &r, GetStockObject(WHITE_BRUSH));
                DrawIconEx(dc, 0, 0, app, 32, 32, 0, NULL, DI_NORMAL);
                GetBitmapBits(bmp, sizeof(b), b);
                SelectObject(dc, old);
                printf("ICON1 %s\n", memcmp(a, b, sizeof(a)) ? "own" : "same-as-program");
            }
        }
        return 0;
    }
    if (!lstrcmpW(argv[1], L"answer"))
    {
        for (int i = 2; i < argc; i++)
        {
            WCHAR text[2048];
            HWND box = find_box(30000);
            int id = argv[i][0] == 'Y' ? IDYES : argv[i][0] == 'N' ? IDNO : IDOK;
            if (!box) { printf("BOX none\n"); return 1; }
            Sleep(300);
            GetDlgItemTextW(box, 0xffff, text, ARRAYSIZE(text));
            print_w("BOX", text);
            SendMessageW(box, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(box, id));
            for (int t = 0; t < 100 && IsWindow(box); t++) Sleep(100);
        }
        return 0;
    }
    if (!lstrcmpW(argv[1], L"nobox"))
    {
        HWND box = find_box(_wtoi(argv[2]) * 1000);
        if (box)
        {
            WCHAR text[2048];
            GetDlgItemTextW(box, 0xffff, text, ARRAYSIZE(text));
            print_w("BOX", text);
            SendMessageW(box, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(box, IDOK));
        }
        else printf("NOBOX\n");
        return 0;
    }
    if (!lstrcmpW(argv[1], L"create"))
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 0;
        if (CreateProcessW(NULL, argv[2], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            printf("CREATE ok %lu\n", code);
        }
        else printf("CREATE err %lu\n", GetLastError());
        return 0;
    }
    if (!lstrcmpW(argv[1], L"shell"))
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpFile = argv[2];
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei))
        {
            if (sei.hProcess) WaitForSingleObject(sei.hProcess, 20000);
            printf("SHELL ok\n");
        }
        else printf("SHELL err %lu\n", GetLastError());
        return 0;
    }
    return 2;
}
