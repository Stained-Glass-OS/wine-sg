/* windows.web JSON batch (patches/sg/2011), run by test/jsonweb-gate.sh.
 * Families: JsonValue parse / stringify / typed getters / statics, JsonObject
 * named getters and SetNamedValue (members sorted by name), JsonArray typed
 * getters and bounds, activation of JsonObject and JsonArray, IStringable and
 * the runtime class names. The WinRT interfaces are declared here by hand.
 *
 *   jsonweb-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef struct Obj Obj;
typedef struct { HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 HRESULT (WINAPI *GetIids)(void *, ULONG *, IID **); HRESULT (WINAPI *GetRuntimeClassName)(void *, HSTRING *); HRESULT (WINAPI *GetTrustLevel)(void *, int *); } Base;
typedef struct { Base b; HRESULT (WINAPI *get_ValueType)(void *, int *); HRESULT (WINAPI *Stringify)(void *, HSTRING *); HRESULT (WINAPI *GetString)(void *, HSTRING *);
                 HRESULT (WINAPI *GetNumber)(void *, double *); HRESULT (WINAPI *GetBoolean)(void *, unsigned char *); HRESULT (WINAPI *GetArray)(void *, void **); HRESULT (WINAPI *GetObject)(void *, void **); } ValueVtbl;
typedef struct { Base b; HRESULT (WINAPI *GetNamedValue)(void *, HSTRING, void **); HRESULT (WINAPI *SetNamedValue)(void *, HSTRING, void *); HRESULT (WINAPI *GetNamedObject)(void *, HSTRING, void **);
                 HRESULT (WINAPI *GetNamedArray)(void *, HSTRING, void **); HRESULT (WINAPI *GetNamedString)(void *, HSTRING, HSTRING *); HRESULT (WINAPI *GetNamedNumber)(void *, HSTRING, double *);
                 HRESULT (WINAPI *GetNamedBoolean)(void *, HSTRING, unsigned char *); } ObjectVtbl;
typedef struct { Base b; HRESULT (WINAPI *GetObjectAt)(void *, UINT32, void **); HRESULT (WINAPI *GetArrayAt)(void *, UINT32, void **); HRESULT (WINAPI *GetStringAt)(void *, UINT32, HSTRING *);
                 HRESULT (WINAPI *GetNumberAt)(void *, UINT32, double *); HRESULT (WINAPI *GetBooleanAt)(void *, UINT32, unsigned char *); } ArrayVtbl;
typedef struct { Base b; HRESULT (WINAPI *Parse)(void *, HSTRING, void **); HRESULT (WINAPI *TryParse)(void *, HSTRING, void **, unsigned char *);
                 HRESULT (WINAPI *CreateBooleanValue)(void *, unsigned char, void **); HRESULT (WINAPI *CreateNumberValue)(void *, double, void **); HRESULT (WINAPI *CreateStringValue)(void *, HSTRING, void **); } StaticsVtbl;
typedef struct { Base b; HRESULT (WINAPI *ToString)(void *, HSTRING *); } StringableVtbl;
typedef struct { Base b; HRESULT (WINAPI *GetAt)(void *, UINT32, void **); HRESULT (WINAPI *get_Size)(void *, UINT32 *); HRESULT (WINAPI *GetView)(void *, void **);
                 HRESULT (WINAPI *IndexOf)(void *, void *, UINT32 *, unsigned char *); HRESULT (WINAPI *SetAt)(void *, UINT32, void *); HRESULT (WINAPI *InsertAt)(void *, UINT32, void *);
                 HRESULT (WINAPI *RemoveAt)(void *, UINT32); HRESULT (WINAPI *Append)(void *, void *); HRESULT (WINAPI *RemoveAtEnd)(void *); HRESULT (WINAPI *Clear)(void *);
                 HRESULT (WINAPI *GetMany)(void *, UINT32, UINT32, void **, UINT32 *); HRESULT (WINAPI *ReplaceAll)(void *, UINT32, void **); } VectorVtbl;
typedef struct { Base b; HRESULT (WINAPI *First)(void *, void **); } IterableVtbl;
typedef struct { Base b; HRESULT (WINAPI *get_Current)(void *, void **); HRESULT (WINAPI *get_HasCurrent)(void *, unsigned char *); HRESULT (WINAPI *MoveNext)(void *, unsigned char *);
                 HRESULT (WINAPI *GetMany)(void *, UINT32, void **, UINT32 *); } IteratorVtbl;
typedef struct { Base b; HRESULT (WINAPI *Lookup)(void *, HSTRING, void **); HRESULT (WINAPI *get_Size)(void *, UINT32 *); HRESULT (WINAPI *HasKey)(void *, HSTRING, unsigned char *);
                 HRESULT (WINAPI *GetView)(void *, void **); HRESULT (WINAPI *Insert)(void *, HSTRING, void *, unsigned char *); HRESULT (WINAPI *Remove)(void *, HSTRING);
                 HRESULT (WINAPI *Clear)(void *); } MapVtbl;
typedef struct { Base b; HRESULT (WINAPI *get_Key)(void *, HSTRING *); HRESULT (WINAPI *get_Value)(void *, void **); } PairVtbl;
struct Obj { const void *vtbl; };

#define BASE(o) (((const Base *)((Obj *)(o))->vtbl))
#define V(o) ((const ValueVtbl *)((Obj *)(o))->vtbl)
#define O(o) ((const ObjectVtbl *)((Obj *)(o))->vtbl)
#define A(o) ((const ArrayVtbl *)((Obj *)(o))->vtbl)
#define S(o) ((const StaticsVtbl *)((Obj *)(o))->vtbl)
#define VEC(o) ((const VectorVtbl *)((Obj *)(o))->vtbl)
#define ITB(o) ((const IterableVtbl *)((Obj *)(o))->vtbl)
#define ITR(o) ((const IteratorVtbl *)((Obj *)(o))->vtbl)
#define MAP(o) ((const MapVtbl *)((Obj *)(o))->vtbl)
#define PAIR(o) ((const PairVtbl *)((Obj *)(o))->vtbl)
#define STR(o) ((const StringableVtbl *)((Obj *)(o))->vtbl)

static const GUID IID_JsonValue = { 0xa3219ecb, 0xf0b3, 0x4dcd, { 0xbe, 0xee, 0x19, 0xd4, 0x8c, 0xd3, 0xed, 0x1e } };
static const GUID IID_JsonObject = { 0x064e24dd, 0x29c2, 0x4f83, { 0x9a, 0xc1, 0x9e, 0xe1, 0x15, 0x78, 0xbe, 0xb3 } };
static const GUID IID_JsonArray = { 0x08c1ddb6, 0x0cbd, 0x4a9a, { 0xb5, 0xd3, 0x2f, 0x85, 0x2d, 0xc3, 0x7e, 0x81 } };
static const GUID IID_JsonValueStatics = { 0x5f6b544a, 0x2f53, 0x48e1, { 0x91, 0xa3, 0xf7, 0x8b, 0x50, 0xa6, 0x34, 0x5c } };
static const GUID IID_Stringable = { 0x96369f54, 0x8eb6, 0x48f0, { 0xab, 0xce, 0xc1, 0xb2, 0x11, 0xe6, 0x27, 0xc3 } };
static const GUID IID_Unk = { 0, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID IID_Insp = { 0xaf86e2e0, 0xb12d, 0x4c6a, { 0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90 } };
static const GUID IID_Agile = { 0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 } };

static const GUID IID_Vector = { 0xd44662bc, 0xdce3, 0x59a8, { 0x92, 0x72, 0x4b, 0x21, 0x0f, 0x33, 0x90, 0x8b } };
static const GUID IID_Iterable = { 0xcb0492b6, 0x4113, 0x55cf, { 0xb2, 0xc5, 0x99, 0xeb, 0x42, 0x8b, 0xa4, 0x93 } };
static const GUID IID_Map = { 0xc9d9a725, 0x786b, 0x5113, { 0xb4, 0xb7, 0x9b, 0x61, 0x76, 0x4c, 0x22, 0x0b } };
static const GUID IID_IterablePairs = { 0xdfabb6e1, 0x0411, 0x5a8f, { 0xaa, 0x87, 0x35, 0x4e, 0x71, 0x10, 0xf0, 0x99 } };
static const GUID IID_VectorView = { 0xcffabb0f, 0x6bc4, 0x5ff6, { 0x9b, 0x9e, 0x7a, 0x9d, 0xf6, 0xc6, 0x87, 0xc8 } };
static const GUID IID_MapView = { 0xeecd690c, 0x1ff3, 0x529f, { 0x92, 0x3f, 0x9b, 0x1c, 0x31, 0xfd, 0x3d, 0x0f } };
#define ERR_CHANGED ((HRESULT)0x8000000c)

enum { Null, Boolean, Number, String, Array, Object };
#define ERR_STRING ((HRESULT)0x83750007)
#define ERR_NOTFOUND ((HRESULT)0x83750009)
#define ERR_ILLEGAL ((HRESULT)0x8000000e)
#define ERR_BOUNDS ((HRESULT)0x8000000b)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[300];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

static HSTRING hs(const WCHAR *s)
{
    HSTRING h = NULL;
    WindowsCreateString(s, s ? wcslen(s) : 0, &h);
    return h;
}

static int hs_is(HSTRING h, const WCHAR *want)
{
    return h && !wcscmp(WindowsGetStringRawBuffer(h, NULL), want);
}

static void *statics;

static void *parse(const WCHAR *text, HRESULT *hr)
{
    void *v = NULL;
    HSTRING h = hs(text);
    *hr = S(statics)->Parse(statics, h, &v);
    WindowsDeleteString(h);
    return v;
}

static int stringify_is(void *v, const WCHAR *want)
{
    HSTRING out = NULL;
    int ok = V(v)->Stringify(v, &out) == S_OK && hs_is(out, want);
    if (!ok && out) wprintf(L"   got: %ls\n", WindowsGetStringRawBuffer(out, NULL));
    WindowsDeleteString(out);
    return ok;
}

static int type_is(void *v, int want)
{
    int t = -1;
    return V(v)->get_ValueType(v, &t) == S_OK && t == want;
}

static void rel(void *o) { if (o) BASE(o)->Release(o); }

struct pcase { const WCHAR *in; int type; const WCHAR *out; const char *name; };

static void test_parse_table(void)
{
    static const struct pcase cases[] =
    {
        { L"null", Null, L"null", "null" },
        { L"true", Boolean, L"true", "true" },
        { L"false", Boolean, L"false", "false" },
        { L"  42 ", Number, L"42", "integer with spaces" },
        { L"-1.5", Number, L"-1.5", "negative fraction" },
        { L"1e3", Number, L"1000", "exponent" },
        { L"0.1", Number, L"0.1", "shortest round trip" },
        { L"123456789012", Number, L"123456789012", "large integer" },
        { L"1E+25", Number, L"1e+25", "large exponent" },
        { L"\"a\\n\\u0041\\\"\\\\\\/\"", String, L"\"a\\nA\\\"\\\\/\"", "string escapes" },
        { L"\"\\u0001\"", String, L"\"\\u0001\"", "control escaped on output" },
        { L"[1,2,[3]]", Array, L"[1,2,[3]]", "nested array" },
        { L"[ ]", Array, L"[]", "empty array" },
        { L"{ }", Object, L"{}", "empty object" },
        { L"{\"b\":1,\"a\":[true,null]}", Object, L"{\"a\":[true,null],\"b\":1}", "object members sorted" },
        { L"{\"a\":1,\"a\":2}", Object, L"{\"a\":2}", "duplicate key keeps the last" },
    };
    int i;
    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        HRESULT hr;
        void *v = parse(cases[i].in, &hr);
        char buf[200];
        snprintf(buf, sizeof(buf), "parse %s", cases[i].name);
        check(hr == S_OK && v && type_is(v, cases[i].type) && stringify_is(v, cases[i].out), buf);
        rel(v);
    }
}

static void test_parse_errors(void)
{
    static const WCHAR *bad[] = { L"", L"   ", L"{", L"}", L"[1,]", L"{\"a\":1,}", L"tru", L"nul", L"01", L"1.", L"-", L".5",
        L"\"abc", L"[1 2]", L"{1:2}", L"{\"a\" 1}", L"1 2", L"'a'", L"\"a\tb\"", L"\"\\x\"", L"\"\\u12\"", L"[1,2", L"1e", L"+1" };
    int i;
    for (i = 0; i < ARRAYSIZE(bad); i++)
    {
        HRESULT hr;
        void *v = parse(bad[i], &hr);
        char buf[200];
        snprintf(buf, sizeof(buf), "parse error %d", i);
        check(hr == ERR_STRING && v == NULL, buf);
        rel(v);
    }
    {
        WCHAR deep[600];
        HRESULT hr;
        void *v;
        int n;
        for (n = 0; n < 300; n++) deep[n] = '[';
        deep[n] = 0;
        v = parse(deep, &hr);
        check(hr == ERR_STRING && !v, "nesting beyond the limit is an error");
        for (n = 0; n < 100; n++) deep[n] = '[';
        for (; n < 200; n++) deep[n] = ']';
        deep[n] = 0;
        v = parse(deep, &hr);
        check(hr == S_OK && v, "nesting of 100 is fine");
        rel(v);
    }
    {
        void *v = (void *)1;
        unsigned char ok = 7;
        HSTRING h = hs(L"{\"x\":");
        HRESULT hr = S(statics)->TryParse(statics, h, &v, &ok);
        check(hr == S_OK && ok == 0 && v == NULL, "TryParse of bad text: S_OK, FALSE, no value");
        WindowsDeleteString(h);
        h = hs(L"[1]");
        hr = S(statics)->TryParse(statics, h, &v, &ok);
        check(hr == S_OK && ok == 1 && v && type_is(v, Array), "TryParse of good text");
        rel(v);
        WindowsDeleteString(h);
        checkhr(S(statics)->Parse(statics, NULL, &v), ERR_STRING, "Parse of a NULL string");
    }
}

static void test_getters(void)
{
    HRESULT hr;
    void *s = parse(L"\"text\"", &hr), *n = parse(L"2.5", &hr), *b = parse(L"true", &hr), *nul = parse(L"null", &hr);
    void *arr = parse(L"[1]", &hr), *obj = parse(L"{}", &hr);
    HSTRING hstr = NULL;
    double d = -1;
    unsigned char bo = 7;
    void *p = (void *)1;

    check(S_OK == V(s)->GetString(s, &hstr) && hs_is(hstr, L"text"), "GetString");
    WindowsDeleteString(hstr);
    checkhr(V(n)->GetString(n, &hstr), ERR_ILLEGAL, "GetString of a number");
    check(S_OK == V(n)->GetNumber(n, &d) && d == 2.5, "GetNumber");
    checkhr(V(s)->GetNumber(s, &d), ERR_ILLEGAL, "GetNumber of a string");
    checkhr(V(nul)->GetNumber(nul, &d), ERR_ILLEGAL, "GetNumber of null");
    check(S_OK == V(b)->GetBoolean(b, &bo) && bo == 1, "GetBoolean");
    checkhr(V(n)->GetBoolean(n, &bo), ERR_ILLEGAL, "GetBoolean of a number");
    check(S_OK == V(arr)->GetArray(arr, &p) && p, "GetArray");
    rel(p);
    checkhr(V(obj)->GetArray(obj, &p), ERR_ILLEGAL, "GetArray of an object");
    check(p == NULL, "failed GetArray clears the result");
    check(S_OK == V(obj)->GetObject(obj, &p) && p, "GetObject");
    {
        void *u1 = NULL, *u2 = NULL;
        BASE(p)->QueryInterface(p, &IID_Unk, &u1);
        BASE(obj)->QueryInterface(obj, &IID_Unk, &u2);
        check(u1 && u1 == u2, "GetObject returns the same object");
        rel(u1); rel(u2);
    }
    rel(p);
    checkhr(V(arr)->GetObject(arr, &p), ERR_ILLEGAL, "GetObject of an array");
    checkhr(V(s)->GetString(s, NULL), E_POINTER, "GetString(NULL)");
    checkhr(V(s)->get_ValueType(s, NULL), E_POINTER, "get_ValueType(NULL)");
    checkhr(V(s)->Stringify(s, NULL), E_POINTER, "Stringify(NULL)");
    rel(s); rel(n); rel(b); rel(nul); rel(arr); rel(obj);
}

static void test_create(void)
{
    void *v = NULL;
    HSTRING h;

    check(S_OK == S(statics)->CreateBooleanValue(statics, 1, &v) && type_is(v, Boolean) && stringify_is(v, L"true"), "CreateBooleanValue");
    rel(v);
    check(S_OK == S(statics)->CreateNumberValue(statics, 0.5, &v) && type_is(v, Number) && stringify_is(v, L"0.5"), "CreateNumberValue");
    rel(v);
    check(S_OK == S(statics)->CreateNumberValue(statics, 1e21, &v) && stringify_is(v, L"1e+21"), "CreateNumberValue 1e21");
    rel(v);
    check(S_OK == S(statics)->CreateNumberValue(statics, -3, &v) && stringify_is(v, L"-3"), "CreateNumberValue integer");
    rel(v);
    { double zero = 0; double nan = zero / zero, inf = 1 / zero;
      checkhr(S(statics)->CreateNumberValue(statics, nan, &v), E_INVALIDARG, "CreateNumberValue(NaN)");
      checkhr(S(statics)->CreateNumberValue(statics, inf, &v), E_INVALIDARG, "CreateNumberValue(Infinity)"); }
    h = hs(L"q\"uote");
    check(S_OK == S(statics)->CreateStringValue(statics, h, &v) && type_is(v, String) && stringify_is(v, L"\"q\\\"uote\""), "CreateStringValue escapes");
    rel(v);
    WindowsDeleteString(h);
}

static void test_object(void)
{
    HRESULT hr;
    void *val = parse(L"{\"s\":\"str\",\"n\":3,\"b\":false,\"o\":{\"k\":1},\"a\":[\"x\"],\"z\":null}", &hr);
    void *obj = NULL, *member = NULL, *other = NULL, *p = NULL;
    HSTRING name, out = NULL;
    double d = 0;
    unsigned char bo = 7;

    checkhr(BASE(val)->QueryInterface(val, &IID_JsonObject, &obj), S_OK, "QI IJsonObject on an object");
    checkhr(BASE(val)->QueryInterface(val, &IID_JsonArray, &p), E_NOINTERFACE, "QI IJsonArray on an object");
    name = hs(L"s");
    check(S_OK == O(obj)->GetNamedString(obj, name, &out) && hs_is(out, L"str"), "GetNamedString");
    WindowsDeleteString(out); WindowsDeleteString(name);
    name = hs(L"n");
    check(S_OK == O(obj)->GetNamedNumber(obj, name, &d) && d == 3, "GetNamedNumber");
    checkhr(O(obj)->GetNamedString(obj, name, &out), ERR_ILLEGAL, "GetNamedString of a number");
    WindowsDeleteString(name);
    name = hs(L"b");
    check(S_OK == O(obj)->GetNamedBoolean(obj, name, &bo) && bo == 0, "GetNamedBoolean");
    WindowsDeleteString(name);
    name = hs(L"o");
    check(S_OK == O(obj)->GetNamedObject(obj, name, &p) && p, "GetNamedObject");
    rel(p);
    checkhr(O(obj)->GetNamedArray(obj, name, &p), ERR_ILLEGAL, "GetNamedArray of an object");
    WindowsDeleteString(name);
    name = hs(L"a");
    check(S_OK == O(obj)->GetNamedArray(obj, name, &p) && p, "GetNamedArray");
    rel(p);
    WindowsDeleteString(name);
    name = hs(L"z");
    check(S_OK == O(obj)->GetNamedValue(obj, name, &member) && type_is(member, Null), "GetNamedValue of null");
    rel(member);
    WindowsDeleteString(name);
    name = hs(L"nope");
    checkhr(O(obj)->GetNamedValue(obj, name, &member), ERR_NOTFOUND, "GetNamedValue of a missing name");
    check(member == NULL, "missing name leaves no value");
    checkhr(O(obj)->GetNamedString(obj, name, &out), ERR_NOTFOUND, "GetNamedString of a missing name");
    checkhr(O(obj)->GetNamedNumber(obj, name, &d), ERR_NOTFOUND, "GetNamedNumber of a missing name");
    WindowsDeleteString(name);
    name = hs(L"S");
    checkhr(O(obj)->GetNamedValue(obj, name, &member), ERR_NOTFOUND, "names are case sensitive");
    WindowsDeleteString(name);

    /* SetNamedValue: add and replace */
    S(statics)->CreateNumberValue(statics, 99, &other);
    name = hs(L"m");
    checkhr(O(obj)->SetNamedValue(obj, name, other), S_OK, "SetNamedValue adds");
    checkhr(O(obj)->GetNamedNumber(obj, name, &d), S_OK, "the added member reads back");
    check(d == 99, "the added member's value");
    rel(other);
    S(statics)->CreateBooleanValue(statics, 1, &other);
    checkhr(O(obj)->SetNamedValue(obj, name, other), S_OK, "SetNamedValue replaces");
    check(S_OK == O(obj)->GetNamedBoolean(obj, name, &bo) && bo == 1, "the replacement reads back");
    checkhr(O(obj)->SetNamedValue(obj, name, NULL), E_POINTER, "SetNamedValue(NULL value)");
    WindowsDeleteString(name);
    rel(other);
    check(stringify_is(val, L"{\"a\":[\"x\"],\"b\":false,\"m\":true,\"n\":3,\"o\":{\"k\":1},\"s\":\"str\",\"z\":null}"),
          "object text after set, members sorted");
    rel(obj); rel(val);
}

static void test_array(void)
{
    HRESULT hr;
    void *val = parse(L"[\"s\",4,true,{\"k\":1},[2]]", &hr);
    void *arr = NULL, *p = NULL;
    HSTRING out = NULL;
    double d = 0;
    unsigned char bo = 7;

    checkhr(BASE(val)->QueryInterface(val, &IID_JsonArray, &arr), S_OK, "QI IJsonArray on an array");
    checkhr(BASE(val)->QueryInterface(val, &IID_JsonObject, &p), E_NOINTERFACE, "QI IJsonObject on an array");
    check(S_OK == A(arr)->GetStringAt(arr, 0, &out) && hs_is(out, L"s"), "GetStringAt");
    WindowsDeleteString(out);
    check(S_OK == A(arr)->GetNumberAt(arr, 1, &d) && d == 4, "GetNumberAt");
    check(S_OK == A(arr)->GetBooleanAt(arr, 2, &bo) && bo == 1, "GetBooleanAt");
    check(S_OK == A(arr)->GetObjectAt(arr, 3, &p) && p, "GetObjectAt");
    rel(p);
    check(S_OK == A(arr)->GetArrayAt(arr, 4, &p) && p, "GetArrayAt");
    rel(p);
    checkhr(A(arr)->GetStringAt(arr, 1, &out), ERR_ILLEGAL, "GetStringAt of a number");
    checkhr(A(arr)->GetNumberAt(arr, 5, &d), ERR_BOUNDS, "GetNumberAt past the end");
    checkhr(A(arr)->GetObjectAt(arr, 0xffffffff, &p), ERR_BOUNDS, "GetObjectAt far past the end");
    check(p == NULL, "out of range leaves no result");
    rel(arr); rel(val);
}


static void *mknum(double d) { void *v = NULL; S(statics)->CreateNumberValue(statics, d, &v); return v; }
static void *mkstr(const WCHAR *t) { void *v = NULL; HSTRING h = hs(t); S(statics)->CreateStringValue(statics, h, &v); WindowsDeleteString(h); return v; }

static void test_vector(void)
{
    HRESULT hr;
    void *val = parse(L"[1,2,3]", &hr), *vec = NULL, *it = NULL, *x = NULL, *a, *b;
    UINT32 size = 99, idx = 99, n = 0;
    unsigned char found = 7, has = 7;
    void *many[4];
    double d;

    checkhr(BASE(val)->QueryInterface(val, &IID_Vector, &vec), S_OK, "QI IVector on an array");
    checkhr(BASE(val)->QueryInterface(val, &IID_Iterable, &it), S_OK, "QI IIterable on an array");
    checkhr(BASE(val)->QueryInterface(val, &IID_Map, &x), E_NOINTERFACE, "QI IMap on an array");
    check(S_OK == VEC(vec)->get_Size(vec, &size) && size == 3, "IVector size");
    check(S_OK == VEC(vec)->GetAt(vec, 1, &x) && type_is(x, Number) && S_OK == V(x)->GetNumber(x, &d) && d == 2, "GetAt(1)");
    rel(x);
    checkhr(VEC(vec)->GetAt(vec, 3, &x), ERR_BOUNDS, "GetAt past the end");
    check(x == NULL, "failed GetAt clears the result");

    a = mkstr(L"z");
    checkhr(VEC(vec)->Append(vec, a), S_OK, "Append");
    checkhr(VEC(vec)->Append(vec, NULL), E_POINTER, "Append(NULL)");
    check(stringify_is(val, L"[1,2,3,\"z\"]"), "array text after Append");
    checkhr(VEC(vec)->IndexOf(vec, a, &idx, &found), S_OK, "IndexOf");
    check(found == 1 && idx == 3, "IndexOf finds the appended item");
    b = mknum(5);
    VEC(vec)->IndexOf(vec, b, &idx, &found);
    check(found == 0 && idx == 0, "IndexOf of an item not in the array");
    checkhr(VEC(vec)->InsertAt(vec, 0, b), S_OK, "InsertAt(0)");
    check(stringify_is(val, L"[5,1,2,3,\"z\"]"), "array text after InsertAt(0)");
    checkhr(VEC(vec)->InsertAt(vec, 5, b), S_OK, "InsertAt(size)");
    checkhr(VEC(vec)->InsertAt(vec, 9, b), ERR_BOUNDS, "InsertAt past the size");
    check(stringify_is(val, L"[5,1,2,3,\"z\",5]"), "array text after InsertAt(size)");
    checkhr(VEC(vec)->RemoveAt(vec, 0), S_OK, "RemoveAt(0)");
    checkhr(VEC(vec)->RemoveAtEnd(vec), S_OK, "RemoveAtEnd");
    checkhr(VEC(vec)->RemoveAt(vec, 40), ERR_BOUNDS, "RemoveAt out of range");
    check(stringify_is(val, L"[1,2,3,\"z\"]"), "array text after removals");
    checkhr(VEC(vec)->SetAt(vec, 0, b), S_OK, "SetAt");
    checkhr(VEC(vec)->SetAt(vec, 4, b), ERR_BOUNDS, "SetAt out of range");
    check(stringify_is(val, L"[5,2,3,\"z\"]"), "array text after SetAt");
    checkhr(VEC(vec)->GetMany(vec, 1, 2, many, &n), S_OK, "GetMany");
    check(n == 2 && type_is(many[0], Number) && type_is(many[1], Number), "GetMany items");
    rel(many[0]); rel(many[1]);
    checkhr(VEC(vec)->GetMany(vec, 4, 3, many, &n), S_OK, "GetMany at the end");
    check(n == 0, "GetMany at the end gives none");
    checkhr(VEC(vec)->GetMany(vec, 9, 3, many, &n), ERR_BOUNDS, "GetMany past the end");

    /* iteration */
    {
        void *iter = NULL, *cur;
        double sum = 0;
        int count = 0;
        VEC(vec)->Clear(vec);
        check(stringify_is(val, L"[]"), "Clear empties the array");
        checkhr(VEC(vec)->RemoveAtEnd(vec), ERR_BOUNDS, "RemoveAtEnd of an empty array");
        { void *items[3] = { mknum(10), mknum(20), mknum(30) };
          checkhr(VEC(vec)->ReplaceAll(vec, 3, items), S_OK, "ReplaceAll");
          rel(items[0]); rel(items[1]); rel(items[2]); }
        check(stringify_is(val, L"[10,20,30]"), "array text after ReplaceAll");
        checkhr(ITB(it)->First(it, &iter), S_OK, "First");
        while (ITR(iter)->get_HasCurrent(iter, &has) == S_OK && has)
        {
            if (ITR(iter)->get_Current(iter, &cur) == S_OK) { double v; V(cur)->GetNumber(cur, &v); sum += v; rel(cur); }
            count++;
            ITR(iter)->MoveNext(iter, &has);
        }
        check(count == 3 && sum == 60, "iterating visits every item");
        checkhr(ITR(iter)->get_Current(iter, &cur), ERR_BOUNDS, "Current past the end");
        rel(iter);
        checkhr(ITB(it)->First(it, &iter), S_OK, "First again");
        VEC(vec)->Append(vec, a);
        checkhr(ITR(iter)->MoveNext(iter, &has), ERR_CHANGED, "MoveNext after the array changed");
        rel(iter);
        checkhr(ITB(it)->First(it, &iter), S_OK, "First on the changed array");
        checkhr(ITR(iter)->GetMany(iter, 3, many, &n), S_OK, "iterator GetMany");
        check(n == 3, "iterator GetMany count");
        for (count = 0; count < (int)n; count++) rel(many[count]);
        rel(iter);
    }
    rel(a); rel(b); rel(vec); rel(it); rel(val);
}

static void test_map(void)
{
    HRESULT hr;
    void *val = parse(L"{\"b\":\"x\",\"a\":1}", &hr), *map = NULL, *it = NULL, *x = NULL, *num;
    UINT32 size = 99;
    unsigned char found = 7, replaced = 7;
    HSTRING k;
    double d;

    checkhr(BASE(val)->QueryInterface(val, &IID_Map, &map), S_OK, "QI IMap on an object");
    checkhr(BASE(val)->QueryInterface(val, &IID_IterablePairs, &it), S_OK, "QI IIterable of pairs on an object");
    checkhr(BASE(val)->QueryInterface(val, &IID_Vector, &x), E_NOINTERFACE, "QI IVector on an object");
    check(S_OK == MAP(map)->get_Size(map, &size) && size == 2, "IMap size");
    k = hs(L"a");
    check(S_OK == MAP(map)->HasKey(map, k, &found) && found == 1, "HasKey of a member");
    check(S_OK == MAP(map)->Lookup(map, k, &x) && S_OK == V(x)->GetNumber(x, &d) && d == 1, "Lookup");
    rel(x);
    WindowsDeleteString(k);
    k = hs(L"zz");
    check(S_OK == MAP(map)->HasKey(map, k, &found) && found == 0, "HasKey of a missing name");
    checkhr(MAP(map)->Lookup(map, k, &x), ERR_BOUNDS, "Lookup of a missing name");
    check(x == NULL, "failed Lookup clears the result");
    checkhr(MAP(map)->Remove(map, k), ERR_BOUNDS, "Remove of a missing name");
    num = mknum(7);
    checkhr(MAP(map)->Insert(map, k, num, &replaced), S_OK, "Insert a new name");
    check(replaced == 0, "Insert reports no replacement");
    checkhr(MAP(map)->Insert(map, k, num, &replaced), S_OK, "Insert an existing name");
    check(replaced == 1, "Insert reports the replacement");
    checkhr(MAP(map)->Insert(map, k, NULL, &replaced), E_POINTER, "Insert(NULL value)");
    WindowsDeleteString(k);
    k = hs(L"b");
    checkhr(MAP(map)->Remove(map, k), S_OK, "Remove a member");
    check(S_OK == MAP(map)->get_Size(map, &size) && size == 2, "size after Insert and Remove");
    WindowsDeleteString(k);
    check(stringify_is(val, L"{\"a\":1,\"zz\":7}"), "object text after map changes");

    /* the pairs, in name order */
    {
        void *iter = NULL, *pair = NULL;
        WCHAR names[16] = L"";
        unsigned char has = 7;
        int n = 0;
        checkhr(ITB(it)->First(it, &iter), S_OK, "First of pairs");
        while (ITR(iter)->get_HasCurrent(iter, &has) == S_OK && has && n < 5)
        {
            HSTRING key = NULL;
            void *value = NULL;
            if (ITR(iter)->get_Current(iter, &pair) == S_OK)
            {
                PAIR(pair)->get_Key(pair, &key);
                PAIR(pair)->get_Value(pair, &value);
                if (key) wcscat(names, WindowsGetStringRawBuffer(key, NULL));
                check(value && (n == 0 ? type_is(value, Number) : type_is(value, Number)), "pair value");
                WindowsDeleteString(key); rel(value); rel(pair);
            }
            n++;
            ITR(iter)->MoveNext(iter, &has);
        }
        check(n == 2 && !wcscmp(names, L"azz"), "pairs come in name order");
        rel(iter);
        checkhr(ITB(it)->First(it, &iter), S_OK, "First of pairs again");
        { void *pairs[3]; UINT32 got = 0;
          checkhr(ITR(iter)->GetMany(iter, 3, pairs, &got), S_OK, "pair iterator GetMany");
          check(got == 2, "pair GetMany count");
          if (got == 2) { rel(pairs[0]); rel(pairs[1]); } }
        MAP(map)->Clear(map);
        checkhr(ITR(iter)->MoveNext(iter, &has), ERR_CHANGED, "MoveNext after Clear");
        rel(iter);
    }
    check(stringify_is(val, L"{}"), "Clear empties the object");
    rel(num); rel(map); rel(it); rel(val);
}

/* GetView: read-only looks at an array and at an object */
static void test_views(void)
{
    typedef struct { Base b; HRESULT (WINAPI *GetAt)(void *, UINT32, void **); HRESULT (WINAPI *get_Size)(void *, UINT32 *);
                     HRESULT (WINAPI *IndexOf)(void *, void *, UINT32 *, unsigned char *);
                     HRESULT (WINAPI *GetMany)(void *, UINT32, UINT32, void **, UINT32 *); } VViewVtbl;
    typedef struct { Base b; HRESULT (WINAPI *Lookup)(void *, HSTRING, void **); HRESULT (WINAPI *get_Size)(void *, UINT32 *);
                     HRESULT (WINAPI *HasKey)(void *, HSTRING, unsigned char *);
                     HRESULT (WINAPI *Split)(void *, void **, void **); } MViewVtbl;
#define VV(o) ((const VViewVtbl *)((Obj *)(o))->vtbl)
#define MV(o) ((const MViewVtbl *)((Obj *)(o))->vtbl)
    HRESULT hr;
    void *arr = parse(L"[1,2,3]", &hr), *vec = NULL, *view = NULL, *it = NULL, *x = NULL, *first = (void *)1, *second = (void *)1;
    void *obj = parse(L"{\"a\":1,\"b\":\"two\"}", &hr), *map = NULL, *mview = NULL;
    UINT32 size = 99, idx = 99, n = 0;
    unsigned char found = 7;
    void *many[4];
    HSTRING key;

    BASE(arr)->QueryInterface(arr, &IID_Vector, &vec);
    checkhr(VEC(vec)->GetView(vec, &view), S_OK, "IVector::GetView");
    check(view != NULL, "the view");
    checkhr(VEC(vec)->GetView(vec, NULL), E_POINTER, "GetView(NULL)");
    check(S_OK == VV(view)->get_Size(view, &size) && size == 3, "view size");
    check(S_OK == VV(view)->GetAt(view, 2, &x) && type_is(x, Number), "view GetAt(2)");
    rel(x);
    checkhr(VV(view)->GetAt(view, 3, &x), ERR_BOUNDS, "view GetAt past the end");
    {
        void *two = NULL;
        VEC(vec)->GetAt(vec, 1, &two);
        checkhr(VV(view)->IndexOf(view, two, &idx, &found), S_OK, "view IndexOf");
        check(found == 1 && idx == 1, "view IndexOf finds the item");
        rel(two);
    }
    checkhr(VV(view)->GetMany(view, 1, 4, many, &n), S_OK, "view GetMany");
    check(n == 2, "view GetMany count");
    rel(many[0]); rel(many[1]);
    checkhr(BASE(view)->QueryInterface(view, &IID_VectorView, &x), S_OK, "QI IVectorView on the view");
    rel(x);
    checkhr(BASE(view)->QueryInterface(view, &IID_Iterable, &it), S_OK, "QI IIterable on the view");
    {
        void *iter = NULL;
        check(S_OK == ITB(it)->First(it, &iter) && iter != NULL, "iterating through the view");
        rel(iter);
    }
    rel(it);
    checkhr(BASE(view)->QueryInterface(view, &IID_Map, &x), E_NOINTERFACE, "the view of an array is no map");
    /* the view follows the array */
    {
        void *extra = mkstr(L"new");
        VEC(vec)->Append(vec, extra);
        check(S_OK == VV(view)->get_Size(view, &size) && size == 4, "the view sees an item appended");
        rel(extra);
    }
    rel(vec);
    rel(arr);
    check(S_OK == VV(view)->get_Size(view, &size) && size == 4, "the view keeps the array alive");
    rel(view);

    BASE(obj)->QueryInterface(obj, &IID_Map, &map);
    checkhr(MAP(map)->GetView(map, &mview), S_OK, "IMap::GetView");
    checkhr(MAP(map)->GetView(map, NULL), E_POINTER, "IMap::GetView(NULL)");
    check(S_OK == MV(mview)->get_Size(mview, &size) && size == 2, "map view size");
    key = hs(L"b");
    check(S_OK == MV(mview)->HasKey(mview, key, &found) && found == 1, "map view HasKey");
    check(S_OK == MV(mview)->Lookup(mview, key, &x) && type_is(x, String), "map view Lookup");
    rel(x);
    WindowsDeleteString(key);
    key = hs(L"nope");
    check(S_OK == MV(mview)->HasKey(mview, key, &found) && found == 0, "map view HasKey of a missing key");
    checkhr(MV(mview)->Lookup(mview, key, &x), ERR_BOUNDS, "map view Lookup of a missing key");
    WindowsDeleteString(key);
    checkhr(MV(mview)->Split(mview, &first, &second), S_OK, "map view Split");
    check(first == NULL && second == NULL, "Split of one view gives none");
    checkhr(BASE(mview)->QueryInterface(mview, &IID_MapView, &x), S_OK, "QI IMapView on the view");
    rel(x);
    checkhr(BASE(mview)->QueryInterface(mview, &IID_IterablePairs, &it), S_OK, "QI IIterable of pairs on the view");
    rel(it);
    checkhr(BASE(mview)->QueryInterface(mview, &IID_Vector, &x), E_NOINTERFACE, "the view of an object is no vector");
    rel(map);
    rel(obj);
    rel(mview);
#undef VV
#undef MV
}

static void test_activation_and_names(void)
{
    static const WCHAR *objname = L"Windows.Data.Json.JsonObject", *arrname = L"Windows.Data.Json.JsonArray";
    void *inst = NULL, *o = NULL, *a = NULL, *st = NULL, *val;
    HSTRING cls = hs(objname), n, out = NULL;
    HRESULT hr;

    hr = RoActivateInstance(cls, (IInspectable **)&inst);
    check(hr == S_OK && inst, "activate a JsonObject");
    WindowsDeleteString(cls);
    check(inst && BASE(inst)->QueryInterface(inst, &IID_JsonObject, &o) == S_OK, "the new JsonObject has IJsonObject");
    check(inst && BASE(inst)->QueryInterface(inst, &IID_Agile, &st) == S_OK, "and IAgileObject");
    rel(st);
    check(inst && BASE(inst)->QueryInterface(inst, &IID_JsonValue, &val) == S_OK && type_is(val, Object) && stringify_is(val, L"{}"), "the new JsonObject is an empty object value");
    BASE(inst)->GetRuntimeClassName(inst, &out);
    check(hs_is(out, objname), "runtime class name of an object");
    WindowsDeleteString(out);
    n = hs(L"k");
    { void *num; S(statics)->CreateNumberValue(statics, 7, &num); O(o)->SetNamedValue(o, n, num); rel(num); }
    WindowsDeleteString(n);
    check(inst && stringify_is(val, L"{\"k\":7}"), "an activated object takes members");
    {
        void *str = NULL;
        out = NULL;
        check(BASE(val)->QueryInterface(val, &IID_Stringable, &str) == S_OK && STR(str)->ToString(str, &out) == S_OK && hs_is(out, L"{\"k\":7}"), "IStringable::ToString");
        WindowsDeleteString(out);
        rel(str);
    }
    rel(val); rel(o); rel(inst);

    cls = hs(arrname);
    inst = NULL;
    hr = RoActivateInstance(cls, (IInspectable **)&inst);
    check(hr == S_OK && inst, "activate a JsonArray");
    WindowsDeleteString(cls);
    check(inst && BASE(inst)->QueryInterface(inst, &IID_JsonArray, &a) == S_OK, "the new JsonArray has IJsonArray");
    check(inst && BASE(inst)->QueryInterface(inst, &IID_JsonValue, &val) == S_OK && type_is(val, Array) && stringify_is(val, L"[]"), "the new JsonArray is an empty array value");
    out = NULL;
    BASE(inst)->GetRuntimeClassName(inst, &out);
    check(hs_is(out, arrname), "runtime class name of an array");
    WindowsDeleteString(out);
    { double d; checkhr(A(a)->GetNumberAt(a, 0, &d), ERR_BOUNDS, "an empty array has no item 0"); }
    rel(val); rel(a); rel(inst);

    val = parse(L"5", &hr);
    out = NULL;
    BASE(val)->GetRuntimeClassName(val, &out);
    check(hs_is(out, L"Windows.Data.Json.JsonValue"), "runtime class name of a value");
    WindowsDeleteString(out);
    { int trust = -1; check(BASE(val)->GetTrustLevel(val, &trust) == S_OK && trust == 0, "trust level is BaseTrust"); }
    { ULONG n2 = 0; IID *ids = NULL; check(BASE(val)->GetIids(val, &n2, &ids) == S_OK && n2 == 2 && ids, "GetIids of a value"); CoTaskMemFree(ids); }
    rel(val);
    val = parse(L"[]", &hr);
    { ULONG n2 = 0; IID *ids = NULL; check(BASE(val)->GetIids(val, &n2, &ids) == S_OK && n2 == 5 && ids, "GetIids of an array"); CoTaskMemFree(ids); }
    rel(val);
    val = parse(L"{}", &hr);
    { ULONG n2 = 0; IID *ids = NULL; check(BASE(val)->GetIids(val, &n2, &ids) == S_OK && n2 == 5 && ids, "GetIids of an object"); CoTaskMemFree(ids); }
    rel(val);
    val = parse(L"5", &hr);
    { void *x = NULL; checkhr(BASE(val)->QueryInterface(val, &IID_Insp, &x), S_OK, "QI IInspectable"); rel(x); }
    check(BASE(val)->Release(val) == 0, "last release frees the value");
}

int main(void)
{
    HSTRING cls = hs(L"Windows.Data.Json.JsonValue");
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(cls, &IID_JsonValueStatics, &statics);
    WindowsDeleteString(cls);
    checkhr(hr, S_OK, "JsonValue statics");
    if (!statics) { printf("RESULT: FAIL\n"); return 1; }

    test_parse_table();
    test_parse_errors();
    test_getters();
    test_create();
    test_object();
    test_array();
    test_vector();
    test_map();
    test_views();
    test_activation_and_names();

    rel(statics);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    RoUninitialize();
    return failures != 0;
}
