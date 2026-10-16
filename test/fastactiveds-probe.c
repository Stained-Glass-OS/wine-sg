#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <iads.h>
#include <adshlp.h>
#include <adserr.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_PN = {0x080d0d78,0xf421,0x11d0,{0xa3,0x6e,0x00,0xc0,0x4f,0xb9,0x50,0xdc}};
static const GUID IID_PN = {0xd592aed4,0xf420,0x11d0,{0xa3,0x6e,0x00,0xc0,0x4f,0xb9,0x50,0xdc}};

static void check_ret(IADsPathname *p, LONG type, const WCHAR *expect, int line)
{
    BSTR b = NULL;
    HRESULT hr = IADsPathname_Retrieve(p, type, &b);
    if (hr != S_OK || !b || wcscmp(b, expect))
    {
        fails++;
        printf("FAIL line %d: retrieve %ld -> %08lx %ls (expected %ls)\n", line, type, hr, b ? b : L"(null)", expect);
    }
    SysFreeString(b);
}
#define RET(p, t, e) check_ret(p, t, e, __LINE__)

int main(void)
{
    IADsPathname *p, *p2;
    IDispatch *disp, *copy;
    BSTR b;
    LONG n;
    HRESULT hr;
    WCHAR *enc = NULL, errbuf[64], namebuf[64];
    BYTE *dec = NULL, bytes[3] = { 0, 0xab, 0x7f };
    ULONG declen = 0;
    DWORD err = 0, ints[3] = { 1, 5, 9 };
    VARIANT v;
    LPWSTR str = NULL;
    ADSVALUE *vals;
    DWORD nvals;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_PN, NULL, CLSCTX_INPROC_SERVER, &IID_PN, (void **)&p);
    CHECK(hr == S_OK, "pathname %08lx", hr);
    if (hr == S_OK)
    {
        b = SysAllocString(L"LDAP://srv/CN=Jeff,OU=Sales,DC=corp,DC=com");
        hr = IADsPathname_Set(p, b, ADS_SETTYPE_FULL);
        SysFreeString(b);
        CHECK(hr == S_OK, "set");
        RET(p, ADS_FORMAT_X500, L"LDAP://srv/CN=Jeff,OU=Sales,DC=corp,DC=com");
        RET(p, ADS_FORMAT_X500_NO_SERVER, L"LDAP://CN=Jeff,OU=Sales,DC=corp,DC=com");
        RET(p, ADS_FORMAT_X500_DN, L"CN=Jeff,OU=Sales,DC=corp,DC=com");
        RET(p, ADS_FORMAT_X500_PARENT, L"LDAP://srv/OU=Sales,DC=corp,DC=com");
        RET(p, ADS_FORMAT_WINDOWS_DN, L"DC=com/DC=corp/OU=Sales/CN=Jeff");
        RET(p, ADS_FORMAT_WINDOWS, L"LDAP://srv/DC=com/DC=corp/OU=Sales/CN=Jeff");
        RET(p, ADS_FORMAT_WINDOWS_PARENT, L"LDAP://srv/DC=com/DC=corp/OU=Sales");
        RET(p, ADS_FORMAT_LEAF, L"CN=Jeff");
        hr = IADsPathname_SetDisplayType(p, ADS_DISPLAY_VALUE_ONLY);
        CHECK(hr == S_OK, "display type");
        RET(p, ADS_FORMAT_LEAF, L"Jeff");
        hr = IADsPathname_GetElement(p, 1, &b);
        CHECK(hr == S_OK && !wcscmp(b, L"Sales"), "element value only");
        SysFreeString(b);
        IADsPathname_SetDisplayType(p, ADS_DISPLAY_FULL);
        hr = IADsPathname_SetDisplayType(p, 77);
        CHECK(hr == E_INVALIDARG, "bad display type");

        b = SysAllocString(L"CN=Ann");
        hr = IADsPathname_AddLeafElement(p, b);
        SysFreeString(b);
        CHECK(hr == S_OK, "addleaf");
        hr = IADsPathname_GetNumElements(p, &n);
        CHECK(hr == S_OK && n == 5, "count %ld", n);
        RET(p, ADS_FORMAT_LEAF, L"CN=Ann");
        hr = IADsPathname_RemoveLeafElement(p);
        CHECK(hr == S_OK, "removeleaf");
        RET(p, ADS_FORMAT_X500_DN, L"CN=Jeff,OU=Sales,DC=corp,DC=com");

        hr = IADsPathname_CopyPath(p, &copy);
        CHECK(hr == S_OK && copy, "copy");
        if (copy)
        {
            IDispatch_QueryInterface(copy, &IID_PN, (void **)&p2);
            RET(p2, ADS_FORMAT_X500_DN, L"CN=Jeff,OU=Sales,DC=corp,DC=com");
            IADsPathname_RemoveLeafElement(p2);
            RET(p, ADS_FORMAT_X500_DN, L"CN=Jeff,OU=Sales,DC=corp,DC=com");
            IADsPathname_Release(p2);
            IDispatch_Release(copy);
        }

        b = SysAllocString(L"CN=Smith, John/Jr");
        { BSTR out = NULL; hr = IADsPathname_GetEscapedElement(p, 0, b, &out); SysFreeString(b); b = out; }
        CHECK(hr == S_OK, "escape %08lx", hr);
        CHECK(!wcscmp(b, L"CN=Smith\\, John\\/Jr"), "escaped '%ls'", b);
        SysFreeString(b);

        hr = IADsPathname_get_EscapedMode(p, &n);
        CHECK(hr == S_OK && n == 1, "escaped mode default %ld", n);
        hr = IADsPathname_put_EscapedMode(p, 3);
        CHECK(hr == S_OK, "put mode");
        IADsPathname_get_EscapedMode(p, &n);
        CHECK(n == 3, "mode stored");
        hr = IADsPathname_put_EscapedMode(p, 9);
        CHECK(hr == E_INVALIDARG, "bad mode");

        /* automation */
        hr = IADsPathname_QueryInterface(p, &IID_IDispatch, (void **)&disp);
        CHECK(hr == S_OK, "idispatch");
        {
            UINT cnt = 0;
            DISPID id;
            LPOLESTR name = (LPOLESTR)L"SetDisplayType";
            VARIANT arg;
            DISPPARAMS dp = { 0 };
            VARIANT res;

            hr = IDispatch_GetTypeInfoCount(disp, &cnt);
            CHECK(hr == S_OK && cnt == 1, "typeinfo count");
            hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id);
            CHECK(hr == S_OK, "ids %08lx", hr);
            VariantInit(&res);
            VariantInit(&arg);
            V_VT(&arg) = VT_I4;
            V_I4(&arg) = ADS_DISPLAY_VALUE_ONLY;
            dp.cArgs = 1;
            dp.rgvarg = &arg;
            hr = IDispatch_Invoke(disp, id, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dp, &res, NULL, NULL);
            CHECK(hr == S_OK, "invoke SetDisplayType %08lx", hr);
            RET(p, ADS_FORMAT_LEAF, L"Jeff");
        }
        IDispatch_Release(disp);
        IADsPathname_Release(p);
    }

    /* binary encoding */
    hr = ADsEncodeBinaryData(bytes, 3, &enc);
    CHECK(hr == S_OK && enc && !wcscmp(enc, L"\\00\\AB\\7F"), "encode %08lx %ls", hr, enc ? enc : L"(null)");
    hr = ADsDecodeBinaryData(enc, &dec, &declen);
    CHECK(hr == S_OK && declen == 3 && !memcmp(dec, bytes, 3), "decode %08lx %lu", hr, declen);
    FreeADsMem(dec);
    FreeADsMem(enc);
    hr = ADsDecodeBinaryData((LPWSTR)L"\\0", &dec, &declen);
    CHECK(FAILED(hr), "bad decode");

    /* last error is per thread */
    ADsSetLastError(1234, (LPWSTR)L"some failure", (LPWSTR)L"LDAP");
    hr = ADsGetLastError(&err, errbuf, 64, namebuf, 64);
    CHECK(hr == S_OK && err == 1234 && !wcscmp(errbuf, L"some failure") && !wcscmp(namebuf, L"LDAP"), "last error");

    /* ADsBuildVarArrayInt */
    VariantInit(&v);
    hr = ADsBuildVarArrayInt(ints, 3, &v);
    CHECK(hr == S_OK && V_VT(&v) == (VT_ARRAY | VT_VARIANT), "int array");
    if (hr == S_OK)
    {
        VARIANT item;
        LONG idx = 1;
        VariantInit(&item);
        SafeArrayGetElement(V_ARRAY(&v), &idx, &item);
        CHECK(V_VT(&item) == VT_I4 && V_I4(&item) == 5, "item");
        VariantClear(&v);
    }

    /* ReallocADsStr */
    str = AllocADsStr((LPWSTR)L"first");
    CHECK(ReallocADsStr(&str, (LPWSTR)L"second") && !wcscmp(str, L"second"), "realloc str");
    FreeADsStr(str);

    /* value conversions */
    VariantInit(&v);
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(L"hello");
    hr = PropVariantToAdsType(&v, 1, &vals, &nvals);
    CHECK(hr == S_OK && nvals == 1 && vals[0].dwType == ADSTYPE_CASE_IGNORE_STRING && !wcscmp(vals[0].CaseIgnoreString, L"hello"), "to ads %08lx", hr);
    if (hr == S_OK)
    {
        VARIANT back;
        hr = AdsTypeToPropVariant(vals, 1, &back);
        CHECK(hr == S_OK && V_VT(&back) == VT_BSTR && !wcscmp(V_BSTR(&back), L"hello"), "to variant");
        VariantClear(&back);
        AdsFreeAdsValues(vals, nvals);
        FreeADsMem(vals);
    }
    VariantClear(&v);
    V_VT(&v) = VT_I4;
    V_I4(&v) = 42;
    hr = PropVariantToAdsType(&v, 1, &vals, &nvals);
    CHECK(hr == S_OK && vals[0].dwType == ADSTYPE_INTEGER && vals[0].Integer == 42, "int to ads");
    FreeADsMem(vals);

    CoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
