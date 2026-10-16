/* Probe for patches/sg/4554: msvcp140's (and msvcp_win's)
 * basic_istream<unsigned short>::operator>>(unsigned short &), a number
 * read through the unsigned short num_get facet, against a table, from a
 * basic_istream<unsigned short> on a basic_streambuf whose get area holds
 * the text (msvcp140 exports no string streams).
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define SB "?$basic_streambuf@GU?$char_traits@G@std@@@"
#define IS "?$basic_istream@GU?$char_traits@G@std@@@std@@"

static char *ios_of(void *stream)
{
    const int *vbtable = *(const int **)stream;
    return (char *)stream + vbtable[1];
}

int main(int argc, char **argv)
{
    static const struct { const WCHAR *in; unsigned short want[3]; int count; int fail; } tests[] =
    {
        {L"123", {123}, 1, 0},
        {L"65535", {65535}, 1, 0},
        {L"  7 8\t9", {7, 8, 9}, 3, 0},
        {L"0x10", {0}, 1, 0},
        {L"70000", {0}, 1, 1},
        {L"abc", {0}, 1, 1},
        {L"", {0}, 1, 1},
    };
    void *(THISCALL *sb_ctor)(void *), *(THISCALL *is_ctor)(void *, void *, int, int);
    void (THISCALL *setg)(void *, WCHAR *, WCHAR *, WCHAR *);
    void *(THISCALL *read)(void *, unsigned short *);
    int (THISCALL *rdstate)(void *);
    HMODULE mod;
    unsigned int i, k;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    sb_ctor = (void *)GetProcAddress(mod, N("??0" SB "std@@IEAA@XZ", "??0" SB "std@@IAE@XZ"));
    setg = (void *)GetProcAddress(mod, N("?setg@" SB "std@@IEAAXPEAG00@Z", "?setg@" SB "std@@IAEXPAG00@Z"));
    is_ctor = (void *)GetProcAddress(mod, N("??0" IS "QEAA@PEAV" SB "1@_N@Z", "??0" IS "QAE@PAV" SB "1@_N@Z"));
    read = (void *)GetProcAddress(mod, N("??5" IS "QEAAAEAV01@AEAG@Z", "??5" IS "QAEAAV01@AAG@Z"));
    rdstate = (void *)GetProcAddress(mod, N("?rdstate@ios_base@std@@QEBAHXZ", "?rdstate@ios_base@std@@QBEHXZ"));
    check(sb_ctor && setg && is_ctor && read && rdstate, "exports %p %p %p %p %p", sb_ctor, setg, is_ctor, read, rdstate);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        void *sb = calloc(1, 1024), *is = calloc(1, 1024);
        WCHAR *text = _wcsdup(tests[i].in);
        unsigned short v[3] = {0xbad, 0xbad, 0xbad};
        int state = 0, ok = 1;

        sb_ctor(sb);
        setg(sb, text, text, text + wcslen(text));
        is_ctor(is, sb, 0, 1);
        for (k = 0; k < tests[i].count; k++)
        {
            if (read(is, &v[k]) != is) ok = 0;
            state = rdstate(ios_of(is));
            if (!tests[i].fail && v[k] != tests[i].want[k]) ok = 0;
        }
        if (tests[i].fail) ok = ok && (state & 2);
        else ok = ok && !(state & 2);
        check(ok, "operator>>(unsigned short &) \"%ls\": %u %u %u, state %#x", tests[i].in, v[0], v[1], v[2], state);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
