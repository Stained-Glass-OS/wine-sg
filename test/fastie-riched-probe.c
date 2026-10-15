#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
const GUID IID_ITextDocument = {0x8cc497c0,0xa1df,0x11ce,{0x80,0x98,0x00,0xaa,0x00,0x47,0xbe,0x5d}};
static int bad, total;
#define EXPECT_HR(e, c) do { HRESULT h_ = (c); total++; if (h_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lx, expected %lx\n", __LINE__, #c, h_, (HRESULT)(e)); } } while (0)
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)


/* a minimal embedded object: its view fills the rectangle with red */
typedef struct { IOleObject IOleObject_iface; IViewObject IViewObject_iface; LONG ref; } testobj;
static testobj *impl_obj(IOleObject *i) { return CONTAINING_RECORD(i, testobj, IOleObject_iface); }
static HRESULT WINAPI o_QI(IOleObject *i, REFIID riid, void **ppv)
{
    testobj *o = impl_obj(i);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleObject)) *ppv = &o->IOleObject_iface;
    else if (IsEqualIID(riid, &IID_IViewObject)) *ppv = &o->IViewObject_iface;
    else { *ppv = NULL; return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}
static ULONG WINAPI o_AddRef(IOleObject *i) { return InterlockedIncrement(&impl_obj(i)->ref); }
static ULONG WINAPI o_Release(IOleObject *i) { return InterlockedDecrement(&impl_obj(i)->ref); }
static HRESULT WINAPI o_SetClientSite(IOleObject *i, IOleClientSite *s) { return S_OK; }
static HRESULT WINAPI o_GetClientSite(IOleObject *i, IOleClientSite **s) { return E_NOTIMPL; }
static HRESULT WINAPI o_SetHostNames(IOleObject *i, LPCOLESTR a, LPCOLESTR b) { return E_NOTIMPL; }
static HRESULT WINAPI o_Close(IOleObject *i, DWORD f) { return S_OK; }
static HRESULT WINAPI o_SetMoniker(IOleObject *i, DWORD w, IMoniker *m) { return E_NOTIMPL; }
static HRESULT WINAPI o_GetMoniker(IOleObject *i, DWORD a, DWORD w, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI o_InitFromData(IOleObject *i, IDataObject *d, BOOL f, DWORD r) { return E_NOTIMPL; }
static HRESULT WINAPI o_GetClipboardData(IOleObject *i, DWORD r, IDataObject **d) { return E_NOTIMPL; }
static HRESULT WINAPI o_DoVerb(IOleObject *i, LONG v, LPMSG m, IOleClientSite *s, LONG idx, HWND w, LPCRECT r) { return E_NOTIMPL; }
static HRESULT WINAPI o_EnumVerbs(IOleObject *i, IEnumOLEVERB **e) { return E_NOTIMPL; }
static HRESULT WINAPI o_Update(IOleObject *i) { return S_OK; }
static HRESULT WINAPI o_IsUpToDate(IOleObject *i) { return S_OK; }
static HRESULT WINAPI o_GetUserClassID(IOleObject *i, CLSID *c) { *c = CLSID_NULL; return S_OK; }
static HRESULT WINAPI o_GetUserType(IOleObject *i, DWORD f, LPOLESTR *t) { return E_NOTIMPL; }
static HRESULT WINAPI o_SetExtent(IOleObject *i, DWORD a, SIZEL *s) { return S_OK; }
static HRESULT WINAPI o_GetExtent(IOleObject *i, DWORD a, SIZEL *s) { return E_NOTIMPL; }
static HRESULT WINAPI o_Advise(IOleObject *i, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI o_Unadvise(IOleObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI o_EnumAdvise(IOleObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static HRESULT WINAPI o_GetMiscStatus(IOleObject *i, DWORD a, DWORD *s) { return E_NOTIMPL; }
static HRESULT WINAPI o_SetColorScheme(IOleObject *i, LOGPALETTE *p) { return E_NOTIMPL; }
static IOleObjectVtbl objvtbl = { o_QI, o_AddRef, o_Release, o_SetClientSite, o_GetClientSite, o_SetHostNames, o_Close,
    o_SetMoniker, o_GetMoniker, o_InitFromData, o_GetClipboardData, o_DoVerb, o_EnumVerbs, o_Update, o_IsUpToDate,
    o_GetUserClassID, o_GetUserType, o_SetExtent, o_GetExtent, o_Advise, o_Unadvise, o_EnumAdvise, o_GetMiscStatus,
    o_SetColorScheme };
static testobj *impl_view(IViewObject *i) { return CONTAINING_RECORD(i, testobj, IViewObject_iface); }
static HRESULT WINAPI v_QI(IViewObject *i, REFIID r, void **p) { return o_QI(&impl_view(i)->IOleObject_iface, r, p); }
static ULONG WINAPI v_AddRef(IViewObject *i) { return o_AddRef(&impl_view(i)->IOleObject_iface); }
static ULONG WINAPI v_Release(IViewObject *i) { return o_Release(&impl_view(i)->IOleObject_iface); }
static HRESULT WINAPI v_Draw(IViewObject *i, DWORD aspect, LONG idx, void *aspect_p, DVTARGETDEVICE *td, HDC ic, HDC dc,
    const RECTL *b, const RECTL *w, BOOL (CALLBACK *cont)(ULONG_PTR), ULONG_PTR c)
{
    HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
    RECT r = { b->left, b->top, b->right, b->bottom };
    FillRect(dc, &r, red);
    DeleteObject(red);
    return S_OK;
}
static HRESULT WINAPI v_GetColorSet(IViewObject *i, DWORD a, LONG l, void *p, DVTARGETDEVICE *t, HDC h, LOGPALETTE **c) { return E_NOTIMPL; }
static HRESULT WINAPI v_Freeze(IViewObject *i, DWORD a, LONG l, void *p, DWORD *f) { return E_NOTIMPL; }
static HRESULT WINAPI v_Unfreeze(IViewObject *i, DWORD f) { return E_NOTIMPL; }
static HRESULT WINAPI v_SetAdvise(IViewObject *i, DWORD a, DWORD f, IAdviseSink *s) { return S_OK; }
static HRESULT WINAPI v_GetAdvise(IViewObject *i, DWORD *a, DWORD *f, IAdviseSink **s) { return E_NOTIMPL; }
static IViewObjectVtbl viewvtbl = { v_QI, v_AddRef, v_Release, v_Draw, v_GetColorSet, v_Freeze, v_Unfreeze, v_SetAdvise, v_GetAdvise };

static HWND make_edit(DWORD style, const WCHAR *text, IRichEditOle **reole)
{
    HWND w = CreateWindowExW(0, L"RichEdit20W", text, style | ES_MULTILINE, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    SendMessageW(w, EM_GETOLEINTERFACE, 0, (LPARAM)reole);
    return w;
}

static HRESULT insert_obj(IRichEditOle *reole, testobj *o, LONG cp, IOleClientSite **site)
{
    REOBJECT ro = { sizeof(ro) };
    reole->lpVtbl->GetClientSite(reole, site);
    ro.cp = cp;
    ro.clsid = CLSID_NULL;
    ro.poleobj = &o->IOleObject_iface;
    ro.polesite = *site;
    ro.sizel.cx = 2000;
    ro.sizel.cy = 2000;
    ro.dvaspect = DVASPECT_CONTENT;
    return reole->lpVtbl->InsertObject(reole, &ro);
}

/* render the control into a bitmap and count the red and white pixels */
static void count_pixels(HWND edit, int *red, int *white)
{
    HDC dc = GetDC(NULL), mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, 300, 300), old = SelectObject(mem, bmp);
    FORMATRANGE fr = { 0 };
    int x, y;

    PatBlt(mem, 0, 0, 300, 300, WHITENESS);
    fr.hdc = fr.hdcTarget = mem;
    fr.rc.right = fr.rcPage.right = 300 * 15;
    fr.rc.bottom = fr.rcPage.bottom = 300 * 15;
    fr.chrg.cpMax = -1;
    SendMessageW(edit, EM_FORMATRANGE, TRUE, (LPARAM)&fr);
    SendMessageW(edit, EM_FORMATRANGE, FALSE, 0);
    *red = *white = 0;
    for (y = 0; y < 300; y++)
        for (x = 0; x < 300; x++)
        {
            COLORREF c = GetPixel(mem, x, y);
            if (c == RGB(255, 0, 0)) (*red)++;
            else if (c == RGB(255, 255, 255)) (*white)++;
        }
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, dc);
}

int main(void)
{
    HWND edit;
    IRichEditOle *reole = NULL;
    ITextDocument *doc = NULL;
    ITextSelection *sel = NULL;
    ITextRange *range = NULL;
    ITextFont *font = NULL, *dup = NULL;
    LONG v;

    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    LoadLibraryW(L"riched20.dll");
    edit = CreateWindowExW(0, L"RichEdit20W", L"hello world", WS_POPUP | ES_MULTILINE, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    EXPECT(edit != NULL);
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&reole);
    EXPECT(reole != NULL);
    EXPECT_HR(S_OK, IUnknown_QueryInterface((IUnknown *)reole, &IID_ITextDocument, (void **)&doc));

    /* selection flags */
    EXPECT_HR(S_OK, ITextDocument_GetSelection(doc, &sel));
    v = 0xdead;
    EXPECT_HR(S_OK, ITextSelection_GetFlags(sel, &v));
    EXPECT(v == 0);
    EXPECT_HR(E_INVALIDARG, ITextSelection_GetFlags(sel, NULL));
    EXPECT_HR(S_OK, ITextSelection_SetFlags(sel, tomSelStartActive | tomSelAtEOL));
    EXPECT_HR(S_OK, ITextSelection_GetFlags(sel, &v));
    EXPECT(v == (tomSelStartActive | tomSelAtEOL));
    EXPECT_HR(S_OK, ITextSelection_SetFlags(sel, 0));
    EXPECT_HR(S_OK, ITextSelection_GetFlags(sel, &v));
    EXPECT(v == 0);

    /* font style handle */
    EXPECT_HR(S_OK, ITextDocument_Range(doc, 0, 5, &range));
    EXPECT_HR(S_OK, ITextRange_GetFont(range, &font));
    v = 0xdead;
    EXPECT_HR(S_OK, ITextFont_GetStyle(font, &v));
    EXPECT(v == 0);
    EXPECT_HR(E_INVALIDARG, ITextFont_GetStyle(font, NULL));
    EXPECT_HR(S_OK, ITextFont_SetStyle(font, 7));
    EXPECT_HR(S_OK, ITextFont_GetStyle(font, &v));
    EXPECT(v == 7);
    EXPECT_HR(S_OK, ITextFont_GetDuplicate(font, &dup));
    EXPECT_HR(S_OK, ITextFont_GetStyle(dup, &v));
    EXPECT(v == 7);
    ITextFont_Release(dup);


    /* IOleClientSite::ShowObject scrolls the object into view */
    {
        IRichEditOle *r2 = NULL;
        IOleClientSite *site = NULL;
        testobj obj = { { &objvtbl }, { &viewvtbl }, 1 };
        WCHAR text[600];
        LONG first;
        int i, red, white, red0;
        HWND e2;

        for (i = 0; i < 60; i++) wcscpy(text + i * 5, L"line\r");

        e2 = make_edit(WS_POPUP | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL, text, &r2);
        EXPECT(e2 != NULL && r2 != NULL);
        SendMessageW(e2, EM_SETSEL, 300, 300);
        EXPECT_HR(S_OK, insert_obj(r2, &obj, REO_CP_SELECTION, &site));
        SendMessageW(e2, WM_VSCROLL, SB_TOP, 0);
        first = SendMessageW(e2, EM_GETFIRSTVISIBLELINE, 0, 0);
        EXPECT(first == 0);
        EXPECT_HR(S_OK, IOleClientSite_ShowObject(site));
        first = SendMessageW(e2, EM_GETFIRSTVISIBLELINE, 0, 0);
        EXPECT(first > 0);
        IOleClientSite_Release(site);
        r2->lpVtbl->Release(r2);
        DestroyWindow(e2);
    }

    /* OnShowWindow shades the object while it is open in its own window */
    {
        IRichEditOle *r2 = NULL;
        IOleClientSite *site = NULL;
        testobj obj = { { &objvtbl }, { &viewvtbl }, 1 };
        int red, white, red0;
        HWND e2 = make_edit(WS_POPUP | WS_VISIBLE, L"", &r2);

        EXPECT_HR(S_OK, insert_obj(r2, &obj, 0, &site));
        count_pixels(e2, &red0, &white);
        EXPECT(red0 > 1000);
        EXPECT_HR(S_OK, IOleClientSite_OnShowWindow(site, TRUE));
        count_pixels(e2, &red, &white);
        EXPECT(red > red0 / 3 && red < red0 * 2 / 3);
        EXPECT_HR(S_OK, IOleClientSite_OnShowWindow(site, FALSE));
        count_pixels(e2, &red, &white);
        EXPECT(red == red0);
        IOleClientSite_Release(site);
        r2->lpVtbl->Release(r2);
        DestroyWindow(e2);
    }

    ITextFont_Release(font);
    ITextRange_Release(range);
    ITextSelection_Release(sel);
    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)reole);
    DestroyWindow(edit);
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
