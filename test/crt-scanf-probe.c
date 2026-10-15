/* Probe for patches/sg/4515: msvcr120's va_list scanf forms (vfscanf,
 * vfwscanf, vscanf, vwscanf, vsscanf_s, vswscanf_s and the _s stream forms)
 * and _getws_s (msvcr80 and later, ucrtbase) of the DLL named on the command
 * line. Standard input is a file set before the DLL loads.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int invalid;
static void __cdecl handler(const wchar_t *e, const wchar_t *f, const wchar_t *fn, unsigned int l, uintptr_t r) { invalid++; }

static void *vfunc;
static int kind;  /* 0 (fmt, va), 1 (stream or buffer, fmt, va) */
static void *first;
static int __cdecl call(const void *fmt, ...)
{
    va_list va;
    int ret;

    va_start(va, fmt);
    if (!kind) ret = ((int (__cdecl *)(const void *, va_list))vfunc)(fmt, va);
    else ret = ((int (__cdecl *)(void *, const void *, va_list))vfunc)(first, fmt, va);
    va_end(va);
    return ret;
}

static HANDLE make_file(const char *name, const char *text)
{
    char path[MAX_PATH];
    HANDLE f;
    DWORD w;

    GetTempPathA(sizeof(path), path);
    strcat(path, name);
    f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    WriteFile(f, text, strlen(text), &w, NULL);
    SetFilePointer(f, 0, NULL, FILE_BEGIN);
    return f;
}

int main(int argc, char **argv)
{
    static const char input[] = "7 alpha\n8 beta\nline one\r\nsecond\n";
    WCHAR *(__cdecl *p_getws_s)(WCHAR *, size_t);
    void *(__cdecl *p_fopen)(const char *, const char *);
    int (__cdecl *p_fclose)(void *);
    void *(__cdecl *p_handler)(void *);
    HANDLE in, saved;
    HMODULE mod;
    char path[MAX_PATH], s1[32], s2[32];
    WCHAR w1[32], w2[32], *wr;
    int n1, n2, r, msvcr120;

    if (argc < 2) { printf("FAIL usage\nRESULT: FAIL\n"); return 1; }
    in = make_file("crt-scanf-stdin.txt", input);
    saved = GetStdHandle(STD_INPUT_HANDLE);
    SetStdHandle(STD_INPUT_HANDLE, in);
    mod = LoadLibraryA(argv[1]);
    SetStdHandle(STD_INPUT_HANDLE, saved);
    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    msvcr120 = !strcmp(argv[1], "msvcr120");
    p_getws_s = (void *)GetProcAddress(mod, "_getws_s");
    p_fopen = (void *)GetProcAddress(mod, "fopen");
    p_fclose = (void *)GetProcAddress(mod, "fclose");
    p_handler = (void *)GetProcAddress(mod, "_set_invalid_parameter_handler");
    check(p_getws_s && p_handler, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    p_handler(handler);

    if (msvcr120)
    {
        static const char *names[] = {"vfscanf", "vfscanf_s", "vfwscanf", "vfwscanf_s", "vscanf", "vscanf_s",
                                      "vwscanf", "vwscanf_s", "vsscanf_s", "vswscanf_s"};
        unsigned int i;
        void *f;

        for (i = 0; i < ARRAYSIZE(names); i++) check(GetProcAddress(mod, names[i]) != NULL, "%s exported", names[i]);

        /* standard input: the first two lines */
        vfunc = GetProcAddress(mod, "vscanf"); kind = 0;
        r = call("%d %s", &n1, s1);
        check(r == 2 && n1 == 7 && !strcmp(s1, "alpha"), "vscanf: %d, %d \"%s\"", r, n1, s1);
        vfunc = GetProcAddress(mod, "vwscanf_s");
        r = call(L" %d %ls%*c", &n2, w1, (unsigned)ARRAYSIZE(w1));
        check(r == 2 && n2 == 8 && !wcscmp(w1, L"beta"), "vwscanf_s: %d, %d", r, n2);

        /* a stream */
        GetTempPathA(sizeof(path), path);
        strcat(path, "crt-scanf-file.txt");
        {
            FILE *t = fopen(path, "w");
            fputs("13 thirteen 14 fourteen 15 fifteen 16 sixteen", t);
            fclose(t);
        }
        f = p_fopen(path, "r");
        first = f; kind = 1;
        vfunc = GetProcAddress(mod, "vfscanf");
        r = call("%d %s", &n1, s1);
        check(r == 2 && n1 == 13 && !strcmp(s1, "thirteen"), "vfscanf: %d, %d \"%s\"", r, n1, s1);
        vfunc = GetProcAddress(mod, "vfscanf_s");
        r = call("%d %s", &n1, s1, (unsigned)sizeof(s1));
        check(r == 2 && n1 == 14 && !strcmp(s1, "fourteen"), "vfscanf_s: %d, %d \"%s\"", r, n1, s1);
        vfunc = GetProcAddress(mod, "vfwscanf");
        r = call(L"%d %ls", &n1, w1);
        check(r == 2 && n1 == 15 && !wcscmp(w1, L"fifteen"), "vfwscanf: %d, %d", r, n1);
        vfunc = GetProcAddress(mod, "vfwscanf_s");
        r = call(L"%d %ls", &n1, w1, (unsigned)ARRAYSIZE(w1));
        check(r == 2 && n1 == 16 && !wcscmp(w1, L"sixteen"), "vfwscanf_s: %d, %d", r, n1);
        r = call(L"%d", &n1);
        check(r == EOF, "vfwscanf_s at end: %d", r);
        p_fclose(f);
        DeleteFileA(path);

        /* the _s string forms honour the buffer size */
        vfunc = GetProcAddress(mod, "vsscanf_s");
        first = (void *)"hello 5";
        memset(s1, 'x', sizeof(s1)); n1 = 0;
        r = call("%s %d", s1, 3u, &n1);
        check(r == 0 && !s1[0] && n1 == 0, "vsscanf_s too small a buffer: %d \"%.8s\"", r, s1);
        r = call("%s %d", s2, (unsigned)sizeof(s2), &n1);
        check(r == 2 && !strcmp(s2, "hello") && n1 == 5, "vsscanf_s: %d \"%s\" %d", r, s2, n1);
        vfunc = GetProcAddress(mod, "vswscanf_s");
        first = (void *)L"wide 6";
        r = call(L"%ls %d", w2, (unsigned)ARRAYSIZE(w2), &n1);
        check(r == 2 && !wcscmp(w2, L"wide") && n1 == 6, "vswscanf_s: %d %d", r, n1);
        memset(w2, 'x', sizeof(w2));
        r = call(L"%ls", w2, 2u);
        check(r == 0 && !w2[0], "vswscanf_s too small a buffer: %d", r);

        invalid = 0;
        vfunc = GetProcAddress(mod, "vsscanf_s");
        first = NULL;
        r = call("%d", &n1);
        check(r == EOF && invalid == 1, "vsscanf_s(NULL buffer): %d, handler %d", r, invalid);
        invalid = 0;
        vfunc = GetProcAddress(mod, "vscanf"); kind = 0;
        r = call(NULL);
        check(r == EOF && invalid == 1, "vscanf(NULL): %d, handler %d", r, invalid);
    }
    else
    {
        /* skip the scanf lines */
        WCHAR skip[32];
        p_getws_s(skip, ARRAYSIZE(skip));
        p_getws_s(skip, ARRAYSIZE(skip));
    }

    wr = p_getws_s(w1, ARRAYSIZE(w1));
    check(wr == w1 && !wcscmp(w1, L"line one"), "_getws_s: %p \"%ls\"", wr, w1);
    invalid = 0;
    memset(w2, 'x', sizeof(w2));
    wr = p_getws_s(w2, 3);
    check(!wr && !w2[0] && invalid == 1, "_getws_s too small a buffer: %p, handler %d", wr, invalid);
    invalid = 0;
    wr = p_getws_s(NULL, 3);
    check(!wr && invalid == 1, "_getws_s(NULL): handler %d", invalid);
    /* what is left of the long line (as gets_s leaves it), then the end */
    invalid = 0;
    for (n1 = 0; n1 < 3 && (wr = p_getws_s(w1, ARRAYSIZE(w1))); n1++);
    check(!wr && n1 <= 1 && !invalid, "_getws_s reaches the end of input after %d more lines", n1);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
