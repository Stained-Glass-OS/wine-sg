/* mshtml batch 4 (patch 3192): text ranges, form/select item+tags, URL file/mime/protocol props, frame props,
 * location puts, comment, attribute nodes, current style, table, style element, and the HTML dialogs. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <shlwapi.h>
#include <stdio.h>
#define INTERNET_MAX_URL_LENGTH 2084
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
#define DEFIID(n,a) static const IID n##_ = {0x3050##a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}}
DEFIID(IID_IHTMLCurrentStyle4, f33b); DEFIID(IID_IHTMLCurrentStyle2, f658); DEFIID(IID_IHTMLDOMAttribute2, f810); DEFIID(IID_IHTMLElement4, f80f);
#define IID_IHTMLCurrentStyle4 IID_IHTMLCurrentStyle4_
#define IID_IHTMLCurrentStyle2 IID_IHTMLCurrentStyle2_
#define IID_IHTMLDOMAttribute2 IID_IHTMLDOMAttribute2_
#define IID_IHTMLElement4 IID_IHTMLElement4_
static HRESULT (WINAPI *pShowHTMLDialog)(HWND, IMoniker *, VARIANT *, WCHAR *, VARIANT *);
static HRESULT (WINAPI *pShowModelessHTMLDialog)(HWND, IMoniker *, VARIANT *, VARIANT *, IHTMLWindow2 **);
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

static IMoniker *moniker_of(const WCHAR *file)
{
    WCHAR full[MAX_PATH], url[INTERNET_MAX_URL_LENGTH];
    DWORD size = INTERNET_MAX_URL_LENGTH;
    IMoniker *mk = NULL;
    GetFullPathNameW(file, MAX_PATH, full, NULL);
    UrlCreateFromPathW(full, url, &size, 0);
    CreateURLMonikerEx(NULL, url, &mk, 0);
    return mk;
}

/* ---- IDispatch helpers ---- */
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
static HRESULT pput(IDispatch *d, const WCHAR *name, VARIANT v) { return dcall(d, name, DISPATCH_PROPERTYPUT, &v, 1, NULL); }
static VARIANT vbstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vi4(int i) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = i; return v; }
static VARIANT vbool(BOOL b) { VARIANT v; V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }

static char sbuf[8][256];
static int sidx;
static const char *vs(VARIANT *v)
{
    VARIANT s; char *b = sbuf[sidx++ & 7];
    V_VT(&s) = VT_EMPTY;
    if (V_VT(v) == VT_EMPTY) return "(empty)";
    if (FAILED(VariantChangeType(&s, v, 0, VT_BSTR))) return "(?)";
    WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&s), -1, b, 255, NULL, NULL);
    VariantClear(&s);
    return b;
}
static const char *pstr(IDispatch *d, const WCHAR *name)
{
    VARIANT v; HRESULT hr = pget(d, name, &v);
    const char *s;
    if (FAILED(hr)) { char *b = sbuf[sidx++ & 7]; sprintf(b, "ERR%08lx", hr); return b; }
    s = vs(&v);
    VariantClear(&v);
    return s;
}
static IDispatch *by_id(IHTMLDocument3 *doc3, const WCHAR *id)
{
    IHTMLElement *e = NULL; IDispatch *d = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e);
    SysFreeString(b);
    if (e) { IHTMLElement_QueryInterface(e, &IID_IDispatch, (void **)&d); IHTMLElement_Release(e); }
    return d;
}
static int coll_len(IDispatch *d)
{
    VARIANT v; int n = -1;
    if (d && SUCCEEDED(pget(d, L"length", &v))) n = V_I4(&v);
    return n;
}

static void test_dialogs(void)
{
    IMoniker *mk;
    VARIANT in, out, opt;
    HRESULT hr;
    HMODULE mshtml = LoadLibraryA("mshtml.dll");
    pShowHTMLDialog = (void *)GetProcAddress(mshtml, "ShowHTMLDialog");
    pShowModelessHTMLDialog = (void *)GetProcAddress(mshtml, "ShowModelessHTMLDialog");
    write_file(L"dlg1.htm", "<html><head><title>Dlg1</title></head><body onload=\"window.returnValue = 'ret:' + window.dialogArguments; window.close();\"></body></html>");
    write_file(L"dlg9.htm", "<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=9\"><title>Dlg9</title></head><body onload=\"window.returnValue = 'ie9:' + window.dialogArguments; window.close();\"></body></html>");
    write_file(L"dlgn.htm", "<html><head><title>DlgN</title></head><body onload=\"window.returnValue = window.dialogArguments + 1; window.close();\"></body></html>");
    write_file(L"dlg2.htm", "<html><head><title>Dlg2 title</title></head><body>modeless</body></html>");
    write_file(L"host.htm", "<html><head><title>host</title></head><body onload=\"window.setTimeout(function(){ var r = window.showModalDialog('dlg1.htm', 'viaScript', 'dialogWidth:200px;dialogHeight:100px'); document.title = 'got:' + r; }, 50);\"></body></html>");

    V_VT(&in) = VT_BSTR; V_BSTR(&in) = SysAllocString(L"hi");
    V_VT(&out) = VT_EMPTY;
    mk = moniker_of(L"dlg1.htm");
    hr = pShowHTMLDialog(NULL, mk, &in, (WCHAR *)L"dialogWidth:300px;dialogHeight:200px", &out);
    CHECK(hr == S_OK && V_VT(&out) == VT_BSTR && !strcmp(vs(&out), "ret:hi"), "modal %08lx %s", hr, vs(&out));
    IMoniker_Release(mk); VariantClear(&out);

    mk = moniker_of(L"dlg9.htm");
    hr = pShowHTMLDialog(NULL, mk, &in, NULL, &out);
    CHECK(hr == S_OK && !strcmp(vs(&out), "ie9:hi"), "modal IE9 %08lx %s", hr, vs(&out));
    IMoniker_Release(mk); VariantClear(&out); VariantClear(&in);

    in = vi4(41);
    mk = moniker_of(L"dlgn.htm");
    hr = pShowHTMLDialog(NULL, mk, &in, NULL, &out);
    CHECK(hr == S_OK && !strcmp(vs(&out), "42"), "modal number %08lx %s", hr, vs(&out));
    IMoniker_Release(mk); VariantClear(&out);

    {
        IHTMLWindow2 *win = NULL; IHTMLDocument2 *doc = NULL; BSTR title = NULL; HWND dlg = NULL; RECT r; int tries;
        opt = vbstr(L"dialogWidth:320px; dialogHeight:210px; resizable:yes");
        V_VT(&in) = VT_EMPTY;
        mk = moniker_of(L"dlg2.htm");
        hr = pShowModelessHTMLDialog(NULL, mk, &in, &opt, &win);
        CHECK(hr == S_OK && win, "modeless %08lx", hr);
        IMoniker_Release(mk); VariantClear(&opt);
        for (tries = 0; tries < 50; tries++) {
            WCHAR t[64];
            pump(100);
            dlg = FindWindowW(L"Internet Explorer_TridentDlgFrame", NULL);
            if (dlg) { GetWindowTextW(dlg, t, 64); if (!wcscmp(t, L"Dlg2 title")) break; }
        }
        CHECK(dlg != NULL, "dialog window");
        if (dlg) { GetWindowRect(dlg, &r); CHECK(r.right - r.left == 320 && r.bottom - r.top == 210, "size %ldx%ld", r.right - r.left, r.bottom - r.top); }
        if (win) {
            hr = IHTMLWindow2_get_document(win, &doc);
            if (doc) { IHTMLDocument2_get_title(doc, &title); CHECK(title && !wcscmp(title, L"Dlg2 title"), "doc title"); SysFreeString(title); IHTMLDocument2_Release(doc); }
            /* location puts navigate the dialog document with one part replaced */
            {
                IHTMLLocation *loc = NULL; BSTR u = NULL, q; int t;
                IHTMLWindow2_get_location(win, &loc);
                q = SysAllocString(L"?x=1");
                hr = loc ? IHTMLLocation_put_search(loc, q) : E_FAIL; CHECK(hr == S_OK, "location put search %08lx", hr);
                SysFreeString(q);
                for (t = 0; t < 30; t++) {
                    pump(100);
                    IHTMLWindow2_get_document(win, &doc);
                    if (doc) { IHTMLDocument2_get_URL(doc, &u); IHTMLDocument2_Release(doc); doc = NULL; }
                    if (u && wcsstr(u, L"dlg2.htm?x=1")) break;
                    SysFreeString(u); u = NULL;
                }
                CHECK(u && wcsstr(u, L"dlg2.htm?x=1"), "URL after search put %ls", u ? u : L"(none)");
                SysFreeString(u);
                if (loc) IHTMLLocation_Release(loc);
            }
            hr = IHTMLWindow2_close(win);
            pump(500);
            CHECK(hr == S_OK && FindWindowW(L"Internet Explorer_TridentDlgFrame", NULL) == NULL, "close %08lx", hr);
            IHTMLWindow2_Release(win);
        }
    }

    /* mshta.exe runs an application file in a window of its own, until its page closes it */
    {
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi; WCHAR cmd[MAX_PATH * 2]; HWND app = NULL; int tries; DWORD code = 99, w = 0;
        write_file(L"app.hta", "<html><head><title>HTA app</title></head><body onload=\"window.setTimeout(function(){ window.close(); }, 1500);\">hta</body></html>");
        {
            WCHAR full[MAX_PATH]; GetFullPathNameW(L"app.hta", MAX_PATH, full, NULL);
            swprintf(cmd, ARRAY_SIZE(cmd), L"mshta.exe \"%ls\"", full);
        }
        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) CHECK(0, "start mshta %lu", GetLastError());
        else {
            for (tries = 0; tries < 60 && !app; tries++) { Sleep(100); app = FindWindowW(L"Internet Explorer_TridentDlgFrame", NULL); }
            CHECK(app != NULL, "mshta window");
            w = WaitForSingleObject(pi.hProcess, 25000);
            CHECK(w == WAIT_OBJECT_0, "mshta exits when the page closes");
            if (w != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 1);
            GetExitCodeProcess(pi.hProcess, &code);
            CHECK(code == 0, "mshta exit code %lu", code);
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        }
    }

    /* showModalDialog called by a script of a page */
    {
        IHTMLDocument2 *doc; IPersistMoniker *pm; int tries; BSTR title = NULL; BOOL got = FALSE;
        CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&doc);
        IHTMLDocument2_QueryInterface(doc, &IID_IPersistMoniker, (void **)&pm);
        mk = moniker_of(L"host.htm");
        hr = IPersistMoniker_Load(pm, FALSE, mk, NULL, STGM_READ);
        CHECK(hr == S_OK, "host load %08lx", hr);
        for (tries = 0; tries < 100 && !got; tries++) {
            pump(100);
            if (SUCCEEDED(IHTMLDocument2_get_title(doc, &title)) && title) { got = !wcsncmp(title, L"got:", 4); if (!got) SysFreeString(title), title = NULL; }
        }
        CHECK(got && !wcscmp(title, L"got:ret:viaScript"), "showModalDialog from script: %ls", title ? title : L"(none)");
        SysFreeString(title);
        IMoniker_Release(mk); IPersistMoniker_Release(pm); IHTMLDocument2_Release(doc);
    }
}

static const char page[] =
    "<html><head><title>b4</title><meta id=m name=n content=c><style id=st type=\"text/css\">p{color:red}</style></head><body>"
    "<form id=f><input id=i1 type=text name=n1><input id=cb type=checkbox name=cb1><textarea id=ta name=tx>hello text</textarea>"
    "<button id=bt name=b1>Go</button><select id=sel name=s1><option id=o1>A</option><option id=o2>B</option></select></form>"
    "<a id=a1 href=\"file:///C:/b4img.gif\">x</a><img id=im src=\"file:///C:/b4img.gif\">"
    "<table id=tb><tr id=r0><td>1</td><td>2</td></tr><tr id=r1><td>3</td></tr><tr id=r2><td>4</td></tr></table>"
    "<iframe id=fr src=\"about:blank\"></iframe><object id=ob></object><!--cmt--><div id=d style=\"display:block\">d</div><span id=sp>s</span>"
    "</body></html>";

static void test_page(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IPersistFile *pf; HRESULT hr;
    WCHAR path[MAX_PATH]; VARIANT v, r; IDispatch *e, *e2; BSTR s; int i;

    write_file(L"b4img.gif", "0123456789");
    write_file(L"b4.htm", page);
    GetFullPathNameW(L"b4.htm", MAX_PATH, path, NULL);
    CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&doc);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void **)&doc3);
    IHTMLDocument2_QueryInterface(doc, &IID_IPersistFile, (void **)&pf);
    hr = IPersistFile_Load(pf, path, STGM_READ);
    CHECK(hr == S_OK, "load %08lx", hr);
    pump(2500);

    /* createTextRange on form controls */
    {
        const WCHAR *ids[] = { L"i1", L"ta", L"bt" };
        const char *texts[] = { "", "hello text", "Go" };
        for (i = 0; i < 3; i++) {
            IHTMLTxtRange *range = NULL; BSTR t = NULL;
            e = by_id(doc3, ids[i]);
            r.vt = VT_EMPTY;
            hr = dcall(e, L"createTextRange", DISPATCH_METHOD, NULL, 0, &r);
            CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "createTextRange %ls %08lx", ids[i], hr);
            if (hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r)) {
                IDispatch_QueryInterface(V_DISPATCH(&r), &IID_IHTMLTxtRange, (void **)&range);
                if (range) { IHTMLTxtRange_get_text(range, &t); CHECK((t ? !strcmp(vs(&(VARIANT){.vt=VT_BSTR,.bstrVal=t}), texts[i]) : !*texts[i]), "range text %ls '%ls'", ids[i], t ? t : L""); SysFreeString(t); IHTMLTxtRange_Release(range); }
                VariantClear(&r);
            }
            IDispatch_Release(e);
        }
    }

    /* status attribute, indeterminate, select */
    e = by_id(doc3, L"i1");
    hr = pput(e, L"status", vbool(TRUE)); CHECK(hr == S_OK, "input put status %08lx", hr);
    CHECK(!strcmp(pstr(e, L"status"), "-1"), "input status '%s'", pstr(e, L"status"));
    IDispatch_Release(e);
    e = by_id(doc3, L"ta");
    hr = pput(e, L"status", vbstr(L"on")); CHECK(hr == S_OK, "textarea put status %08lx", hr);
    CHECK(!strcmp(pstr(e, L"status"), "on"), "textarea status '%s'", pstr(e, L"status"));
    hr = dcall(e, L"select", DISPATCH_METHOD, NULL, 0, NULL); CHECK(hr == S_OK, "textarea select %08lx", hr);
    IDispatch_Release(e);
    e = by_id(doc3, L"bt");
    hr = pput(e, L"status", vbstr(L"hot")); CHECK(hr == S_OK, "button put status %08lx", hr);
    CHECK(!strcmp(pstr(e, L"status"), "hot"), "button status '%s'", pstr(e, L"status"));
    IDispatch_Release(e);
    e = by_id(doc3, L"cb");
    CHECK(!strcmp(pstr(e, L"indeterminate"), "0"), "indeterminate default '%s'", pstr(e, L"indeterminate"));
    hr = pput(e, L"indeterminate", vbool(TRUE)); CHECK(hr == S_OK, "put indeterminate %08lx", hr);
    CHECK(!strcmp(pstr(e, L"indeterminate"), "-1") || !strcmp(pstr(e, L"indeterminate"), "True"), "indeterminate set '%s'", pstr(e, L"indeterminate"));
    IDispatch_Release(e);

    /* meta */
    e = by_id(doc3, L"m");
    pput(e, L"content", vbstr(L"c2")); pput(e, L"name", vbstr(L"n2")); pput(e, L"httpEquiv", vbstr(L"refresh"));
    CHECK(!strcmp(pstr(e, L"content"), "c2"), "meta content '%s'", pstr(e, L"content"));
    CHECK(!strcmp(pstr(e, L"name"), "n2"), "meta name '%s'", pstr(e, L"name"));
    CHECK(!strcmp(pstr(e, L"httpEquiv"), "refresh"), "meta httpEquiv '%s'", pstr(e, L"httpEquiv"));
    IDispatch_Release(e);

    /* form and select: item by name, tags */
    e = by_id(doc3, L"f");
    { VARIANT a = vbstr(L"n1"); r.vt = VT_EMPTY; hr = dcall(e, L"item", DISPATCH_METHOD, &a, 1, &r);
      CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "form.item('n1') %08lx vt %d", hr, V_VT(&r));
      if (hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r)) CHECK(!strcmp(pstr(V_DISPATCH(&r), L"id"), "i1"), "form.item id '%s'", pstr(V_DISPATCH(&r), L"id"));
      VariantClear(&r); VariantClear(&a);
      a = vbstr(L"input"); r.vt = VT_EMPTY; hr = dcall(e, L"tags", DISPATCH_METHOD, &a, 1, &r);
      CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && coll_len(V_DISPATCH(&r)) == 2, "form.tags('input') %08lx len %d", hr, V_VT(&r) == VT_DISPATCH ? coll_len(V_DISPATCH(&r)) : -2);
      VariantClear(&r); VariantClear(&a); }
    IDispatch_Release(e);
    e = by_id(doc3, L"sel");
    { VARIANT a = vbstr(L"o2"); r.vt = VT_EMPTY; hr = dcall(e, L"item", DISPATCH_METHOD, &a, 1, &r);
      CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "select.item('o2') %08lx", hr);
      if (hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r)) CHECK(!strcmp(pstr(V_DISPATCH(&r), L"text"), "B"), "select.item text '%s'", pstr(V_DISPATCH(&r), L"text"));
      VariantClear(&r); VariantClear(&a);
      a = vbstr(L"option"); r.vt = VT_EMPTY; hr = dcall(e, L"tags", DISPATCH_METHOD, &a, 1, &r);
      CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && coll_len(V_DISPATCH(&r)) == 2, "select.tags('option') %08lx", hr);
      VariantClear(&r); VariantClear(&a); }
    IDispatch_Release(e);

    /* URL based properties of images and anchors */
    e = by_id(doc3, L"im");
    CHECK(!strcmp(pstr(e, L"fileSize"), "10"), "img fileSize '%s'", pstr(e, L"fileSize"));
    CHECK(!strcmp(pstr(e, L"nameProp"), "b4img.gif"), "img nameProp '%s'", pstr(e, L"nameProp"));
    CHECK(!strcmp(pstr(e, L"protocol"), "File Protocol"), "img protocol '%s'", pstr(e, L"protocol"));
    CHECK(*pstr(e, L"fileCreatedDate") && *pstr(e, L"fileModifiedDate") && *pstr(e, L"fileUpdatedDate"), "img file dates '%s' '%s' '%s'", pstr(e, L"fileCreatedDate"), pstr(e, L"fileModifiedDate"), pstr(e, L"fileUpdatedDate"));
    CHECK(strstr(pstr(e, L"href"), "b4img.gif") != NULL, "img href '%s'", pstr(e, L"href"));
    printf("INFO img mimeType '%s'\n", pstr(e, L"mimeType"));
    IDispatch_Release(e);
    e = by_id(doc3, L"a1");
    CHECK(!strcmp(pstr(e, L"nameProp"), "b4img.gif"), "anchor nameProp '%s'", pstr(e, L"nameProp"));
    CHECK(!strcmp(pstr(e, L"protocolLong"), "File Protocol"), "anchor protocolLong '%s'", pstr(e, L"protocolLong"));
    hr = pput(e, L"Methods", vbstr(L"GET,POST")); CHECK(hr == S_OK, "put Methods %08lx", hr);
    CHECK(!strcmp(pstr(e, L"Methods"), "GET,POST"), "anchor Methods '%s'", pstr(e, L"Methods"));
    printf("INFO anchor mimeType '%s'\n", pstr(e, L"mimeType"));
    IDispatch_Release(e);

    /* frame and object */
    e = by_id(doc3, L"fr");
    hr = pput(e, L"allowTransparency", vbool(FALSE)); CHECK(hr == S_OK, "put allowTransparency %08lx", hr);
    CHECK(!strcmp(pstr(e, L"allowTransparency"), "0") || !strcmp(pstr(e, L"allowTransparency"), "False"), "allowTransparency off '%s'", pstr(e, L"allowTransparency"));
    pput(e, L"allowTransparency", vbool(TRUE));
    CHECK(!strcmp(pstr(e, L"allowTransparency"), "-1") || !strcmp(pstr(e, L"allowTransparency"), "True"), "allowTransparency on '%s'", pstr(e, L"allowTransparency"));
    hr = pput(e, L"marginWidth", vi4(7)); CHECK(hr == S_OK, "put marginWidth number %08lx", hr);
    CHECK(!strcmp(pstr(e, L"marginWidth"), "7"), "marginWidth '%s'", pstr(e, L"marginWidth"));
    {
        IHTMLFrameBase2 *fb2 = NULL; UINT n = 99;
        IDispatch_QueryInterface(e, &IID_IHTMLFrameBase2, (void **)&fb2);
        CHECK(fb2 != NULL, "IHTMLFrameBase2");
        if (fb2) { hr = IHTMLFrameBase2_GetTypeInfoCount(fb2, &n); CHECK(hr == S_OK && n == 1, "FrameBase2 GetTypeInfoCount %08lx %u", hr, n); IHTMLFrameBase2_Release(fb2); }
    }
    IDispatch_Release(e);
    e = by_id(doc3, L"ob");
    hr = pput(e, L"width", vbstr(L"50")); CHECK(hr == S_OK, "object put width string %08lx", hr);
    hr = pput(e, L"height", vi4(40)); CHECK(hr == S_OK, "object put height %08lx", hr);
    CHECK(!strcmp(pstr(e, L"width"), "50"), "object width '%s'", pstr(e, L"width"));
    { VARIANT d8; V_VT(&d8) = VT_R8; V_R8(&d8) = 60; hr = pput(e, L"width", d8); CHECK(hr == S_OK, "object put width double %08lx", hr); }
    CHECK(!strcmp(pstr(e, L"width"), "60"), "object width double '%s'", pstr(e, L"width"));
    IDispatch_Release(e);

    /* comment */
    {
        IHTMLDocument2 *d2 = doc; IHTMLElementCollection *all = NULL; IDispatch *cm = NULL; LONG len = 0;
        IHTMLDocument2_get_all(d2, &all);
        IHTMLElementCollection_get_length(all, &len);
        for (i = 0; i < len; i++) {
            VARIANT nm, idx; IDispatch *it = NULL; VARIANT tn;
            nm = vi4(i); V_VT(&idx) = VT_EMPTY;
            IHTMLElementCollection_item(all, nm, idx, &it);
            if (it && SUCCEEDED(pget(it, L"tagName", &tn)) && tn.vt == VT_BSTR && !wcscmp(V_BSTR(&tn), L"!")) { cm = it; VariantClear(&tn); break; }
            if (it) IDispatch_Release(it);
        }
        CHECK(cm != NULL, "comment element");
        if (cm) {
            hr = pput(cm, L"text", vbstr(L"<!--new text-->")); CHECK(hr == S_OK, "comment put text %08lx", hr);
            CHECK(!strcmp(pstr(cm, L"text"), "<!--new text-->"), "comment text '%s'", pstr(cm, L"text"));
            hr = pput(cm, L"atomic", vi4(1)); CHECK(hr == S_OK, "comment put atomic %08lx", hr);
            CHECK(!strcmp(pstr(cm, L"atomic"), "1"), "comment atomic '%s'", pstr(cm, L"atomic"));
            IDispatch_Release(cm);
        }
        IHTMLElementCollection_Release(all);
    }

    /* attribute nodes: children, clone */
    e = by_id(doc3, L"d");
    {
        IHTMLElement4 *e4 = NULL; IHTMLDOMAttribute *attr = NULL; IHTMLDOMAttribute2 *a2 = NULL; BSTR nm = SysAllocString(L"id");
        IDispatch_QueryInterface(e, &IID_IHTMLElement4, (void **)&e4);
        if (e4) IHTMLElement4_getAttributeNode(e4, nm, &attr);
        SysFreeString(nm);
        CHECK(attr != NULL, "getAttributeNode");
        if (attr) IHTMLDOMAttribute_QueryInterface(attr, &IID_IHTMLDOMAttribute2, (void **)&a2);
        if (a2) {
            VARIANT_BOOL has = VARIANT_FALSE; IDispatch *kids = NULL; IHTMLDOMNode *first = NULL, *last = NULL; IHTMLDOMAttribute *clone = NULL; BSTR cn = NULL;
            hr = IHTMLDOMAttribute2_hasChildNodes(a2, &has); CHECK(hr == S_OK && has == VARIANT_TRUE, "attr hasChildNodes %08lx %d", hr, has);
            hr = IHTMLDOMAttribute2_get_childNodes(a2, &kids); CHECK(hr == S_OK && kids && coll_len(kids) == 1, "attr childNodes %08lx %d", hr, kids ? coll_len(kids) : -2);
            if (kids) IDispatch_Release(kids);
            hr = IHTMLDOMAttribute2_get_firstChild(a2, &first); CHECK(hr == S_OK && first != NULL, "attr firstChild %08lx", hr);
            if (first) { VARIANT nv; IHTMLDOMNode_get_nodeValue(first, &nv); CHECK(!strcmp(vs(&nv), "d"), "attr first value '%s'", vs(&nv)); VariantClear(&nv); IHTMLDOMNode_Release(first); }
            hr = IHTMLDOMAttribute2_get_lastChild(a2, &last); CHECK(hr == S_OK && last != NULL, "attr lastChild %08lx", hr);
            if (last) IHTMLDOMNode_Release(last);
            hr = IHTMLDOMAttribute2_cloneNode(a2, VARIANT_TRUE, &clone); CHECK(hr == S_OK && clone, "attr cloneNode %08lx", hr);
            if (clone) {
                IHTMLDOMAttribute2 *c2 = NULL; VARIANT nv, ref; IHTMLDOMNode *t1 = NULL, *t2 = NULL, *res = NULL, *fc = NULL;
                IHTMLDOMAttribute_get_nodeName(clone, &cn); CHECK(cn && !wcscmp(cn, L"id"), "clone name %ls", cn ? cn : L""); SysFreeString(cn);
                IHTMLDOMAttribute_QueryInterface(clone, &IID_IHTMLDOMAttribute2, (void **)&c2);
                { BSTR t = SysAllocString(L"zz"); IHTMLDocument3_createTextNode(doc3, t, &t1); SysFreeString(t); }
                { BSTR t = SysAllocString(L"A"); IHTMLDocument3_createTextNode(doc3, t, &t2); SysFreeString(t); }
                if (c2 && t1 && t2) {
                    V_VT(&ref) = VT_NULL;
                    hr = IHTMLDOMAttribute2_appendChild(c2, t1, &res); CHECK(hr == S_OK && res == t1, "attr appendChild %08lx", hr); if (res) IHTMLDOMNode_Release(res); res = NULL;
                    IHTMLDOMAttribute_get_nodeValue(clone, &nv); CHECK(!strcmp(vs(&nv), "dzz"), "after append '%s'", vs(&nv)); VariantClear(&nv);
                    IHTMLDOMAttribute2_get_firstChild(c2, &fc);
                    V_VT(&ref) = VT_DISPATCH; V_DISPATCH(&ref) = (IDispatch *)fc;
                    hr = IHTMLDOMAttribute2_insertBefore(c2, t2, ref, &res); CHECK(hr == S_OK, "attr insertBefore %08lx", hr); if (res) IHTMLDOMNode_Release(res); res = NULL;
                    IHTMLDOMAttribute_get_nodeValue(clone, &nv); CHECK(!strcmp(vs(&nv), "Adzz"), "after insertBefore '%s'", vs(&nv)); VariantClear(&nv);
                    hr = IHTMLDOMAttribute2_replaceChild(c2, t1, fc, &res); CHECK(hr == S_OK, "attr replaceChild %08lx", hr); if (res) IHTMLDOMNode_Release(res); res = NULL;
                    IHTMLDOMAttribute_get_nodeValue(clone, &nv); CHECK(!strcmp(vs(&nv), "zz"), "after replaceChild '%s'", vs(&nv)); VariantClear(&nv);
                    hr = IHTMLDOMAttribute2_removeChild(c2, fc, &res); CHECK(hr == S_OK, "attr removeChild %08lx", hr); if (res) IHTMLDOMNode_Release(res);
                    { VARIANT_BOOL has = VARIANT_TRUE; IHTMLDOMAttribute2_hasChildNodes(c2, &has); CHECK(has == VARIANT_FALSE, "empty attr has children %d", has); }
                }
                if (fc) IHTMLDOMNode_Release(fc);
                if (t1) IHTMLDOMNode_Release(t1);
                if (t2) IHTMLDOMNode_Release(t2);
                if (c2) IHTMLDOMAttribute2_Release(c2);
                IHTMLDOMAttribute_Release(clone);
            }
            IHTMLDOMAttribute2_Release(a2);
        }
        if (attr) IHTMLDOMAttribute_Release(attr);
        if (e4) IHTMLElement4_Release(e4);
    }

    /* current style */
    {
        IHTMLElement2 *e2i = NULL; IHTMLCurrentStyle *cs = NULL; IHTMLCurrentStyle2 *cs2 = NULL; IHTMLCurrentStyle4 *cs4 = NULL;
        IDispatch *sd = by_id(doc3, L"sp");
        for (i = 0; i < 2; i++) {
            IDispatch *x = i ? sd : e;
            IHTMLElement2 *el2 = NULL;
            IDispatch_QueryInterface(x, &IID_IHTMLElement2, (void **)&el2);
            cs = NULL; IHTMLElement2_get_currentStyle(el2, &cs);
            CHECK(cs != NULL, "currentStyle");
            if (cs) {
                VARIANT_BOOL b; VARIANT zv, gv; BSTR bd = NULL, mi = NULL; BSTR nm = SysAllocString(L"display");
                IHTMLCurrentStyle_QueryInterface(cs, &IID_IHTMLCurrentStyle2, (void **)&cs2);
                IHTMLCurrentStyle_QueryInterface(cs, &IID_IHTMLCurrentStyle4, (void **)&cs4);
                hr = IHTMLCurrentStyle_getAttribute(cs, nm, 0, &gv);
                CHECK(hr == S_OK && !strcmp(vs(&gv), i ? "inline" : "block"), "currentStyle.getAttribute(display) %08lx '%s'", hr, vs(&gv));
                VariantClear(&gv); SysFreeString(nm);
                hr = IHTMLCurrentStyle_get_blockDirection(cs, &bd); CHECK(hr == S_OK && bd && !wcscmp(bd, L"ltr"), "blockDirection %08lx '%ls'", hr, bd ? bd : L""); SysFreeString(bd);
                if (cs2) {
                    hr = IHTMLCurrentStyle2_get_isBlock(cs2, &b); CHECK(hr == S_OK && b == (i ? VARIANT_FALSE : VARIANT_TRUE), "isBlock[%d] %08lx %d", i, hr, b);
                    hr = IHTMLCurrentStyle2_get_hasLayout(cs2, &b); CHECK(hr == S_OK && b == (i ? VARIANT_FALSE : VARIANT_TRUE), "hasLayout[%d] %08lx %d", i, hr, b);
                    hr = IHTMLCurrentStyle2_get_zoom(cs2, &zv); CHECK(hr == S_OK && !strcmp(vs(&zv), "normal"), "zoom %08lx '%s'", hr, vs(&zv)); VariantClear(&zv);
                    IHTMLCurrentStyle2_Release(cs2);
                }
                if (cs4) { hr = IHTMLCurrentStyle4_get_msInterpolationMode(cs4, &mi); CHECK(hr == S_OK, "msInterpolationMode %08lx", hr); SysFreeString(mi); IHTMLCurrentStyle4_Release(cs4); }
                if (i == 0) {
                    VARIANT cb;
                    hr = IHTMLCurrentStyle_get_clipBottom(cs, &cb); CHECK(hr == S_OK, "currentStyle.clipBottom %08lx", hr); VariantClear(&cb);
                }
                IHTMLCurrentStyle_Release(cs);
            }
            IHTMLElement2_Release(el2);
        }
        (void)e2i;
        IDispatch_Release(sd);
    }
    IDispatch_Release(e);

    /* style element */
    e = by_id(doc3, L"st");
    CHECK(!strcmp(pstr(e, L"readyState"), "complete"), "style readyState '%s'", pstr(e, L"readyState"));
    hr = pput(e, L"disabled", vbool(TRUE)); CHECK(hr == S_OK, "style put disabled %08lx", hr);
    CHECK(!strcmp(pstr(e, L"disabled"), "-1") || !strcmp(pstr(e, L"disabled"), "True"), "style disabled '%s'", pstr(e, L"disabled"));
    pput(e, L"disabled", vbool(FALSE));
    CHECK(!strcmp(pstr(e, L"disabled"), "0") || !strcmp(pstr(e, L"disabled"), "False"), "style enabled '%s'", pstr(e, L"disabled"));
    IDispatch_Release(e);

    /* table */
    e = by_id(doc3, L"tb");
    { VARIANT d8; V_VT(&d8) = VT_R8; V_R8(&d8) = 3; hr = pput(e, L"cellSpacing", d8); CHECK(hr == S_OK, "cellSpacing double %08lx", hr);
      CHECK(!strcmp(pstr(e, L"cellSpacing"), "3"), "cellSpacing '%s'", pstr(e, L"cellSpacing")); }
    hr = pput(e, L"dataPageSize", vi4(7)); CHECK(hr == S_OK, "put dataPageSize %08lx", hr);
    CHECK(!strcmp(pstr(e, L"dataPageSize"), "7"), "dataPageSize '%s'", pstr(e, L"dataPageSize"));
    hr = pput(e, L"dataPageSize", vi4(-1)); CHECK(hr == E_INVALIDARG, "dataPageSize -1 %08lx", hr);
    CHECK(!strcmp(pstr(e, L"readyState"), "complete"), "table readyState '%s'", pstr(e, L"readyState"));
    hr = dcall(e, L"refresh", DISPATCH_METHOD, NULL, 0, NULL); CHECK(hr == S_OK, "refresh %08lx", hr);
    hr = dcall(e, L"nextPage", DISPATCH_METHOD, NULL, 0, NULL); CHECK(hr == S_OK, "nextPage %08lx", hr);
    hr = dcall(e, L"firstPage", DISPATCH_METHOD, NULL, 0, NULL); CHECK(hr == S_OK, "firstPage %08lx", hr);
    r.vt = VT_EMPTY; hr = pget(e, L"cells", &r);
    CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && coll_len(V_DISPATCH(&r)) == 4, "table.cells %08lx len %d", hr, V_VT(&r) == VT_DISPATCH ? coll_len(V_DISPATCH(&r)) : -2);
    VariantClear(&r);
    {
        VARIANT a[2]; IHTMLElementCollection *rows = NULL; IHTMLTable *tbl = NULL;
        a[1] = vi4(0); a[0] = vi4(2);   /* arguments are in reverse order: indexFrom 0, indexTo 2 */
        r.vt = VT_EMPTY; hr = dcall(e, L"moveRow", DISPATCH_METHOD, a, 2, &r);
        CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "moveRow %08lx", hr);
        VariantClear(&r);
        IDispatch_QueryInterface(e, &IID_IHTMLTable, (void **)&tbl);
        IHTMLTable_get_rows(tbl, &rows);
        {
            VARIANT idx0 = vi4(0), nov; IDispatch *row = NULL; V_VT(&nov) = VT_EMPTY;
            IHTMLElementCollection_item(rows, idx0, nov, &row);
            CHECK(row && !strcmp(pstr(row, L"id"), "r1"), "first row after moveRow '%s'", row ? pstr(row, L"id") : "(none)");
            if (row) IDispatch_Release(row);
            idx0 = vi4(2); row = NULL;
            IHTMLElementCollection_item(rows, idx0, nov, &row);
            CHECK(row && !strcmp(pstr(row, L"id"), "r0"), "last row after moveRow '%s'", row ? pstr(row, L"id") : "(none)");
            if (row) IDispatch_Release(row);
        }
        IHTMLElementCollection_Release(rows); IHTMLTable_Release(tbl);
    }
    IDispatch_Release(e);

    /* window.close default action on a document without a host window: harmless */
    {
        IHTMLWindow2 *win = NULL;
        IHTMLDocument2_get_parentWindow(doc, &win);
        if (win) IHTMLWindow2_Release(win);
    }

    IPersistFile_Release(pf); IHTMLDocument3_Release(doc3); IHTMLDocument2_Release(doc);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    test_page();
    test_dialogs();
    pump(200);
    CoUninitialize();
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
