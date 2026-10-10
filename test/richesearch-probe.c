/* riched20 ITextRange batch (patches/sg/2028), run by test/richesearch-gate.sh.
 * Families: MoveWhile / MoveUntil and their Start and End forms (strings and
 * character types, both directions, counts), FindText / FindTextStart /
 * FindTextEnd (case, whole word, counts, direction), on a range and on the
 * selection.
 *
 *   richesearch-probe.exe */
#define COBJMACROS
#include <windows.h>

#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID SG_IID_ITextDocument = { 0x8cc497c0, 0xa1df, 0x11ce, { 0x80, 0x98, 0x00, 0xaa, 0x00, 0x47, 0xbe, 0x5d } };
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

static HWND edit;
static ITextDocument *doc;

/* "hello world, foo-bar 123" is 24 characters, then a paragraph mark */
static ITextRange *rng(LONG s, LONG e)
{
    ITextRange *r = NULL;
    ITextDocument_Range(doc, s, e, &r);
    return r;
}

static int at(ITextRange *r, LONG s, LONG e)
{
    LONG a = -1, b = -1;
    ITextRange_GetStart(r, &a);
    ITextRange_GetEnd(r, &b);
    return a == s && b == e;
}

static VARIANT bs(const WCHAR *s)
{
    VARIANT v;
    VariantInit(&v);
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(s);
    return v;
}

static VARIANT num(LONG n)
{
    VARIANT v;
    VariantInit(&v);
    V_VT(&v) = VT_I4;
    V_I4(&v) = n;
    return v;
}

typedef HRESULT (WINAPI *move_fn)(ITextRange *, VARIANT *, LONG, LONG *);

struct move_case { const char *what; int fn; LONG s, e; const WCHAR *set; LONG count; HRESULT hr; LONG delta, rs, re; };
enum { WHILE, STARTWHILE, ENDWHILE, UNTIL, STARTUNTIL, ENDUNTIL };

static HRESULT call_move(ITextRange *r, int fn, VARIANT *set, LONG count, LONG *delta)
{
    switch (fn)
    {
    case WHILE: return ITextRange_MoveWhile(r, set, count, delta);
    case STARTWHILE: return ITextRange_MoveStartWhile(r, set, count, delta);
    case ENDWHILE: return ITextRange_MoveEndWhile(r, set, count, delta);
    case UNTIL: return ITextRange_MoveUntil(r, set, count, delta);
    case STARTUNTIL: return ITextRange_MoveStartUntil(r, set, count, delta);
    default: return ITextRange_MoveEndUntil(r, set, count, delta);
    }
}

static const struct move_case move_cases[] = {
    { "while over 'hello '",           WHILE,      0,  0, L" helo",       tomForward, S_OK,    6, 6, 6 },
    { "while limited by count",        WHILE,      0,  0, L" helo",       3,          S_OK,    3, 3, 3 },
    { "while collapses at the end",    WHILE,      0,  2, L"lo ",         50,         S_OK,    4, 6, 6 },
    { "while backwards",               WHILE,     11, 11, L"dlrow",       tomBackward, S_OK,  -5, 6, 6 },
    { "while backwards collapses at the start", WHILE, 8, 11, L"ol",     -5,         S_OK,    -1, 7, 7 },
    { "while nothing matches",         WHILE,      0,  0, L"xyz",         10,         S_FALSE, 0, 0, 0 },
    { "until the comma",               UNTIL,      0,  0, L",",           tomForward, S_OK,    11, 11, 11 },
    { "until already on it",           UNTIL,     11, 11, L",",           tomForward, S_FALSE, 0, 11, 11 },
    { "until backwards",               UNTIL,     20, 20, L"o",           -100,       S_OK,    -4, 16, 16 },
    { "until not found stops at the last position", UNTIL,   0,  0, L"@",           tomForward, S_OK,    36, 36, 36 },
    { "count zero",                    WHILE,      3,  3, L"abc",         0,          S_FALSE, 0, 3, 3 },
    { "start while keeps the end",     STARTWHILE, 0, 11, L" hel",        tomForward, S_OK,    4, 4, 11 },
    { "start while past the end",      STARTWHILE, 2,  5, L"hello world", 50,         S_OK,    9, 11, 11 },
    { "start until",                   STARTUNTIL, 0, 11, L"w",           tomForward, S_OK,    6, 6, 11 },
    { "start while backwards",         STARTWHILE, 8, 11, L"ol",          -5,         S_OK,    -1, 7, 11 },
    { "end while",                     ENDWHILE,   0,  5, L"o wr",        tomForward, S_OK,    4, 0, 9 },
    { "end while backwards",           ENDWHILE,   3,  8, L"o w",         -2,         S_OK,    -2, 3, 6 },
    { "end while backwards past start", ENDWHILE,  3,  8, L"helo wrd",    tomBackward, S_OK,   -8, 0, 0 },
    { "end until",                     ENDUNTIL,   0,  2, L"-",           tomForward, S_OK,    14, 0, 16 },
};

static void test_move_strings(void)
{
    unsigned i;
    for (i = 0; i < sizeof(move_cases) / sizeof(move_cases[0]); i++)
    {
        const struct move_case *c = &move_cases[i];
        ITextRange *r = rng(c->s, c->e);
        VARIANT set = bs(c->set);
        LONG delta = 777;
        HRESULT hr = call_move(r, c->fn, &set, c->count, &delta);
        char buf[200];
        LONG a = -1, b = -1;
        ITextRange_GetStart(r, &a);
        ITextRange_GetEnd(r, &b);
        if (hr != c->hr || delta != c->delta || a != c->rs || b != c->re)
            printf("   hr %08lx delta %ld range %ld-%ld\n", (unsigned long)hr, delta, a, b);
        snprintf(buf, sizeof(buf), "move: %s", c->what);
        check(hr == c->hr && delta == c->delta && a == c->rs && b == c->re, buf);
        VariantClear(&set);
        ITextRange_Release(r);
    }
}

static void test_move_types(void)
{
    ITextRange *r = rng(0, 0);
    VARIANT set = num(4); /* C1_DIGIT */
    LONG delta = 0;
    HRESULT hr;
    VARIANT bad;

    hr = ITextRange_MoveUntil(r, &set, tomForward, &delta);
    check(hr == S_OK && delta == 21 && at(r, 21, 21), "until a digit by type");
    hr = ITextRange_MoveWhile(r, &set, tomForward, &delta);
    check(hr == S_OK && delta == 3 && at(r, 24, 24), "while digits by type");
    set = num(8); /* C1_SPACE */
    ITextRange_SetStart(r, 0); ITextRange_SetEnd(r, 0);
    hr = ITextRange_MoveUntil(r, &set, tomForward, &delta);
    check(hr == S_OK && delta == 5 && at(r, 5, 5), "until a space by type");

    VariantInit(&bad);
    delta = 5;
    hr = ITextRange_MoveWhile(r, &bad, 3, &delta);
    check(hr == E_INVALIDARG && delta == 0, "an empty variant is invalid");
    hr = ITextRange_MoveWhile(r, NULL, 3, &delta);
    check(hr == E_INVALIDARG, "no set is invalid");
    ITextRange_Release(r);
}

static void test_selection(void)
{
    ITextSelection *sel = NULL;
    VARIANT set = bs(L",");
    LONG delta = 0, s = -1, e = -1;
    HRESULT hr;

    ITextDocument_GetSelection(doc, &sel);
    check(sel != NULL, "selection");
    if (!sel) return;
    ITextSelection_SetRange(sel, 0, 0);
    hr = ITextSelection_MoveUntil(sel, &set, tomForward, &delta);
    ITextSelection_GetStart(sel, &s);
    ITextSelection_GetEnd(sel, &e);
    check(hr == S_OK && delta == 11 && s == 11 && e == 11, "selection MoveUntil");
    VariantClear(&set);
    set = bs(L"-");
    hr = ITextSelection_MoveEndUntil(sel, &set, tomForward, &delta);
    ITextSelection_GetStart(sel, &s);
    ITextSelection_GetEnd(sel, &e);
    check(hr == S_OK && delta == 5 && s == 11 && e == 16, "selection MoveEndUntil");
    {
        BSTR w = SysAllocString(L"BAR");
        hr = ITextSelection_FindText(sel, w, tomForward, 0, &delta);
        ITextSelection_GetStart(sel, &s);
        ITextSelection_GetEnd(sel, &e);
        check(hr == S_OK && delta == 3 && s == 17 && e == 20, "selection FindText");
        SysFreeString(w);
    }
    VariantClear(&set);
    ITextSelection_Release(sel);
}

struct find_case { const char *what; int fn; LONG s, e; const WCHAR *text; LONG count, flags; HRESULT hr; LONG length, rs, re; };
enum { FIND, FINDSTART, FINDEND };

static const struct find_case find_cases[] = {
    { "case is ignored",        FIND, 0, 0, L"WORLD", tomForward, 0, S_OK, 5, 6, 11 },
    { "match case",             FIND, 0, 0, L"WORLD", tomForward, tomMatchCase, S_FALSE, 0, 0, 0 },
    { "match case, right case", FIND, 0, 0, L"world", tomForward, tomMatchCase, S_OK, 5, 6, 11 },
    { "whole word fails inside", FIND, 0, 0, L"wor", tomForward, tomMatchWord, S_FALSE, 0, 0, 0 },
    { "whole word next to dash", FIND, 0, 0, L"foo", tomForward, tomMatchWord, S_OK, 3, 13, 16 },
    { "count too short",        FIND, 0, 0, L"world", 5, 0, S_FALSE, 0, 0, 0 },
    { "count long enough",      FIND, 0, 0, L"world", 11, 0, S_OK, 5, 6, 11 },
    { "searches from the end",  FIND, 0, 7, L"o", tomForward, 0, S_OK, 1, 7, 8 },
    { "backwards",              FIND, 20, 20, L"o", -100, 0, S_OK, 1, 15, 16 },
    { "backwards from the start", FIND, 14, 20, L"o", tomBackward, 0, S_OK, 1, 7, 8 },
    { "zero count: in the range", FIND, 6, 11, L"orl", 0, 0, S_OK, 3, 7, 10 },
    { "zero count: outside",    FIND, 0, 5, L"world", 0, 0, S_FALSE, 0, 0, 5 },
    { "across the paragraph mark", FIND, 0, 0, L"3\rsec", tomForward, 0, S_OK, 5, 23, 28 },
    { "start of the match",     FINDSTART, 0, 0, L"world", tomForward, 0, S_OK, 5, 6, 6 },
    { "end of the match",       FINDEND, 0, 0, L"world", tomForward, 0, S_OK, 5, 11, 11 },
    { "start, not found",       FINDSTART, 3, 4, L"zzz", tomForward, 0, S_FALSE, 0, 3, 4 },
};

static void test_find(void)
{
    unsigned i;
    for (i = 0; i < sizeof(find_cases) / sizeof(find_cases[0]); i++)
    {
        const struct find_case *c = &find_cases[i];
        ITextRange *r = rng(c->s, c->e);
        BSTR t = SysAllocString(c->text);
        LONG len = 777, a = -1, b = -1;
        HRESULT hr;
        char buf[200];
        switch (c->fn)
        {
        case FIND: hr = ITextRange_FindText(r, t, c->count, c->flags, &len); break;
        case FINDSTART: hr = ITextRange_FindTextStart(r, t, c->count, c->flags, &len); break;
        default: hr = ITextRange_FindTextEnd(r, t, c->count, c->flags, &len); break;
        }
        ITextRange_GetStart(r, &a);
        ITextRange_GetEnd(r, &b);
        if (hr != c->hr || len != c->length || a != c->rs || b != c->re)
            printf("   hr %08lx length %ld range %ld-%ld\n", (unsigned long)hr, len, a, b);
        snprintf(buf, sizeof(buf), "find: %s", c->what);
        check(hr == c->hr && len == c->length && a == c->rs && b == c->re, buf);
        SysFreeString(t);
        ITextRange_Release(r);
    }
    {
        ITextRange *r = rng(0, 0);
        BSTR empty = SysAllocString(L"");
        LONG len = 5;
        check(ITextRange_FindText(r, empty, tomForward, 0, &len) == E_INVALIDARG && len == 0, "find: empty text is invalid");
        check(ITextRange_FindText(r, NULL, tomForward, 0, &len) == E_INVALIDARG, "find: NULL text is invalid");
        SysFreeString(empty);
        ITextRange_Release(r);
    }
}

int main(void)
{
    IRichEditOle *ole = NULL;
    HMODULE lib = LoadLibraryW(L"msftedit.dll");

    CoInitialize(NULL);
    if (!lib) { printf("FAIL  msftedit not loaded\nRESULT: FAIL\n"); return 1; }
    edit = CreateWindowExW(0, L"RichEdit50W", L"", WS_POPUP | ES_MULTILINE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    check(edit != NULL, "rich edit window");
    if (!edit) { printf("RESULT: FAIL\n"); return 1; }
    SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)L"hello world, foo-bar 123\rsecond line");
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    check(ole != NULL, "IRichEditOle");
    IUnknown_QueryInterface((IUnknown *)ole, &SG_IID_ITextDocument, (void **)&doc);
    check(doc != NULL, "ITextDocument");
    if (!doc) { printf("RESULT: FAIL\n"); return 1; }

    test_move_strings();
    test_move_types();
    test_selection();
    test_find();

    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)ole);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
