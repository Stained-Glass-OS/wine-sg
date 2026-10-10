/* comctl32 batch (patches/sg/2026): tab control focus and selection rules
 * (button style keeps its focus on a negative index and on a selection
 * change; a vetoed change moves neither focus nor selection) and
 * AddMRUStringA with a NULL string. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static int failures, veto;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static LRESULT CALLBACK parent_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_NOTIFY && ((NMHDR *)l)->code == TCN_SELCHANGING) return veto;
    return DefWindowProcA(h, m, w, l);
}

static HWND make_tab(HWND parent, DWORD style)
{
    HWND tab = CreateWindowA(WC_TABCONTROLA, "", WS_CHILD | WS_VISIBLE | style, 0, 0, 400, 100, parent, NULL, GetModuleHandleA(NULL), NULL);
    TCITEMA item = { TCIF_TEXT };
    int i;
    for (i = 0; i < 5; i++) { item.pszText = (char *)"tab"; SendMessageA(tab, TCM_INSERTITEMA, i, (LPARAM)&item); }
    return tab;
}

typedef struct { DWORD cbSize; UINT uMax; UINT fFlags; HKEY hKey; LPCSTR sub; void *cmp; } MRUINFOA;

int main(void)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_TAB_CLASSES };
    WNDCLASSA wc = { 0 };
    HWND parent, tab;
    LRESULT r;
    char buf[100];

    InitCommonControlsEx(&icc);
    wc.lpfnWndProc = parent_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "sgtabparent";
    RegisterClassA(&wc);
    parent = CreateWindowA("sgtabparent", "p", WS_OVERLAPPEDWINDOW, 0, 0, 500, 200, NULL, NULL, wc.hInstance, NULL);

    /* button style */
    tab = make_tab(parent, TCS_BUTTONS);
    SendMessageA(tab, TCM_SETCURFOCUS, 4, 0);
    check(SendMessageA(tab, TCM_GETCURFOCUS, 0, 0) == 4, "buttons: focus set to 4");
    SendMessageA(tab, TCM_SETCURFOCUS, -10, 0);
    check(SendMessageA(tab, TCM_GETCURFOCUS, 0, 0) == 4, "buttons: negative focus ignored");
    check(SendMessageA(tab, TCM_GETCURSEL, 0, 0) == 0, "buttons: negative focus keeps the selection");
    r = SendMessageA(tab, TCM_SETCURSEL, 1, 0);
    check(r == 0, "buttons: SETCURSEL returns the previous selection");
    check(SendMessageA(tab, TCM_GETCURFOCUS, 0, 0) == 4, "buttons: selection change keeps the focus");
    check(SendMessageA(tab, TCM_GETCURSEL, 0, 0) == 1, "buttons: selection is 1");
    DestroyWindow(tab);

    /* plain tabs */
    tab = make_tab(parent, 0);
    SendMessageA(tab, TCM_SETCURSEL, 1, 0);
    check(SendMessageA(tab, TCM_GETCURFOCUS, 0, 0) == 1, "tabs: selection change moves the focus");
    veto = 0;
    SendMessageA(tab, TCM_SETCURFOCUS, 4, 0);
    check(SendMessageA(tab, TCM_GETCURFOCUS, 0, 0) == 4 && SendMessageA(tab, TCM_GETCURSEL, 0, 0) == 4, "tabs: focus change moves both");
    veto = 1;
    SendMessageA(tab, TCM_SETCURFOCUS, 0, 0);
    check(SendMessageA(tab, TCM_GETCURFOCUS, 0, 0) == 4, "tabs: vetoed change keeps the focus");
    check(SendMessageA(tab, TCM_GETCURSEL, 0, 0) == 4, "tabs: vetoed change keeps the selection");
    veto = 0;
    SendMessageA(tab, TCM_SETCURFOCUS, -1, 0);
    check(SendMessageA(tab, TCM_GETCURFOCUS, 0, 0) == -1 && SendMessageA(tab, TCM_GETCURSEL, 0, 0) == -1, "tabs: -1 removes focus and selection");
    DestroyWindow(tab);

    /* AddMRUStringA(NULL) */
    {
        HMODULE cc = GetModuleHandleA("comctl32.dll");
        HANDLE (WINAPI *create)(MRUINFOA *) = (void *)GetProcAddress(cc, (LPCSTR)151);
        void (WINAPI *freelist)(HANDLE) = (void *)GetProcAddress(cc, (LPCSTR)152);
        int (WINAPI *add)(HANDLE, const char *) = (void *)GetProcAddress(cc, (LPCSTR)153);
        MRUINFOA info = { sizeof(info), 3, 0, HKEY_CURRENT_USER, "Software\\SgTabMruProbe", NULL };
        HANDLE list;
        int ret;
        if (create && add && freelist && (list = create(&info)))
        {
            SetLastError(0);
            ret = add(list, NULL);
            snprintf(buf, sizeof(buf), "AddMRUStringA(NULL) = %d, error %lu", ret, GetLastError());
            check(ret == 0 && !GetLastError(), buf);
            ret = add(list, "one");
            check(ret == 0, "AddMRUStringA(\"one\") after the NULL");
            freelist(list);
            RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\SgTabMruProbe");
        }
        else check(0, "MRU list could not be created");
    }

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
