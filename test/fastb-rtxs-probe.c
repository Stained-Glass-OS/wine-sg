/* riched20 ITextServices (patch 3695): OnTxUIActivate/Deactivate, TxGetCachedSize, TxGetBaseLinePos,
 * TxGetCurTargetX, on a windowless text host */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- a windowless text host (64-bit: thiscall is the plain convention) ---- */

struct host
{
    void **vtbl;
    LONG ref;
    RECT client;
};

#define H struct host *h
static HRESULT WINAPI host_QueryInterface(H, REFIID iid, void **out)
{
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI host_AddRef(H) { return InterlockedIncrement(&h->ref); }
static ULONG WINAPI host_Release(H) { return InterlockedDecrement(&h->ref); }
static HDC WINAPI host_TxGetDC(H) { return GetDC(NULL); }
static INT WINAPI host_TxReleaseDC(H, HDC dc) { return ReleaseDC(NULL, dc); }
static BOOL WINAPI host_TxShowScrollBar(H, INT bar, BOOL show) { return TRUE; }
static BOOL WINAPI host_TxEnableScrollBar(H, INT flags, INT arrows) { return TRUE; }
static BOOL WINAPI host_TxSetScrollRange(H, INT bar, LONG min, INT max, BOOL redraw) { return TRUE; }
static BOOL WINAPI host_TxSetScrollPos(H, INT bar, INT pos, BOOL redraw) { return TRUE; }
static void WINAPI host_TxInvalidateRect(H, const RECT *rect, BOOL mode) {}
static void WINAPI host_TxViewChange(H, BOOL update) {}
static BOOL WINAPI host_TxCreateCaret(H, HBITMAP bitmap, INT w, INT hgt) { return TRUE; }
static BOOL WINAPI host_TxShowCaret(H, BOOL show) { return TRUE; }
static BOOL WINAPI host_TxSetCaretPos(H, INT x, INT y) { return TRUE; }
static BOOL WINAPI host_TxSetTimer(H, UINT id, UINT timeout) { return TRUE; }
static void WINAPI host_TxKillTimer(H, UINT id) {}
static void WINAPI host_TxScrollWindowEx(H, INT dx, INT dy, const RECT *scroll, const RECT *clip, HRGN rgn,
                                         RECT *update, UINT flags) {}
static void WINAPI host_TxSetCapture(H, BOOL capture) {}
static void WINAPI host_TxSetFocus(H) {}
static void WINAPI host_TxSetCursor(H, HCURSOR cursor, BOOL text) {}
static BOOL WINAPI host_TxScreenToClient(H, POINT *pt) { return TRUE; }
static BOOL WINAPI host_TxClientToScreen(H, POINT *pt) { return TRUE; }
static HRESULT WINAPI host_TxActivate(H, LONG *old) { return S_OK; }
static HRESULT WINAPI host_TxDeactivate(H, LONG new) { return S_OK; }
static HRESULT WINAPI host_TxGetClientRect(H, RECT *rect) { *rect = h->client; return S_OK; }
static HRESULT WINAPI host_TxGetViewInset(H, RECT *rect) { SetRectEmpty(rect); return S_OK; }
/* a host that answers S_OK and gives no default formats, as Excel's cells do
 * (wine-sg 1513: Wine read the NULL and crashed as the editor was made) */
static int null_formats;
static HRESULT WINAPI host_TxGetCharFormat(H, const CHARFORMATW **fmt)
{
    static CHARFORMAT2W cf;
    if (null_formats) { *fmt = NULL; return S_OK; }
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD | CFM_CHARSET;
    cf.dwEffects = CFE_BOLD;
    cf.yHeight = 20 * 20;
    cf.crTextColor = RGB(0, 0, 0);
    cf.bCharSet = DEFAULT_CHARSET;
    lstrcpyW(cf.szFaceName, L"Tahoma");
    *fmt = (const CHARFORMATW *)&cf;
    return S_OK;
}
static HRESULT WINAPI host_TxGetParaFormat(H, const PARAFORMAT **fmt)
{
    static PARAFORMAT2 pf;
    if (null_formats) { *fmt = NULL; return S_OK; }
    memset(&pf, 0, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_ALIGNMENT;
    pf.wAlignment = PFA_LEFT;
    *fmt = (const PARAFORMAT *)&pf;
    return S_OK;
}
static COLORREF WINAPI host_TxGetSysColor(H, int index) { return GetSysColor(index); }
static HRESULT WINAPI host_TxGetBackStyle(H, int *style) { *style = 0 /* TXTBACK_TRANSPARENT */; return S_OK; }
static HRESULT WINAPI host_TxGetMaxLength(H, DWORD *len) { *len = INFINITE; return S_OK; }
static HRESULT WINAPI host_TxGetScrollBars(H, DWORD *bars) { *bars = 0; return S_OK; }
static HRESULT WINAPI host_TxGetPasswordChar(H, WCHAR *c) { return S_FALSE; }
static HRESULT WINAPI host_TxGetAcceleratorPos(H, LONG *pos) { *pos = -1; return S_OK; }
static HRESULT WINAPI host_TxGetExtent(H, SIZEL *extent) { return E_NOTIMPL; }
static HRESULT WINAPI host_OnTxCharFormatChange(H, const CHARFORMATW *fmt) { return S_OK; }
static HRESULT WINAPI host_OnTxParaFormatChange(H, const PARAFORMAT *fmt) { return S_OK; }
static HRESULT WINAPI host_TxGetPropertyBits(H, DWORD mask, DWORD *bits) { *bits = 0x000001 /* rich text */ & mask; return S_OK; }
static HRESULT WINAPI host_TxNotify(H, DWORD code, void *data) { return S_OK; }
static HIMC WINAPI host_TxImmGetContext(H) { return NULL; }
static void WINAPI host_TxImmReleaseContext(H, HIMC imc) {}
static HRESULT WINAPI host_TxGetSelectionBarWidth(H, LONG *width) { *width = 0; return S_OK; }
#undef H

static void *host_vtbl[] =
{
    host_QueryInterface, host_AddRef, host_Release,
    host_TxGetDC, host_TxReleaseDC, host_TxShowScrollBar, host_TxEnableScrollBar, host_TxSetScrollRange,
    host_TxSetScrollPos, host_TxInvalidateRect, host_TxViewChange, host_TxCreateCaret, host_TxShowCaret,
    host_TxSetCaretPos, host_TxSetTimer, host_TxKillTimer, host_TxScrollWindowEx, host_TxSetCapture,
    host_TxSetFocus, host_TxSetCursor, host_TxScreenToClient, host_TxClientToScreen, host_TxActivate,
    host_TxDeactivate, host_TxGetClientRect, host_TxGetViewInset, host_TxGetCharFormat, host_TxGetParaFormat,
    host_TxGetSysColor, host_TxGetBackStyle, host_TxGetMaxLength, host_TxGetScrollBars, host_TxGetPasswordChar,
    host_TxGetAcceleratorPos, host_TxGetExtent, host_OnTxCharFormatChange, host_OnTxParaFormatChange,
    host_TxGetPropertyBits, host_TxNotify, host_TxImmGetContext, host_TxImmReleaseContext,
    host_TxGetSelectionBarWidth,
};


struct services_vtbl
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *TxSendMessage)(void *, UINT, WPARAM, LPARAM, LRESULT *);
    void *TxDraw, *TxGetHScroll, *TxGetVScroll, *OnTxSetCursor, *TxQueryHitPoint;
    HRESULT (WINAPI *OnTxInPlaceActivate)(void *, const RECT *);
    void *OnTxInPlaceDeactivate;
    HRESULT (WINAPI *OnTxUIActivate)(void *);
    HRESULT (WINAPI *OnTxUIDeactivate)(void *);
    void *TxGetText;
    HRESULT (WINAPI *TxSetText)(void *, const WCHAR *);
    HRESULT (WINAPI *TxGetCurTargetX)(void *, LONG *);
    HRESULT (WINAPI *TxGetBaseLinePos)(void *, LONG *);
    void *TxGetNaturalSize, *TxGetDropTarget, *OnTxPropertyBitsChange;
    HRESULT (WINAPI *TxGetCachedSize)(void *, DWORD *, DWORD *);
};
struct services { const struct services_vtbl *lpVtbl; };
typedef HRESULT (WINAPI *create_text_services_fn)(IUnknown *, void *, IUnknown **);

int main(void)
{
    struct host host = { host_vtbl, 1, { 0, 0, 180, 30 } };
    HMODULE msftedit;
    create_text_services_fn create;
    IUnknown *unk = NULL;
    struct services *srv = NULL;
    const GUID iid_services = {0x8d33f740, 0xcf58, 0x11ce, {0xa8, 0x9d, 0x00, 0xaa, 0x00, 0x6c, 0xad, 0xc5}};
    LONG x0 = -1, x1 = -1, base = -1;
    DWORD w1 = 0, h1 = 0, w2 = 0, h2 = 0;
    LRESULT res;
    RECT big = { 0, 0, 260, 50 };
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    msftedit = LoadLibraryA("msftedit.dll");
    create = (create_text_services_fn)GetProcAddress(msftedit, "CreateTextServices");
    hr = create(NULL, &host, &unk);
    CHECK(hr == S_OK && unk, "create %#lx", hr);
    if (!unk) return 1;
    hr = IUnknown_QueryInterface(unk, &iid_services, (void **)&srv);
    CHECK(hr == S_OK && srv, "QI ITextServices %#lx", hr);
    if (!srv) return 1;

    hr = srv->lpVtbl->TxSetText(srv, L"WWWWWW");
    CHECK(hr == S_OK, "TxSetText %#lx", hr);
    hr = srv->lpVtbl->OnTxInPlaceActivate(srv, &host.client);
    CHECK(hr == S_OK, "OnTxInPlaceActivate %#lx", hr);

    /* UI activation is only a notice, always S_OK */
    CHECK(srv->lpVtbl->OnTxUIActivate(srv) == S_OK, "OnTxUIActivate");
    CHECK(srv->lpVtbl->OnTxUIDeactivate(srv) == S_OK, "OnTxUIDeactivate");

    /* the cached size follows the rectangle the text lives in */
    hr = srv->lpVtbl->TxGetCachedSize(srv, &w1, &h1);
    CHECK(hr == S_OK && w1 > 100 && w1 <= 180 && h1 > 10 && h1 <= 30, "cached size %#lx %lux%lu", hr, w1, h1);
    host.client = big;
    srv->lpVtbl->OnTxInPlaceActivate(srv, &big);
    srv->lpVtbl->TxSendMessage(srv, WM_SIZE, 0, MAKELPARAM(260, 50), &res);
    hr = srv->lpVtbl->TxGetCachedSize(srv, &w2, &h2);
    CHECK(hr == S_OK && w2 > w1 && h2 > h1, "cached size after a bigger client %#lx %lux%lu", hr, w2, h2);
    CHECK(srv->lpVtbl->TxGetCachedSize(srv, NULL, &h2) == E_INVALIDARG, "NULL width");
    CHECK(srv->lpVtbl->TxGetCachedSize(srv, &w2, NULL) == E_INVALIDARG, "NULL height");

    /* the base line of the first line is inside the client rectangle */
    hr = srv->lpVtbl->TxGetBaseLinePos(srv, &base);
    CHECK(hr == S_OK && base > 5 && base < 50, "base line %#lx %ld", hr, base);
    CHECK(srv->lpVtbl->TxGetBaseLinePos(srv, NULL) == E_INVALIDARG, "NULL base line");

    /* the caret's x: at the start of the text, then at its end */
    srv->lpVtbl->TxSendMessage(srv, EM_SETSEL, 0, 0, &res);
    hr = srv->lpVtbl->TxGetCurTargetX(srv, &x0);
    CHECK(hr == S_OK && x0 >= 0 && x0 < 10, "caret x at the start %#lx %ld", hr, x0);
    srv->lpVtbl->TxSendMessage(srv, EM_SETSEL, 6, 6, &res);
    hr = srv->lpVtbl->TxGetCurTargetX(srv, &x1);
    CHECK(hr == S_OK && x1 > x0 + 20, "caret x at the end %#lx %ld vs %ld", hr, x1, x0);
    CHECK(srv->lpVtbl->TxGetCurTargetX(srv, NULL) == E_INVALIDARG, "NULL x");

    srv->lpVtbl->Release(srv);
    IUnknown_Release(unk);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
