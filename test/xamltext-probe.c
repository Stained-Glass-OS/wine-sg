/* Windows.UI.Xaml text and media (patches/sg/6135): TextBlock measured with DirectWrite (Text,
 * FontSize, TextWrapping, Padding), SolidColorBrush (Color, Opacity), FontFamily, as an application
 * sets them in code. Interface identity per windows-rs 0.30.0 (MIT OR Apache-2.0). KEY=VALUE. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(IID_UIE,          0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_TB,           0xae2d9271, 0x3b4a, 0x45fc, 0x84, 0x68, 0xf7, 0x94, 0x95, 0x48, 0xf4, 0xd5);
DEFINE_GUID(IID_TBStatics,    0x193ec042, 0xa847, 0x4552, 0xb4, 0xe8, 0xdb, 0x36, 0x65, 0x5b, 0x84, 0x0a);
DEFINE_GUID(IID_SCBFactory,   0xd935ce0c, 0x86f5, 0x4da6, 0x8a, 0x27, 0xb1, 0x61, 0x9e, 0xf7, 0xf9, 0x2b);
DEFINE_GUID(IID_SCB,          0x9d850850, 0x66f3, 0x48df, 0x9a, 0x8f, 0x82, 0x4b, 0xd5, 0xe0, 0x70, 0xaf);
DEFINE_GUID(IID_Brush,        0x8806a321, 0x1e06, 0x422c, 0xa1, 0xcc, 0x01, 0x69, 0x65, 0x59, 0xe0, 0x21);
DEFINE_GUID(IID_FFFactory,    0xd5603377, 0x3dae, 0x4dcd, 0xaf, 0x09, 0xf9, 0x49, 0x8e, 0x9e, 0xc6, 0x59);
DEFINE_GUID(IID_DO,           0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_PV,           0x4bd682dd, 0x7554, 0x40e9, 0x9a, 0x9b, 0x82, 0x65, 0x4e, 0xde, 0x7e, 0x62);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
struct size { float w, h; };
struct thickness { double l, t, r, b; };
struct color { BYTE a, r, g, b; };
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *act_fn)(obj *, obj **);
typedef HRESULT (WINAPI *measure_fn)(obj *, struct size);
typedef HRESULT (WINAPI *getsize_fn)(obj *, struct size *);
typedef HRESULT (WINAPI *putd_fn)(obj *, double);
typedef HRESULT (WINAPI *getd_fn)(obj *, double *);
typedef HRESULT (WINAPI *puti_fn)(obj *, INT32);
typedef HRESULT (WINAPI *puts_fn)(obj *, HSTRING);
typedef HRESULT (WINAPI *gets_fn)(obj *, HSTRING *);
typedef HRESULT (WINAPI *putth_fn)(obj *, struct thickness);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);

enum { TB_SetFontSize = 7, TB_Foreground = 18, TB_SetForeground = 19, TB_SetTextWrapping = 21, TB_Text = 26,
       TB_SetText = 27, TB_SetPadding = 30 };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static obj *tbf;
static obj *text(const WCHAR *str)
{
    obj *t = NULL;
    ((act_fn)SLOT(tbf, 6))(tbf, &t);
    t = as(t, &IID_TB);
    ((puts_fn)SLOT(t, TB_SetText))(t, hs(str));
    return t;
}
static struct size measure(obj *t, float w)
{
    obj *ui = as(t, &IID_UIE);
    struct size avail = { w, INFINITY }, d = { -1, -1 };
    ((measure_fn)SLOT(ui, 91))(ui, avail);
    ((getsize_fn)SLOT(ui, 6))(ui, &d);
    return d;
}

int main(void)
{
    struct size a, b, big, wrapped, padded;
    obj *t, *scbf, *brush = NULL, *back = NULL, *fff, *family = NULL, *tbs, *tp = NULL, *dobj, *v = NULL;
    struct color red = { 255, 200, 10, 20 }, got = { 0 };
    HSTRING str = NULL;
    IInspectable *inner = NULL;
    double opacity = 0;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(tbf = factory(L"Windows.UI.Xaml.Controls.TextBlock", &IID_IActivationFactory))) return 1;

    a = measure(text(L"Hello"), INFINITY);
    b = measure(text(L"Hello world, this is a longer line"), INFINITY);
    printf("hello=%d line=%d longer=%d same_height=%d\n", a.w > 10, a.h >= 12 && a.h <= 30, b.w > a.w * 3, a.h == b.h);
    t = text(L"Hello");
    ((putd_fn)SLOT(t, TB_SetFontSize))(t, 28);
    big = measure(t, INFINITY);
    printf("fontsize_scales=%d\n", big.h > a.h * 1.6 && big.h < a.h * 2.4 && big.w > a.w * 1.6);
    t = text(L"Hello world, this is a longer line");
    ((puti_fn)SLOT(t, TB_SetTextWrapping))(t, 2);
    wrapped = measure(t, 80);
    printf("wraps=%d\n", wrapped.w <= 80 && wrapped.h > a.h * 1.5);
    t = text(L"Hello");
    { struct thickness p = { 5, 5, 5, 5 }; ((putth_fn)SLOT(t, TB_SetPadding))(t, p); }
    padded = measure(t, INFINITY);
    printf("padding=%.0f,%.0f\n", padded.w - a.w, padded.h - a.h);
    ((gets_fn)SLOT(t, TB_Text))(t, &str);
    printf("text=%ls\n", WindowsGetStringRawBuffer(str, NULL));
    if (!(tbs = factory(L"Windows.UI.Xaml.Controls.TextBlock", &IID_TBStatics))) return 1;
    ((get_fn)SLOT(tbs, 16))(tbs, &tp);                                       /* TextProperty */
    dobj = as(t, &IID_DO);
    ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(dobj, 6))(dobj, tp, &v);
    {
        obj *pv = as(v, &IID_PV);
        HSTRING s2 = NULL;
        if (pv) ((gets_fn)SLOT(pv, 19))(pv, &s2);                            /* GetString */
        printf("textproperty=%ls\n", s2 ? WindowsGetStringRawBuffer(s2, NULL) : L"(none)");
    }

    if (!(scbf = factory(L"Windows.UI.Xaml.Media.SolidColorBrush", &IID_SCBFactory))) return 1;
    ((HRESULT (WINAPI *)(obj *, struct color, obj **))SLOT(scbf, 6))(scbf, red, &brush);   /* CreateInstanceWithColor */
    brush = as(brush, &IID_SCB);
    ((HRESULT (WINAPI *)(obj *, struct color *))SLOT(brush, 6))(brush, &got);
    { obj *br = as(brush, &IID_Brush); ((getd_fn)SLOT(br, 6))(br, &opacity); }
    printf("brush=%02x%02x%02x%02x opacity=%.1f\n", got.a, got.r, got.g, got.b, opacity);
    ((putobj_fn)SLOT(t, TB_SetForeground))(t, as(brush, &IID_Brush));
    ((get_fn)SLOT(t, TB_Foreground))(t, &back);
    printf("foreground=%d\n", as(back, &IID_IUnknown) == as(brush, &IID_IUnknown));

    if (!(fff = factory(L"Windows.UI.Xaml.Media.FontFamily", &IID_FFFactory))) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable *, IInspectable **, obj **))SLOT(fff, 6))(fff, hs(L"Courier New"), NULL, &inner, &family);
    str = NULL;
    ((gets_fn)SLOT(family, 6))(family, &str);
    printf("family=%ls\n", str ? WindowsGetStringRawBuffer(str, NULL) : L"(none)");
    return 0;
}
