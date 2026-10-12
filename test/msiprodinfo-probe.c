/* Probe for patches/sg/2454: MsiGetProductInfoEx's package code (a squashed GUID, else a bad configuration) and package name
 * (kept in the source list). */
#include <windows.h>
#include <msi.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void checku(const char *name, UINT got, UINT want)
{
    printf("      %s  %s (%u, want %u)\n", got == want ? "PASS" : "FAIL", name, got, want);
    if (got != want) fails++;
}
static void checks(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static void rev(char *dst, const char *src, int n) { int i; for (i = 0; i < n; i++) dst[i] = src[n - 1 - i]; }

/* {379B1C47-40C1-42FA-A9BB-BEBB6F1B0172} -> 74C1B9731 0C0AF24A9...: the squashed form */
static void squash(const char *g, char *out)
{
    char h[33];
    int i, n = 0;
    for (i = 0; g[i]; i++) if (g[i] != '{' && g[i] != '}' && g[i] != '-') h[n++] = g[i];
    h[n] = 0;
    rev(out, h, 8); rev(out + 8, h + 8, 4); rev(out + 12, h + 12, 4);
    for (i = 0; i < 8; i++) { out[16 + i * 2] = h[16 + i * 2 + 1]; out[16 + i * 2 + 1] = h[16 + i * 2]; }
    out[32] = 0;
}

int main(void)
{
    static const char guid[] = "{379B1C47-40C1-42FA-A9BB-BEBB6F1B0172}";
    char sq[33], path[200], buf[100];
    DWORD sz;
    HKEY key = NULL, source = NULL;
    UINT r;
    char sid[256] = "";
    HANDLE token;
    DWORD len;
    char userbuf[256];

    /* the current user's SID, as a string */
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, userbuf, sizeof(userbuf), &len);
    {
        BOOL (WINAPI *pConvert)(PSID, char **) = (void *)GetProcAddress(LoadLibraryA("advapi32.dll"), "ConvertSidToStringSidA");
        char *s = NULL;
        if (pConvert && pConvert(((TOKEN_USER *)userbuf)->User.Sid, &s)) { strcpy(sid, s); LocalFree(s); }
    }
    CloseHandle(token);

    squash(guid, sq);
    snprintf(path, sizeof(path), "Software\\Microsoft\\Installer\\Products\\%s", sq);
    RegDeleteTreeA(HKEY_CURRENT_USER, path);
    checku("product key", RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL), 0);

    RegSetValueExA(key, "PackageCode", 0, REG_SZ, (BYTE *)"code", 5);
    sz = 100; strcpy(buf, "apple");
    r = MsiGetProductInfoExA(guid, sid, MSIINSTALLCONTEXT_USERUNMANAGED, "PackageCode", buf, &sz);
    checku("a package code that is no squashed GUID: bad configuration", r, ERROR_BAD_CONFIGURATION);
    checks("the buffer is left alone", !strcmp(buf, "apple") && sz == 100);

    RegSetValueExA(key, "PackageCode", 0, REG_SZ, (BYTE *)sq, 33);
    sz = 100; strcpy(buf, "apple");
    r = MsiGetProductInfoExA(guid, sid, MSIINSTALLCONTEXT_USERUNMANAGED, "PackageCode", buf, &sz);
    checku("a squashed GUID is fine", r, 0);
    checks("and comes back as a GUID", !strcmp(buf, guid) && sz == 38);
    printf("      (got %s)\n", buf);

    RegSetValueExA(key, "PackageName", 0, REG_SZ, (BYTE *)"name", 5);
    sz = 100; strcpy(buf, "apple");
    r = MsiGetProductInfoExA(guid, sid, MSIINSTALLCONTEXT_USERUNMANAGED, "PackageName", buf, &sz);
    checku("PackageName with no source list: unknown product", r, ERROR_UNKNOWN_PRODUCT);
    checks("the buffer is left alone", !strcmp(buf, "apple") && sz == 100);

    RegCreateKeyExA(key, "SourceList", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &source, NULL);
    RegSetValueExA(source, "PackageName", 0, REG_SZ, (BYTE *)"my.msi", 7);
    sz = 100; strcpy(buf, "apple");
    r = MsiGetProductInfoExA(guid, sid, MSIINSTALLCONTEXT_USERUNMANAGED, "PackageName", buf, &sz);
    checku("with a source list", r, 0);
    checks("it is the name there", !strcmp(buf, "my.msi") && sz == 6);

    RegCloseKey(source);
    RegCloseKey(key);
    RegDeleteTreeA(HKEY_CURRENT_USER, path);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
