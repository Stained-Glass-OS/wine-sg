/* Windows.UI.Xaml compiled markup of WinUI 2 apps (patches/sg/6177): an x:Load element (a DeferredElement
 * holding a section reference of kind 9) made only when FindName asks for it, put where it was; an
 * object written as a root (node 0x17) inside the tree no longer ends the reading; an x:Name on an
 * object without a Name member; markup in Silverlight's namespace. Fixtures: test/mkxbf.py.
 * Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_Border,         0x797c4539, 0x45bd, 0x4633, 0xa0, 0x44, 0xbf, 0xb0, 0x2e, 0xf5, 0x17, 0x0f);
DEFINE_GUID(IID_AppStatics,     0x06499997, 0xf7b4, 0x45fe, 0xb7, 0x63, 0x75, 0x77, 0xd1, 0xd3, 0xcb, 0x4a);
DEFINE_GUID(IID_UriFactory,     0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
DEFINE_GUID(IID_GridFactory,    0xae814041, 0xc531, 0x43b4, 0xbf, 0x99, 0x12, 0xf5, 0x06, 0xf7, 0xb0, 0x1c);
DEFINE_GUID(IID_VTHStatics,     0xe75758c4, 0xd25d, 0x4b1d, 0x97, 0x1f, 0x59, 0x6f, 0x17, 0xf1, 0x2b, 0xaa);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }

static obj *app_statics, *uri_factory, *grid_factory, *vth;
static HRESULT load(IInspectable *component, const WCHAR *name)
{
    WCHAR buf[128];
    obj *uri = NULL;
    swprintf(buf, 128, L"ms-appx:///%ls.xaml", name);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(uri_factory, 6))(uri_factory, hs(buf), &uri);
    return ((HRESULT (WINAPI *)(obj *, IInspectable *, obj *))SLOT(app_statics, 8))(app_statics, component, uri);
}
static obj *new_grid(void)
{
    IInspectable *inner = NULL;
    obj *g = NULL;
    ((create_fn)SLOT(grid_factory, 6))(grid_factory, NULL, &inner, &g);
    return g;
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}
static INT32 children(obj *elem)
{
    INT32 n = -1;
    ((HRESULT (WINAPI *)(obj *, obj *, INT32 *))SLOT(vth, 11))(vth, as(elem, &IID_UIE), &n);
    return n;
}
static obj *child(obj *elem, INT32 i)
{
    obj *c = NULL;
    ((HRESULT (WINAPI *)(obj *, obj *, INT32, obj **))SLOT(vth, 10))(vth, as(elem, &IID_UIE), i, &c);
    return c;
}

int main(void)
{
    obj *g, *lazy, *again, *rs, *o = NULL;
    INT32 before, after;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(app_statics = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics))) return 1;
    if (!(uri_factory = factory(L"Windows.Foundation.Uri", &IID_UriFactory))) return 1;
    if (!(grid_factory = factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory))) return 1;
    if (!(vth = factory(L"Windows.UI.Xaml.Media.VisualTreeHelper", &IID_VTHStatics))) return 1;

    /* an x:Load element: not made with the page, made and put in its place when found by name */
    g = new_grid();
    hr = load((IInspectable *)g, L"DeferLoad");
    before = children(g);
    lazy = find(g, L"Lazy");
    after = children(g);
    again = find(g, L"Lazy");
    printf("deferload=%08lx,%d,%d,%d,%d,%d\n", hr, before, find(g, L"After") != NULL, lazy != NULL && as(lazy, &IID_Border) != NULL,
           after, again && as(again, &IID_UIE) == as(lazy, &IID_UIE));
    printf("placed=%d\n", lazy && as(child(g, 0), &IID_UIE) == as(lazy, &IID_UIE));
    printf("unknown=%d\n", find(g, L"Nothing") == NULL);

    /* the kind 9 reference outside a DeferredElement (Deferred.xbf): still left unrealized */
    g = new_grid();
    hr = load((IInspectable *)g, L"Deferred");
    printf("bare=%08lx,%d\n", hr, find(g, L"Lazy") == NULL);

    /* an object written as a root inside the tree */
    g = new_grid();
    hr = load((IInspectable *)g, L"NestedRoot");
    printf("nestedroot=%08lx,%d,%d\n", hr, find(g, L"Inner") != NULL, find(g, L"Tail") != NULL);

    /* an x:Name on an object without a Name member */
    g = new_grid();
    hr = load((IInspectable *)g, L"NamedBrush");
    printf("namedbrush=%08lx,%d\n", hr, find(g, L"Tail2") != NULL);

    /* Silverlight's namespace names the same types */
    rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics);
    hr = ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(rs, 6))(rs, hs(L"<Border xmlns=\"http://schemas.microsoft.com/client/2007\" Width=\"5\"/>"), &o);
    printf("silverlight=%08lx,%d\n", hr, as(o, &IID_Border) != NULL);
    return 0;
}
