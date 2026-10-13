#include <windows.h>
#include <msi.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *prod = "{7E8A5F11-AAAA-4AAA-8AAA-AAAAAAAAAAAA}";
/* the squashed form of the code above */
static const char *key_path = "Software\\Microsoft\\Installer\\Products\\11F5A8E7AAAAAAA4A8AAAAAAAAAAAAAA";

static void list_sources(DWORD type, char out[2048])
{
    char src[MAX_PATH];
    DWORD i, size, ret;

    out[0] = 0;
    for (i = 0;; i++)
    {
        size = sizeof(src);
        ret = MsiSourceListEnumSourcesA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, type, i, src, &size);
        if (ret) break;
        strcat(out, src);
        strcat(out, "|");
    }
}

int main(void)
{
    HKEY key;
    char out[2048], last[MAX_PATH];
    DWORD size;
    UINT r;

    RegCreateKeyExA(HKEY_CURRENT_USER, key_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExA(key, "ProductName", 0, REG_SZ, (const BYTE *)"fastsrc", 8);
    {
        HKEY sl;
        RegCreateKeyExA(key, "SourceList", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sl, NULL);
        RegCloseKey(sl);
    }
    RegCloseKey(key);

    r = MsiSourceListAddSourceExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK, "C:\\one", 0);
    CHECK(r == ERROR_SUCCESS, "add one: %u", r);
    r = MsiSourceListAddSourceExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK, "C:\\two", 0);
    CHECK(r == ERROR_SUCCESS, "add two: %u", r);
    r = MsiSourceListAddSourceExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK, "C:\\three", 0);
    r = MsiSourceListAddSourceExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_URL, "http://x.example/a", 0);
    CHECK(r == ERROR_SUCCESS, "add url: %u", r);

    list_sources(MSISOURCETYPE_NETWORK, out);
    CHECK(!strcmp(out, "C:\\one\\|C:\\two\\|C:\\three\\|"), "net sources: %s", out);

    /* one source goes, the rest stay in order */
    r = MsiSourceListClearSourceA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK, "C:\\two");
    CHECK(r == ERROR_SUCCESS, "clear source: %u", r);
    list_sources(MSISOURCETYPE_NETWORK, out);
    CHECK(!strcmp(out, "C:\\one\\|C:\\three\\|"), "after clearing two: %s", out);
    list_sources(MSISOURCETYPE_URL, out);
    CHECK(!strcmp(out, "http://x.example/a/|"), "url untouched: %s", out);

    /* a source that is not there is not an error */
    r = MsiSourceListClearSourceA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK, "C:\\nothere");
    CHECK(r == ERROR_SUCCESS, "clear a missing source: %u", r);
    SetLastError(0);
    r = MsiSourceListClearSourceA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, 0, "C:\\one");
    CHECK(r == ERROR_INVALID_PARAMETER, "no type: %u", r);
    r = MsiSourceListClearSourceA("{7E8A5F11-BBBB-4BBB-8BBB-BBBBBBBBBBBB}", NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK, "C:\\one");
    CHECK(r == ERROR_UNKNOWN_PRODUCT, "unknown product: %u", r);

    /* force resolution drops the last used source */
    r = MsiSourceListSetInfoA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSICODE_PRODUCT | MSISOURCETYPE_NETWORK, INSTALLPROPERTY_LASTUSEDSOURCE, "n;1;C:\\one\\");
    CHECK(r == ERROR_SUCCESS, "set last used: %u", r);
    size = sizeof(last);
    r = MsiSourceListGetInfoA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, last, &size);
    CHECK(r == ERROR_SUCCESS && last[0], "last used source: %u '%s'", r, last);
    r = MsiSourceListForceResolutionA(prod, NULL, 0);
    CHECK(r == ERROR_SUCCESS, "force resolution: %u", r);
    size = sizeof(last);
    last[0] = 0;
    r = MsiSourceListGetInfoA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSICODE_PRODUCT, INSTALLPROPERTY_LASTUSEDSOURCE, last, &size);
    CHECK(!last[0], "last used source after forcing: %u '%s'", r, last);
    r = MsiSourceListForceResolutionA(prod, NULL, 1);
    CHECK(r == ERROR_INVALID_PARAMETER, "reserved: %u", r);

    /* everything of a type goes */
    r = MsiSourceListClearAllExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK);
    CHECK(r == ERROR_SUCCESS, "clear all net: %u", r);
    list_sources(MSISOURCETYPE_NETWORK, out);
    CHECK(!out[0], "net sources left: %s", out);
    list_sources(MSISOURCETYPE_URL, out);
    CHECK(!strcmp(out, "http://x.example/a/|"), "url still there: %s", out);
    r = MsiSourceListClearAllExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_URL);
    list_sources(MSISOURCETYPE_URL, out);
    CHECK(r == ERROR_SUCCESS && !out[0], "url sources left: %s", out);
    r = MsiSourceListClearAllExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, 0);
    CHECK(r == ERROR_INVALID_PARAMETER, "no type: %u", r);

    /* the old call clears the network sources */
    MsiSourceListAddSourceExA(prod, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, MSISOURCETYPE_NETWORK, "C:\\again", 0);
    r = MsiSourceListClearAllA(prod, NULL, 0);
    list_sources(MSISOURCETYPE_NETWORK, out);
    CHECK(r == ERROR_SUCCESS && !out[0], "MsiSourceListClearAll: %u '%s'", r, out);

    /* a per-user product is not elevated; an unknown one is an error */
    {
        BOOL elevated = TRUE;
        r = MsiIsProductElevatedA(prod, &elevated);
        CHECK(r == ERROR_SUCCESS && !elevated, "per-user product: %u elevated=%d", r, elevated);
        r = MsiIsProductElevatedA("{7E8A5F11-BBBB-4BBB-8BBB-BBBBBBBBBBBB}", &elevated);
        CHECK(r == ERROR_UNKNOWN_PRODUCT, "unknown product: %u", r);
        r = MsiIsProductElevatedA(prod, NULL);
        CHECK(r == ERROR_INVALID_PARAMETER, "NULL result: %u", r);
        r = MsiIsProductElevatedA("junk", &elevated);
        CHECK(r == ERROR_INVALID_PARAMETER, "bad guid: %u", r);
    }

    RegDeleteTreeA(HKEY_CURRENT_USER, key_path);
    printf("checks %d fails %d\n", checks, fails);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
