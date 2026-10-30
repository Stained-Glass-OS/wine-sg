/* xamlpeer-probe: an element's FrameworkElementAutomationPeer, and its composition Visual (patches/sg/6215).
 *
 * CreatePeerForElement gave no peer, and ElementCompositionPreview.GetElementVisual gave the visual's
 * container interface where its Visual was asked for: WinUI 2's NavigationView then called a Visual
 * method through it, and Windows Terminal's settings never showed a page. The peer is made once and
 * kept by its element (FromElement gives it after); the Visual is the visual's own interface.
 *
 * Prints name=value lines; see test/xamlpeer-gate.sh.
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
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_PeerStatics,    0xb9c0b997, 0x2820, 0x44a1, 0xa5, 0xa8, 0x9b, 0x80, 0x1e, 0xdc, 0x26, 0x9e);
DEFINE_GUID(IID_FEPeer,         0xb90ad781, 0xbfeb, 0x4451, 0xbd, 0x47, 0x9f, 0x3a, 0x63, 0xeb, 0xd2, 0x4a);
DEFINE_GUID(IID_AutomationPeer, 0x35aac87a, 0x62ee, 0x4d3e, 0xa2, 0x4c, 0x2b, 0xc8, 0x43, 0x2d, 0x68, 0xb7);
DEFINE_GUID(IID_ECPStatics,     0x08c92b38, 0xec99, 0x4c55, 0xbc, 0x85, 0xa1, 0xc1, 0x80, 0xb2, 0x76, 0x46);
DEFINE_GUID(IID_Visual,         0x117e202d, 0xa859, 0x4c89, 0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
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
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    if (root) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
static const WCHAR page_xaml[] = L"<Grid " NS L"><Border x:Name=\"B\" Width=\"10\" Height=\"10\"/></Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *b, *ps, *before = (obj *)1, *peer = NULL, *again = NULL, *owner = NULL, *ecp;
    obj *visual = NULL;
    boolean enabled = FALSE;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    b = find(root, L"B");

    ps = factory(L"Windows.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer", &IID_PeerStatics);
    CALL(ps, 6, HRESULT (WINAPI *)(obj *, obj *, obj **))(ps, b, &before);          /* FromElement */
    hr = CALL(ps, 7, HRESULT (WINAPI *)(obj *, obj *, obj **))(ps, b, &peer);       /* CreatePeerForElement */
    CALL(ps, 6, HRESULT (WINAPI *)(obj *, obj *, obj **))(ps, b, &again);
    printf("peer=%d,%08lx,%d,%d\n", before == NULL, hr, peer != NULL, again && identity(again) == identity(peer));
    if (peer)
    {
        CALL(as(peer, &IID_FEPeer), 6, get_fn)(as(peer, &IID_FEPeer), &owner);       /* Owner */
        CALL(as(peer, &IID_AutomationPeer), 29, HRESULT (WINAPI *)(obj *, boolean *))(as(peer, &IID_AutomationPeer), &enabled);
        hr = CALL(as(peer, &IID_AutomationPeer), 9, HRESULT (WINAPI *)(obj *, INT32))(as(peer, &IID_AutomationPeer), 0);
    }
    printf("owner=%d enabled=%d raise=%08lx\n", owner && identity(owner) == identity(b), enabled, hr);

    ecp = factory(L"Windows.UI.Xaml.Hosting.ElementCompositionPreview", &IID_ECPStatics);
    CALL(ecp, 6, HRESULT (WINAPI *)(obj *, obj *, obj **))(ecp, b, &visual);        /* GetElementVisual */
    printf("visual=%d\n", visual && visual == as(visual, &IID_Visual));
    printf("done=1\n");
    return 0;
}
