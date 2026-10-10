/* comctl32 batch (patches/sg/2027): toolbar separators get the default
 * width 8 in TB_GETBUTTON when added without one, TB_GETSTYLE has the
 * window's visible bit, and TBSTYLE_EX_VERTICAL sets CCS_VERT. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static HWND make(DWORD style)
{
    return CreateWindowExA(0, TOOLBARCLASSNAMEA, "", WS_CHILD | WS_VISIBLE | style, 0, 0, 300, 30,
                           CreateWindowA("static", "p", WS_OVERLAPPEDWINDOW, 0, 0, 400, 200, NULL, NULL, NULL, NULL),
                           NULL, GetModuleHandleA(NULL), NULL);
}

static int sep_bitmap(int ibitmap, BYTE style)
{
    HWND tb = make(0);
    TBBUTTON b = { 0 }, got = { 0 };
    int r;
    SendMessageA(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    b.iBitmap = ibitmap; b.idCommand = 5; b.fsStyle = style; b.fsState = TBSTATE_ENABLED; b.iString = -1;
    SendMessageA(tb, TB_ADDBUTTONSA, 1, (LPARAM)&b);
    SendMessageA(tb, TB_GETBUTTON, 0, (LPARAM)&got);
    r = got.iBitmap;
    DestroyWindow(tb);
    return r;
}

int main(void)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    HWND tb;
    DWORD style, wstyle;
    InitCommonControlsEx(&icc);

    check(sep_bitmap(0, BTNS_SEP) == 8, "separator with width 0 is 8");
    check(sep_bitmap(-3, BTNS_SEP) == 8, "separator with width -3 is 8");
    check(sep_bitmap(20, BTNS_SEP) == 20, "separator with width 20 keeps 20");
    check(sep_bitmap(0, BTNS_BUTTON) == 0, "a button's bitmap 0 stays 0");
    check(sep_bitmap(-3, BTNS_BUTTON) == -3, "a button's bitmap -3 stays");

    {
        TBBUTTON bb[3] = { { 0, 1000, TBSTATE_ENABLED, BTNS_CHECKGROUP }, { 0, 1001, TBSTATE_ENABLED, BTNS_SEP }, { 0, 1002, TBSTATE_ENABLED, BTNS_CHECKGROUP } };
        HWND par = CreateWindowA("static", "p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 200, NULL, NULL, NULL, NULL);
        tb = CreateToolbarEx(par, WS_VISIBLE | WS_CLIPCHILDREN | CCS_TOP | WS_CHILD | TBSTYLE_LIST, 100, 0, NULL, 0,
                             bb, 3, 0, 0, 20, 16, sizeof(TBBUTTON));
        SendMessageA(tb, TB_ADDSTRINGA, 0, (LPARAM)"test\000");
    }
    style = SendMessageA(tb, TB_GETSTYLE, 0, 0);
    wstyle = GetWindowLongA(tb, GWL_STYLE);
    check(style == wstyle, "TB_GETSTYLE equals the window style of a visible toolbar");
    check((style & WS_VISIBLE) != 0, "TB_GETSTYLE has the visible bit");
    DestroyWindow(tb);

    tb = make(0);
    SendMessageA(tb, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_VERTICAL);
    check((SendMessageA(tb, TB_GETSTYLE, 0, 0) & CCS_VERT) == CCS_VERT, "EX_VERTICAL sets CCS_VERT");
    DestroyWindow(tb);

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
