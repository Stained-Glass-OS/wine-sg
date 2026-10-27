/* Windows.UI.Xaml hosting (patches/sg/6129): WindowsXamlManager, DesktopWindowXamlSource (XAML
 * Islands) and Application, aggregated as an application's App class aggregates it. There was no
 * Windows.UI.Xaml at all: NanaZip stopped at Windows.UI.Xaml.Application.
 * Interface identity per windows-rs 0.30.0 (MIT OR Apache-2.0) and Wine's IDL. Prints KEY=VALUE. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_Closable,       0x30d5a829, 0x7fa4, 0x4026, 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_Source,         0xd585bfe1, 0x00ff, 0x51be, 0xba, 0x1d, 0xa1, 0x32, 0x99, 0x56, 0xea, 0x0a);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_AppFactory,     0x93bbe361, 0xbe5a, 0x4ee3, 0xb4, 0xa3, 0x95, 0x11, 0x8d, 0xc9, 0x7a, 0x89);
DEFINE_GUID(IID_AppStatics,     0x06499997, 0xf7b4, 0x45fe, 0xb7, 0x63, 0x75, 0x77, 0xd1, 0xd3, 0xcb, 0x4a);
DEFINE_GUID(IID_App,            0x74b861a1, 0x7487, 0x46a9, 0x9a, 0x6e, 0xc7, 0x8b, 0x51, 0x27, 0x26, 0xc5);
DEFINE_GUID(IID_WindowStatics,  0x93328409, 0x4ea1, 0x4afa, 0x83, 0xdc, 0x0c, 0x4e, 0x73, 0xe8, 0x8b, 0xb1);
DEFINE_GUID(IID_WindowPrivate,  0x06636c29, 0x5a17, 0x458d, 0x8e, 0xa2, 0x24, 0x22, 0xd9, 0x97, 0xa9, 0x22);
DEFINE_GUID(IID_CoreWinStatic,  0x4d239005, 0x3c2a, 0x41b1, 0x90, 0x22, 0x53, 0x6b, 0xb9, 0xcf, 0x93, 0xb1);
DEFINE_GUID(IID_CoreWinInterop, 0x45d64a29, 0xa63e, 0x4cb6, 0xb4, 0x98, 0x57, 0x81, 0xd2, 0x98, 0xcb, 0x4f);
DEFINE_GUID(IID_Marker,         0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);
typedef HRESULT (WINAPI *get_fn)(obj *, void **);
typedef HRESULT (WINAPI *put_fn)(obj *, void *);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, void **);
typedef HRESULT (WINAPI *attach_fn)(obj *, HWND);
typedef HRESULT (WINAPI *hwnd_fn)(obj *, HWND *);
typedef HRESULT (WINAPI *int_fn)(obj *, INT32 *);
typedef HRESULT (WINAPI *void_fn)(obj *);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}

/* an outer object, as an App class is: it answers IID_Marker itself and hands everything else to
 * the inner Application */
struct outer { IInspectable IInspectable_iface; LONG ref; IInspectable *inner; };
static HRESULT WINAPI outer_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct outer *o = CONTAINING_RECORD(iface, struct outer, IInspectable_iface);
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_Marker))
    { *out = iface; InterlockedIncrement(&o->ref); return S_OK; }
    return o->inner ? IInspectable_QueryInterface(o->inner, iid, out) : E_NOINTERFACE;
}
static ULONG WINAPI outer_addref(IInspectable *iface) { return InterlockedIncrement(&CONTAINING_RECORD(iface, struct outer, IInspectable_iface)->ref); }
static ULONG WINAPI outer_release(IInspectable *iface) { return InterlockedDecrement(&CONTAINING_RECORD(iface, struct outer, IInspectable_iface)->ref); }
static HRESULT WINAPI outer_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI outer_name(IInspectable *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI outer_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl outer_vtbl = { outer_qi, outer_addref, outer_release, outer_iids, outer_name, outer_trust };

/* Window.Current and CoreWindow.GetForCurrentThread on a thread without XAML */
static DWORD WINAPI bare_thread(void *arg)
{
    obj *ws = arg, *w = (obj *)1;
    RoInitialize(RO_INIT_MULTITHREADED);
    ((get_fn)SLOT(ws, 6))(ws, (void **)&w);
    printf("other_thread_window_null=%d\n", w == NULL);
    return 0;
}

int main(void)
{
    obj *ms, *m1 = NULL, *m2 = NULL, *closable = NULL, *sf, *src = NULL, *native = NULL, *af, *as, *app = NULL, *cur = NULL, *marker = NULL;
    IInspectable *inner = NULL;
    struct outer outer = { { &outer_vtbl }, 1, NULL };
    HWND host, island = NULL;
    HRESULT hr;
    INT32 theme = -1;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics))) return 1;
    hr = ((get_fn)SLOT(ms, 6))(ms, (void **)&m1);
    printf("manager=%08lx\n", hr);
    ((get_fn)SLOT(ms, 6))(ms, (void **)&m2);
    printf("manager_same=%d\n", m1 && m1 == m2);
    printf("closable=%08lx\n", ((qi_fn)SLOT(m1, 0))(m1, &IID_Closable, (void **)&closable));
    {
        obj *ws, *win = NULL, *priv = NULL, *cs, *cw = NULL, *cw2 = NULL, *interop = NULL;
        boolean tb = 0;
        HWND core = NULL;
        HANDLE th;

        if (!(ws = factory(L"Windows.UI.Xaml.Window", &IID_WindowStatics))) return 1;
        hr = ((get_fn)SLOT(ws, 6))(ws, (void **)&win);
        printf("window=%08lx nonnull=%d\n", hr, win != NULL);
        if (!win) return 1;
        hr = ((qi_fn)SLOT(win, 0))(win, &IID_WindowPrivate, (void **)&priv);
        printf("window_private=%08lx\n", hr);
        if (priv)
        {
            ((HRESULT (WINAPI *)(obj *, boolean))SLOT(priv, 7))(priv, 1);
            ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(priv, 6))(priv, &tb);
        }
        printf("transparent=%d\n", tb);
        if (!(cs = factory(L"Windows.UI.Core.CoreWindow", &IID_CoreWinStatic))) return 1;
        hr = ((get_fn)SLOT(cs, 6))(cs, (void **)&cw);
        printf("corewindow=%08lx nonnull=%d\n", hr, cw != NULL);
        if (!cw) return 1;
        hr = ((qi_fn)SLOT(cw, 0))(cw, &IID_CoreWinInterop, (void **)&interop);
        if (interop) ((hwnd_fn)SLOT(interop, 3))(interop, &core);
        printf("interop=%08lx hwnd=%d\n", hr, core && IsWindow(core) && !IsWindowVisible(core));
        ((get_fn)SLOT(win, 10))(win, (void **)&cw2);
        printf("window_corewindow_same=%d\n", cw2 == cw);
        th = CreateThread(NULL, 0, bare_thread, ws, 0, NULL);
        WaitForSingleObject(th, 10000);
        fflush(stdout);
    }

    if (!(sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory))) return 1;
    hr = ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, (void **)&src);
    printf("source=%08lx\n", hr);
    hr = ((qi_fn)SLOT(src, 0))(src, &IID_SourceNative, (void **)&native);
    printf("native=%08lx\n", hr);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    printf("attach=%08lx\n", ((attach_fn)SLOT(native, 3))(native, host));
    ((hwnd_fn)SLOT(native, 4))(native, &island);
    printf("island_child=%d\n", island && GetParent(island) == host && (GetWindowLongW(island, GWL_STYLE) & WS_CHILD));
    printf("attach_again=%08lx\n", ((attach_fn)SLOT(native, 3))(native, host));
    {
        obj *content = NULL, *back = (obj *)1;
        ((put_fn)SLOT(src, 7))(src, NULL);
        ((get_fn)SLOT(src, 6))(src, (void **)&back);
        printf("content_null=%d\n", back == NULL);
        (void)content;
    }

    if (!(af = factory(L"Windows.UI.Xaml.Application", &IID_AppFactory))) return 1;
    hr = ((create_fn)SLOT(af, 6))(af, &outer.IInspectable_iface, &inner, (void **)&app);
    outer.inner = inner;
    printf("app=%08lx inner=%d\n", hr, inner != NULL);
    hr = ((qi_fn)SLOT(app, 0))(app, &IID_Marker, (void **)&marker);
    printf("app_delegates=%d\n", hr == S_OK && marker == (obj *)&outer.IInspectable_iface);
    if (!(as = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics))) return 1;
    hr = ((get_fn)SLOT(as, 6))(as, (void **)&cur);
    printf("current=%08lx same=%d\n", hr, cur == app);
    ((int_fn)SLOT(app, 9))(app, &theme);
    printf("theme=%d\n", theme);
    printf("close=%08lx\n", ((void_fn)SLOT(closable, 6))(closable));
    return 0;
}
