/* xamlrnviews-probe: what React Native for Windows asks of Windows.UI.Xaml to show its views
 * (patches/sg/6211).
 *
 * OneDrive's sign-in page (React Native for Windows) stopped at each missing piece:
 *  - AutomationProperties' attached LiveSetting, LabeledBy, IsRequiredForForm, AccessKey (kept);
 *  - AutomationElementIdentifiers and the patterns' identifiers (the same AutomationProperty each
 *    time; ScrollPatternIdentifiers.NoScroll -1) and FrameworkElementAutomationPeer.FromElement
 *    (no peer made);
 *  - TextBox's statics 2 to 8 (PlaceholderTextProperty, DescriptionProperty, ...);
 *  - ScrollViewer 2 to 4 (headers kept, ChangeView taken, events accepted);
 *  - FocusManager 3 to 6 (TryFocusAsync focuses and completes at once with its result).
 * Interface identity per windows-rs 0.30.0 (MIT).
 *
 * Prints name=value lines; see test/xamlrnviews-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_APStatics,      0xb618fd7b, 0x32d0, 0x4970, 0x9c, 0x42, 0x7c, 0x03, 0x9a, 0xc7, 0xbe, 0x78);
DEFINE_GUID(IID_AEIStatics,     0x4549399f, 0x8340, 0x4d67, 0xb9, 0xbf, 0x8c, 0x2a, 0xc6, 0xa0, 0x77, 0x3a);
DEFINE_GUID(IID_ToggleStatics,  0xc7f75544, 0x14a5, 0x4f2f, 0x92, 0xfc, 0x76, 0x05, 0x24, 0xde, 0x06, 0xea);
DEFINE_GUID(IID_ScrollStatics,  0x4bf8e0a1, 0xfb7f, 0x4fa4, 0x83, 0xb3, 0xcf, 0xae, 0xb1, 0x03, 0xa6, 0x85);
DEFINE_GUID(IID_PeerStatics,    0xb9c0b997, 0x2820, 0x44a1, 0xa5, 0xa8, 0x9b, 0x80, 0x1e, 0xdc, 0x26, 0x9e);
DEFINE_GUID(IID_TBStatics2,     0x7592f37d, 0x4e76, 0x4ee4, 0x8e, 0x0f, 0x7d, 0x6e, 0x1a, 0x23, 0x32, 0x6b);
DEFINE_GUID(IID_TBStatics8,     0x7f93d7c4, 0x7d88, 0x5f4d, 0x89, 0x55, 0xa5, 0xad, 0x38, 0x48, 0xbc, 0xb6);
DEFINE_GUID(IID_SV2,            0x64e9be10, 0x4dd1, 0x494d, 0xab, 0xf7, 0xcb, 0xd3, 0xc5, 0x77, 0x49, 0x1d);
DEFINE_GUID(IID_FMStatics5,     0x280edc61, 0x207a, 0x4d7b, 0xb9, 0x8f, 0xce, 0x16, 0x5e, 0x1b, 0x20, 0x15);
DEFINE_GUID(IID_FMStatics6,     0x3546a1b6, 0x20bf, 0x5007, 0x92, 0x9d, 0xe6, 0xd3, 0x2e, 0x16, 0xaf, 0xe4);
DEFINE_GUID(IID_FMStatics,      0x1eccd326, 0x8182, 0x4482, 0x82, 0x6a, 0x09, 0x18, 0xe9, 0xed, 0x9a, 0xf7);
DEFINE_GUID(IID_MoveResult,     0x06dfead3, 0xc2ae, 0x44bb, 0xbf, 0xab, 0x9c, 0x73, 0xde, 0x84, 0x07, 0xa4);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static obj *identity(obj *o) { return as(o, &IID_IUnknown); }
static obj *prop(obj *statics, int slot) { obj *p = NULL; if (statics) ((get_fn)SLOT(statics, slot))(statics, &p); return p; }
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    if (root) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

/* an AsyncOperationCompletedHandler<FocusMovementResult> and an event handler */
static int completed_calls, completed_status = -1;
static HRESULT WINAPI h_qi(void *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_ref(void *h) { return 2; }
static HRESULT WINAPI h_invoke(void *h, void *op, int status) { completed_calls++; completed_status = status; return S_OK; }
static void *h_vtbl[] = { h_qi, h_ref, h_ref, h_invoke };
static struct { void **vtbl; } handler = { h_vtbl };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<Border x:Name=\"B\" Width=\"50\" Height=\"20\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"<TextBox x:Name=\"Tb\" Width=\"150\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"0,40,0,0\"/>"
    L"<ScrollViewer x:Name=\"Sv\" Width=\"100\" Height=\"50\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"0,100,0,0\"/>"
    L"</Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *b, *tb, *sv, *ap, *aei, *got = NULL, *op = NULL;
    obj *fm5, *fm, *result = NULL, *focused = NULL, *sv2, *pvs, *header = NULL, *peer = (obj *)1, *labeled = NULL;
    IInspectable *inner = NULL, *text = NULL;
    INT32 live = -1;
    boolean required = FALSE, ok = FALSE, succeeded = FALSE;
    DOUBLE noscroll = 0;
    INT64 token = 0, token2 = 0;
    HSTRING key = NULL;
    HWND host, island = NULL;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    b = find(root, L"B");
    tb = find(root, L"Tb");
    sv = find(root, L"Sv");

    /* AutomationProperties' attached members */
    ap = factory(L"Windows.UI.Xaml.Automation.AutomationProperties", &IID_APStatics);
    CALL(ap, 35, HRESULT (WINAPI *)(obj *, obj *, INT32))(ap, b, 2);              /* SetLiveSetting Assertive */
    CALL(ap, 34, HRESULT (WINAPI *)(obj *, obj *, INT32 *))(ap, b, &live);
    CALL(ap, 29, HRESULT (WINAPI *)(obj *, obj *, obj *))(ap, b, tb);             /* SetLabeledBy */
    CALL(ap, 28, HRESULT (WINAPI *)(obj *, obj *, obj **))(ap, b, &labeled);
    CALL(ap, 20, HRESULT (WINAPI *)(obj *, obj *, boolean))(ap, b, TRUE);         /* SetIsRequiredForForm */
    CALL(ap, 19, HRESULT (WINAPI *)(obj *, obj *, boolean *))(ap, b, &required);
    CALL(ap, 11, HRESULT (WINAPI *)(obj *, obj *, HSTRING))(ap, b, hs(L"K"));    /* SetAccessKey */
    CALL(ap, 10, HRESULT (WINAPI *)(obj *, obj *, HSTRING *))(ap, b, &key);
    printf("attached=%d,%d,%d,%ls\n", live, labeled && identity(labeled) == identity(tb), required,
           key ? WindowsGetStringRawBuffer(key, NULL) : L"-");

    /* identifiers */
    aei = factory(L"Windows.UI.Xaml.Automation.AutomationElementIdentifiers", &IID_AEIStatics);
    printf("identifiers=%d,%d,%d\n", prop(aei, 26) != NULL && prop(aei, 26) == prop(aei, 26),
           prop(aei, 26) != prop(aei, 10),
           prop(factory(L"Windows.UI.Xaml.Automation.TogglePatternIdentifiers", &IID_ToggleStatics), 6) != NULL);
    CALL(factory(L"Windows.UI.Xaml.Automation.ScrollPatternIdentifiers", &IID_ScrollStatics), 9,
         HRESULT (WINAPI *)(obj *, DOUBLE *))(factory(L"Windows.UI.Xaml.Automation.ScrollPatternIdentifiers", &IID_ScrollStatics), &noscroll);
    hr = CALL(factory(L"Windows.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer", &IID_PeerStatics), 6,
              HRESULT (WINAPI *)(obj *, obj *, obj **))(factory(L"Windows.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer",
              &IID_PeerStatics), b, &peer);
    printf("noscroll=%.0f peer=%08lx,%d\n", noscroll, hr, peer == NULL);

    /* TextBox statics */
    printf("textbox_statics=%d,%d\n", prop(factory(L"Windows.UI.Xaml.Controls.TextBox", &IID_TBStatics2), 8) != NULL,
           prop(factory(L"Windows.UI.Xaml.Controls.TextBox", &IID_TBStatics8), 11) != NULL);

    /* ScrollViewer 2 */
    sv2 = as(sv, &IID_SV2);
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    if (pvs) ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(L"head"), &text);
    if (sv2)
    {
        CALL(sv2, 11, putobj_fn)(sv2, (obj *)text);                                  /* TopHeader */
        CALL(sv2, 10, get_fn)(sv2, &header);
        CALL(sv2, 12, HRESULT (WINAPI *)(obj *, void *, INT64 *))(sv2, &handler, &token);   /* ViewChanging */
        CALL(sv2, 14, HRESULT (WINAPI *)(obj *, void *, void *, void *, boolean *))(sv2, NULL, NULL, NULL, &ok);   /* ChangeView */
    }
    printf("scrollviewer2=%d,%d,%d,%d\n", sv2 != NULL, header && identity(header) == identity((obj *)text), token != 0, ok);

    /* FocusManager: TryFocusAsync in an island */
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(600);
    fm5 = factory(L"Windows.UI.Xaml.Input.FocusManager", &IID_FMStatics5);
    hr = fm5 ? CALL(fm5, 6, HRESULT (WINAPI *)(obj *, obj *, INT32, obj **))(fm5, tb, 2, &op) : E_FAIL;   /* Keyboard */
    if (op)
    {
        CALL(op, 6, HRESULT (WINAPI *)(obj *, void *))(op, &handler);                 /* Completed */
        CALL(op, 8, get_fn)(op, &result);                                             /* GetResults */
        if (result) CALL(as(result, &IID_MoveResult), 6, HRESULT (WINAPI *)(obj *, boolean *))(as(result, &IID_MoveResult), &succeeded);
    }
    fm = factory(L"Windows.UI.Xaml.Input.FocusManager", &IID_FMStatics);
    if (fm) ((get_fn)SLOT(fm, 6))(fm, &focused);
    printf("try_focus=%08lx,%d,%d,%d,%d\n", hr, completed_calls, completed_status, succeeded,
           focused && identity(focused) == identity(tb));
    hr = CALL(factory(L"Windows.UI.Xaml.Input.FocusManager", &IID_FMStatics6), 6,
              HRESULT (WINAPI *)(obj *, void *, INT64 *))(factory(L"Windows.UI.Xaml.Input.FocusManager", &IID_FMStatics6), &handler, &token2);
    printf("focus_events=%08lx,%d\n", hr, token2 != 0);
    printf("done=1\n");
    return 0;
}
