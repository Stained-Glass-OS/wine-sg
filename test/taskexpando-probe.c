/* taskexpando-probe: a task dialog's "See details" control is as wide as its arrow and its text
 * (patches/sg/9662). Built with a common-controls 6 manifest (the gate's .rc). */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static int rc = 1;
static BOOL CALLBACK find_expando(HWND h, LPARAM l)
{
    WCHAR cls[32], text[64];
    GetClassNameW(h, cls, 32);
    GetWindowTextW(h, text, 64);
    if (!wcsicmp(cls, L"Button") && !wcscmp(text, L"See details"))
    {
        RECT r;
        SIZE sz;
        HDC dc = GetDC(h);
        HFONT old = SelectObject(dc, (HFONT)SendMessageW(h, WM_GETFONT, 0, 0));
        GetClientRect(h, &r);
        GetTextExtentPoint32W(dc, text, lstrlenW(text), &sz);
        SelectObject(dc, old);
        ReleaseDC(h, dc);
        /* the arrow comes first: the control must be wider than its text by at least the arrow */
        rc = r.right - r.left >= sz.cx + 8 ? 0 : 1;
        printf("%s  \"See details\" is %ld px wide, its text %ld px\n", rc ? "FAIL" : "PASS", r.right - r.left, sz.cx);
        return FALSE;
    }
    return TRUE;
}
static HRESULT CALLBACK cb(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LONG_PTR data)
{
    if (msg == TDN_CREATED)
    {
        EnumChildWindows(hwnd, find_expando, 0);
        PostMessageW(hwnd, TDM_CLICK_BUTTON, IDCANCEL, 0);
    }
    return S_OK;
}
int main(void)
{
    TASKDIALOGCONFIG tc = { sizeof(tc) };
    int button;
    tc.pszWindowTitle = L"Network Error";
    tc.pszMainInstruction = L"Cannot access \\\\server";
    tc.pszContent = L"Check the spelling of the name.";
    tc.pszExpandedInformation = L"Error code: 0x80070035\nThe network path was not found.";
    tc.pszCollapsedControlText = L"See details";
    tc.pszExpandedControlText = L"Hide details";
    tc.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    tc.pfCallback = cb;
    if (FAILED(TaskDialogIndirect(&tc, &button, NULL, NULL))) { printf("FAIL  no task dialog\n"); return 1; }
    printf("RESULT: %s\n", rc ? "FAIL" : "PASS");
    return rc;
}
