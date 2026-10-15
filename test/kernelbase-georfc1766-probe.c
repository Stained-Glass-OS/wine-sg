/* Probe for patches/sg/5045: GetGeoInfoW(GEO_RFC1766) gives the RFC 4646 name
 * "derived from the GetGeoInfo parameters language and GeoId": the language's
 * primary part and the nation ("en-US" for 0x409 or 0x09 with the United
 * States, "zh-US" for 0x04: "ignores the locale-specific portion"). It failed
 * with ERROR_CALL_NOT_IMPLEMENTED. GetGeoInfoEx still refuses the type.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { GEOID geo; LANGID lang; const WCHAR *want; } tests[] =
    {
        {244, 0x409, L"en-US"}, {244, 0x09, L"en-US"}, {244, 0x04, L"zh-US"}, {244, 0x809, L"en-US"},
        {94, 0x407, L"de-DE"}, {244, 0x407, L"de-US"}, {84, 0x40c, L"fr-FR"},
    };
    INT (WINAPI *pGetGeoInfoEx)(const WCHAR *, GEOTYPE, WCHAR *, int) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetGeoInfoEx");
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        WCHAR buf[32];
        int ret, size;

        size = GetGeoInfoW(tests[i].geo, GEO_RFC1766, NULL, 0, tests[i].lang);
        memset(buf, 0, sizeof(buf));
        ret = GetGeoInfoW(tests[i].geo, GEO_RFC1766, buf, ARRAYSIZE(buf), tests[i].lang);
        check(ret == lstrlenW(tests[i].want) + 1 && size == ret && !lstrcmpW(buf, tests[i].want), "geo %ld lang %#x: %d \"%ls\" (%lu)",
              tests[i].geo, tests[i].lang, ret, buf, ret ? 0 : GetLastError());
    }
    if (pGetGeoInfoEx)
    {
        WCHAR buf[32];
        SetLastError(0xdeadbeef);
        check(!pGetGeoInfoEx(L"US", GEO_RFC1766, buf, ARRAYSIZE(buf)) && GetLastError() == ERROR_INVALID_FLAGS,
              "GetGeoInfoEx still refuses GEO_RFC1766: %lu", GetLastError());
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
