/* Windows.UI.Xaml markup on a program's own types (patches/sg/6262): a member of a program class that its
 * metadata gives on a base type (WinUI 2's RevealBackgroundBrush.TargetTheme and Color are RevealBrush's)
 * is set through the base type's IXamlMember, and an enum member of the program's given as digits
 * (compiled markup) or by name reaches it as an IReference of the enum. Windows Terminal's 74 reveal
 * brushes were left without colour and theme. The application here is the probe's own (composed over
 * Windows.UI.Xaml.Application) and is its IXamlMetadataProvider; its types are SgTest.Derived (own
 * member Label) over SgTest.Base (members Theme, an ApplicationTheme, and Count, an Int32).
 * Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_AppFactory,     0x93bbe361, 0xbe5a, 0x4ee3, 0xb4, 0xa3, 0x95, 0x11, 0x8d, 0xc9, 0x7a, 0x89);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_Provider,       0xb3765d69, 0x68a5, 0x4b32, 0x88, 0x61, 0xfd, 0xb9, 0x0c, 0x1f, 0x58, 0x36);
DEFINE_GUID(IID_XamlType,       0x7920eab1, 0xa2e5, 0x479a, 0xbd, 0x50, 0x6c, 0xef, 0x3c, 0x0b, 0x49, 0x70);
DEFINE_GUID(IID_XamlMember,     0xc541f58c, 0x43a9, 0x4216, 0xb7, 0x18, 0xe0, 0xb1, 0x1b, 0x14, 0xe9, 0x3e);
/* IReference<ApplicationTheme>, IReference<Int32> */
DEFINE_GUID(IID_RefTheme,       0x08c245e1, 0x6ea8, 0x581c, 0x98, 0xa2, 0xfa, 0xc6, 0xbe, 0x82, 0xce, 0xcf);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_RefInt32,       0x548cefbd, 0xbc8a, 0x5fa0, 0x8d, 0xf2, 0x95, 0x74, 0x40, 0xfc, 0x8b, 0xf4);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) (((obj *)(o))->vtbl[n])
static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }

/* what the program's members were given */
static int theme_set = -1, theme_kind, count_set = -1, label_set, kind_set = -1;
static WCHAR label[64];

/* ---- a small object: one vtable of IInspectable + methods, whatever interface is asked of it ---- */
enum kind { K_PROVIDER, K_TYPE_DERIVED, K_TYPE_BASE, K_TYPE_FRAMEWORK, K_TYPE_THEME, K_TYPE_INT32, K_TYPE_STRING,
            K_TYPE_KIND, K_MEMBER_THEME, K_MEMBER_COUNT, K_MEMBER_LABEL, K_MEMBER_KIND, K_INSTANCE };
struct small { void **vtbl; enum kind kind; LONG ref; };

static struct small *small_new(void **vtbl, enum kind kind)
{
    struct small *s = calloc(1, sizeof(*s));
    s->vtbl = vtbl; s->kind = kind; s->ref = 1;
    return s;
}
static HRESULT WINAPI s_qi(struct small *s, REFIID iid, void **out)
{
    *out = s;
    InterlockedIncrement(&s->ref);
    return S_OK;
}
static ULONG WINAPI s_addref(struct small *s) { return InterlockedIncrement(&s->ref); }
static ULONG WINAPI s_release(struct small *s) { return InterlockedDecrement(&s->ref); }
static HRESULT WINAPI s_iids(struct small *s, ULONG *n, IID **iids) { *n = 0; *iids = NULL; return S_OK; }
static HRESULT WINAPI s_class(struct small *s, HSTRING *name)
{
    return WindowsCreateString(L"SgTest.Derived", 14, name);
}
static HRESULT WINAPI s_trust(struct small *s, TrustLevel *t) { *t = BaseTrust; return S_OK; }
static HRESULT WINAPI s_notimpl(void) { return E_NOTIMPL; }

static void *type_vtbl[24], *member_vtbl[14], *provider_vtbl[9], *instance_vtbl[6];

static struct small *make_type(enum kind kind) { return small_new(type_vtbl, kind); }

/* IXamlMetadataProvider */
static HRESULT WINAPI p_by_name(struct small *s, HSTRING name, struct small **type)
{
    const WCHAR *n = WindowsGetStringRawBuffer(name, NULL);
    *type = NULL;
    if (!wcscmp(n, L"SgTest.Derived")) *type = make_type(K_TYPE_DERIVED);
    else if (!wcscmp(n, L"SgTest.Base")) *type = make_type(K_TYPE_BASE);
    return S_OK;
}
static HRESULT WINAPI p_by_typename(struct small *s, void *name, struct small **type) { *type = NULL; return S_OK; }

/* IXamlType */
static HRESULT WINAPI t_base(struct small *s, struct small **base)
{
    /* Derived : Base : a framework type, which a program's metadata knows by name only */
    *base = s->kind == K_TYPE_DERIVED ? make_type(K_TYPE_BASE) : s->kind == K_TYPE_BASE ? make_type(K_TYPE_FRAMEWORK) : NULL;
    return S_OK;
}
static HRESULT WINAPI t_fullname(struct small *s, HSTRING *name)
{
    const WCHAR *n = s->kind == K_TYPE_DERIVED ? L"SgTest.Derived" : s->kind == K_TYPE_BASE ? L"SgTest.Base" :
                     s->kind == K_TYPE_THEME ? L"Windows.UI.Xaml.ApplicationTheme" :
                     s->kind == K_TYPE_INT32 ? L"Int32" : s->kind == K_TYPE_FRAMEWORK ? L"Windows.UI.Xaml.DependencyObject" :
                     s->kind == K_TYPE_KIND ? L"SgTest.Kind" : L"String";
    return WindowsCreateString(n, lstrlenW(n), name);
}
static HRESULT WINAPI t_bool(struct small *s, boolean *b) { *b = FALSE; return S_OK; }
static HRESULT WINAPI t_constructible(struct small *s, boolean *b) { *b = s->kind == K_TYPE_DERIVED; return S_OK; }
static HRESULT WINAPI t_null(struct small *s, void **p) { *p = NULL; return S_OK; }
static HRESULT WINAPI t_activate(struct small *s, struct small **out)
{
    *out = s->kind == K_TYPE_DERIVED ? small_new(instance_vtbl, K_INSTANCE) : NULL;
    return *out ? S_OK : E_NOTIMPL;
}
/* a type's own members only, as generated metadata does */
/* SgTest.Kind, an enum of the program's: First, Second */
static HRESULT WINAPI t_from_string(struct small *s, HSTRING text, IInspectable **value)
{
    const WCHAR *t = WindowsGetStringRawBuffer(text, NULL);
    obj *statics = NULL;
    INT32 v = !wcscmp(t, L"First") ? 0 : !wcscmp(t, L"Second") ? 1 : -1;

    *value = NULL;
    if (s->kind != K_TYPE_KIND || v < 0) return E_INVALIDARG;
    /* PropertyValue.CreateInt32 */
    if (FAILED(RoGetActivationFactory(hs(L"Windows.Foundation.PropertyValue"), &IID_PVStatics, (void **)&statics))) return E_FAIL;
    return ((HRESULT (WINAPI *)(obj *, INT32, IInspectable **))SLOT(statics, 10))(statics, v, value);
}
static HRESULT WINAPI t_member(struct small *s, HSTRING name, struct small **member)
{
    const WCHAR *n = WindowsGetStringRawBuffer(name, NULL);
    *member = NULL;
    if (s->kind == K_TYPE_FRAMEWORK) return E_NOTIMPL;   /* as C++/WinRT metadata throws for it */
    if (s->kind == K_TYPE_DERIVED && !wcscmp(n, L"Kind")) *member = small_new(member_vtbl, K_MEMBER_KIND);
    if (s->kind == K_TYPE_DERIVED && !wcscmp(n, L"Label")) *member = small_new(member_vtbl, K_MEMBER_LABEL);
    if (s->kind == K_TYPE_BASE && !wcscmp(n, L"Theme")) *member = small_new(member_vtbl, K_MEMBER_THEME);
    if (s->kind == K_TYPE_BASE && !wcscmp(n, L"Count")) *member = small_new(member_vtbl, K_MEMBER_COUNT);
    return S_OK;
}

/* IXamlMember */
static HRESULT WINAPI m_false(struct small *s, boolean *b) { *b = FALSE; return S_OK; }
static HRESULT WINAPI m_name(struct small *s, HSTRING *name)
{
    const WCHAR *n = s->kind == K_MEMBER_THEME ? L"Theme" : s->kind == K_MEMBER_COUNT ? L"Count" :
                     s->kind == K_MEMBER_KIND ? L"Kind" : L"Label";
    return WindowsCreateString(n, lstrlenW(n), name);
}
static HRESULT WINAPI m_target(struct small *s, struct small **type)
{
    *type = make_type(s->kind == K_MEMBER_LABEL || s->kind == K_MEMBER_KIND ? K_TYPE_DERIVED : K_TYPE_BASE);
    return S_OK;
}
static HRESULT WINAPI m_type(struct small *s, struct small **type)
{
    *type = make_type(s->kind == K_MEMBER_THEME ? K_TYPE_THEME : s->kind == K_MEMBER_COUNT ? K_TYPE_INT32 :
                      s->kind == K_MEMBER_KIND ? K_TYPE_KIND : K_TYPE_STRING);
    return S_OK;
}
static HRESULT WINAPI m_get(struct small *s, IInspectable *instance, IInspectable **value) { *value = NULL; return S_OK; }
static HRESULT WINAPI m_set(struct small *s, IInspectable *instance, IInspectable *value)
{
    obj *ref = NULL;
    INT32 v = -1;

    if (!value) return E_INVALIDARG;
    if (s->kind == K_MEMBER_THEME)
    {
        /* an enum is unboxed as its own IReference, as C++/WinRT's unbox_value does */
        if (FAILED(IInspectable_QueryInterface(value, &IID_RefTheme, (void **)&ref)) || !ref) return E_NOINTERFACE;
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(ref, 6))(ref, &v);
        theme_set = v;
        theme_kind++;
        return S_OK;
    }
    if (s->kind == K_MEMBER_KIND)
    {
        if (FAILED(IInspectable_QueryInterface(value, &IID_RefInt32, (void **)&ref)) || !ref) return E_NOINTERFACE;
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(ref, 6))(ref, &v);
        kind_set = v;
        return S_OK;
    }
    if (s->kind == K_MEMBER_COUNT)
    {
        if (FAILED(IInspectable_QueryInterface(value, &IID_RefInt32, (void **)&ref)) || !ref) return E_NOINTERFACE;
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(ref, 6))(ref, &v);
        count_set = v;
        return S_OK;
    }
    {
        HSTRING str = NULL;
        if (FAILED(IInspectable_GetRuntimeClassName(value, &str))) return E_INVALIDARG;
        WindowsDeleteString(str);
        label_set = 1;
        return S_OK;
    }
}

/* the application: composed over Windows.UI.Xaml.Application, the metadata provider itself */
static IInspectable *app_inner;
static struct small *provider;
static HRESULT WINAPI app_qi(obj *self, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_Provider)) { *out = provider; return S_OK; }
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)) { *out = self; return S_OK; }
    return IInspectable_QueryInterface(app_inner, iid, out);
}
static ULONG WINAPI app_addref(obj *self) { return 2; }
static ULONG WINAPI app_release(obj *self) { return 1; }
static HRESULT WINAPI app_iids(obj *self, ULONG *n, IID **iids) { *n = 0; *iids = NULL; return S_OK; }
static HRESULT WINAPI app_class(obj *self, HSTRING *name) { return WindowsCreateString(L"SgTest.App", 10, name); }
static HRESULT WINAPI app_trust(obj *self, TrustLevel *t) { *t = BaseTrust; return S_OK; }
static void *app_vtbl[] = { app_qi, app_addref, app_release, app_iids, app_class, app_trust };
static obj app_outer = { app_vtbl };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:t=\"using:SgTest\""

static void load(obj *reader, const WCHAR *markup, const char *what)
{
    obj *root = NULL;
    HRESULT hr = ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(reader, 6))(reader, hs(markup), &root);
    printf("%s_load=%08lx root=%d\n", what, hr, root != NULL);
}

int main(void)
{
    obj *appf, *app = NULL, *reader = NULL;
    int i;

    for (i = 0; i < 6; i++) type_vtbl[i] = i == 0 ? (void *)s_qi : i == 1 ? (void *)s_addref : i == 2 ? (void *)s_release :
                                         i == 3 ? (void *)s_iids : i == 4 ? (void *)s_class : (void *)s_trust;
    memcpy(member_vtbl, type_vtbl, 6 * sizeof(void *));
    memcpy(provider_vtbl, type_vtbl, 6 * sizeof(void *));
    memcpy(instance_vtbl, type_vtbl, 6 * sizeof(void *));
    for (i = 6; i < 24; i++) type_vtbl[i] = s_notimpl;
    type_vtbl[6] = t_base; type_vtbl[7] = t_null; type_vtbl[8] = t_fullname;
    type_vtbl[9] = t_bool; type_vtbl[10] = t_bool; type_vtbl[11] = t_constructible; type_vtbl[12] = t_bool;
    type_vtbl[13] = t_bool; type_vtbl[14] = t_bool; type_vtbl[15] = t_null; type_vtbl[16] = t_null; type_vtbl[17] = t_null;
    type_vtbl[18] = t_activate; type_vtbl[19] = t_from_string; type_vtbl[20] = t_member;
    member_vtbl[6] = m_false; member_vtbl[7] = m_false; member_vtbl[8] = m_false; member_vtbl[9] = m_name;
    member_vtbl[10] = m_target; member_vtbl[11] = m_type; member_vtbl[12] = m_get; member_vtbl[13] = m_set;
    provider_vtbl[6] = p_by_typename; provider_vtbl[7] = p_by_name; provider_vtbl[8] = s_notimpl;
    provider = small_new(provider_vtbl, K_PROVIDER);

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (FAILED(RoGetActivationFactory(hs(L"Windows.UI.Xaml.Application"), &IID_AppFactory, (void **)&appf))) return 1;
    ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(appf, 6))(appf, (IInspectable *)&app_outer, &app_inner, &app);
    printf("app=%d\n", app != NULL);
    if (FAILED(RoGetActivationFactory(hs(L"Windows.UI.Xaml.Markup.XamlReader"), &IID_ReaderStatics, (void **)&reader))) return 1;

    /* the base type's members, the enum as digits (compiled markup gives them so) */
    load(reader, L"<t:Derived " NS L" Theme=\"1\" Count=\"7\" Label=\"x\"/>", "digits");
    printf("digits theme=%d count=%d label=%d\n", theme_set, count_set, label_set);
    /* an enum of the program's, made by its type */
    load(reader, L"<t:Derived " NS L" Kind=\"Second\" Count=\"3\"/>", "kind");
    printf("kind kind=%d count=%d\n", kind_set, count_set);
    /* a member of none of the types: the walk ends at the framework type, whose metadata has no
     * members (E_NOTIMPL); the member is not found (E_INVALIDARG), which compiled markup passes over,
     * not the metadata's failure, which ends compiled markup */
    load(reader, L"<t:Derived " NS L" Missing=\"x\"/>", "missing");
    /* and the enum by its name */
    theme_set = -1;
    load(reader, L"<t:Derived " NS L" Theme=\"Light\"/>", "name");
    printf("name theme=%d\n", theme_set);
    return 0;
}
