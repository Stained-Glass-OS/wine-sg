/* sapiro-probe: SAPI's voice category and tokens are usable from a key the
 * caller may only read (patches/sg/8700). The voices below HKEY_LOCAL_MACHINE
 * are such keys for every standard user; sapi opened them for writing and
 * SpVoice had no voice. The probe makes a category under HKEY_CURRENT_USER and
 * gives it a read-only DACL, so it does not depend on who runs it. */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <sddl.h>
#include <sapi.h>
#include <stdio.h>

#define CAT  L"Software\\SGTest\\sapiro\\Voices"
#define CATID L"HKEY_CURRENT_USER\\" CAT
static int fails;
static void ok(int c, const char *what, HRESULT hr)
{
    printf("%s %s (%08lx)\n", c ? "ok" : "FAIL", what, (unsigned long)hr);
    if (!c) fails++;
}
static void mk(const WCHAR *sub, const WCHAR *name, const WCHAR *val)
{
    HKEY k; WCHAR path[256];
    swprintf(path, 256, L"%ls%ls", CAT, sub);
    RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    if (val) RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (wcslen(val) + 1) * sizeof(WCHAR));
    RegCloseKey(k);
}
static void readonly(const WCHAR *sub)
{
    PSECURITY_DESCRIPTOR sd; HKEY k; WCHAR path[256]; LONG r;
    swprintf(path, 256, L"%ls%ls", CAT, sub);
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;KR;;;WD)", SDDL_REVISION_1, &sd, NULL);
    r = RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, WRITE_DAC, &k);
    if (!r) { RegSetKeySecurity(k, DACL_SECURITY_INFORMATION, sd); RegCloseKey(k); }
    LocalFree(sd);
}

int main(void)
{
    ISpObjectTokenCategory *cat; IEnumSpObjectTokens *en; ISpObjectToken *tok, *tok2;
    ISpDataKey *attrs; WCHAR *s; ULONG n = 0; HRESULT hr; HKEY k; LONG r;

    CoInitialize(NULL);
    mk(L"", L"DefaultDefaultTokenId", CATID L"\\Tokens\\V1");
    mk(L"\\Tokens\\V1", NULL, L"SG Read-only Voice");
    mk(L"\\Tokens\\V1", L"CLSID", L"{C64501F6-E6E6-451F-A150-25D0839BC510}");
    mk(L"\\Tokens\\V1\\Attributes", L"Language", L"409");
    readonly(L"\\Tokens\\V1\\Attributes"); readonly(L"\\Tokens\\V1"); readonly(L"\\Tokens"); readonly(L"");

    r = RegOpenKeyExW(HKEY_CURRENT_USER, CAT, 0, KEY_ALL_ACCESS, &k);
    if (!r) RegCloseKey(k);
    printf("%s the category key refuses writing (%ld)\n", r == ERROR_ACCESS_DENIED ? "ok" : "SETUP", r);
    if (r != ERROR_ACCESS_DENIED) return 2;

    hr = CoCreateInstance(&CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, &IID_ISpObjectTokenCategory, (void **)&cat);
    if (FAILED(hr)) { printf("FAIL no token category (%08lx)\n", hr); return 1; }
    hr = ISpObjectTokenCategory_SetId(cat, CATID, FALSE);
    ok(hr == S_OK, "category SetId on a read-only key", hr);
    hr = ISpObjectTokenCategory_EnumTokens(cat, NULL, NULL, &en);
    ok(hr == S_OK, "EnumTokens", hr);
    if (hr == S_OK) {
        IEnumSpObjectTokens_GetCount(en, &n);
        ok(n == 1, "one voice enumerated", n);
        if (n && IEnumSpObjectTokens_Next(en, 1, &tok, NULL) == S_OK) {
            s = NULL; hr = ISpObjectToken_GetStringValue(tok, NULL, &s);
            ok(hr == S_OK && s && !wcscmp(s, L"SG Read-only Voice"), "the token's name reads", hr);
            hr = ISpObjectToken_OpenKey(tok, L"Attributes", &attrs);
            ok(hr == S_OK, "its Attributes key opens", hr);
            if (hr == S_OK) {
                s = NULL; hr = ISpDataKey_GetStringValue(attrs, L"Language", &s);
                ok(hr == S_OK && s && !wcscmp(s, L"409"), "Language reads 409", hr);
            }
            hr = ISpObjectToken_SetStringValue(tok, L"Changed", L"x");
            ok(FAILED(hr), "and writing to it is still refused", hr);
        }
    }
    hr = CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_ALL, &IID_ISpObjectToken, (void **)&tok2);
    if (SUCCEEDED(hr)) {
        hr = ISpObjectToken_SetId(tok2, NULL, CATID L"\\Tokens\\V1", FALSE);
        ok(hr == S_OK, "token SetId on a read-only key", hr);
    }
    s = NULL; hr = ISpObjectTokenCategory_GetDefaultTokenId(cat, &s);
    ok(hr == S_OK && s && wcsstr(s, L"\\Tokens\\V1"), "the default voice of the category", hr);
    printf("%s\n", fails ? "RESULT FAIL" : "RESULT PASS");
    return fails ? 1 : 0;
}
