/* urlmon batch (patches/sg/2025): GetSecurityIdEx2 takes a domain in its
 * last parameter, which replaces the host in the security id
 * ("scheme:domain" and the zone as a DWORD). */
#define COBJMACROS
#include <windows.h>
#include <urlmon.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

struct t { const wchar_t *uri; const wchar_t *domain; const char *id; int idlen; HRESULT hr; };

int main(void)
{
    static const struct t tests[] = {
        { L"http://google.com.uk", L"com.uk", "http:com.uk\3\0\0\0", 15, S_OK },
        { L"http://google.com.uk", NULL, "http:google.com.uk\3\0\0\0", 22, S_OK },
        { L"http://google.com.uk/a?b=c", L"example.org", "http:example.org\3\0\0\0", 20, S_OK },
        { L"https://www.winehq.org/x", L"winehq.org", "https:winehq.org\3\0\0\0", 20, S_OK },
        { L"ftp://ftp.winehq.org/", L"org", "ftp:org\3\0\0\0", 11, S_OK },
        { L"http://google.com.uk", L"", "http:\3\0\0\0", 9, S_OK },
    };
    IInternetSecurityManager *mgr;
    IInternetSecurityManagerEx2 *mgr2;
    unsigned i;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoInternetCreateSecurityManager(NULL, &mgr, 0);
    if (FAILED(hr)) { printf("FAIL  create manager %08lx\n", hr); return 1; }
    hr = IInternetSecurityManager_QueryInterface(mgr, &IID_IInternetSecurityManagerEx2, (void **)&mgr2);
    if (FAILED(hr)) { printf("FAIL  no Ex2 %08lx\n", hr); return 1; }

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        IUri *uri;
        BYTE buf[256];
        DWORD size = sizeof(buf);
        char what[100];
        memset(buf, 0xf0, sizeof(buf));
        snprintf(what, sizeof(what), "case %u", i);
        hr = CreateUri(tests[i].uri, 0, 0, &uri);
        if (FAILED(hr)) { check(0, what); continue; }
        hr = IInternetSecurityManagerEx2_GetSecurityIdEx2(mgr2, uri, buf, &size, (DWORD_PTR)tests[i].domain);
        if (hr != tests[i].hr || size != (DWORD)tests[i].idlen || memcmp(buf, tests[i].id, tests[i].idlen))
            printf("   hr %08lx size %lu\n", hr, size);
        check(hr == tests[i].hr && size == (DWORD)tests[i].idlen && !memcmp(buf, tests[i].id, tests[i].idlen), what);
        IUri_Release(uri);
    }

    {   /* a too small buffer is measured with the domain */
        IUri *uri;
        BYTE buf[256];
        DWORD size = 10;
        CreateUri(L"http://google.com.uk", 0, 0, &uri);
        hr = IInternetSecurityManagerEx2_GetSecurityIdEx2(mgr2, uri, buf, &size, (DWORD_PTR)L"com.uk");
        check(hr == E_NOT_SUFFICIENT_BUFFER, "small buffer for the domain id");
        size = 15;
        hr = IInternetSecurityManagerEx2_GetSecurityIdEx2(mgr2, uri, buf, &size, (DWORD_PTR)L"com.uk");
        check(hr == S_OK && size == 15, "exact buffer for the domain id");
        IUri_Release(uri);
    }

    IInternetSecurityManagerEx2_Release(mgr2);
    IInternetSecurityManager_Release(mgr);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
