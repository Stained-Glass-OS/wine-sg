#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#undef SetPort
#include <urlmon.h>
#include <stdio.h>
static int bad;
#define EXPECT(c) do { if (!(c)) { bad++; printf("MISMATCH line %d\n", __LINE__); } } while (0)
static HRESULT build(const WCHAR *url, int op, const WCHAR *arg, DWORD port, BSTR *out)
{
    IUri *uri, *nuri; IUriBuilder *b; HRESULT hr;
    *out = NULL;
    hr = CreateUri(url, Uri_CREATE_ALLOW_RELATIVE, 0, &uri); if (hr) return hr;
    hr = CreateIUriBuilder(uri, 0, 0, &b); IUri_Release(uri); if (hr) return hr;
    if (op == 0) hr = IUriBuilder_SetPort(b, TRUE, port);
    else if (op == 1) hr = IUriBuilder_SetHost(b, arg);
    else if (op == 2) hr = IUriBuilder_SetPath(b, arg);
    else if (op == 3) hr = IUriBuilder_SetFragment(b, arg);
    if (hr == S_OK) hr = IUriBuilder_CreateUri(b, 0, 0, 0, &nuri);
    IUriBuilder_Release(b);
    if (hr) return hr;
    hr = IUri_GetAbsoluteUri(nuri, out); IUri_Release(nuri);
    return hr;
}
int main(void)
{
    BSTR s; HRESULT hr;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = build(L"http://b.example:8080/p/q?x=1#h", 0, NULL, 99, &s); printf("PORT %08lx %ls\n", hr, s ? s : L"-");
    EXPECT(hr == S_OK && s && !wcscmp(s, L"http://b.example:99/p/q?x=1#h")); SysFreeString(s);
    hr = build(L"http://b.example:8080/p/q", 1, L"c.example", 0, &s); printf("HOST %08lx %ls\n", hr, s ? s : L"-");
    EXPECT(hr == S_OK && s && !wcscmp(s, L"http://c.example:8080/p/q")); SysFreeString(s);
    hr = build(L"http://b.example:8080/p/q", 2, L"/z", 0, &s); printf("PATH %08lx %ls\n", hr, s ? s : L"-");
    EXPECT(hr == S_OK && s && !wcscmp(s, L"http://b.example:8080/z")); SysFreeString(s);
    hr = build(L"http://u:p@b.example:8080/p#f", 3, L"#g", 0, &s); printf("FRAG %08lx %ls\n", hr, s ? s : L"-");
    EXPECT(hr == S_OK && s && !wcscmp(s, L"http://u:p@b.example:8080/p#g")); SysFreeString(s);
    hr = build(L"http://[::1]:81/p", 0, NULL, 82, &s); printf("IPV6 %08lx %ls\n", hr, s ? s : L"-");
    EXPECT(hr == S_OK && s && !wcscmp(s, L"http://[::1]:82/p")); SysFreeString(s);
    printf("%s\n", bad ? "RESULT: FAIL" : "RESULT: PASS");
    return 0;
}
