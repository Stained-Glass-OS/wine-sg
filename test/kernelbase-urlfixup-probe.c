/* Probe for patches/sg/5066: UrlFixupW corrects a mistyped scheme.
 * The rows are the documented examples (https://learn.microsoft.com/en-us/windows/win32/api/
 * shlwapi/nf-shlwapi-urlfixupw) and the remark that "htp" becomes http, not ftp; plus a
 * drive letter (no scheme), and a correction made in place.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { const WCHAR *url; HRESULT hr; const WCHAR *fixed; } tests[] =
    {
        {L"http://www.microsoft.com", S_OK, L"http://www.microsoft.com"},
        {L"http:www.microsoft.com", S_OK, L"http://www.microsoft.com"},
        {L"mail:someone@example.com", S_OK, L"mailto:someone@example.com"},
        {L"abc:def", S_FALSE, NULL},
        {L"someone@example.com", S_FALSE, NULL},
        {L"htpp:wwwmicrosoft.com", S_OK, L"http://wwwmicrosoft.com"},
        {L"htps:\\\\www.microsoft.com", S_OK, L"http://www.microsoft.com"},
        {L"http:someone@example.com", S_OK, L"http://someone@example.com"},
        {L"htp:www.example.com", S_OK, L"http://www.example.com"},
        {L"c:\\windows", S_FALSE, NULL},
        {L"mailto:someone@example.com", S_OK, L"mailto:someone@example.com"},
    };
    WCHAR buf[256];
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        lstrcpyW(buf, L"untouched");
        hr = UrlFixupW(tests[i].url, buf, ARRAYSIZE(buf));
        check(hr == tests[i].hr && (!tests[i].fixed || !lstrcmpW(buf, tests[i].fixed)),
              "%ls: %#lx \"%ls\"", tests[i].url, hr, buf);
    }
    lstrcpyW(buf, L"htpp:www.example.com");
    hr = UrlFixupW(buf, buf, ARRAYSIZE(buf));
    check(hr == S_OK && !lstrcmpW(buf, L"http://www.example.com"), "in place: %#lx \"%ls\"", hr, buf);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
