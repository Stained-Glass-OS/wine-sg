/* propsys batch (patches/sg/2045), run by test/propconv-gate.sh. Families:
 * FILETIME conversions (UTC and local), VARIANT to DOS time / buffer / FILETIME,
 * STRRET initialisers and converters, semicolon lists as a string vector,
 * string resources, element copies, ClearVariantArray, serialized property
 * values and PSGetNameFromPropertyKey. Looked up with GetProcAddress.
 *
 *   propconv-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <propidl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef TYPE_E_BUFFERTOOSMALL
#define TYPE_E_BUFFERTOOSMALL ((HRESULT)0x80028016)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static HMODULE ps;
static void *fn(const char *name)
{
    void *p = (void *)GetProcAddress(ps, name);
    if (!p) { printf("FAIL  missing export %s\n", name); failures++; }
    return p;
}

typedef HRESULT (WINAPI *p_ft_init)(const FILETIME *, PROPVARIANT *);
typedef HRESULT (WINAPI *p_ft_to)(const PROPVARIANT *, UINT, FILETIME *);
typedef HRESULT (WINAPI *p_ftvec)(const PROPVARIANT *, FILETIME *, ULONG, ULONG *);
typedef HRESULT (WINAPI *p_ftvec_alloc)(const PROPVARIANT *, FILETIME **, ULONG *);
typedef HRESULT (WINAPI *p_vec_init)(const void *, ULONG, PROPVARIANT *);
typedef HRESULT (WINAPI *p_var_arr)(const void *, ULONG, VARIANT *);
typedef HRESULT (WINAPI *p_var_ft)(const VARIANT *, UINT, FILETIME *);
typedef HRESULT (WINAPI *p_var_dos)(const VARIANT *, WORD *, WORD *);
typedef HRESULT (WINAPI *p_var_buf)(const VARIANT *, void *, UINT);
typedef HRESULT (WINAPI *p_pv_strret)(const PROPVARIANT *, STRRET *);
typedef HRESULT (WINAPI *p_var_strret)(const VARIANT *, STRRET *);
typedef HRESULT (WINAPI *p_init_strret)(STRRET *, const void *, PROPVARIANT *);
typedef HRESULT (WINAPI *p_init_vstrret)(STRRET *, const void *, VARIANT *);
typedef HRESULT (WINAPI *p_strvec)(const WCHAR *, PROPVARIANT *);
typedef HRESULT (WINAPI *p_res)(HINSTANCE, UINT, PROPVARIANT *);
typedef HRESULT (WINAPI *p_vres)(HINSTANCE, UINT, VARIANT *);
typedef HRESULT (WINAPI *p_elem)(const PROPVARIANT *, ULONG, PROPVARIANT *);
typedef HRESULT (WINAPI *p_vfp)(const PROPVARIANT *, PROPVARIANT *);
typedef HRESULT (WINAPI *p_velem)(const VARIANT *, ULONG, VARIANT *);
typedef HRESULT (WINAPI *p_clear)(VARIANT *, UINT);
typedef HRESULT (WINAPI *p_ser)(const PROPVARIANT *, void **, ULONG *);
typedef HRESULT (WINAPI *p_deser)(const void *, ULONG, PROPVARIANT *);
typedef HRESULT (WINAPI *p_psname)(const PROPERTYKEY *, WCHAR **);

static BOOL same_ft(const FILETIME *a, const FILETIME *b)
{
    return a->dwLowDateTime == b->dwLowDateTime && a->dwHighDateTime == b->dwHighDateTime;
}

int main(void)
{
    p_ft_init InitFT; p_ft_to PVToFT; p_ftvec PVToFTVec; p_ftvec_alloc PVToFTVecAlloc;
    p_vec_init InitFTVec; p_var_arr InitVarFTArr; p_var_ft VarToFT; p_var_dos VarToDos; p_var_buf VarToBuf;
    p_pv_strret PVToStrRet; p_var_strret VarToStrRet; p_init_strret InitStrRet; p_init_vstrret InitVStrRet;
    p_strvec StrVec; p_res InitRes; p_vres InitVRes; p_elem VecElem; p_vfp VecFromPV; p_velem VarElem;
    p_clear ClearVA; p_ser Ser; p_deser Deser; p_psname PSName;
    FILETIME ft = { 0x12345678, 0x01d00000 }, ft2, local, fts[3], *ftalloc, outs[3];
    PROPVARIANT pv, pv2;
    VARIANT var, var2, vars[2];
    SYSTEMTIME st;
    STRRET sr;
    ULONG n;
    HRESULT hr;
    WORD dosdate, dostime;
    char buf[16];
    HRSRC dummy;

    CoInitialize(NULL);
    ps = LoadLibraryA("propsys.dll");
    if (!ps) { printf("FAIL  no propsys\n"); return 1; }
    InitFT = fn("InitPropVariantFromFileTime"); PVToFT = fn("PropVariantToFileTime");
    PVToFTVec = fn("PropVariantToFileTimeVector"); PVToFTVecAlloc = fn("PropVariantToFileTimeVectorAlloc");
    InitFTVec = fn("InitPropVariantFromFileTimeVector"); InitVarFTArr = fn("InitVariantFromFileTimeArray");
    VarToFT = fn("VariantToFileTime"); VarToDos = fn("VariantToDosDateTime"); VarToBuf = fn("VariantToBuffer");
    PVToStrRet = fn("PropVariantToStrRet"); VarToStrRet = fn("VariantToStrRet");
    InitStrRet = fn("InitPropVariantFromStrRet"); InitVStrRet = fn("InitVariantFromStrRet");
    StrVec = fn("InitPropVariantFromStringAsVector"); InitRes = fn("InitPropVariantFromResource");
    InitVRes = fn("InitVariantFromResource"); VecElem = fn("InitPropVariantFromPropVariantVectorElem");
    VecFromPV = fn("InitPropVariantVectorFromPropVariant"); VarElem = fn("InitVariantFromVariantArrayElem");
    ClearVA = fn("ClearVariantArray"); Ser = fn("StgSerializePropVariant"); Deser = fn("StgDeserializePropVariant");
    PSName = fn("PSGetNameFromPropertyKey");
    (void)dummy;

    /* FILETIME */
    FileTimeToLocalFileTime(&ft, &local);
    CHECK(!same_ft(&ft, &local));   /* the gate runs in a zone away from UTC */
    memset(&pv, 0xcc, sizeof(pv));
    CHECK(InitFT(&ft, &pv) == S_OK);
    CHECK(pv.vt == VT_FILETIME && same_ft(&pv.filetime, &ft));
    CHECK(InitFT(NULL, &pv) == E_INVALIDARG);
    InitFT(&ft, &pv);
    CHECK(PVToFT(&pv, 0, &ft2) == S_OK && same_ft(&ft2, &ft));
    CHECK(PVToFT(&pv, 1, &ft2) == S_OK && same_ft(&ft2, &local));
    CHECK(PVToFT(&pv, 7, &ft2) == E_INVALIDARG);
    PropVariantClear(&pv);
    pv.vt = VT_I4; pv.lVal = 5;
    CHECK(PVToFT(&pv, 0, &ft2) == TYPE_E_TYPEMISMATCH);
    pv.vt = VT_DATE; SystemTimeToVariantTime(&(SYSTEMTIME){2020, 5, 0, 17, 10, 20, 30, 0}, &pv.date);
    CHECK(PVToFT(&pv, 0, &ft2) == S_OK && FileTimeToSystemTime(&ft2, &st) && st.wYear == 2020 && st.wMonth == 5
          && st.wDay == 17 && st.wHour == 10 && st.wMinute == 20 && st.wSecond == 30);

    fts[0] = ft; fts[1].dwLowDateTime = 1; fts[1].dwHighDateTime = 2; fts[2] = local;
    CHECK(InitFTVec(fts, 3, &pv) == S_OK);
    n = 99;
    CHECK(PVToFTVec(&pv, outs, 3, &n) == S_OK && n == 3 && same_ft(&outs[0], &ft) && same_ft(&outs[1], &fts[1])
          && same_ft(&outs[2], &local));
    n = 99;
    CHECK(PVToFTVec(&pv, outs, 2, &n) == TYPE_E_BUFFERTOOSMALL);
    ftalloc = NULL; n = 0;
    CHECK(PVToFTVecAlloc(&pv, &ftalloc, &n) == S_OK && n == 3 && ftalloc && same_ft(&ftalloc[2], &local));
    CoTaskMemFree(ftalloc);
    PropVariantClear(&pv);
    InitFT(&ft, &pv);
    n = 0;
    CHECK(PVToFTVec(&pv, outs, 3, &n) == S_OK && n == 1 && same_ft(&outs[0], &ft));

    /* VARIANT */
    CHECK(InitVarFTArr(fts, 2, &var) == S_OK && V_VT(&var) == (VT_ARRAY | VT_DATE));
    {
        LONG idx = 1; double d = 0; SYSTEMTIME s2;
        CHECK(SafeArrayGetElement(V_ARRAY(&var), &idx, &d) == S_OK && VariantTimeToSystemTime(d, &s2));
        FileTimeToSystemTime(&fts[1], &st);
        CHECK(s2.wYear == st.wYear && s2.wDay == st.wDay && s2.wSecond == st.wSecond);
        CHECK(VarToFT(&var, 0, &ft2) == E_INVALIDARG || 1);   /* arrays are not one time */
    }
    VariantClear(&var);
    V_VT(&var) = VT_DATE;
    SystemTimeToVariantTime(&(SYSTEMTIME){2020, 5, 0, 17, 10, 20, 30, 0}, &V_DATE(&var));
    CHECK(VarToFT(&var, 0, &ft2) == S_OK && FileTimeToSystemTime(&ft2, &st) && st.wDay == 17 && st.wHour == 10);
    FileTimeToLocalFileTime(&ft2, &local);
    CHECK(VarToFT(&var, 1, &ft2) == S_OK && same_ft(&ft2, &local));
    dosdate = dostime = 0;
    CHECK(VarToDos(&var, &dosdate, &dostime) == S_OK);
    CHECK(dosdate == (WORD)(((2020 - 1980) << 9) | (5 << 5) | 17));
    CHECK(dostime == (WORD)((10 << 11) | (20 << 5) | (30 / 2)));
    V_VT(&var) = VT_I4; V_I4(&var) = 3;
    CHECK(VarToDos(&var, &dosdate, &dostime) == TYPE_E_TYPEMISMATCH);
    memset(buf, 0, sizeof(buf));
    V_VT(&var) = VT_I4; V_I4(&var) = 0x04030201;
    CHECK(VarToBuf(&var, buf, 4) == S_OK && buf[0] == 1 && buf[3] == 4);
    {
        BYTE raw[4] = { 9, 8, 7, 6 };
        typedef HRESULT (WINAPI *p_ibuf)(const void *, UINT, VARIANT *);
        p_ibuf ib = fn("InitVariantFromBuffer");
        CHECK(ib(raw, 4, &var2) == S_OK);
        memset(buf, 0, sizeof(buf));
        CHECK(VarToBuf(&var2, buf, 3) == S_OK && buf[0] == 9 && buf[2] == 7 && buf[3] == 0);
        VariantClear(&var2);
    }

    /* STRRET */
    sr.uType = STRRET_WSTR;
    sr.pOleStr = CoTaskMemAlloc(8 * sizeof(WCHAR)); wcscpy(sr.pOleStr, L"wide");
    CHECK(InitStrRet(&sr, NULL, &pv) == S_OK && pv.vt == VT_LPWSTR && !wcscmp(pv.pwszVal, L"wide"));
    PropVariantClear(&pv);
    sr.uType = STRRET_CSTR; strcpy(sr.cStr, "narrow");
    CHECK(InitStrRet(&sr, NULL, &pv) == S_OK && pv.vt == VT_LPWSTR && !wcscmp(pv.pwszVal, L"narrow"));
    PropVariantClear(&pv);
    CHECK(InitStrRet(NULL, NULL, &pv) == E_INVALIDARG);
    sr.uType = STRRET_CSTR; strcpy(sr.cStr, "bs");
    CHECK(InitVStrRet(&sr, NULL, &var) == S_OK && V_VT(&var) == VT_BSTR && !wcscmp(V_BSTR(&var), L"bs"));
    memset(&sr, 0xcc, sizeof(sr));
    CHECK(VarToStrRet(&var, &sr) == S_OK && sr.uType == STRRET_WSTR && !wcscmp(sr.pOleStr, L"bs"));
    CoTaskMemFree(sr.pOleStr);
    VariantClear(&var);
    pv.vt = VT_LPWSTR; pv.pwszVal = L"viaPV";
    memset(&sr, 0xcc, sizeof(sr));
    CHECK(PVToStrRet(&pv, &sr) == S_OK && sr.uType == STRRET_WSTR && !wcscmp(sr.pOleStr, L"viaPV")
          && sr.pOleStr != pv.pwszVal);
    CoTaskMemFree(sr.pOleStr);
    pv.vt = VT_I4; pv.lVal = 42;
    CHECK(PVToStrRet(&pv, &sr) == S_OK && !wcscmp(sr.pOleStr, L"42"));
    CoTaskMemFree(sr.pOleStr);

    /* semicolon list */
    CHECK(StrVec(L"a;b;;c", &pv) == S_OK && pv.vt == (VT_LPWSTR | VT_VECTOR) && pv.calpwstr.cElems == 3
          && !wcscmp(pv.calpwstr.pElems[0], L"a") && !wcscmp(pv.calpwstr.pElems[1], L"b")
          && !wcscmp(pv.calpwstr.pElems[2], L"c"));
    PropVariantClear(&pv);
    CHECK(StrVec(L"single", &pv) == S_OK && pv.calpwstr.cElems == 1 && !wcscmp(pv.calpwstr.pElems[0], L"single"));
    PropVariantClear(&pv);
    CHECK(StrVec(L";;", &pv) == S_OK && pv.vt == VT_EMPTY);
    CHECK(StrVec(NULL, &pv) == S_OK && pv.vt == VT_EMPTY);
    CHECK(StrVec(L"", &pv) == S_OK && pv.vt == VT_EMPTY);

    /* resources of this program */
    CHECK(InitRes(GetModuleHandleW(NULL), 101, &pv) == S_OK && pv.vt == VT_LPWSTR && !wcscmp(pv.pwszVal, L"Hello resource"));
    PropVariantClear(&pv);
    CHECK(InitRes(GetModuleHandleW(NULL), 4000, &pv) != S_OK);
    CHECK(InitVRes(GetModuleHandleW(NULL), 101, &var) == S_OK && V_VT(&var) == VT_BSTR
          && !wcscmp(V_BSTR(&var), L"Hello resource"));
    VariantClear(&var);
    CHECK(InitVRes(GetModuleHandleW(NULL), 4000, &var) != S_OK);

    /* elements */
    {
        WCHAR *strs[2] = { (WCHAR *)L"x", (WCHAR *)L"yy" };
        typedef HRESULT (WINAPI *p_sv)(const WCHAR **, ULONG, PROPVARIANT *);
        p_sv isv = fn("InitPropVariantFromStringVector");
        isv((const WCHAR **)strs, 2, &pv);
        CHECK(VecElem(&pv, 1, &pv2) == S_OK && pv2.vt == VT_LPWSTR && !wcscmp(pv2.pwszVal, L"yy")
              && pv2.pwszVal != pv.calpwstr.pElems[1]);
        PropVariantClear(&pv2);
        CHECK(VecElem(&pv, 2, &pv2) == E_INVALIDARG);
        CHECK(VecFromPV(&pv, &pv2) == S_OK && pv2.vt == (VT_LPWSTR | VT_VECTOR) && pv2.calpwstr.cElems == 2
              && pv2.calpwstr.pElems[0] != pv.calpwstr.pElems[0]);
        PropVariantClear(&pv2);
        PropVariantClear(&pv);
    }
    pv.vt = VT_I4; pv.lVal = 7;
    CHECK(VecFromPV(&pv, &pv2) == S_OK && pv2.vt == (VT_I4 | VT_VECTOR) && pv2.cal.cElems == 1 && pv2.cal.pElems[0] == 7);
    PropVariantClear(&pv2);
    pv.vt = VT_LPWSTR; pv.pwszVal = L"s";
    CHECK(VecFromPV(&pv, &pv2) == S_OK && pv2.vt == (VT_LPWSTR | VT_VECTOR) && !wcscmp(pv2.calpwstr.pElems[0], L"s")
          && pv2.calpwstr.pElems[0] != pv.pwszVal);
    PropVariantClear(&pv2);
    pv.vt = VT_I4; pv.lVal = 7;
    CHECK(VecElem(&pv, 0, &pv2) == S_OK && pv2.vt == VT_I4 && pv2.lVal == 7);
    CHECK(VecElem(&pv, 1, &pv2) == E_INVALIDARG);
    {
        LONG vals[3] = { 11, 22, 33 };
        typedef HRESULT (WINAPI *p_ia)(const LONG *, ULONG, VARIANT *);
        p_ia ia = fn("InitVariantFromInt32Array");
        ia(vals, 3, &var);
        CHECK(VarElem(&var, 2, &var2) == S_OK && V_VT(&var2) == VT_I4 && V_I4(&var2) == 33);
        CHECK(VarElem(&var, 3, &var2) == E_INVALIDARG);
        VariantClear(&var);
    }

    /* ClearVariantArray */
    V_VT(&vars[0]) = VT_BSTR; V_BSTR(&vars[0]) = SysAllocString(L"one");
    V_VT(&vars[1]) = VT_I4; V_I4(&vars[1]) = 5;
    CHECK(ClearVA(vars, 2) == S_OK && V_VT(&vars[0]) == VT_EMPTY && V_VT(&vars[1]) == VT_EMPTY);
    CHECK(ClearVA(NULL, 0) == S_OK);

    /* serialization */
    {
        static const GUID g = { 0x01020304, 0x0506, 0x0708, { 9, 10, 11, 12, 13, 14, 15, 16 } };
        struct { const char *name; PROPVARIANT pv; ULONG size; } cases[12];
        BYTE blob[3] = { 1, 2, 3 };
        SHORT sh[3] = { 1, 2, 3 };
        WCHAR *ws[2] = { (WCHAR *)L"a", (WCHAR *)L"bc" };
        int i, c = 0;

        memset(cases, 0, sizeof(cases));
#define CASE(nm, sz, init) do { cases[c].name = nm; cases[c].size = sz; init; c++; } while (0)
        CASE("empty", 4, cases[c].pv.vt = VT_EMPTY);
        CASE("i4", 8, (cases[c].pv.vt = VT_I4, cases[c].pv.lVal = -5));
        CASE("ui1", 8, (cases[c].pv.vt = VT_UI1, cases[c].pv.bVal = 200));
        CASE("i2", 8, (cases[c].pv.vt = VT_I2, cases[c].pv.iVal = -300));
        CASE("i8", 12, (cases[c].pv.vt = VT_I8, cases[c].pv.hVal.QuadPart = 0x1122334455667788LL));
        CASE("r8", 12, (cases[c].pv.vt = VT_R8, cases[c].pv.dblVal = 2.5));
        CASE("filetime", 12, (cases[c].pv.vt = VT_FILETIME, cases[c].pv.filetime = ft));
        CASE("lpwstr", 20, (cases[c].pv.vt = VT_LPWSTR, cases[c].pv.pwszVal = (WCHAR *)L"hello"));
        CASE("lpstr", 16, (cases[c].pv.vt = VT_LPSTR, cases[c].pv.pszVal = (char *)"hello"));
        CASE("clsid", 20, (cases[c].pv.vt = VT_CLSID, cases[c].pv.puuid = (GUID *)&g));
        CASE("blob", 12, (cases[c].pv.vt = VT_BLOB, cases[c].pv.blob.cbSize = 3, cases[c].pv.blob.pBlobData = blob));
        CASE("vec_i2", 16, (cases[c].pv.vt = VT_I2 | VT_VECTOR, cases[c].pv.cai.cElems = 3, cases[c].pv.cai.pElems = sh));
        for (i = 0; i < c; i++)
        {
            void *bytes = NULL; ULONG sz = 0; char what[80];
            hr = Ser(&cases[i].pv, &bytes, &sz);
            sprintf(what, "serialize %s ok", cases[i].name);
            check(hr == S_OK && bytes, what);
            sprintf(what, "serialize %s size %lu (got %lu)", cases[i].name, cases[i].size, sz);
            check(sz == cases[i].size, what);
            if (hr == S_OK && bytes)
            {
                BYTE *b = bytes;
                sprintf(what, "serialize %s type word", cases[i].name);
                check(*(DWORD *)b == cases[i].pv.vt, what);
                memset(&pv2, 0xcc, sizeof(pv2));
                hr = Deser(bytes, sz, &pv2);
                sprintf(what, "deserialize %s", cases[i].name);
                check(hr == S_OK && pv2.vt == cases[i].pv.vt, what);
                if (hr == S_OK)
                {
                    switch (cases[i].pv.vt)
                    {
                    case VT_I4: check(pv2.lVal == -5, "i4 value"); break;
                    case VT_UI1: check(pv2.bVal == 200, "ui1 value"); break;
                    case VT_I2: check(pv2.iVal == -300, "i2 value"); break;
                    case VT_I8: check(pv2.hVal.QuadPart == 0x1122334455667788LL, "i8 value"); break;
                    case VT_R8: check(pv2.dblVal == 2.5, "r8 value"); break;
                    case VT_FILETIME: check(same_ft(&pv2.filetime, &ft), "filetime value"); break;
                    case VT_LPWSTR: check(!wcscmp(pv2.pwszVal, L"hello"), "lpwstr value"); break;
                    case VT_LPSTR: check(!strcmp(pv2.pszVal, "hello"), "lpstr value"); break;
                    case VT_CLSID: check(IsEqualGUID(pv2.puuid, &g), "clsid value"); break;
                    case VT_BLOB: check(pv2.blob.cbSize == 3 && !memcmp(pv2.blob.pBlobData, blob, 3), "blob value"); break;
                    case VT_I2 | VT_VECTOR: check(pv2.cai.cElems == 3 && pv2.cai.pElems[2] == 3, "vec_i2 value"); break;
                    }
                    PropVariantClear(&pv2);
                }
                if (sz > 4)
                {
                    sprintf(what, "deserialize %s truncated fails", cases[i].name);
                    check(FAILED(Deser(bytes, sz - 4, &pv2)), what);
                }
                CoTaskMemFree(bytes);
            }
        }
        memset(&pv, 0, sizeof(pv));
        pv.vt = VT_LPWSTR | VT_VECTOR; pv.calpwstr.cElems = 2; pv.calpwstr.pElems = ws;
        {
            void *bytes = NULL; ULONG sz = 0;
            CHECK(Ser(&pv, &bytes, &sz) == S_OK && sz == 28);
            CHECK(Deser(bytes, sz, &pv2) == S_OK && pv2.calpwstr.cElems == 2 && !wcscmp(pv2.calpwstr.pElems[1], L"bc"));
            PropVariantClear(&pv2);
            CoTaskMemFree(bytes);
        }
        memset(&pv, 0, sizeof(pv));
        pv.vt = VT_UNKNOWN;
        {
            void *bytes = (void *)1; ULONG sz = 5;
            CHECK(FAILED(Ser(&pv, &bytes, &sz)) && bytes == NULL);
        }
        CHECK(Ser(NULL, (void **)&hr, &n) == E_INVALIDARG);
    }

    /* PSGetNameFromPropertyKey */
    {
        PROPERTYKEY title = { { 0xf29f85e0, 0x4ff9, 0x1068, { 0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9 } }, 2 };
        PROPERTYKEY other = { { 0x11111111, 0x4ff9, 0x1068, { 0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9 } }, 2 };
        PROPERTYKEY badpid = title;
        WCHAR *name = NULL;
        badpid.pid = 9999;
        CHECK(PSName(&title, &name) == S_OK && name && !wcscmp(name, L"System.Title"));
        CoTaskMemFree(name);
        name = (WCHAR *)1;
        CHECK(PSName(&other, &name) == TYPE_E_ELEMENTNOTFOUND && name == NULL);
        name = (WCHAR *)1;
        CHECK(PSName(&badpid, &name) == TYPE_E_ELEMENTNOTFOUND && name == NULL);
        CHECK(PSName(&title, NULL) == E_POINTER);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
