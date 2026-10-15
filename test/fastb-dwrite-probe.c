/* dwrite, fast mode (patch 3543): font download queue, justification, vertical orientation, fallback AddMappings and
   MapCharacters with axes, axis names, glyph image formats */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <stdio.h>
#include <math.h>
#include <dwrite_3.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

struct sink { IDWriteTextAnalysisSink1 IDWriteTextAnalysisSink1_iface; int n; UINT32 pos[8], len[8]; int sideways[8]; int angle[8]; };
static struct sink *impl_sink(IDWriteTextAnalysisSink1 *i) { return (struct sink *)i; }
static HRESULT WINAPI sink_QI(IDWriteTextAnalysisSink1 *i, REFIID r, void **o) { *o = i; return S_OK; }
static ULONG WINAPI sink_AddRef(IDWriteTextAnalysisSink1 *i) { return 2; }
static ULONG WINAPI sink_Release(IDWriteTextAnalysisSink1 *i) { return 1; }
static HRESULT WINAPI sink_SetScriptAnalysis(IDWriteTextAnalysisSink1 *i, UINT32 p, UINT32 l, const DWRITE_SCRIPT_ANALYSIS *s) { return S_OK; }
static HRESULT WINAPI sink_SetLineBreakpoints(IDWriteTextAnalysisSink1 *i, UINT32 p, UINT32 l, const DWRITE_LINE_BREAKPOINT *b) { return S_OK; }
static HRESULT WINAPI sink_SetBidiLevel(IDWriteTextAnalysisSink1 *i, UINT32 p, UINT32 l, UINT8 e, UINT8 r) { return S_OK; }
static HRESULT WINAPI sink_SetNumberSubstitution(IDWriteTextAnalysisSink1 *i, UINT32 p, UINT32 l, IDWriteNumberSubstitution *n) { return S_OK; }
static HRESULT WINAPI sink_SetGlyphOrientation(IDWriteTextAnalysisSink1 *i, UINT32 p, UINT32 l, DWRITE_GLYPH_ORIENTATION_ANGLE a,
        UINT8 level, BOOL sideways, BOOL rtl)
{
    struct sink *s = impl_sink(i);
    if (s->n < 8) { s->pos[s->n] = p; s->len[s->n] = l; s->sideways[s->n] = sideways; s->angle[s->n] = a; s->n++; }
    return S_OK;
}
static const IDWriteTextAnalysisSink1Vtbl sinkvtbl = { sink_QI, sink_AddRef, sink_Release, sink_SetScriptAnalysis, sink_SetLineBreakpoints,
        sink_SetBidiLevel, sink_SetNumberSubstitution, sink_SetGlyphOrientation };

struct source { IDWriteTextAnalysisSource1 IDWriteTextAnalysisSource1_iface; const WCHAR *text; };
static struct source *impl_source(IDWriteTextAnalysisSource1 *i) { return (struct source *)i; }
static HRESULT WINAPI src_QI(IDWriteTextAnalysisSource1 *i, REFIID r, void **o) { *o = i; return S_OK; }
static ULONG WINAPI src_AddRef(IDWriteTextAnalysisSource1 *i) { return 2; }
static ULONG WINAPI src_Release(IDWriteTextAnalysisSource1 *i) { return 1; }
static HRESULT WINAPI src_GetTextAtPosition(IDWriteTextAnalysisSource1 *i, UINT32 p, const WCHAR **t, UINT32 *l)
{
    struct source *s = impl_source(i);
    UINT32 n = lstrlenW(s->text);
    *t = p < n ? s->text + p : NULL; *l = p < n ? n - p : 0; return S_OK;
}
static HRESULT WINAPI src_GetTextBeforePosition(IDWriteTextAnalysisSource1 *i, UINT32 p, const WCHAR **t, UINT32 *l) { *t = NULL; *l = 0; return S_OK; }
static DWRITE_READING_DIRECTION WINAPI src_GetParagraphReadingDirection(IDWriteTextAnalysisSource1 *i) { return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT; }
static HRESULT WINAPI src_GetLocaleName(IDWriteTextAnalysisSource1 *i, UINT32 p, UINT32 *l, const WCHAR **n) { *l = 0; *n = L"en-us"; return S_OK; }
static HRESULT WINAPI src_GetNumberSubstitution(IDWriteTextAnalysisSource1 *i, UINT32 p, UINT32 *l, IDWriteNumberSubstitution **n) { *l = 0; *n = NULL; return S_OK; }
static HRESULT WINAPI src_GetVerticalGlyphOrientation(IDWriteTextAnalysisSource1 *i, UINT32 p, UINT32 *l,
        DWRITE_VERTICAL_GLYPH_ORIENTATION *o, UINT8 *b)
{ *l = 0; *o = DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT; *b = 0; return S_OK; }
static const IDWriteTextAnalysisSource1Vtbl srcvtbl = { src_QI, src_AddRef, src_Release, src_GetTextAtPosition, src_GetTextBeforePosition,
        src_GetParagraphReadingDirection, src_GetLocaleName, src_GetNumberSubstitution, src_GetVerticalGlyphOrientation };

struct listener { IDWriteFontDownloadListener IDWriteFontDownloadListener_iface; LONG refs; };
static HRESULT WINAPI l_QI(IDWriteFontDownloadListener *i, REFIID r, void **o) { *o = i; return S_OK; }
static ULONG WINAPI l_AddRef(IDWriteFontDownloadListener *i) { return InterlockedIncrement(&((struct listener *)i)->refs); }
static ULONG WINAPI l_Release(IDWriteFontDownloadListener *i) { return InterlockedDecrement(&((struct listener *)i)->refs); }
static void WINAPI l_Done(IDWriteFontDownloadListener *i, IDWriteFontDownloadQueue *q, IUnknown *c, HRESULT hr) { }
static const IDWriteFontDownloadListenerVtbl lvtbl = { l_QI, l_AddRef, l_Release, l_Done };

int main(void)
{
    IDWriteFactory7 *factory7 = NULL;
    IDWriteFactory3 *factory3;
    IDWriteFontDownloadQueue *queue = NULL, *queue2 = NULL;
    IDWriteFactory *factory;
    HRESULT hr;
    UINT32 token = 0, token2 = 0;
    struct listener listener = { { &lvtbl }, 1 };

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory);
    if (FAILED(hr)) { printf("FAIL  factory\nRESULT: FAIL\n"); return 1; }
    hr = IDWriteFactory_QueryInterface(factory, &IID_IDWriteFactory3, (void **)&factory3);
    check(hr == S_OK, "IDWriteFactory3");

    /* download queue */
    hr = IDWriteFactory3_GetFontDownloadQueue(factory3, &queue);
    check(hr == S_OK && queue, "GetFontDownloadQueue");
    hr = IDWriteFactory3_GetFontDownloadQueue(factory3, &queue2);
    check(hr == S_OK && queue2 == queue, "same queue every time");
    IDWriteFontDownloadQueue_Release(queue2);
    check(IDWriteFontDownloadQueue_IsEmpty(queue) == TRUE, "queue is empty");
    check(IDWriteFontDownloadQueue_GetGenerationCount(queue) == 0, "generation 0");
    hr = IDWriteFontDownloadQueue_AddListener(queue, &listener.IDWriteFontDownloadListener_iface, &token);
    check(hr == S_OK && listener.refs == 2, "AddListener keeps a reference");
    hr = IDWriteFontDownloadQueue_AddListener(queue, &listener.IDWriteFontDownloadListener_iface, &token2);
    check(hr == S_OK && token2 != token, "second listener has another token");
    check(IDWriteFontDownloadQueue_RemoveListener(queue, token) == S_OK && listener.refs == 2, "RemoveListener releases");
    check(IDWriteFontDownloadQueue_RemoveListener(queue, token) == E_INVALIDARG, "RemoveListener twice");
    check(IDWriteFontDownloadQueue_RemoveListener(queue, token2) == S_OK && listener.refs == 1, "remove the other");
    check(IDWriteFontDownloadQueue_BeginDownload(queue, NULL) == S_OK, "BeginDownload on an empty queue");
    check(IDWriteFontDownloadQueue_CancelDownload(queue) == S_OK, "CancelDownload");
    IDWriteFontDownloadQueue_Release(queue);

    {
        IDWriteFontCollection1 *collection = NULL;
        hr = IDWriteFactory3_GetSystemFontCollection(factory3, TRUE, &collection, TRUE);
        check(hr == S_OK && collection, "system collection with updates and downloadable");
        if (collection) IDWriteFontCollection1_Release(collection);
    }

    /* text analyzer */
    {
        IDWriteTextAnalyzer *analyzer;
        IDWriteTextAnalyzer1 *analyzer1;
        struct sink sink = { { &sinkvtbl } };
        struct source source = { { &srcvtbl }, L"ab\x65e5\x672c" L"c" };

        hr = IDWriteFactory_CreateTextAnalyzer(factory, &analyzer);
        check(hr == S_OK, "CreateTextAnalyzer");
        IDWriteTextAnalyzer_QueryInterface(analyzer, &IID_IDWriteTextAnalyzer1, (void **)&analyzer1);

        hr = IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(analyzer1, &source.IDWriteTextAnalysisSource1_iface, 0, 5,
                &sink.IDWriteTextAnalysisSink1_iface);
        check(hr == S_OK, "AnalyzeVerticalGlyphOrientation");
        check(sink.n == 3, "three orientation runs");
        check(sink.n == 3 && sink.pos[0] == 0 && sink.len[0] == 2 && sink.sideways[0], "Latin is sideways");
        check(sink.n == 3 && sink.pos[1] == 2 && sink.len[1] == 2 && !sink.sideways[1] && sink.angle[1] == 0, "ideographs upright");
        check(sink.n == 3 && sink.pos[2] == 4 && sink.len[2] == 1 && sink.sideways[2], "Latin again sideways");

        {
            DWRITE_JUSTIFICATION_OPPORTUNITY jo[5];
            FLOAT adv[5] = { 10, 10, 5, 10, 10 }, out[5];
            DWRITE_GLYPH_OFFSET off[5] = { { 0 } }, outoff[5];
            DWRITE_SHAPING_GLYPH_PROPERTIES props[5] = { { 0 } };
            UINT16 clustermap[5] = { 0, 1, 2, 3, 4 };
            DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
            const WCHAR *text = L"ab c ";
            float total = 0;
            int i;

            for (i = 0; i < 5; i++) { props[i].justification = (text[i] == ' ') ? 4 : 2; props[i].isClusterStart = 1; }
            hr = IDWriteTextAnalyzer1_GetJustificationOpportunities(analyzer1, NULL, 16.0f, sa, 5, 5, text, clustermap, props, jo);
            check(hr == S_OK, "GetJustificationOpportunities");
            check(jo[2].applyToTrailingEdge && jo[4].applyToTrailingEdge, "blanks get opportunities");
            check(!jo[0].applyToTrailingEdge && !jo[0].applyToLeadingEdge && jo[0].expansionMaximum == 0.0f, "letters do not");

            hr = IDWriteTextAnalyzer1_JustifyGlyphAdvances(analyzer1, 65.0f, 5, jo, adv, off, out, outoff);
            check(hr == S_OK, "JustifyGlyphAdvances");
            for (i = 0; i < 5; i++) total += out[i];
            check(fabsf(total - 65.0f) < 0.01f, "justified to the width");
            check(out[0] == 10 && out[2] == 5 + 10 && out[4] == 10 + 10 && outoff[2].advanceOffset == 0.0f, "extra goes to the blanks, trailing edge");
            hr = IDWriteTextAnalyzer1_JustifyGlyphAdvances(analyzer1, 45.0f, 5, jo, adv, off, out, outoff);
            check(hr == S_OK && out[2] == 5 && out[4] == 10, "narrower width: blanks cannot compress");
            hr = IDWriteTextAnalyzer1_JustifyGlyphAdvances(analyzer1, -1.0f, 5, jo, adv, off, out, outoff);
            check(hr == E_INVALIDARG, "negative width");

            {
                UINT16 idx[5] = { 1, 2, 3, 4, 5 }, mclu[5], midx[5];
                FLOAT madv[5];
                DWRITE_GLYPH_OFFSET moff[5];
                UINT32 actual = 0;
                hr = IDWriteTextAnalyzer1_GetJustifiedGlyphs(analyzer1, NULL, 16.0f, sa, 5, 5, 5, clustermap, idx, adv, out, outoff,
                        props, &actual, mclu, midx, madv, moff);
                check(hr == S_OK && actual == 5 && midx[3] == 4 && madv[0] == out[0] && mclu[4] == 4, "GetJustifiedGlyphs copies");
                hr = IDWriteTextAnalyzer1_GetJustifiedGlyphs(analyzer1, NULL, 16.0f, sa, 5, 5, 3, clustermap, idx, adv, out, outoff,
                        props, &actual, mclu, midx, madv, moff);
                check(hr == E_NOT_SUFFICIENT_BUFFER && actual == 5, "GetJustifiedGlyphs short buffer");
            }
        }
        IDWriteTextAnalyzer1_Release(analyzer1);
        IDWriteTextAnalyzer_Release(analyzer);
    }

    /* font fallback */
    {
        IDWriteFontFallbackBuilder *builder, *builder2;
        IDWriteFontFallback *custom, *custom2, *system;
        IDWriteFontFallback1 *fb1;
        IDWriteFactory2 *factory2;
        struct source source = { { &srcvtbl }, L"abc" };
        DWRITE_UNICODE_RANGE range = { 0x61, 0x63 };
        const WCHAR *families[] = { L"Tahoma" };
        UINT32 len = 0;
        FLOAT scale = 0;
        IDWriteFontFace5 *face = NULL;

        IDWriteFactory_QueryInterface(factory, &IID_IDWriteFactory2, (void **)&factory2);
        hr = IDWriteFactory2_CreateFontFallbackBuilder(factory2, &builder);
        check(hr == S_OK, "CreateFontFallbackBuilder");
        hr = IDWriteFontFallbackBuilder_AddMapping(builder, &range, 1, families, 1, NULL, NULL, NULL, 1.0f);
        check(hr == S_OK, "AddMapping");
        IDWriteFontFallbackBuilder_CreateFontFallback(builder, &custom);

        hr = IDWriteFactory2_CreateFontFallbackBuilder(factory2, &builder2);
        hr = IDWriteFontFallbackBuilder_AddMappings(builder2, custom);
        check(hr == S_OK, "AddMappings from a custom fallback");
        IDWriteFontFallbackBuilder_CreateFontFallback(builder2, &custom2);
        IDWriteFontFallback_QueryInterface(custom2, &IID_IDWriteFontFallback1, (void **)&fb1);
        hr = IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)&source.IDWriteTextAnalysisSource1_iface, 0, 3, NULL, NULL,
                NULL, 0, &len, &scale, &face);
        check(hr == S_OK, "MapCharacters with axes");
        {
            IDWriteFont *font = NULL;
            UINT32 len2 = 0;
            FLOAT scale2 = 0;
            hr = IDWriteFontFallback_MapCharacters(custom2, (IDWriteTextAnalysisSource *)&source.IDWriteTextAnalysisSource1_iface, 0, 3,
                    NULL, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &len2, &font, &scale2);
            check(hr == S_OK && len == len2 && (face != NULL) == (font != NULL), "same answer as MapCharacters of the copied mappings");
            if (font) IDWriteFont_Release(font);
        }
        if (face)
        {
            DWRITE_FONT_AXIS_VALUE axis = { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f };
            IDWriteFontFace5 *face2 = NULL;
            check(IDWriteFontFace5_GetFontAxisValueCount(face) >= 4, "face has the static axes");
            IDWriteFontFace5_Release(face);
            hr = IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)&source.IDWriteTextAnalysisSource1_iface, 0, 3,
                    NULL, NULL, &axis, 1, &len, &scale, &face2);
            check(hr == S_OK && face2, "MapCharacters with a weight axis");
            if (face2) IDWriteFontFace5_Release(face2);
        }
        IDWriteFontFallback1_Release(fb1);

        /* system fallback can be copied too */
        IDWriteFactory2_GetSystemFontFallback(factory2, &system);
        IDWriteFontFallbackBuilder_Release(builder2);
        IDWriteFactory2_CreateFontFallbackBuilder(factory2, &builder2);
        hr = IDWriteFontFallbackBuilder_AddMappings(builder2, system);
        check(hr == S_OK, "AddMappings from the system fallback");
        IDWriteFontFallback_Release(system);
        IDWriteFontFallbackBuilder_Release(builder2);
        IDWriteFontFallback_Release(custom2);
        IDWriteFontFallback_Release(custom);
        IDWriteFontFallbackBuilder_Release(builder);
        IDWriteFactory2_Release(factory2);
    }

    /* font resource axis names, glyph image formats */
    {
        IDWriteFontCollection *syscol = NULL;
        IDWriteFontFamily *family;
        IDWriteFont *font;
        IDWriteFontFace *face;
        IDWriteFontFace4 *face4;
        IDWriteFontFace5 *face5;
        UINT32 idx = 0;
        BOOL exists = FALSE;

        IDWriteFactory_GetSystemFontCollection(factory, &syscol, FALSE);
        IDWriteFontCollection_FindFamilyName(syscol, L"Tahoma", &idx, &exists);
        if (!exists) { idx = 0; exists = TRUE; }
        IDWriteFontCollection_GetFontFamily(syscol, idx, &family);
        IDWriteFontFamily_GetFirstMatchingFont(family, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
        IDWriteFont_CreateFontFace(font, &face);
        IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace5, (void **)&face5);
        IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace4, (void **)&face4);
        {
            IDWriteFontResource *resource = NULL;
            IDWriteLocalizedStrings *names = NULL;
            UINT32 n;

            hr = IDWriteFontFace5_GetFontResource(face5, &resource);
            check(hr == S_OK && resource, "GetFontResource");
            n = IDWriteFontResource_GetFontAxisCount(resource);
            check(n >= 4, "axis count");
            hr = IDWriteFontResource_GetAxisNames(resource, 0, &names);
            check(hr == S_OK && names && IDWriteLocalizedStrings_GetCount(names) > 0, "axis names");
            if (names) IDWriteLocalizedStrings_Release(names);
            hr = IDWriteFontResource_GetAxisNames(resource, n, &names);
            check(FAILED(hr) && !names, "axis names out of range");
            check(IDWriteFontResource_GetAxisValueNameCount(resource, 0) == 0, "no named values in a static font");
            IDWriteFontResource_Release(resource);
        }
        {
            DWRITE_GLYPH_IMAGE_FORMATS formats = 0, all;
            DWRITE_GLYPH_IMAGE_DATA data;
            void *ctx = (void *)1;
            UINT16 gi;
            UINT32 cp = 'A';

            IDWriteFontFace4_GetGlyphIndices(face4, &cp, 1, &gi);
            hr = IDWriteFontFace4_GetGlyphImageFormats_(face4, gi, 1, 100, &formats);
            all = IDWriteFontFace4_GetGlyphImageFormats(face4);
            check(hr == S_OK && (formats & DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE), "glyph 'A' has an outline");
            check((formats & ~all) == 0, "glyph formats are within the face formats");
            hr = IDWriteFontFace4_GetGlyphImageFormats_(face4, gi, 1, 100, NULL);
            check(hr == E_INVALIDARG, "no output pointer");
            hr = IDWriteFontFace4_GetGlyphImageData(face4, gi, 16, DWRITE_GLYPH_IMAGE_FORMATS_PNG, &data, &ctx);
            check(hr == S_OK && ctx == NULL && data.imageData == NULL, "no PNG data in an outline font");
            hr = IDWriteFontFace4_GetGlyphImageData(face4, gi, 16, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE, &data, &ctx);
            check(hr == E_INVALIDARG, "outlines are not image data");
            hr = IDWriteFontFace4_GetGlyphImageData(face4, gi, 16, DWRITE_GLYPH_IMAGE_FORMATS_PNG | DWRITE_GLYPH_IMAGE_FORMATS_SVG, &data, &ctx);
            check(hr == E_INVALIDARG, "more than one format");
            IDWriteFontFace4_ReleaseGlyphImageData(face4, NULL);
        }
        IDWriteFontFace4_Release(face4);
        IDWriteFontFace5_Release(face5);
        IDWriteFontFace_Release(face);
        IDWriteFont_Release(font);
        IDWriteFontFamily_Release(family);
        IDWriteFontCollection_Release(syscol);
    }


    /* per range font axis values of a layout */
    {
        IDWriteTextFormat *format;
        IDWriteTextLayout *layout;
        IDWriteTextLayout4 *layout4;
        DWRITE_FONT_AXIS_VALUE axes[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 80.0f } }, got[2];
        DWRITE_TEXT_RANGE range = { 2, 3 }, r;
        DWRITE_TEXT_METRICS tm;

        hr = IDWriteFactory_CreateTextFormat(factory, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &format);
        hr = IDWriteFactory_CreateTextLayout(factory, L"hello world", 11, format, 500.0f, 100.0f, &layout);
        check(hr == S_OK, "CreateTextLayout");
        IDWriteTextLayout_QueryInterface(layout, &IID_IDWriteTextLayout4, (void **)&layout4);
        check(IDWriteTextLayout4_GetFontAxisValueCount(layout4, 3) == 0, "no axes at first");
        hr = IDWriteTextLayout4_SetFontAxisValues(layout4, NULL, 1, range);
        check(hr == E_INVALIDARG, "values without a pointer");
        hr = IDWriteTextLayout4_SetFontAxisValues(layout4, axes, 2, range);
        check(hr == S_OK, "SetFontAxisValues");
        check(IDWriteTextLayout4_GetFontAxisValueCount(layout4, 3) == 2, "two axes inside the range");
        check(IDWriteTextLayout4_GetFontAxisValueCount(layout4, 1) == 0 && IDWriteTextLayout4_GetFontAxisValueCount(layout4, 5) == 0,
                "none outside it");
        memset(got, 0, sizeof(got));
        hr = IDWriteTextLayout4_GetFontAxisValues(layout4, 3, got, 1, &r);
        check(hr == E_NOT_SUFFICIENT_BUFFER, "small buffer");
        hr = IDWriteTextLayout4_GetFontAxisValues(layout4, 3, got, 2, &r);
        check(hr == S_OK && got[0].axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT && got[0].value == 700.0f && got[1].value == 80.0f &&
                r.startPosition == 2 && r.length == 3, "values and range");
        hr = IDWriteTextLayout_GetMetrics(layout, &tm);
        check(hr == S_OK && tm.width > 0.0f, "layout computes with variation axes");
        IDWriteTextLayout4_Release(layout4);
        IDWriteTextLayout_Release(layout);
        IDWriteTextFormat_Release(format);
    }

    IDWriteFactory3_Release(factory3);
    IDWriteFactory_Release(factory);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
