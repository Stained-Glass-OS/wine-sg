/* Probe for patches/sg/1490: RichEdit's windowless text services answer
 * ITextServices2 and ITextDocument2, and TxDrawD2D draws into a Direct2D
 * target.  Office's React Native text boxes (Word's start screen "Describe
 * the document you'd like to write") look up IID_ITextServices2 in the
 * RichEdit DLL, ask the text services for ITextServices2 and ITextDocument2,
 * and draw with TxDrawD2D; Word ended in a fail-fast when any was missing.
 *
 * Prints name=1 / name=0 lines; the gate checks them.
 *
 *   x86_64-w64-mingw32-gcc -O2 -o richtxsrv2-probe.exe richtxsrv2-probe.c -ld2d1 -lwindowscodecs -lole32 -luuid
 */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <d2d1.h>
#include <wincodec.h>
#include <stdio.h>

static const GUID iid_services2 = {0x8d33f741, 0xcf58, 0x11ce, {0xa8, 0x9d, 0x00, 0xaa, 0x00, 0x6c, 0xad, 0xc5}};
static const GUID iid_document2 = {0xc241f5e0, 0x7206, 0x11d8, {0xa2, 0xc7, 0x00, 0xa0, 0xd1, 0xd6, 0xc6, 0xb3}};

static void report(const char *name, int ok, const char *fmt, ...)
{
    va_list ap;
    printf("%s=%d", name, ok ? 1 : 0);
    if (fmt)
    {
        printf(" ");
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
    printf("\n");
    fflush(stdout);
}

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
static HRESULT WINAPI host_TxGetCharFormat(H, const CHARFORMATW **fmt)
{
    static CHARFORMAT2W cf;
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

/* ---- ITextServices2, by vtable slot ---- */

struct services2_vtbl
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *TxSendMessage)(void *, UINT, WPARAM, LPARAM, LRESULT *);
    void *TxDraw, *TxGetHScroll, *TxGetVScroll, *OnTxSetCursor, *TxQueryHitPoint;
    HRESULT (WINAPI *OnTxInPlaceActivate)(void *, const RECT *);
    void *OnTxInPlaceDeactivate, *OnTxUIActivate, *OnTxUIDeactivate, *TxGetText;
    HRESULT (WINAPI *TxSetText)(void *, const WCHAR *);
    void *TxGetCurTargetX, *TxGetBaseLinePos;
    HRESULT (WINAPI *TxGetNaturalSize)(void *, DWORD, HDC, HDC, void *, DWORD, const SIZEL *, LONG *, LONG *);
    void *TxGetDropTarget, *OnTxPropertyBitsChange, *TxGetCachedSize;
    HRESULT (WINAPI *TxGetNaturalSize2)(void *, DWORD, HDC, HDC, void *, DWORD, const SIZEL *, LONG *, LONG *, LONG *);
    HRESULT (WINAPI *TxDrawD2D)(void *, ID2D1RenderTarget *, const RECTL *, RECT *, LONG);
};
struct services2 { const struct services2_vtbl *lpVtbl; };

typedef HRESULT (WINAPI *create_text_services_fn)(IUnknown *, void *, IUnknown **);

/* an ITextDocument2 method by its slot: IUnknown 3 + IDispatch 4 + ITextDocument 19 = 26 before
 * ITextDocument2's own (GetCaretType); GetNotificationMode is the 11th, SetNotificationMode the 12th,
 * GetTypographyOptions the 15th, SetTypographyOptions the 34th */
#define DOC2_SLOT(n) (26 + (n))
typedef HRESULT (WINAPI *doc_get_fn)(void *, LONG *);
typedef HRESULT (WINAPI *doc_set_fn)(void *, LONG);
typedef HRESULT (WINAPI *doc_set2_fn)(void *, LONG, LONG);

int main(void)
{
    struct host host = { host_vtbl, 1, { 0, 0, 180, 30 } };
    HMODULE msftedit = LoadLibraryA("msftedit.dll"), riched20 = LoadLibraryA("riched20.dll");
    const GUID *exported, *exported20;
    create_text_services_fn create;
    IUnknown *unk = NULL;
    struct services2 *srv2 = NULL;
    void **doc2 = NULL;
    HRESULT hr;

    exported = (const GUID *)GetProcAddress(msftedit, "IID_ITextServices2");
    exported20 = (const GUID *)GetProcAddress(riched20, "IID_ITextServices2");
    report("export_msftedit", exported && IsEqualGUID(exported, &iid_services2), "%p", exported);
    report("export_riched20", exported20 && IsEqualGUID(exported20, &iid_services2), "%p", exported20);

    create = (create_text_services_fn)GetProcAddress(msftedit, "CreateTextServices");
    hr = create ? create(NULL, &host, &unk) : E_FAIL;
    report("create", SUCCEEDED(hr) && unk, "%#lx", hr);
    if (!unk) { printf("done=0\n"); return 1; }

    hr = IUnknown_QueryInterface(unk, &iid_services2, (void **)&srv2);
    report("qi_services2", SUCCEEDED(hr) && srv2, "%#lx", hr);
    hr = IUnknown_QueryInterface(unk, &iid_document2, (void **)&doc2);
    report("qi_document2", SUCCEEDED(hr) && doc2, "%#lx", hr);

    if (doc2)
    {
        void **vt = *(void ***)doc2;
        LONG v = 0;
        hr = ((doc_set_fn)vt[DOC2_SLOT(11)])(doc2, -1);
        hr = SUCCEEDED(hr) ? ((doc_get_fn)vt[DOC2_SLOT(10)])(doc2, &v) : hr;
        report("doc2_notification_mode", SUCCEEDED(hr) && v == -1, "%#lx %ld", hr, v);
        v = 0;
        hr = ((doc_set2_fn)vt[DOC2_SLOT(33)])(doc2, 1, 1);
        hr = SUCCEEDED(hr) ? ((doc_get_fn)vt[DOC2_SLOT(14)])(doc2, &v) : hr;
        report("doc2_typography", SUCCEEDED(hr) && v == 1, "%#lx %ld", hr, v);
        ((IUnknown *)doc2)->lpVtbl->Release((IUnknown *)doc2);
    }

    if (srv2)
    {
        LONG w = 180, hgt = 30, ascent = -1;
        SIZEL extent = { -1, -1 };
        HDC dc = GetDC(NULL);
        IWICImagingFactory *wic = NULL;
        IWICBitmap *bitmap = NULL;
        ID2D1Factory *factory = NULL;
        ID2D1RenderTarget *rt = NULL;
        D2D1_RENDER_TARGET_PROPERTIES props = { D2D1_RENDER_TARGET_TYPE_DEFAULT,
                { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 0, 0,
                D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT };
        D2D1_COLOR_F clear = { 0, 0, 0, 0 };
        RECTL bounds = { 10, 5, 190, 35 };

        hr = srv2->lpVtbl->TxSetText(srv2, L"WWWWWW");
        report("settext", SUCCEEDED(hr), "%#lx", hr);
        hr = srv2->lpVtbl->TxGetNaturalSize2(srv2, DVASPECT_CONTENT, dc, NULL, NULL, 1 /* TXTNS_FITTOCONTENT */,
                                              &extent, &w, &hgt, &ascent);
        report("natural_size2", SUCCEEDED(hr) && w > 20 && hgt > 10, "%#lx %ldx%ld", hr, w, hgt);
        report("ascent", ascent > 0 && ascent < hgt, "%ld of %ld", ascent, hgt);
        ReleaseDC(NULL, dc);

        CoInitialize(NULL);
        hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                              (void **)&wic);
        if (SUCCEEDED(hr))
            hr = IWICImagingFactory_CreateBitmap(wic, 200, 40, &GUID_WICPixelFormat32bppPBGRA,
                                                 WICBitmapCacheOnLoad, &bitmap);
        if (SUCCEEDED(hr))
            hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&factory);
        if (SUCCEEDED(hr))
            hr = ID2D1Factory_CreateWicBitmapRenderTarget(factory, bitmap, &props, &rt);
        report("d2d_target", SUCCEEDED(hr), "%#lx", hr);
        if (rt)
        {
            HRESULT draw_hr;
            IWICBitmapLock *lock;
            WICRect all = { 0, 0, 200, 40 };
            UINT size, stride, x, y, ink = 0, clear_inside = 0, outside_drawn = 0;
            BYTE *data;

            ID2D1RenderTarget_BeginDraw(rt);
            ID2D1RenderTarget_Clear(rt, &clear);
            draw_hr = srv2->lpVtbl->TxDrawD2D(srv2, rt, &bounds, NULL, 0 /* TXTVIEW_ACTIVE */);
            hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
            report("txdrawd2d", SUCCEEDED(draw_hr) && SUCCEEDED(hr), "%#lx %#lx", draw_hr, hr);

            if (SUCCEEDED(IWICBitmap_Lock(bitmap, &all, WICBitmapLockRead, &lock)))
            {
                IWICBitmapLock_GetStride(lock, &stride);
                IWICBitmapLock_GetDataPointer(lock, &size, &data);
                for (y = 0; y < 40; y++)
                    for (x = 0; x < 200; x++)
                    {
                        BYTE *p = data + y * stride + x * 4;
                        BOOL inside = x >= 10 && x < 190 && y >= 5 && y < 35;
                        if (!inside) { if (p[3]) outside_drawn++; continue; }
                        if (p[3] > 200 && p[0] < 80 && p[1] < 80 && p[2] < 80) ink++;
                        if (!p[3]) clear_inside++;
                    }
                IWICBitmapLock_Release(lock);
            }
            report("text_drawn", ink > 20, "%u dark opaque pixels", ink);
            report("background_kept", clear_inside > 1000, "%u transparent pixels inside", clear_inside);
            report("bounds_kept", !outside_drawn, "%u pixels drawn outside", outside_drawn);
            ID2D1RenderTarget_Release(rt);
        }
    }
    printf("done=1\n");
    return 0;
}
