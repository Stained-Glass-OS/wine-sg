#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <mshtmdid.h>
#include <docobj.h>
#include <objsafe.h>
#include <hlink.h>
#include <stdio.h>

static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
static const IID IID_IDisplayServices_ = {0x3050f69d,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
static const IID IID_IPersistHistory_ = {0x91a565c1,0xe38f,0x11d0,{0x94,0xbf,0x00,0xa0,0xc9,0x05,0x5c,0xbf}};
/* IPersistHistory (perhist.h): IPersist's GetClassID, then LoadHistory, SaveHistory, SetPositionCookie, GetPositionCookie */
typedef struct PH { const struct PHVtbl *lpVtbl; } PH;
struct PHVtbl {
    HRESULT (WINAPI *QueryInterface)(PH*, REFIID, void**); ULONG (WINAPI *AddRef)(PH*); ULONG (WINAPI *Release)(PH*);
    void *GetClassID, *LoadHistory, *SaveHistory;
    HRESULT (WINAPI *SetPositionCookie)(PH*, DWORD);
    HRESULT (WINAPI *GetPositionCookie)(PH*, DWORD*);
};
#undef IID_IHTMLDocument4
#undef IID_IDispatchEx
#define IID_IHTMLDocument4 IID_IHTMLDocument4_
#define IID_IDispatchEx IID_IDispatchEx_
static const IID IID_IHTMLDocument4_ = {0x3050f69a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
static const IID IID_IDispatchEx_ = {0xa6ef9860,0xc720,0x11d0,{0x93,0x37,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* IDisplayServices: only the two methods used, in vtable order after IUnknown */
typedef struct DS { const struct DSVtbl *lpVtbl; } DS;
struct DSVtbl {
    HRESULT (WINAPI *QueryInterface)(DS*, REFIID, void**); ULONG (WINAPI *AddRef)(DS*); ULONG (WINAPI *Release)(DS*);
    void *CreateDisplayPointer;
    HRESULT (WINAPI *TransformRect)(DS*, RECT*, int, int, IHTMLElement*);
    HRESULT (WINAPI *TransformPoint)(DS*, POINT*, int, int, IHTMLElement*);
    void *GetCaret, *GetComputedStyle;
    HRESULT (WINAPI *ScrollRectIntoView)(DS*, IHTMLElement*, RECT);
    HRESULT (WINAPI *HasFlowLayout)(DS*, IHTMLElement*, BOOL*);
};

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while (GetTickCount() < end) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } Sleep(10); }
}

int main(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IHTMLDocument4 *doc4; IPersistFile *pf; HRESULT hr;
    WCHAR path[MAX_PATH], path2[MAX_PATH]; HANDLE f; DWORD w, got; const char html[] = "<html><head></head><body><p id=a>hello</p></body></html>";
    VARIANT_BOOL vb; BSTR s; VARIANT v; IHTMLElement *body = NULL;

    CoInitialize(NULL);
    GetFullPathNameW(L"b3.htm", MAX_PATH, path, NULL);
    GetFullPathNameW(L"b3-saved.htm", MAX_PATH, path2, NULL);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, html, sizeof(html) - 1, &w, NULL); CloseHandle(f);
    DeleteFileW(path2);

    hr = CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&doc);
    CHECK(hr == S_OK, "doc %08lx", hr);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void **)&doc3);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument4, (void **)&doc4);
    IHTMLDocument2_QueryInterface(doc, &IID_IPersistFile, (void **)&pf);
    hr = IPersistFile_Load(pf, path, STGM_READ);
    CHECK(hr == S_OK, "load %08lx", hr);
    pump(2500);
    IHTMLDocument2_get_body(doc, &body);
    CHECK(body != NULL, "body");

    /* expando, charset, default charset, base url */
    CHECK(IHTMLDocument2_get_expando(doc, &vb) == S_OK && vb == VARIANT_TRUE, "expando default %d", vb);
    CHECK(IHTMLDocument2_put_expando(doc, VARIANT_FALSE) == S_OK, "put_expando");
    CHECK(IHTMLDocument2_get_expando(doc, &vb) == S_OK && vb == VARIANT_FALSE, "expando off %d", vb);
    IHTMLDocument2_put_expando(doc, VARIANT_TRUE);
    s = SysAllocString(L"utf-8");
    CHECK(IHTMLDocument2_put_charset(doc, s) == S_OK, "put_charset");
    SysFreeString(s);
    s = NULL; hr = IHTMLDocument2_get_charset(doc, &s);
    CHECK(hr == S_OK && s && !wcsicmp(s, L"utf-8"), "charset %ls", s ? s : L"(null)"); SysFreeString(s);
    s = SysAllocString(L"nonexistent-charset");
    CHECK(IHTMLDocument2_put_charset(doc, s) == E_INVALIDARG, "bad charset");
    SysFreeString(s);
    s = SysAllocString(L"windows-1252");
    CHECK(IHTMLDocument2_put_defaultCharset(doc, s) == S_OK, "put_defaultCharset"); SysFreeString(s);
    s = NULL; IHTMLDocument2_get_defaultCharset(doc, &s);
    CHECK(s && !wcsicmp(s, L"windows-1252"), "defaultCharset %ls", s ? s : L"(null)"); SysFreeString(s);
    s = SysAllocString(L"http://example.com/base/");
    CHECK(IHTMLDocument3_put_baseUrl(doc3, s) == S_OK, "put_baseUrl"); SysFreeString(s);
    s = NULL; IHTMLDocument3_get_baseUrl(doc3, &s);
    CHECK(s && !wcscmp(s, L"http://example.com/base/"), "baseUrl %ls", s ? s : L"(null)"); SysFreeString(s);

    /* commands */
    s = SysAllocString(L"Bold");
    { BSTR t = NULL; hr = IHTMLDocument2_queryCommandText(doc, s, &t); CHECK(hr == S_OK && t, "queryCommandText %08lx", hr); SysFreeString(t); }
    SysFreeString(s);
    s = SysAllocString(L"NoSuchCommand");
    hr = IHTMLDocument2_execCommandShowHelp(doc, s, &vb); CHECK(hr == S_OK && vb == VARIANT_FALSE, "ShowHelp unknown %08lx %d", hr, vb);
    SysFreeString(s);

    /* event object copy */
    {
        IHTMLEventObj *ev = NULL, *ev2 = NULL; VARIANT arg, rv, rv2;
        hr = IHTMLDocument4_createEventObject(doc4, NULL, &ev); CHECK(hr == S_OK && ev, "createEventObject %08lx", hr);
        V_VT(&rv) = VT_BOOL; V_BOOL(&rv) = VARIANT_FALSE;
        IHTMLEventObj_put_returnValue(ev, rv);
        V_VT(&arg) = VT_DISPATCH; V_DISPATCH(&arg) = (IDispatch*)ev;
        hr = IHTMLDocument4_createEventObject(doc4, &arg, &ev2); CHECK(hr == S_OK && ev2 && ev2 != ev, "copy %08lx", hr);
        V_VT(&rv2) = VT_EMPTY; IHTMLEventObj_get_returnValue(ev2, &rv2);
        CHECK(V_VT(&rv2) == VT_BOOL && V_BOOL(&rv2) == VARIANT_FALSE, "copied returnValue %d %d", V_VT(&rv2), V_BOOL(&rv2));
        V_VT(&arg) = VT_I4; V_I4(&arg) = 3;
        CHECK(IHTMLDocument4_createEventObject(doc4, &arg, &ev2) == E_INVALIDARG, "bad source");
        if (ev) IHTMLEventObj_Release(ev);
        if (ev2) IHTMLEventObj_Release(ev2);
    }

    /* stylesheets: a link in the head */
    {
        IHTMLStyleSheet *ss = NULL; IHTMLElementCollection *col = NULL; LONG len = 0; VARIANT tag;
        s = SysAllocString(L"http://127.0.0.1:1/x.css");
        hr = IHTMLDocument2_createStyleSheet(doc, s, -1, &ss); CHECK(hr == S_OK && ss, "createStyleSheet href %08lx", hr);
        SysFreeString(s); if (ss) IHTMLStyleSheet_Release(ss);
        s = SysAllocString(L"link");
        IHTMLDocument3_getElementsByTagName(doc3, s, &col); SysFreeString(s);
        if (col) { IHTMLElementCollection_get_length(col, &len); IHTMLElementCollection_Release(col); }
        CHECK(len == 1, "one link element in the document: %ld", len);
        (void)tag;
    }

    /* display services */
    {
        DS *ds = NULL; POINT pt = {10, 20}; BOOL flow = FALSE; RECT rc = {1, 2, 11, 22};
        hr = IHTMLDocument2_QueryInterface(doc, &IID_IDisplayServices_, (void **)&ds);
        CHECK(hr == S_OK && ds, "IDisplayServices %08lx", hr);
        if (ds) {
            CHECK(ds->lpVtbl->TransformPoint(ds, &pt, 3, 4, NULL) == S_OK && pt.x == 10 && pt.y == 20, "content to frame without scroll: %ld,%ld", pt.x, pt.y);
            CHECK(ds->lpVtbl->TransformPoint(ds, &pt, 9, 4, NULL) == E_INVALIDARG, "bad system");
            CHECK(ds->lpVtbl->TransformPoint(ds, &pt, 1, 4, NULL) == E_INVALIDARG, "parent needs an element");
            hr = ds->lpVtbl->TransformRect(ds, &rc, 3, 4, NULL); CHECK(hr == S_OK && rc.right - rc.left == 10, "rect size kept");
            hr = ds->lpVtbl->HasFlowLayout(ds, body, &flow); CHECK(hr == S_OK && flow, "body has flow layout %08lx", hr);
            CHECK(ds->lpVtbl->HasFlowLayout(ds, NULL, &flow) == E_INVALIDARG, "no element");
            ds->lpVtbl->Release(ds);
        }
    }

    /* persist: size, position, hlink, moniker save */
    {
        IPersistStreamInit *psi; PH *ph; IHlinkTarget *ht; IPersistMoniker *pm; IStream *st; ULARGE_INTEGER sz; STATSTG stat;
        DWORD cookie = 99; IMoniker *mon = NULL; LPWSTR name = NULL;
        IHTMLDocument2_QueryInterface(doc, &IID_IPersistStreamInit, (void **)&psi);
        hr = IPersistStreamInit_GetSizeMax(psi, &sz); CHECK(hr == S_OK && sz.QuadPart > 20, "GetSizeMax %08lx %lu", hr, (ULONG)sz.QuadPart);
        CreateStreamOnHGlobal(NULL, TRUE, &st);
        IPersistStreamInit_Save(psi, st, FALSE);
        IStream_Stat(st, &stat, STATFLAG_NONAME);
        CHECK(stat.cbSize.QuadPart == sz.QuadPart, "GetSizeMax equals what Save wrote: %lu %lu", (ULONG)sz.QuadPart, (ULONG)stat.cbSize.QuadPart);
        IStream_Release(st); IPersistStreamInit_Release(psi);

        IHTMLDocument2_QueryInterface(doc, &IID_IPersistHistory_, (void **)&ph);
        hr = ph->lpVtbl->GetPositionCookie(ph, &cookie); CHECK(hr == S_OK && cookie == 0, "position cookie %08lx %lu", hr, cookie);
        hr = ph->lpVtbl->SetPositionCookie(ph, 0); CHECK(hr == S_OK, "SetPositionCookie %08lx", hr);
        ph->lpVtbl->Release(ph);

        IHTMLDocument2_QueryInterface(doc, &IID_IHlinkTarget, (void **)&ht);
        hr = IHlinkTarget_GetMoniker(ht, L"mark", 0, &mon);
        CHECK(hr == S_OK && mon, "GetMoniker %08lx", hr);
        if (mon) { hr = IMoniker_GetDisplayName(mon, NULL, NULL, &name); CHECK(hr == S_OK && name && wcsstr(name, L"#mark"), "moniker %ls", name ? name : L"(null)"); CoTaskMemFree(name); IMoniker_Release(mon); }
        name = NULL; hr = IHlinkTarget_GetFriendlyName(ht, L"mark", &name);
        CHECK(hr == S_OK && name && wcsstr(name, L"mark"), "friendly %ls", name ? name : L"(null)"); CoTaskMemFree(name);
        { IHlinkBrowseContext *bc = (IHlinkBrowseContext *)0x1; hr = IHlinkTarget_GetBrowseContext(ht, &bc); CHECK(hr == S_OK && bc == NULL, "browse context %08lx %p", hr, bc); }
        IHlinkTarget_Release(ht);

        IHTMLDocument2_QueryInterface(doc, &IID_IPersistMoniker, (void **)&pm);
        CreateFileMoniker(path2, &mon);
        hr = IPersistMoniker_Save(pm, mon, NULL, FALSE);
        CHECK(hr == S_OK, "PersistMoniker::Save to a file moniker %08lx", hr);
        CHECK(GetFileAttributesW(path2) != INVALID_FILE_ATTRIBUTES, "the file was written");
        IMoniker_Release(mon);
        { IMoniker *http; CreateURLMoniker(NULL, L"http://127.0.0.1:1/x", &http); CHECK(IPersistMoniker_Save(pm, http, NULL, FALSE) == E_FAIL, "only a file can be saved to"); IMoniker_Release(http); }
        IPersistMoniker_Release(pm);
    }

    /* view object */
    {
        IViewObject2 *vo; DWORD status = 0, frz = 0, frz2 = 0; DWORD hit = 9; RECT r = {0, 0, 100, 100}; POINT p1 = {50, 50}, p2 = {300, 300};
        LOGPALETTE *pal = (LOGPALETTE *)1; SIZEL sz; IAdviseSink *sink = (IAdviseSink *)1; DWORD asp = 7, advf = 7;
        IHTMLDocument2_QueryInterface(doc, &IID_IViewObject2, (void **)&vo);
        CHECK(vo != NULL, "IViewObject2");
        if (vo) {
            IViewObjectEx *vx; IViewObject2_QueryInterface(vo, &IID_IViewObjectEx, (void **)&vx);
            hr = IViewObjectEx_GetViewStatus(vx, &status); CHECK(hr == S_OK && (status & VIEWSTATUS_OPAQUE), "view status %08lx %lx", hr, status);
            hr = IViewObjectEx_QueryHitPoint(vx, DVASPECT_CONTENT, &r, p1, 0, &hit); CHECK(hr == S_OK && hit == HITRESULT_HIT, "hit inside %08lx %lu", hr, hit);
            hr = IViewObjectEx_QueryHitPoint(vx, DVASPECT_CONTENT, &r, p2, 0, &hit); CHECK(hr == S_OK && hit == HITRESULT_OUTSIDE, "hit outside %08lx %lu", hr, hit);
            hr = IViewObjectEx_GetColorSet(vx, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal); CHECK(hr == S_FALSE && pal == NULL, "color set %08lx", hr);
            hr = IViewObjectEx_Freeze(vx, DVASPECT_CONTENT, -1, NULL, &frz); CHECK(hr == S_OK && frz, "Freeze %08lx", hr);
            hr = IViewObjectEx_Freeze(vx, DVASPECT_CONTENT, -1, NULL, &frz2); CHECK(hr == VIEW_S_ALREADY_FROZEN && frz2 == frz, "Freeze again %08lx", hr);
            CHECK(IViewObjectEx_Unfreeze(vx, frz + 100) == E_INVALIDARG, "Unfreeze a stranger");
            CHECK(IViewObjectEx_Unfreeze(vx, frz) == S_OK, "Unfreeze");
            hr = IViewObjectEx_GetAdvise(vx, &asp, &advf, &sink); CHECK(hr == S_OK && asp == 0 && sink == NULL, "GetAdvise %08lx", hr);
            hr = IViewObjectEx_GetExtent(vx, DVASPECT_CONTENT, -1, NULL, &sz); CHECK(hr == S_OK, "GetExtent %08lx", hr);
            IViewObjectEx_Release(vx); IViewObject2_Release(vo);
        }
    }

    /* ole object, control, safety, documents */
    {
        IOleObject *oo; IOleControl *oc; IObjectSafety *os; IOleDocument *od; IMoniker *m1, *m2 = NULL; CONTROLINFO ci = { sizeof(ci) };
        DWORD sup = 0, en = 0; IOleDocumentView *view = NULL; IEnumOleDocumentViews *en2 = NULL;
        IHTMLDocument2_QueryInterface(doc, &IID_IOleObject, (void **)&oo);
        CreateFileMoniker(path, &m1);
        CHECK(IOleObject_SetMoniker(oo, OLEWHICHMK_OBJFULL, m1) == S_OK, "SetMoniker");
        hr = IOleObject_GetMoniker(oo, OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_OBJFULL, &m2);
        CHECK(hr == S_OK && m2 == m1, "GetMoniker gives it back %08lx", hr);
        if (m2) IMoniker_Release(m2);
        CHECK(IOleObject_SetMoniker(oo, 9, m1) == E_INVALIDARG, "bad moniker kind");
        IMoniker_Release(m1); IOleObject_Release(oo);

        IHTMLDocument2_QueryInterface(doc, &IID_IOleControl, (void **)&oc);
        hr = IOleControl_GetControlInfo(oc, &ci); CHECK(hr == S_OK && ci.cAccel == 0, "GetControlInfo %08lx", hr);
        CHECK(IOleControl_FreezeEvents(oc, TRUE) == S_OK && IOleControl_FreezeEvents(oc, FALSE) == S_OK, "FreezeEvents");
        IOleControl_Release(oc);

        IHTMLDocument2_QueryInterface(doc, &IID_IObjectSafety, (void **)&os);
        hr = IObjectSafety_GetInterfaceSafetyOptions(os, &IID_IDispatchEx, &sup, &en);
        CHECK(hr == S_OK && (sup & INTERFACESAFE_FOR_UNTRUSTED_CALLER) && en == 0, "safety get %08lx %lx %lx", hr, sup, en);
        hr = IObjectSafety_SetInterfaceSafetyOptions(os, &IID_IDispatchEx, INTERFACESAFE_FOR_UNTRUSTED_DATA, INTERFACESAFE_FOR_UNTRUSTED_DATA);
        CHECK(hr == S_OK, "safety set %08lx", hr);
        IObjectSafety_GetInterfaceSafetyOptions(os, &IID_IDispatchEx, &sup, &en);
        CHECK(en == INTERFACESAFE_FOR_UNTRUSTED_DATA, "safety enabled %lx", en);
        CHECK(IObjectSafety_SetInterfaceSafetyOptions(os, &IID_IDispatchEx, 0x100, 0x100) == E_FAIL, "an option it does not have");
        CHECK(IObjectSafety_GetInterfaceSafetyOptions(os, &IID_IHTMLDocument2, &sup, &en) == E_NOINTERFACE, "an interface it has no options for");
        IObjectSafety_Release(os);

        IHTMLDocument2_QueryInterface(doc, &IID_IOleDocument, (void **)&od);
        hr = IOleDocument_EnumViews(od, &en2, &view); CHECK(hr == S_OK && view && !en2, "EnumViews with a view %08lx", hr);
        if (view) IOleDocumentView_Release(view);
        view = NULL; hr = IOleDocument_EnumViews(od, &en2, NULL);
        CHECK(hr == S_OK && en2, "EnumViews enumerator %08lx", hr);
        if (en2) {
            ULONG n = 0; hr = IEnumOleDocumentViews_Next(en2, 1, &view, &n); CHECK(hr == S_OK && n == 1 && view, "enumerator Next");
            if (view) IOleDocumentView_Release(view);
            hr = IEnumOleDocumentViews_Next(en2, 1, &view, &n); CHECK(hr == S_FALSE && n == 0, "enumerator ends");
            IEnumOleDocumentViews_Release(en2);
        }
        CHECK(IOleDocument_EnumViews(od, NULL, NULL) == E_INVALIDARG, "EnumViews with nothing asked");
        IOleDocument_Release(od);
    }

    if (body) IHTMLElement_Release(body);
    IPersistFile_Release(pf); IHTMLDocument4_Release(doc4); IHTMLDocument3_Release(doc3); IHTMLDocument2_Release(doc);
    CoUninitialize();
    (void)got; (void)v;
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
