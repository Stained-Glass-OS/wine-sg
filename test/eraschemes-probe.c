/* eraschemes-gate.sh's hands (0743): the controls' parts of the colour
 * scheme in use, drawn by uxtheme on a green window at fixed places.
 *   eraschemes-probe show     the window at 0,0 (600x200), until killed:
 *                               20,20 120x30   a push button (normal)
 *                               180,20         a check box's image, checked
 *                               220,20         a radio button's image, checked
 *                               260,20 17x60   a vertical scroll bar thumb
 *   eraschemes-probe colour   the colour scheme and three system colours
 *   eraschemes-probe notify   broadcast WM_SETTINGCHANGE "ImmersiveColorSet"
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HTHEME button = OpenThemeData(hwnd, L"Button"), scroll = OpenThemeData(hwnd, L"Scrollbar");
        RECT r;
        SIZE sz = { 13, 13 };
        if (button)
        {
            SetRect(&r, 20, 20, 140, 50);
            DrawThemeBackground(button, hdc, BP_PUSHBUTTON, PBS_NORMAL, &r, NULL);
            GetThemePartSize(button, hdc, BP_CHECKBOX, CBS_CHECKEDNORMAL, NULL, TS_DRAW, &sz);
            SetRect(&r, 180, 20, 180 + sz.cx, 20 + sz.cy);
            DrawThemeBackground(button, hdc, BP_CHECKBOX, CBS_CHECKEDNORMAL, &r, NULL);
            GetThemePartSize(button, hdc, BP_RADIOBUTTON, RBS_CHECKEDNORMAL, NULL, TS_DRAW, &sz);
            SetRect(&r, 220, 20, 220 + sz.cx, 20 + sz.cy);
            DrawThemeBackground(button, hdc, BP_RADIOBUTTON, RBS_CHECKEDNORMAL, &r, NULL);
            CloseThemeData(button);
        }
        if (scroll)
        {
            SetRect(&r, 260, 20, 277, 80);
            DrawThemeBackground(scroll, hdc, SBP_THUMBBTNVERT, SCRBS_NORMAL, &r, NULL);
            CloseThemeData(scroll);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int wmain(int argc, WCHAR **argv)
{
    if (argc >= 2 && !lstrcmpW(argv[1], L"show"))
    {
        WNDCLASSW wc = { 0 };
        MSG msg;
        wc.lpfnWndProc = proc;
        wc.lpszClassName = L"EraSchemes";
        wc.hbrBackground = CreateSolidBrush(RGB(0, 255, 0));
        RegisterClassW(&wc);
        CreateWindowW(L"EraSchemes", L"EraSchemes", WS_POPUP | WS_VISIBLE, 0, 0, 600, 200, 0, 0, 0, 0);
        while (GetMessageW(&msg, 0, 0, 0)) DispatchMessageW(&msg);
        return 0;
    }
    if (argc >= 2 && !lstrcmpW(argv[1], L"colour"))
    {
        WCHAR file[MAX_PATH] = L"", colour[64] = L"", size[64] = L"";
        COLORREF f = GetSysColor(COLOR_BTNFACE), h = GetSysColor(COLOR_HIGHLIGHT), m = GetSysColor(COLOR_MENUBAR);
        GetCurrentThemeName(file, MAX_PATH, colour, 64, size, 64);
        printf("colour=%ls\nbtnface=%d,%d,%d\nhighlight=%d,%d,%d\nmenubar=%d,%d,%d\n", colour,
               GetRValue(f), GetGValue(f), GetBValue(f), GetRValue(h), GetGValue(h), GetBValue(h),
               GetRValue(m), GetGValue(m), GetBValue(m));
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
