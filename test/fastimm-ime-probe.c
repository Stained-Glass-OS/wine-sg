/* imm32: the user dictionary, conversion and candidates, WM_IME_CONTROL and the hot keys of the built-in IME */
#include <windows.h>
#include <imm.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)

HIMCC WINAPI ImmCreateIMCC(DWORD);
HIMCC WINAPI ImmDestroyIMCC(HIMCC);
LPVOID WINAPI ImmLockIMCC(HIMCC);
BOOL WINAPI ImmUnlockIMCC(HIMCC);
DWORD WINAPI ImmGetIMCCLockCount(HIMCC);

#define IDC_DICT_READING  101
#define IDC_DICT_WORD     102
#define IDC_DICT_REGISTER 103
#define IDC_DICT_LIST     104
#define IDC_DICT_REMOVE   105

static const WCHAR word1[] = {0x65e5, 0x672c, 0};      /* Nihon */
static const WCHAR word2[] = {0x4e8c, 0x672c, 0};
static const WCHAR word3[] = {0x30cb, 0x30db, 0x30f3, 0};

/* ---------------------------------------------------------------- window messages */

static struct { UINT msg; WPARAM wp; LPARAM lp; } msgs[64];
static int nmsgs;

static WCHAR result_str[64];

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_IME_COMPOSITION && (lp & GCS_RESULTSTR))
    {
        HIMC himc = ImmGetContext(hwnd);
        LONG len = ImmGetCompositionStringW(himc, GCS_RESULTSTR, result_str, sizeof(result_str) - sizeof(WCHAR));
        result_str[len > 0 ? len / sizeof(WCHAR) : 0] = 0;
        ImmReleaseContext(hwnd, himc);
    }
    if ((msg >= WM_IME_STARTCOMPOSITION && msg <= WM_IME_COMPOSITION) || msg == WM_IME_NOTIFY)
    {
        if (nmsgs < ARRAY_SIZE(msgs))
        {
            msgs[nmsgs].msg = msg;
            msgs[nmsgs].wp = wp;
            msgs[nmsgs].lp = lp;
            nmsgs++;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int find_msg(UINT msg, WPARAM wp, int has_wp)
{
    int i, n = 0;
    for (i = 0; i < nmsgs; i++)
        if (msgs[i].msg == msg && (!has_wp || msgs[i].wp == wp)) n++;
    return n;
}

static LPARAM last_lparam(UINT msg)
{
    int i;
    for (i = nmsgs - 1; i >= 0; i--) if (msgs[i].msg == msg) return msgs[i].lp;
    return 0;
}

/* ---------------------------------------------------------------- the dictionary */

struct enum_data { int count; int stop_at; WCHAR reading[8][16]; DWORD style[8]; WCHAR string[8][16]; };

static int CALLBACK enum_proc(const WCHAR *reading, DWORD style, const WCHAR *string, void *data)
{
    struct enum_data *d = data;
    if (d->count < 8)
    {
        lstrcpynW(d->reading[d->count], reading, 16);
        d->style[d->count] = style;
        lstrcpynW(d->string[d->count], string, 16);
    }
    d->count++;
    return d->count == d->stop_at ? 0 : 7;
}

static int count_words(HKL hkl, const WCHAR *reading, DWORD style, const WCHAR *string)
{
    struct enum_data d = {0};
    d.stop_at = -1;
    ImmEnumRegisterWordW(hkl, enum_proc, reading, style, string, &d);
    return d.count;
}

static void test_dictionary(HKL hkl)
{
    STYLEBUFW styles[3];
    struct enum_data d = {0};
    UINT n;
    BOOL ret;
    DWORD ret_dw;

    /* no words yet */
    CHECK(count_words(hkl, NULL, 0, NULL) == 0, "dictionary empty at the start: %d", count_words(hkl, NULL, 0, NULL));

    /* the styles */
    CHECK(ImmGetRegisterWordStyleW(hkl, 0, NULL) == 2, "style count %u", ImmGetRegisterWordStyleW(hkl, 0, NULL));
    memset(styles, 0, sizeof(styles));
    n = ImmGetRegisterWordStyleW(hkl, 3, styles);
    CHECK(n == 2 && styles[0].dwStyle == IME_REGWORD_STYLE_EUDC && styles[1].dwStyle == IME_REGWORD_STYLE_USER_FIRST &&
          styles[0].szDescription[0] && styles[1].szDescription[0], "styles %u %lx %lx", n, styles[0].dwStyle, styles[1].dwStyle);
    n = ImmGetRegisterWordStyleW(hkl, 1, styles);
    CHECK(n == 1, "one style %u", n);

    /* registering */
    ret = ImmRegisterWordW(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word1);
    CHECK(ret, "register nihon %lu", GetLastError());
    ret = ImmRegisterWordW(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word2);
    CHECK(ret, "register second word of the reading");
    ret = ImmRegisterWordW(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word2);
    CHECK(ret, "registering a word again is fine");
    ret = ImmRegisterWordA(hkl, "abc", IME_REGWORD_STYLE_EUDC, "xyz");
    CHECK(ret, "register through the ANSI function");
    ret = ImmRegisterWordW(hkl, L"kana", IME_REGWORD_STYLE_USER_FIRST + 1, word3);
    CHECK(ret, "register in another user style");
    SetLastError(0xdeadbeef);
    ret = ImmRegisterWordW(hkl, L"bad", 5, L"x");
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "style out of the ranges %d %lu", ret, GetLastError());
    ret = ImmRegisterWordW(hkl, L"", IME_REGWORD_STYLE_USER_FIRST, L"x");
    CHECK(!ret, "empty reading");
    ret = ImmRegisterWordW(hkl, L"r", IME_REGWORD_STYLE_USER_FIRST, NULL);
    CHECK(!ret, "no string");
    ret = ImmRegisterWordW(hkl, NULL, IME_REGWORD_STYLE_USER_FIRST, L"x");
    CHECK(!ret, "no reading");
    ret = ImmRegisterWordW(hkl, L"a\001b", IME_REGWORD_STYLE_USER_FIRST, L"x");
    CHECK(!ret, "reading with the separator");

    /* enumerating: filters, wildcards, stopping, the return value */
    CHECK(count_words(hkl, NULL, 0, NULL) == 4, "all words %d", count_words(hkl, NULL, 0, NULL));
    CHECK(count_words(hkl, L"nihon", 0, NULL) == 2, "words of a reading %d", count_words(hkl, L"nihon", 0, NULL));
    CHECK(count_words(hkl, NULL, IME_REGWORD_STYLE_EUDC, NULL) == 1, "words of the EUDC style");
    CHECK(count_words(hkl, NULL, IME_REGWORD_STYLE_USER_FIRST, NULL) == 2, "words of the user style");
    CHECK(count_words(hkl, NULL, 0, word3) == 1, "words with a string");
    CHECK(count_words(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word2) == 1, "one exact word");
    CHECK(count_words(hkl, L"nope", 0, NULL) == 0, "reading without words");
    d.stop_at = -1;
    ret_dw = ImmEnumRegisterWordW(hkl, enum_proc, L"nihon", 0, NULL, &d);
    CHECK(ret_dw == 7 && d.count == 2, "enumeration returns the last callback value: %lu %d", ret_dw, d.count);
    memset(&d, 0, sizeof(d));
    d.stop_at = 1;
    ret_dw = ImmEnumRegisterWordW(hkl, enum_proc, NULL, 0, NULL, &d);
    CHECK(ret_dw == 0 && d.count == 1, "the callback stops the enumeration: %lu %d", ret_dw, d.count);
    memset(&d, 0, sizeof(d));
    d.stop_at = -1;
    ret_dw = ImmEnumRegisterWordW(hkl, enum_proc, L"nihon", 0, NULL, &d);
    CHECK(!lstrcmpW(d.reading[0], L"nihon") && d.style[0] == IME_REGWORD_STYLE_USER_FIRST &&
          ((!lstrcmpW(d.string[0], word1) && !lstrcmpW(d.string[1], word2)) || (!lstrcmpW(d.string[1], word1) && !lstrcmpW(d.string[0], word2))),
          "enumerated words");
    ret_dw = ImmEnumRegisterWordW(hkl, NULL, NULL, 0, NULL, NULL);
    CHECK(ret_dw == 0, "no callback");
    {
        char reading[16] = "abc";
        d.count = 0;
        d.stop_at = -1;
        CHECK(count_words(hkl, L"abc", 0, NULL) == 1, "ANSI-registered word");
        (void)reading;
    }

    /* the conversion lists */
    {
        BYTE buffer[512];
        CANDIDATELIST *list = (CANDIDATELIST *)buffer;
        DWORD size = ImmGetConversionListW(hkl, 0, L"nihon", NULL, 0, GCL_CONVERSION), got;
        DWORD size_rev;

        CHECK(size >= offsetof(CANDIDATELIST, dwOffset) + 2 * sizeof(DWORD) + 2 * 3 * sizeof(WCHAR), "conversion list size %lu", size);
        CHECK(size && size <= sizeof(buffer), "conversion list fits the buffer");
        memset(buffer, 0xcc, sizeof(buffer));
        got = ImmGetConversionListW(hkl, 0, L"nihon", list, sizeof(buffer), GCL_CONVERSION);
        CHECK(got == size, "conversion list copied %lu / %lu", got, size);
        CHECK(list->dwSize == size && list->dwCount == 2 && list->dwSelection == 0 && list->dwPageStart == 0, "list header %lu %lu %lu",
              list->dwSize, list->dwCount, list->dwSelection);
        CHECK(list->dwOffset[0] >= offsetof(CANDIDATELIST, dwOffset) + 2 * sizeof(DWORD) && list->dwOffset[0] < size, "offset 0");
        {
            const WCHAR *c0 = (WCHAR *)(buffer + list->dwOffset[0]), *c1 = (WCHAR *)(buffer + list->dwOffset[1]);
            CHECK((!lstrcmpW(c0, word1) && !lstrcmpW(c1, word2)) || (!lstrcmpW(c0, word2) && !lstrcmpW(c1, word1)), "candidates");
        }
        CHECK(ImmGetConversionListW(hkl, 0, L"nihon", list, size - 1, GCL_CONVERSION) == 0, "buffer too small");
        CHECK(ImmGetConversionListW(hkl, 0, L"nope", NULL, 0, GCL_CONVERSION) == 0, "reading without candidates");
        CHECK(ImmGetConversionListW(hkl, 0, NULL, NULL, 0, GCL_CONVERSION) == 0, "no source");
        CHECK(ImmGetConversionListW(hkl, 0, L"nihon", NULL, 0, 9) == 0, "bad flag");
        /* reverse: from the word to its readings */
        size_rev = ImmGetConversionListW(hkl, 0, word1, NULL, 0, GCL_REVERSECONVERSION);
        got = ImmGetConversionListW(hkl, 0, word1, list, sizeof(buffer), GCL_REVERSECONVERSION);
        CHECK(size_rev && got == size_rev && list->dwCount == 1 && !lstrcmpW((WCHAR *)(buffer + list->dwOffset[0]), L"nihon"),
              "reverse conversion %lu %lu", size_rev, got);
        CHECK(ImmGetConversionListW(hkl, 0, word1, NULL, 0, GCL_REVERSE_LENGTH) == 6 * sizeof(WCHAR), "reverse length %lu",
              ImmGetConversionListW(hkl, 0, word1, NULL, 0, GCL_REVERSE_LENGTH));
        /* the ANSI function converts the list */
        CHECK(ImmGetConversionListA(hkl, 0, "abc", NULL, 0, GCL_CONVERSION) > 0, "ANSI conversion list");
    }

    /* removing */
    ret = ImmUnregisterWordW(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word1);
    CHECK(ret, "unregister");
    SetLastError(0xdeadbeef);
    ret = ImmUnregisterWordW(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word1);
    CHECK(!ret && GetLastError() != 0xdeadbeef, "unregister a missing word %d %lu", ret, GetLastError());
    CHECK(count_words(hkl, L"nihon", 0, NULL) == 1, "one word of the reading is left");
    CHECK(ImmUnregisterWordA(hkl, "abc", IME_REGWORD_STYLE_EUDC, "xyz"), "unregister ANSI");
    CHECK(ImmUnregisterWordW(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word2), "unregister second word");
    CHECK(ImmUnregisterWordW(hkl, L"kana", IME_REGWORD_STYLE_USER_FIRST + 1, word3), "unregister third word");
    CHECK(count_words(hkl, NULL, 0, NULL) == 0, "dictionary empty at the end");
}

/* ---------------------------------------------------------------- the dictionary dialog */

static DWORD WINAPI dialog_driver(void *arg)
{
    HWND dlg = NULL;
    int i;

    for (i = 0; i < 200 && !(dlg = FindWindowW(L"#32770", L"Wine IME User Dictionary")); i++) Sleep(50);
    if (!dlg) return 1;
    CHECK(GetDlgItem(dlg, IDC_DICT_READING) && GetDlgItem(dlg, IDC_DICT_WORD) && GetDlgItem(dlg, IDC_DICT_REGISTER) &&
          GetDlgItem(dlg, IDC_DICT_LIST) && GetDlgItem(dlg, IDC_DICT_REMOVE), "dialog controls");
    {
        WCHAR text[32];
        GetWindowTextW(GetDlgItem(dlg, IDC_DICT_READING), text, ARRAY_SIZE(text));
        CHECK(!lstrcmpW(text, L"preread"), "reading preset from REGISTERWORD '%ls'", text);
        GetWindowTextW(GetDlgItem(dlg, IDC_DICT_WORD), text, ARRAY_SIZE(text));
        CHECK(!lstrcmpW(text, L"preword"), "word preset from REGISTERWORD '%ls'", text);
    }
    /* register what is in the edit boxes */
    SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDC_DICT_REGISTER, BN_CLICKED), (LPARAM)GetDlgItem(dlg, IDC_DICT_REGISTER));
    CHECK(SendMessageW(GetDlgItem(dlg, IDC_DICT_LIST), LB_GETCOUNT, 0, 0) == 1, "list has the word");
    SetWindowTextW(GetDlgItem(dlg, IDC_DICT_READING), L"second");
    SetWindowTextW(GetDlgItem(dlg, IDC_DICT_WORD), L"word2");
    SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDC_DICT_REGISTER, BN_CLICKED), (LPARAM)GetDlgItem(dlg, IDC_DICT_REGISTER));
    CHECK(SendMessageW(GetDlgItem(dlg, IDC_DICT_LIST), LB_GETCOUNT, 0, 0) == 2, "list has two words");
    /* remove the first one selected */
    SendMessageW(GetDlgItem(dlg, IDC_DICT_LIST), LB_SETCURSEL, 0, 0);
    SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDC_DICT_REMOVE, BN_CLICKED), (LPARAM)GetDlgItem(dlg, IDC_DICT_REMOVE));
    CHECK(SendMessageW(GetDlgItem(dlg, IDC_DICT_LIST), LB_GETCOUNT, 0, 0) == 1, "list after remove");
    SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
    return 0;
}

static void test_configure(HKL hkl)
{
    REGISTERWORDW word = { (WCHAR *)L"preread", (WCHAR *)L"preword" };
    HANDLE thread;
    BOOL ret;

    thread = CreateThread(NULL, 0, dialog_driver, NULL, 0, NULL);
    ret = ImmConfigureIMEW(hkl, NULL, IME_CONFIG_REGISTERWORD, &word);
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    CHECK(ret, "ImmConfigureIME register word dialog returns when closed");
    CHECK(count_words(hkl, NULL, 0, NULL) == 1, "one word is left from the dialog: %d", count_words(hkl, NULL, 0, NULL));
    CHECK(count_words(hkl, L"second", 0, L"word2") == 1, "the second word was registered by the dialog");
    ImmUnregisterWordW(hkl, L"second", IME_REGWORD_STYLE_USER_FIRST, L"word2");
    CHECK(!ImmConfigureIMEW(hkl, NULL, 9, NULL), "unknown mode");
    CHECK(!ImmConfigureIMEW(hkl, NULL, IME_CONFIG_REGISTERWORD, NULL), "register word dialog needs the REGISTERWORD data");
}

/* ---------------------------------------------------------------- escapes */

static void test_escape(HKL hkl, HIMC himc)
{
    WCHAR buf[100];
    char abuf[100];
    UINT query;
    LRESULT ret;

    query = IME_ESC_IME_NAME;
    CHECK(ImmEscapeW(hkl, himc, IME_ESC_QUERY_SUPPORT, &query) != 0, "IME_NAME supported");
    query = IME_ESC_HANJA_MODE;
    CHECK(ImmEscapeW(hkl, himc, IME_ESC_QUERY_SUPPORT, &query) == 0, "HANJA_MODE not supported");
    query = IME_ESC_QUERY_SUPPORT;
    CHECK(ImmEscapeW(hkl, himc, IME_ESC_QUERY_SUPPORT, &query) != 0, "QUERY_SUPPORT supported");
    CHECK(ImmEscapeW(hkl, himc, IME_ESC_QUERY_SUPPORT, NULL) == 0, "QUERY_SUPPORT without data");

    memset(buf, 0xcc, sizeof(buf));
    ret = ImmEscapeW(hkl, himc, IME_ESC_IME_NAME, buf);
    CHECK(ret != 0 && buf[0] && wcslen(buf) < 64, "IME name '%ls'", buf);
    ret = ImmEscapeA(hkl, himc, IME_ESC_IME_NAME, abuf);
    CHECK(ret != 0 && abuf[0], "ANSI IME name '%s'", abuf);

    ret = ImmEscapeW(hkl, himc, IME_ESC_SET_EUDC_DICTIONARY, (void *)L"C:\\eudc\\test.tte");
    CHECK(ret != 0, "set EUDC dictionary");
    memset(buf, 0xcc, sizeof(buf));
    ret = ImmEscapeW(hkl, himc, IME_ESC_GET_EUDC_DICTIONARY, buf);
    CHECK(ret != 0 && !lstrcmpW(buf, L"C:\\eudc\\test.tte"), "get EUDC dictionary '%ls'", buf);
    ret = ImmEscapeA(hkl, himc, IME_ESC_GET_EUDC_DICTIONARY, abuf);
    CHECK(ret != 0 && !strcmp(abuf, "C:\\eudc\\test.tte"), "ANSI get EUDC dictionary '%s'", abuf);
    {
        WCHAR longpath[100];
        int i;
        for (i = 0; i < 90; i++) longpath[i] = 'a';
        longpath[90] = 0;
        CHECK(ImmEscapeW(hkl, himc, IME_ESC_SET_EUDC_DICTIONARY, longpath) == 0, "path of 80 characters or more");
    }
    ImmEscapeW(hkl, himc, IME_ESC_SET_EUDC_DICTIONARY, (void *)L"");
    CHECK(ImmEscapeW(hkl, himc, IME_ESC_GETHELPFILENAME, buf) == 0, "no help file");
    CHECK(ImmEscapeW(hkl, himc, IME_ESC_MAX_KEY, buf) == 0, "MAX_KEY not supported");
    CHECK(ImmEscapeW(hkl, himc, 0x7fff, buf) == 0, "unknown escape");
    CHECK(ImmGetImeMenuItemsW(himc, IGIMIF_RIGHTMENU, IGIMII_CMODE, NULL, NULL, 0) == 0, "no IME menu items");
}

/* ---------------------------------------------------------------- composition, conversion, candidates */

static WCHAR *get_comp(HIMC himc, DWORD index, WCHAR *buf, int chars)
{
    LONG len = ImmGetCompositionStringW(himc, index, buf, chars * sizeof(WCHAR));
    if (len < 0) len = 0;
    buf[len / sizeof(WCHAR)] = 0;
    return buf;
}

static void test_composition(HWND hwnd, HIMC himc)
{
    WCHAR buf[64];
    BYTE attr[32];
    DWORD clause[8];
    LONG len;
    BOOL ret;

    ImmRegisterWordW(GetKeyboardLayout(0), L"nihon", IME_REGWORD_STYLE_USER_FIRST, word1);
    ImmRegisterWordW(GetKeyboardLayout(0), L"nihon", IME_REGWORD_STYLE_USER_FIRST, word2);

    /* SCS_SETSTR with a reading string */
    nmsgs = 0;
    ret = ImmSetCompositionStringW(himc, SCS_SETSTR, L"nihon", 5 * sizeof(WCHAR), L"nihon", 5 * sizeof(WCHAR));
    CHECK(ret, "SCS_SETSTR %lu", GetLastError());
    CHECK(!lstrcmpW(get_comp(himc, GCS_COMPSTR, buf, 63), L"nihon"), "composition string '%ls'", buf);
    CHECK(!lstrcmpW(get_comp(himc, GCS_COMPREADSTR, buf, 63), L"nihon"), "reading string '%ls'", buf);
    len = ImmGetCompositionStringW(himc, GCS_COMPATTR, attr, sizeof(attr));
    CHECK(len == 5 && attr[0] == ATTR_INPUT, "attributes %ld", len);
    len = ImmGetCompositionStringW(himc, GCS_COMPREADATTR, attr, sizeof(attr));
    CHECK(len == 5, "reading attributes %ld", len);
    len = ImmGetCompositionStringW(himc, GCS_COMPREADCLAUSE, clause, sizeof(clause));
    CHECK(len == 8 && clause[0] == 0 && clause[1] == 5, "reading clauses %ld", len);
    CHECK(find_msg(WM_IME_STARTCOMPOSITION, 0, 0) == 1, "WM_IME_STARTCOMPOSITION");
    CHECK(find_msg(WM_IME_COMPOSITION, 0, 0) >= 1 && (last_lparam(WM_IME_COMPOSITION) & GCS_COMPREADSTR), "composition message has GCS_COMPREADSTR %lx",
          (unsigned long)last_lparam(WM_IME_COMPOSITION));

    /* SCS_CHANGEATTR */
    nmsgs = 0;
    {
        BYTE new_attr[5] = { ATTR_TARGET_CONVERTED, ATTR_TARGET_CONVERTED, ATTR_CONVERTED, ATTR_CONVERTED, ATTR_INPUT };
        ret = ImmSetCompositionStringW(himc, SCS_CHANGEATTR, new_attr, sizeof(new_attr), NULL, 0);
        CHECK(ret, "SCS_CHANGEATTR");
        len = ImmGetCompositionStringW(himc, GCS_COMPATTR, attr, sizeof(attr));
        CHECK(len == 5 && !memcmp(attr, new_attr, 5), "changed attributes %ld", len);
        CHECK(!lstrcmpW(get_comp(himc, GCS_COMPSTR, buf, 63), L"nihon"), "string kept by the attribute change");
        CHECK(!lstrcmpW(get_comp(himc, GCS_COMPREADSTR, buf, 63), L"nihon"), "reading kept by the attribute change");
        CHECK(find_msg(WM_IME_COMPOSITION, 0, 0) == 1 && (last_lparam(WM_IME_COMPOSITION) & GCS_COMPATTR), "GCS_COMPATTR message");
        ret = ImmSetCompositionStringW(himc, SCS_CHANGEATTR, new_attr, 3, NULL, 0);
        CHECK(!ret, "attributes of the wrong count");
    }
    /* SCS_CHANGECLAUSE */
    {
        DWORD new_clause[3] = { 0, 2, 5 };
        DWORD bad_clause[3] = { 0, 2, 4 };
        DWORD bad_clause2[3] = { 1, 2, 5 };
        DWORD bad_clause3[3] = { 0, 3, 3 };
        ret = ImmSetCompositionStringW(himc, SCS_CHANGECLAUSE, new_clause, sizeof(new_clause), NULL, 0);
        CHECK(ret, "SCS_CHANGECLAUSE");
        len = ImmGetCompositionStringW(himc, GCS_COMPCLAUSE, clause, sizeof(clause));
        CHECK(len == 12 && clause[0] == 0 && clause[1] == 2 && clause[2] == 5, "changed clauses %ld", len);
        CHECK(!ImmSetCompositionStringW(himc, SCS_CHANGECLAUSE, bad_clause, sizeof(bad_clause), NULL, 0), "clause not ending at the length");
        CHECK(!ImmSetCompositionStringW(himc, SCS_CHANGECLAUSE, bad_clause2, sizeof(bad_clause2), NULL, 0), "clause not starting at 0");
        CHECK(!ImmSetCompositionStringW(himc, SCS_CHANGECLAUSE, bad_clause3, sizeof(bad_clause3), NULL, 0), "clauses that do not grow");
        len = ImmGetCompositionStringW(himc, GCS_COMPATTR, attr, sizeof(attr));
        CHECK(len == 5 && attr[0] == ATTR_TARGET_CONVERTED, "attributes kept by the clause change");
    }

    /* conversion */
    nmsgs = 0;
    ret = ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
    CHECK(ret, "CPS_CONVERT");
    get_comp(himc, GCS_COMPSTR, buf, 63);
    CHECK(!lstrcmpW(buf, word1) || !lstrcmpW(buf, word2), "converted string '%ls'", buf);
    CHECK(!lstrcmpW(get_comp(himc, GCS_COMPREADSTR, buf, 63), L"nihon"), "reading of the conversion '%ls'", buf);
    len = ImmGetCompositionStringW(himc, GCS_COMPATTR, attr, sizeof(attr));
    CHECK(len == 2 && attr[0] == ATTR_CONVERTED && attr[1] == ATTR_CONVERTED, "converted attributes %ld", len);
    CHECK(find_msg(WM_IME_COMPOSITION, 0, 0) >= 1 && (last_lparam(WM_IME_COMPOSITION) & GCS_COMPSTR), "conversion message");

    /* the candidates of the reading */
    nmsgs = 0;
    ret = ImmNotifyIME(himc, NI_OPENCANDIDATE, 0, 0);
    CHECK(ret, "NI_OPENCANDIDATE");
    CHECK(find_msg(WM_IME_NOTIFY, IMN_OPENCANDIDATE, 1) == 1 && last_lparam(WM_IME_NOTIFY) == 1, "IMN_OPENCANDIDATE %lx",
          (unsigned long)last_lparam(WM_IME_NOTIFY));
    {
        DWORD count = 0, size, got;
        BYTE buffer[256];
        CANDIDATELIST *list = (CANDIDATELIST *)buffer;

        ImmGetCandidateListCountW(himc, &count);
        CHECK(count == 1, "one candidate list %lu", count);
        size = ImmGetCandidateListW(himc, 0, NULL, 0);
        CHECK(size > 0 && size <= sizeof(buffer), "candidate list size %lu", size);
        got = ImmGetCandidateListW(himc, 0, list, sizeof(buffer));
        CHECK(got == size && list->dwCount == 2, "candidate list %lu %lu", got, list->dwCount);

        nmsgs = 0;
        ret = ImmNotifyIME(himc, NI_SELECTCANDIDATESTR, 0, 1);
        CHECK(ret, "NI_SELECTCANDIDATESTR");
        got = ImmGetCandidateListW(himc, 0, list, sizeof(buffer));
        CHECK(got && list->dwSelection == 1, "selection %lu", list->dwSelection);
        CHECK(!lstrcmpW(get_comp(himc, GCS_COMPSTR, buf, 63), (WCHAR *)(buffer + list->dwOffset[1])), "selected candidate in the composition '%ls'", buf);
        CHECK(find_msg(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1) == 1, "IMN_CHANGECANDIDATE");
        CHECK(!ImmNotifyIME(himc, NI_SELECTCANDIDATESTR, 0, 2), "candidate out of range");
        CHECK(!ImmNotifyIME(himc, NI_SELECTCANDIDATESTR, 1, 0), "list not open");

        nmsgs = 0;
        CHECK(ImmNotifyIME(himc, NI_SETCANDIDATE_PAGESIZE, 0, 5), "page size");
        CHECK(ImmNotifyIME(himc, NI_SETCANDIDATE_PAGESTART, 0, 1), "page start");
        got = ImmGetCandidateListW(himc, 0, list, sizeof(buffer));
        CHECK(list->dwPageSize == 5 && list->dwPageStart == 1, "page %lu %lu", list->dwPageStart, list->dwPageSize);
        CHECK(!ImmNotifyIME(himc, NI_SETCANDIDATE_PAGESTART, 0, 2), "page start out of range");
        CHECK(!ImmNotifyIME(himc, NI_SETCANDIDATE_PAGESIZE, 4, 5), "page size of list 4");
        CHECK(!ImmNotifyIME(himc, NI_SETCANDIDATE_PAGESIZE, 0, 0), "page size 0");
        CHECK(ImmNotifyIME(himc, NI_CHANGECANDIDATELIST, 0, 0), "NI_CHANGECANDIDATELIST");
        nmsgs = 0;
        ret = ImmNotifyIME(himc, NI_CLOSECANDIDATE, 0, 0);
        CHECK(ret, "NI_CLOSECANDIDATE");
        CHECK(find_msg(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1) == 1, "IMN_CLOSECANDIDATE");
        CHECK(!ImmNotifyIME(himc, NI_CLOSECANDIDATE, 0, 0), "close a closed list");
        ImmGetCandidateListCountW(himc, &count);
        CHECK(count == 0, "no candidate list %lu", count);
    }

    /* revert gives the reading back */
    nmsgs = 0;
    ret = ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_REVERT, 0);
    CHECK(ret, "CPS_REVERT");
    CHECK(!lstrcmpW(get_comp(himc, GCS_COMPSTR, buf, 63), L"nihon"), "reverted string '%ls'", buf);
    CHECK(ImmGetCompositionStringW(himc, GCS_COMPREADSTR, buf, sizeof(buf)) == 0, "reading string gone after the revert");
    len = ImmGetCompositionStringW(himc, GCS_COMPATTR, attr, sizeof(attr));
    CHECK(len == 5 && attr[0] == ATTR_INPUT, "attributes after the revert %ld", len);
    CHECK(ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_REVERT, 0), "revert of the unconverted string is fine");

    /* a reading with no word converts to itself */
    ImmSetCompositionStringW(himc, SCS_SETSTR, L"zzz", 3 * sizeof(WCHAR), NULL, 0);
    CHECK(ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CONVERT, 0), "convert a reading without word");
    CHECK(!lstrcmpW(get_comp(himc, GCS_COMPSTR, buf, 63), L"zzz"), "identity conversion '%ls'", buf);
    CHECK(!ImmNotifyIME(himc, NI_OPENCANDIDATE, 0, 0), "no candidates for the reading");

    /* complete takes the converted string as the result */
    ImmSetCompositionStringW(himc, SCS_SETSTR, L"nihon", 5 * sizeof(WCHAR), NULL, 0);
    ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
    get_comp(himc, GCS_COMPSTR, buf, 63);
    {
        WCHAR expected[8];
        lstrcpyW(expected, buf);
        nmsgs = 0;
        result_str[0] = 0;
        CHECK(ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0), "CPS_COMPLETE");
        CHECK(!lstrcmpW(result_str, expected), "result string '%ls'", result_str);
    }
    ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);

    ImmUnregisterWordW(GetKeyboardLayout(0), L"nihon", IME_REGWORD_STYLE_USER_FIRST, word1);
    ImmUnregisterWordW(GetKeyboardLayout(0), L"nihon", IME_REGWORD_STYLE_USER_FIRST, word2);
    CHECK(!ImmNotifyIME(himc, NI_IMEMENUSELECTED, 1, 0), "IME menu selection (no menu)");
}

/* ---------------------------------------------------------------- WM_IME_CONTROL and the hot keys */

static void test_control(HWND hwnd, HIMC himc)
{
    HWND ime = ImmGetDefaultIMEWnd(hwnd);
    CANDIDATEFORM cf = {0}, cf2 = {0};
    COMPOSITIONFORM comp = {0}, comp2 = {0};
    LOGFONTW font = {0}, font2 = {0};
    POINTS pts;
    LRESULT ret;

    CHECK(ime != NULL, "default IME window");
    if (!ime) return;

    cf.dwIndex = 1;
    cf.dwStyle = CFS_CANDIDATEPOS;
    cf.ptCurrentPos.x = 11;
    cf.ptCurrentPos.y = 22;
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_SETCANDIDATEPOS, (LPARAM)&cf);
    CHECK(ret == 0, "IMC_SETCANDIDATEPOS %Id", ret);
    cf2.dwIndex = 1;
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_GETCANDIDATEPOS, (LPARAM)&cf2);
    CHECK(ret == 0 && cf2.dwStyle == CFS_CANDIDATEPOS && cf2.ptCurrentPos.x == 11 && cf2.ptCurrentPos.y == 22, "IMC_GETCANDIDATEPOS %Id %lu %ld,%ld",
          ret, cf2.dwStyle, cf2.ptCurrentPos.x, cf2.ptCurrentPos.y);
    memset(&cf2, 0, sizeof(cf2));
    cf2.dwIndex = 1;
    CHECK(ImmGetCandidateWindow(himc, 1, &cf2) && cf2.ptCurrentPos.x == 11, "the candidate window is the one of the input context");
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_SETCANDIDATEPOS, 0);
    CHECK(ret != 0, "IMC_SETCANDIDATEPOS without data");
    cf2.dwIndex = 9;
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_GETCANDIDATEPOS, (LPARAM)&cf2);
    CHECK(ret != 0, "IMC_GETCANDIDATEPOS of a bad index");

    comp.dwStyle = CFS_POINT;
    comp.ptCurrentPos.x = 5;
    comp.ptCurrentPos.y = 6;
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_SETCOMPOSITIONWINDOW, (LPARAM)&comp);
    CHECK(ret == 0, "IMC_SETCOMPOSITIONWINDOW");
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_GETCOMPOSITIONWINDOW, (LPARAM)&comp2);
    CHECK(ret == 0 && comp2.dwStyle == CFS_POINT && comp2.ptCurrentPos.x == 5 && comp2.ptCurrentPos.y == 6, "IMC_GETCOMPOSITIONWINDOW %Id", ret);

    font.lfHeight = -17;
    lstrcpyW(font.lfFaceName, L"Arial");
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_SETCOMPOSITIONFONT, (LPARAM)&font);
    CHECK(ret == 0, "IMC_SETCOMPOSITIONFONT");
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_GETCOMPOSITIONFONT, (LPARAM)&font2);
    CHECK(ret == 0 && font2.lfHeight == -17 && !lstrcmpW(font2.lfFaceName, L"Arial"), "IMC_GETCOMPOSITIONFONT %Id %ld", ret, font2.lfHeight);

    pts.x = 40;
    pts.y = 50;
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_SETSTATUSWINDOWPOS, (LPARAM)&pts);
    CHECK(ret == 0, "IMC_SETSTATUSWINDOWPOS %Id", ret);
    ret = SendMessageW(ime, WM_IME_CONTROL, IMC_GETSTATUSWINDOWPOS, 0);
    CHECK(LOWORD(ret) == 40 && HIWORD(ret) == 50, "IMC_GETSTATUSWINDOWPOS %Ix", ret);
    {
        POINT pt = {0};
        CHECK(ImmGetStatusWindowPos(himc, &pt) && pt.x == 40 && pt.y == 50, "status position of the input context");
    }
    CHECK(SendMessageW(ime, WM_IME_CONTROL, IMC_OPENSTATUSWINDOW, 0) == 0, "IMC_OPENSTATUSWINDOW");
    CHECK(SendMessageW(ime, WM_IME_CONTROL, IMC_CLOSESTATUSWINDOW, 0) == 0, "IMC_CLOSESTATUSWINDOW");
    CHECK(SendMessageW(ime, WM_IME_CONTROL, 0x7000, 0) != 0, "unknown command");
}

static void test_hotkeys(HWND hwnd, HIMC himc)
{
    DWORD conv, sent;
    BOOL open;

    ImmSetOpenStatus(himc, TRUE);
    CHECK(ImmGetOpenStatus(himc), "open");
    CHECK(ImmSimulateHotKey(hwnd, IME_JHOTKEY_CLOSE_OPEN), "IME_JHOTKEY_CLOSE_OPEN");
    CHECK(!ImmGetOpenStatus(himc), "closed by the hot key");
    CHECK(ImmSimulateHotKey(hwnd, IME_CHOTKEY_IME_NONIME_TOGGLE), "IME_CHOTKEY_IME_NONIME_TOGGLE");
    open = ImmGetOpenStatus(himc);
    CHECK(open, "opened by the hot key");
    ImmGetConversionStatus(himc, &conv, &sent);
    CHECK(ImmSimulateHotKey(hwnd, IME_CHOTKEY_SHAPE_TOGGLE), "shape toggle");
    {
        DWORD conv2;
        ImmGetConversionStatus(himc, &conv2, &sent);
        CHECK((conv2 ^ conv) == IME_CMODE_FULLSHAPE, "full shape toggled %lx -> %lx", conv, conv2);
        ImmSimulateHotKey(hwnd, IME_KHOTKEY_SHAPE_TOGGLE);
        ImmGetConversionStatus(himc, &conv2, &sent);
        CHECK(conv2 == conv, "toggled back");
    }
    CHECK(ImmSimulateHotKey(hwnd, IME_CHOTKEY_SYMBOL_TOGGLE), "symbol toggle");
    {
        DWORD conv2;
        ImmGetConversionStatus(himc, &conv2, &sent);
        CHECK((conv2 ^ conv) == IME_CMODE_SYMBOL, "symbol mode toggled");
        ImmSimulateHotKey(hwnd, IME_THOTKEY_SYMBOL_TOGGLE);
    }
    ImmSetConversionStatus(himc, IME_CMODE_NATIVE | IME_CMODE_HANJACONVERT, 0);
    CHECK(ImmSimulateHotKey(hwnd, IME_KHOTKEY_ENGLISH), "IME_KHOTKEY_ENGLISH");
    ImmGetConversionStatus(himc, &conv, &sent);
    CHECK(!(conv & (IME_CMODE_NATIVE | IME_CMODE_HANJACONVERT)), "switched to English: %lx", conv);
    ImmSetConversionStatus(himc, 0, 0);
    CHECK(ImmSimulateHotKey(hwnd, IME_KHOTKEY_HANJACONVERT), "IME_KHOTKEY_HANJACONVERT");
    ImmGetConversionStatus(himc, &conv, &sent);
    CHECK(conv & IME_CMODE_HANJACONVERT, "Hanja conversion mode");
    ImmSetConversionStatus(himc, 0, 0);
    SetLastError(0xdeadbeef);
    CHECK(!ImmSimulateHotKey(hwnd, IME_ITHOTKEY_RECONVERTSTRING) && GetLastError() == ERROR_INVALID_PARAMETER, "reconversion hot key %lu", GetLastError());
    SetLastError(0xdeadbeef);
    CHECK(!ImmSimulateHotKey((HWND)0x1234, IME_JHOTKEY_CLOSE_OPEN) && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "bad window %lu", GetLastError());
    {
        HMODULE imm = GetModuleHandleA("imm32.dll");
        BOOL (WINAPI *disable_tsf)(DWORD) = (void *)GetProcAddress(imm, "ImmDisableTextFrameService");
        BOOL (WINAPI *disable_legacy)(void) = (void *)GetProcAddress(imm, "ImmDisableLegacyIME");
        CHECK(disable_tsf && disable_tsf(0) == FALSE, "ImmDisableTextFrameService is not available");
        CHECK(disable_legacy && disable_legacy() == TRUE, "ImmDisableLegacyIME");
    }
}

/* ---------------------------------------------------------------- a layout without an IME */

static void test_no_ime(HKL hkl)
{
    STYLEBUFW style;
    WCHAR buf[40];
    UINT query = IME_ESC_IME_NAME;
    struct enum_data d = {0};

    SetLastError(0xdeadbeef);
    CHECK(!ImmRegisterWordW(hkl, L"nihon", IME_REGWORD_STYLE_USER_FIRST, word1), "register without IME");
    CHECK(ImmGetRegisterWordStyleW(hkl, 0, &style) == 0, "styles without IME");
    CHECK(ImmGetConversionListW(hkl, 0, L"nihon", NULL, 0, GCL_CONVERSION) == 0, "conversion list without IME");
    CHECK(ImmEscapeW(hkl, 0, IME_ESC_QUERY_SUPPORT, &query) == 0, "escape without IME");
    CHECK(ImmEscapeW(hkl, 0, IME_ESC_IME_NAME, buf) == 0, "IME name without IME");
    d.stop_at = -1;
    CHECK(ImmEnumRegisterWordW(hkl, enum_proc, NULL, 0, NULL, &d) == 0 && d.count == 0, "enumeration without IME");
    CHECK(!ImmConfigureIMEW(hkl, NULL, IME_CONFIG_GENERAL, NULL), "configuration without IME");
    CHECK(GetLastError() == 0xdeadbeef, "last error untouched %lu", GetLastError());
}

int main(void)
{
    WNDCLASSW wc = {0};
    HKL original = GetKeyboardLayout(0), ime_hkl;
    HWND hwnd;
    HIMC himc;

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"fastimm";
    RegisterClassW(&wc);

    test_no_ime(original);

    {
        HIMCC imcc = ImmCreateIMCC(32);
        void *p = ImmLockIMCC(imcc);
        CHECK(p != NULL, "lock IMCC");
        ImmUnlockIMCC(imcc);
        CHECK(ImmGetIMCCLockCount(imcc) == 0, "IMCC unlocked");
        ImmDestroyIMCC(imcc);
        CHECK(ImmLockIMCC(imcc) == NULL, "locking a destroyed IMCC");
        CHECK(ImmUnlockIMCC(imcc) == FALSE, "unlocking a destroyed IMCC");
    }

    ime_hkl = LoadKeyboardLayoutW(L"E0010411", KLF_ACTIVATE);
    CHECK(ime_hkl != NULL && ((ULONG_PTR)ime_hkl & 0xf0000000) == 0xe0000000, "IME layout loaded %p", ime_hkl);
    CHECK(GetKeyboardLayout(0) == ime_hkl, "IME layout active");
    if (ime_hkl)
    {
        hwnd = CreateWindowW(L"fastimm", L"imm", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, wc.hInstance, NULL);
        himc = ImmGetContext(hwnd);
        CHECK(himc != NULL, "input context");
        test_dictionary(ime_hkl);
        test_configure(ime_hkl);
        if (himc)
        {
            test_escape(ime_hkl, himc);
            test_composition(hwnd, himc);
            test_control(hwnd, himc);
            test_hotkeys(hwnd, himc);
            ImmReleaseContext(hwnd, himc);
        }
        DestroyWindow(hwnd);
        ActivateKeyboardLayout(original, 0);
    }
    test_no_ime(original);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    fflush(stdout);
    return fails;
}
