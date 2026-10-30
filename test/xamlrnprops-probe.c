/* xamlrnprops-probe: what React Native for Windows sets on Windows.UI.Xaml elements (patches/sg/6207).
 *
 * OneDrive's interface (React Native for Windows) stopped, one missing piece after another, where it
 * makes its views:
 *  - Media3D PerspectiveTransform3D / CompositeTransform3D and UIElement.Transform3D (kept, drawn flat);
 *  - RectangleGeometry and UIElement.Clip, which is drawn: an element shows only inside its clip;
 *  - the other UIElement properties' statics (Projection, CacheMode, Transitions, the Is*Enabled
 *    ones, ManipulationMode) and Control's statics 2-7 (CornerRadiusProperty, the attached
 *    IsTemplateFocusTarget);
 *  - TextBlock.TextTrimming (an ellipsis keeps the text within its width; IsTextTrimmed tells) and
 *    TextHighlighters;
 *  - UIElement.Shadow taken without releasing the caller's reference, and cleared with null.
 * Interface identity per windows-rs 0.30.0 (MIT).
 *
 * Prints name=value lines; see test/xamlrnprops-gate.sh.
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
DEFINE_GUID(IID_UIE3,           0xbc2b28f1, 0x26f2, 0x4aab, 0xb2, 0x56, 0x3b, 0x53, 0x50, 0x88, 0x1e, 0x37);
DEFINE_GUID(IID_UIE10,          0xd531c629, 0xad2c, 0x5f6b, 0xad, 0xcf, 0xfb, 0x87, 0x28, 0x7d, 0x18, 0xd7);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_UIEStatics,     0x58d3573b, 0xf52c, 0x45be, 0x98, 0x8b, 0xa5, 0x86, 0x95, 0x64, 0x87, 0x3c);
DEFINE_GUID(IID_UIEStatics3,    0xd1f87ade, 0xeca1, 0x4561, 0xa3, 0x2b, 0x64, 0x60, 0x1b, 0x4e, 0x05, 0x97);
DEFINE_GUID(IID_Perspective,    0x9a7b532a, 0x30f9, 0x40a1, 0x96, 0xc9, 0xc5, 0x9d, 0x87, 0xf9, 0x5a, 0xc3);
DEFINE_GUID(IID_PerspectiveStatics, 0x8e6f6400, 0x620c, 0x48c7, 0x84, 0x4d, 0x3f, 0x09, 0x84, 0xda, 0x5b, 0x17);
DEFINE_GUID(IID_Composite3D,    0x8977cb01, 0xaf8d, 0x4af5, 0xb0, 0x84, 0xc0, 0x8e, 0xb9, 0x70, 0x4a, 0xbe);
DEFINE_GUID(IID_RectGeometry,   0xa25a1f58, 0xc575, 0x4196, 0x91, 0xcf, 0x9f, 0xdf, 0xb1, 0x04, 0x45, 0xc3);
DEFINE_GUID(IID_RectGeometryStatics, 0x377f8dba, 0x7902, 0x48e3, 0x83, 0xbe, 0x7c, 0x80, 0x02, 0xa6, 0x65, 0x3c);
DEFINE_GUID(IID_Geometry,       0xfa123889, 0x0acd, 0x417b, 0xb6, 0x2d, 0x5c, 0xa1, 0xbf, 0x4d, 0xfc, 0x0e);
DEFINE_GUID(IID_ControlStatics3, 0xaad15e61, 0xe4b9, 0x4d72, 0xb6, 0x53, 0x93, 0x41, 0x49, 0xaa, 0x50, 0x03);
DEFINE_GUID(IID_ControlStatics7, 0xaee1ca45, 0xc910, 0x5dab, 0xa2, 0x66, 0xfd, 0xf5, 0x0c, 0x0b, 0xed, 0xd2);
DEFINE_GUID(IID_TextBlock,      0xae2d9271, 0x3b4a, 0x45fc, 0x84, 0x68, 0xf7, 0x94, 0x95, 0x48, 0xf4, 0xd5);
DEFINE_GUID(IID_TextBlock6,     0x31816cd9, 0x0404, 0x410f, 0x91, 0xec, 0x33, 0xb0, 0xa0, 0xbb, 0x92, 0xb2);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *ref_fn)(obj *);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);

struct wrect { float x, y, w, h; };
struct wsize { float w, h; };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *make(const WCHAR *name)
{
    obj *o = NULL;
    HRESULT hr = RoActivateInstance(hs(name), (IInspectable **)&o);
    if (FAILED(hr)) printf("make %ls=%08lx\n", name, hr);
    return o;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static ULONG refs(obj *o) { ((ref_fn)SLOT(o, 1))(o); return ((ref_fn)SLOT(o, 2))(o); }
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
static int red(COLORREF c) { return GetRValue(c) > 200 && GetGValue(c) < 60 && GetBValue(c) < 60; }

/* desired width of a text block measured within 60 */
static float measured(obj *tb)
{
    struct wsize avail = { 60, 1000 }, want = { 0 };
    obj *u = as(tb, &IID_UIE);
    CALL(u, 91, HRESULT (WINAPI *)(obj *, struct wsize))(u, avail);
    CALL(u, 6, HRESULT (WINAPI *)(obj *, struct wsize *))(u, &want);
    return want.w;
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<Border x:Name=\"B\" Width=\"100\" Height=\"100\" Background=\"Red\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"<TextBlock x:Name=\"T\" Text=\"A long line of text that does not fit\" HorizontalAlignment=\"Left\" Margin=\"0,120,0,0\"/>"
    L"</Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *b, *t, *x3, *c3, *got = NULL, *geo, *shadow;
    obj *uis, *uis3, *cs3, *cs7, *highlighters = NULL, *u;
    IInspectable *inner = NULL;
    struct wrect rect = { 0, 0, 50, 50 }, back = { 0 }, bounds = { 0 };
    double depth = 0, scale = 0, offset = 0;
    boolean tap = FALSE, focus_target = FALSE;
    UINT32 mode = 0, count = 99;
    INT32 trimming = -1;
    float trimmed;
    boolean before = 9, after = 9;
    HWND host, island = NULL;
    HDC hdc;
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
    t = find(root, L"T");

    /* Media3D */
    x3 = make(L"Windows.UI.Xaml.Media.Media3D.PerspectiveTransform3D");
    if (x3)
    {
        obj *p = as(x3, &IID_Perspective);
        CALL(p, 6, HRESULT (WINAPI *)(obj *, double *))(p, &depth);
        CALL(p, 9, HRESULT (WINAPI *)(obj *, double))(p, 12.5);
        CALL(p, 8, HRESULT (WINAPI *)(obj *, double *))(p, &offset);
    }
    printf("perspective=%d depth=%.0f offset=%.1f static=%d\n", x3 != NULL, depth, offset,
           prop(factory(L"Windows.UI.Xaml.Media.Media3D.PerspectiveTransform3D", &IID_PerspectiveStatics), 6) != NULL);
    c3 = make(L"Windows.UI.Xaml.Media.Media3D.CompositeTransform3D");
    if (c3) CALL(as(c3, &IID_Composite3D), 18, HRESULT (WINAPI *)(obj *, double *))(as(c3, &IID_Composite3D), &scale);
    printf("composite3d=%d scale=%.0f\n", c3 != NULL, scale);
    u = as(b, &IID_UIE3);
    hr = CALL(u, 7, putobj_fn)(u, x3);
    CALL(u, 6, get_fn)(u, &got);
    printf("transform3d=%08lx,%d\n", hr, got && identity(got) == identity(x3));
    uis3 = factory(L"Windows.UI.Xaml.UIElement", &IID_UIEStatics3);
    printf("transform3d_static=%d\n", prop(uis3, 6) != NULL);

    /* RectangleGeometry, an element's Clip */
    geo = make(L"Windows.UI.Xaml.Media.RectangleGeometry");
    if (geo)
    {
        CALL(as(geo, &IID_RectGeometry), 7, HRESULT (WINAPI *)(obj *, struct wrect))(as(geo, &IID_RectGeometry), rect);
        CALL(as(geo, &IID_RectGeometry), 6, HRESULT (WINAPI *)(obj *, struct wrect *))(as(geo, &IID_RectGeometry), &back);
        CALL(as(geo, &IID_Geometry), 8, HRESULT (WINAPI *)(obj *, struct wrect *))(as(geo, &IID_Geometry), &bounds);
    }
    printf("rect_geometry=%d rect=%.0f,%.0f,%.0fx%.0f bounds=%.0fx%.0f static=%d\n", geo != NULL, back.x, back.y, back.w, back.h,
           bounds.w, bounds.h, prop(factory(L"Windows.UI.Xaml.Media.RectangleGeometry", &IID_RectGeometryStatics), 6) != NULL);
    u = as(b, &IID_UIE);
    hr = CALL(u, 12, putobj_fn)(u, geo);
    got = NULL;
    CALL(u, 11, get_fn)(u, &got);
    printf("clip=%08lx,%d\n", hr, got && identity(got) == identity(geo));

    /* the other UIElement properties */
    uis = factory(L"Windows.UI.Xaml.UIElement", &IID_UIEStatics);
    printf("ui_statics=%d%d%d%d%d%d%d\n", prop(uis, 31) != NULL, prop(uis, 33) != NULL, prop(uis, 38) != NULL,
           prop(uis, 39) != NULL, prop(uis, 40) != NULL, prop(uis, 43) != NULL, prop(uis, 44) != NULL);
    CALL(u, 30, HRESULT (WINAPI *)(obj *, boolean *))(u, &tap);
    CALL(u, 38, HRESULT (WINAPI *)(obj *, UINT32 *))(u, &mode);
    printf("tap_enabled=%d manipulation=%x\n", tap, mode);
    CALL(u, 31, HRESULT (WINAPI *)(obj *, boolean))(u, FALSE);
    CALL(u, 30, HRESULT (WINAPI *)(obj *, boolean *))(u, &tap);
    printf("tap_disabled=%d\n", !tap);

    /* Control's later statics */
    cs3 = factory(L"Windows.UI.Xaml.Controls.Control", &IID_ControlStatics3);
    cs7 = factory(L"Windows.UI.Xaml.Controls.Control", &IID_ControlStatics7);
    if (cs3)
    {
        CALL(cs3, 9, HRESULT (WINAPI *)(obj *, obj *, boolean))(cs3, b, TRUE);
        CALL(cs3, 8, HRESULT (WINAPI *)(obj *, obj *, boolean *))(cs3, b, &focus_target);
    }
    printf("control_statics=%d,%d focus_target=%d\n", cs3 != NULL, prop(cs7, 7) != NULL, focus_target);

    /* TextBlock: trimming keeps the text within its width; highlighters */
    measured(t);
    CALL(as(t, &IID_TextBlock6), 6, HRESULT (WINAPI *)(obj *, boolean *))(as(t, &IID_TextBlock6), &before);   /* IsTextTrimmed */
    CALL(as(t, &IID_TextBlock), 23, HRESULT (WINAPI *)(obj *, INT32))(as(t, &IID_TextBlock), 1);   /* CharacterEllipsis */
    CALL(as(t, &IID_TextBlock), 22, HRESULT (WINAPI *)(obj *, INT32 *))(as(t, &IID_TextBlock), &trimming);
    trimmed = measured(t);
    CALL(as(t, &IID_TextBlock6), 6, HRESULT (WINAPI *)(obj *, boolean *))(as(t, &IID_TextBlock6), &after);
    printf("trimming=%d trimmed=%d,%d within=%d\n", trimming, before, after, trimmed <= 60.5f && trimmed > 10);
    CALL(as(t, &IID_TextBlock6), 9, get_fn)(as(t, &IID_TextBlock6), &highlighters);
    if (highlighters) CALL(highlighters, 7, HRESULT (WINAPI *)(obj *, UINT32 *))(highlighters, &count);
    printf("highlighters=%d,%u\n", highlighters != NULL, count);

    /* Shadow: the caller keeps its reference; null clears it */
    shadow = make(L"Windows.UI.Xaml.Media.ThemeShadow");
    if (shadow)
    {
        ULONG before = refs(shadow), after;
        obj *u10 = as(b, &IID_UIE10);
        CALL(u10, 12, putobj_fn)(u10, shadow);
        after = refs(shadow);
        hr = CALL(u10, 12, putobj_fn)(u10, NULL);
        got = (obj *)1;
        CALL(u10, 11, get_fn)(u10, &got);
        printf("shadow=%d,%08lx,%d\n", after == before + 1, hr, got == NULL);
    }

    /* the clip drawn: the 100x100 red border shows only in its top left 50x50 */
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    hdc = GetDC(island);
    printf("clip_drawn=%d,%d\n", red(GetPixel(hdc, 25, 25)), !red(GetPixel(hdc, 75, 75)));
    ReleaseDC(island, hdc);
    printf("done=1\n");
    return 0;
}
