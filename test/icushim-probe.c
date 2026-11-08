/* test/icushim-gate.sh probe: icu.dll, loaded the way .NET's System.Globalization.Native loads it
 * on Windows (LoadLibraryEx from System32), has every function on .NET's list (its
 * FOR_ALL_ICU_FUNCTIONS, .NET 5 through 10: it aborts on the first missing one), unversioned, and
 * they work: ICU data, collation, sort keys, locales, time zones, dates, normalization, IDNA. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *names[] =
{
    "ubrk_close",
    "ubrk_openRules",
    "ucal_add",
    "ucal_close",
    "ucal_get",
    "ucal_getAttribute",
    "ucal_getKeywordValuesForLocale",
    "ucal_getLimit",
    "ucal_getNow",
    "ucal_getTimeZoneDisplayName",
    "ucal_getTimeZoneIDForWindowsID",
    "ucal_getWindowsTimeZoneID",
    "ucal_open",
    "ucal_openTimeZoneIDEnumeration",
    "ucal_set",
    "ucal_setMillis",
    "u_charsToUChars",
    "ucol_clone",
    "ucol_close",
    "ucol_closeElements",
    "ucol_getOffset",
    "ucol_getRules",
    "ucol_getSortKey",
    "ucol_getStrength",
    "ucol_getVersion",
    "ucol_next",
    "ucol_open",
    "ucol_openElements",
    "ucol_openRules",
    "ucol_previous",
    "ucol_safeClone",
    "ucol_setAttribute",
    "ucol_setMaxVariable",
    "ucol_setVariableTop",
    "ucol_strcoll",
    "ucurr_forLocale",
    "ucurr_getName",
    "udat_close",
    "udat_countSymbols",
    "udat_format",
    "udat_getSymbols",
    "udat_open",
    "udatpg_close",
    "udatpg_getBestPattern",
    "udatpg_open",
    "udat_setCalendar",
    "udat_toPattern",
    "uenum_close",
    "uenum_count",
    "uenum_next",
    "u_getVersion",
    "uidna_close",
    "uidna_nameToASCII",
    "uidna_nameToUnicode",
    "uidna_openUTS46",
    "uldn_close",
    "uldn_keyValueDisplayName",
    "uldn_open",
    "uloc_canonicalize",
    "uloc_countAvailable",
    "ulocdata_getCLDRVersion",
    "ulocdata_getMeasurementSystem",
    "uloc_getAvailable",
    "uloc_getBaseName",
    "uloc_getCharacterOrientation",
    "uloc_getCountry",
    "uloc_getDefault",
    "uloc_getDisplayCountry",
    "uloc_getDisplayLanguage",
    "uloc_getDisplayName",
    "uloc_getISO3Country",
    "uloc_getISO3Language",
    "uloc_getKeywordValue",
    "uloc_getLanguage",
    "uloc_getLCID",
    "uloc_getName",
    "uloc_getParent",
    "uloc_setKeywordValue",
    "unorm2_getNFCInstance",
    "unorm2_getNFDInstance",
    "unorm2_getNFKCInstance",
    "unorm2_getNFKDInstance",
    "unorm2_isNormalized",
    "unorm2_normalize",
    "unum_close",
    "unum_getAttribute",
    "unum_getSymbol",
    "unum_open",
    "unum_toPattern",
    "ures_close",
    "ures_getByKey",
    "ures_getSize",
    "ures_getStringByIndex",
    "ures_open",
    "usearch_close",
    "usearch_first",
    "usearch_getBreakIterator",
    "usearch_getMatchedLength",
    "usearch_last",
    "usearch_openFromCollator",
    "usearch_setPattern",
    "usearch_setText",
    "u_strcmp",
    "u_strcpy",
    "u_strlen",
    "u_strncpy",
    "u_tolower",
    "u_toupper",
    "u_uastrncpy",
};

typedef int (__cdecl *fn)();
static HMODULE icu;
static void *get(const char *n) { return (void *)GetProcAddress(icu, n); }
static int failures;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

int main(void)
{
    unsigned char ver[4] = {0};
    int err = 0, i, missing = 0, n;
    char msg[300];
    WCHAR buf[256], a[] = L"a", B[] = L"B", out[64];
    void *coll, *en, *cal, *fmt, *nfd, *idna;
    BYTE key[64];

    setvbuf(stdout, NULL, _IONBF, 0);
    icu = LoadLibraryExW(L"icu.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    check(icu != NULL, "icu.dll loads from System32");
    if (!icu) { printf("RESULT: FAIL\n"); return 0; }
    for (i = 0; i < ARRAYSIZE(names); i++)
        if (!get(names[i])) { printf("missing %s\n", names[i]); missing++; }
    sprintf(msg, "all %d functions .NET binds are there (%d missing)", (int)ARRAYSIZE(names), missing);
    check(!missing, msg);
    if (missing) { printf("RESULT: FAIL\n"); return 0; }

    ((void (__cdecl *)(unsigned char *, int *))get("ulocdata_getCLDRVersion"))(ver, &err);
    sprintf(msg, "ICU data: CLDR %d.%d (status %d)", ver[0], ver[1], err);
    check(err <= 0 && ver[0] >= 30, msg);
    ((void (__cdecl *)(unsigned char *))get("u_getVersion"))(ver);
    sprintf(msg, "u_getVersion %d.%d", ver[0], ver[1]);
    check(ver[0] >= 50, msg);

    err = 0;
    coll = ((void *(__cdecl *)(const char *, int *))get("ucol_open"))("en", &err);
    check(coll && err <= 0, "ucol_open(en)");
    n = ((int (__cdecl *)(void *, const WCHAR *, int, const WCHAR *, int))get("ucol_strcoll"))(coll, a, 1, B, 1);
    check(n == -1, "ucol_strcoll: a before B");
    n = ((int (__cdecl *)(void *, const WCHAR *, int, BYTE *, int))get("ucol_getSortKey"))(coll, a, 1, key, sizeof(key));
    check(n > 1, "ucol_getSortKey");
    {
        int len = -1;
        const WCHAR *r = ((const WCHAR *(__cdecl *)(void *, int *))get("ucol_getRules"))(coll, &len);
        check(r != NULL && len >= 0, "ucol_getRules");
    }
    err = 0;
    {
        void *clone = ((void *(__cdecl *)(void *, void *, int *, int *))get("ucol_safeClone"))(coll, NULL, NULL, &err);
        check(clone && err <= 0, "ucol_safeClone");
        if (clone) ((void (__cdecl *)(void *))get("ucol_close"))(clone);
    }
    ((void (__cdecl *)(void *))get("ucol_close"))(coll);

    n = ((int (__cdecl *)(void))get("uloc_countAvailable"))();
    sprintf(msg, "uloc_countAvailable %d", n);
    check(n > 100, msg);
    {
        const char *l = ((const char *(__cdecl *)(int))get("uloc_getAvailable"))(0);
        check(l && *l, "uloc_getAvailable(0)");
    }
    err = 0;
    n = ((int (__cdecl *)(const WCHAR *, int, const char *, WCHAR *, int, int *))get("ucal_getTimeZoneIDForWindowsID"))
        (L"Pacific Standard Time", -1, "US", buf, 256, &err);
    sprintf(msg, "Pacific Standard Time is %ls", n > 0 ? buf : L"?");
    check(n > 0 && !wcscmp(buf, L"America/Los_Angeles"), msg);
    err = 0;
    en = ((void *(__cdecl *)(int, const char *, const int *, int *))get("ucal_openTimeZoneIDEnumeration"))(0, NULL, NULL, &err);
    n = en ? ((int (__cdecl *)(void *, int *))get("uenum_count"))(en, &err) : 0;
    {
        int len = 0;
        const char *z = en ? ((const char *(__cdecl *)(void *, int *, int *))get("uenum_next"))(en, &len, &err) : NULL;
        sprintf(msg, "time zones: %d, the first %s", n, z ? z : "?");
        check(n > 300 && z && (int)strlen(z) == len, msg);
    }
    if (en) ((void (__cdecl *)(void *))get("uenum_close"))(en);

    err = 0;
    cal = ((void *(__cdecl *)(const WCHAR *, int, const char *, int, int *))get("ucal_open"))(L"UTC", -1, "en_US", 1, &err);
    check(cal && err <= 0, "ucal_open");
    if (cal)
    {
        double now = ((double (__cdecl *)(void))get("ucal_getNow"))();
        check(now > 1.7e12, "ucal_getNow");
        ((void (__cdecl *)(void *, double, int *))get("ucal_setMillis"))(cal, 0.0, &err);
        n = ((int (__cdecl *)(void *, int, int *))get("ucal_get"))(cal, 1 /* UCAL_YEAR */, &err);
        sprintf(msg, "ucal_setMillis(0) is year %d", n);
        check(n == 1970, msg);
        ((void (__cdecl *)(void *))get("ucal_close"))(cal);
    }
    err = 0;
    fmt = ((void *(__cdecl *)(int, int, const char *, const WCHAR *, int, const WCHAR *, int, int *))get("udat_open"))
        (-2, -2, "en_US", L"UTC", -1, L"yyyy-MM-dd", -1, &err);
    if (fmt)
    {
        n = ((int (__cdecl *)(void *, double, WCHAR *, int, void *, int *))get("udat_format"))(fmt, 86400000.0, out, 64, NULL, &err);
        sprintf(msg, "udat_format: %ls", n > 0 ? out : L"?");
        check(n == 10 && !wcscmp(out, L"1970-01-02"), msg);
        ((void (__cdecl *)(void *))get("udat_close"))(fmt);
    }
    else check(0, "udat_open");
    err = 0;
    nfd = ((void *(__cdecl *)(int *))get("unorm2_getNFDInstance"))(&err);
    n = nfd ? ((int (__cdecl *)(void *, const WCHAR *, int, WCHAR *, int, int *))get("unorm2_normalize"))(nfd, L"\x00e9", 1, out, 64, &err) : 0;
    check(n == 2 && out[0] == 'e' && out[1] == 0x301, "NFD of e acute: e and the combining acute");
    err = 0;
    idna = ((void *(__cdecl *)(unsigned int, int *))get("uidna_openUTS46"))(0, &err);
    if (idna)
    {
        struct { short size; char a, b; unsigned int errors; int r1, r2; } info = { sizeof(info) };
        n = ((int (__cdecl *)(void *, const WCHAR *, int, WCHAR *, int, void *, int *))get("uidna_nameToASCII"))
            (idna, L"b\x00fc" L"cher.de", -1, out, 64, &info, &err);
        if (n >= 0 && n < 64) out[n] = 0;
        sprintf(msg, "IDNA: %ls", n > 0 ? out : L"?");
        check(n > 0 && !wcscmp(out, L"xn--bcher-kva.de"), msg);
        ((void (__cdecl *)(void *))get("uidna_close"))(idna);
    }
    else check(0, "uidna_openUTS46");
    {
        WCHAR s[8];
        ((WCHAR *(__cdecl *)(WCHAR *, const char *, int))get("u_uastrncpy"))(s, "abc", 8);
        check(((int (__cdecl *)(const WCHAR *))get("u_strlen"))(s) == 3 && s[2] == 'c', "u_uastrncpy, u_strlen");
        check(((int (__cdecl *)(int))get("u_toupper"))('q') == 'Q', "u_toupper");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return 0;
}
