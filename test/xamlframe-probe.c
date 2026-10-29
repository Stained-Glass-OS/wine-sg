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


DEFINE_GUID(IID_FrameFactory,   0x02ee93d4, 0x448e, 0x469e, 0x97, 0x99, 0x0a, 0x8a, 0x1f, 0x70, 0xf1, 0x71);
DEFINE_GUID(IID_Frame,          0x68ea500b, 0x3fe9, 0x4735, 0xb6, 0xa0, 0xe7, 0x60, 0x19, 0x33, 0xb0, 0x89);
DEFINE_GUID(IID_Navigate,       0xbf2195a9, 0xf4ea, 0x4336, 0x97, 0x7c, 0xf8, 0xfc, 0xf7, 0x8b, 0x0d, 0x9e);
DEFINE_GUID(IID_NavArgs,        0xb6aa9834, 0x6691, 0x44d1, 0xbd, 0xf7, 0x58, 0x82, 0x0c, 0x27, 0xb0, 0xd0);
DEFINE_GUID(IID_Page,           0xc4b27075, 0xe641, 0x453e, 0x82, 0x4d, 0x01, 0x2f, 0xc7, 0xcf, 0x95, 0xcf);
DEFINE_GUID(IID_CC,             0xa26dd1dc, 0xcd44, 0x435c, 0xbe, 0x94, 0x01, 0xd6, 0x24, 0x1c, 0x23, 0x1c);
DEFINE_GUID(IID_NavTransition,  0x8833848c, 0x4eb7, 0x41f2, 0x87, 0x99, 0x9e, 0xef, 0x0a, 0x21, 0x3b, 0x73);
DEFINE_GUID(IID_DrillIn,        0x3b86201a, 0x45d3, 0x463b, 0x93, 0x9e, 0xc8, 0x59, 0x5f, 0x43, 0x9b, 0xcc);

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

/* a Frame as Windows Terminal's settings page has it */
static const WCHAR page_xaml[] =
    L"<Grid " NS L"><Frame x:Name=\"F\" Padding=\"16,0,16,48\"><Frame.ContentTransitions><TransitionCollection>"
    L"<NavigationThemeTransition><NavigationThemeTransition.DefaultNavigationTransitionInfo>"
    L"<DrillInNavigationTransitionInfo/>"
    L"</NavigationThemeTransition.DefaultNavigationTransitionInfo></NavigationThemeTransition>"
    L"</TransitionCollection></Frame.ContentTransitions></Frame></Grid>";

/* merged dictionaries whose Source is relative to the file naming them */
static const WCHAR rel_xaml[] =
    L"<Grid " NS L"><Grid.Resources><ResourceDictionary><ResourceDictionary.MergedDictionaries>"
    L"<ResourceDictionary Source=\"ms-appx:///Sub/Outer.xaml\"/>"
    L"</ResourceDictionary.MergedDictionaries></ResourceDictionary></Grid.Resources>"
    L"<Border x:Name=\"B\" Background=\"{StaticResource InnerBrush}\"/><Border x:Name=\"C\" Background=\"{StaticResource TopBrush}\"/></Grid>";

DEFINE_GUID(IID_BorderX,        0x797c4539, 0x45bd, 0x4633, 0xa0, 0x44, 0xbf, 0xb0, 0x2e, 0xf5, 0x17, 0x0f);

static int has_background(obj *root, const WCHAR *name)
{
    obj *b = NULL, *brush = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &b);
    if (!b || !as(b, &IID_BorderX)) return 0;
    ((get_fn)SLOT(as(b, &IID_BorderX), 10))(as(b, &IID_BorderX), &brush);
    return brush != NULL;
}

typedef struct { HSTRING Name; INT32 Kind; } type_name;
typedef HRESULT (WINAPI *nav_fn)(obj *, type_name, IInspectable *, boolean *);

static int navigated, last_mode = -1;
static obj *last_content;
static WCHAR last_type[128];
static IInspectable *last_param;
static HRESULT WINAPI h_qi(void *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI h_ref(void *iface) { return 2; }
static HRESULT WINAPI nav_invoke(void *iface, IInspectable *sender, IInspectable *args)
{
    obj *a = as(args, &IID_NavArgs);
    type_name tn = { 0 };
    navigated++;
    if (!a) return S_OK;
    ((get_fn)SLOT(a, 6))(a, &last_content);
    ((HRESULT (WINAPI *)(obj *, IInspectable **))SLOT(a, 7))(a, &last_param);
    ((HRESULT (WINAPI *)(obj *, type_name *))SLOT(a, 8))(a, &tn);
    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(a, 9))(a, &last_mode);
    lstrcpynW(last_type, tn.Name ? WindowsGetStringRawBuffer(tn.Name, NULL) : L"", 128);
    return S_OK;
}
static void *nav_vtbl[] = { h_qi, h_ref, h_ref, nav_invoke };
static obj nav_h = { nav_vtbl };

static obj *content(obj *frame)
{
    obj *c = NULL;
    ((get_fn)SLOT(as(frame, &IID_CC), 6))(as(frame, &IID_CC), &c);
    return c;
}

static void stacks(obj *frame, const char *tag)
{
    boolean back = 9, fwd = 9;
    INT32 depth = -1;
    ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(frame, 8))(frame, &back);
    ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(frame, 9))(frame, &fwd);
    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(frame, 13))(frame, &depth);
    printf("%s back=%d forward=%d depth=%d\n", tag, back, fwd, depth);
}

int main(void)
{
    obj *rs, *root = NULL, *ff, *frame = NULL, *f2 = NULL, *page, *pframe = NULL, *trans = NULL, *item = NULL, *info = NULL;
    IInspectable *inner = NULL, *param;
    type_name tn;
    boolean ok = 9;
    INT64 token;
    UINT32 size = 0;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(ff = factory(L"Windows.UI.Xaml.Controls.Frame", &IID_FrameFactory))) return 1;
    hr = ((create_fn)SLOT(ff, 6))(ff, NULL, &inner, &frame);
    printf("frame=%08lx\n", hr);
    if (!frame) return 1;
    hr = ((HRESULT (WINAPI *)(obj *, obj *, INT64 *))SLOT(frame, 14))(frame, &nav_h, &token);
    printf("navigated_add=%08lx\n", hr);
    stacks(frame, "start");

    /* Navigate(Page, parameter) */
    ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics), 18))
        (factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics), hs(L"param-1"), &param);
    tn.Name = hs(L"Windows.UI.Xaml.Controls.Page"); tn.Kind = 1;
    hr = ((nav_fn)SLOT(frame, 24))(frame, tn, param, &ok);
    printf("navigate=%08lx ok=%d\n", hr, ok);
    page = content(frame);
    printf("content_page=%d\n", as(page, &IID_Page) != NULL);
    if (page && as(page, &IID_Page)) ((get_fn)SLOT(as(page, &IID_Page), 6))(as(page, &IID_Page), &pframe);
    printf("page_frame_is_frame=%d\n", pframe && as(pframe, &IID_Frame) == as(frame, &IID_Frame));
    printf("navigated=%d mode=%d type=%ls param=%d content_same=%d\n", navigated, last_mode, last_type,
           last_param == param, last_content && as(last_content, &IID_Page) == as(page, &IID_Page));
    stacks(frame, "one");

    /* a second page (INavigate.Navigate, no parameter) */
    tn.Name = hs(L"Windows.UI.Xaml.Controls.Page");
    hr = ((HRESULT (WINAPI *)(obj *, type_name, boolean *))SLOT(as(frame, &IID_Navigate), 6))(as(frame, &IID_Navigate), tn, &ok);
    printf("navigate2=%08lx ok=%d new_page=%d\n", hr, ok, as(content(frame), &IID_Page) != as(page, &IID_Page));
    stacks(frame, "two");

    /* back: the first page's type and parameter again */
    hr = ((HRESULT (WINAPI *)(obj *))SLOT(frame, 22))(frame);
    printf("back=%08lx navigated=%d mode=%d param=%d\n", hr, navigated, last_mode, last_param == param);
    stacks(frame, "after_back");
    hr = ((HRESULT (WINAPI *)(obj *))SLOT(frame, 23))(frame);
    printf("forward=%08lx mode=%d\n", hr, last_mode);
    stacks(frame, "after_forward");

    /* a type that cannot be made: not navigated, no error */
    tn.Name = hs(L"Nowhere.NoSuchPage");
    ok = 9;
    hr = ((nav_fn)SLOT(frame, 24))(frame, tn, NULL, &ok);
    printf("bad=%08lx ok=%d\n", hr, ok);

    /* markup */
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(L"F"), &f2);
    printf("markup_frame=%d\n", f2 && as(f2, &IID_Frame) != NULL);
    if (f2) ((get_fn)SLOT(as(f2, &IID_CC), 12))(as(f2, &IID_CC), &trans);
    if (trans) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(trans, 7))(trans, &size);
    if (trans && size) ((HRESULT (WINAPI *)(obj *, UINT32, obj **))SLOT(trans, 6))(trans, 0, &item);
    if (item && as(item, &IID_NavTransition)) ((get_fn)SLOT(as(item, &IID_NavTransition), 6))(as(item, &IID_NavTransition), &info);
    printf("transitions=%u navigation_transition=%d drill_in=%d\n", size, as(item, &IID_NavTransition) != NULL,
           as(info, &IID_DrillIn) != NULL);

    root = NULL;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(rel_xaml), &root);
    printf("load_relative=%08lx\n", hr);
    printf("relative=%d up=%d\n", root && has_background(root, L"B"), root && has_background(root, L"C"));
    return 0;
}
