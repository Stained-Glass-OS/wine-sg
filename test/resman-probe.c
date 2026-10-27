/* Windows.ApplicationModel.Resources.Core.ResourceManager (patches/sg/6128): Current, its
 * MainResourceMap, values and subtrees of maps, a candidate's string, a missing resource, resource
 * references. NanaZip's file manager gets its strings this way; the class did not exist.
 * Interfaces called through their vtables (order per windows-rs, MIT).
 *   resman-probe   prints KEY=VALUE lines, the app's resources.pri beside it (test/mkpri.py) */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <stdio.h>

DEFINE_GUID(IID_Statics, 0x1cc0fdfc, 0x69ee, 0x4e43, 0x99, 0x01, 0x47, 0xf1, 0x26, 0x87, 0xba, 0xf7);
DEFINE_GUID(IID_Manager, 0xf744d97b, 0x9988, 0x44fb, 0xab, 0xd6, 0x53, 0x78, 0x84, 0x4c, 0xfa, 0x8b);
/* IMapView<String, NamedResource> (the signature's IID); INamedResource per windows-rs 0.100 (MIT) */
DEFINE_GUID(IID_MapView, 0x4825d6c4, 0x835a, 0x5da1, 0x9b, 0xdd, 0x12, 0xe9, 0x7e, 0x16, 0xfb, 0x7a);
DEFINE_GUID(IID_Map,     0x72284824, 0xdb8c, 0x42f8, 0xb0, 0x8c, 0x53, 0xff, 0x35, 0x7d, 0xad, 0x82);

typedef struct { void **vtbl; } obj;
#define CALL(o, slot, type, ...) (((type)((o)->vtbl[slot]))((o), __VA_ARGS__))
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *getvalue_fn)(obj *, HSTRING, obj **);
typedef HRESULT (WINAPI *str_fn)(obj *, HSTRING *);
typedef HRESULT (WINAPI *isref_fn)(obj *, HSTRING, boolean *);
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }

static void value(const char *tag, obj *map, const WCHAR *key)
{
    obj *cand = NULL;
    HSTRING v = NULL;
    HRESULT hr = CALL(map, 7, getvalue_fn, hs(key), &cand);   /* GetValue */
    if (FAILED(hr)) { printf("%s=hr %08lx\n", tag, hr); return; }
    hr = CALL(cand, 10, str_fn, &v);                         /* ValueAsString */
    {
        char utf8[256];
        WideCharToMultiByte(CP_UTF8, 0, SUCCEEDED(hr) ? WindowsGetStringRawBuffer(v, NULL) : L"(no string)", -1,
                            utf8, sizeof(utf8), NULL, NULL);
        printf("%s=%s\n", tag, utf8);
    }
    ((rel_fn)cand->vtbl[2])(cand);
}

typedef HRESULT (WINAPI *haskey_fn)(obj *, HSTRING, boolean *);
typedef HRESULT (WINAPI *getat_fn)(obj *, UINT32, obj **);
typedef HRESULT (WINAPI *size_fn)(obj *, UINT32 *);

/* as NanaZip reads its strings: map.HasKey(k), then map.Lookup(k).Candidates().GetAt(0).ValueAsString() */
static void lookup(const char *tag, obj *view, const WCHAR *key)
{
    obj *named = NULL, *cands = NULL, *cand = NULL;
    boolean has = 2;
    UINT32 n = 0;
    HSTRING v = NULL;
    char utf8[256];
    HRESULT hr;

    CALL(view, 8, haskey_fn, hs(key), &has);                       /* HasKey */
    hr = CALL(view, 6, getvalue_fn, hs(key), &named);             /* Lookup */
    if (FAILED(hr)) { printf("%s=has %d hr %08lx\n", tag, has, hr); return; }
    CALL(named, 7, get_fn, &cands);                               /* NamedResource.Candidates */
    if (!cands) { printf("%s=has %d no candidates\n", tag, has); return; }
    CALL(cands, 7, size_fn, &n);                                  /* Size */
    CALL(cands, 6, getat_fn, 0, &cand);                           /* GetAt(0) */
    if (!cand) { printf("%s=has %d n %u no candidate\n", tag, has, n); return; }
    hr = CALL(cand, 10, str_fn, &v);                              /* ValueAsString */
    WideCharToMultiByte(CP_UTF8, 0, SUCCEEDED(hr) ? WindowsGetStringRawBuffer(v, NULL) : L"(no string)", -1,
                        utf8, sizeof(utf8), NULL, NULL);
    printf("%s=has %d n %u %s\n", tag, has, n, utf8);
}

int main(void)
{
    obj *statics, *manager, *map, *sub, *custom, *mq;
    boolean isref = 2;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(hs(L"Windows.ApplicationModel.Resources.Core.ResourceManager"), &IID_Statics, (void **)&statics);
    printf("factory=%08lx\n", hr);
    if (FAILED(hr)) return 1;
    hr = CALL(statics, 6, get_fn, &manager);                    /* Current */
    printf("current=%08lx\n", hr);
    if (FAILED(hr)) return 1;
    printf("manager_qi=%08lx\n", CALL(manager, 0, qi_fn, &IID_Manager, (void **)&mq));
    hr = CALL(manager, 6, get_fn, &map);                        /* MainResourceMap */
    printf("mainmap=%08lx\n", hr);
    printf("map_qi=%08lx\n", CALL(map, 0, qi_fn, &IID_Map, (void **)&mq));
    value("greeting", map, L"Resources/Greeting");
    value("missing", map, L"Resources/Missing");
    CALL(map, 9, getvalue_fn, hs(L"Resources"), &sub);          /* GetSubtree */
    value("farewell", sub, L"Farewell");
    value("deep", sub, L"Nested/Deep");
    value("fromroot", sub, L"/custom/Title");
    CALL(map, 9, getvalue_fn, hs(L"custom"), &custom);
    value("utf8", custom, L"Utf8");
    {
        obj *view = NULL;
        printf("mapview_qi=%08lx\n", CALL(sub, 0, qi_fn, &IID_MapView, (void **)&view));
        if (view)
        {
            lookup("lookup", view, L"Farewell");
            lookup("lookup_missing", view, L"Missing");
        }
    }
    CALL(statics, 7, isref_fn, hs(L"ms-resource:///Resources/Greeting"), &isref);   /* IsResourceReference */
    printf("isref=%d\n", isref);
    CALL(statics, 7, isref_fn, hs(L"plain text"), &isref);
    printf("notref=%d\n", isref);
    return 0;
}
