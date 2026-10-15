/* mshtml ngt-recheck round (patch 3282): IDispatchEx::GetMemberProperties, pointer capture, createRangeCollection,
 * XMLHttpRequest.send bodies, IMarkupServices::ParseString(PARSE_ABSOLUTIFYIE40URLS), navigator.userProfile,
 * ITravelLogClient::LoadHistoryPosition, script onerror. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <dispex.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <stdio.h>
#include <stdlib.h>
#define INTERNET_MAX_URL_LENGTH 2084
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
static const CLSID CLSID_DOMDocument_ = {0x2933bf90,0x7b36,0x11d2,{0xb2,0x0e,0x00,0xc0,0x4f,0x98,0x3e,0x60}};
static const IID IID_IMarkupServices_ = {0x3050f4a0,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
static const IID IID_IHTMLSelectionObject2_ = {0x3050f7ec,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLSelectionObject2 IID_IHTMLSelectionObject2_
static const IID IID_ITravelLogClient_ = {0x241c033e,0xe659,0x43da,{0xaa,0x4d,0x40,0x86,0xdb,0xc4,0x75,0x8d}};
static const IID IID_IDispatchEx_ = {0xa6ef9860,0xc720,0x11d0,{0x93,0x37,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
/* the mingw headers lack these; vtables written from the documented method order */
typedef struct ITLC ITLC;
typedef struct ITLCVtbl {
    HRESULT (WINAPI *QueryInterface)(ITLC*, REFIID, void**);
    ULONG (WINAPI *AddRef)(ITLC*);
    ULONG (WINAPI *Release)(ITLC*);
    HRESULT (WINAPI *FindWindowByIndex)(ITLC*, DWORD, IUnknown**);
    HRESULT (WINAPI *GetWindowData)(ITLC*, IStream*, void*);
    HRESULT (WINAPI *LoadHistoryPosition)(ITLC*, LPWSTR, DWORD);
} ITLCVtbl;
struct ITLC { const ITLCVtbl *lpVtbl; };
typedef struct IMS IMS;
typedef struct IMSVtbl {
    HRESULT (WINAPI *QueryInterface)(IMS*, REFIID, void**);
    ULONG (WINAPI *AddRef)(IMS*);
    ULONG (WINAPI *Release)(IMS*);
    HRESULT (WINAPI *CreateMarkupPointer)(IMS*);
    HRESULT (WINAPI *CreateMarkupContainer)(IMS*);
    HRESULT (WINAPI *CreateElement)(IMS*);
    HRESULT (WINAPI *CloneElement)(IMS*);
    HRESULT (WINAPI *InsertElement)(IMS*);
    HRESULT (WINAPI *RemoveElement)(IMS*);
    HRESULT (WINAPI *Remove)(IMS*);
    HRESULT (WINAPI *Copy)(IMS*);
    HRESULT (WINAPI *Move)(IMS*);
    HRESULT (WINAPI *InsertText)(IMS*);
    HRESULT (WINAPI *ParseString)(IMS*, OLECHAR*, DWORD, IUnknown**, void*, void*);
} IMSVtbl;
struct IMS { const IMSVtbl *lpVtbl; };

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while (GetTickCount() < end) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } Sleep(10); }
}
static void write_file(const WCHAR *name, const char *text)
{
    DWORD w;
    HANDLE f = CreateFileW(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, text, strlen(text), &w, NULL);
    CloseHandle(f);
}
static HRESULT dcall(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *args, int n, VARIANT *ret)
{
    DISPID id, put = DISPID_PROPERTYPUT;
    DISPPARAMS dp = { args, NULL, n, 0 };
    OLECHAR *nm = (OLECHAR *)name;
    HRESULT hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &nm, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    if (flags & DISPATCH_PROPERTYPUT) { dp.rgdispidNamedArgs = &put; dp.cNamedArgs = 1; }
    if (ret) V_VT(ret) = VT_EMPTY;
    return IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, ret, NULL, NULL);
}
static HRESULT pget(IDispatch *d, const WCHAR *name, VARIANT *ret) { return dcall(d, name, DISPATCH_PROPERTYGET, NULL, 0, ret); }
static VARIANT vbstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vi4(int i) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = i; return v; }
static char sbuf[8][1024]; static int sidx;
static const char *vs(VARIANT *v)
{
    VARIANT s; char *b = sbuf[sidx++ & 7];
    V_VT(&s) = VT_EMPTY;
    if (V_VT(v) == VT_EMPTY) return "(empty)";
    if (FAILED(VariantChangeType(&s, v, 0, VT_BSTR)) || !V_BSTR(&s)) return "(?)";
    WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&s), -1, b, 1023, NULL, NULL);
    VariantClear(&s);
    return b;
}
static const char *pstr(IDispatch *d, const WCHAR *name)
{
    VARIANT v; HRESULT hr = pget(d, name, &v); const char *s;
    if (FAILED(hr)) { char *b = sbuf[sidx++ & 7]; sprintf(b, "ERR%08lx", hr); return b; }
    s = vs(&v); VariantClear(&v); return s;
}
static IDispatch *by_id(IHTMLDocument3 *doc3, const WCHAR *id)
{
    IHTMLElement *e = NULL; IDispatch *d = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e);
    SysFreeString(b);
    if (e) { IHTMLElement_QueryInterface(e, &IID_IDispatch, (void **)&d); IHTMLElement_Release(e); }
    return d;
}
static void run_script(IHTMLDocument2 *doc, const WCHAR *code)
{
    IHTMLWindow2 *win = NULL; VARIANT r; BSTR c = SysAllocString(code), l = SysAllocString(L"JScript");
    IHTMLDocument2_get_parentWindow(doc, &win);
    if (win) { HRESULT hr = IHTMLWindow2_execScript(win, c, l, &r); CHECK(hr == S_OK, "execScript %08lx for %ls", hr, code); IHTMLWindow2_Release(win); }
    SysFreeString(c); SysFreeString(l);
}
static IDispatch *window_disp(IHTMLDocument2 *doc)
{
    IHTMLWindow2 *win = NULL; IDispatch *d = NULL;
    IHTMLDocument2_get_parentWindow(doc, &win);
    if (win) { IHTMLWindow2_QueryInterface(win, &IID_IDispatch, (void **)&d); IHTMLWindow2_Release(win); }
    return d;
}

static void load(const WCHAR *file, IHTMLDocument2 **pdoc, IHTMLDocument3 **pdoc3, ULONG waitms)
{
    IPersistMoniker *pm; WCHAR path[MAX_PATH], url[INTERNET_MAX_URL_LENGTH]; DWORD size = INTERNET_MAX_URL_LENGTH; IMoniker *mk = NULL; HRESULT hr;
    GetFullPathNameW(file, MAX_PATH, path, NULL);
    UrlCreateFromPathW(path, url, &size, 0);
    CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)pdoc);
    IHTMLDocument2_QueryInterface(*pdoc, &IID_IHTMLDocument3, (void **)pdoc3);
    IHTMLDocument2_QueryInterface(*pdoc, &IID_IPersistMoniker, (void **)&pm);
    CreateURLMonikerEx(NULL, url, &mk, 0);
    hr = IPersistMoniker_Load(pm, FALSE, mk, NULL, STGM_READ);
    CHECK(hr == S_OK, "load %ls %08lx", file, hr);
    IMoniker_Release(mk); IPersistMoniker_Release(pm);
    pump(waitms);
}

static const char page[] =
    "<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=edge\"><title>rc</title></head><body>"
    "<div id=d style=\"width:10px\">div</div><a name=anc></a><p id=pp>some text to select</p>"
    "<div style=\"height:3000px\"></div></body></html>";

static void test_json(IHTMLDocument2 *doc)
{
    IDispatch *w = window_disp(doc);
    const char *j;
    run_script(doc, L"window.j1 = JSON.stringify(window.performance.timing.toJSON());"
                    L"window.j2 = JSON.stringify(window.performance.navigation.toJSON());"
                    L"window.j3 = JSON.stringify(window.performance.toJSON());"
                    L"window.t = typeof window.performance.timing.toJSON();"
                    L"window.k1 = Object.keys(window.performance.timing.toJSON()).length;"
                    L"window.k3 = Object.keys(window.performance.toJSON()).join(',');");
    j = pstr(w, L"j1");
    CHECK(j[0] == '{' && strstr(j, "\"navigationStart\":") && strstr(j, "\"loadEventEnd\":") && strstr(j, "\"domComplete\":"), "timing.toJSON: %s", j);
    CHECK(!strcmp(pstr(w, L"k1"), "21"), "timing.toJSON has 21 keys: %s", pstr(w, L"k1"));
    j = pstr(w, L"j2");
    CHECK(!strcmp(j, "{\"type\":0,\"redirectCount\":0}"), "navigation.toJSON: %s", j);
    j = pstr(w, L"j3");
    CHECK(strstr(j, "\"timing\":{") && strstr(j, "\"navigation\":{\"type\":0") , "performance.toJSON: %s", j);
    CHECK(!strcmp(pstr(w, L"k3"), "timing,navigation"), "performance.toJSON keys: %s", pstr(w, L"k3"));
    IDispatch_Release(w);
}

int main(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    write_file(L"cs.htm", page);
    load(L"cs.htm", &doc, &doc3, 2500);
    test_json(doc);
    IHTMLDocument3_Release(doc3); IHTMLDocument2_Release(doc);
    pump(200);
    CoUninitialize();
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
