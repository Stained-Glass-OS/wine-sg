/* propsys string conversion batch (patches/sg/2018), run by test/pvstring-gate.sh.
 * Families (from Wine's todo_wine blocks, which record Windows):
 * PropVariantToString / ToStringAlloc / ToBSTR of numbers, times and vectors,
 * truncation into a small buffer, PropVariantToBuffer of a byte array,
 * PSRefreshPropertySchema without COM, VariantToPropVariant of bad types.
 *
 *   pvstring-probe.exe */
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <propvarutil.h>
#include <strsafe.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

HRESULT WINAPI PropVariantToBSTR(const PROPVARIANT *, BSTR *);
HRESULT WINAPI VariantToPropVariant(const VARIANT *, PROPVARIANT *);
HRESULT WINAPI PSRefreshPropertySchema(void);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* the three converters agree on the text */
static void text_is(const PROPVARIANT *pv, const WCHAR *want, const char *what)
{
    WCHAR buf[300] = { 0 }, *alloc = NULL;
    BSTR bstr = NULL;
    HRESULT hr1, hr2, hr3;
    char msg[200];

    hr1 = PropVariantToString(pv, buf, ARRAYSIZE(buf));
    hr2 = PropVariantToStringAlloc(pv, &alloc);
    hr3 = PropVariantToBSTR(pv, &bstr);
    snprintf(msg, sizeof(msg), "%s -> %ls (%08lx %08lx %08lx)", what, want, (unsigned long)hr1, (unsigned long)hr2, (unsigned long)hr3);
    check(hr1 == S_OK && hr2 == S_OK && hr3 == S_OK && !wcscmp(buf, want) && alloc && !wcscmp(alloc, want) && bstr && !wcscmp(bstr, want), msg);
    if (hr1 == S_OK && wcscmp(buf, want)) printf("   got %ls\n", buf);
    CoTaskMemFree(alloc);
    SysFreeString(bstr);
}

#define SCALAR(var, vt_, member, value, want)                       \
    do { PROPVARIANT pv_; memset(&pv_, 0, sizeof(pv_)); pv_.vt = vt_; pv_.member = value; text_is(&pv_, want, var); } while (0)

static void test_scalars(void)
{
    SCALAR("I1", VT_I1, cVal, -123, L"-123");
    SCALAR("I2", VT_I2, iVal, -456, L"-456");
    SCALAR("I4", VT_I4, lVal, -789, L"-789");
    SCALAR("I8", VT_I8, hVal.QuadPart, -101112, L"-101112");
    SCALAR("I8 min", VT_I8, hVal.QuadPart, (-9223372036854775807LL - 1), L"-9223372036854775808");
    SCALAR("UI1", VT_UI1, bVal, 0xcd, L"205");
    SCALAR("UI2", VT_UI2, uiVal, 0xdead, L"57005");
    SCALAR("UI4", VT_UI4, ulVal, 0xdeadbeef, L"3735928559");
    SCALAR("UI8", VT_UI8, uhVal.QuadPart, 0xdeadbeefdeadbeefULL, L"16045690984833335023");
    SCALAR("UI8 max", VT_UI8, uhVal.QuadPart, 0xffffffffffffffffULL, L"18446744073709551615");
    SCALAR("zero", VT_I4, lVal, 0, L"0");
    SCALAR("INT", VT_INT, lVal, -5, L"-5");
    SCALAR("UINT", VT_UINT, ulVal, 7, L"7");
    SCALAR("BOOL", VT_BOOL, boolVal, TRUE, L"1");
    SCALAR("R4", VT_R4, fltVal, 0.125f, L"0.125");
    SCALAR("R4 neg", VT_R4, fltVal, -2.25f, L"-2.25");
    SCALAR("R8", VT_R8, dblVal, 0.456, L"0.456");
    SCALAR("R8 whole", VT_R8, dblVal, 42.0, L"42");
    SCALAR("R8 neg", VT_R8, dblVal, -0.5, L"-0.5");

    {
        PROPVARIANT pv;
        memset(&pv, 0, sizeof(pv));
        pv.vt = VT_FILETIME;
        pv.filetime.dwLowDateTime = 0xdead;
        pv.filetime.dwHighDateTime = 0xbeef;
        text_is(&pv, L"1601/08/31:23:29:30.651", "FILETIME");
        pv.filetime.dwLowDateTime = 0;
        pv.filetime.dwHighDateTime = 0;
        text_is(&pv, L"1601/01/01:00:00:00.000", "FILETIME zero");
        pv.vt = VT_DATE;
        pv.date = 123.123f;
        text_is(&pv, L"1900/05/02:02:57:07.000", "DATE");
        pv.date = 0;
        text_is(&pv, L"1899/12/30:00:00:00.000", "DATE zero");
        pv.date = 36526.5;
        text_is(&pv, L"2000/01/01:12:00:00.000", "DATE noon of 2000");
    }
}

static void test_vectors(void)
{
    PROPVARIANT pv;
    unsigned char bytes[] = { 1, 20, 30, 4 };
    short shorts[] = { -1, 2 };
    ULONG longs[] = { 3000000000UL, 5 };
    ULONGLONG big[] = { 0xffffffffffffffffULL };
    double dbl[] = { 0.5, 1.5 };

    memset(&pv, 0, sizeof(pv));
    pv.vt = VT_VECTOR | VT_I1;
    pv.caub.cElems = 4; pv.caub.pElems = bytes;
    text_is(&pv, L"1; 20; 30; 4", "I1 vector");
    pv.vt = VT_VECTOR | VT_UI1;
    text_is(&pv, L"1; 20; 30; 4", "UI1 vector");
    pv.caub.cElems = 1;
    text_is(&pv, L"1", "one element");
    pv.caub.cElems = 0;
    text_is(&pv, L"", "empty vector");
    pv.vt = VT_VECTOR | VT_I2;
    pv.cai.cElems = 2; pv.cai.pElems = shorts;
    text_is(&pv, L"-1; 2", "I2 vector");
    pv.vt = VT_VECTOR | VT_UI4;
    pv.caul.cElems = 2; pv.caul.pElems = longs;
    text_is(&pv, L"3000000000; 5", "UI4 vector");
    pv.vt = VT_VECTOR | VT_UI8;
    pv.cauh.cElems = 1; pv.cauh.pElems = (ULARGE_INTEGER *)big;
    text_is(&pv, L"18446744073709551615", "UI8 vector");
    pv.vt = VT_VECTOR | VT_R8;
    pv.cadbl.cElems = 2; pv.cadbl.pElems = dbl;
    text_is(&pv, L"0.5; 1.5", "R8 vector");
}

static void test_truncation(void)
{
    PROPVARIANT pv;
    WCHAR buf[16];
    HRESULT hr;

    memset(&pv, 0, sizeof(pv));
    pv.vt = VT_UI4;
    pv.ulVal = 123456;
    wmemset(buf, 'x', 16);
    hr = PropVariantToString(&pv, buf, 4);
    check(hr == STRSAFE_E_INSUFFICIENT_BUFFER && !wcscmp(buf, L"123"), "a number cut to fit 4 characters");
    wmemset(buf, 'x', 16);
    hr = PropVariantToString(&pv, buf, 7);
    check(hr == S_OK && !wcscmp(buf, L"123456"), "an exact fit");
    wmemset(buf, 'x', 16);
    hr = PropVariantToString(&pv, buf, 6);
    check(hr == STRSAFE_E_INSUFFICIENT_BUFFER && !wcscmp(buf, L"12345"), "one short");
    hr = PropVariantToString(&pv, buf, 0);
    check(hr == E_INVALIDARG, "no room at all");
    pv.vt = VT_VECTOR | VT_UI1;
    {
        unsigned char b[] = { 1, 2, 3 };
        pv.caub.cElems = 3; pv.caub.pElems = b;
        hr = PropVariantToString(&pv, buf, 5);
        check(hr == STRSAFE_E_INSUFFICIENT_BUFFER && !wcscmp(buf, L"1; 2"), "a vector cut to fit");
    }
    pv.vt = VT_ERROR;
    wmemset(buf, 'x', 16);
    hr = PropVariantToString(&pv, buf, 16);
    check(FAILED(hr), "a type without text fails");
    check(buf[0] == 0, "and leaves an empty string");
}

static void test_buffer(void)
{
    static const UINT8 data[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    PROPVARIANT pv;
    SAFEARRAYBOUND bound = { 10, 0 };
    SAFEARRAY *sa;
    UINT8 buffer[32];
    void *p;
    HRESULT hr;

    sa = SafeArrayCreate(VT_UI1, 1, &bound);
    SafeArrayAccessData(sa, &p);
    memcpy(p, data, sizeof(data));
    SafeArrayUnaccessData(sa);
    memset(&pv, 0, sizeof(pv));
    pv.vt = VT_ARRAY | VT_UI1;
    pv.parray = sa;

    memset(buffer, 0, sizeof(buffer));
    hr = PropVariantToBuffer(&pv, buffer, 10);
    check(hr == S_OK && !memcmp(buffer, data, 10) && !buffer[10], "a byte array copies in full");
    memset(buffer, 0, sizeof(buffer));
    hr = PropVariantToBuffer(&pv, buffer, 4);
    check(hr == S_OK && !memcmp(buffer, data, 4) && !buffer[4], "and in part");
    buffer[0] = 99;
    hr = PropVariantToBuffer(&pv, buffer, 11);
    check(hr == E_FAIL && buffer[0] == 99, "asking for more than the array has fails");
    hr = PropVariantToBuffer(&pv, NULL, 0);
    check(hr == S_OK, "asking for nothing is fine");
    SafeArrayDestroy(sa);

    pv.parray = NULL;
    hr = PropVariantToBuffer(&pv, buffer, 1);
    check(hr == E_INVALIDARG, "no array");
    {
        SAFEARRAYBOUND b2[2] = { { 2, 0 }, { 2, 0 } };
        pv.parray = SafeArrayCreate(VT_UI1, 2, b2);
        hr = PropVariantToBuffer(&pv, buffer, 1);
        check(hr == E_INVALIDARG, "a two-dimensional array");
        SafeArrayDestroy(pv.parray);
    }
    {
        SAFEARRAYBOUND b1 = { 4, 5 };
        pv.parray = SafeArrayCreate(VT_UI1, 1, &b1);
        SafeArrayAccessData(pv.parray, &p);
        memcpy(p, data, 4);
        SafeArrayUnaccessData(pv.parray);
        memset(buffer, 0, sizeof(buffer));
        hr = PropVariantToBuffer(&pv, buffer, 4);
        check(hr == S_OK && !memcmp(buffer, data, 4), "an array with another lower bound");
        hr = PropVariantToBuffer(&pv, buffer, 5);
        check(hr == E_FAIL, "and its real size");
        SafeArrayDestroy(pv.parray);
    }
}

static void test_variants(void)
{
    PROPVARIANT pv;
    VARIANT v;
    HRESULT hr;

    VariantInit(&v);
    V_VT(&v) = 0xdead;
    hr = VariantToPropVariant(&v, &pv);
    check(hr == DISP_E_BADVARTYPE, "an unknown type");
    V_VT(&v) = VT_ILLEGAL;
    hr = VariantToPropVariant(&v, &pv);
    check(hr == TYPE_E_TYPEMISMATCH, "VT_ILLEGAL");
    V_VT(&v) = VT_CLSID;
    hr = VariantToPropVariant(&v, &pv);
    check(hr == DISP_E_BADVARTYPE, "VT_CLSID");
    V_VT(&v) = VT_I4;
    V_I4(&v) = 77;
    hr = VariantToPropVariant(&v, &pv);
    check(hr == S_OK && pv.vt == VT_I4 && pv.lVal == 77, "an integer");
    V_VT(&v) = VT_EMPTY;
    hr = VariantToPropVariant(&v, &pv);
    check(hr == S_OK && pv.vt == VT_EMPTY, "empty");
}

int main(void)
{
    HRESULT hr;

    hr = PSRefreshPropertySchema();
    check(hr == CO_E_NOTINITIALIZED, "PSRefreshPropertySchema without COM");
    CoInitialize(NULL);
    hr = PSRefreshPropertySchema();
    check(hr == S_OK, "PSRefreshPropertySchema with COM");

    test_scalars();
    test_vectors();
    test_truncation();
    test_buffer();
    test_variants();

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
