/* propsys batch (patches/sg/2038), run by test/propvec-gate.sh. Families: the
 * element count and typed element accessors of a property variant, vectors
 * made from arrays, vectors read into caller or allocated arrays (every
 * number type, booleans, strings), the WithDefault conversions, and
 * ClearPropVariantArray. Looked up with GetProcAddress in propsys.dll.
 *
 *   propvec-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <propidl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef TYPE_E_BUFFERTOOSMALL
#define TYPE_E_BUFFERTOOSMALL ((HRESULT)0x80028016)
#endif
#define OVERFLOW_HR HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static HMODULE ps;
#define FN(type, name) type name = (type)GetProcAddress(ps, #name)

typedef ULONG (WINAPI *count_t)(const PROPVARIANT *);
typedef HRESULT (WINAPI *elem_t)(const PROPVARIANT *, ULONG, void *);
typedef HRESULT (WINAPI *init_t)(const void *, ULONG, PROPVARIANT *);
typedef HRESULT (WINAPI *tovec_t)(const PROPVARIANT *, void *, ULONG, ULONG *);
typedef HRESULT (WINAPI *tovecalloc_t)(const PROPVARIANT *, void **, ULONG *);

static void *fn(const char *name) { return (void *)GetProcAddress(ps, name); }

static void make_vec(PROPVARIANT *pv, VARTYPE vt, const void *data, ULONG count, ULONG size)
{
    memset(pv, 0, sizeof(*pv));
    pv->vt = vt | VT_VECTOR;
    pv->caub.cElems = count;
    pv->caub.pElems = CoTaskMemAlloc(count * size);
    memcpy(pv->caub.pElems, data, count * size);
}

int main(void)
{
    PROPVARIANT pv, out, arr[2];
    static const LONG longs[] = { 10, 20, 30 };
    static const SHORT shorts[] = { 5, -6 };
    static const BOOL bools[] = { 1, 0, 5 };
    static const double doubles[] = { 1.5, -2.25 };
    LONG lbuf[5], *lalloc = NULL, l;
    double dbuf[4];
    BOOL bbuf[4];
    ULONG n, u;
    ULONGLONG ull;
    WCHAR *s, *strs[4], **salloc;
    HRESULT hr;
    count_t count;
    elem_t get_i32, get_u32, get_i16, get_dbl, get_bool, get_str;
    init_t init_i32, init_bool, init_dbl, init_u64;
    tovec_t to_i32, to_dbl, to_bool, to_str;
    tovecalloc_t to_i32_alloc, to_str_alloc;
    HRESULT (WINAPI *to_i32_default)(const PROPVARIANT *, LONG);
    double (WINAPI *to_dbl_default)(const PROPVARIANT *, double);
    BOOL (WINAPI *to_bool_default)(const PROPVARIANT *, BOOL);
    void (WINAPI *clear_array)(PROPVARIANT *, UINT);

    CoInitialize(NULL);
    ps = LoadLibraryA("propsys.dll");
    count = fn("PropVariantGetElementCount");
    get_i32 = fn("PropVariantGetInt32Elem"); get_u32 = fn("PropVariantGetUInt32Elem"); get_i16 = fn("PropVariantGetInt16Elem");
    get_dbl = fn("PropVariantGetDoubleElem"); get_bool = fn("PropVariantGetBooleanElem"); get_str = fn("PropVariantGetStringElem");
    init_i32 = fn("InitPropVariantFromInt32Vector"); init_bool = fn("InitPropVariantFromBooleanVector");
    init_dbl = fn("InitPropVariantFromDoubleVector"); init_u64 = fn("InitPropVariantFromUInt64Vector");
    to_i32 = fn("PropVariantToInt32Vector"); to_dbl = fn("PropVariantToDoubleVector"); to_bool = fn("PropVariantToBooleanVector");
    to_str = fn("PropVariantToStringVector");
    to_i32_alloc = fn("PropVariantToInt32VectorAlloc"); to_str_alloc = fn("PropVariantToStringVectorAlloc");
    to_i32_default = fn("PropVariantToInt32WithDefault"); to_dbl_default = fn("PropVariantToDoubleWithDefault");
    to_bool_default = fn("PropVariantToBooleanWithDefault"); clear_array = fn("ClearPropVariantArray");
    CHECK(count && get_i32 && get_u32 && get_i16 && get_dbl && get_bool && get_str && init_i32 && init_bool && init_dbl && init_u64);
    CHECK(to_i32 && to_dbl && to_bool && to_str && to_i32_alloc && to_str_alloc && to_i32_default && to_dbl_default && to_bool_default && clear_array);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    /* the element count */
    PropVariantInit(&pv);
    CHECK(count(&pv) == 0);
    pv.vt = VT_NULL;
    CHECK(count(&pv) == 0);
    pv.vt = VT_I4; pv.lVal = 7;
    CHECK(count(&pv) == 1);
    pv.vt = VT_LPWSTR; pv.pwszVal = (WCHAR *)L"one";
    CHECK(count(&pv) == 1);
    make_vec(&pv, VT_I4, longs, 3, sizeof(LONG));
    CHECK(count(&pv) == 3);

    /* elements */
    l = 0;
    CHECK(get_i32(&pv, 1, &l) == S_OK && l == 20);
    CHECK(get_i32(&pv, 2, &l) == S_OK && l == 30);
    CHECK(get_i32(&pv, 3, &l) == E_INVALIDARG);
    CHECK(get_i32(&pv, 0, NULL) == E_INVALIDARG);
    u = 99;
    CHECK(get_u32(&pv, 0, &u) == S_OK && u == 10);
    CHECK(get_dbl(&pv, 0, &dbuf[0]) == S_OK && dbuf[0] == 10.0);
    PropVariantClear(&pv);
    make_vec(&pv, VT_I2, shorts, 2, sizeof(SHORT));
    l = 0;
    CHECK(get_i32(&pv, 1, &l) == S_OK && l == -6);
    CHECK(get_u32(&pv, 1, &u) == OVERFLOW_HR);
    PropVariantClear(&pv);
    pv.vt = VT_I4; pv.lVal = 42;
    CHECK(get_i32(&pv, 0, &l) == S_OK && l == 42);
    CHECK(get_i32(&pv, 1, &l) == E_INVALIDARG);
    PropVariantInit(&pv);
    CHECK(get_i32(&pv, 0, &l) == E_INVALIDARG);
    {
        WCHAR *strings[] = { (WCHAR *)L"alpha", (WCHAR *)L"beta" };
        PROPVARIANT sv;
        memset(&sv, 0, sizeof(sv));
        sv.vt = VT_LPWSTR | VT_VECTOR; sv.calpwstr.cElems = 2; sv.calpwstr.pElems = strings;
        s = NULL;
        CHECK(get_str(&sv, 1, &s) == S_OK && s && !wcscmp(s, L"beta"));
        CoTaskMemFree(s);
        CHECK(count(&sv) == 2);
        n = 0;
        memset(strs, 0, sizeof(strs));
        CHECK(to_str(&sv, strs, 4, &n) == S_OK && n == 2 && !wcscmp(strs[0], L"alpha") && !wcscmp(strs[1], L"beta"));
        CoTaskMemFree(strs[0]); CoTaskMemFree(strs[1]);
        CHECK(to_str(&sv, strs, 1, &n) == TYPE_E_BUFFERTOOSMALL);
        salloc = NULL; n = 0;
        CHECK(to_str_alloc(&sv, (void **)&salloc, &n) == S_OK && n == 2 && salloc && !wcscmp(salloc[1], L"beta"));
        if (salloc) { CoTaskMemFree(salloc[0]); CoTaskMemFree(salloc[1]); CoTaskMemFree(salloc); }
    }

    /* vectors from arrays */
    memset(&out, 0xcc, sizeof(out));
    CHECK(init_i32(longs, 3, &out) == S_OK && out.vt == (VT_I4 | VT_VECTOR) && out.caub.cElems == 3
          && ((LONG *)out.caub.pElems)[0] == 10 && ((LONG *)out.caub.pElems)[2] == 30);
    PropVariantClear(&out);
    CHECK(init_bool(bools, 3, &out) == S_OK && out.vt == (VT_BOOL | VT_VECTOR) && out.cabool.cElems == 3
          && out.cabool.pElems[0] == VARIANT_TRUE && out.cabool.pElems[1] == VARIANT_FALSE && out.cabool.pElems[2] == VARIANT_TRUE);
    PropVariantClear(&out);
    CHECK(init_dbl(doubles, 2, &out) == S_OK && out.vt == (VT_R8 | VT_VECTOR) && out.cadbl.pElems[1] == -2.25);
    PropVariantClear(&out);
    ull = 0x123456789abcdefULL;
    CHECK(init_u64(&ull, 1, &out) == S_OK && out.vt == (VT_UI8 | VT_VECTOR) && out.cauh.pElems[0].QuadPart == (LONGLONG)ull);
    PropVariantClear(&out);
    CHECK(init_i32(NULL, 2, &out) == E_INVALIDARG);
    CHECK(init_i32(longs, 3, NULL) == E_INVALIDARG);
    CHECK(init_i32(NULL, 0, &out) == S_OK && out.vt == (VT_I4 | VT_VECTOR) && out.caub.cElems == 0);
    PropVariantClear(&out);

    /* vectors into arrays */
    make_vec(&pv, VT_I4, longs, 3, sizeof(LONG));
    memset(lbuf, 0, sizeof(lbuf));
    n = 99;
    CHECK(to_i32(&pv, lbuf, 5, &n) == S_OK && n == 3 && lbuf[0] == 10 && lbuf[2] == 30);
    n = 99;
    CHECK(to_i32(&pv, lbuf, 2, &n) == TYPE_E_BUFFERTOOSMALL);
    CHECK(to_i32(&pv, lbuf, 5, NULL) == E_INVALIDARG);
    CHECK(to_dbl(&pv, dbuf, 4, &n) == S_OK && n == 3 && dbuf[1] == 20.0);
    CHECK(to_i32_alloc(&pv, (void **)&lalloc, &n) == S_OK && n == 3 && lalloc && lalloc[1] == 20);
    CoTaskMemFree(lalloc);
    PropVariantClear(&pv);
    pv.vt = VT_I4; pv.lVal = -9;
    n = 0;
    CHECK(to_i32(&pv, lbuf, 5, &n) == S_OK && n == 1 && lbuf[0] == -9);
    pv.vt = VT_BOOL; pv.boolVal = VARIANT_TRUE;
    CHECK(to_bool(&pv, bbuf, 4, &n) == S_OK && n == 1 && bbuf[0] == TRUE);
    PropVariantInit(&pv);
    n = 99;
    CHECK(to_i32(&pv, lbuf, 5, &n) == S_OK && n == 0);

    /* defaults */
    pv.vt = VT_I4; pv.lVal = 5;
    CHECK((LONG)to_i32_default(&pv, 99) == 5);
    pv.vt = VT_LPWSTR; pv.pwszVal = (WCHAR *)L"abc";
    CHECK((LONG)to_i32_default(&pv, 99) == 99);
    CHECK(to_dbl_default(&pv, 2.5) == 2.5);
    pv.vt = VT_R8; pv.dblVal = 7.5;
    CHECK(to_dbl_default(&pv, 2.5) == 7.5);
    pv.vt = VT_LPWSTR; pv.pwszVal = (WCHAR *)L"abc";
    CHECK(to_bool_default(&pv, TRUE) == TRUE && to_bool_default(&pv, FALSE) == FALSE);
    pv.vt = VT_BOOL; pv.boolVal = VARIANT_FALSE;
    CHECK(to_bool_default(&pv, TRUE) == FALSE);

    /* clearing an array */
    memset(arr, 0, sizeof(arr));
    arr[0].vt = VT_LPWSTR; arr[0].pwszVal = CoTaskMemAlloc(8 * sizeof(WCHAR)); wcscpy(arr[0].pwszVal, L"one");
    arr[1].vt = VT_BSTR; arr[1].bstrVal = SysAllocString(L"two");
    clear_array(arr, 2);
    CHECK(arr[0].vt == VT_EMPTY && arr[1].vt == VT_EMPTY);
    clear_array(NULL, 2);

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
