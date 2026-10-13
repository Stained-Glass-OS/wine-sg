#include <windows.h>
#include <msi.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", #c, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static const WCHAR *P = L"{12345678-1234-1234-1234-123456789012}";
static const WCHAR *SQ = L"87654321432143212143214365870921";

static void setkey(const WCHAR *path, const WCHAR *name, const WCHAR *val)
{
    HKEY k;
    LONG r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &k, NULL);
    CHECK(r == 0, "create %ls %ld", path, r);
    if (r) return;
    if (name) RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (lstrlenW(val) + 1) * sizeof(WCHAR));
    RegCloseKey(k);
}

int main(void)
{
    WCHAR path[300];
    DWORD count = 99;
    WORD date = 0, today;
    SYSTEMTIME st;
    FILETIME ft;
    WORD dtime;
    UINT r;
    INSTALLSTATE s;

    r = MsiGetFeatureUsageW(P, L"F1", &count, &date);
    CHECK(r == ERROR_UNKNOWN_PRODUCT, "no product %u", r);
    r = MsiGetFeatureUsageW(NULL, L"F1", &count, &date);
    CHECK(r == ERROR_INVALID_PARAMETER, "null product %u", r);
    r = MsiGetFeatureUsageW(P, NULL, &count, &date);
    CHECK(r == ERROR_INVALID_PARAMETER, "null feature %u", r);

    wsprintfW(path, L"Software\\Classes\\Installer\\Features\\%s", SQ);
    setkey(path, L"F1", L"Parent");
    wsprintfW(path, L"Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Products\\%s\\Features", SQ);
    setkey(path, L"F1", L"\x02");
    r = MsiGetFeatureUsageW(P, L"F1", &count, &date);
    CHECK(r == ERROR_UNKNOWN_FEATURE, "no usage record %u", r);
    s = MsiQueryFeatureStateW(P, L"F1");
    CHECK(s == INSTALLSTATE_LOCAL, "feature state %d", s);

    CHECK(MsiUseFeatureW(P, L"F1") == INSTALLSTATE_LOCAL, "use 1");
    CHECK(MsiUseFeatureW(P, L"F1") == INSTALLSTATE_LOCAL, "use 2");
    CHECK(MsiUseFeatureExW(P, L"F1", 0, 0) == INSTALLSTATE_LOCAL, "use 3");
    CHECK(MsiUseFeatureExW(P, L"F1", INSTALLMODE_NODETECTION, 0) == INSTALLSTATE_LOCAL, "nodetection use");
    r = MsiGetFeatureUsageW(P, L"F1", &count, &date);
    CHECK(r == ERROR_SUCCESS && count == 3, "count %u r %u", count, r);
    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);
    FileTimeToDosDateTime(&ft, &today, &dtime);
    CHECK(date == today && date != 0, "date %04x today %04x", date, today);
    r = MsiGetFeatureUsageW(P, L"F1", NULL, NULL);
    CHECK(r == ERROR_SUCCESS, "null outs %u", r);
    r = MsiGetFeatureUsageW(P, L"Other", &count, &date);
    CHECK(r == ERROR_UNKNOWN_FEATURE, "other feature %u", r);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
