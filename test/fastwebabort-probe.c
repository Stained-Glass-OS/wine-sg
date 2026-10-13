#include <windows.h>
#include <webservices.h>
#include <stdio.h>
#include <string.h>
#define WS_E_INVALID_FORMAT ((HRESULT)0x803d0000)
#define WS_E_INVALID_OPERATION ((HRESULT)0x803d0003)
static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

typedef HRESULT (WINAPI *combine_t)(const WS_STRING *, const WS_STRING *, ULONG, WS_HEAP *, WS_STRING *, WS_ERROR *);
typedef HRESULT (WINAPI *copyerr_t)(WS_ERROR *, WS_ERROR *);
typedef HRESULT (WINAPI *trim_t)(WCHAR *, ULONG, WCHAR **, ULONG *, WS_ERROR *);
typedef HRESULT (WINAPI *ncname_t)(const WCHAR *, ULONG, WS_ERROR *);
typedef HRESULT (WINAPI *abortl_t)(WS_LISTENER *, WS_ERROR *);
static combine_t pCombine; static copyerr_t pCopy; static trim_t pTrim; static ncname_t pNC; static abortl_t pAbortL;

static const char *combine(WS_HEAP *heap, const char *base, const char *ref, ULONG flags, HRESULT *hr)
{
    static char out[512];
    WCHAR b[256], r[256];
    WS_STRING bs, rs, res;
    ULONG i;
    for (i = 0; base[i]; i++) b[i] = base[i];
    bs.chars = b; bs.length = i;
    for (i = 0; ref[i]; i++) r[i] = ref[i];
    rs.chars = r; rs.length = i;
    memset(&res, 0, sizeof(res));
    WsResetHeap(heap, NULL);
    *hr = pCombine(&bs, &rs, flags, heap, &res, NULL);
    if (*hr != S_OK) return "";
    for (i = 0; i < res.length; i++) out[i] = (char)res.chars[i];
    out[i] = 0;
    return out;
}

static void test_combine(void)
{
    static const struct { const char *ref, *want; } t[] = {
        {"g:h","g:h"},{"g","http://a/b/c/g"},{"./g","http://a/b/c/g"},{"g/","http://a/b/c/g/"},
        {"/g","http://a/g"},{"//g","http://g"},{"?y","http://a/b/c/d;p?y"},{"g?y","http://a/b/c/g?y"},
        {"#s","http://a/b/c/d;p?q#s"},{"g#s","http://a/b/c/g#s"},{"g?y#s","http://a/b/c/g?y#s"},
        {";x","http://a/b/c/;x"},{"g;x","http://a/b/c/g;x"},{"","http://a/b/c/d;p?q"},{".","http://a/b/c/"},
        {"./","http://a/b/c/"},{"..","http://a/b/"},{"../","http://a/b/"},{"../g","http://a/b/g"},
        {"../..","http://a/"},{"../../","http://a/"},{"../../g","http://a/g"},{"../../../g","http://a/g"},
        {"/./g","http://a/g"},{"/../g","http://a/g"},{"g.","http://a/b/c/g."},{".g","http://a/b/c/.g"},
        {"g..","http://a/b/c/g.."},{"..g","http://a/b/c/..g"},{"./../g","http://a/b/g"},{"./g/.","http://a/b/c/g/"},
        {"g/./h","http://a/b/c/g/h"},{"g/../h","http://a/b/c/h"},{"g;x=1/./y","http://a/b/c/g;x=1/y"},
        {"g;x=1/../y","http://a/b/c/y"},{"g?y/./x","http://a/b/c/g?y/./x"},{"http:g","http:g"},
    };
    WS_HEAP *heap;
    HRESULT hr;
    ULONG i;
    EXPECT(WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL) == S_OK);
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        const char *s = combine(heap, "http://a/b/c/d;p?q", t[i].ref, 0, &hr);
        total++;
        if (hr != S_OK || strcmp(s, t[i].want)) { bad++; printf("MISMATCH combine '%s': got '%s' hr %lx, want '%s'\n", t[i].ref, s, hr, t[i].want); }
    }
    EXPECT(!strcmp(combine(heap, "http://a/b/c/d", "../../../x", WS_URL_FLAGS_NO_PATH_COLLAPSE, &hr), "http://a/b/c/../../../x"));
    EXPECT(!strcmp(combine(heap, "http://a", "x", 0, &hr), "http://a/x"));
    EXPECT(!strcmp(combine(heap, "net.tcp://h:80/p", "q", 0, &hr), "net.tcp://h:80/q"));
    combine(heap, "/relative", "x", 0, &hr);
    EXPECT(hr == WS_E_INVALID_FORMAT);
    {
        WS_STRING r = {0, NULL}, b = {1, L"x"}, res;
        EXPECT(pCombine(NULL, &b, 0, heap, &res, NULL) == E_INVALIDARG);
        EXPECT(pCombine(&b, &b, 0, NULL, &res, NULL) == E_INVALIDARG);
        EXPECT(pCombine(&b, &b, 0x80, heap, &res, NULL) == E_INVALIDARG);
        (void)r;
    }
    {
        WS_STRING b = {17, L"http://host/a/b/c"}, r = {1, L"d"}, res;
        EXPECT(pCombine(&b, &r, WS_URL_FLAGS_ZERO_TERMINATE, heap, &res, NULL) == S_OK && res.length == 17 && res.chars[res.length] == 0);
    }
    WsFreeHeap(heap);
}

static void test_text(void)
{
    WCHAR a[] = L" \t\r\n x y \n", *out;
    WCHAR none[] = L" \t ", empty[] = L"";
    ULONG len;
    EXPECT(pTrim(a, 10, &out, &len, NULL) == S_OK && out == a + 5 && len == 3);
    EXPECT(pTrim(none, 3, &out, &len, NULL) == S_OK && len == 0);
    EXPECT(pTrim(empty, 0, &out, &len, NULL) == S_OK && len == 0);
    EXPECT(pTrim(NULL, 0, &out, &len, NULL) == E_INVALIDARG);
    EXPECT(pTrim(a, 1, NULL, &len, NULL) == E_INVALIDARG);
    EXPECT(pNC(L"abc", 3, NULL) == S_OK);
    EXPECT(pNC(L"_a-b.c1", 7, NULL) == S_OK);
    EXPECT(pNC(L"a:b", 3, NULL) == WS_E_INVALID_FORMAT);
    EXPECT(pNC(L"1a", 2, NULL) == WS_E_INVALID_FORMAT);
    EXPECT(pNC(L"-a", 2, NULL) == WS_E_INVALID_FORMAT);
    EXPECT(pNC(L"a b", 3, NULL) == WS_E_INVALID_FORMAT);
    EXPECT(pNC(L"\x00e9t\x00e9", 3, NULL) == S_OK);
    EXPECT(pNC(L"\xd800\xdc00x", 3, NULL) == S_OK);
    EXPECT(pNC(L"\xd800x", 2, NULL) == WS_E_INVALID_FORMAT);
    EXPECT(pNC(L"", 0, NULL) == E_INVALIDARG);
    EXPECT(pNC(NULL, 3, NULL) == E_INVALIDARG);
}

static void test_copy_error(void)
{
    WS_ERROR *src, *dst;
    WS_STRING s1 = {5, L"first"}, s2 = {6, L"second"}, str;
    WS_FAULT_REASON reason = {{10, L"the reason"}, {2, L"en"}};
    WS_FAULT fault;
    WS_XML_STRING action = {6, (BYTE *)"urn:ac"};
    WS_FAULT *got;
    WS_XML_STRING gotact;
    ULONG code = 0x1234, n;
    LANGID lang;
    WS_ERROR_PROPERTY prop = {WS_ERROR_PROPERTY_ORIGINAL_ERROR_CODE, &code, sizeof(code)};

    EXPECT(WsCreateError(NULL, 0, &src) == S_OK);
    EXPECT(WsCreateError(NULL, 0, &dst) == S_OK);
    EXPECT(WsAddErrorString(src, &s1) == S_OK);
    EXPECT(WsAddErrorString(src, &s2) == S_OK);
    EXPECT(WsSetErrorProperty(src, WS_ERROR_PROPERTY_ORIGINAL_ERROR_CODE, &code, sizeof(code)) == S_OK);
    memset(&fault, 0, sizeof(fault));
    fault.reasons = &reason; fault.reasonCount = 1;
    EXPECT(WsSetFaultErrorProperty(src, WS_FAULT_ERROR_PROPERTY_FAULT, &fault, sizeof(fault)) == S_OK);
    EXPECT(WsSetFaultErrorProperty(src, WS_FAULT_ERROR_PROPERTY_ACTION, &action, sizeof(action)) == S_OK);
    EXPECT(WsAddErrorString(dst, &s2) == S_OK);   /* must be replaced */

    EXPECT(pCopy(src, dst) == S_OK);
    EXPECT(WsGetErrorProperty(dst, WS_ERROR_PROPERTY_STRING_COUNT, &n, sizeof(n)) == S_OK && n == 3);
    EXPECT(WsGetErrorString(dst, 2, &str) == S_OK && str.length == 5 && !memcmp(str.chars, L"first", 10));
    EXPECT(WsGetErrorString(dst, 0, &str) == S_OK && str.length > 5);   /* the fault reason string */
    EXPECT(WsGetErrorString(dst, 1, &str) == S_OK && str.length == 6 && !memcmp(str.chars, L"second", 12));
    code = 0;
    EXPECT(WsGetErrorProperty(dst, WS_ERROR_PROPERTY_ORIGINAL_ERROR_CODE, &code, sizeof(code)) == S_OK && code == 0x1234);
    EXPECT(WsGetErrorProperty(dst, WS_ERROR_PROPERTY_LANGID, &lang, sizeof(lang)) == S_OK);
    got = NULL;
    EXPECT(WsGetFaultErrorProperty(dst, WS_FAULT_ERROR_PROPERTY_FAULT, &got, sizeof(got)) == S_OK && got && got != &fault);
    EXPECT(got && got->reasonCount == 1 && got->reasons[0].text.length == 10 && !memcmp(got->reasons[0].text.chars, L"the reason", 20));
    EXPECT(WsGetFaultErrorProperty(dst, WS_FAULT_ERROR_PROPERTY_ACTION, &gotact, sizeof(gotact)) == S_OK &&
           gotact.length == 6 && !memcmp(gotact.bytes, "urn:ac", 6));
    /* the source is untouched and independent */
    EXPECT(WsGetErrorProperty(src, WS_ERROR_PROPERTY_STRING_COUNT, &n, sizeof(n)) == S_OK && n == 3);
    EXPECT(WsResetError(src) == S_OK);
    EXPECT(WsGetErrorString(dst, 2, &str) == S_OK && str.length == 5);

    /* copying an empty error clears the destination */
    EXPECT(pCopy(src, dst) == S_OK);
    EXPECT(WsGetErrorProperty(dst, WS_ERROR_PROPERTY_STRING_COUNT, &n, sizeof(n)) == S_OK && n == 0);
    got = (void *)1;
    EXPECT(WsGetFaultErrorProperty(dst, WS_FAULT_ERROR_PROPERTY_FAULT, &got, sizeof(got)) == S_OK && got == NULL);
    EXPECT(pCopy(dst, dst) == S_OK);
    EXPECT(pCopy(NULL, dst) == E_INVALIDARG);
    EXPECT(pCopy(src, NULL) == E_INVALIDARG);
    (void)prop;
    WsFreeError(src);
    WsFreeError(dst);
}

static void test_abort(void)
{
    WS_LISTENER *l, *l2;
    WS_SERVICE_PROXY *proxy;
    WS_LISTENER_STATE state;
    WS_SERVICE_PROXY_STATE pstate;
    WS_STRING url = {26, L"net.tcp://127.0.0.1:47123/"};
    HRESULT hr;

    EXPECT(WsCreateListener(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &l, NULL) == S_OK);
    EXPECT(pAbortL(NULL, NULL) == E_INVALIDARG);
    EXPECT(pAbortL(l, NULL) == S_OK);   /* created: nothing to abort */
    EXPECT(WsGetListenerProperty(l, WS_LISTENER_PROPERTY_STATE, &state, sizeof(state), NULL) == S_OK && state == WS_LISTENER_STATE_CREATED);
    hr = WsOpenListener(l, &url, NULL, NULL);
    EXPECT(hr == S_OK);
    if (hr == S_OK)
    {
        EXPECT(pAbortL(l, NULL) == S_OK);
        EXPECT(WsGetListenerProperty(l, WS_LISTENER_PROPERTY_STATE, &state, sizeof(state), NULL) == S_OK && state == WS_LISTENER_STATE_FAULTED);
        EXPECT(WsOpenListener(l, &url, NULL, NULL) == WS_E_INVALID_OPERATION);
        /* the port was released */
        EXPECT(WsCreateListener(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &l2, NULL) == S_OK);
        EXPECT(WsOpenListener(l2, &url, NULL, NULL) == S_OK);
        EXPECT(WsCloseListener(l2, NULL, NULL) == S_OK);
        WsFreeListener(l2);
        EXPECT(WsResetListener(l, NULL) == S_OK);
        EXPECT(WsGetListenerProperty(l, WS_LISTENER_PROPERTY_STATE, &state, sizeof(state), NULL) == S_OK && state == WS_LISTENER_STATE_CREATED);
        EXPECT(WsOpenListener(l, &url, NULL, NULL) == S_OK);
    }
    WsFreeListener(l);

    EXPECT(WsCreateServiceProxy(WS_CHANNEL_TYPE_REQUEST, WS_HTTP_CHANNEL_BINDING, NULL, NULL, 0, NULL, 0, &proxy, NULL) == S_OK);
    EXPECT(WsAbortServiceProxy(NULL, NULL) == E_INVALIDARG);
    EXPECT(WsAbortServiceProxy(proxy, NULL) == S_OK);
    EXPECT(WsGetServiceProxyProperty(proxy, WS_PROXY_PROPERTY_STATE, &pstate, sizeof(pstate), NULL) == S_OK && pstate == WS_SERVICE_PROXY_STATE_CREATED);
    WsFreeServiceProxy(proxy);
}

int main(void)
{
    HMODULE ws = LoadLibraryA("webservices.dll");
    pCombine = (combine_t)GetProcAddress(ws, "WsCombineUrl");
    pCopy = (copyerr_t)GetProcAddress(ws, "WsCopyError");
    pTrim = (trim_t)GetProcAddress(ws, "WsTrimXmlWhitespace");
    pNC = (ncname_t)GetProcAddress(ws, "WsVerifyXmlNCName");
    pAbortL = (abortl_t)GetProcAddress(ws, "WsAbortListener");
    if (!pCombine || !pCopy || !pTrim || !pNC || !pAbortL) { printf("missing exports\nRESULT: FAIL\n"); return 1; }
    test_combine();
    test_text();
    test_copy_error();
    test_abort();
    printf("checks %d, mismatches %d\n", total, bad);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
