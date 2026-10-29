#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_Border,         0x797c4539, 0x45bd, 0x4633, 0xa0, 0x44, 0xbf, 0xb0, 0x2e, 0xf5, 0x17, 0x0f);
DEFINE_GUID(IID_Grid,           0xfd104460, 0x2e15, 0x4ba3, 0x8b, 0x8f, 0xfa, 0x69, 0x3a, 0x41, 0x61, 0xe9);
DEFINE_GUID(IID_TB,             0xae2d9271, 0x3b4a, 0x45fc, 0x84, 0x68, 0xf7, 0x94, 0x95, 0x48, 0xf4, 0xd5);
DEFINE_GUID(IID_PageFactory,    0xdfa149ac, 0x1849, 0x445e, 0x93, 0x7c, 0x40, 0xa9, 0x59, 0x0c, 0xc0, 0x76);
DEFINE_GUID(IID_UC,             0xa7a69ec9, 0xea35, 0x4679, 0xbf, 0x29, 0xf4, 0xf0, 0x92, 0x86, 0xd3, 0x14);
DEFINE_GUID(IID_AppStatics,     0x06499997, 0xf7b4, 0x45fe, 0xb7, 0x63, 0x75, 0x77, 0xd1, 0xd3, 0xcb, 0x4a);
DEFINE_GUID(IID_UriFactory,     0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
DEFINE_GUID(IID_GridFactory,    0xae814041, 0xc531, 0x43b4, 0xbf, 0x99, 0x12, 0xf5, 0x06, 0xf7, 0xb0, 0x1c);
DEFINE_GUID(IID_ControlProtected, 0x1791587a, 0x2a7c, 0x4981, 0xa2, 0xd8, 0xab, 0x99, 0xff, 0xfc, 0x65, 0xb9);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_SCBFactory,     0xd935ce0c, 0x86f5, 0x4da6, 0x8a, 0x27, 0xb1, 0x61, 0x9e, 0xf7, 0xf9, 0x2b);
DEFINE_GUID(IID_Brush,          0x8806a321, 0x1e06, 0x422c, 0xa1, 0xcc, 0x01, 0x69, 0x65, 0x59, 0xe0, 0x21);
DEFINE_GUID(IID_Panel,         0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_AppFactory,     0x93bbe361, 0xbe5a, 0x4ee3, 0xb4, 0xa3, 0x95, 0x11, 0x8d, 0xc9, 0x7a, 0x89);
DEFINE_GUID(IID_RDFactory,      0xea3639b5, 0x31b7, 0x4271, 0x92, 0xc9, 0x7c, 0x95, 0x84, 0xa9, 0x1c, 0x22);
DEFINE_GUID(IID_RD,             0xc1ea4f24, 0xd6de, 0x4191, 0x8e, 0x3a, 0xf4, 0x86, 0x01, 0xf7, 0x48, 0x9c);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_TextBox,        0xe48f5a8b, 0x1dff, 0x4352, 0xa1, 0xf4, 0xe5, 0x16, 0x51, 0x4e, 0xc8, 0x82);
DEFINE_GUID(IID_ItemsControl,   0xf4a91dd8, 0xd979, 0x4381, 0x86, 0x52, 0xbd, 0xa0, 0x34, 0x2a, 0x76, 0x5e);
DEFINE_GUID(IID_UIE7,           0xcafc4968, 0x6369, 0x4249, 0x80, 0xf9, 0x3d, 0x65, 0x63, 0x19, 0xe8, 0x11);
DEFINE_GUID(IID_KeyArgs,        0xd4cd3dfe, 0x4079, 0x42e9, 0xa3, 0x9a, 0x30, 0x95, 0xd3, 0xf0, 0x49, 0xc6);
DEFINE_GUID(IID_ButtonBase,     0xfa002c1a, 0x494e, 0x46cf, 0x91, 0xd4, 0xe1, 0x4a, 0x8d, 0x79, 0x86, 0x74);
DEFINE_GUID(IID_TakeFocusArgs,  0xfe61e4b9, 0xa7af, 0x52b3, 0xbd, 0xb9, 0xc3, 0x30, 0x5c, 0x0b, 0x8d, 0xf2);
DEFINE_GUID(IID_NavRequest,     0xfbb93bb5, 0x1496, 0x5a80, 0xac, 0x00, 0xe7, 0x57, 0x35, 0x97, 0x55, 0xe6);
DEFINE_GUID(IID_NavRequestFactory, 0xe746ab8f, 0xb4ef, 0x5390, 0x97, 0xe5, 0xcc, 0x0a, 0x27, 0x79, 0xc5, 0x74);
DEFINE_GUID(IID_NavResult,      0x88d55a5f, 0x9603, 0x5d8f, 0x9c, 0xc7, 0xd1, 0xc4, 0x07, 0x0d, 0x98, 0x01);
DEFINE_GUID(IID_UIE5,           0x8eed9bc2, 0xa58c, 0x4453, 0xaf, 0x0f, 0xa9, 0x2e, 0xe0, 0x6d, 0x03, 0x17);
DEFINE_GUID(IID_GT,             0xa06798b7, 0xa2ec, 0x415f, 0xad, 0xe2, 0xea, 0xde, 0x93, 0x33, 0xf2, 0xc7);
DEFINE_GUID(IID_Connector,      0xf6790987, 0xe6e5, 0x47f2, 0x92, 0xc6, 0xec, 0xcc, 0xe4, 0xba, 0x15, 0x9a);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *getd_fn)(obj *, double *);
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
static int close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 40 && abs(GetGValue(c) - g) < 40 && abs(GetBValue(c) - b) < 40;
}


DEFINE_GUID(IID_DPStatics,      0x49e5f28f, 0x8259, 0x4d5c, 0xaa, 0xe0, 0x83, 0xd5, 0x6d, 0xbb, 0x68, 0xd9);
DEFINE_GUID(IID_PMStatics,      0x3b01077a, 0x6e06, 0x45e9, 0x8b, 0x5c, 0xaf, 0x24, 0x34, 0x58, 0xc0, 0x62);
DEFINE_GUID(IID_VectorObject,   0xb32bdca4, 0x5e52, 0x5b27, 0xbc, 0x5d, 0xd6, 0x6a, 0x1a, 0x26, 0x8c, 0x2a);
DEFINE_GUID(IID_NCCArgs,        0x4cf68d33, 0xe3f2, 0x4964, 0xb8, 0x5e, 0x94, 0x5b, 0x4f, 0x7e, 0x2f, 0x21);
DEFINE_GUID(IID_NCCArgsFactory, 0xb30c3e3a, 0xdf8d, 0x44a5, 0x9a, 0x38, 0x7a, 0xc0, 0xd0, 0x8c, 0xe6, 0x3d);
DEFINE_GUID(IID_FontIcon,       0xd4d61391, 0x39ac, 0x4950, 0x91, 0x66, 0x36, 0x06, 0xc2, 0x64, 0x41, 0x8b);
DEFINE_GUID(IID_FontIcon2,      0xf6ebd84d, 0x37e8, 0x46ee, 0xa5, 0x74, 0x41, 0xad, 0x55, 0x04, 0x82, 0xe0);
DEFINE_GUID(IID_FontIcon3,      0x6155d919, 0x1c6c, 0x4950, 0x87, 0x94, 0x23, 0x1e, 0xf0, 0x91, 0x4d, 0x94);

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

/* items of a collection property a program registered (WinUI 2's NavigationView.MenuItems) */
static const WCHAR page_xaml[] =
    L"<Grid " NS L"><Grid.ProbeItems><Border/><Border/><Border/></Grid.ProbeItems></Grid>";

/* the program's collection: an IVector<Object> counting what is appended */
static int appended;
static HRESULT WINAPI v_qi(void *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_VectorObject)) { *out = iface; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI v_ref(void *iface) { return 2; }
static HRESULT WINAPI v_notimpl(void *iface) { return E_NOTIMPL; }
static HRESULT WINAPI v_name(void *iface, HSTRING *name) { return WindowsCreateString(L"Probe.Items", 11, name); }
static HRESULT WINAPI v_append(void *iface, IInspectable *item) { appended++; return S_OK; }
static void *v_vtbl[] = { v_qi, v_ref, v_ref, v_notimpl, v_name, v_notimpl, v_notimpl, v_notimpl, v_notimpl, v_notimpl,
                          v_notimpl, v_notimpl, v_notimpl, v_append, v_notimpl, v_notimpl, v_notimpl, v_notimpl, v_notimpl };
static obj items = { v_vtbl };

struct type_name { HSTRING name; INT32 kind; };

int main(void)
{
    obj *rs, *root = NULL, *dps, *pms, *meta = NULL, *dp = NULL, *ncf, *args = NULL, *fif, *icon = NULL, *i2, *i3;
    struct type_name tn_type = { NULL, 1 }, tn_owner = { NULL, 1 };
    IInspectable *inner = NULL, *got = NULL;
    INT32 action = -1, nidx = -9, oidx = -9;
    boolean scale = 9, scale2 = 9, mirrored = 9;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);

    /* a collection property registered by the program, its collection the default value */
    dps = factory(L"Windows.UI.Xaml.DependencyProperty", &IID_DPStatics);
    pms = factory(L"Windows.UI.Xaml.PropertyMetadata", &IID_PMStatics);
    if (pms) ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(pms, 6))(pms, &items, &meta);
    tn_type.name = hs(L"Windows.Foundation.Collections.IVector`1<Object>"); tn_owner.name = hs(L"Windows.UI.Xaml.Controls.Grid");
    hr = dps ? ((HRESULT (WINAPI *)(obj *, HSTRING, struct type_name, struct type_name, obj *, obj **))SLOT(dps, 7))
                   (dps, hs(L"ProbeItems"), tn_type, tn_owner, meta, &dp) : E_FAIL;
    printf("register=%08lx\n", hr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    printf("appended=%d\n", appended);

    /* NotifyCollectionChangedEventArgs (Add, new items, at 2) */
    ncf = factory(L"Windows.UI.Xaml.Interop.NotifyCollectionChangedEventArgs", &IID_NCCArgsFactory);
    hr = ncf ? ((HRESULT (WINAPI *)(obj *, INT32, obj *, obj *, INT32, INT32, IInspectable *, IInspectable **, obj **))SLOT(ncf, 6))
                   (ncf, 0, &items, NULL, 2, -1, NULL, &inner, &args) : E_FAIL;
    if (args && as(args, &IID_NCCArgs))
    {
        obj *a = as(args, &IID_NCCArgs);
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(a, 6))(a, &action);
        ((HRESULT (WINAPI *)(obj *, IInspectable **))SLOT(a, 7))(a, &got);
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(a, 9))(a, &nidx);
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(a, 10))(a, &oidx);
    }
    printf("collection_args=%08lx action=%d new_items=%d new=%d old=%d\n", hr, action, got == (IInspectable *)&items, nidx, oidx);

    /* FontIcon's IFontIcon2/IFontIcon3 (WinUI 2's FontIconSource makes its icon with them) */
    fif = factory(L"Windows.UI.Xaml.Controls.FontIcon", &IID_IActivationFactory);
    if (fif) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(fif, 6))(fif, &icon);
    i2 = as(icon, &IID_FontIcon2);
    i3 = as(icon, &IID_FontIcon3);
    if (i2)
    {
        ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(i2, 6))(i2, &scale);
        ((HRESULT (WINAPI *)(obj *, boolean))SLOT(i2, 7))(i2, FALSE);
        ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(i2, 6))(i2, &scale2);
    }
    if (i3)
    {
        ((HRESULT (WINAPI *)(obj *, boolean))SLOT(i3, 7))(i3, TRUE);
        ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(i3, 6))(i3, &mirrored);
    }
    printf("font_icon=%d,%d text_scale=%d,%d mirrored=%d\n", i2 != NULL, i3 != NULL, scale, scale2, mirrored);
    return 0;
}
