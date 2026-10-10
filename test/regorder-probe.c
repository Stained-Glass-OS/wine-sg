/* Registry value order and RegGetValue sizes (patches/sg/2248).
 *   probe write  : create keys and values, check this process's view
 *   probe verify : after a wineserver restart, check the order persisted */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void set(HKEY k, const char *name, DWORD v) { RegSetValueExA(k, name, 0, REG_DWORD, (BYTE *)&v, sizeof(v)); }

static void order(HKEY k, char *out, size_t len)
{
    DWORD i;
    out[0] = 0;
    for (i = 0;; i++)
    {
        char name[32]; DWORD n = sizeof(name);
        if (RegEnumValueA(k, i, name, &n, NULL, NULL, NULL, NULL)) break;
        if (strlen(out) + strlen(name) + 2 < len) { strcat(out, name); strcat(out, " "); }
    }
}

static void order_w(HKEY k, char *out, size_t len)
{
    DWORD i;
    out[0] = 0;
    for (i = 0;; i++)
    {
        WCHAR name[32]; DWORD n = 32; char a[32];
        if (RegEnumValueW(k, i, name, &n, NULL, NULL, NULL, NULL)) break;
        WideCharToMultiByte(CP_ACP, 0, name, -1, a, sizeof(a), NULL, NULL);
        if (strlen(out) + strlen(a) + 2 < len) { strcat(out, a); strcat(out, " "); }
    }
}

int main(int argc, char **argv)
{
    HKEY k;
    char buf[128];
    LONG r;

    if (argc > 1 && !strcmp(argv[1], "verify"))
    {
        r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SGOrder\\Persist", 0, KEY_READ, &k);
        CHECK(!r, "open persisted key %ld", r);
        if (!r)
        {
            order(k, buf, sizeof(buf));
            CHECK(!strcmp(buf, "Zed Alpha Mid Beta "), "order after restart: '%s'", buf);
            RegCloseKey(k);
        }
        RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SGOrder\\Persist");
        RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SGOrder");
        printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
        return fails != 0;
    }

    /* enumeration follows creation order, for both Enum flavours */
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SGOrder\\Persist");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SGOrder\\Live");
    r = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SGOrder\\Live", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    CHECK(!r, "create %ld", r);
    set(k, "Zed", 1); set(k, "Alpha", 2); set(k, "Mid", 3); set(k, "Beta", 4);
    order(k, buf, sizeof(buf));  CHECK(!strcmp(buf, "Zed Alpha Mid Beta "), "A order '%s'", buf);
    order_w(k, buf, sizeof(buf)); CHECK(!strcmp(buf, "Zed Alpha Mid Beta "), "W order '%s'", buf);
    set(k, "Mid", 33);             /* overwriting keeps the place */
    order(k, buf, sizeof(buf));  CHECK(!strcmp(buf, "Zed Alpha Mid Beta "), "overwrite order '%s'", buf);
    RegDeleteValueA(k, "Alpha");
    order(k, buf, sizeof(buf));  CHECK(!strcmp(buf, "Zed Mid Beta "), "after delete '%s'", buf);
    set(k, "alpha", 5);            /* re-created: last */
    order(k, buf, sizeof(buf));  CHECK(!strcmp(buf, "Zed Mid Beta alpha "), "after re-create '%s'", buf);
    {
        DWORD v = 0, sz = sizeof(v);
        r = RegQueryValueExA(k, "ALPHA", NULL, NULL, (BYTE *)&v, &sz);   /* lookup still by name, any case */
        CHECK(!r && v == 5, "lookup %ld %lu", r, v);
    }
    RegCloseKey(k);

    /* RegGetValue on a REG_EXPAND_SZ: sizes */
    r = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SGOrder\\Persist", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    CHECK(!r, "create persist %ld", r);
    set(k, "Zed", 1); set(k, "Alpha", 2); set(k, "Mid", 3); set(k, "Beta", 4);
    {
        const char *raw = "%SystemRoot%\\sgsub";   /* 18 chars */
        char out[260], expanded[260];
        DWORD sz, type, want;
        WCHAR rawW[] = L"%SystemRoot%\\sgsub", outW[260];
        RegSetValueExA(k, "Exp", 0, REG_EXPAND_SZ, (const BYTE *)raw, strlen(raw) + 1);
        ExpandEnvironmentStringsA(raw, expanded, sizeof(expanded));
        want = strlen(expanded) + 1;

        sz = sizeof(out); type = 0;
        r = RegGetValueA(k, NULL, "Exp", RRF_RT_REG_SZ, &type, out, &sz);
        CHECK(!r && type == REG_SZ && !strcmp(out, expanded), "A expanded text '%s' (%ld)", out, r);
        CHECK(sz == strlen(raw) + 1 && sz > want, "A expanded size is the stored one (%lu, stored %zu, expanded %lu)", sz, strlen(raw) + 1, want);

        sz = 0;
        r = RegGetValueA(k, NULL, "Exp", RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, NULL, NULL, &sz);
        CHECK(!r && sz == strlen(raw) + 2, "A size-only NOEXPAND %lu (want %zu)", sz, strlen(raw) + 2);

        RegSetValueExW(k, L"ExpW", 0, REG_EXPAND_SZ, (const BYTE *)rawW, sizeof(rawW));
        sz = sizeof(outW); type = 0;
        r = RegGetValueW(k, NULL, L"ExpW", RRF_RT_REG_SZ, &type, outW, &sz);
        CHECK(!r && type == REG_SZ, "W expanded %ld", r);
        CHECK(sz == sizeof(rawW), "W expanded size is the stored one (%lu, stored %zu)", sz, sizeof(rawW));
        sz = 0;
        r = RegGetValueW(k, NULL, L"ExpW", RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, NULL, NULL, &sz);
        CHECK(!r && sz == sizeof(rawW) + sizeof(WCHAR), "W size-only NOEXPAND %lu (want %zu)", sz, sizeof(rawW) + sizeof(WCHAR));

        /* a plain string is unchanged */
        RegSetValueExA(k, "Plain", 0, REG_SZ, (const BYTE *)"abc", 4);
        sz = 0;
        r = RegGetValueA(k, NULL, "Plain", RRF_RT_REG_SZ | RRF_NOEXPAND, NULL, NULL, &sz);
        CHECK(!r && sz == 4, "A plain REG_SZ size %lu", sz);
        RegDeleteValueA(k, "Exp"); RegDeleteValueA(k, "ExpW"); RegDeleteValueA(k, "Plain");
    }
    RegCloseKey(k);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SGOrder\\Live");

    /* an ini file mapping: a named entry, then the default mapping created after it; the
     * section lists the named entry first and the default's values after it */
    {
        HKEY map, named, def;
        const char *base = "Software\\Microsoft\\Windows NT\\CurrentVersion\\IniFileMapping\\sgorder.ini\\sect";
        r = RegCreateKeyExA(HKEY_LOCAL_MACHINE, base, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &map, NULL);
        CHECK(!r, "create mapping %ld", r);
        RegCreateKeyExA(HKEY_CURRENT_USER, "SGMapNamed", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &named, NULL);
        RegSetValueExA(named, "n2", 0, REG_SZ, (const BYTE *)"v2", 3);
        RegCreateKeyExA(HKEY_CURRENT_USER, "SGMapDefault", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &def, NULL);
        RegSetValueExA(def, "n3", 0, REG_SZ, (const BYTE *)"v3", 3);
        RegSetValueExA(map, "n2", 0, REG_SZ, (const BYTE *)"USR:SGMapNamed", sizeof("USR:SGMapNamed"));
        RegSetValueExA(map, NULL, 0, REG_SZ, (const BYTE *)"USR:SGMapDefault", sizeof("USR:SGMapDefault"));
        {
            char sec[128]; DWORD got, j; char shown[128];
            memset(sec, 0xcc, sizeof(sec));
            got = GetPrivateProfileSectionA("sect", sec, sizeof(sec), "sgorder.ini");
            for (j = 0; j < got && j < sizeof(shown) - 1; j++) shown[j] = sec[j] ? sec[j] : '|';
            shown[j] = 0;
            CHECK(got == 12 && !memcmp(sec, "n2=v2\0n3=v3\0", 12), "mapped section '%s' (%lu)", shown, got);
        }
        RegDeleteValueA(def, "n3"); RegDeleteValueA(named, "n2");
        RegCloseKey(def); RegCloseKey(named); RegCloseKey(map);
        RegDeleteKeyA(HKEY_CURRENT_USER, "SGMapDefault"); RegDeleteKeyA(HKEY_CURRENT_USER, "SGMapNamed");
        RegDeleteKeyExA(HKEY_LOCAL_MACHINE, base, KEY_WOW64_64KEY, 0);
        RegDeleteKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\IniFileMapping\\sgorder.ini", KEY_WOW64_64KEY, 0);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
