/* kbdlayout-probe -- for test/kbdlayout-gate.sh (wine-sg 0250, 0251).
 *
 *   labels OUT          per scan code: the virtual key and what ToUnicodeEx
 *                       gives plain, with Shift and with Ctrl+Alt (AltGr)
 *   follow OUT READY GO a window with an edit control, in front; writes READY,
 *                       waits for the file GO (the gate switches the X layout
 *                       meanwhile), then writes the labels and types the keys
 *                       at scan codes 0x15, 0x1a and Ctrl+Alt+0x10 into the
 *                       edit with SendInput, and writes what the edit got
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

static const UINT scs[] = { 0x02, 0x03, 0x04, 0x0c, 0x0d, 0x10, 0x15, 0x1a, 0x1b, 0x27, 0x28, 0x2c, 0x56 };

static void one(FILE *f, UINT sc, BYTE *st)
{
    HKL hkl = GetKeyboardLayout(0);
    UINT vk = MapVirtualKeyExW(sc, MAPVK_VSC_TO_VK_EX, hkl);
    WCHAR b[8] = {0};
    int r = vk ? ToUnicodeEx(vk, sc, st, b, 8, 0, hkl) : 0;
    if (r == 0) fprintf(f, " -");
    else fprintf(f, " %04x", b[0]);
}

static void labels(FILE *f)
{
    BYTE st[256];
    for (int i = 0; i < (int)(sizeof(scs) / sizeof(*scs)); i++)
    {
        fprintf(f, "sc %02x vk %02x", scs[i], MapVirtualKeyExW(scs[i], MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0)));
        memset(st, 0, 256); one(f, scs[i], st);
        st[VK_SHIFT] = st[VK_LSHIFT] = 0x80; one(f, scs[i], st);
        memset(st, 0, 256); st[VK_CONTROL] = st[VK_LCONTROL] = st[VK_MENU] = st[VK_RMENU] = 0x80; one(f, scs[i], st);
        fprintf(f, "\n");
    }
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 20, QS_ALLINPUT);
    }
}

static void key(INPUT *in, int *n, UINT vk, UINT sc, BOOL up, BOOL ext)
{
    memset(&in[*n], 0, sizeof(INPUT));
    in[*n].type = INPUT_KEYBOARD;
    in[*n].ki.wVk = vk;
    in[*n].ki.wScan = sc;
    in[*n].ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (ext ? KEYEVENTF_EXTENDEDKEY : 0);
    (*n)++;
}

int wmain(int argc, WCHAR **argv)
{
    FILE *f;
    if (argc >= 3 && !wcscmp(argv[1], L"labels"))
    {
        HWND w = CreateWindowW(L"STATIC", L"probe", WS_OVERLAPPEDWINDOW, 0, 0, 50, 50, NULL, NULL, NULL, NULL);
        (void)w;
        if (!(f = _wfopen(argv[2], L"w"))) return 1;
        labels(f);
        fclose(f);
        return 0;
    }
    if (argc >= 5 && !wcscmp(argv[1], L"follow"))
    {
        HWND w = CreateWindowW(L"STATIC", L"kbdlayout probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 400, 200, NULL, NULL, NULL, NULL);
        HWND ed = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 360, 30, w, NULL, NULL, NULL);
        INPUT in[16];
        UINT vk;
        int n = 0;
        WCHAR text[64];
        SetForegroundWindow(w);
        SetFocus(ed);
        pump(1000);
        if ((f = _wfopen(argv[3], L"w"))) fclose(f);
        while (GetFileAttributesW(argv[4]) == INVALID_FILE_ATTRIBUTES) pump(100);
        pump(500);
        if (!(f = _wfopen(argv[2], L"w"))) return 1;
        labels(f);
        SetForegroundWindow(w);
        SetFocus(ed);
        vk = MapVirtualKeyExW(0x15, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
        key(in, &n, vk, 0x15, FALSE, FALSE); key(in, &n, vk, 0x15, TRUE, FALSE);
        vk = MapVirtualKeyExW(0x1a, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
        key(in, &n, vk, 0x1a, FALSE, FALSE); key(in, &n, vk, 0x1a, TRUE, FALSE);
        vk = MapVirtualKeyExW(0x10, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
        key(in, &n, VK_LCONTROL, 0x1d, FALSE, FALSE); key(in, &n, VK_RMENU, 0x38, FALSE, TRUE);
        key(in, &n, vk, 0x10, FALSE, FALSE); key(in, &n, vk, 0x10, TRUE, FALSE);
        key(in, &n, VK_RMENU, 0x38, TRUE, TRUE); key(in, &n, VK_LCONTROL, 0x1d, TRUE, FALSE);
        SendInput(n, in, sizeof(INPUT));
        pump(1500);
        GetWindowTextW(ed, text, 64);
        fprintf(f, "text");
        for (int i = 0; text[i]; i++) fprintf(f, " %04x", text[i]);
        fprintf(f, "\n");
        fclose(f);
        return 0;
    }
    return 2;
}
