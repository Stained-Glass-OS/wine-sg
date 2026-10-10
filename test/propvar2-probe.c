/* propsys batch (patches/sg/2039), run by test/propvar2-gate.sh. The VARIANT
 * forms: VariantTo<number> (and WithDefault), VariantGetElementCount,
 * VariantGet<type>Elem, InitVariantFrom<type>Array, VariantTo<type>Array
 * (and Alloc), booleans and strings, VariantToStringAlloc. Looked up with
 * GetProcAddress in propsys.dll.
 *
 *   propvar2-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <oleauto.h>
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
static void *fn(const char *name) { return (void *)GetProcAddress(ps, name); }

int main(void)
{
    ULONG (WINAPI *count)(const VARIANT *);
    HRESULT (WINAPI *to_i32)(const VARIANT *, LONG *), (WINAPI *to_u32)(const VARIANT *, ULONG *), (WINAPI *to_u16)(const VARIANT *, USHORT *);
    HRESULT (WINAPI *to_dbl)(const VARIANT *, double *), (WINAPI *to_bool)(const VARIANT *, BOOL *), (WINAPI *to_str_alloc)(const VARIANT *, WCHAR **);
    LONG (WINAPI *i32_default)(const VARIANT *, LONG);
    double (WINAPI *dbl_default)(const VARIANT *, double);
    HRESULT (WINAPI *get_i32)(const VARIANT *, ULONG, LONG *), (WINAPI *get_str)(const VARIANT *, ULONG, WCHAR **), (WINAPI *get_bool)(const VARIANT *, ULONG, BOOL *);
    HRESULT (WINAPI *init_i32)(const LONG *, ULONG, VARIANT *), (WINAPI *init_bool)(const BOOL *, ULONG, VARIANT *), (WINAPI *init_dbl)(const double *, ULONG, VARIANT *);
    HRESULT (WINAPI *init_str)(const WCHAR **, ULONG, VARIANT *);
    HRESULT (WINAPI *i32_array)(const VARIANT *, LONG *, ULONG, ULONG *), (WINAPI *i32_alloc)(const VARIANT *, LONG **, ULONG *);
    HRESULT (WINAPI *dbl_array)(const VARIANT *, double *, ULONG, ULONG *), (WINAPI *bool_array)(const VARIANT *, BOOL *, ULONG, ULONG *);
    HRESULT (WINAPI *str_alloc)(const VARIANT *, WCHAR ***, ULONG *), (WINAPI *str_array)(const VARIANT *, WCHAR **, ULONG, ULONG *);
    VARIANT v, a;
    static const LONG longs[] = { 1, 2, 3 };
    static const BOOL bools[] = { 1, 0 };
    static const double dbls[] = { 0.5, -1.5 };
    static const WCHAR *strings[] = { L"a", L"bb" };
    LONG l, lbuf[4], *lalloc;
    ULONG u, n;
    USHORT us;
    double d, dbuf[3];
    BOOL b, bbuf[3];
    WCHAR *s, *sbuf[3], **salloc;
    void *data;
    LONG lower, upper;

    CoInitialize(NULL);
    ps = LoadLibraryA("propsys.dll");
    count = fn("VariantGetElementCount");
    to_i32 = fn("VariantToInt32"); to_u32 = fn("VariantToUInt32"); to_u16 = fn("VariantToUInt16");
    to_dbl = fn("VariantToDouble"); to_bool = fn("VariantToBoolean"); to_str_alloc = fn("VariantToStringAlloc");
    i32_default = fn("VariantToInt32WithDefault"); dbl_default = fn("VariantToDoubleWithDefault");
    get_i32 = fn("VariantGetInt32Elem"); get_str = fn("VariantGetStringElem"); get_bool = fn("VariantGetBooleanElem");
    init_i32 = fn("InitVariantFromInt32Array"); init_bool = fn("InitVariantFromBooleanArray"); init_dbl = fn("InitVariantFromDoubleArray");
    init_str = fn("InitVariantFromStringArray");
    i32_array = fn("VariantToInt32Array"); i32_alloc = fn("VariantToInt32ArrayAlloc");
    dbl_array = fn("VariantToDoubleArray"); bool_array = fn("VariantToBooleanArray");
    str_alloc = fn("VariantToStringArrayAlloc"); str_array = fn("VariantToStringArray");
    CHECK(count && to_i32 && to_u32 && to_u16 && to_dbl && to_bool && to_str_alloc && i32_default && dbl_default);
    CHECK(get_i32 && get_str && get_bool && init_i32 && init_bool && init_dbl && init_str && i32_array && i32_alloc);
    CHECK(dbl_array && bool_array && str_alloc && str_array);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    /* scalars */
    VariantInit(&v);
    CHECK(count(&v) == 0);
    V_VT(&v) = VT_I4; V_I4(&v) = 5;
    CHECK(count(&v) == 1);
    l = 0;
    CHECK(to_i32(&v, &l) == S_OK && l == 5);
    V_I4(&v) = -1;
    CHECK(to_u32(&v, &u) == OVERFLOW_HR);
    V_I4(&v) = 70000;
    CHECK(to_u16(&v, &us) == OVERFLOW_HR);
    CHECK(to_i32(&v, NULL) == E_INVALIDARG);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"17");
    CHECK(to_i32(&v, &l) == S_OK && l == 17);
    CHECK(i32_default(&v, 99) == 17);
    SysFreeString(V_BSTR(&v)); V_BSTR(&v) = SysAllocString(L"zz");
    CHECK(i32_default(&v, 99) == 99);
    CHECK(dbl_default(&v, 1.25) == 1.25);
    s = NULL;
    CHECK(to_str_alloc(&v, &s) == S_OK && s && !wcscmp(s, L"zz"));
    CoTaskMemFree(s);
    VariantClear(&v);
    V_VT(&v) = VT_R8; V_R8(&v) = 2.5;
    CHECK(to_dbl(&v, &d) == S_OK && d == 2.5);
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    CHECK(to_bool(&v, &b) == S_OK && b == TRUE);
    V_VT(&v) = VT_I4; V_I4(&v) = 5;
    s = NULL;
    CHECK(to_str_alloc(&v, &s) == S_OK && s && !wcscmp(s, L"5"));
    CoTaskMemFree(s);

    /* arrays */
    CHECK(init_i32(longs, 3, &a) == S_OK && V_VT(&a) == (VT_ARRAY | VT_I4) && V_ARRAY(&a) != NULL);
    CHECK(SafeArrayGetDim(V_ARRAY(&a)) == 1 && SUCCEEDED(SafeArrayGetLBound(V_ARRAY(&a), 1, &lower))
          && SUCCEEDED(SafeArrayGetUBound(V_ARRAY(&a), 1, &upper)) && upper - lower + 1 == 3);
    SafeArrayAccessData(V_ARRAY(&a), &data);
    CHECK(((LONG *)data)[0] == 1 && ((LONG *)data)[2] == 3);
    SafeArrayUnaccessData(V_ARRAY(&a));
    CHECK(count(&a) == 3);
    CHECK(get_i32(&a, 1, &l) == S_OK && l == 2);
    CHECK(get_i32(&a, 3, &l) == E_INVALIDARG);
    n = 0;
    memset(lbuf, 0, sizeof(lbuf));
    CHECK(i32_array(&a, lbuf, 4, &n) == S_OK && n == 3 && lbuf[1] == 2);
    CHECK(i32_array(&a, lbuf, 2, &n) == TYPE_E_BUFFERTOOSMALL);
    lalloc = NULL;
    CHECK(i32_alloc(&a, &lalloc, &n) == S_OK && n == 3 && lalloc && lalloc[2] == 3);
    CoTaskMemFree(lalloc);
    CHECK(dbl_array(&a, dbuf, 3, &n) == S_OK && n == 3 && dbuf[2] == 3.0);
    VariantClear(&a);
    CHECK(init_i32(NULL, 2, &a) == E_INVALIDARG);
    CHECK(init_i32(longs, 3, NULL) == E_INVALIDARG);

    CHECK(init_bool(bools, 2, &a) == S_OK && V_VT(&a) == (VT_ARRAY | VT_BOOL));
    CHECK(get_bool(&a, 0, &b) == S_OK && b == TRUE && get_bool(&a, 1, &b) == S_OK && b == FALSE);
    CHECK(bool_array(&a, bbuf, 3, &n) == S_OK && n == 2 && bbuf[0] == TRUE && bbuf[1] == FALSE);
    SafeArrayAccessData(V_ARRAY(&a), &data);
    CHECK(((VARIANT_BOOL *)data)[0] == VARIANT_TRUE);
    SafeArrayUnaccessData(V_ARRAY(&a));
    VariantClear(&a);

    CHECK(init_dbl(dbls, 2, &a) == S_OK && V_VT(&a) == (VT_ARRAY | VT_R8));
    CHECK(dbl_array(&a, dbuf, 3, &n) == S_OK && n == 2 && dbuf[0] == 0.5 && dbuf[1] == -1.5);
    VariantClear(&a);

    CHECK(init_str(strings, 2, &a) == S_OK && V_VT(&a) == (VT_ARRAY | VT_BSTR));
    CHECK(count(&a) == 2);
    s = NULL;
    CHECK(get_str(&a, 1, &s) == S_OK && s && !wcscmp(s, L"bb"));
    CoTaskMemFree(s);
    salloc = NULL;
    CHECK(str_alloc(&a, &salloc, &n) == S_OK && n == 2 && salloc && !wcscmp(salloc[0], L"a") && !wcscmp(salloc[1], L"bb"));
    if (salloc) { CoTaskMemFree(salloc[0]); CoTaskMemFree(salloc[1]); CoTaskMemFree(salloc); }
    memset(sbuf, 0, sizeof(sbuf));
    CHECK(str_array(&a, sbuf, 3, &n) == S_OK && n == 2 && !wcscmp(sbuf[1], L"bb"));
    CoTaskMemFree(sbuf[0]); CoTaskMemFree(sbuf[1]);
    CHECK(str_array(&a, sbuf, 1, &n) == TYPE_E_BUFFERTOOSMALL);
    VariantClear(&a);

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
