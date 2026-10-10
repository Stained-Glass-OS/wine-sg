/* shell32 ExplorerBrowser batch (patches/sg/2016), run by test/ebrowser-gate.sh.
 * Families (the first four are what Windows does, per Wine's conformance
 * tests): IShellBrowser stubs that answer S_OK, SendControlMsg, the window
 * style, IInputObject before and after browsing, the shell view's key
 * handling; and what the control adds: IServiceProvider and the
 * IExplorerBrowserEvents connection point.
 *
 *   ebrowser-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <servprov.h>
#include <ocidl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

#define FCIDM_TB_SMALLICON 0xA003
DEFINE_GUID(SID_STopLevelBrowser, 0x4C96BE40, 0x915C, 0x11CF, 0x99, 0xD3, 0x00, 0xAA, 0x00, 0x4A, 0xE8, 0x37);
#define SID_SShellBrowser IID_IShellBrowser
#define SG_CONNECT_E_NOCONNECTION ((HRESULT)0x80040200)
#define SG_CONNECT_E_CANNOTCONNECT ((HRESULT)0x80040201)
DEFINE_GUID(SG_CLSID_ExplorerBrowser, 0x71f96385, 0xddd6, 0x48d3, 0xa0, 0xc1, 0xae, 0x06, 0xe8, 0xb0, 0x55, 0xfb);
DEFINE_GUID(SG_SID_Marker, 0x3b8e2f1a, 0x1111, 0x4222, 0x83, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);

/* an event sink counting what it is told */
static int created, completed;
typedef struct { IExplorerBrowserEvents iface; LONG ref; } Sink;
static HRESULT WINAPI sink_QI(IExplorerBrowserEvents *i, REFIID riid, void **o)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IExplorerBrowserEvents)) { *o = i; IExplorerBrowserEvents_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IExplorerBrowserEvents *i) { return InterlockedIncrement(&((Sink *)i)->ref); }
static ULONG WINAPI sink_Release(IExplorerBrowserEvents *i) { return InterlockedDecrement(&((Sink *)i)->ref); }
static HRESULT WINAPI sink_Nav(IExplorerBrowserEvents *i, PCIDLIST_ABSOLUTE p) { return S_OK; }
static HRESULT WINAPI sink_Created(IExplorerBrowserEvents *i, IShellView *v) { created++; return S_OK; }
static HRESULT WINAPI sink_Complete(IExplorerBrowserEvents *i, PCIDLIST_ABSOLUTE p) { completed++; return S_OK; }
static HRESULT WINAPI sink_Failed(IExplorerBrowserEvents *i, PCIDLIST_ABSOLUTE p) { return S_OK; }
static IExplorerBrowserEventsVtbl sink_vtbl = { sink_QI, sink_AddRef, sink_Release, sink_Nav, sink_Created, sink_Complete, sink_Failed };
static Sink sink = { { &sink_vtbl }, 1 };

/* a site providing one service */
typedef struct { IServiceProvider iface; IUnknown marker; LONG ref; } Site;
static HRESULT WINAPI site_QI(IServiceProvider *i, REFIID riid, void **o)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IServiceProvider)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI site_AddRef(IServiceProvider *i) { return 2; }
static ULONG WINAPI site_Release(IServiceProvider *i) { return 1; }
static HRESULT WINAPI marker_QI(IUnknown *i, REFIID riid, void **o)
{
    if (IsEqualIID(riid, &IID_IUnknown)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI marker_AddRef(IUnknown *i) { return 2; }
static ULONG WINAPI marker_Release(IUnknown *i) { return 1; }
static IUnknownVtbl marker_vtbl = { marker_QI, marker_AddRef, marker_Release };
static HRESULT WINAPI site_Query(IServiceProvider *i, REFGUID g, REFIID riid, void **o)
{
    Site *s = (Site *)i;
    if (IsEqualGUID(g, &SG_SID_Marker)) { *o = &s->marker; return S_OK; }
    *o = NULL; return E_FAIL;
}
static IServiceProviderVtbl site_vtbl = { site_QI, site_AddRef, site_Release, site_Query };
static Site site = { { &site_vtbl }, { &marker_vtbl }, 1 };

static void pump(void)
{
    MSG msg;
    int i;
    for (i = 0; i < 20; i++)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static int same_object(IUnknown *a, IUnknown *b)
{
    IUnknown *u1 = NULL, *u2 = NULL;
    int ok;
    IUnknown_QueryInterface(a, &IID_IUnknown, (void **)&u1);
    IUnknown_QueryInterface(b, &IID_IUnknown, (void **)&u2);
    ok = u1 && u1 == u2;
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    return ok;
}

int main(void)
{
    IExplorerBrowser *peb = NULL;
    IShellBrowser *psb = NULL;
    IInputObject *pio = NULL;
    IServiceProvider *sp = NULL;
    IConnectionPointContainer *cpc = NULL;
    IConnectionPoint *cp = NULL, *cp2;
    IObjectWithSite *ows = NULL;
    IShellFolder *desktop = NULL;
    HWND hwnd, eb_hwnd = NULL;
    LRESULT lres;
    HRESULT hr;
    RECT rc = { 0, 0, 200, 150 };
    MSG msg;
    DWORD cookie = 0;
    IID iid;
    void *unk;
    WPARAM key;
    int i, wrong;
    static const WPARAM handled[] = { VK_RETURN, VK_PRIOR, VK_NEXT, VK_END, VK_HOME, VK_LEFT, VK_UP, VK_RIGHT,
                                      VK_DOWN, VK_DELETE, VK_F1, VK_F2, VK_F10, 0 };

    CoInitialize(NULL);
    hwnd = CreateWindowExW(0, L"static", NULL, WS_POPUP | WS_VISIBLE, 0, 0, 300, 250, NULL, NULL, NULL, NULL);
    hr = CoCreateInstance(&SG_CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&peb);
    checkhr(hr, S_OK, "create the ExplorerBrowser");
    if (!peb) { printf("RESULT: FAIL\n"); return 1; }
    IExplorerBrowser_QueryInterface(peb, &IID_IShellBrowser, (void **)&psb);
    IExplorerBrowser_QueryInterface(peb, &IID_IInputObject, (void **)&pio);
    hr = IExplorerBrowser_QueryInterface(peb, &IID_IServiceProvider, (void **)&sp);
    checkhr(hr, S_OK, "QI IServiceProvider");
    hr = IExplorerBrowser_QueryInterface(peb, &IID_IConnectionPointContainer, (void **)&cpc);
    checkhr(hr, S_OK, "QI IConnectionPointContainer");
    IExplorerBrowser_QueryInterface(peb, &IID_IObjectWithSite, (void **)&ows);
    hr = IExplorerBrowser_QueryInterface(peb, &IID_IConnectionPoint, &unk);
    checkhr(hr, E_NOINTERFACE, "no IConnectionPoint on the browser itself");

    /* the shell browser's answers before the control has a window */
    lres = 0xdead;
    checkhr(IShellBrowser_SendControlMsg(psb, FCW_STATUS, 0, 0, 0, &lres), S_OK, "SendControlMsg(status)");
    check(lres == 0, "its result is 0");
    lres = 0xdead;
    checkhr(IShellBrowser_SendControlMsg(psb, FCW_TOOLBAR, TB_CHECKBUTTON, FCIDM_TB_SMALLICON, TRUE, &lres), S_OK, "SendControlMsg(toolbar)");
    check(lres == 0, "toolbar result 0");
    checkhr(IShellBrowser_SendControlMsg(psb, FCW_STATUS, 0, 0, 0, NULL), S_OK, "SendControlMsg without a result pointer");
    checkhr(IShellBrowser_SendControlMsg(psb, FCW_TREE, 0, 0, 0, NULL), S_OK, "SendControlMsg(tree)");
    checkhr(IShellBrowser_SendControlMsg(psb, FCW_PROGRESS, 0, 0, 0, NULL), S_OK, "SendControlMsg(progress)");
    lres = 0xdead;
    checkhr(IShellBrowser_SendControlMsg(psb, 1234, 0, 0, 0, &lres), S_OK, "SendControlMsg(unknown control)");
    check(lres == 0, "unknown control result 0");
    checkhr(IShellBrowser_SetStatusTextSB(psb, NULL), S_OK, "SetStatusTextSB(NULL)");
    checkhr(IShellBrowser_SetStatusTextSB(psb, L"text"), S_OK, "SetStatusTextSB(text)");
    checkhr(IShellBrowser_ContextSensitiveHelp(psb, FALSE), S_OK, "ContextSensitiveHelp");
    checkhr(IShellBrowser_EnableModelessSB(psb, TRUE), S_OK, "EnableModelessSB");
    checkhr(IShellBrowser_SetToolbarItems(psb, NULL, 1, 1), S_OK, "SetToolbarItems");
    checkhr(IShellBrowser_OnViewWindowActive(psb, NULL), S_OK, "OnViewWindowActive");

    /* input handling before the window and the view */
    memset(&msg, 0, sizeof(msg));
    msg.hwnd = hwnd; msg.message = WM_KEYDOWN; msg.wParam = VK_F5;
    checkhr(IInputObject_TranslateAcceleratorIO(pio, &msg), E_FAIL, "key before Initialize");
    checkhr(IInputObject_HasFocusIO(pio), E_FAIL, "focus query before Initialize");
    checkhr(IInputObject_UIActivateIO(pio, TRUE, &msg), S_OK, "activate before Initialize");
    checkhr(IInputObject_HasFocusIO(pio), E_FAIL, "still no focus");

    hr = IExplorerBrowser_Initialize(peb, hwnd, &rc, NULL);
    checkhr(hr, S_OK, "Initialize");
    IShellBrowser_GetWindow(psb, &eb_hwnd);
    check(eb_hwnd && (GetWindowLongW(eb_hwnd, GWL_STYLE) & ~(DWORD)WS_DISABLED) ==
          (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_BORDER), "the control's window style");
    checkhr(IInputObject_HasFocusIO(pio), E_FAIL, "no focus query without a view");
    checkhr(IInputObject_TranslateAcceleratorIO(pio, &msg), E_FAIL, "no key handling without a view");
    checkhr(IShellBrowser_SendControlMsg(psb, FCW_STATUS, 0, 0, 0, NULL), S_OK, "SendControlMsg after Initialize");

    /* service provider */
    unk = NULL;
    hr = IServiceProvider_QueryService(sp, &SID_SShellBrowser, &IID_IShellBrowser, &unk);
    check(hr == S_OK && unk && same_object((IUnknown *)unk, (IUnknown *)psb), "QueryService(SShellBrowser) is the shell browser");
    if (unk) IUnknown_Release((IUnknown *)unk);
    unk = NULL;
    hr = IServiceProvider_QueryService(sp, &SID_STopLevelBrowser, &IID_IShellBrowser, &unk);
    check(hr == S_OK && unk, "QueryService(STopLevelBrowser)");
    if (unk) IUnknown_Release((IUnknown *)unk);
    unk = (void *)1;
    hr = IServiceProvider_QueryService(sp, &SG_SID_Marker, &IID_IUnknown, &unk);
    check(FAILED(hr) && unk == NULL, "an unknown service without a site fails");
    IObjectWithSite_SetSite(ows, (IUnknown *)&site.iface);
    unk = NULL;
    hr = IServiceProvider_QueryService(sp, &SG_SID_Marker, &IID_IUnknown, &unk);
    check(hr == S_OK && unk == (void *)&site.marker, "a service the site provides comes from the site");
    IObjectWithSite_SetSite(ows, NULL);
    checkhr(IServiceProvider_QueryService(sp, &SG_SID_Marker, &IID_IUnknown, NULL), E_POINTER, "QueryService without a result");

    /* connection point */
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &IID_IExplorerBrowserEvents, &cp);
    checkhr(hr, S_OK, "FindConnectionPoint(IExplorerBrowserEvents)");
    cp2 = (void *)1;
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &IID_IUnknown, &cp2);
    check(hr == SG_CONNECT_E_NOCONNECTION && cp2 == NULL, "FindConnectionPoint(another IID)");
    memset(&iid, 0, sizeof(iid));
    check(cp && IConnectionPoint_GetConnectionInterface(cp, &iid) == S_OK && IsEqualIID(&iid, &IID_IExplorerBrowserEvents), "GetConnectionInterface");
    cpc = cpc;
    {
        IConnectionPointContainer *back = NULL;
        check(cp && IConnectionPoint_GetConnectionPointContainer(cp, &back) == S_OK && back && same_object((IUnknown *)back, (IUnknown *)peb),
              "GetConnectionPointContainer is the browser");
        if (back) IConnectionPointContainer_Release(back);
    }
    {
        IUnknown *dumb = (IUnknown *)&site.marker;
        hr = IConnectionPoint_Advise(cp, dumb, &cookie);
        checkhr(hr, SG_CONNECT_E_CANNOTCONNECT, "Advise with a sink that is not IExplorerBrowserEvents");
    }
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink.iface, &cookie);
    check(hr == S_OK && cookie, "Advise");
    SHGetDesktopFolder(&desktop);
    hr = IExplorerBrowser_BrowseToObject(peb, (IUnknown *)desktop, SBSP_DEFBROWSER);
    checkhr(hr, S_OK, "BrowseToObject(desktop)");
    pump();
    check(created >= 1, "the sink was told a view was created");
    check(completed >= 1, "and that navigation completed");
    checkhr(IConnectionPoint_Unadvise(cp, cookie), S_OK, "Unadvise");
    checkhr(IConnectionPoint_Unadvise(cp, cookie), SG_CONNECT_E_NOCONNECTION, "Unadvise again");
    {
        int before = created;
        IExplorerBrowser_BrowseToObject(peb, (IUnknown *)desktop, SBSP_DEFBROWSER);
        pump();
        check(created == before, "an unadvised sink hears nothing more");
    }
    IConnectionPoint_Release(cp);

    /* with a view */
    checkhr(IInputObject_UIActivateIO(pio, TRUE, &msg), S_OK, "activate with a view");
    checkhr(IInputObject_HasFocusIO(pio), S_OK, "the view has the focus");
    checkhr(IInputObject_UIActivateIO(pio, FALSE, &msg), S_OK, "deactivate");
    checkhr(IInputObject_HasFocusIO(pio), S_OK, "focus stays");
    {
        HWND other = CreateWindowExW(0, L"edit", NULL, WS_CHILD | WS_VISIBLE, 210, 0, 50, 20, hwnd, NULL, NULL, NULL);
        SetFocus(other);
        checkhr(IInputObject_HasFocusIO(pio), S_FALSE, "no focus in the view when it is elsewhere");
        DestroyWindow(other);
    }
    checkhr(IInputObject_UIActivateIO(pio, TRUE, &msg), S_OK, "activate again");
    checkhr(IInputObject_HasFocusIO(pio), S_OK, "the focus is back");
    wrong = 0;
    for (i = 0; i < 0x100; i++)
    {
        int found = 0, j;
        for (j = 0; handled[j]; j++) if (handled[j] == (WPARAM)i) found = 1;
        key = i;
        msg.wParam = key;
        pump();
        hr = IInputObject_TranslateAcceleratorIO(pio, &msg);
        if (hr != (found ? S_OK : S_FALSE)) { wrong++; printf("   key %02x: hr %08lx\n", i, (unsigned long)hr); }
    }
    check(wrong == 0, "the shell view handles exactly its keys (13 of 256)");
    pump();

    if (desktop) IShellFolder_Release(desktop);
    IObjectWithSite_Release(ows);
    IConnectionPointContainer_Release(cpc);
    IServiceProvider_Release(sp);
    IInputObject_Release(pio);
    IShellBrowser_Release(psb);
    IExplorerBrowser_Destroy(peb);
    IExplorerBrowser_Release(peb);
    DestroyWindow(hwnd);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
