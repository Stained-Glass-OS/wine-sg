/* Windows.UI.Composition shapes drawn in Windows.UI.Xaml (patches/sg/6265): a ShapeVisual set as an element's
 * child visual (ElementCompositionPreview) draws its sprite shapes -- a rectangle, an ellipse in a container
 * shape moved by its Offset, a path over the program's own geometry source (CompositionPath over an
 * IGeometrySource2DInterop); an animation paused through its AnimationController (ICompositionObject4) takes
 * the progress it is given; a boolean key frame animation (ICompositor6) hides a visual. What Lottie's
 * animated visuals (WinUI 2's AnimatedIcon: Windows Terminal's new-tab chevron) are made of.
 * Interface identity per windows-rs 0.52.0 (MIT OR Apache-2.0). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <d2d1.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_EcpStatics,     0x08c92b38, 0xec99, 0x4c55, 0xbc, 0x85, 0xa1, 0xc1, 0x80, 0xb2, 0x76, 0x46);
DEFINE_GUID(IID_CompObject,     0xbcb4ad45, 0x7609, 0x4550, 0x93, 0x4f, 0x16, 0x00, 0x2a, 0x68, 0xfd, 0xed);
DEFINE_GUID(IID_CompObject4,    0x0bb3784c, 0x346b, 0x4a7c, 0x96, 0x6b, 0x73, 0x10, 0x96, 0x65, 0x53, 0xd5);
DEFINE_GUID(IID_Compositor,     0xb403ca50, 0x7f8c, 0x4e83, 0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8);
DEFINE_GUID(IID_Compositor5,    0x48ea31ad, 0x7fcd, 0x4076, 0xa7, 0x9c, 0x90, 0xcc, 0x4b, 0x85, 0x2c, 0x9b);
DEFINE_GUID(IID_Compositor6,    0x7a38b2bd, 0xcec8, 0x4eeb, 0x83, 0x0f, 0xd8, 0xd0, 0x7a, 0xed, 0xeb, 0xc3);
DEFINE_GUID(IID_Visual,         0x117e202d, 0xa859, 0x4c89, 0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3);
DEFINE_GUID(IID_ShapeVisual,    0xf2bd13c3, 0xba7e, 0x4b0f, 0x91, 0x26, 0xff, 0xb7, 0x53, 0x6b, 0x81, 0x76);
DEFINE_GUID(IID_Shape,          0xb47ce2f7, 0x9a88, 0x42c4, 0x9e, 0x87, 0x2e, 0x50, 0x0c, 0xa8, 0x68, 0x8c);
DEFINE_GUID(IID_ContainerShape, 0x4f5e859b, 0x2e5b, 0x44a8, 0x98, 0x2c, 0xaa, 0x0f, 0x69, 0xc1, 0x60, 0x59);
DEFINE_GUID(IID_SpriteShape,    0x401b61bb, 0x0007, 0x4363, 0xb1, 0xf3, 0x6b, 0xcc, 0x00, 0x3f, 0xb8, 0x3e);
DEFINE_GUID(IID_RectGeometry,   0x0cd51428, 0x5356, 0x4246, 0xae, 0xcf, 0x7a, 0x0b, 0x76, 0x97, 0x54, 0x00);
DEFINE_GUID(IID_EllipseGeometry, 0x4801f884, 0xf6ad, 0x4b93, 0xaf, 0xa9, 0x89, 0x7b, 0x64, 0xe5, 0x7b, 0x1f);
DEFINE_GUID(IID_PathFactory,    0x9c1e8c6a, 0x0f33, 0x4751, 0x94, 0x37, 0xeb, 0x3f, 0xb9, 0xd3, 0xab, 0x07);
DEFINE_GUID(IID_Interop,        0x0657af73, 0x53fd, 0x47cf, 0x84, 0xff, 0xc8, 0x49, 0x2d, 0x2a, 0x80, 0xa3);
DEFINE_GUID(IID_KeyFrame,       0x126e7f22, 0x3ae9, 0x4540, 0x9a, 0x8a, 0xde, 0xae, 0x8a, 0x4a, 0x4a, 0x84);
DEFINE_GUID(IID_Animation,      0x464c4c2c, 0x1caa, 0x4061, 0x9b, 0x40, 0xe1, 0x3f, 0xde, 0x15, 0x03, 0xca);
DEFINE_GUID(IID_BoolKeyFrame,   0x95e23a08, 0xd1f4, 0x4972, 0x97, 0x70, 0x3e, 0xfe, 0x68, 0xd8, 0x2e, 0x14);
DEFINE_GUID(IID_FEOverrides,    0xda007e54, 0xb3c2, 0x4b9a, 0xaa, 0x8e, 0xd3, 0xf0, 0x71, 0x26, 0x2b, 0x97);
DEFINE_GUID(IID_VTHStatics,     0xe75758c4, 0xd25d, 0x4b1d, 0x97, 0x1f, 0x59, 0x6f, 0x17, 0xf1, 0x2b, 0xaa);
DEFINE_GUID(IID_PathIconFactory, 0xaf9d9652, 0x9c5d, 0x4a37, 0x9e, 0x1a, 0x04, 0x4a, 0xbe, 0xef, 0x79, 0x2b);
DEFINE_GUID(IID_Panel,          0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_AppFactory,     0x93bbe361, 0xbe5a, 0x4ee3, 0xb4, 0xa3, 0x95, 0x11, 0x8d, 0xc9, 0x7a, 0x89);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) (((obj *)(o))->vtbl[n])
typedef struct { float X, Y; } vec2;
typedef struct { BYTE A, R, G, B; } color;
typedef struct { INT64 Duration; } span100;

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((HRESULT (WINAPI *)(obj *, REFIID, void **))SLOT(o, 0))(o, iid, (void **)&r); return r; }
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

/* the program's geometry source: a triangle, as a Direct2D path geometry */
static ID2D1Factory *d2d;
static HRESULT WINAPI src_qi(obj *self, REFIID iid, void **out)
{
    *out = self;   /* IUnknown, IInspectable, IGeometrySource2D, IGeometrySource2DInterop: one object */
    return S_OK;
}
static ULONG WINAPI src_addref(obj *self) { return 2; }
static ULONG WINAPI src_release(obj *self) { return 1; }
static HRESULT WINAPI src_get_geometry(obj *self, ID2D1Geometry **out)
{
    ID2D1PathGeometry *path = NULL;
    ID2D1GeometrySink *sink;
    D2D1_POINT_2F p0 = { 60, 40 }, p1 = { 90, 40 }, p2 = { 75, 10 };

    if (FAILED(ID2D1Factory_CreatePathGeometry(d2d, &path))) return E_FAIL;
    ID2D1PathGeometry_Open(path, &sink);
    ID2D1GeometrySink_BeginFigure(sink, p0, D2D1_FIGURE_BEGIN_FILLED);
    ID2D1GeometrySink_AddLine(sink, p1);
    ID2D1GeometrySink_AddLine(sink, p2);
    ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
    ID2D1GeometrySink_Close(sink);
    ID2D1GeometrySink_Release(sink);
    *out = (ID2D1Geometry *)path;
    return S_OK;
}
static HRESULT WINAPI src_try_factory(obj *self, void *factory, ID2D1Geometry **out) { *out = NULL; return E_NOTIMPL; }
static void *src_vtbl[] = { src_qi, src_addref, src_release, src_get_geometry, src_try_factory };
static obj source = { src_vtbl };

static obj *comp, *comp5;

static obj *brush(BYTE r, BYTE g, BYTE b)
{
    color c = { 255, r, g, b };
    obj *br = NULL;
    ((HRESULT (WINAPI *)(obj *, color, obj **))SLOT(comp, 8))(comp, c, &br);
    return br;
}
static obj *sprite(obj *geometry, obj *fill)
{
    obj *s = NULL;
    ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(comp5, 23))(comp5, geometry, &s);   /* CreateSpriteShapeWithGeometry */
    if (s) ((HRESULT (WINAPI *)(obj *, obj *))SLOT(as(s, &IID_SpriteShape), 7))(as(s, &IID_SpriteShape), fill);
    return s;
}
static void append(obj *collection, obj *shape)
{
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(collection, 13))(collection, as(shape, &IID_Shape));
}
static int close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 60 && abs(GetGValue(c) - g) < 60 && abs(GetBValue(c) - b) < 60;
}

/* a program's icon over PathIcon (as WinUI 2's AnimatedIcon): its own OnApplyTemplate, the rest PathIcon's */
static IInspectable *icon_inner;
static int icon_template_told;
static obj icon_outer, icon_overrides;
static HRESULT WINAPI outer_qi(obj *self, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)) { *out = &icon_outer; return S_OK; }
    if (IsEqualGUID(iid, &IID_FEOverrides)) { *out = &icon_overrides; return S_OK; }
    return IInspectable_QueryInterface(icon_inner, iid, out);
}
static ULONG WINAPI outer_addref(obj *self) { return 2; }
static ULONG WINAPI outer_release(obj *self) { return 1; }
static HRESULT WINAPI outer_iids(obj *self, ULONG *n, IID **iids) { *n = 0; *iids = NULL; return S_OK; }
static HRESULT WINAPI outer_class(obj *self, HSTRING *name) { return WindowsCreateString(L"SgTest.Icon", 11, name); }
static HRESULT WINAPI outer_trust(obj *self, TrustLevel *t) { *t = BaseTrust; return S_OK; }
static obj *inner_overrides(void)
{
    obj *o = NULL;
    IInspectable_QueryInterface(icon_inner, &IID_FEOverrides, (void **)&o);
    return o;
}
static HRESULT WINAPI ovr_measure(obj *self, vec2 size, vec2 *out)
{ obj *o = inner_overrides(); return ((HRESULT (WINAPI *)(obj *, vec2, vec2 *))SLOT(o, 6))(o, size, out); }
static HRESULT WINAPI ovr_arrange(obj *self, vec2 size, vec2 *out)
{ obj *o = inner_overrides(); return ((HRESULT (WINAPI *)(obj *, vec2, vec2 *))SLOT(o, 7))(o, size, out); }
static HRESULT WINAPI ovr_apply(obj *self) { icon_template_told++; return S_OK; }
static void *outer_vtbl[] = { outer_qi, outer_addref, outer_release, outer_iids, outer_class, outer_trust };
static void *overrides_vtbl[] = { outer_qi, outer_addref, outer_release, outer_iids, outer_class, outer_trust,
                                  ovr_measure, ovr_arrange, ovr_apply };
static obj icon_outer = { outer_vtbl }, icon_overrides = { overrides_vtbl };

/* the island's paints, counted */
static WNDPROC island_proc;
static LONG paints;
static LRESULT CALLBACK counting_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_PAINT) InterlockedIncrement(&paints);
    return CallWindowProcW(island_proc, hwnd, msg, wparam, lparam);
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

int main(void)
{
    static const WCHAR page[] = L"<Grid " NS L" Background=\"White\"><Border x:Name=\"Host\" Width=\"100\" Height=\"100\" "
                                L"HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/></Grid>";
    obj *appf, *app = NULL, *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *ecp, *host = NULL;
    obj *visual = NULL, *sv = NULL, *shapes = NULL, *rect = NULL, *ellipse = NULL, *container = NULL, *cshapes = NULL;
    obj *pathf, *path = NULL, *pgeom = NULL, *rsprite, *anim = NULL, *controller = NULL, *comp6, *banim = NULL, *bcontroller = NULL;
    IInspectable *inner = NULL;
    HWND hwnd, island = NULL;
    vec2 v;
    HDC dc;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&d2d);
    if (!(appf = factory(L"Windows.UI.Xaml.Application", &IID_AppFactory))) return 1;
    ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(appf, 6))(appf, NULL, &inner, &app);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(rs, 6))(rs, hs(page), &grid);
    if (!grid) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(grid, &IID_FE), 51))(as(grid, &IID_FE), hs(L"Host"), &host);   /* FindName */

    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    hwnd = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 200, 120, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, hwnd);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 200, 120, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(src, 7))(src, as(grid, &IID_UIE));

    /* the compositor of the element's visual */
    ecp = factory(L"Windows.UI.Xaml.Hosting.ElementCompositionPreview", &IID_EcpStatics);
    ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(ecp, 6))(ecp, as(host, &IID_UIE), &visual);
    if (visual) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(as(visual, &IID_CompObject), 6))(as(visual, &IID_CompObject), &comp);
    comp = as(comp, &IID_Compositor);
    comp5 = as(comp, &IID_Compositor5);
    printf("compositor=%d,%d\n", comp != NULL, comp5 != NULL);
    if (!comp5) return 1;

    /* a ShapeVisual 100x100 */
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp5, 21))(comp5, &sv);
    printf("shape_visual=%d\n", sv != NULL && as(sv, &IID_Visual) != NULL);
    if (!sv) return 1;
    v.X = 100; v.Y = 100;
    ((HRESULT (WINAPI *)(obj *, vec2))SLOT(as(sv, &IID_Visual), 36))(as(sv, &IID_Visual), v);
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(as(sv, &IID_ShapeVisual), 6))(as(sv, &IID_ShapeVisual), &shapes);

    /* a red 40x40 rectangle at 10,10 */
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp5, 19))(comp5, &rect);
    v.X = 10; v.Y = 10;
    ((HRESULT (WINAPI *)(obj *, vec2))SLOT(as(rect, &IID_RectGeometry), 7))(as(rect, &IID_RectGeometry), v);
    v.X = 40; v.Y = 40;
    ((HRESULT (WINAPI *)(obj *, vec2))SLOT(as(rect, &IID_RectGeometry), 9))(as(rect, &IID_RectGeometry), v);
    rsprite = sprite(rect, brush(255, 0, 0));
    append(shapes, rsprite);

    /* a blue triangle: a path over the program's geometry */
    pathf = factory(L"Windows.UI.Composition.CompositionPath", &IID_PathFactory);
    if (pathf) ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(pathf, 6))(pathf, &source, &path);
    if (path) ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(comp5, 17))(comp5, path, &pgeom);   /* CreatePathGeometryWithPath */
    printf("path=%d\n", pgeom != NULL);
    if (pgeom) append(shapes, sprite(pgeom, brush(0, 0, 255)));

    /* a green ellipse at 25,75 in a container moved 50 to the right */
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp5, 14))(comp5, &ellipse);
    v.X = 25; v.Y = 75;
    ((HRESULT (WINAPI *)(obj *, vec2))SLOT(as(ellipse, &IID_EllipseGeometry), 7))(as(ellipse, &IID_EllipseGeometry), v);
    v.X = 10; v.Y = 10;
    ((HRESULT (WINAPI *)(obj *, vec2))SLOT(as(ellipse, &IID_EllipseGeometry), 9))(as(ellipse, &IID_EllipseGeometry), v);
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp5, 13))(comp5, &container);
    v.X = 50; v.Y = 0;
    ((HRESULT (WINAPI *)(obj *, vec2))SLOT(as(container, &IID_Shape), 9))(as(container, &IID_Shape), v);
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(as(container, &IID_ContainerShape), 6))(as(container, &IID_ContainerShape), &cshapes);
    append(cshapes, sprite(ellipse, brush(0, 160, 0)));
    append(shapes, container);

    ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(ecp, 8))(ecp, as(host, &IID_UIE), as(sv, &IID_Visual));
    pump(800); UpdateWindow(island); pump(300);
    dc = GetDC(island);
    printf("drawn=%d%d%d%d%d\n", close_to(GetPixel(dc, 30, 30), 255, 0, 0), close_to(GetPixel(dc, 75, 32), 0, 0, 255),
           close_to(GetPixel(dc, 75, 75), 0, 160, 0), close_to(GetPixel(dc, 25, 75), 255, 255, 255),
           close_to(GetPixel(dc, 150, 60), 255, 255, 255));
    ReleaseDC(island, dc);

    /* the rectangle's Offset.Y animated 0 to 50, paused at its end through its controller */
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp, 20))(comp, &anim);
    if (anim)
    {
        span100 ten = { 100000000 };
        ((HRESULT (WINAPI *)(obj *, float, float))SLOT(anim, 6))(anim, 0.0f, 0.0f);
        ((HRESULT (WINAPI *)(obj *, float, float))SLOT(anim, 6))(anim, 1.0f, 50.0f);
        ((HRESULT (WINAPI *)(obj *, span100))SLOT(as(anim, &IID_KeyFrame), 9))(as(anim, &IID_KeyFrame), ten);
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj *))SLOT(as(rsprite, &IID_CompObject), 9))(as(rsprite, &IID_CompObject),
                hs(L"Offset.Y"), as(anim, &IID_Animation));
        if (as(rsprite, &IID_CompObject4))
            ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(rsprite, &IID_CompObject4), 6))(as(rsprite, &IID_CompObject4),
                    hs(L"Offset.Y"), &controller);
    }
    printf("controller=%d\n", controller != NULL);
    if (controller)
    {
        ((HRESULT (WINAPI *)(obj *))SLOT(controller, 12))(controller);                       /* Pause */
        ((HRESULT (WINAPI *)(obj *, float))SLOT(controller, 9))(controller, 1.0f);          /* Progress */
    }
    pump(800); UpdateWindow(island); pump(300);
    dc = GetDC(island);
    printf("progress=%d%d\n", close_to(GetPixel(dc, 30, 75), 255, 0, 0), close_to(GetPixel(dc, 30, 30), 255, 255, 255));
    ReleaseDC(island, dc);

    /* IsVisible key frames: true, then false from half way; the visual at three quarters */
    comp6 = as(comp, &IID_Compositor6);
    if (comp6) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp6, 10))(comp6, &banim);
    if (banim)
    {
        span100 ten = { 100000000 };
        obj *b = as(banim, &IID_BoolKeyFrame);
        ((HRESULT (WINAPI *)(obj *, float, boolean))SLOT(b, 6))(b, 0.0f, TRUE);
        ((HRESULT (WINAPI *)(obj *, float, boolean))SLOT(b, 6))(b, 0.5f, FALSE);
        ((HRESULT (WINAPI *)(obj *, span100))SLOT(as(banim, &IID_KeyFrame), 9))(as(banim, &IID_KeyFrame), ten);
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj *))SLOT(as(sv, &IID_CompObject), 9))(as(sv, &IID_CompObject),
                hs(L"IsVisible"), as(banim, &IID_Animation));
        if (as(sv, &IID_CompObject4))
            ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(sv, &IID_CompObject4), 6))(as(sv, &IID_CompObject4),
                    hs(L"IsVisible"), &bcontroller);
    }
    if (bcontroller)
    {
        ((HRESULT (WINAPI *)(obj *))SLOT(bcontroller, 12))(bcontroller);
        ((HRESULT (WINAPI *)(obj *, float))SLOT(bcontroller, 9))(bcontroller, 0.75f);
    }
    pump(800); UpdateWindow(island); pump(300);
    dc = GetDC(island);
    printf("hidden=%d,%d\n", banim != NULL, close_to(GetPixel(dc, 30, 75), 255, 255, 255) && close_to(GetPixel(dc, 75, 75), 255, 255, 255));
    ReleaseDC(island, dc);

    /* a program's icon over PathIcon: a Panel is its first visual child (with a placeholder in it), its
     * OnApplyTemplate is called when it is first measured */
    {
        obj *pif = factory(L"Windows.UI.Xaml.Controls.PathIcon", &IID_PathIconFactory), *icon = NULL, *vth, *child = NULL;
        obj *children = NULL;
        INT32 count = -1, inner_count = -1;
        UINT32 size = 0;
        vec2 avail = { 50, 50 };

        if (pif) ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(pif, 6))(pif,
                (IInspectable *)&icon_outer, &icon_inner, &icon);
        vth = factory(L"Windows.UI.Xaml.Media.VisualTreeHelper", &IID_VTHStatics);
        if (icon && vth)
        {
            ((HRESULT (WINAPI *)(obj *, obj *, INT32 *))SLOT(vth, 11))(vth, as(icon, &IID_UIE), &count);
            ((HRESULT (WINAPI *)(obj *, obj *, INT32, obj **))SLOT(vth, 10))(vth, as(icon, &IID_UIE), 0, &child);
            if (child && as(child, &IID_Panel))
            {
                ((HRESULT (WINAPI *)(obj *, obj **))SLOT(as(child, &IID_Panel), 6))(as(child, &IID_Panel), &children);
                if (children) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(children, 7))(children, &size);
            }
            /* measured: its template is applied */
            ((HRESULT (WINAPI *)(obj *, vec2))SLOT(as(icon, &IID_UIE), 91))(as(icon, &IID_UIE), avail);
        }
        (void)inner_count;
        printf("icon=%d,%d,%d,%u,%d\n", icon != NULL, count, child && as(child, &IID_Panel) != NULL, size, icon_template_told);
    }

    /* a visual drawn nowhere, animated without end: the island does not draw again for it */
    {
        obj *other = NULL, *spin = NULL;
        span100 one = { 10000000 };

        ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp5, 21))(comp5, &other);
        ((HRESULT (WINAPI *)(obj *, obj **))SLOT(comp, 20))(comp, &spin);
        if (other && spin)
        {
            ((HRESULT (WINAPI *)(obj *, float, float))SLOT(spin, 6))(spin, 0.0f, 0.0f);
            ((HRESULT (WINAPI *)(obj *, float, float))SLOT(spin, 6))(spin, 1.0f, 360.0f);
            ((HRESULT (WINAPI *)(obj *, span100))SLOT(as(spin, &IID_KeyFrame), 9))(as(spin, &IID_KeyFrame), one);
            ((HRESULT (WINAPI *)(obj *, INT32))SLOT(as(spin, &IID_KeyFrame), 11))(as(spin, &IID_KeyFrame), 1);   /* Forever */
            pump(500);
            island_proc = (WNDPROC)SetWindowLongPtrW(island, GWLP_WNDPROC, (LONG_PTR)counting_proc);
            ((HRESULT (WINAPI *)(obj *, HSTRING, obj *))SLOT(as(other, &IID_CompObject), 9))(as(other, &IID_CompObject),
                    hs(L"RotationAngleInDegrees"), as(spin, &IID_Animation));
            pump(1500);
        }
        printf("quiet=%d,%ld\n", other && spin, paints);
    }
    return 0;
}
