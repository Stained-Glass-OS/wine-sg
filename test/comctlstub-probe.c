/* comctl32 stub batch (patches/sg/2007), run by test/comctlstub-gate.sh.
 * Families: CCM_SETWINDOWTHEME (TB_/TTM_/RB_/CBEM_SETWINDOWTHEME) on the
 * common controls, WM_PRINTCLIENT on the tree view and month calendar, RB_SETPALETTE / RB_GETPALETTE, LVSIL_GROUPHEADER.
 *
 *   comctlstub-probe.exe */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
/* ---- the parent: rebar custom draw sees the palette in the DC -------- */

static HPALETTE seen_palette;      /* what the DC had selected when the rebar drew */
static int seen_draws;
static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY)
    {
        NMCUSTOMDRAW *nm = (NMCUSTOMDRAW *)lp;
        if (nm->hdr.code == NM_CUSTOMDRAW)
        {
            if (nm->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (nm->dwDrawStage == CDDS_ITEMPREPAINT)
            {
                HPALETTE cur = SelectPalette(nm->hdc, GetStockObject(DEFAULT_PALETTE), TRUE);
                SelectPalette(nm->hdc, cur, TRUE);
                seen_palette = cur;
                seen_draws++;
                return CDRF_DODEFAULT;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LONG themechanged;
static LRESULT CALLBACK count_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR ref)
{
    if (msg == WM_THEMECHANGED) InterlockedIncrement(&themechanged);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* the sub-app name SetWindowTheme gave the window, "" if none */
static const WCHAR *subapp(HWND hwnd, WCHAR *buf, int len)
{
    ATOM prop = GlobalFindAtomW(L"ux_subapp"), val;
    buf[0] = 0;
    if (!prop) return buf;
    val = (ATOM)(ULONG_PTR)GetPropW(hwnd, (LPCWSTR)(ULONG_PTR)prop);
    if (val) GetAtomNameW(val, buf, len);
    return buf;
}

static HWND make(const WCHAR *cls, DWORD style, HWND parent, int w, int h)
{
    return CreateWindowExW(0, cls, L"", style | (parent ? WS_CHILD : WS_POPUP), 0, 0, w, h,
                           parent, NULL, GetModuleHandleW(NULL), NULL);
}

/* ---- CCM_SETWINDOWTHEME ---------------------------------------------- */

static void test_setwindowtheme(HWND parent)
{
    static const struct { const WCHAR *cls; DWORD style; const char *name; } tests[] = {
        { L"SysListView32", LVS_REPORT, "listview" },
        { L"SysTreeView32", 0, "treeview" },
        { L"SysTabControl32", 0, "tab" },
        { L"SysHeader32", 0, "header" },
        { L"msctls_trackbar32", 0, "trackbar" },
        { L"msctls_progress32", 0, "progress" },
        { L"msctls_updown32", 0, "updown" },
        { L"msctls_statusbar32", 0, "statusbar" },
        { L"SysMonthCal32", 0, "monthcal" },
        { L"SysDateTimePick32", 0, "datetime" },
        { L"SysIPAddress32", 0, "ipaddress" },
        { L"SysPager", 0, "pager" },
        { L"ToolbarWindow32", 0, "toolbar" },
        { L"tooltips_class32", 0, "tooltips" },
        { L"ReBarWindow32", 0, "rebar" },
        { L"ComboBoxEx32", CBS_DROPDOWN, "comboex" },
    };
    unsigned int i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        WCHAR buf[64];
        char what[100];
        LRESULT ret;
        LONG before;
        HWND hwnd = make(tests[i].cls, tests[i].style, !wcscmp(tests[i].cls, L"tooltips_class32") ? NULL : parent, 100, 40);

        snprintf(what, sizeof(what), "%s created", tests[i].name);
        check(hwnd != NULL, what);
        if (!hwnd) continue;
        SetWindowSubclass(hwnd, count_proc, 1, 0);

        before = themechanged;
        ret = SendMessageW(hwnd, CCM_SETWINDOWTHEME, 0, (LPARAM)L"Explorer");
        snprintf(what, sizeof(what), "%s: CCM_SETWINDOWTHEME returns S_OK", tests[i].name);
        check(ret == S_OK, what);
        snprintf(what, sizeof(what), "%s: WM_THEMECHANGED followed", tests[i].name);
        check(themechanged > before, what);
        snprintf(what, sizeof(what), "%s: sub-app name is Explorer", tests[i].name);
        check(!wcscmp(subapp(hwnd, buf, 64), L"Explorer"), what);

        before = themechanged;
        ret = SendMessageW(hwnd, CCM_SETWINDOWTHEME, 0, (LPARAM)L"DarkMode_Explorer");
        snprintf(what, sizeof(what), "%s: a second name replaces the first", tests[i].name);
        check(ret == S_OK && themechanged > before && !wcscmp(subapp(hwnd, buf, 64), L"DarkMode_Explorer"), what);

        SendMessageW(hwnd, CCM_SETWINDOWTHEME, 0, 0);
        snprintf(what, sizeof(what), "%s: NULL clears the name", tests[i].name);
        check(subapp(hwnd, buf, 64)[0] == 0, what);

        if (!wcscmp(tests[i].cls, L"ComboBoxEx32"))
        {
            HWND combo = (HWND)SendMessageW(hwnd, CBEM_GETCOMBOCONTROL, 0, 0);
            HWND edit = (HWND)SendMessageW(hwnd, CBEM_GETEDITCONTROL, 0, 0);
            check(combo && edit, "comboex has a combo and an edit");
            SendMessageW(hwnd, CBEM_SETWINDOWTHEME, 0, (LPARAM)L"Explorer");
            check(!wcscmp(subapp(combo, buf, 64), L"Explorer"), "comboex: the combo box got the name");
            check(!wcscmp(subapp(edit, buf, 64), L"Explorer"), "comboex: the edit got the name");
            check(!wcscmp(subapp(hwnd, buf, 64), L"Explorer"), "comboex: the control itself got the name");
        }
        DestroyWindow(hwnd);
    }
}

/* ---- WM_PRINTCLIENT options ------------------------------------------ */

#define PW 160
#define PH 120
#define MAGENTA RGB(255, 0, 255)
#define BKRED RGB(250, 20, 20)

static HDC memdc;
static HBITMAP membmp;
static DWORD *bits;

static void setup_dc(void)
{
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), PW, -PH, 1, 32, BI_RGB }};
    HDC screen = GetDC(NULL);
    memdc = CreateCompatibleDC(screen);
    membmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(memdc, membmp);
    ReleaseDC(NULL, screen);
}

/* print with OPTIONS onto a magenta bitmap; count non-magenta and bk pixels */
static void snap(HWND hwnd, DWORD options, int *nonmagenta, int *red, int *corner_magenta)
{
    int i;
    RECT rc = { 0, 0, PW, PH };
    HBRUSH br = CreateSolidBrush(MAGENTA);
    FillRect(memdc, &rc, br);
    DeleteObject(br);
    GdiFlush();
    SendMessageW(hwnd, WM_PRINTCLIENT, (WPARAM)memdc, options);
    GdiFlush();
    *nonmagenta = *red = 0;
    for (i = 0; i < PW * PH; i++)
    {
        DWORD p = bits[i] & 0xffffff;
        if (p != 0xff00ff) (*nonmagenta)++;
        if (p == (((BKRED & 0xff) << 16) | (BKRED & 0xff00) | ((BKRED >> 16) & 0xff))) (*red)++;
    }
    *corner_magenta = (bits[(PH - 1) * PW + PW - 1] & 0xffffff) == 0xff00ff;
}

static void test_printclient(HWND parent)
{
    static const DWORD params[] = { 0, PRF_CHECKVISIBLE, PRF_NONCLIENT, PRF_CLIENT, PRF_ERASEBKGND, PRF_CHILDREN, PRF_OWNED };
    HWND hwnd;
    TVINSERTSTRUCTW tvi;
    int nm, red, corner, i, pass;
    char what[160];

    /* tree view: the options are not looked at, a hidden one paints as well
     * (the same as Wine's conformance test, which Windows passes) */
    hwnd = make(L"SysTreeView32", WS_VISIBLE, parent, PW, PH);
    SendMessageW(hwnd, TVM_SETBKCOLOR, 0, BKRED);
    memset(&tvi, 0, sizeof(tvi));
    tvi.hParent = TVI_ROOT;
    tvi.hInsertAfter = TVI_LAST;
    tvi.item.mask = TVIF_TEXT;
    for (i = 0; i < 4; i++)
    {
        tvi.item.pszText = (WCHAR *)L"Tree node text";
        SendMessageW(hwnd, TVM_INSERTITEMW, 0, (LPARAM)&tvi);
    }
    for (pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < (int)(sizeof(params) / sizeof(params[0])); i++)
        {
            snap(hwnd, params[i], &nm, &red, &corner);
            snprintf(what, sizeof(what), "treeview %s: lParam 0x%lx erases (red %d) and draws the items (%d)",
                     pass ? "hidden" : "visible", (long)params[i], red, nm);
            check(red > PW * PH * 9 / 10 && nm > red, what);
        }
        ShowWindow(hwnd, SW_HIDE);
    }
    DestroyWindow(hwnd);

    /* month calendar: its own options (this is Wine's behaviour, kept) */
    hwnd = make(L"SysMonthCal32", WS_VISIBLE, parent, PW, PH);
    SendMessageW(hwnd, MCM_SETCOLOR, MCSC_BACKGROUND, BKRED);
    snap(hwnd, 0, &nm, &red, &corner);
    snprintf(what, sizeof(what), "monthcal: no options paints nothing (%d)", nm);
    check(nm == 0, what);
    snap(hwnd, PRF_ERASEBKGND, &nm, &red, &corner);
    check(red == PW * PH, "monthcal: PRF_ERASEBKGND alone fills the background");
    snap(hwnd, PRF_CLIENT, &nm, &red, &corner);
    check(nm > 0, "monthcal: PRF_CLIENT alone draws the calendar");
    ShowWindow(hwnd, SW_HIDE);
    snap(hwnd, PRF_CHECKVISIBLE | PRF_ERASEBKGND | PRF_CLIENT, &nm, &red, &corner);
    check(nm == 0, "monthcal: PRF_CHECKVISIBLE on a hidden window paints nothing");
    DestroyWindow(hwnd);
}

/* ---- rebar palette --------------------------------------------------- */

static HPALETTE make_palette(int shade)
{
    LOGPALETTE *lp = calloc(1, sizeof(LOGPALETTE) + 3 * sizeof(PALETTEENTRY));
    HPALETTE pal;
    int i;
    lp->palVersion = 0x300;
    lp->palNumEntries = 4;
    for (i = 0; i < 4; i++)
    {
        lp->palPalEntry[i].peRed = shade;
        lp->palPalEntry[i].peGreen = i * 40;
        lp->palPalEntry[i].peBlue = 7;
    }
    pal = CreatePalette(lp);
    free(lp);
    return pal;
}

static void test_rebar_palette(HWND parent)
{
    HWND rebar = make(L"ReBarWindow32", WS_VISIBLE, parent, PW, 40);
    HPALETTE p1 = make_palette(10), p2 = make_palette(200);
    REBARBANDINFOW band = { sizeof(band), RBBIM_TEXT | RBBIM_SIZE };
    LRESULT old;

    band.lpText = (WCHAR *)L"Band";
    band.cx = 100;
    SendMessageW(rebar, RB_INSERTBANDW, -1, (LPARAM)&band);

    check(SendMessageW(rebar, RB_GETPALETTE, 0, 0) == 0, "rebar: no palette to begin with");
    old = SendMessageW(rebar, RB_SETPALETTE, 0, (LPARAM)p1);
    check(old == 0, "rebar: RB_SETPALETTE returns the previous (none)");
    check(SendMessageW(rebar, RB_GETPALETTE, 0, 0) == (LRESULT)p1, "rebar: RB_GETPALETTE returns what was set");
    old = SendMessageW(rebar, RB_SETPALETTE, 0, (LPARAM)p2);
    check(old == (LRESULT)p1, "rebar: the second RB_SETPALETTE returns the first palette");
    check(SendMessageW(rebar, RB_GETPALETTE, 0, 0) == (LRESULT)p2, "rebar: RB_GETPALETTE returns the second");

    /* painting: the palette is selected in the DC the bands are drawn into */
    seen_palette = NULL;
    seen_draws = 0;
    SendMessageW(rebar, WM_PRINTCLIENT, (WPARAM)memdc, PRF_CLIENT | PRF_ERASEBKGND);
    check(seen_draws > 0, "rebar: the band was drawn (custom draw seen)");
    check(seen_palette == p2, "rebar: the palette is selected while it paints");

    old = SendMessageW(rebar, RB_SETPALETTE, 0, 0);
    check(old == (LRESULT)p2, "rebar: RB_SETPALETTE(NULL) returns the second palette");
    check(SendMessageW(rebar, RB_GETPALETTE, 0, 0) == 0, "rebar: RB_GETPALETTE after clearing is none");
    seen_palette = (HPALETTE)1;
    SendMessageW(rebar, WM_PRINTCLIENT, (WPARAM)memdc, PRF_CLIENT | PRF_ERASEBKGND);
    check(seen_palette != p2 && seen_palette != p1, "rebar: with no palette the DC keeps its own");

    /* the palette is the program's: the rebar does not delete it */
    DestroyWindow(rebar);
    check(GetObjectType(p2) == OBJ_PAL, "rebar: the program's palette outlives the control");
    DeleteObject(p1);
    DeleteObject(p2);
}

/* ---- list view LVSIL_GROUPHEADER --------------------------------------- */

static HIMAGELIST make_iml(void)
{
    HIMAGELIST iml = ImageList_Create(16, 16, ILC_COLOR32, 1, 1);
    HBITMAP bm = CreateBitmap(16, 16, 1, 32, NULL);
    ImageList_Add(iml, bm, NULL);
    DeleteObject(bm);
    return iml;
}

static void test_group_imagelist(HWND parent)
{
    HIMAGELIST a = make_iml(), b = make_iml(), shared = make_iml();
    HWND lv = make(L"SysListView32", LVS_REPORT, parent, 100, 100);
    HWND lv2;

    check(ImageList_GetImageCount(a) == 1, "imagelist helper makes a one-image list");
    check(SendMessageW(lv, LVM_GETIMAGELIST, LVSIL_GROUPHEADER, 0) == 0, "listview: no group header image list at first");
    check(SendMessageW(lv, LVM_SETIMAGELIST, LVSIL_GROUPHEADER, (LPARAM)a) == 0,
          "listview: setting LVSIL_GROUPHEADER returns the previous (none)");
    check(SendMessageW(lv, LVM_GETIMAGELIST, LVSIL_GROUPHEADER, 0) == (LRESULT)a,
          "listview: LVM_GETIMAGELIST returns the group header list");
    check(SendMessageW(lv, LVM_GETIMAGELIST, LVSIL_SMALL, 0) == 0 && SendMessageW(lv, LVM_GETIMAGELIST, LVSIL_NORMAL, 0) == 0
          && SendMessageW(lv, LVM_GETIMAGELIST, LVSIL_STATE, 0) == 0, "listview: the other lists are untouched");
    check(SendMessageW(lv, LVM_SETIMAGELIST, LVSIL_GROUPHEADER, (LPARAM)b) == (LRESULT)a,
          "listview: replacing returns the first list");
    check(ImageList_GetImageCount(a) == 1, "listview: a replaced list is the program's to destroy");
    ImageList_Destroy(a);
    DestroyWindow(lv);
    check(ImageList_GetImageCount(b) == 0, "listview: the list it owns is destroyed with it");

    lv2 = make(L"SysListView32", LVS_REPORT | LVS_SHAREIMAGELISTS, parent, 100, 100);
    SendMessageW(lv2, LVM_SETIMAGELIST, LVSIL_GROUPHEADER, (LPARAM)shared);
    DestroyWindow(lv2);
    check(ImageList_GetImageCount(shared) == 1, "listview: LVS_SHAREIMAGELISTS keeps the list alive");
    ImageList_Destroy(shared);
}

int main(void)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), 0xffff };
    WNDCLASSW wc = { 0 };
    HWND parent;

    InitCommonControlsEx(&icc);
    wc.lpfnWndProc = parent_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"sgstubparent";
    RegisterClassW(&wc);
    parent = CreateWindowExW(0, L"sgstubparent", L"p", WS_POPUP | WS_VISIBLE, 0, 0, 300, 300, NULL, NULL, wc.hInstance, NULL);
    check(parent != NULL, "parent window created");
    setup_dc();

    test_setwindowtheme(parent);
    test_printclient(parent);
    test_rebar_palette(parent);
    test_group_imagelist(parent);

    DestroyWindow(parent);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
