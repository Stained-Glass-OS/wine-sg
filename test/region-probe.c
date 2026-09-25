/* region-probe: prints the user's regional format as programs see it.
 *
 *   region-probe          LOCALE <name>  SHORTDATE <fmt>  DECIMAL <sep>  GEO <id>
 *
 * Used by test/region-gate.sh (patch 0168). */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    WCHAR name[LOCALE_NAME_MAX_LENGTH] = L"", fmt[80] = L"", dec[8] = L"";
    SYSTEMTIME st = { 2026, 9, 5, 25, 0, 0, 0, 0 };
    WCHAR date[80] = L"";

    GetUserDefaultLocaleName(name, ARRAYSIZE(name));
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE, fmt, ARRAYSIZE(fmt));
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, dec, ARRAYSIZE(dec));
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, ARRAYSIZE(date), NULL);
    printf("LOCALE %ls\nLCID %04lx\nSHORTDATE %ls\nDATE %ls\nDECIMAL %ls\nGEO %ld\n", name,
           (unsigned long)GetUserDefaultLCID(), fmt, date, dec, (long)GetUserGeoID(GEOCLASS_NATION));
    return 0;
}
