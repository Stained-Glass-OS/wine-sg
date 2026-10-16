/* Probe for patches/sg/5224 and 5225: OpenType features of a font loaded from memory.
 * GSUB ChainContextSubst format 1 (a lookahead rule, a backtrack rule, an input-sequence rule),
 * GPOS ContextPos formats 1 and 3 and ChainContextPos format 1 (advance changes seen with
 * ScriptPlaceOpenType), ScriptGetFontAlternateGlyphs (also through an extension lookup) and the
 * filtering of ScriptGetFontFeatureTags by the shaping engine.  The tables are built here from the
 * OpenType specification's layouts.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <usp10.h>
#include <stdio.h>
#include <string.h>
#include "sgfont.h"

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define TAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

struct buf { unsigned char d[2048]; unsigned n; };
static void p16(struct buf *b, unsigned v) { b->d[b->n++] = v >> 8; b->d[b->n++] = v; }
static void p32(struct buf *b, unsigned v) { p16(b, v >> 16); p16(b, v & 0xffff); }
static void ptag(struct buf *b, const char *t) { memcpy(b->d + b->n, t, 4); b->n += 4; }
static void set16(struct buf *b, unsigned at, unsigned v) { b->d[at] = v >> 8; b->d[at + 1] = v; }
static unsigned here(struct buf *b) { return b->n; }
/* a coverage table (format 1) for the glyph ids; returns its offset */
static unsigned coverage(struct buf *b, int n, const unsigned *glyphs)
{
    unsigned at = here(b);
    int i;
    p16(b, 1); p16(b, n);
    for (i = 0; i < n; i++) p16(b, glyphs[i]);
    return at;
}

/* ---- the GSUB table ---- */
static unsigned char *build_gsub(unsigned *len)
{
    static struct buf b;
    unsigned lookup_off[8], sub_off[8], i, t_script, t_feat, t_look, ll, fl, sl;
    unsigned fix_script, fix_feat, fix_look;
    unsigned cov[8];
    unsigned c1[] = {1}, c3[] = {3}, c2[] = {2}, c5[] = {5}, ch[] = {1, 3, 4};

    memset(&b, 0, sizeof(b));
    p32(&b, 0x00010000); fix_script = here(&b); p16(&b, 0); fix_feat = here(&b); p16(&b, 0); fix_look = here(&b); p16(&b, 0);
    /* script list: arab and latn share one script table with the default language system (features 0-4) */
    sl = here(&b); set16(&b, fix_script, sl);
    p16(&b, 2); ptag(&b, "arab"); p16(&b, 14); ptag(&b, "latn"); p16(&b, 14);
    p16(&b, 4); p16(&b, 0);                       /* Script: DefaultLangSys at 4, no other */
    p16(&b, 0); p16(&b, 0xffff); p16(&b, 5); p16(&b, 0); p16(&b, 1); p16(&b, 2); p16(&b, 3); p16(&b, 4);   /* LangSys */
    /* feature list: fina, init, liga -> lookup 3, salt -> lookup 5, ss01 -> lookup 6 */
    fl = here(&b); set16(&b, fix_feat, fl);
    p16(&b, 5);
    ptag(&b, "fina"); p16(&b, 2 + 5 * 6);
    ptag(&b, "init"); p16(&b, 2 + 5 * 6 + 6);
    ptag(&b, "liga"); p16(&b, 2 + 5 * 6 + 12);
    ptag(&b, "salt"); p16(&b, 2 + 5 * 6 + 18);
    ptag(&b, "ss01"); p16(&b, 2 + 5 * 6 + 24);
    p16(&b, 0); p16(&b, 1); p16(&b, 3);
    p16(&b, 0); p16(&b, 1); p16(&b, 3);
    p16(&b, 0); p16(&b, 1); p16(&b, 3);
    p16(&b, 0); p16(&b, 1); p16(&b, 5);
    p16(&b, 0); p16(&b, 1); p16(&b, 6);
    /* lookup list: 7 lookups; each lookup is written after the offsets array */
    ll = here(&b); set16(&b, fix_look, ll);
    p16(&b, 7);
    for (i = 0; i < 7; i++) p16(&b, 0);
    (void)t_script; (void)t_feat; (void)t_look; (void)cov; (void)sub_off;

#define LOOKUP_START(idx, type, nsub) do { lookup_off[idx] = here(&b); set16(&b, ll + 2 + 2 * (idx), lookup_off[idx] - ll); \
    p16(&b, type); p16(&b, 0); p16(&b, nsub); p16(&b, 8); } while (0)
    /* 0..2, 4: single substitution format 2: A->30, C->31, B->32, E->33 */
    {
        static const struct { unsigned idx, from, to; } singles[] = { {0, 1, 30}, {1, 3, 31}, {2, 2, 32}, {4, 5, 33} };
        for (i = 0; i < 4; i++)
        {
            unsigned st, covat;
            LOOKUP_START(singles[i].idx, 1, 1);
            st = here(&b);
            p16(&b, 2); p16(&b, 0); p16(&b, 1); p16(&b, singles[i].to);
            covat = here(&b);
            set16(&b, st + 2, covat - st);
            p16(&b, 1); p16(&b, 1); p16(&b, singles[i].from);
        }
    }
    /* 3: chaining context substitution format 1 */
    {
        unsigned st, ruleset_at[3], rule_at[3], covat, j;
        LOOKUP_START(3, 6, 1);
        st = here(&b);
        p16(&b, 1); p16(&b, 0); p16(&b, 3); p16(&b, 0); p16(&b, 0); p16(&b, 0);
        for (j = 0; j < 3; j++)
        {
            ruleset_at[j] = here(&b);
            set16(&b, st + 6 + 2 * j, ruleset_at[j] - st);
            p16(&b, 1); p16(&b, 4);                           /* one rule at +4 */
            rule_at[j] = here(&b);
            if (j == 0)       /* A: lookahead B, lookup 0 at 0 */
            { p16(&b, 0); p16(&b, 1); p16(&b, 1); p16(&b, 2); p16(&b, 1); p16(&b, 0); p16(&b, 0); }
            else if (j == 1)  /* C: backtrack B, lookup 1 at 0 */
            { p16(&b, 1); p16(&b, 2); p16(&b, 1); p16(&b, 0); p16(&b, 1); p16(&b, 0); p16(&b, 1); }
            else              /* D: input E (2 glyphs), lookup 4 at 1 */
            { p16(&b, 0); p16(&b, 2); p16(&b, 5); p16(&b, 0); p16(&b, 1); p16(&b, 1); p16(&b, 4); }
        }
        covat = coverage(&b, 3, ch);
        set16(&b, st + 2, covat - st);
    }
    /* 5: alternate substitution A -> {34,35,36}; 6: extension lookup wrapping B -> {37,38} */
    {
        unsigned st, covat;
        LOOKUP_START(5, 3, 1);
        st = here(&b);
        p16(&b, 1); p16(&b, 0); p16(&b, 1); p16(&b, 8);
        covat = coverage(&b, 1, c1);
        set16(&b, st + 2, covat - st);
        /* alternate set at +8: placed after the coverage table */
        set16(&b, st + 6, here(&b) - st);
        p16(&b, 3); p16(&b, 34); p16(&b, 35); p16(&b, 36);
    }
    {
        unsigned ext, st, covat;
        LOOKUP_START(6, 7, 1);
        ext = here(&b);
        p16(&b, 1); p16(&b, 3); p32(&b, 8);
        st = here(&b);
        p16(&b, 1); p16(&b, 0); p16(&b, 1); p16(&b, 8);
        covat = coverage(&b, 1, c2);
        set16(&b, st + 2, covat - st);
        set16(&b, st + 6, here(&b) - st);
        p16(&b, 2); p16(&b, 37); p16(&b, 38);
        (void)ext;
    }
    (void)c3; (void)c5; (void)ch;
    *len = here(&b);
    return b.d;
}

/* ---- the GPOS table ---- */
static unsigned char *build_gpos(unsigned *len)
{
    static struct buf b;
    unsigned fix_script, fix_feat, fix_look, ll, i;
    unsigned cA[] = {6}, cC[] = {9}, cE[] = {11}, cD[] = {10};

    memset(&b, 0, sizeof(b));
    p32(&b, 0x00010000); fix_script = here(&b); p16(&b, 0); fix_feat = here(&b); p16(&b, 0); fix_look = here(&b); p16(&b, 0);
    set16(&b, fix_script, here(&b));
    p16(&b, 1); ptag(&b, "latn"); p16(&b, 8);
    p16(&b, 4); p16(&b, 0);
    p16(&b, 0); p16(&b, 0xffff); p16(&b, 1); p16(&b, 0);
    set16(&b, fix_feat, here(&b));
    p16(&b, 1); ptag(&b, "kern"); p16(&b, 8);
    p16(&b, 0); p16(&b, 4); p16(&b, 2); p16(&b, 3); p16(&b, 4); p16(&b, 6);       /* lookups 2, 3, 4, 6 */
    ll = here(&b); set16(&b, fix_look, ll);
    p16(&b, 8);
    for (i = 0; i < 8; i++) p16(&b, 0);

#define PLOOKUP(idx, type) do { set16(&b, ll + 2 + 2 * (idx), here(&b) - ll); p16(&b, type); p16(&b, 0); p16(&b, 1); p16(&b, 8); } while (0)
    /* 0: single pos A +100, 1: C +50, 5: E +70  (XAdvance) */
    {
        static const struct { unsigned idx, glyph, adv; } singles[] = { {0, 6, 100}, {1, 9, 50}, {5, 11, 70}, {7, 12, 30} };
        for (i = 0; i < 4; i++)
        {
            unsigned st, covat;
            unsigned g[1];
            g[0] = singles[i].glyph;
            PLOOKUP(singles[i].idx, 1);
            st = here(&b);
            p16(&b, 1); p16(&b, 0); p16(&b, 0x0004); p16(&b, singles[i].adv);
            covat = coverage(&b, 1, g);
            set16(&b, st + 2, covat - st);
        }
    }
    /* 2: context pos format 1: A followed by B -> lookup 0 at 0 */
    {
        unsigned st, setat;
        PLOOKUP(2, 7);
        st = here(&b);
        p16(&b, 1); p16(&b, 0); p16(&b, 1); p16(&b, 0);
        set16(&b, st + 2, coverage(&b, 1, cA) - st);
        setat = here(&b);
        set16(&b, st + 6, setat - st);
        p16(&b, 1); p16(&b, 4);
        p16(&b, 2); p16(&b, 1); p16(&b, 7); p16(&b, 0); p16(&b, 0);
    }
    /* 3: chaining context pos format 1: C after B -> lookup 1 at 0 */
    {
        unsigned st, setat;
        PLOOKUP(3, 8);
        st = here(&b);
        p16(&b, 1); p16(&b, 0); p16(&b, 1); p16(&b, 0);
        set16(&b, st + 2, coverage(&b, 1, cC) - st);
        setat = here(&b);
        set16(&b, st + 6, setat - st);
        p16(&b, 1); p16(&b, 4);
        p16(&b, 1); p16(&b, 8);      /* backtrack: H */
        p16(&b, 1);                  /* input glyph count (C only) */
        p16(&b, 0);                  /* lookahead count */
        p16(&b, 1); p16(&b, 0); p16(&b, 1);   /* 1 record: seq 0, lookup 1 */
    }
    /* 4: context pos format 3: D then E -> lookup 5 at 1 */
    {
        unsigned st;
        PLOOKUP(4, 7);
        st = here(&b);
        p16(&b, 3); p16(&b, 2); p16(&b, 1); p16(&b, 0); p16(&b, 0);
        p16(&b, 1); p16(&b, 5);      /* record: seq 1, lookup 5 */
        set16(&b, st + 6, coverage(&b, 1, cD) - st);
        set16(&b, st + 8, coverage(&b, 1, cE) - st);
    }
    /* 6: chaining context pos format 2: L (input class 1) after M (backtrack class 1) -> lookup 7 at 0 */
    {
        unsigned st, cd, sets, cov[1] = {12};
        PLOOKUP(6, 8);
        st = here(&b);
        p16(&b, 2); p16(&b, 0); p16(&b, 0); p16(&b, 0); p16(&b, 0); p16(&b, 2); p16(&b, 0); p16(&b, 0);
        set16(&b, st + 2, coverage(&b, 1, cov) - st);
        cd = here(&b); set16(&b, st + 4, cd - st);       /* backtrack classes: M = 1 */
        p16(&b, 1); p16(&b, 13); p16(&b, 1); p16(&b, 1);
        cd = here(&b); set16(&b, st + 6, cd - st);       /* input classes: L = 1 */
        p16(&b, 1); p16(&b, 12); p16(&b, 1); p16(&b, 1);
        cd = here(&b); set16(&b, st + 8, cd - st);       /* lookahead classes: none */
        p16(&b, 1); p16(&b, 0); p16(&b, 0);
        sets = here(&b); set16(&b, st + 12 + 2, sets - st);   /* class 1 set */
        p16(&b, 1); p16(&b, 4);
        p16(&b, 1); p16(&b, 1);       /* backtrack: class 1 */
        p16(&b, 1);                   /* input count */
        p16(&b, 0);                   /* lookahead count */
        p16(&b, 1); p16(&b, 0); p16(&b, 7);
    }
    (void)cC;
    *len = here(&b);
    return b.d;
}

static HDC hdc;
static SCRIPT_CACHE sc;

static HRESULT shape(const WCHAR *text, WORD *glyphs, int *nglyphs, int *advances)
{
    SCRIPT_ITEM items[4];
    int nitems, n = lstrlenW(text), i;
    WORD clust[16];
    SCRIPT_VISATTR vis[16];
    SCRIPT_CONTROL ctl = {0};
    SCRIPT_STATE st = {0};
    OPENTYPE_FEATURE_RECORD feat = {0};
    HRESULT hr;
    GOFFSET off[16];
    SCRIPT_CHARPROP cp[16];
    SCRIPT_GLYPHPROP gp[16];

    hr = ScriptItemize(text, n, 4, &ctl, &st, items, &nitems);
    if (hr) return hr;
    hr = ScriptShapeOpenType(hdc, &sc, &items[0].a, TAG('l','a','t','n'), 0, NULL, NULL, 0, text, n, 16, clust, cp, glyphs, gp, nglyphs);
    (void)feat; (void)vis; (void)i;
    if (hr || !advances) return hr;
    return ScriptPlaceOpenType(hdc, &sc, &items[0].a, TAG('l','a','t','n'), 0, NULL, NULL, 0, text, clust, cp, n, glyphs, gp, *nglyphs, advances, off, NULL);
}

int main(void)
{
    unsigned gsub_len, gpos_len, font_len;
    unsigned char *gsub = build_gsub(&gsub_len), *gpos, *font;
    HANDLE res;
    DWORD count = 0;
    LOGFONTA lf = {0};
    HFONT hf;
    WORD g[16];
    int ng, adv[16], adv2[16], i;
    static const struct { const WCHAR *text; int n; WORD want[4]; const char *what; } gsub_tests[] = {
        { L"AB", 2, {30, 2}, "lookahead rule: A before B" },
        { L"AC", 2, {1, 3}, "no rule: A before C" },
        { L"BC", 2, {2, 31}, "backtrack rule: C after B" },
        { L"CB", 2, {3, 2}, "no rule: C before B" },
        { L"DE", 2, {4, 33}, "input sequence rule: E after D" },
        { L"DF", 2, {4, 6}, "no rule: D before F" },
        { L"ABC", 3, {30, 2, 31}, "both rules in a row" },
    };

    setvbuf(stdout, NULL, _IONBF, 0);
    gpos = build_gpos(&gpos_len);
    sgf_gpos = gpos; sgf_gpos_len = gpos_len;
    font = sgf_build("SgOtProbe", gsub, gsub_len, &font_len);
    res = AddFontMemResourceEx(font, font_len, NULL, &count);
    check(res != NULL, "font loaded");
    strcpy(lf.lfFaceName, "SgOtProbe");
    lf.lfHeight = -100; lf.lfCharSet = DEFAULT_CHARSET;
    hf = CreateFontIndirectA(&lf);
    hdc = CreateCompatibleDC(NULL);
    SelectObject(hdc, hf);

    for (i = 0; i < (int)(sizeof(gsub_tests) / sizeof(*gsub_tests)); i++)
    {
        HRESULT hr = shape(gsub_tests[i].text, g, &ng, NULL);
        int k, same = 1;
        for (k = 0; k < gsub_tests[i].n; k++) if (g[k] != gsub_tests[i].want[k]) same = 0;
        check(!hr && ng == gsub_tests[i].n && same, "GSUB chain format 1, %s: %d glyphs %u %u %u (hr %#lx)", gsub_tests[i].what, ng, g[0], g[1], g[2], hr);
    }

    /* GPOS: the advance of the first/second glyph changes only when the rule matches */
    {
        int base_a, base_c, base_e;
        shape(L"FZ", g, &ng, adv);   base_a = adv[0];
        shape(L"ZI", g, &ng, adv);   base_c = adv[1];
        shape(L"ZK", g, &ng, adv);   base_e = adv[1];
        shape(L"FG", g, &ng, adv2);
        check(adv2[0] > base_a, "ContextPos format 1: F before G is wider (%d vs %d)", adv2[0], base_a);
        shape(L"HI", g, &ng, adv2);
        check(adv2[1] > base_c, "ChainContextPos format 1: I after H is wider (%d vs %d)", adv2[1], base_c);
        shape(L"IH", g, &ng, adv2);
        check(adv2[0] == base_c, "ChainContextPos format 1: I before H is not wider (%d vs %d)", adv2[0], base_c);
        shape(L"JK", g, &ng, adv2);
        check(adv2[1] > base_e, "ContextPos format 3: K after J is wider (%d vs %d)", adv2[1], base_e);
        shape(L"KK", g, &ng, adv2);
        check(adv2[1] == base_e, "ContextPos format 3: K after K is not (%d vs %d)", adv2[1], base_e);
        shape(L"ZL", g, &ng, adv);
        shape(L"ML", g, &ng, adv2);
        check(adv2[1] > adv[1], "ChainContextPos format 2: L after M is wider (%d vs %d)", adv2[1], adv[1]);
        shape(L"LM", g, &ng, adv2);
        check(adv2[0] == adv[1], "ChainContextPos format 2: L before M is not (%d vs %d)", adv2[0], adv[1]);
    }

    /* alternate glyphs */
    {
        WORD alt[8] = {0};
        int cnt = -1;
        HRESULT hr;
        SCRIPT_CACHE sc2 = NULL;

        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('l','a','t','n'), 0, TAG('s','a','l','t'), 1, 8, alt, &cnt);
        check(hr == S_OK && cnt == 4 && alt[0] == 1 && alt[1] == 34 && alt[2] == 35 && alt[3] == 36,
              "alternates of A in salt: %#lx count %d: %u %u %u %u", hr, cnt, alt[0], alt[1], alt[2], alt[3]);
        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('l','a','t','n'), 0, TAG('s','s','0','1'), 2, 8, alt, &cnt);
        check(hr == S_OK && cnt == 3 && alt[0] == 2 && alt[1] == 37 && alt[2] == 38, "alternates of B in ss01 (extension lookup): %#lx count %d", hr, cnt);
        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('l','a','t','n'), 0, TAG('s','a','l','t'), 2, 8, alt, &cnt);
        check(hr == S_OK && cnt == 0, "B has none in salt: %#lx count %d", hr, cnt);
        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('l','a','t','n'), 0, TAG('s','a','l','t'), 1, 2, alt, &cnt);
        check(hr == E_OUTOFMEMORY && cnt == 4, "list too short: %#lx count %d", hr, cnt);
        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('l','a','t','n'), 0, TAG('s','a','l','t'), 1, 8, NULL, &cnt);
        check(hr == E_INVALIDARG, "no list: %#lx", hr);
        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('l','a','t','n'), 0, TAG('s','a','l','t'), 1, 0, alt, &cnt);
        check(hr == E_INVALIDARG, "size 0: %#lx", hr);
        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('g','r','e','k'), 0, TAG('s','a','l','t'), 1, 8, alt, &cnt);
        check(hr == USP_E_SCRIPT_NOT_IN_FONT, "a script the font lacks: %#lx", hr);
        hr = ScriptGetFontAlternateGlyphs(hdc, &sc2, NULL, TAG('l','a','t','n'), 0, TAG('z','z','z','z'), 1, 8, alt, &cnt);
        check(hr == S_OK && cnt == 0, "a feature the font lacks: %#lx count %d", hr, cnt);
        ScriptFreeCache(&sc2);
    }

    /* ScriptGetFontFeatureTags: the features the Arabic shaping engine requires are hidden */
    {
        SCRIPT_ITEM items[4];
        SCRIPT_CONTROL ctl = {0};
        SCRIPT_STATE st = {0};
        SCRIPT_CACHE sc3 = NULL;
        OPENTYPE_TAG tags[16];
        int nitems, nt = 0, k, has_init = 0, has_fina = 0, has_liga = 0;
        HRESULT hr;
        static const WCHAR arabic[] = { 0x627, 0x628, 0 };

        ScriptItemize(arabic, 2, 4, &ctl, &st, items, &nitems);
        hr = ScriptGetFontFeatureTags(hdc, &sc3, &items[0].a, TAG('a','r','a','b'), TAG('d','f','l','t'), 16, tags, &nt);
        for (k = 0; k < nt; k++) { has_init |= tags[k] == TAG('i','n','i','t'); has_fina |= tags[k] == TAG('f','i','n','a'); has_liga |= tags[k] == TAG('l','i','g','a'); }
        check(hr == S_OK && nt == 3 && !has_init && !has_fina && has_liga, "Arabic engine: %d tags (hr %#lx), init %d fina %d liga %d", nt, hr, has_init, has_fina, has_liga);
        hr = ScriptGetFontFeatureTags(hdc, &sc3, &items[0].a, TAG('a','r','a','b'), TAG('d','f','l','t'), 2, tags, &nt);
        check(hr == E_OUTOFMEMORY && nt == 3, "Arabic engine, list too short: %#lx count %d", hr, nt);
        hr = ScriptGetFontFeatureTags(hdc, &sc3, NULL, TAG('a','r','a','b'), TAG('d','f','l','t'), 16, tags, &nt);
        check(hr == S_OK && nt == 5, "no analysis, unfiltered: %d tags (hr %#lx)", nt, hr);
        ScriptItemize(L"AB", 2, 4, &ctl, &st, items, &nitems);
        hr = ScriptGetFontFeatureTags(hdc, &sc3, &items[0].a, TAG('l','a','t','n'), TAG('d','f','l','t'), 16, tags, &nt);
        check(hr == S_OK && nt == 6, "Latin engine hides nothing: %d tags (hr %#lx)", nt, hr);
        ScriptFreeCache(&sc3);
    }

    ScriptFreeCache(&sc);
    DeleteDC(hdc);
    DeleteObject(hf);
    RemoveFontMemResourceEx(res);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
