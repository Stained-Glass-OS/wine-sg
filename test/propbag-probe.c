/* shlwapi batch (patches/sg/2040), run by test/propbag-gate.sh. Families: the
 * in-memory property bag (SHCreatePropertyBagOnMemory, read-only and writable)
 * and the typed helpers on any property bag: SHPropertyBag_Read/Write for
 * strings, numbers, booleans, GUIDs, points and rectangles (named name.x,
 * name.left, ...), and SHPropertyBag_Delete. Everything is looked up by ordinal.
 *
 *   propbag-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ocidl.h>
#include <oleauto.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef STG_E_ACCESSDENIED
#define STG_E_ACCESSDENIED ((HRESULT)0x80030005)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static HMODULE sh;
#define ORD(n) ((void *)GetProcAddress(sh ? sh : (sh = LoadLibraryA("shlwapi.dll")), (LPCSTR)(ULONG_PTR)(n)))

typedef HRESULT (WINAPI *create_t)(DWORD, REFIID, void **);
typedef HRESULT (WINAPI *rd_t)(IPropertyBag *, const WCHAR *, void *);
typedef HRESULT (WINAPI *wr_long_t)(IPropertyBag *, const WCHAR *, LONG);
typedef HRESULT (WINAPI *wr_ptr_t)(IPropertyBag *, const WCHAR *, const void *);
typedef HRESULT (WINAPI *rd_str_t)(IPropertyBag *, const WCHAR *, WCHAR *, int);
typedef HRESULT (WINAPI *rd_type_t)(IPropertyBag *, const WCHAR *, VARIANT *, VARTYPE);
typedef HRESULT (WINAPI *del_t)(IPropertyBag *, const WCHAR *);

int main(void)
{
    create_t create = ORD(477);
    rd_type_t read_type = ORD(493);
    rd_str_t read_str = ORD(494);
    wr_ptr_t write_str = ORD(495);
    rd_t read_long = ORD(496);
    wr_long_t write_long = ORD(497), write_bool = ORD(499), write_dword = ORD(508), write_short = ORD(528), write_int = ORD(530);
    rd_t read_guid = ORD(505), read_dword = ORD(507), read_bstr = ORD(520), read_pointl = ORD(521), read_rectl = ORD(523);
    rd_t read_points = ORD(525), read_short = ORD(527), read_int = ORD(529), read_bool = ORD(534);
    wr_ptr_t write_guid = ORD(506), write_pointl = ORD(522), write_rectl = ORD(524), write_points = ORD(526);
    del_t del = ORD(535);
    IPropertyBag *ro = NULL, *rw = NULL, *again = NULL;
    GUID guid = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } }, got;
    POINTL pl = { 3, -4 }, pl2;
    POINTS ps = { -7, 8 }, ps2;
    RECTL rl = { 10, 20, 30, 40 }, rl2;
    WCHAR buf[64];
    VARIANT var;
    LONG l = 0;
    DWORD dw = 0;
    SHORT sv = 0;
    INT iv = 0;
    BOOL b = FALSE;
    BSTR bs = NULL;
    HRESULT hr;

    CoInitialize(NULL);
    sh = LoadLibraryA("shlwapi.dll");
    CHECK(create && read_type && read_str && write_str && read_long && write_long && write_bool && write_dword);
    CHECK(read_guid && read_dword && read_bstr && read_pointl && read_rectl && read_points && read_short && read_int);
    CHECK(read_bool && write_guid && write_pointl && write_rectl && write_points && write_short && write_int && del);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    /* the bag */
    CHECK(create(0, &IID_IPropertyBag, (void **)&ro) == S_OK && ro);
    CHECK(create(STGM_READWRITE, &IID_IPropertyBag, (void **)&rw) == S_OK && rw);
    CHECK(create(0, &IID_IPropertyBag, NULL) == E_INVALIDARG);
    {
        IUnknown *unk = NULL;
        CHECK(create(0, &IID_IUnknown, (void **)&unk) == S_OK && unk);
        if (unk) IUnknown_Release(unk);
        CHECK(create(0, &IID_IPropertyBag2, (void **)&again) == E_NOINTERFACE && !again);
    }
    CHECK(write_long(ro, L"a", 1) == STG_E_ACCESSDENIED);
    CHECK(read_long(ro, L"a", &l) == E_INVALIDARG);

    /* numbers */
    CHECK(write_long(rw, L"a", -5) == S_OK);
    CHECK(read_long(rw, L"A", &l) == S_OK && l == -5);
    CHECK(read_int(rw, L"a", &iv) == S_OK && iv == -5);
    CHECK(write_int(rw, L"i", 77) == S_OK && read_int(rw, L"i", &iv) == S_OK && iv == 77);
    CHECK(write_dword(rw, L"d", 0xfffffff0) == S_OK && read_dword(rw, L"d", &dw) == S_OK && dw == 0xfffffff0);
    CHECK(write_short(rw, L"s", -300) == S_OK && read_short(rw, L"s", &sv) == S_OK && sv == -300);
    CHECK(read_dword(rw, L"i", &dw) == S_OK && dw == 77);
    CHECK(read_long(rw, L"missing", &l) == E_INVALIDARG);
    CHECK(read_long(NULL, L"a", &l) == E_INVALIDARG && read_long(rw, NULL, &l) == E_INVALIDARG && read_long(rw, L"a", NULL) == E_INVALIDARG);

    /* booleans */
    CHECK(write_bool(rw, L"yes", 5) == S_OK && read_bool(rw, L"yes", &b) == S_OK && b == TRUE);
    CHECK(write_bool(rw, L"no", 0) == S_OK && read_bool(rw, L"no", &b) == S_OK && b == FALSE);

    /* strings */
    CHECK(write_str(rw, L"str", L"h\xe9llo") == S_OK);
    memset(buf, 0, sizeof(buf));
    CHECK(read_str(rw, L"str", buf, 32) == S_OK && !wcscmp(buf, L"h\xe9llo"));
    memset(buf, 0, sizeof(buf));
    hr = read_str(rw, L"str", buf, 3);
    CHECK(FAILED(hr) && !wcscmp(buf, L"h\xe9"));
    CHECK(read_str(rw, L"str", buf, 0) == E_INVALIDARG);
    CHECK(read_bstr(rw, L"str", &bs) == S_OK && bs && !wcscmp(bs, L"h\xe9llo"));
    SysFreeString(bs);
    CHECK(read_long(rw, L"str", &l) == DISP_E_TYPEMISMATCH);
    CHECK(write_str(rw, L"num", L"42") == S_OK && read_long(rw, L"num", &l) == S_OK && l == 42);

    /* GUIDs */
    CHECK(write_guid(rw, L"g", &guid) == S_OK);
    memset(&got, 0, sizeof(got));
    CHECK(read_guid(rw, L"g", &got) == S_OK && IsEqualGUID(&got, &guid));
    memset(buf, 0, sizeof(buf));
    hr = read_str(rw, L"g", buf, 64);
    if (wcscmp(buf, L"{12345678-9ABC-DEF0-0102-030405060708}")) printf("   guid string [%ls]\n", buf);
    CHECK(hr == S_OK && !wcscmp(buf, L"{12345678-9ABC-DEF0-0102-030405060708}"));
    CHECK(read_guid(rw, L"str", &got) != S_OK);

    /* points and rectangles */
    CHECK(write_pointl(rw, L"p", &pl) == S_OK);
    CHECK(read_long(rw, L"p.x", &l) == S_OK && l == 3 && read_long(rw, L"p.y", &l) == S_OK && l == -4);
    memset(&pl2, 0, sizeof(pl2));
    CHECK(read_pointl(rw, L"p", &pl2) == S_OK && pl2.x == 3 && pl2.y == -4);
    CHECK(write_points(rw, L"q", &ps) == S_OK && read_points(rw, L"q", &ps2) == S_OK && ps2.x == -7 && ps2.y == 8);
    CHECK(write_rectl(rw, L"r", &rl) == S_OK);
    CHECK(read_long(rw, L"r.left", &l) == S_OK && l == 10 && read_long(rw, L"r.bottom", &l) == S_OK && l == 40);
    memset(&rl2, 0, sizeof(rl2));
    CHECK(read_rectl(rw, L"r", &rl2) == S_OK && rl2.left == 10 && rl2.top == 20 && rl2.right == 30 && rl2.bottom == 40);
    pl2.x = 99;
    CHECK(read_pointl(rw, L"nopoint", &pl2) == E_INVALIDARG && pl2.x == 99);
    CHECK(read_pointl(rw, L"p", NULL) == E_INVALIDARG);

    /* typed read, delete */
    VariantInit(&var);
    CHECK(read_type(rw, L"a", &var, VT_I4) == S_OK && V_VT(&var) == VT_I4 && V_I4(&var) == -5);
    VariantClear(&var);
    CHECK(read_type(rw, L"a", &var, VT_BSTR) == S_OK && V_VT(&var) == VT_BSTR && !wcscmp(V_BSTR(&var), L"-5"));
    VariantClear(&var);
    CHECK(read_type(rw, L"missing", &var, VT_I4) == E_INVALIDARG);
    CHECK(del(rw, L"a") == S_OK && read_long(rw, L"a", &l) == E_INVALIDARG);
    CHECK(del(rw, L"a") == S_OK);
    CHECK(del(ro, L"x") == STG_E_ACCESSDENIED);
    CHECK(del(NULL, L"x") == E_INVALIDARG);

    IPropertyBag_Release(ro);
    IPropertyBag_Release(rw);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
