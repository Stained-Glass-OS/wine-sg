/* txdrawscroll-probe: a windowless RichEdit (ITextServices) whose host had no
 * client area yet when its text was set, then drawn with TxDraw into a wider
 * rectangle (patches/sg/7001) -- what Zoom's Join dialog does with its "Your
 * name" box. The caret at the end of the text scrolled the text out of a
 * zero-width view; drawn in a view where all of it fits, it must start at the
 * left edge again, as on Windows, not show only its last character.
 * Prints key=value lines; 64- and 32-bit (thiscall host, as Windows calls it).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <richedit.h>
#include <textserv.h>
#include <stdio.h>

static RECT client_rect;   /* what TxGetClientRect reports */
static HDC mem_dc;

class Host : public ITextHost
{
public:
    LONG ref = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **out) override
    { if (IsEqualIID(riid, IID_IUnknown)) { *out = this; AddRef(); return S_OK; } *out = NULL; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { return InterlockedDecrement(&ref); }
    HDC TxGetDC() override { return mem_dc; }
    INT TxReleaseDC(HDC) override { return 1; }
    WINBOOL TxShowScrollBar(INT, WINBOOL) override { return TRUE; }
    WINBOOL TxEnableScrollBar(INT, INT) override { return TRUE; }
    WINBOOL TxSetScrollRange(INT, LONG, INT, WINBOOL) override { return TRUE; }
    WINBOOL TxSetScrollPos(INT, INT, WINBOOL) override { return TRUE; }
    void TxInvalidateRect(LPCRECT, WINBOOL) override {}
    void TxViewChange(WINBOOL) override {}
    WINBOOL TxCreateCaret(HBITMAP, INT, INT) override { return TRUE; }
    WINBOOL TxShowCaret(WINBOOL) override { return TRUE; }
    WINBOOL TxSetCaretPos(INT, INT) override { return TRUE; }
    WINBOOL TxSetTimer(UINT, UINT) override { return TRUE; }
    void TxKillTimer(UINT) override {}
    void TxScrollWindowEx(INT, INT, LPCRECT, LPCRECT, HRGN, LPRECT, UINT) override {}
    void TxSetCapture(WINBOOL) override {}
    void TxSetFocus() override {}
    void TxSetCursor(HCURSOR, WINBOOL) override {}
    WINBOOL TxScreenToClient(LPPOINT) override { return TRUE; }
    WINBOOL TxClientToScreen(LPPOINT) override { return TRUE; }
    HRESULT TxActivate(LONG *old) override { if (old) *old = 0; return S_OK; }
    HRESULT TxDeactivate(LONG) override { return S_OK; }
    HRESULT TxGetClientRect(LPRECT prc) override { *prc = client_rect; return S_OK; }
    HRESULT TxGetViewInset(LPRECT prc) override { SetRectEmpty(prc); return S_OK; }
    HRESULT TxGetCharFormat(const CHARFORMATW **) override { return E_NOTIMPL; }
    HRESULT TxGetParaFormat(const PARAFORMAT **) override { return E_NOTIMPL; }
    COLORREF TxGetSysColor(int index) override { return index == COLOR_WINDOW ? RGB(255, 255, 255) : RGB(0, 0, 0); }
    HRESULT TxGetBackStyle(TXTBACKSTYLE *style) override { *style = TXTBACK_OPAQUE; return S_OK; }
    HRESULT TxGetMaxLength(DWORD *len) override { *len = INFINITE; return S_OK; }
    HRESULT TxGetScrollBars(DWORD *bars) override { *bars = ES_AUTOHSCROLL; return S_OK; }
    HRESULT TxGetPasswordChar(TCHAR *ch) override { *ch = 0; return S_FALSE; }
    HRESULT TxGetAcceleratorPos(LONG *cp) override { *cp = -1; return S_OK; }
    HRESULT TxGetExtent(LPSIZEL) override { return E_NOTIMPL; }
    HRESULT OnTxCharFormatChange(const CHARFORMATW *) override { return S_OK; }
    HRESULT OnTxParaFormatChange(const PARAFORMAT *) override { return S_OK; }
    HRESULT TxGetPropertyBits(DWORD mask, DWORD *bits) override { *bits = 0; return S_OK; }  /* single line, plain */
    HRESULT TxNotify(DWORD, void *) override { return S_OK; }
    HIMC TxImmGetContext() override { return NULL; }
    void TxImmReleaseContext(HIMC) override {}
    HRESULT TxGetSelectionBarWidth(LONG *w) override { *w = 0; return S_OK; }
};

/* the leftmost column of the drawn text (a non-white pixel), or -1 */
static int leftmost_ink(const DWORD *bits, int w, int h)
{
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            if ((bits[y * w + x] & 0xffffff) != 0xffffff) return x;
    return -1;
}

int main()
{
    HMODULE re = LoadLibraryA("riched20.dll");
    PCreateTextServices create = (PCreateTextServices)GetProcAddress(re, "CreateTextServices");
    const IID *iid = (const IID *)GetProcAddress(re, "IID_ITextServices");
    Host host;
    IUnknown *unk = NULL;
    ITextServices *ts = NULL;
    BITMAPINFO bi = {};
    void *bits;
    HBITMAP bmp;
    LRESULT res;
    LONG pos_before = -1, pos_after = -1, max_pos = 0, page = 0;
    const int W = 300, H = 30;
    RECTL bounds = { 0, 0, W, H };
    RECT update = { 0, 0, W, H };
    int left_fit, left_wide;

    if (!create || !iid) { printf("loaded=0\n"); return 1; }
    mem_dc = CreateCompatibleDC(NULL);
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bmp = CreateDIBSection(mem_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(mem_dc, bmp);

    if (FAILED(create(NULL, &host, &unk)) || FAILED(unk->QueryInterface(*iid, (void **)&ts)))
    { printf("created=0\n"); return 1; }
    printf("created=1\n");

    /* in place active with no client area yet; the text goes in with the caret after it */
    SetRectEmpty(&client_rect);
    ts->OnTxInPlaceActivate(NULL);
    ts->TxSendMessage(EM_REPLACESEL, FALSE, (LPARAM)L"davidxyz", &res);
    ts->TxGetHScroll(NULL, &max_pos, &pos_before, &page, NULL);
    printf("hscroll_before pos=%ld max=%ld page=%ld\n", pos_before, max_pos, page);

    /* drawn in a view wide enough for all of it */
    PatBlt(mem_dc, 0, 0, W, H, WHITENESS);
    HRESULT hr = ts->TxDraw(DVASPECT_CONTENT, 0, NULL, NULL, mem_dc, NULL, &bounds, NULL, &update, NULL, 0, TXTVIEW_ACTIVE);
    GdiFlush();
    left_wide = leftmost_ink((DWORD *)bits, W, H);
    ts->TxGetHScroll(NULL, &max_pos, &pos_after, &page, NULL);
    printf("txdraw hr=%#lx leftmost_ink=%d hscroll_after pos=%ld max=%ld page=%ld\n", hr, left_wide, pos_after, max_pos, page);
    printf("drawn=%d\n", hr == S_OK && left_wide >= 0);
    printf("starts_at_left=%d\n", left_wide >= 0 && left_wide < 12);
    printf("scroll_reset=%d\n", pos_after == 0);

    /* a second draw at the same size draws the same */
    PatBlt(mem_dc, 0, 0, W, H, WHITENESS);
    ts->TxDraw(DVASPECT_CONTENT, 0, NULL, NULL, mem_dc, NULL, &bounds, NULL, &update, NULL, 0, TXTVIEW_ACTIVE);
    GdiFlush();
    left_fit = leftmost_ink((DWORD *)bits, W, H);
    printf("same_again=%d\n", left_fit == left_wide);

    /* in a narrow view, text that does not fit keeps the caret's scroll position:
       the reset is only for text that fits */
    RECTL narrow = { 0, 0, 40, H };
    ts->TxDraw(DVASPECT_CONTENT, 0, NULL, NULL, mem_dc, NULL, &narrow, NULL, &update, NULL, 0, TXTVIEW_ACTIVE);
    ts->TxSendMessage(EM_SETSEL, -1, -1, &res);
    ts->TxSendMessage(EM_REPLACESEL, FALSE, (LPARAM)L" and a much longer name than fits", &res);
    ts->TxGetHScroll(NULL, &max_pos, &pos_before, &page, NULL);
    ts->TxDraw(DVASPECT_CONTENT, 0, NULL, NULL, mem_dc, NULL, &narrow, NULL, &update, NULL, 0, TXTVIEW_ACTIVE);
    ts->TxGetHScroll(NULL, &max_pos, &pos_after, &page, NULL);
    printf("narrow before=%ld after=%ld max=%ld page=%ld\n", pos_before, pos_after, max_pos, page);
    printf("narrow_still_scrolled=%d\n", pos_before > 0 && pos_after == pos_before);

    ts->Release();
    unk->Release();
    printf("done=1\n");
    return 0;
}
