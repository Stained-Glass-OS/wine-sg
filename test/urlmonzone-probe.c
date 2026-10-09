/* urlmon zone manager and security manager batch (patches/sg/2004), run by
 * test/urlmonzone-gate.sh. Families: SetZoneMapping/GetZoneMappings (round
 * trip through MapUrlToZone), zone action policies (Set, then
 * ProcessUrlAction/Ex/Ex2 for allow, disallow and query), zone and
 * security-manager custom policies, LogAction and CompareSecurityIds.
 *
 *   urlmonzone-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <urlmon.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static IInternetSecurityManager *secmgr;
static IInternetSecurityManagerEx2 *secmgr2;
static IInternetZoneManager *zonemgr;

static DWORD zone_of(const WCHAR *url)
{
    DWORD zone = 0xdead;
    IInternetSecurityManager_MapUrlToZone(secmgr, url, &zone, 0);
    return zone;
}

static BOOL enum_has(IEnumString *en, const WCHAR *want)
{
    LPOLESTR str;
    BOOL found = FALSE;
    IEnumString_Reset(en);
    while (IEnumString_Next(en, 1, &str, NULL) == S_OK)
    {
        if (!wcsicmp(str, want)) found = TRUE;
        CoTaskMemFree(str);
    }
    return found;
}

static ULONG enum_count(IEnumString *en)
{
    LPOLESTR str;
    ULONG n = 0;
    IEnumString_Reset(en);
    while (IEnumString_Next(en, 1, &str, NULL) == S_OK) { n++; CoTaskMemFree(str); }
    return n;
}

static void test_zone_mappings(void)
{
    static const struct { const WCHAR *pattern; DWORD zone; const WCHAR *url; DWORD other_zone;
                          const WCHAR *other_url; const WCHAR *listed; } maps[] = {
        { L"http://www.sgzonea.com", 2, L"http://www.sgzonea.com/page.htm", 3, L"https://www.sgzonea.com/", L"http://www.sgzonea.com" },
        { L"sgnoscheme.example", 4, L"ftp://sgnoscheme.example/x", 3, L"http://other.example/", L"*://sgnoscheme.example" },
        { L"https://deep.sub.sgzoneb.net", 1, L"https://deep.sub.sgzoneb.net/", 3, L"https://sub.sgzoneb.net/", L"https://deep.sub.sgzoneb.net" },
        { L"*.sgzonec.org", 2, L"http://x.sgzonec.org/", 3, L"http://sgzoned.org/", L"*://sgzonec.org" },
    };
    IEnumString *en = NULL, *clone;
    unsigned i;
    HRESULT hr;
    LPOLESTR str;
    ULONG n;

    for (i = 0; i < ARRAY_SIZE(maps); i++)
    {
        char what[200];
        CHECK(zone_of(maps[i].url) == 3);
        hr = IInternetSecurityManager_SetZoneMapping(secmgr, maps[i].zone, maps[i].pattern, SZM_CREATE);
        snprintf(what, sizeof(what), "SetZoneMapping(%ls) hr %08lx", maps[i].pattern, (unsigned long)hr);
        check(hr == S_OK, what);
        snprintf(what, sizeof(what), "%ls maps to zone %lu (got %lu)", maps[i].url,
                 (unsigned long)maps[i].zone, (unsigned long)zone_of(maps[i].url));
        check(zone_of(maps[i].url) == maps[i].zone, what);
        snprintf(what, sizeof(what), "%ls stays in zone %lu (got %lu)", maps[i].other_url,
                 (unsigned long)maps[i].other_zone, (unsigned long)zone_of(maps[i].other_url));
        check(zone_of(maps[i].other_url) == maps[i].other_zone, what);

        en = NULL;
        hr = IInternetSecurityManager_GetZoneMappings(secmgr, maps[i].zone, &en, 0);
        snprintf(what, sizeof(what), "GetZoneMappings(%lu) hr %08lx", (unsigned long)maps[i].zone, (unsigned long)hr);
        check(hr == S_OK && en, what);
        if (en)
        {
            snprintf(what, sizeof(what), "zone %lu list contains %ls", (unsigned long)maps[i].zone, maps[i].listed);
            check(enum_has(en, maps[i].listed), what);
            IEnumString_Release(en);
        }
    }

    /* enumerator behaviour: zone 2 has two entries here */
    hr = IInternetSecurityManager_GetZoneMappings(secmgr, 2, &en, 0);
    CHECK(hr == S_OK && en);
    if (en)
    {
        CHECK(enum_count(en) == 2);
        IEnumString_Reset(en);
        CHECK(IEnumString_Skip(en, 1) == S_OK);
        n = 0;
        CHECK(IEnumString_Next(en, 1, &str, &n) == S_OK && n == 1);
        CoTaskMemFree(str);
        CHECK(IEnumString_Next(en, 1, &str, &n) == S_FALSE && n == 0);
        IEnumString_Reset(en);
        CHECK(IEnumString_Clone(en, &clone) == S_OK && clone && enum_count(clone) == 2);
        if (clone) IEnumString_Release(clone);
        CHECK(IEnumString_Skip(en, 5) == S_FALSE);
        IEnumString_Release(en);
    }
    hr = IInternetSecurityManager_GetZoneMappings(secmgr, 4, &en, 0);
    CHECK(hr == S_OK && en && enum_has(en, L"*://sgnoscheme.example"));
    if (en) IEnumString_Release(en);

    /* delete */
    hr = IInternetSecurityManager_SetZoneMapping(secmgr, 2, L"http://www.sgzonea.com", SZM_DELETE);
    CHECK(hr == S_OK);
    CHECK(zone_of(L"http://www.sgzonea.com/page.htm") == 3);
    hr = IInternetSecurityManager_GetZoneMappings(secmgr, 2, &en, 0);
    CHECK(hr == S_OK && en && !enum_has(en, L"http://www.sgzonea.com") && enum_has(en, L"*://sgzonec.org"));
    if (en) IEnumString_Release(en);
    hr = IInternetSecurityManager_SetZoneMapping(secmgr, 4, L"sgnoscheme.example", SZM_DELETE);
    CHECK(hr == S_OK && zone_of(L"ftp://sgnoscheme.example/x") == 3);
    hr = IInternetSecurityManager_SetZoneMapping(secmgr, 1, L"https://deep.sub.sgzoneb.net", SZM_DELETE);
    CHECK(hr == S_OK && zone_of(L"https://deep.sub.sgzoneb.net/") == 3);
    hr = IInternetSecurityManager_SetZoneMapping(secmgr, 2, L"*.sgzonec.org", SZM_DELETE);
    CHECK(hr == S_OK && zone_of(L"http://x.sgzonec.org/") == 3);

    CHECK(IInternetSecurityManager_SetZoneMapping(secmgr, 2, NULL, SZM_CREATE) == E_INVALIDARG);
    CHECK(IInternetSecurityManager_SetZoneMapping(secmgr, 2, L"http://x.invalid", 5) == E_INVALIDARG);
    CHECK(IInternetSecurityManager_SetZoneMapping(secmgr, 2, L"", SZM_CREATE) == E_INVALIDARG);
}

#define ACTION_TEST 0x00001a10
static void test_policies(void)
{
    static const WCHAR url[] = L"http://policy.sgzonep.com/";
    DWORD pol, got, out;
    BYTE *buf;
    DWORD size;
    IUri *uri;
    HRESULT hr;
    GUID guid = {0x5e600444, 0x1234, 0x4321, {1, 2, 3, 4, 5, 6, 7, 8}};
    BYTE blob[5] = { 9, 8, 7, 6, 5 };
    BYTE ctx[4] = { 0 };

    CHECK(IInternetSecurityManager_SetZoneMapping(secmgr, 2, L"http://policy.sgzonep.com", SZM_CREATE) == S_OK);
    CHECK(zone_of(url) == 2);
    CreateUri(url, 0, 0, &uri);

    pol = 0x3; /* URLPOLICY_DISALLOW */
    hr = IInternetZoneManager_SetZoneActionPolicy(zonemgr, 2, ACTION_TEST, (BYTE *)&pol, sizeof(pol), URLZONEREG_HKCU);
    check(hr == S_OK, "SetZoneActionPolicy(HKCU) succeeds");
    got = 0xff;
    hr = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 2, ACTION_TEST, (BYTE *)&got, sizeof(got), URLZONEREG_HKCU);
    check(hr == S_OK && got == 0x3, "GetZoneActionPolicy returns the stored policy");
    got = 0;
    hr = IInternetSecurityManager_ProcessUrlAction(secmgr, url, ACTION_TEST, (BYTE *)&got, sizeof(got), NULL, 0, 0, 0);
    check(hr == S_FALSE && got == 3, "ProcessUrlAction disallow -> S_FALSE, policy returned");

    out = 0xdead;
    hr = IInternetSecurityManagerEx2_ProcessUrlActionEx(secmgr2, url, ACTION_TEST, (BYTE *)&got, sizeof(got), ctx, 0, 0, 0, &out);
    check(hr == S_FALSE && out == 0, "ProcessUrlActionEx disallow -> S_FALSE, outflags 0");
    out = 0xdead;
    hr = IInternetSecurityManagerEx2_ProcessUrlActionEx2(secmgr2, uri, ACTION_TEST, (BYTE *)&got, sizeof(got), ctx, 0, 0, 0, &out);
    check(hr == S_FALSE && out == 0, "ProcessUrlActionEx2 disallow -> S_FALSE, outflags 0");

    pol = 0; /* ALLOW */
    IInternetZoneManager_SetZoneActionPolicy(zonemgr, 2, ACTION_TEST, (BYTE *)&pol, sizeof(pol), URLZONEREG_DEFAULT);
    hr = IInternetSecurityManager_ProcessUrlAction(secmgr, url, ACTION_TEST, (BYTE *)&got, sizeof(got), NULL, 0, 0, 0);
    check(hr == S_OK, "ProcessUrlAction allow -> S_OK");
    out = 0xdead;
    hr = IInternetSecurityManagerEx2_ProcessUrlActionEx(secmgr2, url, ACTION_TEST, (BYTE *)&got, sizeof(got), ctx, 0, 0, 0, &out);
    check(hr == S_OK && out == 0, "ProcessUrlActionEx allow -> S_OK");
    hr = IInternetSecurityManagerEx2_ProcessUrlActionEx2(secmgr2, uri, ACTION_TEST, (BYTE *)&got, sizeof(got), ctx, 0, 0, 0, &out);
    check(hr == S_OK, "ProcessUrlActionEx2 allow -> S_OK");

    pol = 1; /* QUERY */
    hr = IInternetZoneManager_SetZoneActionPolicy(zonemgr, 2, ACTION_TEST, (BYTE *)&pol, sizeof(pol), URLZONEREG_HKCU);
    CHECK(hr == S_OK);
    hr = IInternetSecurityManager_ProcessUrlAction(secmgr, url, ACTION_TEST, (BYTE *)&got, sizeof(got), NULL, 0, 0, 0);
    check(hr == S_FALSE, "ProcessUrlAction query policy is refused without UI (S_FALSE)");

    hr = IInternetZoneManager_SetZoneActionPolicy(zonemgr, 2, ACTION_TEST, NULL, 4, URLZONEREG_HKCU);
    check(hr == E_INVALIDARG, "SetZoneActionPolicy(NULL) is E_INVALIDARG");
    hr = IInternetZoneManager_SetZoneActionPolicy(zonemgr, 2, ACTION_TEST, (BYTE *)&pol, 4, 99);
    check(hr == E_FAIL, "SetZoneActionPolicy with a bad URLZONEREG fails");
    {
        IInternetZoneManagerEx *ex = NULL;
        pol = 3;
        if (SUCCEEDED(IInternetZoneManager_QueryInterface(zonemgr, &IID_IInternetZoneManagerEx, (void **)&ex)))
        {
            hr = IInternetZoneManagerEx_SetZoneActionPolicyEx(ex, 2, ACTION_TEST, (BYTE *)&pol, 4, URLZONEREG_HKCU, 0);
            got = 0xff;
            IInternetZoneManagerEx_GetZoneActionPolicyEx(ex, 2, ACTION_TEST, (BYTE *)&got, 4, URLZONEREG_HKCU, 0);
            check(hr == S_OK && got == 3, "SetZoneActionPolicyEx stores the policy");
            IInternetZoneManagerEx_Release(ex);
        } else check(0, "IInternetZoneManagerEx available");
    }

    /* custom policies */
    buf = NULL; size = 0;
    hr = IInternetZoneManager_GetZoneCustomPolicy(zonemgr, 2, &guid, &buf, &size, URLZONEREG_DEFAULT);
    check(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "GetZoneCustomPolicy missing is ERROR_NOT_FOUND");
    hr = IInternetZoneManager_SetZoneCustomPolicy(zonemgr, 2, &guid, blob, sizeof(blob), URLZONEREG_HKCU);
    check(hr == S_OK, "SetZoneCustomPolicy succeeds");
    hr = IInternetZoneManager_GetZoneCustomPolicy(zonemgr, 2, &guid, &buf, &size, URLZONEREG_HKCU);
    check(hr == S_OK && size == sizeof(blob) && buf && !memcmp(buf, blob, sizeof(blob)), "GetZoneCustomPolicy returns the blob");
    CoTaskMemFree(buf);
    buf = NULL; size = 0;
    hr = IInternetSecurityManager_QueryCustomPolicy(secmgr, url, &guid, &buf, &size, ctx, 0, 0);
    check(hr == S_OK && size == sizeof(blob) && buf && !memcmp(buf, blob, sizeof(blob)), "QueryCustomPolicy follows the url's zone");
    CoTaskMemFree(buf);
    buf = NULL; size = 0;
    hr = IInternetSecurityManagerEx2_QueryCustomPolicyEx2(secmgr2, uri, &guid, &buf, &size, ctx, 0, 0);
    check(hr == S_OK && size == sizeof(blob) && buf && !memcmp(buf, blob, sizeof(blob)), "QueryCustomPolicyEx2 follows the uri's zone");
    CoTaskMemFree(buf);
    {
        GUID other = guid;
        other.Data1++;
        hr = IInternetSecurityManager_QueryCustomPolicy(secmgr, url, &other, &buf, &size, ctx, 0, 0);
        check(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "QueryCustomPolicy unknown guid is ERROR_NOT_FOUND");
    }

    IUri_Release(uri);
    IInternetSecurityManager_SetZoneMapping(secmgr, 2, L"http://policy.sgzonep.com", SZM_DELETE);
}

static void test_misc(void)
{
    BYTE a[] = "http:host\x03\0\0\0", b[] = "http:host\x03\0\0\0", c[] = "http:host\x02\0\0\0";
    HRESULT hr;

    hr = IInternetZoneManager_LogAction(zonemgr, ACTION_TEST, L"http://x/", L"text", 0);
    check(hr == S_OK, "LogAction succeeds");
    CHECK(CompareSecurityIds(a, 13, b, 13, 0) == S_OK);
    CHECK(CompareSecurityIds(a, 13, c, 13, 0) == S_FALSE);
    CHECK(CompareSecurityIds(a, 13, b, 12, 0) == S_FALSE);
    CHECK(CompareSecurityIds(NULL, 13, b, 13, 0) == E_INVALIDARG);
    CHECK(CompareSecurityIds(a, 13, b, 13, 1) == E_INVALIDARG);
}

int main(void)
{
    HRESULT hr;
    CoInitialize(NULL);
    hr = CoInternetCreateSecurityManager(NULL, &secmgr, 0);
    CHECK(hr == S_OK);
    hr = CoInternetCreateZoneManager(NULL, &zonemgr, 0);
    CHECK(hr == S_OK);
    if (!secmgr || !zonemgr) return 1;
    hr = IInternetSecurityManager_QueryInterface(secmgr, &IID_IInternetSecurityManagerEx2, (void **)&secmgr2);
    CHECK(hr == S_OK && secmgr2);
    if (!secmgr2) return 1;
    test_zone_mappings();
    test_policies();
    test_misc();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
