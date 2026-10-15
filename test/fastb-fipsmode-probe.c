/* BCryptGetFipsAlgorithmMode reads the FIPS algorithm policy (patch 3743) */
#include <windows.h>
#include <stdio.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char vista[] = "System\\CurrentControlSet\\Control\\Lsa\\FipsAlgorithmPolicy";
static const char lsa[] = "System\\CurrentControlSet\\Control\\Lsa";

static LONG (WINAPI *pGet)(BOOLEAN *);

static void set_dword(const char *key, const char *name, DWORD v)
{
    HKEY h;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL)) { CHECK(0, "create %s", key); return; }
    RegSetValueExA(h, name, 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    RegCloseKey(h);
}
static void set_sz(const char *key, const char *name, const char *v)
{
    HKEY h;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL)) return;
    RegSetValueExA(h, name, 0, REG_SZ, (const BYTE *)v, strlen(v) + 1);
    RegCloseKey(h);
}
static void clear(void)
{
    HKEY h;
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, vista);
    if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, lsa, 0, KEY_ALL_ACCESS, &h)) { RegDeleteValueA(h, "FIPSAlgorithmPolicy"); RegCloseKey(h); }
}
static int mode(void)
{
    BOOLEAN e = 0x55;
    LONG st = pGet(&e);
    return st == 0 ? e : -1;
}

int main(void)
{
    HMODULE m = LoadLibraryA("bcrypt.dll");
    BOOLEAN e;

    pGet = (void *)GetProcAddress(m, "BCryptGetFipsAlgorithmMode");
    CHECK(pGet(NULL) == (LONG)0xc000000d, "NULL");
    clear();
    CHECK(mode() == 0, "nothing set: %d", mode());
    set_dword(vista, "Enabled", 1);
    CHECK(mode() == 1, "Vista key enabled: %d", mode());
    e = 0x55; pGet(&e); CHECK(e == 1, "the value is a BOOLEAN of 1: %d", e);
    set_dword(vista, "Enabled", 0);
    CHECK(mode() == 0, "Vista key disabled: %d", mode());
    set_dword(vista, "Enabled", 5);
    CHECK(mode() == 1, "any non-zero value is on: %d", mode());
    clear();
    set_dword(lsa, "FIPSAlgorithmPolicy", 1);
    CHECK(mode() == 1, "XP value: %d", mode());
    set_dword(lsa, "FIPSAlgorithmPolicy", 0);
    CHECK(mode() == 0, "XP value off: %d", mode());
    set_dword(lsa, "FIPSAlgorithmPolicy", 1);
    set_dword(vista, "Enabled", 0);
    CHECK(mode() == 0, "the Vista key wins over the XP value: %d", mode());
    clear();
    set_sz(vista, "Enabled", "1");
    CHECK(mode() == 0, "a string is no policy: %d", mode());
    set_sz(lsa, "FIPSAlgorithmPolicy", "1");
    CHECK(mode() == 0, "a string is no policy (XP): %d", mode());
    /* the Vista key exists but has no value: the XP value counts */
    clear();
    set_dword(lsa, "FIPSAlgorithmPolicy", 1);
    set_dword(vista, "Other", 1);
    CHECK(mode() == 1, "key without Enabled falls back to the XP value: %d", mode());
    clear();
    CHECK(mode() == 0, "cleared again: %d", mode());
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
