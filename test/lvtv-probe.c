/* List view and tree view breadth (patches/sg/1670), run by
 * test/lvtv-gate.sh:
 *  - list view: work areas (set, get, count; icons arranged in the first);
 *    snap to grid; the insertion mark (set, get, rect, hit test, colour);
 *    the outline colour; the incremental search string; the background
 *    image (tiled, from a file, read back); info tips (LVM_SETINFOTIP and
 *    LVN_GETINFOTIP on hover); the small icon view's approximate rect;
 *  - tree view: TVSI_NOSINGLEEXPAND; extended item states
 *    (TVIS_EX_DISABLED kept, and not selected by a click);
 *    TVE_EXPANDPARTIAL; the incremental search string.
 * These were FIXMEs or missing. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static WCHAR infotip_seen[256];
static LONG infotip_calls;

static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY && ((NMHDR *)lp)->code == LVN_GETINFOTIPW)
    {
        NMLVGETINFOTIPW *tip = (NMLVGETINFOTIPW *)lp;
        lstrcpynW(infotip_seen, tip->pszText, 256);
        infotip_calls++;
        lstrcpynW(tip->pszText, L"Tip from the parent", tip->cchTextMax);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

static void add_item(HWND lv, int i, const WCHAR *text)
{
    LVITEMW item = { LVIF_TEXT };
    item.iItem = i;
    item.pszText = (WCHAR *)text;
    SendMessageW(lv, LVM_INSERTITEMW, 0, (LPARAM)&item);
}

static HTREEITEM add_tree(HWND tv, HTREEITEM parent, const WCHAR *text)
{
    TVINSERTSTRUCTW ins = { parent, TVI_LAST };
    ins.item.mask = TVIF_TEXT;
    ins.item.pszText = (WCHAR *)text;
    return (HTREEITEM)SendMessageW(tv, TVM_INSERTITEMW, 0, (LPARAM)&ins);
}

int main(void)
{
    WNDCLASSW wc = {0};
    HWND parent, lv, tv, tips;
    RECT area = { 100, 50, 300, 400 }, areas[2], rc;
    POINT pt, off;
    LVINSERTMARK mark = { sizeof(mark) };
    LVBKIMAGEW bk = {0};
    LVSETINFOTIP tip = { sizeof(tip) };
    WCHAR buf[MAX_PATH], path[MAX_PATH];
    UINT n;
    HTREEITEM a, a1, b, b1;
    TVITEMEXW item;
    HBITMAP red;
    HDC dc;
    DWORD *bits;
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), 200, -200, 1, 32, BI_RGB }};
    int i;

    InitCommonControls();
    wc.lpfnWndProc = parent_proc;
    wc.lpszClassName = L"lvtv_parent";
    RegisterClassW(&wc);
    parent = CreateWindowW(L"lvtv_parent", L"lvtv", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 600, 500, NULL, NULL, NULL, NULL);
    lv = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_ICON, 0, 0, 500, 450, parent, NULL, NULL, NULL);
    add_item(lv, 0, L"alpha");
    add_item(lv, 1, L"beta");
    add_item(lv, 2, L"apple");

    /* work areas */
    check(SendMessageW(lv, LVM_SETWORKAREAS, 1, (LPARAM)&area), "LVM_SETWORKAREAS");
    n = 0;
    SendMessageW(lv, LVM_GETNUMBEROFWORKAREAS, 0, (LPARAM)&n);
    check(n == 1, "one work area");
    memset(areas, 0, sizeof(areas));
    check(SendMessageW(lv, LVM_GETWORKAREAS, 2, (LPARAM)areas) && EqualRect(&areas[0], &area), "LVM_GETWORKAREAS");
    SendMessageW(lv, LVM_ARRANGE, LVA_ALIGNTOP, 0);
    SendMessageW(lv, LVM_GETITEMPOSITION, 0, (LPARAM)&pt);
    check(pt.x >= 100 && pt.y >= 50, "icons are arranged in the work area");
    printf("      item 0 at %ld,%ld\n", pt.x, pt.y);
    off = pt;
    SendMessageW(lv, LVM_SETITEMPOSITION, 1, MAKELPARAM(423, 377));
    SendMessageW(lv, LVM_ARRANGE, LVA_SNAPTOGRID, 0);
    SendMessageW(lv, LVM_GETITEMPOSITION, 1, (LPARAM)&pt);
    {
        DWORD spacing = SendMessageW(lv, LVM_GETITEMSPACING, FALSE, 0);
        printf("      snapped to %ld,%ld (spacing %u,%u)\n", pt.x, pt.y, LOWORD(spacing), HIWORD(spacing));
        check((pt.x - off.x) % LOWORD(spacing) == 0 && (pt.y - off.y) % HIWORD(spacing) == 0 && pt.x > 300,
              "LVA_SNAPTOGRID: on the grid, near where it was");
    }
    SendMessageW(lv, LVM_SETWORKAREAS, 0, 0);
    SendMessageW(lv, LVM_ARRANGE, LVA_ALIGNTOP, 0);

    /* the insertion mark */
    mark.iItem = 1;
    mark.dwFlags = LVIM_AFTER;
    check(SendMessageW(lv, LVM_SETINSERTMARK, 0, (LPARAM)&mark), "LVM_SETINSERTMARK");
    memset(&mark, 0, sizeof(mark));
    mark.cbSize = sizeof(mark);
    check(SendMessageW(lv, LVM_GETINSERTMARK, 0, (LPARAM)&mark) && mark.iItem == 1 && mark.dwFlags == LVIM_AFTER,
          "LVM_GETINSERTMARK");
    SetRectEmpty(&rc);
    check(SendMessageW(lv, LVM_GETINSERTMARKRECT, 0, (LPARAM)&rc) && !IsRectEmpty(&rc), "LVM_GETINSERTMARKRECT");
    SendMessageW(lv, LVM_GETITEMPOSITION, 0, (LPARAM)&pt);
    pt.x += 2;
    pt.y += 2;
    mark.iItem = -2;
    SendMessageW(lv, LVM_INSERTMARKHITTEST, (WPARAM)&pt, (LPARAM)&mark);
    check(mark.iItem == 0 && mark.dwFlags == 0, "LVM_INSERTMARKHITTEST: before item 0");
    check(SendMessageW(lv, LVM_SETINSERTMARKCOLOR, 0, RGB(255, 0, 0)) == GetSysColor(COLOR_WINDOWTEXT) &&
          SendMessageW(lv, LVM_GETINSERTMARKCOLOR, 0, 0) == RGB(255, 0, 0), "the insertion mark's colour");
    SendMessageW(lv, LVM_SETOUTLINECOLOR, 0, RGB(1, 2, 3));
    check(SendMessageW(lv, LVM_GETOUTLINECOLOR, 0, 0) == RGB(1, 2, 3), "the outline colour");

    /* the incremental search string */
    SetFocus(lv);
    SendMessageW(lv, WM_CHAR, 'a', 1);
    buf[0] = 0;
    check(SendMessageW(lv, LVM_GETISEARCHSTRINGW, 0, (LPARAM)buf) == 1 && !lstrcmpW(buf, L"a"),
          "LVM_GETISEARCHSTRING while searching");
    Sleep(600);
    check(SendMessageW(lv, LVM_GETISEARCHSTRINGW, 0, (LPARAM)buf) == 0, "and none after a pause");

    /* the background image */
    red = CreateBitmap(4, 4, 1, 32, NULL);
    {
        HDC mdc = CreateCompatibleDC(0);
        HBRUSH brush = CreateSolidBrush(RGB(255, 0, 0));
        RECT r = { 0, 0, 4, 4 };
        SelectObject(mdc, red);
        FillRect(mdc, &r, brush);
        DeleteObject(brush);
        DeleteDC(mdc);
    }
    bk.ulFlags = LVBKIF_SOURCE_HBITMAP | LVBKIF_STYLE_TILE;
    bk.hbm = red;
    check(SendMessageW(lv, LVM_SETBKIMAGEW, 0, (LPARAM)&bk), "a tiled background bitmap");
    memset(&bk, 0, sizeof(bk));
    check(SendMessageW(lv, LVM_GETBKIMAGEW, 0, (LPARAM)&bk) && (bk.ulFlags & LVBKIF_STYLE_TILE) && bk.hbm == red,
          "LVM_GETBKIMAGE");
    dc = CreateCompatibleDC(0);
    SelectObject(dc, CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0));
    for (i = 0; i < 200 * 200; i++) bits[i] = 0x123456;
    SendMessageW(lv, WM_ERASEBKGND, (WPARAM)dc, 0);
    check((bits[150 * 200 + 150] & 0xffffff) == 0xff0000, "tiled across the whole background");
    DeleteDC(dc);
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"lvtv-bk.bmp");
    {
        /* a small bitmap file */
        BYTE file[14 + 40 + 16];
        BITMAPFILEHEADER *fh = (BITMAPFILEHEADER *)file;
        BITMAPINFOHEADER *ih = (BITMAPINFOHEADER *)(file + 14);
        HANDLE h;
        DWORD written;
        memset(file, 0, sizeof(file));
        fh->bfType = 0x4d42;
        fh->bfSize = sizeof(file);
        fh->bfOffBits = 54;
        ih->biSize = 40; ih->biWidth = 2; ih->biHeight = 2; ih->biPlanes = 1; ih->biBitCount = 32;
        memset(file + 54, 0x80, 16);
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(h, file, sizeof(file), &written, NULL);
        CloseHandle(h);
    }
    memset(&bk, 0, sizeof(bk));
    bk.ulFlags = LVBKIF_SOURCE_URL;
    bk.pszImage = path;
    check(SendMessageW(lv, LVM_SETBKIMAGEW, 0, (LPARAM)&bk), "a background image from a file");
    memset(&bk, 0, sizeof(bk));
    bk.pszImage = buf;
    bk.cchImageMax = MAX_PATH;
    check(SendMessageW(lv, LVM_GETBKIMAGEW, 0, (LPARAM)&bk) && (bk.ulFlags & LVBKIF_SOURCE_URL) && !lstrcmpiW(buf, path),
          "LVM_GETBKIMAGE: its path");
    DeleteFileW(path);
    bk.ulFlags = LVBKIF_SOURCE_NONE;
    SendMessageW(lv, LVM_SETBKIMAGEW, 0, (LPARAM)&bk);

    /* info tips */
    SendMessageW(lv, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_INFOTIP, LVS_EX_INFOTIP);
    tip.pszText = (WCHAR *)L"Set by LVM_SETINFOTIP";
    tip.iItem = 0;
    check(SendMessageW(lv, LVM_SETINFOTIP, 0, (LPARAM)&tip), "LVM_SETINFOTIP");
    SendMessageW(lv, LVM_GETITEMPOSITION, 0, (LPARAM)&pt);
    {
        RECT ir = { LVIR_LABEL };
        SendMessageW(lv, LVM_GETITEMRECT, 0, (LPARAM)&ir);
        printf("      item 0 label %ld,%ld-%ld,%ld\n", ir.left, ir.top, ir.right, ir.bottom);
        pt.x = (ir.left + ir.right) / 2;
        pt.y = ir.top + 2;
    }
    SendMessageW(lv, WM_MOUSEHOVER, 0, MAKELPARAM(pt.x, pt.y));
    tips = (HWND)SendMessageW(lv, LVM_GETTOOLTIPS, 0, 0);
    check(infotip_calls == 1 && !lstrcmpW(infotip_seen, L"Set by LVM_SETINFOTIP"),
          "LVN_GETINFOTIP on hovering, with the tip set");
    check(tips && IsWindowVisible(tips), "the tip shows");
    SendMessageW(lv, WM_MOUSELEAVE, 0, 0);
    check(!tips || !IsWindowVisible(tips), "and goes on leaving");

    /* the small icon view */
    SendMessageW(lv, LVM_SETVIEW, LV_VIEW_SMALLICON, 0);
    check(SendMessageW(lv, LVM_APPROXIMATEVIEWRECT, -1, MAKELPARAM(-1, -1)) != 0,
          "LVM_APPROXIMATEVIEWRECT in the small icon view");
    DestroyWindow(lv);

    /* tree view */
    tv = CreateWindowW(WC_TREEVIEWW, L"", WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_SINGLEEXPAND | TVS_LINESATROOT,
                       0, 0, 300, 300, parent, NULL, NULL, NULL);
    a = add_tree(tv, TVI_ROOT, L"Animals");
    a1 = add_tree(tv, a, L"Ant");
    b = add_tree(tv, TVI_ROOT, L"Birds");
    b1 = add_tree(tv, b, L"Bat");
    SendMessageW(tv, TVM_SELECTITEM, TVGN_CARET | TVSI_NOSINGLEEXPAND, (LPARAM)a);
    check(!(SendMessageW(tv, TVM_GETITEMSTATE, (WPARAM)a, TVIS_EXPANDED) & TVIS_EXPANDED),
          "TVSI_NOSINGLEEXPAND: selected, not expanded");
    SendMessageW(tv, TVM_SELECTITEM, TVGN_CARET, (LPARAM)b);
    check(SendMessageW(tv, TVM_GETITEMSTATE, (WPARAM)b, TVIS_EXPANDED) & TVIS_EXPANDED,
          "without it the single-expand tree expands");

    SendMessageW(tv, TVM_EXPAND, TVE_EXPAND | TVE_EXPANDPARTIAL, (LPARAM)a);
    check((SendMessageW(tv, TVM_GETITEMSTATE, (WPARAM)a, 0xffff) & (TVIS_EXPANDED | TVIS_EXPANDPARTIAL)) ==
          (TVIS_EXPANDED | TVIS_EXPANDPARTIAL), "TVE_EXPANDPARTIAL");
    SendMessageW(tv, TVM_EXPAND, TVE_COLLAPSE, (LPARAM)a);
    check(!(SendMessageW(tv, TVM_GETITEMSTATE, (WPARAM)a, 0xffff) & (TVIS_EXPANDED | TVIS_EXPANDPARTIAL)),
          "collapsed: neither");
    SendMessageW(tv, TVM_EXPAND, TVE_EXPAND, (LPARAM)a);

    memset(&item, 0, sizeof(item));
    item.mask = TVIF_STATEEX;
    item.hItem = a1;
    item.uStateEx = TVIS_EX_DISABLED;
    SendMessageW(tv, TVM_SETITEMW, 0, (LPARAM)&item);
    item.uStateEx = 0;
    SendMessageW(tv, TVM_GETITEMW, 0, (LPARAM)&item);
    check(item.uStateEx == TVIS_EX_DISABLED, "TVIS_EX_DISABLED is kept");
    *(HTREEITEM *)&rc = a1;
    SendMessageW(tv, TVM_GETITEMRECT, TRUE, (LPARAM)&rc);
    SendMessageW(tv, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM((rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2));
    SendMessageW(tv, WM_LBUTTONUP, 0, MAKELPARAM((rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2));
    check((HTREEITEM)SendMessageW(tv, TVM_GETNEXTITEM, TVGN_CARET, 0) != a1, "a click does not select it");

    SetFocus(tv);
    SendMessageW(tv, WM_CHAR, 'b', 1);
    buf[0] = 0;
    check(SendMessageW(tv, TVM_GETISEARCHSTRINGW, 0, (LPARAM)buf) == 1 && !lstrcmpW(buf, L"b"),
          "TVM_GETISEARCHSTRING while searching");
    (void)b1;
    DestroyWindow(parent);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
