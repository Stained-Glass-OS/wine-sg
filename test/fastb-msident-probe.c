/* msident: identities from HKCU\Identities, enumeration, logon/logoff */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *OBJ;
#define SLOT(o, n) ((*(void ***)(o))[n])
#define CALL(o, n, ret, ...) ((ret (WINAPI *)())SLOT(o, n))(o, ##__VA_ARGS__)

static const GUID CLSID_Mgr = {0xa9ae6c91,0x1d1b,0x11d2,{0xb2,0x1a,0x00,0xc0,0x4f,0xa3,0x57,0xfa}};
static const GUID IID_Mgr = {0xa9ae6c90,0x1d1b,0x11d2,{0xb2,0x1a,0x00,0xc0,0x4f,0xa3,0x57,0xfa}};

#define E_IDENTITY_NOT_FOUND_ ((HRESULT)0x80007114)
#define E_NO_CURRENT_IDENTITY_ ((HRESULT)0x80007111)
#define E_USER_CANCELLED_ ((HRESULT)0x80007112)

/* IUserIdentityManager: EnumIdentities 3, ManageIdentities 4, Logon 5, Logoff 6, GetIdentityByCookie 7
 * IEnumUserIdentity: Next 3, Skip 4, Reset 5, Clone 6, GetCount 7
 * IUserIdentity: GetCookie 3, GetName 4, OpenIdentityRegKey 5, GetIdentityFolder 6 */

static void check_name(OBJ ident, const WCHAR *expected)
{
    WCHAR name[64];
    HRESULT hr = CALL(ident, 4, HRESULT, name, 64);
    CHECK(hr == S_OK && !wcscmp(name, expected), "name %ls", name);
    hr = CALL(ident, 4, HRESULT, name, 3);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "small buffer %08lx", hr);
}

int main(void)
{
    OBJ mgr = NULL, en = NULL, en2 = NULL, id = NULL, id2 = NULL;
    GUID cookie, cookie2, none = {1, 2, 3, {4}};
    ULONG count, fetched;
    HRESULT hr;
    HKEY key;
    WCHAR path[MAX_PATH];
    OBJ list[4];

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_Mgr, NULL, CLSCTX_ALL, &IID_Mgr, &mgr);
    CHECK(hr == S_OK, "create %08lx", hr);
    if (hr != S_OK) goto end;

    /* nothing registered: the one default identity */
    hr = CALL(mgr, 3, HRESULT, &en);
    CHECK(hr == S_OK && en, "EnumIdentities %08lx", hr);
    hr = CALL(en, 7, HRESULT, &count);
    CHECK(hr == S_OK && count == 1, "count %lu", count);
    hr = CALL(en, 3, HRESULT, 2, list, &fetched);
    CHECK(hr == S_FALSE && fetched == 1, "Next %08lx %lu", hr, fetched);
    if (fetched)
    {
        check_name(list[0], L"Main Identity");
        hr = CALL(list[0], 3, HRESULT, &cookie);
        CHECK(hr == S_OK && !IsEqualGUID(&cookie, &GUID_NULL), "cookie");
        hr = CALL(list[0], 6, HRESULT, 1, path, MAX_PATH);
        CHECK(hr == S_OK && wcsstr(path, L"\\Identities\\{"), "roaming folder %ls", path);
        hr = CALL(list[0], 6, HRESULT, 2, path, MAX_PATH);
        CHECK(hr == S_OK && wcsstr(path, L"\\Identities\\{"), "local folder %ls", path);
        hr = CALL(list[0], 6, HRESULT, 7, path, MAX_PATH);
        CHECK(hr == E_INVALIDARG, "bad folder flag %08lx", hr);
        hr = CALL(list[0], 5, HRESULT, KEY_READ | KEY_WRITE, &key);
        CHECK(hr == S_OK && key, "OpenIdentityRegKey %08lx", hr);
        if (key) RegCloseKey(key);
        CALL(list[0], 2, ULONG);
    }
    hr = CALL(en, 3, HRESULT, 1, list, &fetched);
    CHECK(hr == S_FALSE && fetched == 0, "exhausted");
    CALL(en, 5, HRESULT);
    hr = CALL(en, 4, HRESULT, 2);
    CHECK(hr == S_FALSE, "Skip past the end %08lx", hr);
    CALL(en, 2, ULONG);

    /* logon / logoff */
    hr = CALL(mgr, 6, HRESULT, NULL);
    CHECK(hr == E_NO_CURRENT_IDENTITY_, "Logoff without logon %08lx", hr);
    hr = CALL(mgr, 5, HRESULT, NULL, 0x80000001u, &id);
    CHECK(hr == E_USER_CANCELLED_ && !id, "forced UI %08lx", hr);
    hr = CALL(mgr, 5, HRESULT, NULL, 0, &id);
    CHECK(hr == S_OK && id, "Logon %08lx", hr);
    if (id)
    {
        CALL(id, 3, HRESULT, &cookie2);
        CHECK(IsEqualGUID(&cookie, &cookie2), "Logon returns the identity");
        CALL(id, 2, ULONG);
    }
    hr = CALL(mgr, 6, HRESULT, NULL);
    CHECK(hr == S_OK, "Logoff %08lx", hr);

    hr = CALL(mgr, 7, HRESULT, &cookie, &id);
    CHECK(hr == S_OK && id, "GetIdentityByCookie %08lx", hr);
    if (id) CALL(id, 2, ULONG);
    hr = CALL(mgr, 7, HRESULT, &none, &id2);
    CHECK(hr == E_IDENTITY_NOT_FOUND_ && !id2, "unknown cookie %08lx", hr);

    /* identities in the registry replace the default one */
    {
        GUID g1 = {0x11111111,0x1111,0x1111,{1,1,1,1,1,1,1,1}}, g2 = {0x22222222,0x2222,0x2222,{2,2,2,2,2,2,2,2}};
        WCHAR k[80], guid[40];
        HKEY sub;

        StringFromGUID2(&g1, guid, 40);
        wcscpy(k, L"Identities\\"); wcscat(k, guid);
        RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sub, NULL);
        RegSetValueExW(sub, L"Username", 0, REG_SZ, (BYTE *)L"Alice", 12);
        RegCloseKey(sub);
        StringFromGUID2(&g2, guid, 40);
        wcscpy(k, L"Identities\\"); wcscat(k, guid);
        RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sub, NULL);
        RegSetValueExW(sub, L"Username", 0, REG_SZ, (BYTE *)L"Bob", 8);
        RegCloseKey(sub);

        CALL(mgr, 3, HRESULT, &en);
        CALL(en, 7, HRESULT, &count);
        CHECK(count == 2, "two registered identities: %lu", count);
        hr = CALL(en, 6, HRESULT, &en2);
        CHECK(hr == S_OK && en2, "Clone");
        hr = CALL(en, 3, HRESULT, 1, list, &fetched);
        CHECK(hr == S_OK && fetched == 1, "first");
        if (fetched) { check_name(list[0], L"Alice"); CALL(list[0], 2, ULONG); }
        hr = CALL(en2, 3, HRESULT, 2, list, &fetched);
        CHECK(hr == S_OK && fetched == 2, "clone has its own position");
        if (fetched == 2) { check_name(list[1], L"Bob"); CALL(list[0], 2, ULONG); CALL(list[1], 2, ULONG); }
        CALL(en2, 2, ULONG);
        CALL(en, 2, ULONG);
        hr = CALL(mgr, 7, HRESULT, &g2, &id);
        CHECK(hr == S_OK && id, "GetIdentityByCookie registered");
        if (id) { check_name(id, L"Bob"); CALL(id, 2, ULONG); }
        hr = CALL(mgr, 7, HRESULT, &cookie, &id);
        CHECK(hr == E_IDENTITY_NOT_FOUND_, "the default identity is gone once others exist");
    }
    CALL(mgr, 2, ULONG);
end:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
