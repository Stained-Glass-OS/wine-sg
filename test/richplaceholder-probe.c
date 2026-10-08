/* richplaceholder-probe.c (test/richplaceholder-gate.sh, wine-sg 1511): a
 * RichEdit's placeholder text (EM_SETTEXTEX, ST_PLACEHOLDERTEXT) -- not the
 * control's text, shown greyed while it is empty, gone when the user types
 * (Word's start screen box: typing went after "Describe the document you'd
 * like to write"). */
#include <windows.h>
#include <richedit.h>

#ifndef ST_PLACEHOLDERTEXT
#define ST_PLACEHOLDERTEXT 0x10
#endif

static HANDLE out;
static int failures;
static void say(const char *fmt, ...)
{
    char buf[512]; DWORD n; va_list ap;
    va_start(ap, fmt); n = wvsprintfA(buf, fmt, ap); va_end(ap);
    WriteFile(out, buf, n, &n, NULL);
}
static void check(BOOL ok, const char *what) { say("%s  %s\r\n", ok ? "PASS" : "FAIL", what); if (!ok) failures++; }

/* the window's pixels in the grey text colour */
static int grey_pixels(HWND w)
{
    RECT r; HDC dc, mem; HBITMAP bm; int x, y, n = 0;
    COLORREF grey = GetSysColor(COLOR_GRAYTEXT);
    MSG msg;
    InvalidateRect(w, NULL, TRUE); UpdateWindow(w);
    while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    Sleep(300);
    GetClientRect(w, &r);
    dc = GetDC(w); mem = CreateCompatibleDC(dc); bm = CreateCompatibleBitmap(dc, r.right, r.bottom);
    SelectObject(mem, bm);
    BitBlt(mem, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
    for (y = 0; y < r.bottom; y++)
        for (x = 0; x < r.right; x++)
            if (GetPixel(mem, x, y) == grey) n++;
    DeleteDC(mem); DeleteObject(bm); ReleaseDC(w, dc);
    return n;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    SETTEXTEX st = { ST_PLACEHOLDERTEXT, 1200 };
    WCHAR text[64];
    HWND w;
    int n0, n1, n2;
    LRESULT r;

    out = GetStdHandle(STD_OUTPUT_HANDLE);
    LoadLibraryA("msftedit.dll");
    w = CreateWindowExW(0, L"RICHEDIT50W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 10, 10, 420, 60, 0, 0, inst, 0);
    SendMessageW(w, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
    n0 = grey_pixels(w);
    r = SendMessageW(w, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)L"Describe the document");
    check(r != 0, "EM_SETTEXTEX with ST_PLACEHOLDERTEXT is taken");
    check(!SendMessageW(w, WM_GETTEXTLENGTH, 0, 0), "...the placeholder is not the control's text");
    n1 = grey_pixels(w);
    say("      grey pixels: %d empty, %d with the placeholder\r\n", n0, n1);
    check(n1 > n0 + 40, "...it is shown, greyed, while the control is empty");
    SetFocus(w);
    SendMessageW(w, WM_CHAR, 'H', 0);
    GetWindowTextW(w, text, 64);
    check(!lstrcmpW(text, L"H"), "a typed character replaces it: the text is \"H\"");
    n2 = grey_pixels(w);
    check(n2 <= n0 + 5, "...and the placeholder is no longer shown");
    SetWindowTextW(w, L"");
    check(grey_pixels(w) > n0 + 40, "emptied again, the placeholder shows again");
    DestroyWindow(w);
    say("RESULT: %s\r\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
