/* Windows.UI.Xaml panels (patches/sg/6134): Panel.Children (UIElementCollection), Grid with Auto,
 * pixel and star rows and columns, spacing and Grid.Row/Column, StackPanel, Border, as an
 * application builds them in code. Interface identity per windows-rs 0.30.0 (MIT OR Apache-2.0);
 * collection IIDs from their type signatures. Prints KEY=VALUE. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(IID_FEFactory,    0xdeaee126, 0x03ca, 0x4966, 0xb5, 0x76, 0x60, 0x4c, 0xce, 0x93, 0xb5, 0xe8);
DEFINE_GUID(IID_FE,           0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_UIE,          0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_GridFactory,  0xae814041, 0xc531, 0x43b4, 0xbf, 0x99, 0x12, 0xf5, 0x06, 0xf7, 0xb0, 0x1c);
DEFINE_GUID(IID_GridStatics,  0x64fe2e9f, 0xf951, 0x42b6, 0xa9, 0xce, 0xbb, 0x17, 0x9a, 0xf1, 0x15, 0x95);
DEFINE_GUID(IID_Grid,         0xfd104460, 0x2e15, 0x4ba3, 0x8b, 0x8f, 0xfa, 0x69, 0x3a, 0x41, 0x61, 0xe9);
DEFINE_GUID(IID_Grid3,        0x12dfc5fc, 0x2342, 0x4dd2, 0x9e, 0x7d, 0x20, 0x90, 0xa1, 0x71, 0xd1, 0xef);
DEFINE_GUID(IID_Panel,        0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_RowDef,       0x4abae829, 0xd80c, 0x4a5e, 0xa4, 0x8c, 0xf8, 0xb3, 0xd3, 0xb6, 0x53, 0x3d);
DEFINE_GUID(IID_ColDef,       0xf7f1b229, 0xf024, 0x467f, 0x97, 0x0a, 0x7e, 0x70, 0x56, 0x15, 0xdb, 0x7b);
DEFINE_GUID(IID_StackFactory, 0x63d8248a, 0x8b34, 0x445a, 0x80, 0x8f, 0xb6, 0xec, 0xd6, 0x2a, 0x27, 0xd9);
DEFINE_GUID(IID_Stack,        0xb8ae8fe2, 0xd641, 0x4fd7, 0x80, 0xb4, 0x74, 0x39, 0x20, 0x7d, 0x27, 0x98);
DEFINE_GUID(IID_Stack4,       0x43ebf7f6, 0x3196, 0x412e, 0x8a, 0x95, 0xad, 0xd0, 0x02, 0xff, 0x43, 0xf0);
DEFINE_GUID(IID_Border,       0x797c4539, 0x45bd, 0x4633, 0xa0, 0x44, 0xbf, 0xb0, 0x2e, 0xf5, 0x17, 0x0f);
DEFINE_GUID(IID_VecUIE,       0xb4c1e3ac, 0x8768, 0x5b9d, 0xa6, 0x61, 0xf6, 0x33, 0x30, 0xb8, 0x50, 0x7b);
DEFINE_GUID(IID_IterUIE,      0x42e26ae1, 0xd357, 0x57e8, 0xbb, 0x48, 0xf7, 0x5c, 0x9f, 0xf6, 0x9d, 0x91);
DEFINE_GUID(IID_VecRow,       0x62225981, 0x7a0d, 0x5d38, 0x80, 0x88, 0x7c, 0x6b, 0xb6, 0xd9, 0x59, 0xbe);
DEFINE_GUID(IID_VecCol,       0x6e137874, 0xdd39, 0x5b4f, 0xa7, 0xec, 0x2e, 0xa6, 0x9e, 0x4c, 0x22, 0x29);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
struct size { float w, h; };
struct rect { float x, y, w, h; };
struct thickness { double l, t, r, b; };
struct gridlength { double value; INT32 unit; };
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *act_fn)(obj *, obj **);
typedef HRESULT (WINAPI *measure_fn)(obj *, struct size);
typedef HRESULT (WINAPI *arrange_fn)(obj *, struct rect);
typedef HRESULT (WINAPI *getsize_fn)(obj *, struct size *);
typedef HRESULT (WINAPI *putd_fn)(obj *, double);
typedef HRESULT (WINAPI *getd_fn)(obj *, double *);
typedef HRESULT (WINAPI *puti_fn)(obj *, INT32);
typedef HRESULT (WINAPI *putth_fn)(obj *, struct thickness);
typedef HRESULT (WINAPI *putgl_fn)(obj *, struct gridlength);
typedef HRESULT (WINAPI *append_fn)(obj *, obj *);
typedef HRESULT (WINAPI *size_fn)(obj *, UINT32 *);
typedef HRESULT (WINAPI *getat_fn)(obj *, UINT32, obj **);
typedef HRESULT (WINAPI *indexof_fn)(obj *, obj *, UINT32 *, boolean *);
typedef HRESULT (WINAPI *removeat_fn)(obj *, UINT32);
typedef HRESULT (WINAPI *setattached_fn)(obj *, obj *, INT32);
typedef HRESULT (WINAPI *getattached_fn)(obj *, obj *, INT32 *);

enum { UIE_DesiredSize = 6, UIE_Measure = 91, UIE_Arrange = 92 };
enum { FE_ActualWidth = 13, FE_ActualHeight = 14, FE_SetWidth = 16, FE_SetHeight = 18, FE_Parent = 40 };
enum { VEC_GetAt = 6, VEC_Size = 7, VEC_IndexOf = 9, VEC_RemoveAt = 12, VEC_Append = 13 };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static obj *fef, *gridf, *gstat, *stackf;

/* a plain element of a fixed size (0: auto) */
static obj *box(double w, double h)
{
    IInspectable *inner = NULL;
    obj *fe = NULL;
    ((create_fn)SLOT(fef, 6))(fef, NULL, &inner, &fe);
    if (w) ((putd_fn)SLOT(fe, FE_SetWidth))(fe, w);
    if (h) ((putd_fn)SLOT(fe, FE_SetHeight))(fe, h);
    return fe;
}
static obj *children(obj *panel)
{
    obj *p = as(panel, &IID_Panel), *c = NULL;
    ((get_fn)SLOT(p, 6))(p, &c);
    return c;
}
static HRESULT add(obj *panel, obj *child) { obj *c = children(panel); return ((append_fn)SLOT(c, VEC_Append))(c, as(child, &IID_UIE)); }
static void layout(obj *o, float w, float h)
{
    obj *ui = as(o, &IID_UIE);
    struct size avail = { w, h };
    struct rect final = { 0, 0, w, h };
    ((measure_fn)SLOT(ui, UIE_Measure))(ui, avail);
    ((arrange_fn)SLOT(ui, UIE_Arrange))(ui, final);
}
static struct size desired(obj *o) { obj *ui = as(o, &IID_UIE); struct size s = { -1, -1 }; ((getsize_fn)SLOT(ui, UIE_DesiredSize))(ui, &s); return s; }
static double aw(obj *o) { obj *fe = as(o, &IID_FE); double v = -1; ((getd_fn)SLOT(fe, FE_ActualWidth))(fe, &v); return v; }
static double ah(obj *o) { obj *fe = as(o, &IID_FE); double v = -1; ((getd_fn)SLOT(fe, FE_ActualHeight))(fe, &v); return v; }
static obj *new_def(const WCHAR *cls, REFIID iid, double value, INT32 unit)
{
    obj *f = factory(cls, &IID_IActivationFactory), *d = NULL;
    struct gridlength gl = { value, unit };
    ((act_fn)SLOT(f, 6))(f, &d);
    d = as(d, iid);
    ((putgl_fn)SLOT(d, 7))(d, gl);
    return d;
}
static void place(obj *child, INT32 row, INT32 col)
{
    ((setattached_fn)SLOT(gstat, 8))(gstat, as(child, &IID_UIE), row);      /* SetRow */
    ((setattached_fn)SLOT(gstat, 11))(gstat, as(child, &IID_UIE), col);     /* SetColumn */
}
static obj *new_grid(void)
{
    IInspectable *inner = NULL;
    obj *g = NULL;
    ((create_fn)SLOT(gridf, 6))(gridf, NULL, &inner, &g);
    return as(g, &IID_Grid);
}

int main(void)
{
    obj *grid, *rows = NULL, *cols = NULL, *r0, *r1, *c0, *c1, *c2, *a, *b, *c, *stack, *border, *kids, *got = NULL;
    double v0 = 0, v1 = 0, v2 = 0;
    UINT32 n = 0, idx = 9;
    boolean found = 0;
    INT32 col = -1;
    struct size d;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(fef = factory(L"Windows.UI.Xaml.FrameworkElement", &IID_FEFactory))) return 1;
    if (!(gridf = factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory))) return 1;
    if (!(gstat = factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridStatics))) return 1;
    if (!(stackf = factory(L"Windows.UI.Xaml.Controls.StackPanel", &IID_StackFactory))) return 1;

    /* a grid: columns Auto, *, 2*; rows 40, * */
    grid = new_grid();
    ((get_fn)SLOT(grid, 6))(grid, &rows);
    ((get_fn)SLOT(grid, 7))(grid, &cols);
    r0 = new_def(L"Windows.UI.Xaml.Controls.RowDefinition", &IID_RowDef, 40, 1);
    r1 = new_def(L"Windows.UI.Xaml.Controls.RowDefinition", &IID_RowDef, 1, 2);
    c0 = new_def(L"Windows.UI.Xaml.Controls.ColumnDefinition", &IID_ColDef, 1, 0);
    c1 = new_def(L"Windows.UI.Xaml.Controls.ColumnDefinition", &IID_ColDef, 1, 2);
    c2 = new_def(L"Windows.UI.Xaml.Controls.ColumnDefinition", &IID_ColDef, 2, 2);
    ((append_fn)SLOT(rows, VEC_Append))(rows, r0);
    ((append_fn)SLOT(rows, VEC_Append))(rows, r1);
    ((append_fn)SLOT(cols, VEC_Append))(cols, c0);
    ((append_fn)SLOT(cols, VEC_Append))(cols, c1);
    ((append_fn)SLOT(cols, VEC_Append))(cols, c2);
    a = box(60, 30); b = box(0, 0); c = box(0, 0);
    place(a, 0, 0); place(b, 1, 1); place(c, 1, 2);
    add(grid, a); add(grid, b); add(grid, c);
    ((getattached_fn)SLOT(gstat, 10))(gstat, as(c, &IID_UIE), &col);         /* GetColumn */
    printf("getcolumn=%d\n", col);
    layout(grid, 360, 240);
    ((getd_fn)SLOT(c0, 12))(c0, &v0); ((getd_fn)SLOT(c1, 12))(c1, &v1); ((getd_fn)SLOT(c2, 12))(c2, &v2);
    printf("columns=%.0f,%.0f,%.0f\n", v0, v1, v2);
    ((getd_fn)SLOT(r0, 12))(r0, &v0); ((getd_fn)SLOT(r1, 12))(r1, &v1);
    printf("rows=%.0f,%.0f\n", v0, v1);
    printf("cells=%.0fx%.0f,%.0fx%.0f,%.0fx%.0f\n", aw(a), ah(a), aw(b), ah(b), aw(c), ah(c));
    /* unbounded: the stars size to their content */
    layout(grid, INFINITY, INFINITY);
    d = desired(grid);
    printf("unbounded=%.0fx%.0f\n", d.w, d.h);
    /* spacing takes from the stars */
    {
        obj *g3 = as(grid, &IID_Grid3);
        ((putd_fn)SLOT(g3, 9))(g3, 30);                                       /* SetColumnSpacing */
        layout(grid, 360, 240);
        ((getd_fn)SLOT(c1, 12))(c1, &v1); ((getd_fn)SLOT(c2, 12))(c2, &v2);
        printf("spacing=%.0f,%.0f\n", v1, v2);
    }

    /* the collection */
    kids = children(grid);
    ((size_fn)SLOT(kids, VEC_Size))(kids, &n);
    ((getat_fn)SLOT(kids, VEC_GetAt))(kids, 1, &got);
    ((indexof_fn)SLOT(kids, VEC_IndexOf))(kids, as(c, &IID_UIE), &idx, &found);
    printf("children=%u getat=%d indexof=%u,%d iterable=%d\n", n, as(got, &IID_IUnknown) == as(b, &IID_IUnknown), idx, found,
           as(kids, &IID_IterUIE) != NULL);
    {
        obj *parent = NULL, *fe = as(a, &IID_FE);
        ((get_fn)SLOT(fe, FE_Parent))(fe, &parent);
        printf("parent=%d\n", as(parent, &IID_IUnknown) == as(grid, &IID_IUnknown));
    }
    printf("second_parent=%08lx\n", add(new_grid(), a));
    ((removeat_fn)SLOT(kids, VEC_RemoveAt))(kids, 0);
    ((size_fn)SLOT(kids, VEC_Size))(kids, &n);
    printf("removed=%u readd=%08lx\n", n, add(new_grid(), a));

    /* StackPanel: vertical with spacing, then horizontal */
    {
        IInspectable *inner = NULL;
        obj *s4;
        ((create_fn)SLOT(stackf, 6))(stackf, NULL, &inner, &stack);
        stack = as(stack, &IID_Stack);
        s4 = as(stack, &IID_Stack4);
        ((putd_fn)SLOT(s4, 7))(s4, 5);                                       /* SetSpacing */
        add(stack, box(50, 20)); add(stack, box(70, 30));
        layout(stack, INFINITY, INFINITY);
        d = desired(stack);
        printf("vertical=%.0fx%.0f\n", d.w, d.h);
        ((puti_fn)SLOT(stack, 9))(stack, 1);                                  /* SetOrientation Horizontal */
        layout(stack, INFINITY, INFINITY);
        d = desired(stack);
        printf("horizontal=%.0fx%.0f\n", d.w, d.h);
    }

    /* Border: thickness and padding around its child */
    {
        obj *bf = factory(L"Windows.UI.Xaml.Controls.Border", &IID_IActivationFactory), *child = box(10, 10);
        struct thickness two = { 2, 2, 2, 2 }, three = { 3, 3, 3, 3 };
        ((act_fn)SLOT(bf, 6))(bf, &border);
        border = as(border, &IID_Border);
        ((putth_fn)SLOT(border, 9))(border, two);                              /* SetBorderThickness */
        ((putth_fn)SLOT(border, 15))(border, three);                           /* SetPadding */
        ((append_fn)SLOT(border, 17))(border, as(child, &IID_UIE));            /* SetChild */
        layout(border, 100, 100);
        d = desired(border);
        printf("border=%.0fx%.0f child=%.0fx%.0f\n", d.w, d.h, aw(child), ah(child));
    }
    return 0;
}
