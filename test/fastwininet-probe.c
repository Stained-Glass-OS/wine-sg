/* wininet batch (patches/sg/3002), run by test/fastwininet-gate.sh. Families: the per-site cookie
 * decisions (set, get, enumerate, clear), IsHostInProxyBypassList against the registry's
 * ProxyOverride, the global HTTP version and connected state options.
 *
 *   fastwininet-probe.exe */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(e) check((e), #e)

int main(void)
{
    ULONG decision, size;
    char name[64];
    DWORD err;
    HKEY key;
    HTTP_VERSION_INFO ver;
    DWORD vsize = sizeof(ver), state, ssize = sizeof(state);
    int i, found = 0;

    /* per-site cookie decisions */
    CHECK(InternetClearAllPerSiteCookieDecisions());
    decision = 99;
    SetLastError(0);
    CHECK(!InternetGetPerSiteCookieDecisionA("example.com", &decision));
    CHECK(decision == COOKIE_STATE_UNKNOWN);

    CHECK(InternetSetPerSiteCookieDecisionA("example.com", COOKIE_STATE_REJECT));
    CHECK(InternetSetPerSiteCookieDecisionA("wine.org", COOKIE_STATE_ACCEPT));
    decision = 0;
    CHECK(InternetGetPerSiteCookieDecisionA("example.com", &decision));
    CHECK(decision == COOKIE_STATE_REJECT);
    decision = 0;
    CHECK(InternetGetPerSiteCookieDecisionW(L"wine.org", &decision));
    CHECK(decision == COOKIE_STATE_ACCEPT);

    SetLastError(0);
    CHECK(!InternetSetPerSiteCookieDecisionA("example.com", COOKIE_STATE_MAX + 1));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);

    for (i = 0; i < 4; i++)
    {
        size = sizeof(name);
        decision = 99;
        if (!InternetEnumPerSiteCookieDecisionA(name, &size, &decision, i)) break;
        if (!strcmp(name, "example.com")) { found++; CHECK(decision == COOKIE_STATE_REJECT); }
        if (!strcmp(name, "wine.org")) { found++; CHECK(decision == COOKIE_STATE_ACCEPT); }
    }
    CHECK(found == 2);
    CHECK(i == 2);
    size = sizeof(name);
    SetLastError(0);
    CHECK(!InternetEnumPerSiteCookieDecisionA(name, &size, &decision, 2));
    CHECK(GetLastError() == ERROR_NO_MORE_ITEMS);
    size = 3;
    SetLastError(0);
    CHECK(!InternetEnumPerSiteCookieDecisionA(name, &size, &decision, 0));
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER && size > 3);

    /* "unknown" forgets the decision */
    CHECK(InternetSetPerSiteCookieDecisionA("example.com", COOKIE_STATE_UNKNOWN));
    CHECK(!InternetGetPerSiteCookieDecisionA("example.com", &decision));
    CHECK(InternetClearAllPerSiteCookieDecisions());
    CHECK(!InternetGetPerSiteCookieDecisionA("wine.org", &decision));

    /* proxy bypass list */
    err = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, NULL, 0,
            KEY_SET_VALUE, NULL, &key, NULL);
    CHECK(err == ERROR_SUCCESS);
    if (!err)
    {
        static const char bypass[] = "*.internal.example;intranet;<local>";
        RegSetValueExA(key, "ProxyOverride", 0, REG_SZ, (const BYTE *)bypass, sizeof(bypass));
        RegCloseKey(key);
    }
    /* the settings are read when wininet first needs them: nothing here has asked yet in this process */
    {
        BOOL (WINAPI *bypass)(INTERNET_SCHEME, LPCSTR, DWORD) =
            (void *)GetProcAddress(GetModuleHandleA("wininet.dll"), "IsHostInProxyBypassList");
        check(bypass != NULL, "IsHostInProxyBypassList is exported");
        if (bypass)
        {
            CHECK(bypass(INTERNET_SCHEME_HTTP, "intranet", 8));
            CHECK(bypass(INTERNET_SCHEME_HTTP, "host.internal.example", 21));
            CHECK(bypass(INTERNET_SCHEME_HTTP, "plainname", 9));
            CHECK(!bypass(INTERNET_SCHEME_HTTP, "www.example.com", 15));
            CHECK(!bypass(INTERNET_SCHEME_HTTP, NULL, 0));
        }
    }

    /* global options */
    ver.dwMajorVersion = ver.dwMinorVersion = 0;
    CHECK(InternetQueryOptionA(NULL, INTERNET_OPTION_HTTP_VERSION, &ver, &vsize));
    CHECK(ver.dwMajorVersion == 1 && ver.dwMinorVersion == 1);
    ver.dwMajorVersion = 1;
    ver.dwMinorVersion = 0;
    CHECK(InternetSetOptionA(NULL, INTERNET_OPTION_HTTP_VERSION, &ver, sizeof(ver)));
    ver.dwMajorVersion = ver.dwMinorVersion = 7;
    vsize = sizeof(ver);
    CHECK(InternetQueryOptionA(NULL, INTERNET_OPTION_HTTP_VERSION, &ver, &vsize));
    CHECK(ver.dwMajorVersion == 1 && ver.dwMinorVersion == 0);
    SetLastError(0);
    CHECK(!InternetSetOptionA(NULL, INTERNET_OPTION_HTTP_VERSION, &ver, 3));
    CHECK(GetLastError() == ERROR_INTERNET_BAD_OPTION_LENGTH);

    state = 0;
    CHECK(InternetQueryOptionA(NULL, INTERNET_OPTION_CONNECTED_STATE, &state, &ssize));
    CHECK(state == INTERNET_STATE_CONNECTED);
    state = INTERNET_STATE_DISCONNECTED_BY_USER;
    CHECK(InternetSetOptionA(NULL, INTERNET_OPTION_CONNECTED_STATE, &state, sizeof(state)));
    state = 0;
    ssize = sizeof(state);
    CHECK(InternetQueryOptionA(NULL, INTERNET_OPTION_CONNECTED_STATE, &state, &ssize));
    CHECK(state == INTERNET_STATE_DISCONNECTED_BY_USER);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
