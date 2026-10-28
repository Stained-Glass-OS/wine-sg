/* Windows.UI.Xaml for an island host's own dialogs (patches/sg/6173): a default Application once the
 * thread's XAML is initialized (Application.Current, its Resources falling back on the framework's),
 * Canvas (Left/Top attached), UIElement.XamlRoot (Content, Size), VisualTreeHelper (children, parent),
 * UIElement.Transitions kept, a BitmapImage from a file name, KeyboardAccelerators, VisualStateManager
 * and FontWeights -- Advanced Installer's XAML dialogs (TeraCopy's installer). Interface identity per
 * windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_AppStatics,     0x06499997, 0xf7b4, 0x45fe, 0xb7, 0x63, 0x75, 0x77, 0xd1, 0xd3, 0xcb, 0x4a);
DEFINE_GUID(IID_App,            0x74b861a1, 0x7487, 0x46a9, 0x9a, 0x6e, 0xc7, 0x8b, 0x51, 0x27, 0x26, 0xc5);
DEFINE_GUID(IID_MapObject,      0xf5f69427, 0x55ed, 0x5512, 0x84, 0x29, 0xd4, 0xf6, 0x62, 0x6d, 0xfc, 0xdd);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_CanvasStatics,  0x40ce5c46, 0x2962, 0x446f, 0xaa, 0xfb, 0x4c, 0xdc, 0x48, 0x69, 0x39, 0xc9);
DEFINE_GUID(IID_UIE10,          0xd531c629, 0xad2c, 0x5f6b, 0xad, 0xcf, 0xfb, 0x87, 0x28, 0x7d, 0x18, 0xd7);
DEFINE_GUID(IID_XamlRoot,       0x34b50756, 0x1696, 0x5b6d, 0x8e, 0x9b, 0xc7, 0x14, 0x64, 0xcc, 0xad, 0x5a);
DEFINE_GUID(IID_VTHStatics,     0xe75758c4, 0xd25d, 0x4b1d, 0x97, 0x1f, 0x59, 0x6f, 0x17, 0xf1, 0x2b, 0xaa);
DEFINE_GUID(IID_KAFactory,      0x44d88a99, 0x4bfd, 0x4a47, 0xa8, 0x93, 0x51, 0x5f, 0x38, 0x86, 0x23, 0xf6);
DEFINE_GUID(IID_KA,             0x92e6181e, 0x19ae, 0x465a, 0x9b, 0x3c, 0xa7, 0x1e, 0xe9, 0xea, 0x74, 0x20);
DEFINE_GUID(IID_UIE7,           0xcafc4968, 0x6369, 0x4249, 0x80, 0xf9, 0x3d, 0x65, 0x63, 0x19, 0xe8, 0x11);
DEFINE_GUID(IID_VSMStatics,     0x01d0e9e0, 0xd713, 0x414e, 0xa7, 0x4e, 0xe6, 0x3e, 0xc7, 0xac, 0x8c, 0x3d);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_FontWeights,    0xb3b579d5, 0x1ba9, 0x48eb, 0x9d, 0xad, 0xc0, 0x95, 0xe8, 0xc2, 0x3b, 0xa3);
DEFINE_GUID(IID_ToggleButton,   0x589877fb, 0x0fc7, 0x4036, 0x9d, 0x8b, 0x12, 0x7d, 0xfa, 0x75, 0xc1, 0x6d);
DEFINE_GUID(IID_RefBool,        0x3c00fd60, 0x2950, 0x5939, 0xa2, 0x1a, 0x2d, 0x12, 0xc5, 0xa0, 0x1b, 0x8a);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);
typedef HRESULT (WINAPI *add_fn)(obj *, void *, INT64 *);

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
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}


#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Canvas " NS L" Background=\"White\" Width=\"300\" Height=\"200\">"
    L"<Border x:Name=\"Box\" Canvas.Left=\"30\" Canvas.Top=\"40\" Width=\"20\" Height=\"20\" Background=\"Red\"/>"
    L"<Image x:Name=\"Pic\" Canvas.Left=\"100\" Canvas.Top=\"40\" Width=\"20\" Height=\"20\" Stretch=\"Fill\" Source=\"C:\\sq.bmp\"/>"
    L"<CheckBox x:Name=\"Chk\" Canvas.Top=\"100\" Content=\"Key\"/>"
    L"</Canvas>";

static HWND island;

static COLORREF pixel(int x, int y)
{
    HDC hdc = GetDC(island);
    COLORREF c;
    UpdateWindow(island);
    c = GetPixel(hdc, x, y);
    ReleaseDC(island, hdc);
    return c;
}
static int close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 40 && abs(GetGValue(c) - g) < 40 && abs(GetBValue(c) - b) < 40;
}

static int checked(obj *o)
{
    obj *t = as(o, &IID_ToggleButton), *ref = NULL, *b;
    boolean v = FALSE;
    if (!t) return -9;
    ((get_fn)SLOT(t, 6))(t, &ref);
    if (!ref) return -1;
    if (!(b = as(ref, &IID_RefBool))) return -8;
    ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(b, 6))(b, &v);
    return v ? 1 : 0;
}

/* a 4x4 blue bitmap file */
static void write_bmp(const char *path)
{
    BYTE file[14 + 40 + 64] = { 'B', 'M' };
    BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)(file + 14);
    DWORD *px = (DWORD *)(file + 54), written;
    HANDLE h;
    int i;
    *(DWORD *)(file + 2) = sizeof(file);
    *(DWORD *)(file + 10) = 54;
    bih->biSize = 40; bih->biWidth = 4; bih->biHeight = 4; bih->biPlanes = 1; bih->biBitCount = 32; bih->biCompression = BI_RGB;
    for (i = 0; i < 16; i++) px[i] = 0x0000ff;
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, file, sizeof(file), &written, NULL);
    CloseHandle(h);
}

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *as_, *app = NULL, *res = NULL, *map, *pvs, *key = NULL, *val = NULL;
    obj *cs, *box, *xr = NULL, *content = NULL, *vth, *child = NULL, *parent = NULL, *trans = NULL, *entrance = NULL, *kaf, *ka = NULL, *accs = NULL;
    obj *vsm, *fw, *chk;
    IInspectable *inner = NULL;
    HWND host;
    HRESULT hr, hr2;
    boolean has = FALSE, went = FALSE;
    double left = -1;
    float size[2] = { 0 };
    INT32 count = -1;
    UINT32 n = 0;
    UINT16 bold = 0;
    int before, after;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    write_bmp("C:\\sq.bmp");
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    /* an Application of the framework's own: its Resources find the framework's */
    as_ = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics);
    if (as_) ((get_fn)SLOT(as_, 6))(as_, &app);
    if (app && as(app, &IID_App)) ((get_fn)SLOT(as(app, &IID_App), 6))(as(app, &IID_App), &res);
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(pvs, 18))(pvs, hs(L"SystemColorButtonFaceColor"), &key);
    map = as(res, &IID_MapObject);
    hr = hr2 = E_FAIL;
    if (map)
    {
        hr = ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(map, 6))(map, key, &val);
        hr2 = ((HRESULT (WINAPI *)(obj *, obj *, boolean *))SLOT(map, 8))(map, key, &has);
    }
    printf("app=%d,%d\n", app != NULL, res != NULL);
    printf("appres=%08lx,%d,%d\n", hr, val != NULL, SUCCEEDED(hr2) && has);

    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 100, 100, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);

    /* Canvas: placed by Left and Top, moved when they change */
    box = find(root, L"Box");
    cs = factory(L"Windows.UI.Xaml.Controls.Canvas", &IID_CanvasStatics);
    if (cs) ((HRESULT (WINAPI *)(obj *, obj *, double *))SLOT(cs, 7))(cs, as(box, &IID_UIE), &left);
    before = close_to(pixel(40, 50), 255, 0, 0) && close_to(pixel(20, 30), 255, 255, 255);
    if (cs) ((HRESULT (WINAPI *)(obj *, obj *, double))SLOT(cs, 8))(cs, as(box, &IID_UIE), 60);
    pump(200);
    after = close_to(pixel(70, 50), 255, 0, 0) && close_to(pixel(40, 50), 255, 255, 255);
    printf("canvas=%d,%.0f,%d\n", before, left, after);

    /* its XamlRoot: the island's content and size */
    if (as(box, &IID_UIE10)) ((get_fn)SLOT(as(box, &IID_UIE10), 8))(as(box, &IID_UIE10), &xr);
    if (xr && as(xr, &IID_XamlRoot))
    {
        ((get_fn)SLOT(as(xr, &IID_XamlRoot), 6))(as(xr, &IID_XamlRoot), &content);
        ((HRESULT (WINAPI *)(obj *, float *))SLOT(as(xr, &IID_XamlRoot), 7))(as(xr, &IID_XamlRoot), size);
    }
    printf("xamlroot=%d,%d,%.0f\n", xr != NULL, content && as(content, &IID_UIE) == as(root, &IID_UIE), size[0]);

    /* VisualTreeHelper */
    vth = factory(L"Windows.UI.Xaml.Media.VisualTreeHelper", &IID_VTHStatics);
    if (vth)
    {
        ((HRESULT (WINAPI *)(obj *, obj *, INT32 *))SLOT(vth, 11))(vth, as(root, &IID_UIE), &count);
        ((HRESULT (WINAPI *)(obj *, obj *, INT32, obj **))SLOT(vth, 10))(vth, as(root, &IID_UIE), 0, &child);
        ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(vth, 12))(vth, as(box, &IID_UIE), &parent);
    }
    printf("vth=%d,%d,%d\n", count, child && as(child, &IID_UIE) == as(box, &IID_UIE), parent && as(parent, &IID_UIE) == as(root, &IID_UIE));

    /* Transitions: a collection kept */
    ((get_fn)SLOT(as(box, &IID_UIE), 26))(as(box, &IID_UIE), &trans);
    RoActivateInstance(hs(L"Windows.UI.Xaml.Media.Animation.EntranceThemeTransition"), (IInspectable **)&entrance);
    if (trans && entrance)
    {
        ((putobj_fn)SLOT(trans, 13))(trans, entrance);
        ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(trans, 7))(trans, &n);
    }
    printf("transitions=%d,%d,%u\n", trans != NULL, entrance != NULL, n);

    /* a BitmapImage from a file name */
    printf("image=%d\n", close_to(pixel(110, 50), 0, 0, 255));

    /* a keyboard accelerator: F5 checks the check box */
    chk = find(root, L"Chk");
    kaf = factory(L"Windows.UI.Xaml.Input.KeyboardAccelerator", &IID_KAFactory);
    if (kaf) ((create_fn)SLOT(kaf, 6))(kaf, NULL, &inner, &ka);
    if (ka) ((HRESULT (WINAPI *)(obj *, INT32))SLOT(as(ka, &IID_KA), 7))(as(ka, &IID_KA), VK_F5);
    if (as(chk, &IID_UIE7)) ((get_fn)SLOT(as(chk, &IID_UIE7), 6))(as(chk, &IID_UIE7), &accs);
    if (accs && ka) ((putobj_fn)SLOT(accs, 13))(accs, as(ka, &IID_KA));
    before = checked(chk);
    SetFocus(island);
    SendMessageW(island, WM_KEYDOWN, VK_F5, 0x003f0001);
    SendMessageW(island, WM_KEYUP, VK_F5, 0xc03f0001);
    pump(100);
    printf("accel=%d,%d\n", before, checked(chk));

    /* VisualStateManager.GoToState and FontWeights */
    vsm = factory(L"Windows.UI.Xaml.VisualStateManager", &IID_VSMStatics);
    if (vsm) ((HRESULT (WINAPI *)(obj *, obj *, HSTRING, boolean, boolean *))SLOT(vsm, 10))(vsm, as(chk, &IID_Control), hs(L"Checked"), FALSE, &went);
    fw = factory(L"Windows.UI.Text.FontWeights", &IID_FontWeights);
    if (fw) ((HRESULT (WINAPI *)(obj *, UINT16 *))SLOT(fw, 7))(fw, &bold);
    printf("misc=%d,%u\n", went, bold);
    return 0;
}
