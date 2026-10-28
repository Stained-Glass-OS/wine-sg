/* icu.dll UText and regular expressions (patches/sg/6179): a UText provider of the program's that
 * gives its text one row (chunk) at a time, as Windows Terminal's buffer does, searched with
 * uregex_open/setUText/findNext/start64/end64; ICU's common syntax and flags; uregex_setText; errors.
 * The UText layout and the API per ICU's public headers (utext.h, uregex.h). */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef WCHAR UChar;
typedef int UErrorCode;
typedef signed char UBool;
typedef struct UText UText;
typedef struct
{
    int32_t tableSize, r1, r2, r3;
    void *clone;
    int64_t (__cdecl *nativeLength)(UText *);
    UBool (__cdecl *access)(UText *, int64_t, UBool);
    void *extract, *replace, *copy, *mapOffsetToNative, *mapNativeIndexToUTF16;
    void (__cdecl *close)(UText *);
    void *s1, *s2, *s3;
} UTextFuncs;
struct UText
{
    uint32_t magic; int32_t flags, providerProperties, sizeOfStruct;
    int64_t chunkNativeLimit; int32_t extraSize, nativeIndexingLimit;
    int64_t chunkNativeStart; int32_t chunkOffset, chunkLength;
    const UChar *chunkContents; const UTextFuncs *pFuncs; void *pExtra; const void *context;
    const void *p, *q, *r; void *privP; int64_t a; int32_t b, c; int64_t privA; int32_t privB, privC;
};

static UText *(__cdecl *p_utext_setup)(UText *, int32_t, UErrorCode *);
static UText *(__cdecl *p_utext_close)(UText *);
static void *(__cdecl *p_uregex_open)(const UChar *, int32_t, uint32_t, void *, UErrorCode *);
static void (__cdecl *p_uregex_close)(void *);
static void (__cdecl *p_uregex_setUText)(void *, UText *, UErrorCode *);
static void (__cdecl *p_uregex_setText)(void *, const UChar *, int32_t, UErrorCode *);
static UBool (__cdecl *p_uregex_find)(void *, int32_t, UErrorCode *);
static UBool (__cdecl *p_uregex_findNext)(void *, UErrorCode *);
static int64_t (__cdecl *p_uregex_start64)(void *, int32_t, UErrorCode *);
static int64_t (__cdecl *p_uregex_end64)(void *, int32_t, UErrorCode *);
static void (__cdecl *p_uregex_setTimeLimit)(void *, int32_t, UErrorCode *);
static void (__cdecl *p_uregex_setStackLimit)(void *, int32_t, UErrorCode *);

/* three rows of 20 code units each, given one row per access */
static const WCHAR *rows[3] = { L"see http://a.b/c?d=1", L"  and FTP stuff xyz ", L"https://x.org/y.  ok" };
static int closed;

static int64_t __cdecl rows_length(UText *ut) { return 60; }
static UBool __cdecl rows_access(UText *ut, int64_t index, UBool forward)
{
    int row;
    if (!forward) index--;
    if (index < 0 || index >= 60) return FALSE;
    row = (int)(index / 20);
    ut->chunkContents = rows[row];
    ut->chunkNativeStart = row * 20;
    ut->chunkNativeLimit = row * 20 + 20;
    ut->chunkLength = ut->nativeIndexingLimit = 20;
    ut->chunkOffset = (int32_t)(index - row * 20) + (forward ? 0 : 1);
    return TRUE;
}
static void __cdecl rows_close(UText *ut) { closed++; }
static const UTextFuncs rows_funcs = { sizeof(UTextFuncs), 0, 0, 0, NULL, rows_length, rows_access, NULL, NULL, NULL, NULL, NULL, rows_close };

/* all matches of a pattern as start-end pairs */
static void all(const char *name, const WCHAR *pattern, uint32_t flags, UText *ut)
{
    UErrorCode status = 0;
    void *re = p_uregex_open(pattern, -1, flags, NULL, &status);
    char out[256] = "";
    int n = 0;

    if (!re) { printf("%s=error %x\n", name, status); return; }
    p_uregex_setTimeLimit(re, 4096, &status);
    p_uregex_setStackLimit(re, 4 << 20, &status);
    p_uregex_setUText(re, ut, &status);
    while (p_uregex_findNext(re, &status) && n++ < 10)
        sprintf(out + strlen(out), "%s%d-%d", *out ? " " : "", (int)p_uregex_start64(re, 0, &status), (int)p_uregex_end64(re, 0, &status));
    printf("%s=%s,%x\n", name, *out ? out : "none", status);
    p_uregex_close(re);
}

static void str(const char *name, const WCHAR *pattern, uint32_t flags, const WCHAR *text)
{
    UErrorCode status = 0;
    void *re = p_uregex_open(pattern, -1, flags, NULL, &status);
    char out[256] = "";

    if (!re) { printf("%s=error %x\n", name, status); return; }
    p_uregex_setText(re, text, -1, &status);
    while (p_uregex_findNext(re, &status) && strlen(out) < 200)
    {
        int g, groups = 0;
        sprintf(out + strlen(out), "%s%d-%d", *out ? " " : "", (int)p_uregex_start64(re, 0, &status), (int)p_uregex_end64(re, 0, &status));
        for (g = 1; g <= 2; g++)
        {
            UErrorCode s2 = 0;
            int64_t st = p_uregex_start64(re, g, &s2);
            if (s2) break;
            sprintf(out + strlen(out), "/%d", (int)st);
            groups++;
        }
    }
    printf("%s=%s,%x\n", name, *out ? out : "none", status);
    p_uregex_close(re);
}

int main(void)
{
    HMODULE icu = LoadLibraryA("icu.dll");
    UText stack_ut, *ut;
    UErrorCode status = 0;
    void *re;

    setvbuf(stdout, NULL, _IONBF, 0);
#define GET(f) p_##f = (void *)GetProcAddress(icu, #f)
    GET(utext_setup); GET(utext_close); GET(uregex_open); GET(uregex_close); GET(uregex_setUText); GET(uregex_setText);
    GET(uregex_find); GET(uregex_findNext); GET(uregex_start64); GET(uregex_end64); GET(uregex_setTimeLimit); GET(uregex_setStackLimit);
    printf("load=%d\n", icu && p_utext_setup && p_utext_close && p_uregex_open && p_uregex_close && p_uregex_setUText &&
           p_uregex_setText && p_uregex_find && p_uregex_findNext && p_uregex_start64 && p_uregex_end64 && p_uregex_setTimeLimit &&
           p_uregex_setStackLimit);
    if (!p_utext_setup || !p_uregex_open) return 1;

    /* a UText of the program's (on the stack, as UTEXT_INITIALIZER makes it): ICU's is 144 bytes on
     * 64-bit and nothing past it is written (Windows Terminal's stack cookie sits there) */
    {
        struct { UText ut; unsigned char guard[64]; } guarded;
        int i, clean = 1;
        memset(&guarded, 0xcc, sizeof(guarded));
        memset(&guarded.ut, 0, sizeof(guarded.ut));
        guarded.ut.magic = 0x345ad82c;
        guarded.ut.sizeOfStruct = sizeof(guarded.ut);
        status = 0;
        p_utext_setup(&guarded.ut, 0, &status);
        p_utext_close(&guarded.ut);
        for (i = 0; i < 64; i++) if (guarded.guard[i] != 0xcc) clean = 0;
        printf("guard=%d,%d\n", (int)sizeof(UText), clean);
    }
    memset(&stack_ut, 0, sizeof(stack_ut));
    stack_ut.magic = 0x345ad82c;
    stack_ut.sizeOfStruct = sizeof(stack_ut);
    status = 0;
    ut = p_utext_setup(&stack_ut, 0, &status);
    printf("setup=%d,%x,%x\n", ut == &stack_ut, status, stack_ut.flags);
    ut->pFuncs = &rows_funcs;
    ut->providerProperties = 0;

    /* Windows Terminal's URL pattern, across rows */
    all("urls", L"(\\b(https?|ftp|file)://[-A-Za-z0-9+&@#/%?=~_|$!:,.;]*[A-Za-z0-9+&@#/%=~_|$])", 0, ut);
    all("literal", L"ftp", 16 /* LITERAL */, ut);
    all("nocase", L"ftp", 16 | 2 /* LITERAL | CASE_INSENSITIVE */, ut);
    all("words", L"\\b[a-z]{3}\\b", 0, ut);
    all("bol", L"^ *a", 8 /* MULTILINE */, ut);

    /* strings */
    str("alt", L"cat|dog|bird", 0, L"a dog, a cat");
    str("groups", L"(a+)(b*)c", 0, L"xaabcyac");
    str("lazy", L"<.+?>", 0, L"<a><bb>");
    str("greedy", L"<.+>", 0, L"<a><bb>");
    str("interval", L"\\d{2,3}", 0, L"1 12 1234 98765");
    str("negclass", L"[^\\s,]+", 0, L"ab, c d");
    str("dot", L"a.b", 0, L"a\nb axb");
    str("dotall", L"a.b", 32, L"a\nb axb");
    str("eol", L"x$", 0, L"x\nx");
    str("eolm", L"x$", 8, L"x\nx");
    str("empty", L"z*", 0, L"ab");
    str("escape", L"\\x41\\u0042\\.", 0, L"AB. AB!");

    /* errors */
    status = 0;
    re = p_uregex_open(L"(ab", -1, 0, NULL, &status);
    printf("paren=%d,%x\n", re == NULL, status);
    status = 0;
    re = p_uregex_open(L"[ab", -1, 0, NULL, &status);
    printf("bracket=%d,%x\n", re == NULL, status);
    status = 0;
    re = p_uregex_open(L"a{3,1}", -1, 0, NULL, &status);
    printf("interval_bad=%d,%x\n", re == NULL, status);
    status = 0;
    p_uregex_findNext(NULL, &status);
    printf("null=%x\n", status);
    status = 1;   /* an error already: nothing done */
    re = p_uregex_open(L"a", -1, 0, NULL, &status);
    printf("failed_before=%d,%x\n", re == NULL, status);

    /* closing the UText calls the provider's close; a heap one is freed */
    closed = 0;
    ut = p_utext_close(ut);
    printf("close=%d,%d\n", ut == &stack_ut, closed);
    status = 0;
    ut = p_utext_setup(NULL, 16, &status);
    printf("heap=%d,%x,%d\n", ut && ut->pExtra && ut->extraSize >= 16, status, ut ? ut->flags : -1);
    printf("heap_close=%d\n", p_utext_close(ut) == NULL);
    return 0;
}
