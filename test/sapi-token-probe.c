/* SAPI data keys, token categories, token enumerators and tokens (patch 2841),
 * run by test/sapi-token-gate.sh on Xvfb. A native client over a scratch
 * category in HKEY_CURRENT_USER\Software\SGProbeSapi: the ISpDataKey methods
 * of categories and tokens, ISpObjectTokenCategory (GetId, GetDataKey,
 * default token), the enumerator builder (Skip, Reset, Clone, Sort,
 * AddTokensFromDataKey/-TokenEnum), ISpObjectToken (category lookup,
 * attributes, storage files, Remove) and the automation views late-bound
 * through IDispatch (ISpeechObjectToken, ISpeechDataKey,
 * ISpeechObjectTokenCategory, ISpeechObjectTokens and its IEnumVARIANT).
 *
 *   sapi-token-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <shlobj.h>
#include <sapi.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);
DEFINE_GUID(PR_CLSID_SpObjectTokenEnum, 0x3918d75f, 0x0acb, 0x41f2, 0xb7, 0x33, 0x92, 0xaa, 0x15, 0xbc, 0xec, 0xf6);
DEFINE_GUID(PR_IID_ISpObjectTokenEnumBuilder, 0x06b64f9f, 0x7fda, 0x11d2, 0xb4, 0xf2, 0x00, 0xc0, 0x4f, 0x79, 0x73, 0x96);
DEFINE_GUID(PR_CALLER, 0x0a0b0c0d, 0x1111, 0x2222, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44);

#define SPERR_UNINITIALIZED          ((HRESULT)0x80045001)
#define SPERR_ALREADY_INITIALIZED    ((HRESULT)0x80045002)
#define SPERR_NO_MORE_ITEMS          ((HRESULT)0x80045039)
#define SPERR_NOT_FOUND              ((HRESULT)0x8004503a)
#define SPERR_INVALID_REGISTRY_KEY   ((HRESULT)0x80045040)

typedef struct EB EB;
typedef struct EBVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(EB *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(EB *);
    ULONG (STDMETHODCALLTYPE *Release)(EB *);
    HRESULT (STDMETHODCALLTYPE *Next)(EB *, ULONG, ISpObjectToken **, ULONG *);
    HRESULT (STDMETHODCALLTYPE *Skip)(EB *, ULONG);
    HRESULT (STDMETHODCALLTYPE *Reset)(EB *);
    HRESULT (STDMETHODCALLTYPE *Clone)(EB *, IEnumSpObjectTokens **);
    HRESULT (STDMETHODCALLTYPE *Item)(EB *, ULONG, ISpObjectToken **);
    HRESULT (STDMETHODCALLTYPE *GetCount)(EB *, ULONG *);
    HRESULT (STDMETHODCALLTYPE *SetAttribs)(EB *, LPCWSTR, LPCWSTR);
    HRESULT (STDMETHODCALLTYPE *AddTokens)(EB *, ULONG, ISpObjectToken **);
    HRESULT (STDMETHODCALLTYPE *AddTokensFromDataKey)(EB *, ISpDataKey *, LPCWSTR, LPCWSTR);
    HRESULT (STDMETHODCALLTYPE *AddTokensFromTokenEnum)(EB *, IEnumSpObjectTokens *);
    HRESULT (STDMETHODCALLTYPE *Sort)(EB *, LPCWSTR);
} EBVtbl;
struct EB { const EBVtbl *lpVtbl; };

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

#define CATID L"HKEY_CURRENT_USER\\Software\\SGProbeSapi\\Cat"
#define CATSUB L"Software\\SGProbeSapi\\Cat"
#define TOK(n) CATID L"\\Tokens\\" n

static void reg_set(const WCHAR *sub, const WCHAR *name, const WCHAR *val)
{
    HKEY k;
    RegCreateKeyExW(HKEY_CURRENT_USER, sub, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (wcslen(val) + 1) * sizeof(WCHAR));
    RegCloseKey(k);
}

static void setup(void)
{
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SGProbeSapi");
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Vendor", L"A");
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Lang", L"409;40c");
    reg_set(CATSUB L"\\Tokens\\T1", NULL, L"Token one");
    reg_set(CATSUB L"\\Tokens\\T1", L"CLSID", L"{715d9c59-4442-11d2-9605-00c04f8ee628}");
    reg_set(CATSUB L"\\Tokens\\T2\\Attributes", L"Vendor", L"B");
    reg_set(CATSUB L"\\Tokens\\T2\\Attributes", L"Lang", L"409");
    reg_set(CATSUB L"\\Tokens\\T3\\Attributes", L"Vendor", L"A");
    reg_set(CATSUB L"\\Tokens\\T3\\Attributes", L"Lang", L"407");
    reg_set(CATSUB, L"DefaultDefaultTokenId", TOK(L"T2"));
}

static BSTR to_bstr(const WCHAR *s) { return SysAllocString(s); }

/* ---- late binding helpers ---------------------------------------------- */
static HRESULT dget(IDispatch *d, const WCHAR *name, VARIANT *res, VARIANT *args, int nargs, WORD flags)
{
    DISPID id, named = DISPID_PROPERTYPUT;
    DISPPARAMS dp;
    VARIANT rev[6];
    EXCEPINFO ex;
    UINT argerr = 0;
    HRESULT hr;
    LPOLESTR n = (LPOLESTR)name;
    int i;

    hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    for (i = 0; i < nargs; i++) rev[i] = args[nargs - 1 - i];
    dp.rgvarg = rev;
    dp.cArgs = nargs;
    dp.rgdispidNamedArgs = (flags & DISPATCH_PROPERTYPUT) ? &named : NULL;
    dp.cNamedArgs = dp.rgdispidNamedArgs ? 1 : 0;
    if (res) VariantInit(res);
    memset(&ex, 0, sizeof(ex));
    hr = IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, res, &ex, &argerr);
    if (hr == DISP_E_EXCEPTION) hr = ex.scode;
    return hr;
}
static VARIANT vstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vi4(LONG l) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = l; return v; }
static HRESULT dcall(IDispatch *d, const WCHAR *name, VARIANT *res, VARIANT *args, int nargs)
{ return dget(d, name, res, args, nargs, DISPATCH_METHOD | DISPATCH_PROPERTYGET); }
static HRESULT dcall1(IDispatch *d, const WCHAR *name, VARIANT *res, VARIANT a)
{ return dcall(d, name, res, &a, 1); }
/* a returned object as IDispatch */
static IDispatch *disp_of(HRESULT hr, VARIANT *r)
{
    IDispatch *d = NULL;
    if (hr == S_OK && V_VT(r) == VT_DISPATCH) d = V_DISPATCH(r);
    return d;
}
static int is_str(VARIANT *r, const WCHAR *want) { return V_VT(r) == VT_BSTR && V_BSTR(r) && !wcscmp(V_BSTR(r), want); }

static ISpObjectTokenCategory *new_category(const WCHAR *id, BOOL create)
{
    ISpObjectTokenCategory *c = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_SpObjectTokenCategory, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectTokenCategory, (void **)&c);
    if (hr == S_OK && id) hr = ISpObjectTokenCategory_SetId(c, id, create);
    return c;
}
static ISpObjectToken *new_token(const WCHAR *cat, const WCHAR *id)
{
    ISpObjectToken *t = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectToken, (void **)&t);
    if (hr == S_OK && id) hr = ISpObjectToken_SetId(t, cat, id, FALSE);
    CHECKF(hr == S_OK, "token %ls", id ? id : L"(uninitialized)");
    return t;
}

/* ---- ISpDataKey through a category -------------------------------------- */
static void test_data_key(void)
{
    ISpObjectTokenCategory *cat, *cat2;
    ISpDataKey *key, *sub;
    BYTE buf[16];
    ULONG size;
    DWORD dw;
    WCHAR *str;
    HRESULT hr;
    int i;

    CoCreateInstance(&CLSID_SpObjectTokenCategory, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectTokenCategory, (void **)&cat);
    str = (WCHAR *)1; size = 1;
    hr = ISpObjectTokenCategory_GetId(cat, &str);
    CHECKF(hr == SPERR_UNINITIALIZED && str == (WCHAR *)1, "category uninit: GetId (%#lx)", hr);
    hr = ISpObjectTokenCategory_SetData(cat, L"x", 1, buf);
    CHECKF(hr == SPERR_UNINITIALIZED, "category uninit: SetData (%#lx)", hr);
    hr = ISpObjectTokenCategory_GetData(cat, L"x", &size, buf);
    CHECKF(hr == SPERR_UNINITIALIZED, "category uninit: GetData (%#lx)", hr);
    hr = ISpObjectTokenCategory_SetDWORD(cat, L"x", 1);
    CHECKF(hr == SPERR_UNINITIALIZED, "category uninit: SetDWORD (%#lx)", hr);
    hr = ISpObjectTokenCategory_EnumKeys(cat, 0, &str);
    CHECKF(hr == SPERR_UNINITIALIZED, "category uninit: EnumKeys (%#lx)", hr);
    hr = ISpObjectTokenCategory_DeleteKey(cat, L"x");
    CHECKF(hr == SPERR_UNINITIALIZED, "category uninit: DeleteKey (%#lx)", hr);
    hr = ISpObjectTokenCategory_GetDataKey(cat, SPDKL_DefaultLocation, &key);
    CHECKF(hr == SPERR_UNINITIALIZED, "category uninit: GetDataKey (%#lx)", hr);
    hr = ISpObjectTokenCategory_SetDefaultTokenId(cat, L"x");
    CHECKF(hr == SPERR_UNINITIALIZED, "category uninit: SetDefaultTokenId (%#lx)", hr);
    ISpObjectTokenCategory_Release(cat);

    cat = new_category(CATID, TRUE);
    str = NULL;
    hr = ISpObjectTokenCategory_GetId(cat, &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, CATID), "category: GetId returns the id it was given (%#lx)", hr);
    CoTaskMemFree(str);
    hr = ISpObjectTokenCategory_GetId(cat, NULL);
    CHECKF(hr == E_POINTER, "category: GetId(NULL) is E_POINTER (%#lx)", hr);

    /* binary data */
    for (i = 0; i < 6; i++) buf[i] = 0x10 + i;
    hr = ISpObjectTokenCategory_SetData(cat, L"bin", 6, buf);
    CHECKF(hr == S_OK, "SetData 6 bytes (%#lx)", hr);
    size = 0;
    hr = ISpObjectTokenCategory_GetData(cat, L"bin", &size, NULL);
    CHECKF(hr == S_OK && size == 6, "GetData without a buffer reports the size (%#lx, %lu)", hr, size);
    size = 4;
    hr = ISpObjectTokenCategory_GetData(cat, L"bin", &size, buf);
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA) && size == 6, "GetData with a small buffer is ERROR_MORE_DATA and the size (%#lx, %lu)", hr, size);
    memset(buf, 0, sizeof(buf)); size = 16;
    hr = ISpObjectTokenCategory_GetData(cat, L"bin", &size, buf);
    CHECKF(hr == S_OK && size == 6 && buf[0] == 0x10 && buf[5] == 0x15, "GetData returns the bytes (%#lx, %lu)", hr, size);
    hr = ISpObjectTokenCategory_GetData(cat, L"nobin", &size, buf);
    CHECKF(hr == SPERR_NOT_FOUND, "GetData of a missing value is SPERR_NOT_FOUND (%#lx)", hr);
    hr = ISpObjectTokenCategory_GetData(cat, L"bin", NULL, buf);
    CHECKF(hr == E_POINTER, "GetData with no size is E_POINTER (%#lx)", hr);

    /* DWORD */
    hr = ISpObjectTokenCategory_SetDWORD(cat, L"num", 0xdeadbeef);
    CHECKF(hr == S_OK, "SetDWORD (%#lx)", hr);
    dw = 0;
    hr = ISpObjectTokenCategory_GetDWORD(cat, L"num", &dw);
    CHECKF(hr == S_OK && dw == 0xdeadbeef, "GetDWORD (%#lx, %#lx)", hr, dw);
    hr = ISpObjectTokenCategory_GetDWORD(cat, L"nonum", &dw);
    CHECKF(hr == SPERR_NOT_FOUND, "GetDWORD of a missing value (%#lx)", hr);
    ISpObjectTokenCategory_SetStringValue(cat, L"str", L"text");
    hr = ISpObjectTokenCategory_GetDWORD(cat, L"str", &dw);
    CHECKF(hr == SPERR_NOT_FOUND, "GetDWORD of a string value (%#lx)", hr);
    hr = ISpObjectTokenCategory_GetDWORD(cat, L"num", NULL);
    CHECKF(hr == E_POINTER, "GetDWORD(NULL) is E_POINTER (%#lx)", hr);
    str = NULL;
    hr = ISpObjectTokenCategory_GetStringValue(cat, L"str", &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, L"text"), "SetStringValue/GetStringValue through the category");
    CoTaskMemFree(str);

    /* values and keys */
    {
        static const WCHAR *want[] = { L"bin", L"num", L"str", L"DefaultDefaultTokenId" };
        int found = 0;
        for (i = 0; i < 4; i++)
        {
            str = NULL;
            hr = ISpObjectTokenCategory_EnumValues(cat, i, &str);
            if (hr == S_OK) { for (size = 0; size < 4; size++) if (!wcsicmp(str, want[size])) found |= 1 << size; }
            CoTaskMemFree(str);
        }
        CHECKF(found == 15, "EnumValues 0..3 return the four values (mask %d)", found);
        str = (WCHAR *)1;
        hr = ISpObjectTokenCategory_EnumValues(cat, 4, &str);
        CHECKF(hr == SPERR_NO_MORE_ITEMS, "EnumValues past the end is SPERR_NO_MORE_ITEMS (%#lx)", hr);
        hr = ISpObjectTokenCategory_EnumValues(cat, 0, NULL);
        CHECKF(hr == E_POINTER, "EnumValues(NULL) is E_POINTER (%#lx)", hr);
    }
    hr = ISpObjectTokenCategory_DeleteValue(cat, L"num");
    CHECKF(hr == S_OK, "DeleteValue (%#lx)", hr);
    hr = ISpObjectTokenCategory_DeleteValue(cat, L"num");
    CHECKF(hr == SPERR_NOT_FOUND, "DeleteValue again is SPERR_NOT_FOUND (%#lx)", hr);
    hr = ISpObjectTokenCategory_GetDWORD(cat, L"num", &dw);
    CHECKF(hr == SPERR_NOT_FOUND, "deleted value is gone");

    hr = ISpObjectTokenCategory_CreateKey(cat, L"Sub\\Deep", &sub);
    CHECKF(hr == S_OK, "CreateKey nested (%#lx)", hr);
    if (hr == S_OK) ISpDataKey_Release(sub);
    {
        int n = 0, tok = 0;
        for (i = 0; i < 4; i++)
        {
            str = NULL;
            hr = ISpObjectTokenCategory_EnumKeys(cat, i, &str);
            if (hr == S_OK) { n++; if (!wcsicmp(str, L"Tokens") || !wcsicmp(str, L"Sub")) tok++; }
            else CHECKF(i == 2 && hr == SPERR_NO_MORE_ITEMS, "EnumKeys %d ends with SPERR_NO_MORE_ITEMS (%#lx)", i, hr);
            CoTaskMemFree(str);
            if (hr != S_OK) break;
        }
        CHECKF(n == 2 && tok == 2, "EnumKeys finds Tokens and Sub (%d keys)", n);
    }
    hr = ISpObjectTokenCategory_DeleteKey(cat, L"Sub");
    CHECKF(hr == S_OK, "DeleteKey removes a key with children (%#lx)", hr);
    hr = ISpObjectTokenCategory_OpenKey(cat, L"Sub", &sub);
    CHECKF(hr == SPERR_NOT_FOUND, "deleted key is gone (%#lx)", hr);
    hr = ISpObjectTokenCategory_DeleteKey(cat, L"Sub");
    CHECKF(hr == SPERR_NOT_FOUND, "DeleteKey again is SPERR_NOT_FOUND (%#lx)", hr);

    /* GetDataKey locations */
    hr = ISpObjectTokenCategory_GetDataKey(cat, SPDKL_DefaultLocation, &key);
    CHECKF(hr == S_OK, "GetDataKey(default) (%#lx)", hr);
    if (hr == S_OK)
    {
        str = NULL;
        hr = ISpDataKey_GetStringValue(key, L"str", &str);
        CHECKF(hr == S_OK && str && !wcscmp(str, L"text"), "GetDataKey(default) is the category's own key");
        CoTaskMemFree(str);
        ISpDataKey_Release(key);
    }
    hr = ISpObjectTokenCategory_GetDataKey(cat, SPDKL_CurrentUser, &key);
    CHECKF(hr == S_OK, "GetDataKey(current user) (%#lx)", hr);
    if (hr == S_OK)
    {
        str = NULL;
        hr = ISpDataKey_GetStringValue(key, L"str", &str);
        CHECKF(hr == S_OK && str, "GetDataKey(current user): the same path under HKCU");
        CoTaskMemFree(str);
        ISpDataKey_Release(key);
    }
    hr = ISpObjectTokenCategory_GetDataKey(cat, SPDKL_LocalMachine, &key);
    CHECKF(hr == S_OK, "GetDataKey(local machine) (%#lx)", hr);
    if (hr == S_OK)
    {
        ISpDataKey_SetStringValue(key, L"lm", L"machine");
        str = NULL;
        hr = ISpObjectTokenCategory_GetStringValue(cat, L"lm", &str);
        CHECKF(hr == SPERR_NOT_FOUND, "GetDataKey(local machine) is a different key from the HKCU category (%#lx)", hr);
        CoTaskMemFree(str);
        ISpDataKey_Release(key);
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\SGProbeSapi");
    }
    hr = ISpObjectTokenCategory_GetDataKey(cat, (SPDATAKEYLOCATION)7, &key);
    CHECKF(hr == E_INVALIDARG, "GetDataKey with a bad location is E_INVALIDARG (%#lx)", hr);
    hr = ISpObjectTokenCategory_GetDataKey(cat, SPDKL_DefaultLocation, NULL);
    CHECKF(hr == E_POINTER, "GetDataKey(NULL) is E_POINTER (%#lx)", hr);

    /* default token: the machine default until the user picks one */
    str = NULL;
    hr = ISpObjectTokenCategory_GetDefaultTokenId(cat, &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, TOK(L"T2")), "GetDefaultTokenId: machine default (%#lx)", hr);
    CoTaskMemFree(str);
    hr = ISpObjectTokenCategory_SetDefaultTokenId(cat, NULL);
    CHECKF(hr == E_POINTER, "SetDefaultTokenId(NULL) is E_POINTER (%#lx)", hr);
    hr = ISpObjectTokenCategory_SetDefaultTokenId(cat, TOK(L"T3"));
    CHECKF(hr == S_OK, "SetDefaultTokenId (%#lx)", hr);
    str = NULL;
    hr = ISpObjectTokenCategory_GetDefaultTokenId(cat, &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, TOK(L"T3")), "GetDefaultTokenId: the user's choice wins (%#lx)", hr);
    CoTaskMemFree(str);
    cat2 = new_category(CATID, FALSE);
    str = NULL;
    hr = ISpObjectTokenCategory_GetDefaultTokenId(cat2, &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, TOK(L"T3")), "GetDefaultTokenId: kept for other category objects");
    CoTaskMemFree(str);
    ISpObjectTokenCategory_Release(cat2);
    {
        HKEY k;
        RegOpenKeyExW(HKEY_CURRENT_USER, CATSUB, 0, KEY_ALL_ACCESS, &k);
        RegDeleteValueW(k, L"DefaultTokenId");
        RegCloseKey(k);
    }
    str = NULL;
    hr = ISpObjectTokenCategory_GetDefaultTokenId(cat, &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, TOK(L"T2")), "GetDefaultTokenId: back to the machine default");
    CoTaskMemFree(str);

    ISpObjectTokenCategory_Release(cat);
    RegDeleteValueW(HKEY_CURRENT_USER, L"x");
}

/* ---- the enumerator ---------------------------------------------------- */
static WCHAR *token_id(ISpObjectToken *t)
{
    WCHAR *id = NULL;
    ISpObjectToken_GetId(t, &id);
    return id;
}
static int token_is(ISpObjectToken *t, const WCHAR *want)
{
    WCHAR *id = token_id(t);
    int ok = id && !wcsicmp(id, want);
    CoTaskMemFree(id);
    return ok;
}

static void test_enum(void)
{
    ISpObjectTokenCategory *cat;
    IEnumSpObjectTokens *en, *clone;
    ISpObjectToken *t[4];
    ISpDataKey *key;
    EB *eb, *eb2;
    ULONG n, count;
    HRESULT hr;

    cat = new_category(CATID, FALSE);
    hr = ISpObjectTokenCategory_EnumTokens(cat, NULL, NULL, &en);
    CHECKF(hr == S_OK, "EnumTokens (%#lx)", hr);
    if (hr != S_OK) return;
    IEnumSpObjectTokens_GetCount(en, &count);
    CHECKF(count == 3, "enumerator holds the three tokens (%lu)", count);

    hr = IEnumSpObjectTokens_Next(en, 1, t, &n);
    CHECKF(hr == S_OK && n == 1, "Next(1)");
    if (hr == S_OK) ISpObjectToken_Release(t[0]);
    hr = IEnumSpObjectTokens_Skip(en, 1);
    CHECKF(hr == S_OK, "Skip(1) leaves one (%#lx)", hr);
    hr = IEnumSpObjectTokens_Clone(en, &clone);
    CHECKF(hr == S_OK && clone && clone != en, "Clone (%#lx)", hr);
    if (hr == S_OK)
    {
        ULONG ccount = 0;
        IEnumSpObjectTokens_GetCount(clone, &ccount);
        CHECKF(ccount == 3, "Clone has the same tokens");
        hr = IEnumSpObjectTokens_Next(clone, 1, &t[1], &n);
        CHECKF(hr == S_OK && n == 1, "Clone continues at the saved position (%#lx)", hr);
        hr = IEnumSpObjectTokens_Next(en, 1, &t[0], &n);
        CHECKF(hr == S_OK && n == 1 && token_is(t[0], TOK(L"T3")) && token_is(t[1], TOK(L"T3")),
               "the original is unaffected by the clone: both give the third token");
        ISpObjectToken_Release(t[0]);
        ISpObjectToken_Release(t[1]);
        hr = IEnumSpObjectTokens_Next(clone, 1, &t[1], &n);
        CHECKF(hr == S_FALSE && n == 0, "Clone at the end: S_FALSE");
        IEnumSpObjectTokens_Release(clone);
    }
    hr = IEnumSpObjectTokens_Reset(en);
    CHECKF(hr == S_OK, "Reset (%#lx)", hr);
    hr = IEnumSpObjectTokens_Next(en, 3, t, &n);
    CHECKF(hr == S_OK && n == 3, "after Reset: Next(3) returns all three");
    for (n = 0; n < 3 && hr == S_OK; n++) ISpObjectToken_Release(t[n]);
    hr = IEnumSpObjectTokens_Skip(en, 1);
    CHECKF(hr == S_FALSE, "Skip at the end is S_FALSE (%#lx)", hr);
    IEnumSpObjectTokens_Reset(en);
    hr = IEnumSpObjectTokens_Skip(en, 5);
    CHECKF(hr == S_FALSE, "Skip past the end is S_FALSE (%#lx)", hr);
    hr = IEnumSpObjectTokens_Next(en, 1, t, &n);
    CHECKF(hr == S_FALSE && n == 0, "Skip past the end leaves the enumerator at the end");
    IEnumSpObjectTokens_Reset(en);
    hr = IEnumSpObjectTokens_Skip(en, 3);
    CHECKF(hr == S_OK, "Skip(3) of three is S_OK (%#lx)", hr);
    IEnumSpObjectTokens_Release(en);

    /* The builder: uninitialized, then the AddTokens variants and Sort. */
    hr = CoCreateInstance(&PR_CLSID_SpObjectTokenEnum, NULL, CLSCTX_INPROC_SERVER, &PR_IID_ISpObjectTokenEnumBuilder, (void **)&eb);
    CHECKF(hr == S_OK, "create the enumerator builder (%#lx)", hr);
    if (hr != S_OK) { ISpObjectTokenCategory_Release(cat); return; }
    hr = eb->lpVtbl->Skip(eb, 1);
    CHECKF(hr == SPERR_UNINITIALIZED, "builder uninit: Skip (%#lx)", hr);
    hr = eb->lpVtbl->Reset(eb);
    CHECKF(hr == SPERR_UNINITIALIZED, "builder uninit: Reset (%#lx)", hr);
    hr = eb->lpVtbl->Clone(eb, &clone);
    CHECKF(hr == SPERR_UNINITIALIZED, "builder uninit: Clone (%#lx)", hr);
    hr = eb->lpVtbl->AddTokensFromDataKey(eb, NULL, NULL, NULL);
    CHECKF(hr == SPERR_UNINITIALIZED, "builder uninit: AddTokensFromDataKey (%#lx)", hr);
    hr = eb->lpVtbl->AddTokensFromTokenEnum(eb, NULL);
    CHECKF(hr == SPERR_UNINITIALIZED, "builder uninit: AddTokensFromTokenEnum (%#lx)", hr);
    hr = eb->lpVtbl->SetAttribs(eb, NULL, NULL);
    CHECKF(hr == S_OK, "builder SetAttribs (%#lx)", hr);
    hr = eb->lpVtbl->AddTokensFromDataKey(eb, NULL, L"Tokens", CATID);
    CHECKF(hr == E_POINTER, "AddTokensFromDataKey(NULL key) is E_POINTER (%#lx)", hr);
    hr = eb->lpVtbl->AddTokensFromTokenEnum(eb, NULL);
    CHECKF(hr == E_POINTER, "AddTokensFromTokenEnum(NULL) is E_POINTER (%#lx)", hr);

    hr = ISpObjectTokenCategory_GetDataKey(cat, SPDKL_DefaultLocation, &key);
    hr = eb->lpVtbl->AddTokensFromDataKey(eb, key, L"Tokens", CATID);
    CHECKF(hr == S_OK, "AddTokensFromDataKey (%#lx)", hr);
    hr = eb->lpVtbl->GetCount(eb, &count);
    CHECKF(hr == S_OK && count == 3, "AddTokensFromDataKey adds the three sub keys (%lu)", count);
    hr = eb->lpVtbl->Item(eb, 0, &t[0]);
    if (hr == S_OK)
    {
        WCHAR *id = token_id(t[0]);
        CHECKF(id && wcsstr(id, L"\\Tokens\\T") && wcslen(id) == wcslen(TOK(L"T1")), "AddTokensFromDataKey: token ids are category\\Tokens\\name (%ls)", id);
        CoTaskMemFree(id);
        ISpObjectToken_Release(t[0]);
    }
    ISpDataKey_Release(key);

    hr = CoCreateInstance(&PR_CLSID_SpObjectTokenEnum, NULL, CLSCTX_INPROC_SERVER, &PR_IID_ISpObjectTokenEnumBuilder, (void **)&eb2);
    eb2->lpVtbl->SetAttribs(eb2, L"Vendor=A", NULL);
    hr = eb2->lpVtbl->AddTokensFromTokenEnum(eb2, (IEnumSpObjectTokens *)eb);
    CHECKF(hr == S_OK, "AddTokensFromTokenEnum (%#lx)", hr);
    eb2->lpVtbl->GetCount(eb2, &count);
    CHECKF(count == 2, "AddTokensFromTokenEnum applies the new enumerator's required attributes (%lu)", count);

    /* Sort: put T3 first (T1, T2, T3 in registry order) */
    hr = eb->lpVtbl->Sort(eb, TOK(L"T3"));
    CHECKF(hr == S_OK, "Sort with a first token (%#lx)", hr);
    hr = eb->lpVtbl->Item(eb, 0, &t[0]);
    CHECKF(hr == S_OK && token_is(t[0], TOK(L"T3")), "Sort: the named token is first");
    if (hr == S_OK) ISpObjectToken_Release(t[0]);
    hr = eb->lpVtbl->Item(eb, 1, &t[0]);
    CHECKF(hr == S_OK && token_is(t[0], TOK(L"T1")), "Sort: the others keep their order");
    if (hr == S_OK) ISpObjectToken_Release(t[0]);
    hr = eb->lpVtbl->Sort(eb, TOK(L"NoSuch"));
    CHECKF(hr == S_OK, "Sort with an unknown token id is S_OK (%#lx)", hr);
    hr = eb->lpVtbl->Item(eb, 3, &t[0]);
    CHECKF(hr == SPERR_NO_MORE_ITEMS, "Item past the end is SPERR_NO_MORE_ITEMS (%#lx)", hr);

    eb2->lpVtbl->Release(eb2);
    eb->lpVtbl->Release(eb);
    ISpObjectTokenCategory_Release(cat);
}

/* ---- ISpObjectToken ---------------------------------------------------- */
static void test_token(void)
{
    ISpObjectToken *tok, *tok2;
    ISpObjectTokenCategory *cat;
    ISpDataKey *sub;
    WCHAR *str, *path;
    BYTE buf[8];
    ULONG size;
    DWORD dw;
    BOOL b;
    WCHAR expect[MAX_PATH];
    HRESULT hr;
    int i;

    tok = new_token(NULL, NULL);
    cat = (ISpObjectTokenCategory *)0xdeadbeef;
    hr = ISpObjectToken_GetCategory(tok, NULL);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: GetCategory(NULL) (%#lx)", hr);
    hr = ISpObjectToken_GetCategory(tok, &cat);
    CHECKF(hr == SPERR_UNINITIALIZED && cat == (ISpObjectTokenCategory *)0xdeadbeef, "token uninit: GetCategory (%#lx)", hr);
    size = 1;
    hr = ISpObjectToken_SetData(tok, L"x", 1, buf);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: SetData (%#lx)", hr);
    hr = ISpObjectToken_GetData(tok, L"x", &size, buf);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: GetData (%#lx)", hr);
    hr = ISpObjectToken_SetStringValue(tok, L"x", L"y");
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: SetStringValue (%#lx)", hr);
    hr = ISpObjectToken_GetStringValue(tok, L"x", &str);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: GetStringValue (%#lx)", hr);
    hr = ISpObjectToken_OpenKey(tok, L"x", &sub);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: OpenKey (%#lx)", hr);
    hr = ISpObjectToken_CreateKey(tok, L"x", &sub);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: CreateKey (%#lx)", hr);
    hr = ISpObjectToken_EnumKeys(tok, 0, &str);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: EnumKeys (%#lx)", hr);
    hr = ISpObjectToken_Remove(tok, NULL);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: Remove (%#lx)", hr);
    hr = ISpObjectToken_GetStorageFileName(tok, &PR_CALLER, L"k", L"f", 0, &str);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: GetStorageFileName (%#lx)", hr);
    hr = ISpObjectToken_MatchesAttributes(tok, L"a", &b);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: MatchesAttributes (%#lx)", hr);
    hr = ISpObjectToken_IsUISupported(tok, L"x", NULL, 0, NULL, &b);
    CHECKF(hr == SPERR_UNINITIALIZED, "token uninit: IsUISupported (%#lx)", hr);
    ISpObjectToken_Release(tok);

    /* category: derived from the id, or the one given */
    tok = new_token(NULL, TOK(L"T1"));
    hr = ISpObjectToken_GetCategory(tok, NULL);
    CHECKF(hr == E_POINTER, "GetCategory(NULL) is E_POINTER (%#lx)", hr);
    cat = NULL;
    hr = ISpObjectToken_GetCategory(tok, &cat);
    CHECKF(hr == S_OK && cat, "GetCategory from the id (%#lx)", hr);
    if (hr == S_OK)
    {
        str = NULL;
        ISpObjectTokenCategory_GetId(cat, &str);
        CHECKF(str && !wcscmp(str, CATID), "GetCategory: the category id is the part before \\Tokens (%ls)", str ? str : L"?");
        CoTaskMemFree(str);
        ISpObjectTokenCategory_Release(cat);
    }
    tok2 = new_token(CATID, TOK(L"T2"));
    cat = NULL;
    hr = ISpObjectToken_GetCategory(tok2, &cat);
    CHECKF(hr == S_OK && cat, "GetCategory with an explicit category (%#lx)", hr);
    if (cat) ISpObjectTokenCategory_Release(cat);
    ISpObjectToken_Release(tok2);
    tok2 = new_token(NULL, CATID);
    cat = (ISpObjectTokenCategory *)0xdeadbeef;
    hr = ISpObjectToken_GetCategory(tok2, &cat);
    CHECKF(hr == SPERR_INVALID_REGISTRY_KEY && cat == (ISpObjectTokenCategory *)0xdeadbeef, "GetCategory of an id that is not below a category (%#lx)", hr);
    ISpObjectToken_Release(tok2);

    /* data */
    hr = ISpObjectToken_SetData(tok, L"blob", 3, (const BYTE *)"\1\2\3");
    CHECKF(hr == S_OK, "token SetData (%#lx)", hr);
    size = 8;
    hr = ISpObjectToken_GetData(tok, L"blob", &size, buf);
    CHECKF(hr == S_OK && size == 3 && buf[2] == 3, "token GetData (%#lx)", hr);
    hr = ISpObjectToken_SetDWORD(tok, L"n", 77);
    CHECKF(hr == S_OK, "token SetDWORD (%#lx)", hr);
    hr = ISpObjectToken_GetDWORD(tok, L"n", &dw);
    CHECKF(hr == S_OK && dw == 77, "token GetDWORD (%#lx)", hr);
    {
        int found = 0;
        for (i = 0; i < 6; i++)
        {
            str = NULL;
            hr = ISpObjectToken_EnumValues(tok, i, &str);
            if (hr == S_OK) { if (!wcsicmp(str, L"blob")) found |= 1; if (!wcsicmp(str, L"n")) found |= 2; if (!wcsicmp(str, L"CLSID")) found |= 4; }
            CoTaskMemFree(str);
            if (hr != S_OK) break;
        }
        CHECKF(found == 7, "token EnumValues finds blob, n, CLSID (mask %d)", found);
    }
    hr = ISpObjectToken_DeleteValue(tok, L"blob");
    CHECKF(hr == S_OK, "token DeleteValue (%#lx)", hr);
    hr = ISpObjectToken_DeleteValue(tok, L"blob");
    CHECKF(hr == SPERR_NOT_FOUND, "token DeleteValue again (%#lx)", hr);
    str = NULL;
    hr = ISpObjectToken_EnumKeys(tok, 0, &str);
    CHECKF(hr == S_OK && str && !wcsicmp(str, L"Attributes"), "token EnumKeys: Attributes (%#lx)", hr);
    CoTaskMemFree(str);
    hr = ISpObjectToken_EnumKeys(tok, 1, &str);
    CHECKF(hr == SPERR_NO_MORE_ITEMS, "token EnumKeys: no more (%#lx)", hr);
    hr = ISpObjectToken_DeleteKey(tok, L"Attributes");
    CHECKF(hr == S_OK, "token DeleteKey (%#lx)", hr);
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Vendor", L"A");
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Lang", L"409;40c");

    /* attributes */
    {
        static const struct { const WCHAR *attrs; HRESULT hr; BOOL match; } t[] =
        {
            { L"Vendor=A", S_OK, TRUE }, { L"Vendor=B", S_OK, FALSE }, { L"Vendor!=B", S_OK, TRUE }, { L"Vendor!=A", S_OK, FALSE },
            { L"Vendor", S_OK, TRUE }, { L"Missing", S_OK, FALSE }, { L"Missing!=1", S_OK, TRUE }, { L"", S_OK, TRUE },
            { NULL, S_OK, TRUE }, { L"Vendor=A;Lang=409", S_OK, TRUE }, { L"Lang=40c", S_OK, TRUE }, { L"Vendor=A;Lang=40a", S_OK, FALSE },
            { L"Vendor!X", E_INVALIDARG, FALSE },
        };
        for (i = 0; i < (int)ARRAY_SIZE(t); i++)
        {
            b = 0x55;
            hr = ISpObjectToken_MatchesAttributes(tok, t[i].attrs, &b);
            CHECKF(hr == t[i].hr && (FAILED(hr) || b == t[i].match), "MatchesAttributes(%ls) is %d (hr %#lx, got %d)", t[i].attrs ? t[i].attrs : L"NULL", t[i].match, hr, b);
        }
        hr = ISpObjectToken_MatchesAttributes(tok, L"Vendor=A", NULL);
        CHECKF(hr == E_POINTER, "MatchesAttributes(NULL result) is E_POINTER (%#lx)", hr);
    }

    /* UI */
    b = 0x55;
    hr = ISpObjectToken_IsUISupported(tok, L"AddRemoveWord", NULL, 0, NULL, &b);
    CHECKF(hr == S_OK && b == FALSE, "IsUISupported: no UI, S_OK and FALSE (%#lx, %d)", hr, b);
    hr = ISpObjectToken_IsUISupported(tok, L"AddRemoveWord", NULL, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "IsUISupported(NULL result) is E_POINTER (%#lx)", hr);
    hr = ISpObjectToken_DisplayUI(tok, NULL, L"t", L"AddRemoveWord", NULL, 0, NULL);
    CHECKF(hr == SPERR_NOT_FOUND, "DisplayUI: nothing to display (%#lx)", hr);

    /* storage files */
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, expect);
    str = NULL;
    hr = ISpObjectToken_GetStorageFileName(tok, &PR_CALLER, NULL, L"x.dat", 0x801c, &str);
    CHECKF(hr == E_POINTER, "GetStorageFileName(NULL key) is E_POINTER (%#lx)", hr);
    hr = ISpObjectToken_GetStorageFileName(tok, &PR_CALLER, L"nokey", NULL, 0x801c, &str);
    CHECKF(hr == E_INVALIDARG, "GetStorageFileName of an unknown key without a name is E_INVALIDARG (%#lx)", hr);
    hr = ISpObjectToken_GetStorageFileName(tok, &PR_CALLER, L"f1", L"x.dat", 0x801c, &str);
    CHECKF(hr == S_OK && str, "GetStorageFileName creates a name (%#lx)", hr);
    if (hr == S_OK)
    {
        WCHAR want[MAX_PATH + 16];
        swprintf(want, ARRAY_SIZE(want), L"%ls\\x.dat", expect);
        CHECKF(!wcsicmp(str, want), "GetStorageFileName: <local app data>\\x.dat (%ls)", str);
        path = NULL;
        hr = ISpObjectToken_GetStorageFileName(tok, &PR_CALLER, L"f1", L"other.dat", 0x801c, &path);
        CHECKF(hr == S_OK && path && !wcsicmp(path, str), "GetStorageFileName: a registered key returns the same path");
        CoTaskMemFree(path);
        {
            HANDLE h = CreateFileW(str, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            CloseHandle(h);
            CHECKF(GetFileAttributesW(str) != INVALID_FILE_ATTRIBUTES, "storage file can be created at that path");
        }
        hr = ISpObjectToken_RemoveStorageFileName(tok, &PR_CALLER, L"f1", TRUE);
        CHECKF(hr == S_OK, "RemoveStorageFileName (%#lx)", hr);
        CHECKF(GetFileAttributesW(str) == INVALID_FILE_ATTRIBUTES, "RemoveStorageFileName deletes the file");
        hr = ISpObjectToken_RemoveStorageFileName(tok, &PR_CALLER, L"f1", TRUE);
        CHECKF(hr == SPERR_NOT_FOUND, "RemoveStorageFileName again is SPERR_NOT_FOUND (%#lx)", hr);
        CoTaskMemFree(str);
        str = NULL;
        hr = ISpObjectToken_GetStorageFileName(tok, &PR_CALLER, L"f1", L"y.dat", 0x801c, &str);
        CHECKF(hr == S_OK && str && wcsstr(str, L"\\y.dat"), "GetStorageFileName after the removal makes a new name");
        CoTaskMemFree(str);
        ISpObjectToken_RemoveStorageFileName(tok, &PR_CALLER, L"f1", FALSE);
    }
    hr = ISpObjectToken_GetStorageFileName(tok, &PR_CALLER, L"f2", L"z.dat", 0x801c, &str);
    if (hr == S_OK)
    {
        HANDLE h = CreateFileW(str, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(h);
        CoTaskMemFree(str);
    }

    /* Remove: the files of that caller and the registry key go */
    hr = ISpObjectToken_Remove(tok, &PR_CALLER);
    CHECKF(hr == S_OK, "Remove (%#lx)", hr);
    {
        HKEY k;
        LONG r = RegOpenKeyExW(HKEY_CURRENT_USER, CATSUB L"\\Tokens\\T1", 0, KEY_READ, &k);
        CHECKF(r == ERROR_FILE_NOT_FOUND, "Remove deletes the token's registry key (%ld)", r);
        r = RegOpenKeyExW(HKEY_CURRENT_USER, CATSUB L"\\Tokens\\T2", 0, KEY_READ, &k);
        CHECKF(r == ERROR_SUCCESS, "Remove leaves the other tokens");
        if (r == ERROR_SUCCESS) RegCloseKey(k);
    }
    {
        WCHAR want[MAX_PATH + 16];
        swprintf(want, ARRAY_SIZE(want), L"%ls\\z.dat", expect);
        CHECKF(GetFileAttributesW(want) == INVALID_FILE_ATTRIBUTES, "Remove with a caller deletes the caller's storage files");
    }
    ISpObjectToken_Release(tok);
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Vendor", L"A");
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Lang", L"409;40c");
    reg_set(CATSUB L"\\Tokens\\T1", NULL, L"Token one");
    reg_set(CATSUB L"\\Tokens\\T1", L"CLSID", L"{715d9c59-4442-11d2-9605-00c04f8ee628}");
    tok = new_token(NULL, TOK(L"T1"));
    hr = ISpObjectToken_Remove(tok, NULL);
    CHECKF(hr == S_OK, "Remove without a caller (%#lx)", hr);
    hr = ISpObjectToken_Remove(tok, NULL);
    CHECKF(hr == SPERR_NOT_FOUND, "Remove of a removed token is SPERR_NOT_FOUND (%#lx)", hr);
    ISpObjectToken_Release(tok);
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Vendor", L"A");
    reg_set(CATSUB L"\\Tokens\\T1\\Attributes", L"Lang", L"409;40c");
    reg_set(CATSUB L"\\Tokens\\T1", NULL, L"Token one");
    reg_set(CATSUB L"\\Tokens\\T1", L"CLSID", L"{715d9c59-4442-11d2-9605-00c04f8ee628}");
}

/* ---- the automation views ---------------------------------------------- */
static void test_automation(void)
{
    ISpObjectTokenCategory *cat;
    IEnumSpObjectTokens *en;
    IDispatch *tokens, *tok, *key, *sub, *catobj;
    IEnumVARIANT *ev, *ev2;
    ISpObjectToken *t;
    VARIANT r, a[4], items[4];
    ITypeInfo *ti;
    UINT cnt;
    ULONG got;
    HRESULT hr;
    LONG l;
    SAFEARRAY *sa;
    void *p;
    unsigned char bytes[3] = { 7, 8, 9 };

    cat = new_category(CATID, FALSE);
    ISpObjectTokenCategory_EnumTokens(cat, NULL, NULL, &en);
    IEnumSpObjectTokens_QueryInterface(en, &IID_IDispatch, (void **)&tokens);
    IEnumSpObjectTokens_Release(en);

    cnt = 9;
    hr = IDispatch_GetTypeInfoCount(tokens, &cnt);
    CHECKF(hr == S_OK && cnt == 1, "ObjectTokens: GetTypeInfoCount (%#lx, %u)", hr, cnt);
    hr = IDispatch_GetTypeInfo(tokens, 0, LOCALE_USER_DEFAULT, &ti);
    CHECKF(hr == S_OK, "ObjectTokens: GetTypeInfo(0) (%#lx)", hr);
    if (hr == S_OK) ITypeInfo_Release(ti);
    hr = IDispatch_GetTypeInfo(tokens, 1, LOCALE_USER_DEFAULT, &ti);
    CHECKF(hr == DISP_E_BADINDEX, "ObjectTokens: GetTypeInfo(1) is DISP_E_BADINDEX (%#lx)", hr);
    {
        DISPID id = 0; LPOLESTR n = (LPOLESTR)L"Count";
        hr = IDispatch_GetIDsOfNames(tokens, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
        CHECKF(hr == S_OK && id == 1, "ObjectTokens: GetIDsOfNames(Count) is 1 (%#lx, %ld)", hr, id);
    }
    hr = dcall(tokens, L"Count", &r, NULL, 0);
    CHECKF(hr == S_OK && V_I4(&r) == 3, "ObjectTokens.Count is 3 (%#lx)", hr);

    hr = dcall1(tokens, L"Item", &r, vi4(1));
    tok = disp_of(hr, &r);
    CHECKF(tok != NULL, "ObjectTokens.Item(1) (%#lx)", hr);
    hr = dcall1(tokens, L"Item", &r, vi4(-1));
    CHECKF(hr == E_INVALIDARG, "ObjectTokens.Item(-1) is E_INVALIDARG (%#lx)", hr);
    hr = dcall1(tokens, L"Item", &r, vi4(3));
    CHECKF(hr == E_INVALIDARG, "ObjectTokens.Item(3) is E_INVALIDARG (%#lx)", hr);

    /* _NewEnum */
    hr = dget(tokens, L"_NewEnum", &r, NULL, 0, DISPATCH_PROPERTYGET);
    CHECKF(hr == S_OK && (V_VT(&r) == VT_UNKNOWN || V_VT(&r) == VT_DISPATCH), "ObjectTokens._NewEnum (%#lx)", hr);
    if (hr == S_OK)
    {
        IUnknown_QueryInterface(V_UNKNOWN(&r), &IID_IEnumVARIANT, (void **)&ev);
        hr = IEnumVARIANT_Skip(ev, 1);
        CHECKF(hr == S_OK, "IEnumVARIANT::Skip(1) (%#lx)", hr);
        hr = IEnumVARIANT_Clone(ev, &ev2);
        CHECKF(hr == S_OK && ev2 && ev2 != ev, "IEnumVARIANT::Clone (%#lx)", hr);
        if (hr == S_OK)
        {
            VariantInit(&items[0]);
            hr = IEnumVARIANT_Next(ev2, 1, items, &got);
            CHECKF(hr == S_OK && got == 1 && V_VT(&items[0]) == VT_DISPATCH, "clone continues at the saved position (%#lx)", hr);
            if (hr == S_OK)
            {
                hr = dcall(V_DISPATCH(&items[0]), L"Id", &r, NULL, 0);
                CHECKF(hr == S_OK && is_str(&r, TOK(L"T2")), "clone: second token is T2 (%ls)", V_VT(&r) == VT_BSTR ? V_BSTR(&r) : L"?");
                VariantClear(&r);
                VariantClear(&items[0]);
            }
            IEnumVARIANT_Release(ev2);
        }
        hr = IEnumVARIANT_Next(ev, 1, items, &got);
        CHECKF(hr == S_OK && got == 1, "original still at the second token");
        if (hr == S_OK) VariantClear(&items[0]);
        hr = IEnumVARIANT_Skip(ev, 5);
        CHECKF(hr == S_FALSE, "IEnumVARIANT::Skip past the end is S_FALSE (%#lx)", hr);
        hr = IEnumVARIANT_Next(ev, 1, items, &got);
        CHECKF(hr == S_FALSE && got == 0, "IEnumVARIANT: at the end after a long Skip");
        hr = IEnumVARIANT_Reset(ev);
        CHECKF(hr == S_OK, "IEnumVARIANT::Reset (%#lx)", hr);
        hr = IEnumVARIANT_Next(ev, 4, items, &got);
        CHECKF(hr == S_FALSE && got == 3, "after Reset: Next(4) returns three and S_FALSE (%#lx, %lu)", hr, got);
        for (l = 0; l < (LONG)got; l++) VariantClear(&items[l]);
        IEnumVARIANT_Release(ev);
        VariantClear(&r);
    }

    /* ISpeechObjectToken */
    if (tok)
    {
        hr = dcall(tok, L"Id", &r, NULL, 0);
        CHECKF(hr == S_OK && is_str(&r, TOK(L"T2")), "SpeechToken.Id");
        VariantClear(&r);
        hr = dcall1(tok, L"GetAttribute", &r, vstr(L"Vendor"));
        CHECKF(hr == S_OK && is_str(&r, L"B"), "SpeechToken.GetAttribute(Vendor) is B (%#lx)", hr);
        VariantClear(&r);
        hr = dcall1(tok, L"GetAttribute", &r, vstr(L"NoSuchAttribute"));
        CHECKF(hr == SPERR_NOT_FOUND, "SpeechToken.GetAttribute of a missing attribute is SPERR_NOT_FOUND (%#lx)", hr);
        hr = dcall1(tok, L"MatchesAttributes", &r, vstr(L"Vendor=B;Lang=409"));
        CHECKF(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_TRUE, "SpeechToken.MatchesAttributes true (%#lx)", hr);
        hr = dcall1(tok, L"MatchesAttributes", &r, vstr(L"Vendor=A"));
        CHECKF(hr == S_OK && V_BOOL(&r) == VARIANT_FALSE, "SpeechToken.MatchesAttributes false (%#lx)", hr);
        a[0] = vstr(L"AddRemoveWord");
        a[1].vt = VT_EMPTY;
        a[2].vt = VT_EMPTY;
        hr = dcall(tok, L"IsUISupported", &r, a, 1);
        CHECKF(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_FALSE, "SpeechToken.IsUISupported is False (%#lx)", hr);
        SysFreeString(V_BSTR(&a[0]));

        hr = dget(tok, L"Category", &r, NULL, 0, DISPATCH_PROPERTYGET);
        catobj = disp_of(hr, &r);
        CHECKF(catobj != NULL, "SpeechToken.Category (%#lx)", hr);
        if (catobj)
        {
            hr = dcall(catobj, L"Id", &r, NULL, 0);
            CHECKF(hr == S_OK && is_str(&r, CATID), "SpeechTokenCategory.Id");
            VariantClear(&r);
            hr = dget(catobj, L"Default", &r, NULL, 0, DISPATCH_PROPERTYGET);
            CHECKF(hr == S_OK && is_str(&r, TOK(L"T2")), "SpeechTokenCategory.Default is the machine default (%#lx)", hr);
            VariantClear(&r);
            a[0] = vstr(TOK(L"T3"));
            hr = dget(catobj, L"Default", NULL, a, 1, DISPATCH_PROPERTYPUT);
            CHECKF(hr == S_OK, "SpeechTokenCategory.Default = T3 (%#lx)", hr);
            SysFreeString(V_BSTR(&a[0]));
            hr = dget(catobj, L"Default", &r, NULL, 0, DISPATCH_PROPERTYGET);
            CHECKF(hr == S_OK && is_str(&r, TOK(L"T3")), "SpeechTokenCategory.Default reads the user's choice");
            VariantClear(&r);
            {
                HKEY k;
                RegOpenKeyExW(HKEY_CURRENT_USER, CATSUB, 0, KEY_ALL_ACCESS, &k);
                RegDeleteValueW(k, L"DefaultTokenId");
                RegCloseKey(k);
            }
            a[0] = vstr(L"");
            a[1] = vstr(L"Vendor=A");
            hr = dcall(catobj, L"EnumerateTokens", &r, a, 2);
            CHECKF(hr == S_OK && V_VT(&r) == VT_DISPATCH, "SpeechTokenCategory.EnumerateTokens (%#lx)", hr);
            if (hr == S_OK)
            {
                hr = dcall(V_DISPATCH(&r), L"Count", &a[3], NULL, 0);
                CHECKF(hr == S_OK && V_I4(&a[3]) == 3, "EnumerateTokens with an optional attribute still lists three");
                VariantClear(&r);
            }
            SysFreeString(V_BSTR(&a[0])); SysFreeString(V_BSTR(&a[1]));
            a[0] = vi4(1);
            hr = dcall(catobj, L"GetDataKey", &r, a, 1);
            sub = disp_of(hr, &r);
            CHECKF(sub != NULL, "SpeechTokenCategory.GetDataKey(current user) (%#lx)", hr);
            if (sub) IDispatch_Release(sub);
            IDispatch_Release(catobj);
        }

        /* DataKey */
        hr = dget(tok, L"DataKey", &r, NULL, 0, DISPATCH_PROPERTYGET);
        key = disp_of(hr, &r);
        CHECKF(key != NULL, "SpeechToken.DataKey (%#lx)", hr);
        if (key)
        {
            a[0] = vstr(L"s"); a[1] = vstr(L"string");
            hr = dcall(key, L"SetStringValue", &r, a, 2);
            CHECKF(hr == S_OK, "DataKey.SetStringValue (%#lx)", hr);
            SysFreeString(V_BSTR(&a[0])); SysFreeString(V_BSTR(&a[1]));
            hr = dcall1(key, L"GetStringValue", &r, vstr(L"s"));
            CHECKF(hr == S_OK && is_str(&r, L"string"), "DataKey.GetStringValue");
            VariantClear(&r);
            a[0] = vstr(L"l"); a[1] = vi4(-5);
            hr = dcall(key, L"SetLongValue", &r, a, 2);
            CHECKF(hr == S_OK, "DataKey.SetLongValue (%#lx)", hr);
            hr = dcall1(key, L"GetLongValue", &r, vstr(L"l"));
            CHECKF(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == -5, "DataKey.GetLongValue is -5 (%#lx)", hr);
            hr = dcall1(key, L"GetLongValue", &r, vstr(L"nolong"));
            CHECKF(hr == SPERR_NOT_FOUND, "DataKey.GetLongValue of a missing value (%#lx)", hr);
            sa = SafeArrayCreateVector(VT_UI1, 0, 3);
            SafeArrayAccessData(sa, &p); memcpy(p, bytes, 3); SafeArrayUnaccessData(sa);
            a[0] = vstr(L"b"); V_VT(&a[1]) = VT_ARRAY | VT_UI1; V_ARRAY(&a[1]) = sa;
            hr = dcall(key, L"SetBinaryValue", &r, a, 2);
            CHECKF(hr == S_OK, "DataKey.SetBinaryValue (%#lx)", hr);
            SafeArrayDestroy(sa);
            hr = dcall1(key, L"GetBinaryValue", &r, vstr(L"b"));
            CHECKF(hr == S_OK && V_VT(&r) == (VT_ARRAY | VT_UI1), "DataKey.GetBinaryValue returns a byte array (%#lx)", hr);
            if (hr == S_OK && V_VT(&r) == (VT_ARRAY | VT_UI1))
            {
                LONG ub = -1;
                SafeArrayGetUBound(V_ARRAY(&r), 1, &ub);
                SafeArrayAccessData(V_ARRAY(&r), &p);
                CHECKF(ub == 2 && !memcmp(p, bytes, 3), "DataKey.GetBinaryValue: the three bytes");
                SafeArrayUnaccessData(V_ARRAY(&r));
                VariantClear(&r);
            }
            a[0] = vstr(L"b"); a[1] = vstr(L"x");
            hr = dcall(key, L"SetBinaryValue", &r, a, 2);
            CHECKF(hr == DISP_E_TYPEMISMATCH, "DataKey.SetBinaryValue with a string is a type mismatch (%#lx)", hr);
            SysFreeString(V_BSTR(&a[0])); SysFreeString(V_BSTR(&a[1]));
            hr = dcall1(key, L"CreateKey", &r, vstr(L"New"));
            sub = disp_of(hr, &r);
            CHECKF(sub != NULL, "DataKey.CreateKey returns a data key (%#lx)", hr);
            if (sub)
            {
                a[0] = vstr(L"in"); a[1] = vstr(L"side");
                hr = dcall(sub, L"SetStringValue", &r, a, 2);
                CHECKF(hr == S_OK, "sub key: SetStringValue");
                SysFreeString(V_BSTR(&a[0])); SysFreeString(V_BSTR(&a[1]));
                IDispatch_Release(sub);
            }
            hr = dcall1(key, L"OpenKey", &r, vstr(L"New"));
            sub = disp_of(hr, &r);
            CHECKF(sub != NULL, "DataKey.OpenKey (%#lx)", hr);
            if (sub)
            {
                hr = dcall1(sub, L"GetStringValue", &r, vstr(L"in"));
                CHECKF(hr == S_OK && is_str(&r, L"side"), "sub key: value written through CreateKey is there");
                VariantClear(&r);
                hr = dcall1(sub, L"EnumValues", &r, vi4(0));
                CHECKF(hr == S_OK && is_str(&r, L"in"), "sub key: EnumValues(0)");
                VariantClear(&r);
                hr = dcall1(sub, L"EnumValues", &r, vi4(1));
                CHECKF(hr == SPERR_NO_MORE_ITEMS, "sub key: EnumValues(1) is SPERR_NO_MORE_ITEMS (%#lx)", hr);
                hr = dcall1(sub, L"EnumValues", &r, vi4(-1));
                CHECKF(hr == E_INVALIDARG, "sub key: EnumValues(-1) is E_INVALIDARG (%#lx)", hr);
                IDispatch_Release(sub);
            }
            hr = dcall1(key, L"OpenKey", &r, vstr(L"Nope"));
            CHECKF(hr == SPERR_NOT_FOUND, "DataKey.OpenKey of a missing key (%#lx)", hr);
            hr = dcall1(key, L"DeleteKey", &r, vstr(L"New"));
            CHECKF(hr == S_OK, "DataKey.DeleteKey (%#lx)", hr);
            hr = dcall1(key, L"DeleteValue", &r, vstr(L"s"));
            CHECKF(hr == S_OK, "DataKey.DeleteValue (%#lx)", hr);
            hr = dcall1(key, L"DeleteValue", &r, vstr(L"s"));
            CHECKF(hr == SPERR_NOT_FOUND, "DataKey.DeleteValue again (%#lx)", hr);
            hr = dcall1(key, L"EnumKeys", &r, vi4(0));
            CHECKF(hr == S_OK && is_str(&r, L"Attributes"), "DataKey.EnumKeys(0) is Attributes (%#lx)", hr);
            VariantClear(&r);
            IDispatch_Release(key);
        }

        /* CreateInstance through the token's CLSID value, SetId, storage */
        IDispatch_Release(tok);
    }

    hr = CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&tok);
    CHECKF(hr == S_OK, "SpObjectToken as IDispatch");
    if (hr == S_OK)
    {
        a[0] = vstr(TOK(L"T1")); a[1] = vstr(L""); V_VT(&a[2]) = VT_BOOL; V_BOOL(&a[2]) = VARIANT_FALSE;
        hr = dcall(tok, L"SetId", &r, a, 3);
        CHECKF(hr == S_OK, "SpeechToken.SetId (%#lx)", hr);
        SysFreeString(V_BSTR(&a[0])); SysFreeString(V_BSTR(&a[1]));
        hr = dcall(tok, L"GetDescription", &r, (a[0] = vi4(0x409), a), 1);
        CHECKF(hr == S_OK && is_str(&r, L"Token one"), "SpeechToken.GetDescription is the default value (%#lx)", hr);
        VariantClear(&r);
        {
            VARIANT ca[2];
            ca[0].vt = VT_EMPTY; V_VT(&ca[0]) = VT_DISPATCH; V_DISPATCH(&ca[0]) = NULL;
            ca[1] = vi4(23);
            hr = dcall(tok, L"CreateInstance", &r, ca, 2);
            CHECKF(hr == S_OK && V_VT(&r) == VT_UNKNOWN && V_UNKNOWN(&r), "SpeechToken.CreateInstance creates the CLSID object (%#lx)", hr);
            if (hr == S_OK)
            {
                ISpStream *ss = NULL;
                hr = IUnknown_QueryInterface(V_UNKNOWN(&r), &IID_ISpStream, (void **)&ss);
                CHECKF(hr == S_OK, "SpeechToken.CreateInstance: the object is an SpStream");
                if (ss) ISpStream_Release(ss);
            }
            VariantClear(&r);
        }
        a[0] = vstr(L"{0a0b0c0d-1111-2222-3333-444444444444}"); a[1] = vstr(L"sf"); a[2] = vstr(L"auto.dat"); a[3] = vi4(0x801c);
        hr = dcall(tok, L"GetStorageFileName", &r, a, 4);
        CHECKF(hr == S_OK && V_VT(&r) == VT_BSTR && wcsstr(V_BSTR(&r), L"\\auto.dat"), "SpeechToken.GetStorageFileName (%#lx)", hr);
        VariantClear(&r);
        a[2] = vi4(1); V_VT(&a[2]) = VT_BOOL; V_BOOL(&a[2]) = VARIANT_TRUE; a[1] = vstr(L"sf");
        a[0] = vstr(L"{0a0b0c0d-1111-2222-3333-444444444444}");
        hr = dcall(tok, L"RemoveStorageFileName", &r, a, 3);
        CHECKF(hr == S_OK, "SpeechToken.RemoveStorageFileName (%#lx)", hr);
        hr = dcall1(tok, L"Remove", &r, vstr(L"{0a0b0c0d-1111-2222-3333-444444444444}"));
        CHECKF(hr == S_OK, "SpeechToken.Remove (%#lx)", hr);
        IDispatch_Release(tok);
    }
    {
        /* an uninitialized speech token */
        hr = CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&tok);
        hr = dget(tok, L"Id", &r, NULL, 0, DISPATCH_PROPERTYGET);
        CHECKF(hr == SPERR_UNINITIALIZED, "uninitialized SpeechToken.Id (%#lx)", hr);
        hr = dget(tok, L"DataKey", &r, NULL, 0, DISPATCH_PROPERTYGET);
        CHECKF(hr == SPERR_UNINITIALIZED, "uninitialized SpeechToken.DataKey (%#lx)", hr);
        hr = dget(tok, L"Category", &r, NULL, 0, DISPATCH_PROPERTYGET);
        CHECKF(hr == SPERR_UNINITIALIZED, "uninitialized SpeechToken.Category (%#lx)", hr);
        IDispatch_Release(tok);
    }
    IDispatch_Release(tokens);
    ISpObjectTokenCategory_Release(cat);
    (void)t; (void)to_bstr;
}

int main(void)
{
    CoInitialize(NULL);
    setup();
    test_data_key();
    test_enum();
    test_token();
    test_automation();
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SGProbeSapi");
    CoUninitialize();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
