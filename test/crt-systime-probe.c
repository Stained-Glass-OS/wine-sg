/* Probe for patches/sg/4520: _getsystime and _setsystime of the DLL named on
 * the command line (crtdll, msvcrt, msvcrtd, msvcr70 .. msvcr120, ucrtbase).
 * _getsystime against GetLocalTime; _setsystime only with an invalid date,
 * so the clock is never changed. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

struct crt_tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int yday(int y, int m, int d)
{
    static const int days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return days[m - 1] + d - 1 + (m > 2 && ((!(y % 4) && (y % 100)) || !(y % 400)));
}

int main(int argc, char **argv)
{
    unsigned int (__cdecl *p_get)(struct crt_tm *);
    unsigned int (__cdecl *p_set)(struct crt_tm *, unsigned int);
    struct crt_tm tm;
    SYSTEMTIME before, after;
    TIME_ZONE_INFORMATION tzi;
    unsigned int ms, r, i;
    HMODULE mod;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_get = (void *)GetProcAddress(mod, "_getsystime");
    p_set = (void *)GetProcAddress(mod, "_setsystime");
    check(p_get && p_set, "exports");
    if (!p_get || !p_set) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < 3; i++)
    {
        GetLocalTime(&before);
        memset(&tm, 0xcc, sizeof(tm));
        ms = p_get(&tm);
        GetLocalTime(&after);
        if (before.wSecond != after.wSecond) continue;  /* a second boundary in between: again */
        break;
    }
    check(tm.tm_year == before.wYear - 1900 && tm.tm_mon == before.wMonth - 1 && tm.tm_mday == before.wDay &&
          tm.tm_hour == before.wHour && tm.tm_min == before.wMinute && tm.tm_sec == before.wSecond,
          "_getsystime %04d-%02d-%02d %02d:%02d:%02d (local %04u-%02u-%02u %02u:%02u:%02u)", tm.tm_year + 1900,
          tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, before.wYear, before.wMonth, before.wDay,
          before.wHour, before.wMinute, before.wSecond);
    check(ms >= before.wMilliseconds && ms <= after.wMilliseconds, "milliseconds %u in [%u, %u]", ms,
          before.wMilliseconds, after.wMilliseconds);
    check(tm.tm_wday == before.wDayOfWeek && tm.tm_yday == yday(before.wYear, before.wMonth, before.wDay),
          "week day %d, year day %d", tm.tm_wday, tm.tm_yday);
    check(tm.tm_isdst == (GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_DAYLIGHT), "isdst %d", tm.tm_isdst);

    /* an invalid date: an error code, the clock unchanged */
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 124; tm.tm_mon = 12; tm.tm_mday = 40; tm.tm_hour = 25;
    GetLocalTime(&before);
    r = p_set(&tm, 0);
    GetLocalTime(&after);
    check(r != 0 && after.wYear == before.wYear && after.wMonth == before.wMonth, "_setsystime invalid date: %u", r);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
