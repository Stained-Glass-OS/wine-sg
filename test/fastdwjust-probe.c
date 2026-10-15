/* dwrite justified text alignment (patch 3304), run by test/fastdwjust-gate.sh */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dwrite.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CLOSE(a, b) (fabs((a) - (b)) < 0.02)

struct run { float x, y, right; UINT32 glyphs; UINT32 pos; float adv[64]; };
struct ul { float x, y, width; };
struct renderer
{
    IDWriteTextRenderer IDWriteTextRenderer_iface;
    struct run runs[64];
    int count;
    struct ul uls[64];
    int ul_count;
};
static struct renderer *impl(IDWriteTextRenderer *i) { return CONTAINING_RECORD(i, struct renderer, IDWriteTextRenderer_iface); }

static HRESULT WINAPI r_QI(IDWriteTextRenderer *i, REFIID riid, void **o)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWritePixelSnapping) || IsEqualIID(riid, &IID_IDWriteTextRenderer))
    { *o = i; return S_OK; }
    *o = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI r_AddRef(IDWriteTextRenderer *i) { return 2; }
static ULONG WINAPI r_Release(IDWriteTextRenderer *i) { return 1; }
static HRESULT WINAPI r_IsSnap(IDWriteTextRenderer *i, void *c, BOOL *d) { *d = TRUE; return S_OK; }
static HRESULT WINAPI r_Transform(IDWriteTextRenderer *i, void *c, DWRITE_MATRIX *m)
{ m->m11 = 1; m->m12 = 0; m->m21 = 0; m->m22 = 1; m->dx = 0; m->dy = 0; return S_OK; }
static HRESULT WINAPI r_Dip(IDWriteTextRenderer *i, void *c, FLOAT *d) { *d = 1.0f; return S_OK; }
static HRESULT WINAPI r_Glyphs(IDWriteTextRenderer *i, void *c, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE m,
        const DWRITE_GLYPH_RUN *run, const DWRITE_GLYPH_RUN_DESCRIPTION *d, IUnknown *e)
{
    struct renderer *r = impl(i);
    struct run *u = &r->runs[r->count++];
    UINT32 k;
    float sum = 0;
    u->x = x; u->y = y; u->glyphs = run->glyphCount; u->pos = d->textPosition;
    for (k = 0; k < run->glyphCount; ++k) { if (k < 64) u->adv[k] = run->glyphAdvances[k]; sum += run->glyphAdvances[k]; }
    u->right = x + sum;
    return S_OK;
}
static HRESULT WINAPI r_Underline(IDWriteTextRenderer *i, void *c, FLOAT x, FLOAT y, const DWRITE_UNDERLINE *u, IUnknown *e)
{
    struct renderer *r = impl(i);
    r->uls[r->ul_count].x = x; r->uls[r->ul_count].y = y; r->uls[r->ul_count].width = u->width;
    r->ul_count++;
    return S_OK;
}
static HRESULT WINAPI r_Strike(IDWriteTextRenderer *i, void *c, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH *u, IUnknown *e) { return S_OK; }
static HRESULT WINAPI r_Inline(IDWriteTextRenderer *i, void *c, FLOAT x, FLOAT y, IDWriteInlineObject *o, BOOL s, BOOL r, IUnknown *e) { return S_OK; }
static const IDWriteTextRendererVtbl renderer_vtbl = { r_QI, r_AddRef, r_Release, r_IsSnap, r_Transform, r_Dip, r_Glyphs, r_Underline, r_Strike, r_Inline };

static IDWriteFactory *factory;

static IDWriteTextLayout *make_layout(const WCHAR *text, float width, DWRITE_TEXT_ALIGNMENT align, DWRITE_WORD_WRAPPING wrap)
{
    IDWriteTextFormat *format;
    IDWriteTextLayout *layout = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(factory, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &format);
    if (FAILED(hr)) return NULL;
    IDWriteTextFormat_SetTextAlignment(format, align);
    IDWriteTextFormat_SetWordWrapping(format, wrap);
    IDWriteFactory_CreateTextLayout(factory, text, lstrlenW(text), format, width, 400.0f, &layout);
    IDWriteTextFormat_Release(format);
    return layout;
}

static void draw(IDWriteTextLayout *layout, struct renderer *r)
{
    memset(r, 0, sizeof(*r));
    r->IDWriteTextRenderer_iface.lpVtbl = &renderer_vtbl;
    IDWriteTextLayout_Draw(layout, NULL, &r->IDWriteTextRenderer_iface, 0, 0);
}

/* extent of the glyph runs on one baseline: [left, right] */
static int line_extent(const struct renderer *r, float y, float *left, float *right)
{
    int i, found = 0;
    *left = 1e9f; *right = -1e9f;
    for (i = 0; i < r->count; ++i)
    {
        if (!CLOSE(r->runs[i].y, y)) continue;
        found = 1;
        if (r->runs[i].x < *left) *left = r->runs[i].x;
        if (r->runs[i].right > *right) *right = r->runs[i].right;
    }
    return found;
}

static int line_ys(const struct renderer *r, float *ys)
{
    int i, j, n = 0;
    for (i = 0; i < r->count; ++i)
    {
        for (j = 0; j < n; ++j) if (CLOSE(ys[j], r->runs[i].y)) break;
        if (j == n) ys[n++] = r->runs[i].y;
    }
    return n;
}

int main(void)
{
    static const WCHAR text[] = L"one two three four five six seven eight nine ten eleven twelve thirteen";
    struct renderer lead, just;
    float ys[32], jys[32], l, r, l2, r2;
    int n, jn, i;
    IDWriteTextLayout *layout, *layout2;
    UINT32 count1 = 0, count2 = 0;
    DWRITE_LINE_METRICS lm1[16], lm2[16];
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory);
    if (FAILED(hr)) { check(0, "create the factory"); return 1; }

    layout = make_layout(text, 200.0f, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_WORD_WRAPPING_WRAP);
    if (!layout) { check(0, "create a layout with Tahoma"); return 1; }
    layout2 = make_layout(text, 200.0f, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_WORD_WRAPPING_WRAP);
    draw(layout, &lead);
    draw(layout2, &just);
    n = line_ys(&lead, ys);
    jn = line_ys(&just, jys);
    CHECKF(n >= 3 && jn == n, "the same number of lines: %d and %d", n, jn);

    IDWriteTextLayout_GetLineMetrics(layout, lm1, 16, &count1);
    IDWriteTextLayout_GetLineMetrics(layout2, lm2, 16, &count2);
    {
        int same = count1 == count2;
        for (i = 0; same && i < (int)count1; ++i) same = lm1[i].length == lm2[i].length;
        CHECKF(same, "justifying does not change where lines break (%u, %u lines)", count1, count2);
    }

    for (i = 0; i < n - 1; ++i)
    {
        char what[100];
        line_extent(&just, jys[i], &l, &r);
        sprintf(what, "justified line %d spans the layout width (%.3f..%.3f)", i, l, r);
        check(CLOSE(l, 0.0) && CLOSE(r, 200.0), what);
        line_extent(&lead, ys[i], &l2, &r2);
        sprintf(what, "line %d is narrower when not justified (%.3f)", i, r2);
        check(r2 < 199.9, what);
    }
    line_extent(&just, jys[n - 1], &l, &r);
    line_extent(&lead, ys[n - 1], &l2, &r2);
    CHECKF(CLOSE(l, 0.0) && CLOSE(r, r2) && r < 199.0, "the last line keeps its natural width (%.3f vs %.3f)", r, r2);

    /* the gaps are the blanks: glyph advances other than the blanks' are the natural ones */
    {
        int diff = 0, same_adv = 0, k;
        for (i = 0; i < just.count && i < lead.count; ++i)
        {
            if (just.runs[i].glyphs != lead.runs[i].glyphs) continue;
            for (k = 0; k < (int)just.runs[i].glyphs && k < 64; ++k)
            {
                if (CLOSE(just.runs[i].adv[k], lead.runs[i].adv[k])) same_adv++; else diff++;
            }
        }
        CHECKF(diff > 0 && same_adv > diff, "only some glyphs are wider (%d wider, %d as before)", diff, same_adv);
    }

    /* hit testing follows the stretched blanks: the last glyph of a justified line ends at the layout width */
    {
        DWRITE_HIT_TEST_METRICS hm;
        float px, py;
        BOOL trailing, inside;
        /* the last character of the first line */
        UINT32 last = lm2[0].length - lm2[0].trailingWhitespaceLength - 1;
        hr = IDWriteTextLayout_HitTestTextPosition(layout2, last, FALSE, &px, &py, &hm);
        CHECKF(hr == S_OK && CLOSE(hm.left + hm.width, 200.0), "hit test: the line's last character ends at the width (%.3f + %.3f)", hm.left, hm.width);
        hr = IDWriteTextLayout_HitTestPoint(layout2, 199.0f, 3.0f, &trailing, &inside, &hm);
        CHECKF(hr == S_OK && hm.textPosition == last, "hit test at the right edge is the last character (%u vs %u)", hm.textPosition, last);
    }

    /* back to leading, without a new layout */
    IDWriteTextLayout_SetTextAlignment(layout2, DWRITE_TEXT_ALIGNMENT_LEADING);
    draw(layout2, &just);
    line_extent(&just, jys[0], &l, &r);
    line_extent(&lead, ys[0], &l2, &r2);
    CHECKF(CLOSE(r, r2), "set back to leading, the first line is natural again (%.3f vs %.3f)", r, r2);
    IDWriteTextLayout_SetTextAlignment(layout2, DWRITE_TEXT_ALIGNMENT_JUSTIFIED);
    draw(layout2, &just);
    line_extent(&just, jys[0], &l, &r);
    CHECKF(CLOSE(r, 200.0), "and justified again (%.3f)", r);
    IDWriteTextLayout_Release(layout2);

    /* an underline on the first line covers the stretched line */
    layout2 = make_layout(text, 200.0f, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_WORD_WRAPPING_WRAP);
    {
        DWRITE_TEXT_RANGE first = { 0, lm2[0].length - lm2[0].trailingWhitespaceLength };
        int found = 0;

        IDWriteTextLayout_SetUnderline(layout2, TRUE, first);
        draw(layout2, &just);
        for (i = 0; i < just.ul_count; ++i)
            if (CLOSE(just.uls[i].width, 200.0)) found++;
        CHECKF(found == 1 && just.ul_count == 1, "the underline of the first line is as wide as the line (%d of %d)", found, just.ul_count);
    }
    IDWriteTextLayout_Release(layout2);

    /* a paragraph ends where the text has a line break: that line is not stretched */
    layout2 = make_layout(L"alpha beta\ngamma delta epsilon zeta eta theta iota kappa lambda mu", 200.0f,
            DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_WORD_WRAPPING_WRAP);
    draw(layout2, &just);
    jn = line_ys(&just, jys);
    line_extent(&just, jys[0], &l, &r);
    CHECKF(jn >= 3 && r < 150.0, "the line before a line break is natural (%.3f)", r);
    line_extent(&just, jys[1], &l, &r);
    CHECKF(CLOSE(r, 200.0), "and the next paragraph's first line is stretched (%.3f)", r);
    IDWriteTextLayout_Release(layout2);

    /* no wrapping, no stretching */
    layout2 = make_layout(text, 200.0f, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_WORD_WRAPPING_NO_WRAP);
    draw(layout2, &just);
    jn = line_ys(&just, jys);
    line_extent(&just, jys[0], &l, &r);
    CHECKF(jn == 1 && r > 200.0, "without wrapping the line is natural (%d lines, %.3f)", jn, r);
    IDWriteTextLayout_Release(layout2);

    /* a line of one word has nothing to stretch */
    layout2 = make_layout(L"internationalization internationalization internationalization", 150.0f,
            DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_WORD_WRAPPING_WRAP);
    draw(layout2, &just);
    jn = line_ys(&just, jys);
    layout = make_layout(L"internationalization internationalization internationalization", 150.0f,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_WORD_WRAPPING_WRAP);
    draw(layout, &lead);
    n = line_ys(&lead, ys);
    line_extent(&just, jys[0], &l, &r);
    line_extent(&lead, ys[0], &l2, &r2);
    CHECKF(jn == n && CLOSE(r, r2), "one-word lines are as before (%.3f vs %.3f)", r, r2);
    IDWriteTextLayout_Release(layout2);
    IDWriteTextLayout_Release(layout);

    /* ideographs share the room when there are no blanks */
    layout2 = make_layout(L"\x3042\x3044\x3046\x3048\x304a\x304b\x304d\x304f\x3051\x3053\x3055\x3057\x3059\x305b\x305d\x305f\x3061\x3064\x3066\x3068",
            130.0f, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_WORD_WRAPPING_CHARACTER);
    draw(layout2, &just);
    jn = line_ys(&just, jys);
    CHECKF(jn >= 2, "kana wraps into lines (%d)", jn);
    if (jn >= 2)
    {
        line_extent(&just, jys[0], &l, &r);
        CHECKF(CLOSE(r, 130.0), "a line of kana is stretched to the width (%.3f)", r);
    }
    IDWriteTextLayout_Release(layout2);

    IDWriteTextLayout_Release(layout);
    IDWriteFactory_Release(factory);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
