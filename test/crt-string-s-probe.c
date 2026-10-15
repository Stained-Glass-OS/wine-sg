/* Probe for patches/sg/4511: secure and locale string functions of the CRT
 * named on the command line (msvcr90, msvcr120, ucrtbase; msvcrt for the
 * plain _mbsncoll/_mbsnicoll). Table driven where it can be. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int failures;
static HMODULE mod;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
#define GET(v, n) v = (void *)GetProcAddress(mod, n); check(v != NULL, "export %s", n)

static void __cdecl ignore(const wchar_t *e, const wchar_t *f, const wchar_t *file, unsigned int line, UINT_PTR r) {}
static int __cdecl cmp_ctx(void *ctx, const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(int argc, char **argv)
{
    int (__cdecl *mbsncoll)(const char *, const char *, size_t), (__cdecl *mbsnicoll)(const char *, const char *, size_t);
    int (__cdecl *strset_s)(char *, size_t, int), (__cdecl *strerror_s_)(char *, size_t, const char *);
    int (__cdecl *umask_s)(int, int *), (__cdecl *mbsset_s)(char *, size_t, unsigned int);
    int (__cdecl *mbsnset_s)(char *, size_t, unsigned int, size_t), (__cdecl *mbsnbset_s)(char *, size_t, unsigned int, size_t);
    int (__cdecl *mbsncat_s)(char *, size_t, const char *, size_t), (__cdecl *setmbcp)(int);
    size_t (__cdecl *wcsncnt)(const wchar_t *, size_t), (__cdecl *mbstrnlen)(const char *, size_t);
    void *(__cdecl *lsearch_s)(const void *, void *, unsigned int *, size_t, void *, void *);
    char *(__cdecl *mbsninc_l)(const char *, size_t, void *), *(__cdecl *mbsstr_l)(const char *, const char *, void *);
    void *(__cdecl *set_iph)(void *);
    int *(__cdecl *errno_)(void);
    char buf[64];
    int arr[8] = {1, 2}, key, old, ret;
    unsigned int num = 2;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    GET(mbsncoll, "_mbsncoll");
    GET(mbsnicoll, "_mbsnicoll");
    if (mbsncoll && mbsnicoll)
    {
        check(mbsncoll("abcX", "abcY", 3) == 0 && mbsncoll("abc", "abd", 3) < 0 && mbsncoll("b", "a", 1) > 0, "_mbsncoll");
        check(mbsnicoll("ABCx", "abcy", 3) == 0 && mbsnicoll("ABC", "abd", 3) < 0, "_mbsnicoll");
    }
    if (!strcmp(argv[1], "msvcrt")) goto done;

    GET(set_iph, "_set_invalid_parameter_handler");
    GET(errno_, "_errno");
    GET(strset_s, "_strset_s"); GET(strerror_s_, "_strerror_s"); GET(umask_s, "_umask_s");
    GET(mbsset_s, "_mbsset_s"); GET(mbsnset_s, "_mbsnset_s"); GET(mbsnbset_s, "_mbsnbset_s"); GET(mbsncat_s, "_mbsncat_s");
    GET(wcsncnt, "__wcsncnt"); GET(mbstrnlen, "_mbstrnlen"); GET(lsearch_s, "_lsearch_s");
    GET(mbsninc_l, "_mbsninc_l"); GET(mbsstr_l, "_mbsstr_l"); GET(setmbcp, "_setmbcp");
    if (failures) goto done;
    set_iph(ignore);

    strcpy(buf, "abc");
    check(!strset_s(buf, 4, 'x') && !strcmp(buf, "xxx"), "_strset_s: %s", buf);
    strcpy(buf, "abc");
    check(strset_s(buf, 3, 'x') == EINVAL && !buf[0], "_strset_s on an unterminated string");
    check(wcsncnt(L"abcd", 2) == 2 && wcsncnt(L"ab", 5) == 2, "__wcsncnt");

    *errno_() = ENOENT;
    ret = strerror_s_(buf, sizeof(buf), "pre");
    check(!ret && !strncmp(buf, "pre: ", 5) && buf[strlen(buf) - 1] == '\n', "_strerror_s with a prefix: %s", buf);
    *errno_() = ENOENT;
    ret = strerror_s_(buf, 6, NULL);
    check(!ret && strlen(buf) == 5, "_strerror_s truncated to the buffer: \"%s\"", buf);

    check(!umask_s(0, &old), "_umask_s");
    check(!umask_s(old, &key) && key == 0, "_umask_s gives the previous mask");
    check(umask_s(0x1, &key) == EINVAL, "_umask_s with an invalid mode");

    key = 3;
    check(lsearch_s(&key, arr, &num, sizeof(int), cmp_ctx, NULL) == &arr[2] && num == 3 && arr[2] == 3, "_lsearch_s appends a missing key");
    key = 2;
    check(lsearch_s(&key, arr, &num, sizeof(int), cmp_ctx, NULL) == &arr[1] && num == 3, "_lsearch_s finds a key");

    check(mbsninc_l("abc", 2, NULL) && !strcmp(mbsninc_l("abc", 2, NULL), "c"), "_mbsninc_l");
    check(mbsstr_l("hello world", "wor", NULL) && !strcmp(mbsstr_l("hello world", "wor", NULL), "world") &&
          !mbsstr_l("hello", "xyz", NULL), "_mbsstr_l");
    check(mbstrnlen("abc", 10) == 3 && mbstrnlen("abcdef", 2) == 2, "_mbstrnlen");

    strcpy(buf, "abc");
    check(!mbsset_s(buf, 4, 'z') && !strcmp(buf, "zzz"), "_mbsset_s");
    strcpy(buf, "abcd");
    check(!mbsnset_s(buf, 5, 'q', 2) && !strcmp(buf, "qqcd"), "_mbsnset_s: %s", buf);
    strcpy(buf, "abcd");
    check(!mbsnbset_s(buf, 5, 'r', 3) && !strcmp(buf, "rrrd"), "_mbsnbset_s: %s", buf);
    strcpy(buf, "abcd");
    check(mbsset_s(buf, 4, 'z') == EINVAL && !buf[0], "_mbsset_s on an unterminated string");
    strcpy(buf, "ab");
    check(!mbsncat_s(buf, 8, "cdef", 2) && !strcmp(buf, "abcd"), "_mbsncat_s: %s", buf);

    setmbcp(932);
    check(mbstrnlen("a\x82\xa0" "b", 10) == 3, "_mbstrnlen counts a double-byte character once (932)");
    check(mbstrnlen("a\x82", 10) == (size_t)-1, "_mbstrnlen: a lead byte without a trail byte");
    check(mbsstr_l("\x82\x61" "a", "a", NULL) && !strcmp(mbsstr_l("\x82\x61" "a", "a", NULL), "a"),
          "_mbsstr_l does not match a trail byte (932)");
    setmbcp(1252);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
