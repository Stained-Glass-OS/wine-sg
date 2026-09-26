/* userlocale-probe: what a standard user's program gets from the time-zone
 * and locale APIs, for userlocale-gate.sh. */
#include <windows.h>
#include <stdio.h>

static int cps, locales, groups;
static BOOL CALLBACK cp_cb(LPWSTR s) { (void)s; cps++; return TRUE; }
static BOOL CALLBACK loc_cb(LPWSTR s, DWORD f, LPARAM p) { (void)s; (void)f; (void)p; locales++; return TRUE; }
static BOOL CALLBACK grp_cb(LGRPID id, LPWSTR a, LPWSTR b, DWORD f, LONG_PTR p) { (void)id; (void)a; (void)b; (void)f; (void)p; groups++; return TRUE; }

int main(void)
{
    TIME_ZONE_INFORMATION tz, ty;
    DWORD r = GetTimeZoneInformation(&tz);
    BOOL y = GetTimeZoneInformationForYear(2026, NULL, &ty);
    printf("TZID %s\n", r == TIME_ZONE_ID_INVALID ? "invalid" : "ok");
    printf("FORYEAR %d\n", y);
    printf("STDNAME %ls\n", tz.StandardName);
    EnumSystemCodePagesW(cp_cb, CP_INSTALLED);
    EnumSystemLocalesEx(loc_cb, LOCALE_ALL, 0, NULL);
    EnumSystemLanguageGroupsW(grp_cb, LGRPID_INSTALLED, 0);
    printf("CODEPAGES %d\nLOCALES %d\nGROUPS %d\n", cps, locales, groups);
    fflush(stdout);
    return 0;
}
