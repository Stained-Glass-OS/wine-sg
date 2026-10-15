/* Probe for patches/sg/4573: in the "C" locale the msvcp collation helpers
 * _Strcoll, _Wcscoll, _Strxfrm and _Wcsxfrm (and through them collate<>::
 * compare and transform) order strings by code unit, like strcmp/wcscmp; a
 * named locale still collates linguistically. msvcp DLL on the command line.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef int (__cdecl *fn_strcoll)(const char *, const char *, const char *, const char *, const void *);
typedef int (__cdecl *fn_wcscoll)(const wchar_t *, const wchar_t *, const wchar_t *, const wchar_t *, const void *);
typedef size_t (__cdecl *fn_strxfrm)(char *, char *, const char *, const char *, const void *);
typedef size_t (__cdecl *fn_wcsxfrm)(wchar_t *, wchar_t *, const wchar_t *, const wchar_t *, const void *);
typedef char *(__cdecl *fn_setlocale)(int, const char *);

static int sign(int v) { return v < 0 ? -1 : v > 0; }

/* the C runtime each msvcp uses */
static const char *crt_of(const char *dll)
{
    static const struct { const char *p, *r; } map[] =
    {
        {"msvcp60", "msvcrt"}, {"msvcp70", "msvcr70"}, {"msvcp71", "msvcr71"}, {"msvcp80", "msvcr80"},
        {"msvcp90", "msvcr90"}, {"msvcp100", "msvcr100"}, {"msvcp110", "msvcr110"}, {"msvcp120", "msvcr120"},
        {"msvcp140", "ucrtbase"},
    };
    unsigned int i;
    for (i = 0; i < ARRAYSIZE(map); i++) if (!strcmp(map[i].p, dll)) return map[i].r;
    return NULL;
}

int main(int argc, char **argv)
{
    /* code unit order: the sign of strcmp on the bytes, a prefix first */
    static const struct { const char *a; size_t alen; const char *b; size_t blen; int want; } narrow[] =
    {
        {"A", 1, "a", 1, -1}, {"Z", 1, "a", 1, -1}, {"a", 1, "B", 1, 1}, {"abc", 3, "abd", 3, -1},
        {"ab", 2, "abc", 3, -1}, {"abc", 3, "ab", 2, 1}, {"same", 4, "same", 4, 0}, {"", 0, "", 0, 0},
        {"", 0, "a", 1, -1}, {"\xe9", 1, "z", 1, 1}, {"a b", 3, "a-b", 3, -1}, {"co-op", 5, "coop", 4, -1},
        {"a\0b", 3, "a\0c", 3, -1}, {"\x7f", 1, "\x80", 1, -1}, {"[", 1, "a", 1, -1},
    };
    static const struct { const wchar_t *a; size_t alen; const wchar_t *b; size_t blen; int want; } wide[] =
    {
        {L"A", 1, L"a", 1, -1}, {L"Z", 1, L"a", 1, -1}, {L"abc", 3, L"abd", 3, -1}, {L"ab", 2, L"abc", 3, -1},
        {L"same", 4, L"same", 4, 0}, {L"\x00e9", 1, L"z", 1, 1}, {L"\xff21", 1, L"a", 1, 1},
        {L"co-op", 5, L"coop", 4, -1}, {L"\x8000", 1, L"\x7fff", 1, 1},
    };
    fn_strcoll p_strcoll;
    fn_wcscoll p_wcscoll;
    fn_strxfrm p_strxfrm;
    fn_wcsxfrm p_wcsxfrm;
    fn_setlocale p_setlocale = NULL;
    HMODULE mod, crt;
    const char *crtname;
    unsigned int i;
    int r;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_strcoll = (void *)GetProcAddress(mod, "_Strcoll");
    p_wcscoll = (void *)GetProcAddress(mod, "_Wcscoll");
    p_strxfrm = (void *)GetProcAddress(mod, "_Strxfrm");
    p_wcsxfrm = (void *)GetProcAddress(mod, "_Wcsxfrm");
    check(p_strcoll && p_strxfrm, "_Strcoll and _Strxfrm exported");
    check(!strcmp(argv[1], "msvcp60") || (p_wcscoll && p_wcsxfrm), "_Wcscoll and _Wcsxfrm exported");
    if ((crtname = crt_of(argv[1])) && (crt = GetModuleHandleA(crtname)))
        p_setlocale = (void *)GetProcAddress(crt, "setlocale");
    check(p_setlocale != NULL, "%s setlocale", crtname ? crtname : "(no CRT)");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    /* the "C" locale */
    for (i = 0; i < ARRAYSIZE(narrow); i++)
    {
        r = p_strcoll(narrow[i].a, narrow[i].a + narrow[i].alen, narrow[i].b, narrow[i].b + narrow[i].blen, NULL);
        check(sign(r) == narrow[i].want, "C _Strcoll case %u: %d, want %d", i, r, narrow[i].want);
        r = p_strcoll(narrow[i].b, narrow[i].b + narrow[i].blen, narrow[i].a, narrow[i].a + narrow[i].alen, NULL);
        check(sign(r) == -narrow[i].want, "C _Strcoll case %u swapped: %d", i, r);
    }
    for (i = 0; p_wcscoll && i < ARRAYSIZE(wide); i++)
    {
        r = p_wcscoll(wide[i].a, wide[i].a + wide[i].alen, wide[i].b, wide[i].b + wide[i].blen, NULL);
        check(sign(r) == wide[i].want, "C _Wcscoll case %u: %d, want %d", i, r, wide[i].want);
        r = p_wcscoll(wide[i].b, wide[i].b + wide[i].blen, wide[i].a, wide[i].a + wide[i].alen, NULL);
        check(sign(r) == -wide[i].want, "C _Wcscoll case %u swapped: %d", i, r);
    }
    /* transform in the "C" locale is the string itself, so sort keys keep the byte order */
    {
        static const char in[] = "Za-b";
        static const wchar_t win[] = L"Za-b";
        char out[16];
        wchar_t wout[16];
        size_t n;

        memset(out, 0x55, sizeof(out));
        n = p_strxfrm(out, out + sizeof(out), in, in + 4, NULL);
        check(n == 4 && !memcmp(out, in, 4), "C _Strxfrm copies: %Iu", n);
        n = p_strxfrm(out, out + 2, in, in + 4, NULL);
        check(n == 4, "C _Strxfrm small buffer: %Iu", n);
        if (p_wcsxfrm)
        {
            memset(wout, 0x55, sizeof(wout));
            n = p_wcsxfrm(wout, wout + ARRAYSIZE(wout), win, win + 4, NULL);
            check(n == 4 && !memcmp(wout, win, 4 * sizeof(wchar_t)), "C _Wcsxfrm copies: %Iu", n);
        }
    }

    /* the C runtimes before msvcr110 keep a category that is "C" already when
     * "C" is set again: a collate handle given to it survives */
    {
        struct locinfo_head { unsigned int refcount, lc_codepage, lc_collate_cp; unsigned long lc_handle[6]; };
        struct locale_s { struct locinfo_head *locinfo; void *mbcinfo; } *(__cdecl *p_get_locale)(void);
        void (__cdecl *p_free_locale)(void *);
        unsigned long *(__cdecl *p_handles)(void);
        static const char *const old[] = {"msvcp80", "msvcp90", "msvcp100"};  /* _get_current_locale: msvcr80+ */
        unsigned int j, is_old = 0;

        for (j = 0; j < ARRAYSIZE(old); j++) if (!strcmp(argv[1], old[j])) is_old = 1;
        p_get_locale = (void *)GetProcAddress(crt, "_get_current_locale");
        p_free_locale = (void *)GetProcAddress(crt, "_free_locale");
        p_handles = (void *)GetProcAddress(crt, "___lc_handle_func");
        if (is_old && p_get_locale && p_free_locale && p_handles)
        {
            struct locale_s *l = p_get_locale();
            l->locinfo->lc_handle[LC_COLLATE] = 0x409;
            p_free_locale(l);
            p_setlocale(LC_ALL, "C");
            check(p_handles()[LC_COLLATE] == 0x409, "setlocale(\"C\") keeps the \"C\" collate category: %#lx",
                  p_handles()[LC_COLLATE]);
            r = p_strcoll("Z", "Z" + 1, "a", "a" + 1, NULL);
            check(r > 0, "with that handle _Strcoll is linguistic: %d", r);
            l = p_get_locale();
            l->locinfo->lc_handle[LC_COLLATE] = 0;
            p_free_locale(l);
        }
        else check(!is_old, "%s: CRT locale exports", argv[1]);
    }

    /* a named locale still collates linguistically: lower case first, "Z" after "a" */
    if (p_setlocale(LC_ALL, "English_United States.1252"))
    {
        r = p_strcoll("A", "A" + 1, "a", "a" + 1, NULL);
        check(r > 0, "English _Strcoll A, a: %d", r);
        r = p_strcoll("Z", "Z" + 1, "a", "a" + 1, NULL);
        check(r > 0, "English _Strcoll Z, a: %d", r);
        r = p_strcoll("same", "same" + 4, "same", "same" + 4, NULL);
        check(r == 0, "English _Strcoll equal: %d", r);
        if (p_wcscoll)
        {
            r = p_wcscoll(L"Z", L"Z" + 1, L"a", L"a" + 1, NULL);
            check(r > 0, "English _Wcscoll Z, a: %d", r);
        }
        p_setlocale(LC_ALL, "C");
        r = p_strcoll("Z", "Z" + 1, "a", "a" + 1, NULL);
        check(r < 0, "back in C _Strcoll Z, a: %d", r);
    }
    else check(0, "setlocale English");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
