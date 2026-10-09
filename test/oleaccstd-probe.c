/* oleacc's standard objects (patches/sg/1687), run by test/oleaccstd-gate.sh
 * on Xvfb (Wine draws the frames: Decorated=N): a window's OBJID_WINDOW
 * object and its parts (title bar, menu bar, system menu, scroll bars,
 * size grip), the caret and the cursor, and a button's client object.
 * The window object was nearly all E_NOTIMPL, the parts and the caret and
 * cursor were "unhandled object id", and buttons had no role or action. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleacc.h>
#include <stdio.h>

#ifndef INDEX_TITLEBAR_CLOSEBUTTON
#define INDEX_TITLEBAR_MINBUTTON 2
#define INDEX_TITLEBAR_CLOSEBUTTON 5
#define INDEX_SCROLLBAR_DOWN 5
#define INDEX_SCROLLBAR_THUMB 3
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int closes, scrolls, clicks;

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SYSCOMMAND && (wp & 0xfff0) == SC_CLOSE) { closes++; return 0; }
    if (msg == WM_VSCROLL && LOWORD(wp) == SB_LINEDOWN) scrolls++;
    if (msg == WM_COMMAND && HIWORD(wp) == BN_CLICKED) clicks++;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

static VARIANT self_id(void)
{
    VARIANT v;
    V_VT(&v) = VT_I4;
    V_I4(&v) = CHILDID_SELF;
    return v;
}

static VARIANT child_id(LONG id)
{
    VARIANT v;
    V_VT(&v) = VT_I4;
    V_I4(&v) = id;
    return v;
}

static LONG role_of(IAccessible *acc, VARIANT id)
{
    VARIANT v;
    VariantInit(&v);
    if (FAILED(IAccessible_get_accRole(acc, id, &v)) || V_VT(&v) != VT_I4) return -1;
    return V_I4(&v);
}

static LONG state_of(IAccessible *acc, VARIANT id)
{
    VARIANT v;
    VariantInit(&v);
    if (FAILED(IAccessible_get_accState(acc, id, &v)) || V_VT(&v) != VT_I4) return -1;
    return V_I4(&v);
}

static int name_is(IAccessible *acc, VARIANT id, const WCHAR *want)
{
    BSTR s = NULL;
    int ok;
    IAccessible_get_accName(acc, id, &s);
    ok = s && !lstrcmpW(s, want);
    if (!ok) printf("      name %ls\n", s ? s : L"(none)");
    SysFreeString(s);
    return ok;
}

static IAccessible *object(HWND hwnd, LONG objid)
{
    IAccessible *acc = NULL;
    HRESULT hr = AccessibleObjectFromWindow(hwnd, objid, &IID_IAccessible, (void **)&acc);
    if (FAILED(hr)) { printf("      object %ld: %08lx\n", objid, hr); return NULL; }
    return acc;
}

int main(void)
{
    WNDCLASSW wc = { 0 };
    IAccessible *win, *part, *acc;
    IEnumVARIANT *en;
    HWND hwnd, button;
    HMENU menu, popup;
    VARIANT v, vs[8];
    LONG count = 0, l, t, w, h;
    ULONG fetched = 0;
    BSTR s = NULL;
    RECT rc;
    SCROLLINFO si = { sizeof(si), SIF_ALL, 0, 100, 10, 0 };
    HRESULT hr;

    CoInitialize(NULL);
    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = L"SgOleaccStd";
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    RegisterClassW(&wc);
    menu = CreateMenu();
    popup = CreatePopupMenu();
    AppendMenuW(popup, MF_STRING, 101, L"&Open");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)popup, L"&File");
    AppendMenuW(menu, MF_STRING, 102, L"&Help");
    hwnd = CreateWindowW(L"SgOleaccStd", L"SG std objects", WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE,
                         50, 50, 400, 300, NULL, menu, NULL, NULL);
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    button = CreateWindowW(L"BUTTON", L"&Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 10, 80, 30, hwnd,
                           (HMENU)7, NULL, NULL);
    SetForegroundWindow(hwnd);
    pump(300);

    /* the window object */
    win = object(hwnd, OBJID_WINDOW);
    check(win != NULL, "OBJID_WINDOW");
    if (!win) goto done;
    check(role_of(win, self_id()) == ROLE_SYSTEM_WINDOW, "the window's role: ROLE_SYSTEM_WINDOW");
    check(name_is(win, self_id(), L"SG std objects"), "its name: the window's text");
    check(IAccessible_get_accChildCount(win, &count) == S_OK && count == 7, "seven parts");
    l = state_of(win, self_id());
    check(l >= 0 && (l & STATE_SYSTEM_MOVEABLE) && (l & STATE_SYSTEM_SIZEABLE) && !(l & STATE_SYSTEM_INVISIBLE),
          "its state: moveable, sizeable, visible");
    GetWindowRect(hwnd, &rc);
    check(IAccessible_accLocation(win, &l, &t, &w, &h, self_id()) == S_OK && l == rc.left && t == rc.top &&
          w == rc.right - rc.left, "accLocation: the window's rectangle");
    VariantInit(&v);
    hr = IAccessible_accNavigate(win, NAVDIR_FIRSTCHILD, self_id(), &v);
    check(hr == S_OK && V_VT(&v) == VT_DISPATCH, "accNavigate(NAVDIR_FIRSTCHILD): a part");
    if (V_VT(&v) == VT_DISPATCH)
    {
        IDispatch_QueryInterface(V_DISPATCH(&v), &IID_IAccessible, (void **)&acc);
        check(acc && role_of(acc, self_id()) == ROLE_SYSTEM_MENUBAR, "the system menu first");
        if (acc) IAccessible_Release(acc);
    }
    VariantClear(&v);
    check(SUCCEEDED(IAccessible_QueryInterface(win, &IID_IEnumVARIANT, (void **)&en)) &&
          IEnumVARIANT_Next(en, 8, vs, &fetched) == S_FALSE && fetched == 7, "IEnumVARIANT: the seven parts");
    if (en)
    {
        ULONG i;
        for (i = 0; i < fetched; i++) VariantClear(&vs[i]);
        IEnumVARIANT_Release(en);
    }
    {
        TITLEBARINFO ti = { sizeof(ti) };
        GetTitleBarInfo(hwnd, &ti);
        VariantInit(&v);
        hr = IAccessible_accHitTest(win, (ti.rcTitleBar.left + ti.rcTitleBar.right) / 2,
                                    (ti.rcTitleBar.top + ti.rcTitleBar.bottom) / 2, &v);
        acc = NULL;
        if (V_VT(&v) == VT_DISPATCH) IDispatch_QueryInterface(V_DISPATCH(&v), &IID_IAccessible, (void **)&acc);
        VariantClear(&v);
        check(acc && role_of(acc, self_id()) == ROLE_SYSTEM_TITLEBAR, "accHitTest on the caption: the title bar");
        if (acc) IAccessible_Release(acc);
    }

    /* the title bar */
    part = object(hwnd, OBJID_TITLEBAR);
    check(part != NULL, "OBJID_TITLEBAR (was unhandled)");
    if (part)
    {
        check(role_of(part, self_id()) == ROLE_SYSTEM_TITLEBAR, "ROLE_SYSTEM_TITLEBAR");
        check(IAccessible_get_accChildCount(part, &count) == S_OK && count == 5, "five buttons");
        check(role_of(part, child_id(INDEX_TITLEBAR_CLOSEBUTTON)) == ROLE_SYSTEM_PUSHBUTTON &&
              name_is(part, child_id(INDEX_TITLEBAR_CLOSEBUTTON), L"Close"), "the close button");
        check(IAccessible_accLocation(part, &l, &t, &w, &h, child_id(INDEX_TITLEBAR_CLOSEBUTTON)) == S_OK && w > 0,
              "where it is");
        check(IAccessible_accDoDefaultAction(part, child_id(INDEX_TITLEBAR_CLOSEBUTTON)) == S_OK, "pressing it");
        pump(200);
        check(closes == 1, "sends SC_CLOSE");
        IAccessible_Release(part);
    }

    /* the menu bar */
    part = object(hwnd, OBJID_MENU);
    check(part != NULL, "OBJID_MENU");
    if (part)
    {
        check(IAccessible_get_accChildCount(part, &count) == S_OK && count == 2, "two menu items");
        check(role_of(part, child_id(1)) == ROLE_SYSTEM_MENUITEM && name_is(part, child_id(1), L"File"),
              "the first: File, a menu item");
        IAccessible_get_accKeyboardShortcut(part, child_id(1), &s);
        check(s && !lstrcmpW(s, L"Alt+F"), "its shortcut: Alt+F");
        SysFreeString(s); s = NULL;
        check(state_of(part, child_id(1)) & STATE_SYSTEM_HASPOPUP, "it has a popup");
        IAccessible_Release(part);
    }

    /* the vertical scroll bar */
    part = object(hwnd, OBJID_VSCROLL);
    check(part != NULL, "OBJID_VSCROLL");
    if (part)
    {
        check(role_of(part, self_id()) == ROLE_SYSTEM_SCROLLBAR, "ROLE_SYSTEM_SCROLLBAR");
        check(role_of(part, child_id(INDEX_SCROLLBAR_THUMB)) == ROLE_SYSTEM_INDICATOR, "the thumb: ROLE_SYSTEM_INDICATOR");
        check(IAccessible_put_accValue(part, self_id(), (BSTR)L"50") == S_OK, "put_accValue(50)");
        IAccessible_get_accValue(part, self_id(), &s);
        check(s && !lstrcmpW(s, L"50"), "get_accValue: 50");
        SysFreeString(s); s = NULL;
        check(IAccessible_accDoDefaultAction(part, child_id(INDEX_SCROLLBAR_DOWN)) == S_OK && scrolls == 1,
              "the down arrow sends SB_LINEDOWN");
        IAccessible_Release(part);
    }
    part = object(hwnd, OBJID_SIZEGRIP);
    check(part && role_of(part, self_id()) == ROLE_SYSTEM_GRIP && !(state_of(part, self_id()) & STATE_SYSTEM_INVISIBLE),
          "OBJID_SIZEGRIP: a grip, shown with both scroll bars");
    if (part) IAccessible_Release(part);

    /* the cursor and the caret */
    part = NULL;
    hr = AccessibleObjectFromWindow(NULL, OBJID_CURSOR, &IID_IAccessible, (void **)&part);
    check(hr == S_OK && part && role_of(part, self_id()) == ROLE_SYSTEM_CURSOR, "OBJID_CURSOR without a window");
    if (part) IAccessible_Release(part);
    CreateCaret(hwnd, NULL, 2, 16);
    SetCaretPos(30, 60);
    part = object(hwnd, OBJID_CARET);
    check(part && role_of(part, self_id()) == ROLE_SYSTEM_CARET, "OBJID_CARET");
    if (part)
    {
        POINT pt = { 30, 60 };
        ClientToScreen(hwnd, &pt);
        check(IAccessible_accLocation(part, &l, &t, &w, &h, self_id()) == S_OK && l == pt.x && t == pt.y && h == 16,
              "the caret's place");
        IAccessible_Release(part);
    }
    DestroyCaret();

    /* a button's client object */
    acc = object(button, OBJID_CLIENT);
    check(acc && role_of(acc, self_id()) == ROLE_SYSTEM_PUSHBUTTON, "a button: ROLE_SYSTEM_PUSHBUTTON");
    if (acc)
    {
        IAccessible_get_accDefaultAction(acc, self_id(), &s);
        check(s && !lstrcmpW(s, L"Press"), "its default action: Press");
        SysFreeString(s);
        check(IAccessible_accDoDefaultAction(acc, self_id()) == S_OK, "accDoDefaultAction");
        pump(300);
        check(clicks == 1, "clicks it");
        IAccessible_Release(acc);
    }
    IAccessible_Release(win);

done:
    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
