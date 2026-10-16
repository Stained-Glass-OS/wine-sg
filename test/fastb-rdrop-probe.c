/* riched20 ITextServices (patch 3699): TxQueryHitPoint and TxGetDropTarget on a windowless text host */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <stdio.h>
#include <ole2.h>
#define TXTBIT_READONLY 4

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
static int back_style;  /* 0 transparent, 1 opaque */
static HRESULT WINAPI host_TxGetBackStyle(H, int *style) { *style = back_style; return S_OK; }
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
    void *TxDraw, *TxGetHScroll, *TxGetVScroll, *OnTxSetCursor;
    HRESULT (WINAPI *TxQueryHitPoint)(void *, DWORD, LONG, void *, void *, HDC, HDC, const RECT *, INT, INT, DWORD *);
    HRESULT (WINAPI *OnTxInPlaceActivate)(void *, const RECT *);
    void *OnTxInPlaceDeactivate, *OnTxUIActivate, *OnTxUIDeactivate;
    HRESULT (WINAPI *TxGetText)(void *, BSTR *);
    HRESULT (WINAPI *TxSetText)(void *, const WCHAR *);
    void *TxGetCurTargetX, *TxGetBaseLinePos, *TxGetNaturalSize;
    HRESULT (WINAPI *TxGetDropTarget)(void *, IDropTarget **);
    HRESULT (WINAPI *OnTxPropertyBitsChange)(void *, DWORD, DWORD);
};
struct services { const struct services_vtbl *lpVtbl; };
typedef HRESULT (WINAPI *create_text_services_fn)(IUnknown *, void *, IUnknown **);

/* ---- a data object with a few formats ---- */
struct dobj
{
    IDataObject IDataObject_iface;
    LONG ref;
    UINT cf[3];
    const void *data[3];
    SIZE_T size[3];
};
static struct dobj *impl_dobj(IDataObject *iface) { return CONTAINING_RECORD(iface, struct dobj, IDataObject_iface); }
static HRESULT WINAPI dobj_QueryInterface(IDataObject *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IDataObject)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI dobj_AddRef(IDataObject *iface) { return InterlockedIncrement(&impl_dobj(iface)->ref); }
static ULONG WINAPI dobj_Release(IDataObject *iface) { return InterlockedDecrement(&impl_dobj(iface)->ref); }
static int find_format(struct dobj *d, FORMATETC *f)
{
    int i;
    if (f->tymed != TYMED_HGLOBAL && f->tymed != (TYMED_HGLOBAL | TYMED_ISTREAM)) return -1;
    for (i = 0; i < 3; i++) if (d->data[i] && d->cf[i] == f->cfFormat) return i;
    return -1;
}
static HRESULT WINAPI dobj_GetData(IDataObject *iface, FORMATETC *f, STGMEDIUM *m)
{
    struct dobj *d = impl_dobj(iface);
    int i = find_format(d, f);
    void *p;
    if (i < 0) return DV_E_FORMATETC;
    m->tymed = TYMED_HGLOBAL;
    m->pUnkForRelease = NULL;
    m->hGlobal = GlobalAlloc(GMEM_MOVEABLE, d->size[i]);
    p = GlobalLock(m->hGlobal);
    memcpy(p, d->data[i], d->size[i]);
    GlobalUnlock(m->hGlobal);
    return S_OK;
}
static HRESULT WINAPI dobj_GetDataHere(IDataObject *iface, FORMATETC *f, STGMEDIUM *m) { return E_NOTIMPL; }
static HRESULT WINAPI dobj_QueryGetData(IDataObject *iface, FORMATETC *f)
{
    return find_format(impl_dobj(iface), f) >= 0 ? S_OK : DV_E_FORMATETC;
}
static HRESULT WINAPI dobj_GetCanonicalFormatEtc(IDataObject *iface, FORMATETC *a, FORMATETC *b) { return E_NOTIMPL; }
static HRESULT WINAPI dobj_SetData(IDataObject *iface, FORMATETC *a, STGMEDIUM *b, BOOL c) { return E_NOTIMPL; }
static HRESULT WINAPI dobj_EnumFormatEtc(IDataObject *iface, DWORD d, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI dobj_DAdvise(IDataObject *iface, FORMATETC *a, DWORD b, IAdviseSink *c, DWORD *d) { return E_NOTIMPL; }
static HRESULT WINAPI dobj_DUnadvise(IDataObject *iface, DWORD a) { return E_NOTIMPL; }
static HRESULT WINAPI dobj_EnumDAdvise(IDataObject *iface, IEnumSTATDATA **e) { return E_NOTIMPL; }
static const IDataObjectVtbl dobj_vtbl =
{
    dobj_QueryInterface, dobj_AddRef, dobj_Release, dobj_GetData, dobj_GetDataHere, dobj_QueryGetData,
    dobj_GetCanonicalFormatEtc, dobj_SetData, dobj_EnumFormatEtc, dobj_DAdvise, dobj_DUnadvise, dobj_EnumDAdvise,
};

static struct services *srv;

static void expect_text(const WCHAR *want, const char *what)
{
    BSTR got = NULL;
    HRESULT hr = srv->lpVtbl->TxGetText(srv, &got);
    CHECK(hr == S_OK && got && !wcscmp(got, want), "%s: text %s want %s", what, got ? "(set)" : "(null)", "(see next)");
    if (got && wcscmp(got, want)) printf("   got [%ls] want [%ls]\n", got, want);
    SysFreeString(got);
}

static void test_hit(struct host *host)
{
    DWORD hit;
    HRESULT hr;
    RECT r = { 0, 0, 180, 30 };

    srv->lpVtbl->TxSetText(srv, L"WWW");
    srv->lpVtbl->OnTxInPlaceActivate(srv, &host->client);

    back_style = 0;
    hit = 99;
    hr = srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, 10, 8, &hit);
    CHECK(hr == S_OK && hit == 3, "over text: hr %#lx hit %lu", hr, hit);
    hit = 99;
    hr = srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, 175, 8, &hit);
    CHECK(hr == S_OK && hit == 1, "far from the text, transparent: hr %#lx hit %lu", hr, hit);
    hit = 99;
    hr = srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, 10, 100, &hit);
    CHECK(hr == S_OK && hit == 0, "outside the client rectangle: hr %#lx hit %lu", hr, hit);
    hit = 99;
    hr = srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, -1, 5, &hit);
    CHECK(hr == S_OK && hit == 0, "left of the client rectangle: hr %#lx hit %lu", hr, hit);
    /* a point a little right of the last glyph is close */
    {
        int x, found = 0;
        for (x = 10; x < 175; x++)
        {
            hit = 99;
            srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, x, 8, &hit);
            if (hit == 2) found++;
        }
        CHECK(found >= 2 && found <= 8, "points close to the text: %d", found);
    }
    back_style = 1;
    hit = 99;
    hr = srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, 175, 8, &hit);
    CHECK(hr == S_OK && hit == 3, "opaque background anywhere: hr %#lx hit %lu", hr, hit);
    hit = 99;
    hr = srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, 400, 8, &hit);
    CHECK(hr == S_OK && hit == 0, "opaque, outside: hr %#lx hit %lu", hr, hit);
    CHECK(srv->lpVtbl->TxQueryHitPoint(srv, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &r, 5, 5, NULL) == E_INVALIDARG,
          "NULL result");
    back_style = 0;
}

static void test_drop(struct host *host)
{
    static const WCHAR wide[] = L"XYZ";
    static const char ansi[] = "abc";
    static const char rtf[] = "{\\rtf1 RTF}";
    UINT rtf_cf = RegisterClipboardFormatW(L"Rich Text Format");
    struct dobj d = { { (IDataObjectVtbl *)&dobj_vtbl }, 1 };
    IDropTarget *dt = NULL, *dt2 = NULL;
    POINTL pt = { 0, 5 };
    DWORD effect;
    HRESULT hr;

    srv->lpVtbl->TxSetText(srv, L"WWWWWW");
    hr = srv->lpVtbl->TxGetDropTarget(srv, &dt);
    CHECK(hr == S_OK && dt, "TxGetDropTarget %#lx", hr);
    if (!dt) return;
    hr = srv->lpVtbl->TxGetDropTarget(srv, &dt2);
    CHECK(hr == S_OK && dt2 && dt2 != dt, "a second drop target %#lx", hr);
    if (dt2) IDropTarget_Release(dt2);
    CHECK(srv->lpVtbl->TxGetDropTarget(srv, NULL) == E_INVALIDARG, "NULL target");

    d.cf[0] = CF_UNICODETEXT; d.data[0] = wide; d.size[0] = sizeof(wide);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    hr = IDropTarget_DragEnter(dt, &d.IDataObject_iface, 0, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_MOVE, "DragEnter move by default: %#lx effect %lu", hr, effect);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    hr = IDropTarget_DragOver(dt, MK_CONTROL, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_COPY, "Control copies: effect %lu", effect);
    effect = DROPEFFECT_COPY;
    IDropTarget_DragOver(dt, 0, pt, &effect);
    CHECK(effect == DROPEFFECT_COPY, "only copy allowed: effect %lu", effect);
    effect = DROPEFFECT_LINK;
    IDropTarget_DragOver(dt, 0, pt, &effect);
    CHECK(effect == DROPEFFECT_NONE, "only link allowed: effect %lu", effect);
    IDropTarget_DragLeave(dt);
    CHECK(d.ref == 1, "DragLeave lets go of the data: ref %ld", d.ref);

    /* the drop puts the text at the point */
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    IDropTarget_DragEnter(dt, &d.IDataObject_iface, 0, pt, &effect);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    hr = IDropTarget_Drop(dt, &d.IDataObject_iface, 0, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_MOVE, "Drop: %#lx effect %lu", hr, effect);
    expect_text(L"XYZWWWWWW", "unicode drop at the start");
    CHECK(d.ref == 1, "Drop lets go of the data: ref %ld", d.ref);

    /* ANSI text only, dropped further right */
    d.cf[0] = CF_TEXT; d.data[0] = ansi; d.size[0] = sizeof(ansi);
    pt.x = 900; pt.y = 5;
    effect = DROPEFFECT_COPY;
    hr = IDropTarget_Drop(dt, &d.IDataObject_iface, MK_CONTROL, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_COPY, "ANSI drop: %#lx effect %lu", hr, effect);
    expect_text(L"XYZWWWWWWabc", "ANSI drop after the end");

    /* rich text */
    d.cf[0] = rtf_cf; d.data[0] = rtf; d.size[0] = sizeof(rtf);
    pt.x = 0;
    effect = DROPEFFECT_COPY;
    hr = IDropTarget_Drop(dt, &d.IDataObject_iface, 0, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_COPY, "RTF drop: %#lx effect %lu", hr, effect);
    expect_text(L"RTFXYZWWWWWWabc", "rich text drop");

    /* data that is not text is refused */
    d.cf[0] = CF_BITMAP; d.data[0] = wide; d.size[0] = sizeof(wide);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    hr = IDropTarget_DragEnter(dt, &d.IDataObject_iface, 0, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_NONE, "bitmap data: %#lx effect %lu", hr, effect);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    hr = IDropTarget_Drop(dt, &d.IDataObject_iface, 0, pt, &effect);
    CHECK(effect == DROPEFFECT_NONE, "bitmap drop: effect %lu", effect);
    expect_text(L"RTFXYZWWWWWWabc", "refused drop changes nothing");

    /* a read-only control takes no drop */
    d.cf[0] = CF_UNICODETEXT; d.data[0] = wide; d.size[0] = sizeof(wide);
    srv->lpVtbl->OnTxPropertyBitsChange(srv, TXTBIT_READONLY, TXTBIT_READONLY);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    IDropTarget_DragEnter(dt, &d.IDataObject_iface, 0, pt, &effect);
    CHECK(effect == DROPEFFECT_NONE, "read-only DragEnter: effect %lu", effect);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    IDropTarget_Drop(dt, &d.IDataObject_iface, 0, pt, &effect);
    CHECK(effect == DROPEFFECT_NONE, "read-only Drop: effect %lu", effect);
    expect_text(L"RTFXYZWWWWWWabc", "read-only drop changes nothing");
    srv->lpVtbl->OnTxPropertyBitsChange(srv, TXTBIT_READONLY, 0);
    IDropTarget_Release(dt);
}

int main(void)
{
    struct host host = { host_vtbl, 1, { 0, 0, 180, 30 } };
    HMODULE msftedit;
    create_text_services_fn create;
    IUnknown *unk = NULL;
    const GUID iid_services = {0x8d33f740, 0xcf58, 0x11ce, {0xa8, 0x9d, 0x00, 0xaa, 0x00, 0x6c, 0xad, 0xc5}};
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    msftedit = LoadLibraryA("msftedit.dll");
    create = (create_text_services_fn)GetProcAddress(msftedit, "CreateTextServices");
    hr = create(NULL, &host, &unk);
    CHECK(hr == S_OK && unk, "create %#lx", hr);
    if (!unk) return 1;
    hr = IUnknown_QueryInterface(unk, &iid_services, (void **)&srv);
    CHECK(hr == S_OK && srv, "QI ITextServices %#lx", hr);
    if (!srv) return 1;

    test_hit(&host);
    test_drop(&host);

    srv->lpVtbl->Release(srv);
    IUnknown_Release(unk);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
