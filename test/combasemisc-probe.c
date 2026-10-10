/* combase: WindowsReplaceString, HSTRING_User* marshalling, CoWaitForMultipleObjects,
 * CoInvalidateRemoteMachineBindings, CoAllowUnmarshalerCLSID (patches/sg/2240). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef void *HSTRING;
static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static HMODULE combase;
static const GUID nullguid;
static HRESULT (WINAPI *pCreate)(const WCHAR *, UINT32, HSTRING *);
static HRESULT (WINAPI *pDelete)(HSTRING);
static const WCHAR *(WINAPI *pRaw)(HSTRING, UINT32 *);
static HRESULT (WINAPI *pReplace)(HSTRING, HSTRING, HSTRING, HSTRING *);
static ULONG (WINAPI *pSize)(ULONG *, ULONG, HSTRING *);
static unsigned char *(WINAPI *pMarshal)(ULONG *, unsigned char *, HSTRING *);
static unsigned char *(WINAPI *pUnmarshal)(ULONG *, unsigned char *, HSTRING *);
static void (WINAPI *pFree)(ULONG *, HSTRING *);
static HRESULT (WINAPI *pWaitObjects)(DWORD, DWORD, ULONG, HANDLE *, DWORD *);
static HRESULT (WINAPI *pInvalidate)(const WCHAR *);
static HRESULT (WINAPI *pAllow)(const GUID *);

static HSTRING mk(const WCHAR *s) { HSTRING h = NULL; pCreate(s, s ? wcslen(s) : 0, &h); return h; }
static int is(HSTRING h, const WCHAR *s)
{
    UINT32 len = 0;
    const WCHAR *raw;
    if (!h) return !s || !*s;
    raw = pRaw(h, &len);
    return s && len == wcslen(s) && !memcmp(raw, s, len * sizeof(WCHAR));
}

static void test_replace(const WCHAR *src, const WCHAR *old, const WCHAR *new, const WCHAR *expect, const char *what)
{
    HSTRING s = mk(src), o = mk(old), n = new ? mk(new) : NULL, out = (HSTRING)1;
    HRESULT hr = pReplace(s, o, n, &out);
    CHECK(hr == S_OK && is(out, expect), "%s: hr %#lx result %ls", what, hr, hr == S_OK && out ? pRaw(out, NULL) : L"(null)");
    if (hr == S_OK) pDelete(out);
    pDelete(s); pDelete(o); if (n) pDelete(n);
}

int main(void)
{
    HSTRING a, b, out;
    HANDLE ev;
    DWORD idx;
    HRESULT hr;
    ULONG flags = 0, size;
    unsigned char buf[128];
    unsigned char *end;
    int i;

    combase = LoadLibraryA("combase.dll");
#define G(f, name) *(void **)&f = GetProcAddress(combase, name)
    G(pCreate, "WindowsCreateString"); G(pDelete, "WindowsDeleteString"); G(pRaw, "WindowsGetStringRawBuffer");
    G(pReplace, "WindowsReplaceString"); G(pSize, "HSTRING_UserSize"); G(pMarshal, "HSTRING_UserMarshal");
    G(pUnmarshal, "HSTRING_UserUnmarshal"); G(pFree, "HSTRING_UserFree");
    G(pWaitObjects, "CoWaitForMultipleObjects"); G(pInvalidate, "CoInvalidateRemoteMachineBindings");
    G(pAllow, "CoAllowUnmarshalerCLSID");
    CHECK(pReplace && pSize && pMarshal && pUnmarshal && pFree && pWaitObjects && pInvalidate && pAllow, "all exports present");
    if (!(pReplace && pSize && pMarshal && pUnmarshal && pFree && pWaitObjects && pInvalidate && pAllow))
    { printf("RESULT: FAIL\n"); return 1; }

    test_replace(L"a-b-c", L"-", L"--", L"a--b--c", "longer replacement");
    test_replace(L"a--b--c", L"--", L"-", L"a-b-c", "shorter replacement");
    test_replace(L"hello world", L"o", NULL, L"hell wrld", "NULL replacement deletes");
    test_replace(L"aaa", L"aa", L"b", L"ba", "non overlapping, left to right");
    test_replace(L"abc", L"x", L"y", L"abc", "no occurrence");
    test_replace(L"abab", L"abab", NULL, NULL, "everything deleted gives NULL");
    test_replace(L"foo", L"foo", L"bar", L"bar", "whole string");
    test_replace(L"x", L"xyz", L"q", L"x", "pattern longer than string");
    a = mk(L"abc");
    b = mk(L"b");
    out = NULL;
    hr = pReplace(NULL, b, a, &out);
    CHECK(hr == S_OK && !out, "NULL string: hr %#lx", hr);
    hr = pReplace(a, NULL, b, &out);
    CHECK(hr == E_INVALIDARG, "NULL pattern: %#lx", hr);
    hr = pReplace(a, b, b, NULL);
    CHECK(hr == E_INVALIDARG, "NULL out: %#lx", hr);
    pDelete(a); pDelete(b);

    /* HSTRING over the wire */
    a = mk(L"hello");
    size = pSize(&flags, 1, &a);
    CHECK(size == 4 + 4 + 10, "size of 'hello' starting at 1: %lu", size);
    memset(buf, 0xcc, sizeof(buf));
    end = pMarshal(&flags, buf, &a);
    CHECK(end == buf + 14 && *(ULONG *)buf == 5 && !memcmp(buf + 4, L"hello", 10), "marshalled form, end offset %d", (int)(end - buf));
    b = NULL;
    end = pUnmarshal(&flags, buf, &b);
    CHECK(end == buf + 14 && is(b, L"hello"), "unmarshal");
    pFree(&flags, &b);
    CHECK(b == NULL, "free clears");
    b = mk(L"old");
    pUnmarshal(&flags, buf, &b);   /* replaces what is there */
    CHECK(is(b, L"hello"), "unmarshal over an existing string");
    pDelete(b);
    pDelete(a);
    a = NULL;
    CHECK(pSize(&flags, 0, &a) == 4, "NULL string takes a length only");
    end = pMarshal(&flags, buf, &a);
    b = mk(L"x");
    end = pUnmarshal(&flags, buf, &b);
    CHECK(b == NULL && end == buf + 4, "NULL string round trip");
    /* alignment: a start not on 4 */
    a = mk(L"ab");
    CHECK(pSize(&flags, 3, &a) == 4 + 4 + 4, "aligned size: %lu", pSize(&flags, 3, &a));
    pDelete(a);
#ifdef _WIN64
    {
        void *p64 = GetProcAddress(combase, "HSTRING_UserSize64");
        CHECK(p64 != NULL, "64-bit names exported");
    }
#endif

    /* the COM entry points */
    ev = CreateEventW(NULL, TRUE, TRUE, NULL);
    idx = 0xdead;
    hr = pWaitObjects(0, 0, 1, &ev, &idx);
    CHECK(hr == S_OK && idx == 0, "wait on a signalled event: %#lx index %lu", hr, idx);
    ResetEvent(ev);
    idx = 0xdead;
    hr = pWaitObjects(0, 10, 1, &ev, &idx);
    CHECK(hr != S_OK && hr == (HRESULT)0x80010115 /* RPC_E_TIMEOUT */, "wait times out: %#lx", hr);
    CloseHandle(ev);
    CHECK(pInvalidate(L"server") == S_OK, "invalidate");
    CHECK(pAllow(&nullguid) == S_OK && pAllow(NULL) == E_INVALIDARG, "allow unmarshaler");

    (void)i;
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
