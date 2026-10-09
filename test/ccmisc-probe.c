/* comctl32 odds and ends (patches/sg/1672), run by test/ccmisc-gate.sh:
 * the date and time picker's app-defined 'X' fields (DTN_FORMATQUERY,
 * DTN_FORMAT, DTN_WMKEYDOWN); MirrorIcon; DrawShadowText's soft, offset
 * shadow; DelMRUString; PSM_RECALCPAGESIZES; a tooltip's
 * WM_NOTIFYFORMAT; tab items' TCIF_RTLREADING and TCS_EX_REGISTERDROP
 * (TCN_GETOBJECT on a drag). These were FIXMEs or stubs. */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static LONG formatquery_calls, format_calls, keydown_calls, getobject_calls, getobject_item = -1;

/* the tab's drop target */
struct target { IDropTarget iface; LONG enter, drop; };
static HRESULT WINAPI t_qi(IDropTarget *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI t_addref(IDropTarget *iface) { return 2; }
static ULONG WINAPI t_release(IDropTarget *iface) { return 1; }
static HRESULT WINAPI t_enter(IDropTarget *iface, IDataObject *d, DWORD k, POINTL pt, DWORD *e)
{ ((struct target *)iface)->enter++; *e = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI t_over(IDropTarget *iface, DWORD k, POINTL pt, DWORD *e) { *e = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI t_leave(IDropTarget *iface) { return S_OK; }
static HRESULT WINAPI t_drop(IDropTarget *iface, IDataObject *d, DWORD k, POINTL pt, DWORD *e)
{ ((struct target *)iface)->drop++; *e = DROPEFFECT_COPY; return S_OK; }
static IDropTargetVtbl t_vtbl = { t_qi, t_addref, t_release, t_enter, t_over, t_leave, t_drop };
static struct target target = { { &t_vtbl } };

static HRESULT WINAPI s_qi(IDropSource *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI s_addref(IDropSource *iface) { return 2; }
static ULONG WINAPI s_release(IDropSource *iface) { return 1; }
static int continue_calls;
static HRESULT WINAPI s_continue(IDropSource *iface, BOOL esc, DWORD keys) { return ++continue_calls > 2 ? DRAGDROP_S_DROP : S_OK; }
static HRESULT WINAPI s_feedback(IDropSource *iface, DWORD effect) { return DRAGDROP_S_USEDEFAULTCURSORS; }
static IDropSourceVtbl s_vtbl = { s_qi, s_addref, s_release, s_continue, s_feedback };

static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY)
    {
        NMHDR *hdr = (NMHDR *)lp;
        switch (hdr->code)
        {
        case DTN_FORMATQUERYW:
            formatquery_calls++;
            ((NMDATETIMEFORMATQUERYW *)lp)->szMax.cx = 40;
            ((NMDATETIMEFORMATQUERYW *)lp)->szMax.cy = 16;
            return 0;
        case DTN_FORMATW:
        {
            NMDATETIMEFORMATW *f = (NMDATETIMEFORMATW *)lp;
            format_calls++;
            if (!lstrcmpW(f->pszFormat, L"XX")) swprintf(f->szDisplay, 64, L"W%u", f->st.wDay);
            return 0;
        }
        case DTN_WMKEYDOWNW:
        {
            NMDATETIMEWMKEYDOWNW *k = (NMDATETIMEWMKEYDOWNW *)lp;
            keydown_calls++;
            if (k->nVirtKey == VK_UP && !lstrcmpW(k->pszFormat, L"XX")) k->st.wDay = 20;
            return 0;
        }
        case TCN_GETOBJECT:
        {
            NMOBJECTNOTIFY *nmo = (NMOBJECTNOTIFY *)lp;
            getobject_calls++;
            getobject_item = nmo->iItem;
            nmo->pObject = &target.iface;
            nmo->hResult = S_OK;
            return 0;
        }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK ansi_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFYFORMAT) return NFR_ANSI;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

typedef struct { DWORD cbSize; UINT uMax; UINT fFlags; HKEY hKey; LPCWSTR lpszSubKey; void *lpfnCompare; } MRUINFOW;

static BYTE dlg_template[64];
static DLGTEMPLATE *make_template(short cx, short cy)
{
    DLGTEMPLATE *t = (DLGTEMPLATE *)dlg_template;
    memset(dlg_template, 0, sizeof(dlg_template));
    t->style = WS_CHILD | DS_CONTROL;
    t->cx = cx;
    t->cy = cy;
    return t;
}

static INT_PTR CALLBACK page_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) { return FALSE; }

int main(void)
{
    WNDCLASSW wc = {0};
    HWND parent, dtp, tips, ansi, tab, sheet;
    SYSTEMTIME st = { 2026, 10, 4, 8, 12, 0, 0, 0 };
    HICON icon;
    ICONINFO ii;
    HDC dc;
    DWORD *bits;
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), 100, -40, 1, 32, BI_RGB }};
    HMODULE comctl = LoadLibraryA("comctl32.dll");
    BOOL (WINAPI *mirror)(HICON *, HICON *) = (void *)GetProcAddress(comctl, (LPCSTR)414);
    HANDLE (WINAPI *create_mru)(MRUINFOW *) = (void *)GetProcAddress(comctl, (LPCSTR)400);
    INT (WINAPI *add_mru)(HANDLE, LPCWSTR) = (void *)GetProcAddress(comctl, (LPCSTR)401);
    INT (WINAPI *enum_mru)(HANDLE, INT, void *, DWORD) = (void *)GetProcAddress(comctl, (LPCSTR)403);
    BOOL (WINAPI *del_mru)(HANDLE, INT) = (void *)GetProcAddress(comctl, (LPCSTR)156);
    void (WINAPI *free_mru)(HANDLE) = (void *)GetProcAddress(comctl, (LPCSTR)152);
    int i, pink;
    RECT rc, before, after;
    WCHAR buf[64];

    OleInitialize(NULL);
    InitCommonControls();
    wc.lpfnWndProc = parent_proc;
    wc.lpszClassName = L"ccmisc_parent";
    RegisterClassW(&wc);
    wc.lpfnWndProc = ansi_proc;
    wc.lpszClassName = L"ccmisc_ansi";
    RegisterClassW(&wc);
    parent = CreateWindowW(L"ccmisc_parent", L"ccmisc", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, NULL, NULL);

    /* the date and time picker's own fields */
    dtp = CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_CHILD | WS_VISIBLE, 10, 10, 300, 24, parent, NULL, NULL, NULL);
    SendMessageW(dtp, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    SendMessageW(dtp, DTM_SETFORMATW, 0, (LPARAM)L"'Week 'XX yyyy");
    UpdateWindow(dtp);
    RedrawWindow(dtp, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    check(formatquery_calls >= 1, "DTN_FORMATQUERY for the field's width");
    check(format_calls >= 1, "DTN_FORMAT for its text");
    SetFocus(dtp);
    /* the 'X' field, after the literal: clicked to select it */
    SendMessageW(dtp, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(50, 12));
    SendMessageW(dtp, WM_LBUTTONUP, 0, MAKELPARAM(50, 12));
    SendMessageW(dtp, WM_KEYDOWN, VK_UP, 0);
    memset(&st, 0, sizeof(st));
    SendMessageW(dtp, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    printf("      keydown %ld, day %u\n", keydown_calls, st.wDay);
    check(keydown_calls >= 1 && st.wDay == 20, "DTN_WMKEYDOWN: the parent changes the date");

    /* MirrorIcon */
    {
        BYTE and_bits[16 * 16 / 8], xor_bits[16 * 16 * 4];
        DWORD *px = (DWORD *)xor_bits;
        memset(and_bits, 0, sizeof(and_bits));
        for (i = 0; i < 256; i++) px[i] = (i % 16) < 8 ? 0xff000000 : 0xffffffff;
        icon = CreateIcon(NULL, 16, 16, 1, 32, and_bits, xor_bits);
    }
    {
        HICON before_icon = icon;
        check(mirror && mirror(&icon, NULL) && icon && icon != before_icon, "MirrorIcon: a new icon");
    }
    dc = CreateCompatibleDC(0);
    SelectObject(dc, CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0));
    for (i = 0; i < 100 * 40; i++) bits[i] = 0x808080;
    DrawIconEx(dc, 0, 0, icon, 16, 16, 0, NULL, DI_NORMAL);
    check((bits[5 * 100 + 2] & 0xffffff) == 0xffffff && (bits[5 * 100 + 13] & 0xffffff) == 0x000000,
          "flipped: white on the left now");
    (void)ii;

    /* DrawShadowText */
    for (i = 0; i < 100 * 40; i++) bits[i] = 0xffffff;
    SetRect(&rc, 5, 5, 95, 30);
    SelectObject(dc, GetStockObject(SYSTEM_FONT));
    DrawShadowText(dc, L"WWW", 3, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE, RGB(0, 0, 0), RGB(255, 0, 0), 4, 4);
    for (i = 0, pink = 0; i < 100 * 40; i++)
    {
        BYTE r = (bits[i] >> 16) & 0xff, g = (bits[i] >> 8) & 0xff;
        if (r == 0xff && g > 40 && g < 230) pink++;
    }
    printf("      %d soft shadow pixels\n", pink);
    check(pink > 5, "DrawShadowText: a soft shadow");
    DeleteDC(dc);

    /* DelMRUString */
    {
        MRUINFOW mi = { sizeof(mi), 5, 0, HKEY_CURRENT_USER, L"Software\\StainedGlass\\MruProbe", NULL };
        HANDLE mru;
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\StainedGlass\\MruProbe");
        mru = create_mru ? create_mru(&mi) : NULL;
        check(mru != NULL, "an MRU list");
        if (mru)
        {
            add_mru(mru, L"one");
            add_mru(mru, L"two");
            add_mru(mru, L"three");
            check(del_mru(mru, 1), "DelMRUString");
            check(enum_mru(mru, -1, NULL, 0) == 2, "two items left");
            enum_mru(mru, 1, buf, sizeof(buf));
            check(!lstrcmpW(buf, L"one"), "the middle one went");
            check(!del_mru(mru, 5), "a position out of range: FALSE");
            free_mru(mru);
            mru = create_mru(&mi);
            check(mru && enum_mru(mru, -1, NULL, 0) == 2, "and so in the registry");
            if (mru) free_mru(mru);
        }
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\StainedGlass\\MruProbe");
    }

    /* PSM_RECALCPAGESIZES */
    {
        PROPSHEETPAGEW page = { sizeof(page) };
        PROPSHEETHEADERW header = { sizeof(header) };
        HPROPSHEETPAGE hpage, big;
        page.dwFlags = PSP_DLGINDIRECT;
        page.pResource = make_template(100, 50);
        page.pfnDlgProc = page_proc;
        hpage = CreatePropertySheetPageW(&page);
        header.dwFlags = PSH_MODELESS;
        header.hwndParent = parent;
        header.nPages = 1;
        header.phpage = &hpage;
        sheet = (HWND)PropertySheetW(&header);
        GetWindowRect(sheet, &before);
        page.pResource = make_template(250, 180);
        big = CreatePropertySheetPageW(&page);
        SendMessageW(sheet, PSM_ADDPAGE, 0, (LPARAM)big);
        check(SendMessageW(sheet, PSM_RECALCPAGESIZES, 0, 0), "PSM_RECALCPAGESIZES");
        GetWindowRect(sheet, &after);
        printf("      %ldx%ld -> %ldx%ld\n", before.right - before.left, before.bottom - before.top,
               after.right - after.left, after.bottom - after.top);
        check(after.right - after.left > before.right - before.left &&
              after.bottom - after.top > before.bottom - before.top, "the sheet grows to the bigger page");
        DestroyWindow(sheet);
    }

    /* a tooltip's WM_NOTIFYFORMAT */
    tips = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL, 0, 0, 0, 0, 0, parent, NULL, NULL, NULL);
    ansi = CreateWindowW(L"ccmisc_ansi", L"", WS_CHILD, 0, 0, 10, 10, parent, NULL, NULL, NULL);
    {
        TTTOOLINFOW ti = { sizeof(ti) };
        ti.hwnd = ansi;
        ti.uId = 1;
        ti.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(tips, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    }
    check(SendMessageW(tips, WM_NOTIFYFORMAT, (WPARAM)parent, NF_QUERY) == NFR_UNICODE, "NF_QUERY: Unicode");
    check(SendMessageW(tips, WM_NOTIFYFORMAT, (WPARAM)ansi, NF_REQUERY) == NFR_ANSI, "NF_REQUERY: the window's ANSI");

    /* tabs */
    tab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE, 10, 200, 300, 100, parent, NULL, NULL, NULL);
    {
        TCITEMW item = { TCIF_TEXT | TCIF_RTLREADING };
        item.pszText = (WCHAR *)L"First";
        SendMessageW(tab, TCM_INSERTITEMW, 0, (LPARAM)&item);
        item.mask = TCIF_TEXT;
        item.pszText = (WCHAR *)L"Second";
        SendMessageW(tab, TCM_INSERTITEMW, 1, (LPARAM)&item);
        item.mask = TCIF_RTLREADING;
        SendMessageW(tab, TCM_SETITEMW, 0, (LPARAM)&item);
        item.mask = TCIF_RTLREADING;
        SendMessageW(tab, TCM_GETITEMW, 0, (LPARAM)&item);
        check(item.mask & TCIF_RTLREADING, "TCIF_RTLREADING kept on a tab");
        item.mask = TCIF_RTLREADING;
        SendMessageW(tab, TCM_GETITEMW, 1, (LPARAM)&item);
        check(!(item.mask & TCIF_RTLREADING), "and not on another");
    }
    SendMessageW(tab, TCM_SETEXTENDEDSTYLE, TCS_EX_REGISTERDROP, TCS_EX_REGISTERDROP);
    check(SendMessageW(tab, TCM_GETEXTENDEDSTYLE, 0, 0) & TCS_EX_REGISTERDROP, "TCS_EX_REGISTERDROP is kept");
    {
        IDropSource source = { &s_vtbl };
        IDataObject *data = NULL;
        DWORD effect;
        POINT pt;
        UpdateWindow(parent);
        SendMessageW(tab, TCM_GETITEMRECT, 1, (LPARAM)&rc);
        pt.x = (rc.left + rc.right) / 2;
        pt.y = (rc.top + rc.bottom) / 2;
        ClientToScreen(tab, &pt);
        SetCursorPos(pt.x, pt.y);
        OleGetClipboard(&data);
        if (data) DoDragDrop(data, &source, DROPEFFECT_COPY, &effect);
        printf("      getobject %ld (item %ld), enter %ld, drop %ld\n", getobject_calls, getobject_item, target.enter, target.drop);
        check(getobject_calls >= 1 && getobject_item == 1, "TCN_GETOBJECT for the tab under the drag");
        check(target.drop == 1, "the tab's drop target gets the drop");
        if (data) IDataObject_Release(data);
    }

    DestroyWindow(parent);
    OleUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
