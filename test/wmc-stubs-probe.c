/* windows.media.mediacontrol (patches/sg/2820), run by test/wmc-stubs-gate.sh
 * on Xvfb. Table-driven over the SystemMediaTransportControls factory, the
 * control for a window, its display updater, the music properties and the
 * Genres list: the IInspectable contract of each, then the property
 * round-trips with their defaults, the event registrations and the
 * IVector<HSTRING> behaviour.
 *
 *   wmc-stubs-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_ISMTC, 0x99fa3ff4, 0x1742, 0x42a6, 0x90, 0x2e, 0x08, 0x7d, 0x41, 0xf9, 0x65, 0xec);
DEFINE_GUID(IID_ISMTCInterop, 0xddb0472d, 0xc911, 0x4a1f, 0x86, 0xd9, 0xdc, 0x3d, 0x71, 0xa9, 0x5f, 0x5a);
DEFINE_GUID(IID_IUpdater, 0x8abbc53e, 0xfa55, 0x4ecf, 0xad, 0x8e, 0xc9, 0x84, 0xe5, 0xdd, 0x15, 0x50);
DEFINE_GUID(IID_IMusic, 0x6bbf0c59, 0xd0a0, 0x4d26, 0x92, 0xa0, 0xf9, 0x78, 0xe1, 0xd1, 0x8e, 0x7b);
DEFINE_GUID(IID_IMusic2, 0x00368462, 0x97d3, 0x44b9, 0xb0, 0x0f, 0x00, 0x8a, 0xfc, 0xef, 0xaf, 0x18);
DEFINE_GUID(IID_IVectorHS, 0x98b9acc1, 0x4b56, 0x532e, 0xac, 0x73, 0x03, 0xd5, 0x29, 0x1c, 0xca, 0x90);
DEFINE_GUID(IID_IVectorViewHS, 0x2f13c006, 0xa03a, 0x5f69, 0xb0, 0x90, 0x75, 0xa4, 0x3e, 0x33, 0x42, 0x3e);
DEFINE_GUID(IID_IIterableHS, 0xe2fcc7c1, 0x3bfc, 0x5a0b, 0xb2, 0xb0, 0x72, 0xe7, 0x69, 0xd1, 0xcb, 0x7e);
DEFINE_GUID(IID_IIteratorHS, 0x8c304ebb, 0x6615, 0x50a4, 0x88, 0x29, 0x87, 0x9e, 0xcd, 0x44, 0x32, 0x36);
DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

#undef IInspectable_Release
#undef IInspectable_QueryInterface
#define IInspectable_Release(p) IUnknown_Release((IUnknown *)(p))
#define IInspectable_QueryInterface(p, i, o) IUnknown_QueryInterface((IUnknown *)(p), i, (void **)(o))
typedef struct { INT64 value; } EventRegistrationToken;
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define E_BOUNDS_ ((HRESULT)0x8000000B)
#define E_CHANGED_STATE_ ((HRESULT)0x8000000C)
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* raw vtable calls: slot numbers count IUnknown (3) and IInspectable (3) */
#define VT(obj) (*(void ***)(obj))
typedef HRESULT (WINAPI *f_pp)(void *, void *);
typedef HRESULT (WINAPI *f_u)(void *, UINT32);
typedef HRESULT (WINAPI *f_up)(void *, UINT32, void *);
typedef HRESULT (WINAPI *f_upp)(void *, UINT32, void *, void *);
typedef HRESULT (WINAPI *f_b)(void *, BOOLEAN);
typedef HRESULT (WINAPI *f_v)(void *);
typedef HRESULT (WINAPI *f_ppp)(void *, void *, void *, void *);
typedef HRESULT (WINAPI *f_uupp)(void *, UINT32, UINT32, void *, void *);
typedef HRESULT (WINAPI *f_q)(void *, UINT64);
#define CALL_P(o, s, a) ((f_pp)VT(o)[s])(o, a)
#define CALL_V(o, s) ((f_v)VT(o)[s])(o)
#define CALL_U(o, s, a) ((f_u)VT(o)[s])(o, a)
#define CALL_UP(o, s, a, b) ((f_up)VT(o)[s])(o, a, b)
#define CALL_UPP(o, s, a, b, c) ((f_upp)VT(o)[s])(o, a, b, c)
#define CALL_B(o, s, a) ((f_b)VT(o)[s])(o, a)

static HSTRING mk(const WCHAR *s)
{
    HSTRING h = NULL;
    WindowsCreateString(s, wcslen(s), &h);
    return h;
}
static int hs_eq(HSTRING h, const WCHAR *s)
{
    const WCHAR *b = WindowsGetStringRawBuffer(h, NULL);
    return !wcscmp(b ? b : L"", s);
}

/* a minimal COM object, to hand out as an event handler or thumbnail */
static LONG obj_ref;
static HRESULT WINAPI o_qi(IUnknown *i, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI o_addref(IUnknown *i) { return InterlockedIncrement(&obj_ref); }
static ULONG WINAPI o_release(IUnknown *i) { return InterlockedDecrement(&obj_ref); }
static IUnknownVtbl o_vtbl = { o_qi, o_addref, o_release };
static IUnknown the_obj = { &o_vtbl };

#define MAXIIDS 4
struct inspectable_case
{
    const char *name;
    const WCHAR *class_name;
    const GUID *iids[MAXIIDS];
};

static int in_list(const GUID *g, const GUID *const *list)
{
    for (; *list; list++) if (IsEqualGUID(g, *list)) return 1;
    return 0;
}

static void check_inspectable(IInspectable *obj, const struct inspectable_case *c)
{
    ULONG count = 0, i, expected = 0;
    IID *iids = NULL;
    TrustLevel trust = 0x5555;
    HSTRING name = NULL;
    HRESULT hr;
    void *out;
    int all_known = 1, all_qi = 1;

    while (c->iids[expected]) expected++;

    hr = IInspectable_GetIids(obj, &count, &iids);
    CHECKF(hr == S_OK && count == expected, "%s: GetIids reports %lu interfaces (want %lu, hr %#lx)", c->name, count, expected, hr);
    if (hr == S_OK)
    {
        for (i = 0; i < count; i++)
        {
            IUnknown *unk = NULL;
            if (!in_list(&iids[i], c->iids)) all_known = 0;
            if (FAILED(IInspectable_QueryInterface(obj, &iids[i], (void **)&unk)) || !unk) all_qi = 0;
            else IUnknown_Release(unk);
        }
        CHECKF(all_known, "%s: every reported IID is an expected one", c->name);
        CHECKF(all_qi, "%s: every reported IID answers QueryInterface", c->name);
        CoTaskMemFree(iids);
    }
    hr = IInspectable_GetTrustLevel(obj, &trust);
    CHECKF(hr == S_OK && trust == BaseTrust, "%s: GetTrustLevel is BaseTrust (hr %#lx, trust %d)", c->name, hr, trust);
    hr = IInspectable_GetRuntimeClassName(obj, &name);
    CHECKF(hr == S_OK && name && hs_eq(name, c->class_name) && WindowsGetStringLen(name) == wcslen(c->class_name),
           "%s: GetRuntimeClassName is %ls (hr %#lx)", c->name, c->class_name, hr);
    WindowsDeleteString(name);
    out = (void *)0x1234;
    hr = IInspectable_QueryInterface(obj, &IID_Bogus, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "%s: unknown IID gives E_NOINTERFACE and NULL (%#lx %p)", c->name, hr, out);
}

/* the boolean properties of the control: getter slot, default, name */
static const struct { const char *name; int get; BOOLEAN def; } bools[] =
{
    { "IsPlayEnabled", 12, 0 }, { "IsStopEnabled", 14, 0 }, { "IsPauseEnabled", 16, 0 },
    { "IsRecordEnabled", 18, 0 }, { "IsFastForwardEnabled", 20, 0 }, { "IsRewindEnabled", 22, 0 },
    { "IsPreviousEnabled", 24, 0 }, { "IsNextEnabled", 26, 0 },
    { "IsChannelUpEnabled", 28, 0 }, { "IsChannelDownEnabled", 30, 0 },
};

/* the string properties: IID, getter slot (setter is the next one) */
static void check_string_prop(void *obj, int get, const char *name)
{
    HSTRING s = NULL, v = mk(L"Some Value");
    HRESULT hr = CALL_P(obj, get, &s);
    CHECKF(hr == S_OK && hs_eq(s, L""), "%s: empty by default (%#lx)", name, hr);
    WindowsDeleteString(s);
    s = NULL;
    hr = CALL_P(obj, get + 1, v);
    CHECKF(hr == S_OK, "%s: put (%#lx)", name, hr);
    hr = CALL_P(obj, get, &s);
    CHECKF(hr == S_OK && hs_eq(s, L"Some Value"), "%s: round-trips (%#lx)", name, hr);
    WindowsDeleteString(s);
    hr = CALL_P(obj, get, NULL);
    CHECKF(hr == E_POINTER, "%s: NULL out pointer is E_POINTER (%#lx)", name, hr);
    WindowsDeleteString(v);
}

static void check_events(void *smtc, int add, const char *name)
{
    EventRegistrationToken t1 = {0}, t2 = {0};
    HRESULT hr;
    LONG base;

    obj_ref = 1;
    {
        typedef HRESULT (WINAPI *ev_add_fn)(void *, void *, EventRegistrationToken *);
        typedef HRESULT (WINAPI *ev_rem_fn)(void *, EventRegistrationToken);
        ev_add_fn add_fn = (ev_add_fn)VT(smtc)[add];
        ev_rem_fn rem_fn = (ev_rem_fn)VT(smtc)[add + 1];

        base = obj_ref;
        hr = add_fn(smtc, &the_obj, &t1);
        CHECKF(hr == S_OK && t1.value != 0, "%s: add returns a token (%#lx, %I64d)", name, hr, (long long)t1.value);
        CHECKF(obj_ref == base + 1, "%s: the handler is referenced (%ld)", name, obj_ref - base);
        hr = add_fn(smtc, &the_obj, &t2);
        CHECKF(hr == S_OK && t2.value != 0 && t2.value != t1.value, "%s: a second add gives another token", name);
        hr = add_fn(smtc, &the_obj, NULL);
        CHECKF(hr == E_POINTER, "%s: NULL token is E_POINTER (%#lx)", name, hr);
        { EventRegistrationToken t3 = {5}; hr = add_fn(smtc, NULL, &t3); }
        CHECKF(hr == E_INVALIDARG, "%s: NULL handler is E_INVALIDARG (%#lx)", name, hr);
        hr = rem_fn(smtc, t1);
        CHECKF(hr == S_OK && obj_ref == base + 1, "%s: remove releases the handler (%ld)", name, obj_ref - base);
        hr = rem_fn(smtc, t1);
        CHECKF(hr == S_OK && obj_ref == base + 1, "%s: removing it again is harmless", name);
        hr = rem_fn(smtc, t2);
        CHECKF(hr == S_OK && obj_ref == base, "%s: removing the second", name);
        /* a handler left registered is released with the control */
        add_fn(smtc, &the_obj, &t1);
    }
}

static void check_genres(void *music2)
{
    void *genres = NULL, *genres2 = NULL, *view = NULL, *iterable = NULL, *iter = NULL;
    HSTRING a = mk(L"Rock"), b = mk(L"Jazz"), c = mk(L"Blues"), out = NULL, items[4] = {0};
    UINT32 size = 99, idx = 99, n = 99;
    BOOLEAN found = 7, has = 7;
    HRESULT hr;
    struct inspectable_case vec = { "Genres vector", L"Windows.Foundation.Collections.IVector`1<String>",
                                    { &IID_IVectorHS, &IID_IIterableHS } };

    hr = CALL_P(music2, 10, &genres);
    CHECKF(hr == S_OK && genres, "Genres returns a list (%#lx)", hr);
    if (!genres) return;
    hr = CALL_P(music2, 10, &genres2);
    CHECKF(hr == S_OK && genres2 == genres, "Genres is the same list each time");
    IInspectable_Release(genres2);
    check_inspectable(genres, &vec);

    hr = CALL_P(genres, 7, &size);
    CHECKF(hr == S_OK && size == 0, "Genres starts empty (%u)", size);
    hr = CALL_V(genres, 14);
    CHECKF(hr == E_BOUNDS_, "RemoveAtEnd of an empty list is E_BOUNDS (%#lx)", hr);
    CHECKF(CALL_P(genres, 13, a) == S_OK && CALL_P(genres, 13, b) == S_OK, "Append twice");
    hr = CALL_P(genres, 7, &size);
    CHECKF(hr == S_OK && size == 2, "size is 2 (%u)", size);
    hr = CALL_UP(genres, 6, 1, &out);
    CHECKF(hr == S_OK && hs_eq(out, L"Jazz"), "GetAt(1) is Jazz (%#lx)", hr);
    WindowsDeleteString(out);
    hr = CALL_UP(genres, 6, 2, &out);
    CHECKF(hr == E_BOUNDS_, "GetAt(2) is E_BOUNDS (%#lx)", hr);
    hr = ((f_ppp)VT(genres)[9])(genres, b, &idx, &found);
    CHECKF(hr == S_OK && found && idx == 1, "IndexOf(Jazz) is 1 (%#lx, %u)", hr, idx);
    hr = ((f_ppp)VT(genres)[9])(genres, c, &idx, &found);
    CHECKF(hr == S_OK && !found && idx == 0, "IndexOf(Blues) is not found");
    hr = CALL_UP(genres, 11, 3, c);
    CHECKF(hr == E_BOUNDS_, "InsertAt past the end is E_BOUNDS (%#lx)", hr);
    hr = CALL_UP(genres, 11, 1, c);
    CHECKF(hr == S_OK, "InsertAt(1) (%#lx)", hr);
    hr = CALL_UP(genres, 10, 0, c);
    CHECKF(hr == S_OK, "SetAt(0) (%#lx)", hr);
    hr = CALL_UP(genres, 10, 3, c);
    CHECKF(hr == E_BOUNDS_, "SetAt(3) is E_BOUNDS (%#lx)", hr);
    hr = ((f_uupp)VT(genres)[16])(genres, 0, 4, items, &n);
    CHECKF(hr == S_OK && n == 3 && hs_eq(items[0], L"Blues") && hs_eq(items[1], L"Blues") && hs_eq(items[2], L"Jazz"),
           "GetMany returns Blues, Blues, Jazz (%#lx, %u)", hr, n);
    while (n--) WindowsDeleteString(items[n]);
    hr = CALL_U(genres, 12, 3);
    CHECKF(hr == E_BOUNDS_, "RemoveAt(3) is E_BOUNDS (%#lx)", hr);

    /* view and iterator */
    hr = CALL_P(genres, 8, &view);
    CHECKF(hr == S_OK && view, "GetView (%#lx)", hr);
    if (view)
    {
        struct inspectable_case vv = { "Genres view", L"Windows.Foundation.Collections.IVectorView`1<String>",
                                       { &IID_IVectorViewHS, &IID_IIterableHS } };
        check_inspectable(view, &vv);
        hr = IInspectable_QueryInterface(view, &IID_IIterableHS, &iterable);
        if (iterable)
        {
            hr = CALL_P(iterable, 6, &iter);
            CHECKF(hr == S_OK && iter, "First (%#lx)", hr);
            if (iter)
            {
                struct inspectable_case ic = { "Genres iterator", L"Windows.Foundation.Collections.IIterator`1<String>",
                                               { &IID_IIteratorHS } };
                int count = 0;
                check_inspectable(iter, &ic);
                hr = CALL_P(iter, 7, &has);
                while (hr == S_OK && has)
                {
                    count++;
                    CALL_P(iter, 8, &has);
                }
                CHECKF(count == 3, "the iterator visits 3 elements (%d)", count);
                IInspectable_Release(iter);
                iter = NULL;
                /* modifying the list invalidates a running iterator */
                CALL_P(iterable, 6, &iter);
                hr = CALL_P(genres, 13, a);
                CHECKF(hr == S_OK, "Append while iterating");
                has = 7;
                hr = CALL_P(iter, 7, &has);
                CHECKF(hr == E_CHANGED_STATE_, "HasCurrent after a change is E_CHANGED_STATE (%#lx)", hr);
                hr = CALL_P(iter, 8, &has);
                CHECKF(hr == E_CHANGED_STATE_, "MoveNext after a change is E_CHANGED_STATE (%#lx)", hr);
                IInspectable_Release(iter);
            }
            IInspectable_Release(iterable);
        }
        hr = CALL_P(view, 7, &size);
        CHECKF(hr == S_OK && size == 4, "the view sees the 4 elements (%u)", size);
        IInspectable_Release(view);
    }

    hr = CALL_V(genres, 14);
    CHECKF(hr == S_OK, "RemoveAtEnd");
    items[0] = a;
    hr = ((f_up)VT(genres)[17])(genres, 1, items);
    CHECKF(hr == S_OK, "ReplaceAll (%#lx)", hr);
    CALL_P(genres, 7, &size);
    CHECKF(size == 1, "ReplaceAll leaves one element (%u)", size);
    CHECKF(CALL_V(genres, 15) == S_OK, "Clear");
    CALL_P(genres, 7, &size);
    CHECKF(size == 0, "Clear empties the list (%u)", size);
    CALL_P(genres, 13, c);
    IInspectable_Release(genres);
    WindowsDeleteString(a);
    WindowsDeleteString(b);
    WindowsDeleteString(c);
}

static void check_updater(void *updater)
{
    void *music = NULL, *music2 = NULL, *music_b = NULL, *thumb = NULL;
    HSTRING s = NULL, v = mk(L"Name");
    UINT32 n = 99;
    int type = -1;
    HRESULT hr;
    struct inspectable_case uc = { "DisplayUpdater", L"Windows.Media.SystemMediaTransportControlsDisplayUpdater",
                                   { &IID_IUpdater } };
    struct inspectable_case mc = { "MusicProperties", L"Windows.Media.MusicDisplayProperties",
                                   { &IID_IMusic, &IID_IMusic2 } };

    check_inspectable(updater, &uc);
    check_string_prop(updater, 8, "AppMediaId");

    hr = CALL_P(updater, 10, &thumb);
    CHECKF(hr == S_OK && !thumb, "Thumbnail is NULL by default (%#lx)", hr);
    obj_ref = 1;
    hr = CALL_P(updater, 11, &the_obj);
    CHECKF(hr == S_OK && obj_ref == 2, "put_Thumbnail keeps a reference (%#lx, %ld)", hr, obj_ref);
    hr = CALL_P(updater, 10, &thumb);
    CHECKF(hr == S_OK && thumb == &the_obj && obj_ref == 3, "Thumbnail returns it (%#lx, %ld)", hr, obj_ref);
    IUnknown_Release((IUnknown *)thumb);
    hr = CALL_P(updater, 11, NULL);
    CHECKF(hr == S_OK && obj_ref == 1, "put_Thumbnail(NULL) releases it (%#lx, %ld)", hr, obj_ref);

    hr = CALL_P(updater, 12, &music);
    CHECKF(hr == S_OK && music, "MusicProperties (%#lx)", hr);
    hr = CALL_P(updater, 12, &music_b);
    CHECKF(hr == S_OK && music_b == music, "MusicProperties is the same object each time");
    IInspectable_Release(music_b);
    check_inspectable(music, &mc);

    check_string_prop(music, 6, "Title");
    check_string_prop(music, 8, "AlbumArtist");
    check_string_prop(music, 10, "Artist");
    /* AlbumArtist and Artist are separate properties */
    CALL_P(music, 11, v);
    hr = CALL_P(music, 8, &s);
    CHECKF(hr == S_OK && hs_eq(s, L"Some Value"), "AlbumArtist is not changed by Artist");
    WindowsDeleteString(s);
    hr = IInspectable_QueryInterface(music, &IID_IMusic2, &music2);
    CHECKF(hr == S_OK, "IMusicDisplayProperties2");
    if (music2)
    {
        check_string_prop(music2, 6, "AlbumTitle");
        hr = CALL_P(music2, 8, &n);
        CHECKF(hr == S_OK && n == 0, "TrackNumber is 0 by default (%u)", n);
        CHECKF(CALL_U(music2, 9, 7) == S_OK, "put_TrackNumber");
        CALL_P(music2, 8, &n);
        CHECKF(n == 7, "TrackNumber round-trips (%u)", n);
        hr = CALL_P(music2, 8, NULL);
        CHECKF(hr == E_POINTER, "TrackNumber: NULL out pointer is E_POINTER");
        check_genres(music2);
    }

    /* Type, then ClearAll puts everything back */
    CHECKF(CALL_U(updater, 7, 1) == S_OK, "put_Type(Music)");
    CALL_P(updater, 6, &type);
    CHECKF(type == 1, "Type round-trips (%d)", type);
    hr = CALL_U(updater, 7, 4);
    CHECKF(hr == E_INVALIDARG, "put_Type(4) is E_INVALIDARG (%#lx)", hr);
    CALL_P(updater, 9, v);
    obj_ref = 1;
    CALL_P(updater, 11, &the_obj);
    hr = CALL_V(updater, 16);
    CHECKF(hr == S_OK, "ClearAll (%#lx)", hr);
    CALL_P(updater, 6, &type);
    CHECKF(type == 0, "ClearAll: Type is Unknown (%d)", type);
    CALL_P(updater, 8, &s);
    CHECKF(hs_eq(s, L""), "ClearAll: AppMediaId is empty");
    WindowsDeleteString(s);
    CALL_P(updater, 10, &thumb);
    CHECKF(!thumb && obj_ref == 1, "ClearAll: Thumbnail is gone and released (%ld)", obj_ref);
    CALL_P(music, 6, &s);
    CHECKF(hs_eq(s, L""), "ClearAll: Title is empty");
    WindowsDeleteString(s);
    CALL_P(music, 10, &s);
    CHECKF(hs_eq(s, L""), "ClearAll: Artist is empty");
    WindowsDeleteString(s);
    if (music2)
    {
        CALL_P(music2, 8, &n);
        CHECKF(n == 0, "ClearAll: TrackNumber is 0 (%u)", n);
        CALL_P(music2, 6, &s);
        CHECKF(hs_eq(s, L""), "ClearAll: AlbumTitle is empty");
        WindowsDeleteString(s);
        IInspectable_Release(music2);
    }
    CHECKF(CALL_V(updater, 17) == S_OK, "Update");
    IInspectable_Release(music);
    WindowsDeleteString(v);
}

static void check_smtc(void *interop)
{
    HWND window = CreateWindowExA(0, "static", 0, 0, 0, 0, 100, 100, NULL, NULL, GetModuleHandleA(NULL), 0);
    typedef HRESULT (WINAPI *f_get)(void *, HWND, REFIID, void **);
    void *smtc = NULL, *updater = NULL, *updater2 = NULL;
    struct inspectable_case sc = { "SystemMediaTransportControls", L"Windows.Media.SystemMediaTransportControls",
                                   { &IID_ISMTC } };
    HRESULT hr;
    int level = -1;
    BOOLEAN v;
    size_t i;

    hr = ((f_get)VT(interop)[6])(interop, window, &IID_Bogus, &smtc);
    CHECKF(hr == E_NOINTERFACE && !smtc, "GetForWindow with an unknown IID is E_NOINTERFACE (%#lx)", hr);
    hr = ((f_get)VT(interop)[6])(interop, window, &IID_ISMTC, &smtc);
    CHECKF(hr == S_OK && smtc, "GetForWindow (%#lx)", hr);
    if (!smtc) return;
    check_inspectable(smtc, &sc);

    hr = CALL_P(smtc, 9, &level);
    CHECKF(hr == S_OK && level == 2, "SoundLevel is Full (%#lx, %d)", hr, level);
    hr = CALL_P(smtc, 9, NULL);
    CHECKF(hr == E_POINTER, "SoundLevel: NULL out pointer is E_POINTER (%#lx)", hr);
    for (i = 0; i < ARRAY_SIZE(bools); i++)
    {
        v = 7;
        hr = CALL_P(smtc, bools[i].get, &v);
        CHECKF(hr == S_OK && v == bools[i].def, "%s is %d by default (%#lx, %d)", bools[i].name, bools[i].def, hr, v);
        hr = CALL_B(smtc, bools[i].get + 1, TRUE);
        CHECKF(hr == S_OK, "%s: put TRUE (%#lx)", bools[i].name, hr);
        v = 0;
        hr = CALL_P(smtc, bools[i].get, &v);
        CHECKF(hr == S_OK && v == TRUE, "%s round-trips TRUE (%d)", bools[i].name, v);
        CALL_B(smtc, bools[i].get + 1, FALSE);
        v = 7;
        CALL_P(smtc, bools[i].get, &v);
        CHECKF(v == FALSE, "%s round-trips FALSE (%d)", bools[i].name, v);
        hr = CALL_P(smtc, bools[i].get, NULL);
        CHECKF(hr == E_POINTER, "%s: NULL out pointer is E_POINTER (%#lx)", bools[i].name, hr);
    }
    /* the properties that were not touched keep their own values */
    CALL_B(smtc, 15, TRUE);
    v = 7;
    CALL_P(smtc, 18, &v);
    CHECKF(v == FALSE, "setting IsStopEnabled leaves IsRecordEnabled alone");
    CALL_B(smtc, 15, FALSE);

    check_events(smtc, 32, "ButtonPressed");
    check_events(smtc, 34, "PropertyChanged");

    hr = CALL_P(smtc, 8, &updater);
    CHECKF(hr == S_OK && updater, "DisplayUpdater (%#lx)", hr);
    hr = CALL_P(smtc, 8, &updater2);
    CHECKF(hr == S_OK && updater2 == updater, "DisplayUpdater is the same object each time");
    if (updater2) IInspectable_Release(updater2);
    if (updater)
    {
        check_updater(updater);
        IInspectable_Release(updater);
    }
    IInspectable_Release(smtc);
    DestroyWindow(window);
}

int main(void)
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    HSTRING cls = mk(L"Windows.Media.SystemMediaTransportControls");
    IActivationFactory *factory = NULL;
    struct inspectable_case fc = { "SMTC factory", L"Windows.Media.SystemMediaTransportControls",
                                   { &IID_IActivationFactory, &IID_ISMTCInterop } };
    HMODULE dll;
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    void *obj = (void *)1, *interop = NULL;
    IInspectable *inst = (IInspectable *)1;

    check(SUCCEEDED(hr), "RoInitialize");
    hr = RoGetActivationFactory(cls, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(cls);
    if (FAILED(hr)) { check(0, "RoGetActivationFactory"); return 1; }

    check_inspectable((IInspectable *)factory, &fc);
    hr = IActivationFactory_ActivateInstance(factory, &inst);
    CHECKF(hr == E_NOTIMPL && !inst, "ActivateInstance is E_NOTIMPL with a NULL result (%#lx)", hr);
    hr = IActivationFactory_QueryInterface(factory, &IID_ISMTCInterop, &interop);
    CHECKF(hr == S_OK, "the interop interface");
    if (interop)
    {
        check_smtc(interop);
        IUnknown_Release((IUnknown *)interop);
    }
    IActivationFactory_Release(factory);

    dll = LoadLibraryW(L"windows.media.mediacontrol.dll");
    get_class_object = dll ? (void *)GetProcAddress(dll, "DllGetClassObject") : NULL;
    if (get_class_object)
    {
        hr = get_class_object(&IID_Bogus, &IID_IUnknown, &obj);
        CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE && !obj, "DllGetClassObject: CLASS_E_CLASSNOTAVAILABLE and a NULL pointer (%#lx)", hr);
    }
    else check(0, "DllGetClassObject export");

    RoUninitialize();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
