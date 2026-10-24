/* icuconv-probe.c: the system ICU's converters, icuuc.dll ucnv_* (wine-sg 6084). KEY=VALUE lines.
 * Loads icuuc.dll from system32 by name, as Qt 6 builds against the system ICU import it. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef int UErrorCode;
typedef int8_t UBool;
typedef WCHAR UChar;
typedef struct UConverter UConverter;
typedef struct
{
    uint16_t size;
    UBool flush;
    UConverter *converter;
    const char *source;
    const char *sourceLimit;
    UChar *target;
    const UChar *targetLimit;
    int32_t *offsets;
} ToUArgs;
typedef void (__cdecl *ToUCallback)(const void *, ToUArgs *, const char *, int32_t, int, UErrorCode *);

static UConverter *(__cdecl *p_open)(const char *, UErrorCode *);
static void (__cdecl *p_close)(UConverter *);
static void (__cdecl *p_reset)(UConverter *);
static const char *(__cdecl *p_getName)(const UConverter *, UErrorCode *);
static const char *(__cdecl *p_getStandardName)(const char *, const char *, UErrorCode *);
static int8_t (__cdecl *p_getMaxCharSize)(const UConverter *);
static void (__cdecl *p_toUnicode)(UConverter *, UChar **, const UChar *, const char **, const char *, int32_t *,
                                   UBool, UErrorCode *);
static void (__cdecl *p_fromUnicode)(UConverter *, char **, const char *, const UChar **, const UChar *, int32_t *,
                                     UBool, UErrorCode *);
static int32_t (__cdecl *p_toUCountPending)(const UConverter *, UErrorCode *);
static int32_t (__cdecl *p_countAvailable)(void);
static const char *(__cdecl *p_getAvailableName)(int32_t);
static void (__cdecl *p_setToUCallBack)(UConverter *, ToUCallback, const void *, ToUCallback *, const void **,
                                        UErrorCode *);
static void (__cdecl *p_cbToUWriteUChars)(ToUArgs *, const UChar *, int32_t, int32_t, UErrorCode *);

static void say(const char *key, const char *fmt, ...)
{
    va_list args;
    printf("%s=", key);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

/* UTF-16 as hex code units */
static const char *hexw(const UChar *s, int n)
{
    static char buf[512];
    int i, len = 0;
    buf[0] = 0;
    for (i = 0; i < n && len < (int)sizeof(buf) - 6; i++) len += sprintf(buf + len, "%s%04x", i ? " " : "", s[i]);
    return buf;
}

static const char *hexb(const char *s, int n)
{
    static char buf[512];
    int i, len = 0;
    buf[0] = 0;
    for (i = 0; i < n && len < (int)sizeof(buf) - 4; i++) len += sprintf(buf + len, "%s%02x", i ? " " : "",
                                                                         (unsigned char)s[i]);
    return buf;
}

/* bytes -> UTF-16 in one call */
static void to_u(const char *key, const char *charset, const char *in, int len)
{
    UErrorCode err = 0;
    UConverter *conv = p_open(charset, &err);
    UChar out[64], *t = out;
    const char *s = in;

    if (!conv) { say(key, "open error %d", err); return; }
    p_toUnicode(conv, &t, out + 64, &s, in + len, NULL, 1, &err);
    say(key, "%s err %d", hexw(out, t - out), err);
    p_close(conv);
}

static void from_u(const char *key, const char *charset, const UChar *in, int len)
{
    UErrorCode err = 0;
    UConverter *conv = p_open(charset, &err);
    char out[64], *t = out;
    const UChar *s = in;

    if (!conv) { say(key, "open error %d", err); return; }
    p_fromUnicode(conv, &t, out + 64, &s, in + len, NULL, 1, &err);
    say(key, "%s err %d", hexb(out, t - out), err);
    p_close(conv);
}

static int invalid;
static void __cdecl null_substituter(const void *context, ToUArgs *args, const char *units, int32_t length,
                                     int reason, UErrorCode *err)
{
    if (reason <= 2)
    {
        UChar c = 0;
        *err = 0;
        p_cbToUWriteUChars(args, &c, 1, 0, err);
        invalid += length;
        if (context != (void *)0x1234) invalid += 1000;
    }
}

#define GET(f, n) p_##f = (void *)GetProcAddress(icu, n)

int main(void)
{
    static const UChar naive[] = { 'n', 'a', 0xef, 'v', 'e', ' ', 0x20ac };
    static const UChar hello[] = { 'h', 'i', 0x3042 };
    HMODULE icu = LoadLibraryA("icuuc.dll");
    UErrorCode err = 0;
    UConverter *conv;
    UChar out[64], *t;
    const char *s, *name;
    char path[MAX_PATH];

    say("loaded", "%d", icu != NULL);
    if (!icu) { say("done", "0"); return 1; }
    GetModuleFileNameA(icu, path, sizeof(path));
    say("system32", "%d", !!strstr(_strlwr(path), "system32\\icuuc.dll"));
    GET(open, "ucnv_open"); GET(close, "ucnv_close"); GET(reset, "ucnv_reset"); GET(getName, "ucnv_getName");
    GET(getStandardName, "ucnv_getStandardName"); GET(getMaxCharSize, "ucnv_getMaxCharSize");
    GET(toUnicode, "ucnv_toUnicode"); GET(fromUnicode, "ucnv_fromUnicode");
    GET(toUCountPending, "ucnv_toUCountPending"); GET(countAvailable, "ucnv_countAvailable");
    GET(getAvailableName, "ucnv_getAvailableName"); GET(setToUCallBack, "ucnv_setToUCallBack");
    GET(cbToUWriteUChars, "ucnv_cbToUWriteUChars");
    say("exports", "%d", p_open && p_close && p_reset && p_getName && p_getStandardName && p_getMaxCharSize &&
        p_toUnicode && p_fromUnicode && p_toUCountPending && p_countAvailable && p_getAvailableName &&
        p_setToUCallBack && p_cbToUWriteUChars && GetProcAddress(icu, "UCNV_TO_U_CALLBACK_SUBSTITUTE") &&
        GetProcAddress(icu, "UCNV_FROM_U_CALLBACK_SUBSTITUTE") && GetProcAddress(icu, "ucnv_cbFromUWriteUChars"));

    /* names */
    err = 0;
    conv = p_open("cp1252", &err);
    name = conv ? p_getName(conv, &err) : "(none)";
    say("name_cp1252", "%s", name);
    say("mime_cp1252", "%s", p_getStandardName(name, "MIME", &err));
    say("max_cp1252", "%d", conv ? p_getMaxCharSize(conv) : -1);
    p_close(conv);
    err = 0;
    conv = p_open("utf8", &err);
    say("max_utf8", "%d", conv ? p_getMaxCharSize(conv) : -1);
    p_close(conv);
    err = 0;
    conv = p_open("no-such-charset", &err);
    say("missing", "%d err %d", conv == NULL, err);
    say("available", "%d", p_countAvailable() >= 20 && !strcmp(p_getAvailableName(0), "UTF-8"));

    /* conversions */
    to_u("cp1252_to", "windows-1252", "caf\xe9 \x80", 6);
    from_u("latin1_from", "ISO-8859-1", naive, 7);
    from_u("cp1252_from", "windows-1252", naive, 7);
    to_u("sjis_to", "Shift_JIS", "hi\x82\xa0", 4);
    from_u("sjis_from", "Shift_JIS", hello, 3);
    to_u("badutf8_to", "UTF-8", "\xc3\x28", 2);
    to_u("utf16_bom", "UTF-16", "\xff\xfe" "A\0" "B\0", 6);
    from_u("utf16_from", "UTF-16", hello, 2);
    to_u("koi8_to", "KOI8-R", "\xd0\xd2\xc9\xd7\xc5\xd4", 6);

    /* a character split across calls */
    err = 0;
    conv = p_open("UTF-8", &err);
    t = out;
    s = "x\xe2\x82";
    p_toUnicode(conv, &t, out + 64, &s, s + 3, NULL, 0, &err);
    say("split_first", "%s pending %d err %d", hexw(out, t - out), p_toUCountPending(conv, &err), err);
    s = "\xac" "y";
    p_toUnicode(conv, &t, out + 64, &s, s + 2, NULL, 1, &err);
    say("split_whole", "%s err %d", hexw(out, t - out), err);
    /* the input ending inside a character */
    t = out;
    s = "\xe2\x82";
    p_toUnicode(conv, &t, out + 64, &s, s + 2, NULL, 1, &err);
    say("truncated", "%s err %d", hexw(out, t - out), err);
    p_close(conv);

    /* a small target: the rest comes next call */
    err = 0;
    conv = p_open("ISO-8859-1", &err);
    t = out;
    s = "abcde";
    p_toUnicode(conv, &t, out + 2, &s, s + 5, NULL, 1, &err);
    say("overflow", "%s err %d", hexw(out, t - out), err);
    err = 0;
    p_toUnicode(conv, &t, out + 64, &s, s + strlen(s), NULL, 1, &err);
    say("overflow_rest", "%s err %d", hexw(out, t - out), err);
    p_close(conv);

    /* a program's own callback, as Qt's ConvertInvalidToNull sets one */
    err = 0;
    conv = p_open("UTF-8", &err);
    p_setToUCallBack(conv, null_substituter, (void *)0x1234, NULL, NULL, &err);
    t = out;
    s = "a\xff" "b";
    p_toUnicode(conv, &t, out + 64, &s, s + 3, NULL, 1, &err);
    say("callback", "%s invalid %d err %d", hexw(out, t - out), invalid, err);
    p_close(conv);

    say("done", "1");
    return 0;
}
