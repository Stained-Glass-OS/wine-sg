/* Probe for patches/sg/5044: UrlEscapeW with URL_ESCAPE_ASCII_URI_COMPONENT
 * "percent-encode[s] all ASCII characters outside of the unreserved set from
 * URI RFC 3986 (a-zA-Z0-9-.~_)" over the whole string (no URL parts);
 * URL_ESCAPE_URI_COMPONENT adds AS_UTF8, so other characters become UTF-8
 * %XX; without it they stay. The flag was ignored.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef URL_ESCAPE_ASCII_URI_COMPONENT
#define URL_ESCAPE_ASCII_URI_COMPONENT 0x00080000
#endif
#ifndef URL_ESCAPE_AS_UTF8
#define URL_ESCAPE_AS_UTF8 0x00040000
#endif

int main(void)
{
    static const struct { const WCHAR *in; DWORD flags; const WCHAR *out; } tests[] =
    {
        {L"a b/c?d#e~f-g_h.i%", URL_ESCAPE_ASCII_URI_COMPONENT, L"a%20b%2Fc%3Fd%23e~f-g_h.i%25"},
        {L"http://x.org/a b?q=1&r=2", URL_ESCAPE_ASCII_URI_COMPONENT, L"http%3A%2F%2Fx.org%2Fa%20b%3Fq%3D1%26r%3D2"},
        {L"AZaz09", URL_ESCAPE_ASCII_URI_COMPONENT, L"AZaz09"},
        {L"\x00e9t\x00e9", URL_ESCAPE_ASCII_URI_COMPONENT, L"\x00e9t\x00e9"},
        {L"\x00e9 x", URL_ESCAPE_ASCII_URI_COMPONENT | URL_ESCAPE_AS_UTF8, L"%C3%A9%20x"},
        {L"\xd83d\xde00", URL_ESCAPE_ASCII_URI_COMPONENT | URL_ESCAPE_AS_UTF8, L"%F0%9F%98%80"},
        {L"!*'();:@&=+$,", URL_ESCAPE_ASCII_URI_COMPONENT, L"%21%2A%27%28%29%3B%3A%40%26%3D%2B%24%2C"},
    };
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        WCHAR out[256];
        DWORD len = ARRAYSIZE(out);
        HRESULT hr = UrlEscapeW(tests[i].in, out, &len, tests[i].flags);

        check(hr == S_OK && !lstrcmpW(out, tests[i].out) && len == lstrlenW(tests[i].out), "case %u: %#lx \"%ls\" (%lu)", i, hr,
              hr == S_OK ? out : L"", len);
    }
    {
        WCHAR out[4];
        DWORD len = ARRAYSIZE(out);
        HRESULT hr = UrlEscapeW(L"a b c", out, &len, URL_ESCAPE_ASCII_URI_COMPONENT);
        check(hr == E_POINTER && len == 10, "short buffer: %#lx needs %lu", hr, len);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
