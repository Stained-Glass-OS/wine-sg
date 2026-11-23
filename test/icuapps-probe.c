/* icu.dll's string, locale, list, plural, relative-date and field-position functions (patches/sg/9800):
 * what Telegram Desktop (Qt) delay-imports from Windows' icu.dll beyond the .NET list -- u_strToUpper/Lower,
 * u_strFromUTF8/ToUTF8, uloc_toLanguageTag/forLanguageTag, ucol_setStrength, ucal_getDefaultTimeZone,
 * unum_formatDouble(ForFields), unum_set/getTextAttribute, unum_setAttribute, udat_formatForFields,
 * ufieldpositer_*, ulistfmt_*, uplrules_*, ureldatefmt_*. Prints KEY=VALUE lines; UTF-16 results as
 * hex code units. Built 64- and 32-bit (the 32-bit side passes handles and the list's string array). */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef WCHAR UChar;
#define FN(r, n, a) typedef r (__cdecl *n##_fn) a; static n##_fn p_##n;
FN(int32_t, u_strToUpper, (UChar *, int32_t, const UChar *, int32_t, const char *, int *))
FN(int32_t, u_strToLower, (UChar *, int32_t, const UChar *, int32_t, const char *, int *))
FN(UChar *, u_strFromUTF8, (UChar *, int32_t, int32_t *, const char *, int32_t, int *))
FN(char *, u_strToUTF8, (char *, int32_t, int32_t *, const UChar *, int32_t, int *))
FN(int32_t, uloc_toLanguageTag, (const char *, char *, int32_t, signed char, int *))
FN(int32_t, uloc_forLanguageTag, (const char *, char *, int32_t, int32_t *, int *))
FN(void *, ucol_open, (const char *, int *))
FN(void, ucol_setStrength, (void *, int))
FN(int, ucol_strcoll, (void *, const UChar *, int32_t, const UChar *, int32_t))
FN(void, ucol_close, (void *))
FN(int32_t, ucal_getDefaultTimeZone, (UChar *, int32_t, int *))
FN(void *, unum_open, (int, const UChar *, int32_t, const char *, void *, int *))
FN(void, unum_close, (void *))
FN(int32_t, unum_formatDouble, (void *, double, UChar *, int32_t, void *, int *))
FN(int32_t, unum_formatDoubleForFields, (void *, double, UChar *, int32_t, void *, int *))
FN(void, unum_setAttribute, (void *, int, int32_t))
FN(void, unum_setTextAttribute, (void *, int, const UChar *, int32_t, int *))
FN(int32_t, unum_getTextAttribute, (void *, int, UChar *, int32_t, int *))
FN(void *, udat_open, (int, int, const char *, const UChar *, int32_t, const UChar *, int32_t, int *))
FN(void, udat_close, (void *))
FN(int32_t, udat_formatForFields, (void *, double, UChar *, int32_t, void *, int *))
FN(void *, ufieldpositer_open, (int *))
FN(int32_t, ufieldpositer_next, (void *, int32_t *, int32_t *))
FN(void, ufieldpositer_close, (void *))
FN(void *, ulistfmt_open, (const char *, int *))
FN(int32_t, ulistfmt_format, (void *, const UChar *const *, const int32_t *, int32_t, UChar *, int32_t, int *))
FN(void, ulistfmt_close, (void *))
FN(void *, uplrules_openForType, (const char *, int, int *))
FN(int32_t, uplrules_select, (void *, double, UChar *, int32_t, int *))
FN(void, uplrules_close, (void *))
FN(void *, ureldatefmt_open, (const char *, void *, int, int, int *))
FN(int32_t, ureldatefmt_format, (void *, double, int, UChar *, int32_t, int *))
FN(int32_t, ureldatefmt_formatNumeric, (void *, double, int, UChar *, int32_t, int *))
FN(void, ureldatefmt_close, (void *))

static void hex(const char *key, const UChar *s, int32_t len, int err)
{
    int32_t i;
    printf("%s=", key);
    for (i = 0; i < len; i++) printf("%s%x", i ? " " : "", s[i]);
    printf(" err=%d\n", err);
}

/* an ASCII result as text (the list, plural and relative-date results are ASCII in English) */
static void text(const char *key, const UChar *s, int32_t len, int err)
{
    int32_t i;
    printf("%s=", key);
    for (i = 0; i < len; i++) putchar(s[i] < 0x80 ? (char)s[i] : '?');
    printf("|err=%d\n", err);
}

static void word(const UChar *s, int32_t len, int err)
{
    int32_t i;
    for (i = 0; i < len; i++) putchar(s[i] < 0x80 ? (char)s[i] : '?');
    printf(err > 0 ? "!%d " : " ", err);
}

int main(void)
{
    HMODULE icu = LoadLibraryA("icu.dll");
    UChar buf[128];
    char abuf[64];
    int err, missing = 0;
    int32_t len, begin, end, field;
    void *h, *it;

    if (!icu) { printf("load=0\n"); return 1; }
#define GET(n) if (!(p_##n = (n##_fn)GetProcAddress(icu, #n))) { printf("missing=%s\n", #n); missing++; }
    GET(u_strToUpper) GET(u_strToLower) GET(u_strFromUTF8) GET(u_strToUTF8) GET(uloc_toLanguageTag)
    GET(uloc_forLanguageTag) GET(ucol_open) GET(ucol_setStrength) GET(ucol_strcoll) GET(ucol_close)
    GET(ucal_getDefaultTimeZone) GET(unum_open) GET(unum_close) GET(unum_formatDouble)
    GET(unum_formatDoubleForFields) GET(unum_setAttribute) GET(unum_setTextAttribute) GET(unum_getTextAttribute)
    GET(udat_open) GET(udat_close) GET(udat_formatForFields) GET(ufieldpositer_open) GET(ufieldpositer_next)
    GET(ufieldpositer_close) GET(ulistfmt_open) GET(ulistfmt_format) GET(ulistfmt_close) GET(uplrules_openForType)
    GET(uplrules_select) GET(uplrules_close) GET(ureldatefmt_open) GET(ureldatefmt_format)
    GET(ureldatefmt_formatNumeric) GET(ureldatefmt_close)
    printf("load=%d\n", !missing);
    if (missing) return 1;

    {   /* full case mapping: sharp s upper-cases to SS; Turkish lower-cases I to dotless i */
        static const UChar s1[] = { 's', 't', 'r', 'a', 0xdf, 'e', 0 }, s2[] = { 'I', 'X', 0 };
        err = 0; len = p_u_strToUpper(buf, 128, s1, -1, "de", &err); hex("upper", buf, len, err);
        err = 0; len = p_u_strToLower(buf, 128, s2, -1, "tr", &err); hex("lower_tr", buf, len, err);
        err = 0; len = p_u_strToLower(buf, 128, s2, -1, "en", &err); hex("lower_en", buf, len, err);
        err = 0; len = p_u_strToUpper(buf, 2, s1, -1, "de", &err); printf("upper_small=%d err=%d\n", len, err);
    }
    {   /* UTF-8 both ways: e-acute and a non-BMP emoji */
        static const UChar w[] = { 0x20ac, 0xd83d, 0xde00, 0 };
        err = 0; len = -1; p_u_strFromUTF8(buf, 128, &len, "\xc3\xa9\xf0\x9f\x98\x80", -1, &err); hex("from8", buf, len, err);
        err = 0; len = -1; p_u_strToUTF8(abuf, 64, &len, w, -1, &err);
        printf("to8=");
        for (begin = 0; begin < len; begin++) printf("%s%02x", begin ? " " : "", (unsigned char)abuf[begin]);
        printf(" err=%d\n", err);
    }
    err = 0; len = p_uloc_toLanguageTag("en_US", abuf, 64, 1, &err); printf("totag=%s %d err=%d\n", abuf, len, err);
    err = 0; begin = -1; len = p_uloc_forLanguageTag("de-DE", abuf, 64, &begin, &err);
    printf("fortag=%s %d %d err=%d\n", abuf, len, begin, err);
    {   /* collation strength: a and A differ at tertiary strength, not at primary */
        static const UChar a[] = { 'a' }, A[] = { 'A' };
        err = 0; h = p_ucol_open("en", &err);
        begin = p_ucol_strcoll(h, a, 1, A, 1);
        p_ucol_setStrength(h, 0);
        end = p_ucol_strcoll(h, a, 1, A, 1);
        printf("coll=%d %d err=%d\n", begin, end, err);
        p_ucol_close(h);
    }
    err = 0; len = p_ucal_getDefaultTimeZone(buf, 128, &err); text("tz", buf, len, err);
    {   /* numbers: format, fraction digits, a text attribute, field positions */
        static const UChar m[] = { 'm' };
        err = 0; h = p_unum_open(1, NULL, 0, "en_US", NULL, &err);
        len = p_unum_formatDouble(h, 1234.5, buf, 128, NULL, &err); text("num", buf, len, err);
        p_unum_setAttribute(h, 7, 2);
        err = 0; len = p_unum_formatDouble(h, 1234.5, buf, 128, NULL, &err); text("num_frac", buf, len, err);
        err = 0; p_unum_setTextAttribute(h, 2, m, 1, &err);
        len = p_unum_getTextAttribute(h, 2, buf, 128, &err); text("negprefix", buf, len, err);
        err = 0; len = p_unum_formatDouble(h, -3, buf, 128, NULL, &err); text("num_neg", buf, len, err);
        err = 0; it = p_ufieldpositer_open(&err);
        len = p_unum_formatDoubleForFields(h, 1234.5, buf, 128, it, &err); text("numfields", buf, len, err);
        printf("numfield=");
        while ((field = p_ufieldpositer_next(it, &begin, &end)) >= 0) printf("%d:%d-%d ", field, begin, end);
        printf("end\n");
        p_ufieldpositer_close(it);
        p_unum_close(h);
    }
    {   /* a date pattern in UTC: the year field */
        static const UChar utc[] = { 'U', 'T', 'C' }, pat[] = { 'y', 'y', 'y', 'y', '-', 'M', 'M' };
        err = 0; h = p_udat_open(-2, -2, "en_US", utc, 3, pat, 7, &err);
        it = p_ufieldpositer_open(&err);
        len = p_udat_formatForFields(h, 86400000.0 * 40, buf, 128, it, &err); text("date", buf, len, err);
        printf("datefield=");
        while ((field = p_ufieldpositer_next(it, &begin, &end)) >= 0) printf("%d:%d-%d ", field, begin, end);
        printf("end\n");
        p_ufieldpositer_close(it);
        p_udat_close(h);
    }
    {   /* a list of three */
        static const UChar a[] = { 'a', 'p', 'p', 'l', 'e', 's' }, b[] = { 'p', 'e', 'a', 'r', 's' }, c[] = { 'f', 'i', 'g', 's', 0 };
        const UChar *items[3] = { a, b, c };
        int32_t lens[3] = { 6, 5, -1 };
        err = 0; h = p_ulistfmt_open("en", &err);
        len = p_ulistfmt_format(h, items, lens, 3, buf, 128, &err); text("list", buf, len, err);
        err = 0; len = p_ulistfmt_format(h, items, lens, 2, buf, 128, &err); text("list2", buf, len, err);
        p_ulistfmt_close(h);
    }
    {   /* plural categories: English cardinals and ordinals */
        void *o;
        err = 0; h = p_uplrules_openForType("en", 0, &err); o = p_uplrules_openForType("en", 1, &err);
        printf("plural=");
        for (begin = 1; begin <= 3; begin++)
        {
            len = p_uplrules_select(h, begin, buf, 128, &err); buf[len] = 0;
            word(buf, len, err);
        }
        printf("\nordinal=");
        for (begin = 1; begin <= 4; begin++)
        {
            len = p_uplrules_select(o, begin, buf, 128, &err); buf[len] = 0;
            word(buf, len, err);
        }
        printf("\n");
        p_uplrules_close(h); p_uplrules_close(o);
    }
    {   /* relative dates: long style, no capitalization context change */
        err = 0; h = p_ureldatefmt_open("en", NULL, 0, 0x100, &err);
        len = p_ureldatefmt_format(h, -1, 4, buf, 128, &err); text("rel", buf, len, err);
        err = 0; len = p_ureldatefmt_formatNumeric(h, -1, 4, buf, 128, &err); text("relnum", buf, len, err);
        err = 0; len = p_ureldatefmt_format(h, 3, 5, buf, 128, &err); text("relhours", buf, len, err);
        p_ureldatefmt_close(h);
    }
    printf("done=1\n");
    return 0;
}
