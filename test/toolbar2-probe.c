/* Toolbar breadth (patches/sg/1671), run by test/toolbar2-gate.sh:
 * TB_SETCMDID moves the button's tooltip; TB_REPLACEBITMAP replaces a
 * standard bitmap; TB_SETBOUNDINGSIZE wraps the rows; combined custom
 * draw answers to the erase; TBSTYLE_REGISTERDROP asks for a button's
 * drop target (TBN_GETOBJECT) and hands it a drag.
 * These were FIXMEs. */
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

/* the button's drop target */
struct target { IDropTarget iface; LONG enter, drop; };
static HRESULT WINAPI t_qi(IDropTarget *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI t_addref(IDropTarget *iface) { return 2; }
static ULONG WINAPI t_release(IDropTarget *iface) { return 1; }
static HRESULT WINAPI t_enter(IDropTarget *iface, IDataObject *d, DWORD k, POINTL pt, DWORD *effect)
{
    ((struct target *)iface)->enter++;
    *effect = DROPEFFECT_COPY;
    return S_OK;
}
static HRESULT WINAPI t_over(IDropTarget *iface, DWORD k, POINTL pt, DWORD *effect) { *effect = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI t_leave(IDropTarget *iface) { return S_OK; }
static HRESULT WINAPI t_drop(IDropTarget *iface, IDataObject *d, DWORD k, POINTL pt, DWORD *effect)
{
    ((struct target *)iface)->drop++;
    *effect = DROPEFFECT_COPY;
    return S_OK;
}
static IDropTargetVtbl t_vtbl = { t_qi, t_addref, t_release, t_enter, t_over, t_leave, t_drop };
static struct target target = { { &t_vtbl } };
static LONG getobject_calls, getobject_id = -1;

/* a source that drops at once */
static HRESULT WINAPI s_qi(IDropSource *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI s_addref(IDropSource *iface) { return 2; }
static ULONG WINAPI s_release(IDropSource *iface) { return 1; }
static int continue_calls;
static HRESULT WINAPI s_continue(IDropSource *iface, BOOL esc, DWORD keys)
{
    return ++continue_calls > 2 ? DRAGDROP_S_DROP : S_OK;
}
static HRESULT WINAPI s_feedback(IDropSource *iface, DWORD effect) { return DRAGDROP_S_USEDEFAULTCURSORS; }
static IDropSourceVtbl s_vtbl = { s_qi, s_addref, s_release, s_continue, s_feedback };

static DWORD erase_answer;
static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY)
    {
        NMHDR *hdr = (NMHDR *)lp;
        if (hdr->code == TBN_GETOBJECT)
        {
            NMOBJECTNOTIFY *nmo = (NMOBJECTNOTIFY *)lp;
            getobject_calls++;
            getobject_id = nmo->iItem;
            if (IsEqualIID(nmo->piid, &IID_IDropTarget))
            {
                nmo->pObject = &target.iface;
                nmo->hResult = S_OK;
            }
            return 0;
        }
        if (hdr->code == NM_CUSTOMDRAW && ((NMCUSTOMDRAW *)lp)->dwDrawStage == CDDS_PREERASE)
            return erase_answer;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND make_toolbar(HWND parent, DWORD style, int buttons)
{
    HWND tb = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL, WS_CHILD | WS_VISIBLE | style, 0, 0, 600, 30, parent,
                              (HMENU)1, NULL, NULL);
    TBBUTTON b = { 0, 100, TBSTATE_ENABLED, BTNS_BUTTON };
    int i;

    SendMessageW(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    for (i = 0; i < buttons; i++)
    {
        b.idCommand = 100 + i;
        b.iBitmap = i;
        SendMessageW(tb, TB_ADDBUTTONSW, 1, (LPARAM)&b);
    }
    SendMessageW(tb, TB_AUTOSIZE, 0, 0);
    return tb;
}

int main(void)
{
    WNDCLASSW wc = {0};
    HWND parent, tb, tips;
    TTTOOLINFOW ti = { sizeof(ti) };
    TBADDBITMAP add = { HINST_COMMCTRL, IDB_STD_SMALL_COLOR };
    TBREPLACEBITMAP rep = { HINST_COMMCTRL, IDB_STD_SMALL_COLOR, HINST_COMMCTRL, IDB_VIEW_SMALL_COLOR, 0 };
    SIZE bound = { 60, 200 };
    HIMAGELIST himl;
    RECT rc;
    POINT pt;
    DWORD effect;
    IDataObject *data = NULL;
    HDC dc;
    DWORD *bits;
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), 100, -40, 1, 32, BI_RGB }};
    int i;

    OleInitialize(NULL);
    InitCommonControls();
    wc.lpfnWndProc = parent_proc;
    wc.lpszClassName = L"tb2_parent";
    RegisterClassW(&wc);
    parent = CreateWindowW(L"tb2_parent", L"tb2", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 400, NULL, NULL, NULL, NULL);

    /* TB_SETCMDID and the tooltip */
    tb = make_toolbar(parent, TBSTYLE_TOOLTIPS, 1);
    tips = (HWND)SendMessageW(tb, TB_GETTOOLTIPS, 0, 0);
    ti.hwnd = tb;
    ti.uId = 100;
    check(tips && SendMessageW(tips, TTM_GETTOOLINFOW, 0, (LPARAM)&ti), "the button's tool");
    check(SendMessageW(tb, TB_SETCMDID, 0, 200), "TB_SETCMDID");
    ti.uId = 200;
    check(SendMessageW(tips, TTM_GETTOOLINFOW, 0, (LPARAM)&ti), "the tool is the new command's");
    ti.uId = 100;
    check(!SendMessageW(tips, TTM_GETTOOLINFOW, 0, (LPARAM)&ti), "and no longer the old one's");

    /* TB_REPLACEBITMAP of a standard bitmap */
    SendMessageW(tb, TB_ADDBITMAP, 0, (LPARAM)&add);
    himl = (HIMAGELIST)SendMessageW(tb, TB_GETIMAGELIST, 0, 0);
    check(himl && ImageList_GetImageCount(himl) == 15, "the standard bitmap: 15 images");
    check(SendMessageW(tb, TB_REPLACEBITMAP, 0, (LPARAM)&rep), "TB_REPLACEBITMAP of a standard bitmap");
    himl = (HIMAGELIST)SendMessageW(tb, TB_GETIMAGELIST, 0, 0);
    check(himl && ImageList_GetImageCount(himl) == 12, "now the view bitmap's 12");
    DestroyWindow(tb);

    /* TB_SETBOUNDINGSIZE */
    tb = make_toolbar(parent, TBSTYLE_WRAPABLE | CCS_NORESIZE, 6);
    SendMessageW(tb, TB_AUTOSIZE, 0, 0);
    check(SendMessageW(tb, TB_GETROWS, 0, 0) == 1, "one row in the wide window");
    SendMessageW(tb, TB_SETBOUNDINGSIZE, 0, (LPARAM)&bound);
    printf("      %Iu rows\n", SendMessageW(tb, TB_GETROWS, 0, 0));
    check(SendMessageW(tb, TB_GETROWS, 0, 0) > 1, "TB_SETBOUNDINGSIZE: wrapped to the bounding width");
    DestroyWindow(tb);

    /* combined custom draw answers to the erase */
    tb = make_toolbar(parent, TBSTYLE_CUSTOMERASE, 1);
    dc = CreateCompatibleDC(0);
    SelectObject(dc, CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0));
    for (i = 0; i < 100 * 40; i++) bits[i] = 0x123456;
    erase_answer = CDRF_SKIPDEFAULT | CDRF_NOTIFYPOSTERASE;
    check(SendMessageW(tb, WM_ERASEBKGND, (WPARAM)dc, 0) && (bits[10 * 100 + 50] & 0xffffff) == 0x123456,
          "CDRF_SKIPDEFAULT with other flags skips the erase");
    erase_answer = 0;
    DeleteDC(dc);
    DestroyWindow(tb);

    /* TBSTYLE_REGISTERDROP */
    tb = make_toolbar(parent, TBSTYLE_REGISTERDROP, 2);
    UpdateWindow(parent);
    SendMessageW(tb, TB_GETITEMRECT, 1, (LPARAM)&rc);
    pt.x = (rc.left + rc.right) / 2;
    pt.y = (rc.top + rc.bottom) / 2;
    ClientToScreen(tb, &pt);
    SetCursorPos(pt.x, pt.y);
    /* any data object will do: the clipboard's */
    OleGetClipboard(&data);
    {
        IDropSource source = { &s_vtbl };
        HRESULT hr;

        hr = data ? DoDragDrop(data, &source, DROPEFFECT_COPY, &effect) : E_FAIL;
        printf("      DoDragDrop %08lx, enter %ld, drop %ld, getobject %ld (id %ld)\n", hr, target.enter, target.drop,
               getobject_calls, getobject_id);
        check(getobject_calls >= 1 && getobject_id == 101, "TBN_GETOBJECT for the button under the drag");
        check(target.enter >= 1 && target.drop == 1, "the button's drop target gets the drag and the drop");
    }
    if (data) IDataObject_Release(data);
    DestroyWindow(parent);
    OleUninitialize();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
