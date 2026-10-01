/* roundedctl-gate.sh's hands (0740): controls whose frames the Rounded
 * style rounds, on a green window, at fixed places.
 *   roundedctl-probe show     the window at 0,0 (600x300), until killed:
 *                               20,20 200x26   edit box
 *                               20,60 200x26   edit box, focused
 *                               240,20 150x80  list box
 *                               240,120 150x26 list view (report, no header)
 *                               240,170 150    combo box with an edit box
 *                               420,20         a radio button's image, unchecked
 *                               420,60 120x30  EP_EDITTEXT drawn by DrawThemeBackground
 *   roundedctl-probe colour   the colour scheme in use
 *   roundedctl-probe notify   broadcast WM_SETTINGCHANGE "ImmersiveColorSet"
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static HWND focus_me;

static HWND add(HWND parent, const WCHAR *cls, DWORD style, int x, int y, int w, int h)
{
    return CreateWindowExW(WS_EX_CLIENTEDGE, cls, L"", WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent, 0, 0, 0);
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        add(hwnd, L"EDIT", ES_AUTOHSCROLL, 20, 20, 200, 26);
        focus_me = add(hwnd, L"EDIT", ES_AUTOHSCROLL, 20, 60, 200, 26);
        add(hwnd, L"LISTBOX", LBS_NOINTEGRALHEIGHT, 240, 20, 150, 80);
        add(hwnd, WC_LISTVIEWW, LVS_REPORT | LVS_NOCOLUMNHEADER, 240, 120, 150, 26);
        CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN, 240, 170, 150, 200, hwnd, 0, 0, 0);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HTHEME button = OpenThemeData(hwnd, L"Button"), edit = OpenThemeData(hwnd, L"Edit");
        RECT r;
        SIZE sz = { 13, 13 };
        if (button)
        {
            GetThemePartSize(button, hdc, BP_RADIOBUTTON, RBS_UNCHECKEDNORMAL, NULL, TS_DRAW, &sz);
            SetRect(&r, 420, 20, 420 + sz.cx, 20 + sz.cy);
            DrawThemeBackground(button, hdc, BP_RADIOBUTTON, RBS_UNCHECKEDNORMAL, &r, NULL);
            CloseThemeData(button);
        }
        if (edit)
        {
            SetRect(&r, 420, 60, 540, 90);
            DrawThemeBackground(edit, hdc, EP_EDITTEXT, ETS_NORMAL, &r, NULL);
            CloseThemeData(edit);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int wmain(int argc, WCHAR **argv)
{
    if (argc >= 2 && !lstrcmpW(argv[1], L"show"))
    {
        WNDCLASSW wc = { 0 };
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
        HWND w;
        MSG msg;
        InitCommonControlsEx(&icc);
        wc.lpfnWndProc = proc;
        wc.lpszClassName = L"RoundedCtl";
        wc.hbrBackground = CreateSolidBrush(RGB(0, 255, 0));
        wc.hCursor = LoadCursorW(0, (const WCHAR *)IDC_ARROW);
        RegisterClassW(&wc);
        w = CreateWindowW(L"RoundedCtl", L"RoundedCtl", WS_POPUP | WS_VISIBLE, 0, 0, 600, 300, 0, 0, 0, 0);
        SetForegroundWindow(w);
        SetFocus(focus_me);
        while (GetMessageW(&msg, 0, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return 0;
    }
    if (argc >= 2 && !lstrcmpW(argv[1], L"colour"))
    {
        WCHAR file[MAX_PATH] = L"", colour[64] = L"", size[64] = L"";
        GetCurrentThemeName(file, MAX_PATH, colour, 64, size, 64);
        printf("colour=%ls\n", colour);
        return 0;
    }
    if (argc >= 2 && !lstrcmpW(argv[1], L"notify"))
    {
        DWORD_PTR res;
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, &res);
        return 0;
    }
    return 2;
}
