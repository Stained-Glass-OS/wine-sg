/* dwrite: IDWriteTextAnalyzer::AnalyzeBidi (patches/sg/2625): results the Wine tests
 * record from Windows for the isolate formatting characters of Unicode 6.3 (not known:
 * neutral characters) and for an embedding that is not closed at the end of the text. */
#define COBJMACROS
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define IID_IDWriteFactory my_iid_factory
static const GUID my_iid_factory = {0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

struct source
{
    IDWriteTextAnalysisSource iface;
    const WCHAR *text;
    UINT32 length;
    DWRITE_READING_DIRECTION direction;
};

static HRESULT WINAPI src_QI(IDWriteTextAnalysisSource *iface, REFIID iid, void **out)
{
    *out = iface;
    return S_OK;
}
static ULONG WINAPI src_AddRef(IDWriteTextAnalysisSource *iface) { return 2; }
static ULONG WINAPI src_Release(IDWriteTextAnalysisSource *iface) { return 1; }

static HRESULT WINAPI src_GetTextAtPosition(IDWriteTextAnalysisSource *iface, UINT32 position, const WCHAR **text, UINT32 *length)
{
    struct source *s = (struct source *)iface;

    if (position >= s->length) { *text = NULL; *length = 0; }
    else { *text = s->text + position; *length = s->length - position; }
    return S_OK;
}

static HRESULT WINAPI src_GetTextBeforePosition(IDWriteTextAnalysisSource *iface, UINT32 position, const WCHAR **text, UINT32 *length)
{
    struct source *s = (struct source *)iface;

    if (!position || position > s->length) { *text = NULL; *length = 0; }
    else { *text = s->text; *length = position; }
    return S_OK;
}

static DWRITE_READING_DIRECTION WINAPI src_GetParagraphReadingDirection(IDWriteTextAnalysisSource *iface)
{
    return ((struct source *)iface)->direction;
}

static HRESULT WINAPI src_GetLocaleName(IDWriteTextAnalysisSource *iface, UINT32 position, UINT32 *length, const WCHAR **name)
{
    *length = ((struct source *)iface)->length - position;
    *name = L"en-us";
    return S_OK;
}

static HRESULT WINAPI src_GetNumberSubstitution(IDWriteTextAnalysisSource *iface, UINT32 position, UINT32 *length, IDWriteNumberSubstitution **sub)
{
    *length = ((struct source *)iface)->length - position;
    *sub = NULL;
    return S_OK;
}

static const IDWriteTextAnalysisSourceVtbl src_vtbl =
{
    src_QI, src_AddRef, src_Release, src_GetTextAtPosition, src_GetTextBeforePosition,
    src_GetParagraphReadingDirection, src_GetLocaleName, src_GetNumberSubstitution,
};

struct sink
{
    IDWriteTextAnalysisSink iface;
    UINT8 explicit_levels[16], resolved_levels[16];
};

static HRESULT WINAPI snk_QI(IDWriteTextAnalysisSink *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI snk_AddRef(IDWriteTextAnalysisSink *iface) { return 2; }
static ULONG WINAPI snk_Release(IDWriteTextAnalysisSink *iface) { return 1; }
static HRESULT WINAPI snk_SetScriptAnalysis(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, const DWRITE_SCRIPT_ANALYSIS *a) { return S_OK; }
static HRESULT WINAPI snk_SetLineBreakpoints(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, const DWRITE_LINE_BREAKPOINT *b) { return S_OK; }
static HRESULT WINAPI snk_SetBidiLevel(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, UINT8 e, UINT8 r)
{
    struct sink *s = (struct sink *)iface;

    while (l-- && p < 16) { s->explicit_levels[p] = e; s->resolved_levels[p] = r; ++p; }
    return S_OK;
}
static HRESULT WINAPI snk_SetNumberSubstitution(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, IDWriteNumberSubstitution *n) { return S_OK; }

static const IDWriteTextAnalysisSinkVtbl snk_vtbl =
{
    snk_QI, snk_AddRef, snk_Release, snk_SetScriptAnalysis, snk_SetLineBreakpoints, snk_SetBidiLevel, snk_SetNumberSubstitution,
};

#define LRE 0x202a
#define RLE 0x202b
#define PDF 0x202c
#define LRI 0x2066
#define RLI 0x2067
#define PDI 0x2069

static const struct
{
    WCHAR text[8];
    DWRITE_READING_DIRECTION direction;
    UINT8 explicit_levels[8];
    UINT8 resolved[8];
    const char *name;
} tests[] =
{
    {{LRE, 'a', 'b', PDF, 0}, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, {2, 2, 2, 2}, {0, 2, 2, 2}, "an embedding closed at the end"},
    {{LRI, 'a', 'b', PDI, 0}, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, {0, 0, 0, 0}, {0, 0, 0, 0}, "left to right isolate"},
    {{RLI, 'a', 'b', PDI, 0}, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, {0, 0, 0, 0}, {0, 0, 0, 0}, "right to left isolate"},
    {{'a', LRI, 'b', PDI, 'c', 0}, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, "isolate between letters"},
    {{LRE, PDF, 'a', 'b', 0}, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, {2, 2, 0, 0}, {0, 0, 0, 0}, "empty embedding"},
    {{'a', RLE, PDF, 'b', 0}, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, {0, 1, 1, 0}, {0, 0, 0, 0}, "empty right to left embedding"},
    {{'a', RLE, 'b', 0}, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, {0, 1, 1}, {0, 0, 2}, "embedding open at the end"},
};

int main(void)
{
    IDWriteFactory *factory = NULL;
    IDWriteTextAnalyzer *analyzer = NULL;
    HRESULT hr;
    unsigned int i, j;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &my_iid_factory, (IUnknown **)&factory);
    check(hr == S_OK, "factory (%#lx)", hr);
    if (FAILED(hr)) goto done;
    hr = IDWriteFactory_CreateTextAnalyzer(factory, &analyzer);
    check(hr == S_OK, "analyzer (%#lx)", hr);
    if (FAILED(hr)) goto done;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    {
        struct source src = { { &src_vtbl }, tests[i].text, wcslen(tests[i].text), tests[i].direction };
        struct sink snk = { { &snk_vtbl } };
        int explicit_ok = 1, resolved_ok = 1;

        memset(snk.explicit_levels, 0xff, sizeof(snk.explicit_levels));
        memset(snk.resolved_levels, 0xff, sizeof(snk.resolved_levels));
        hr = IDWriteTextAnalyzer_AnalyzeBidi(analyzer, &src.iface, 0, src.length, &snk.iface);
        check(hr == S_OK, "%s: AnalyzeBidi (%#lx)", tests[i].name, hr);
        for (j = 0; j < src.length; ++j)
        {
            if (snk.explicit_levels[j] != tests[i].explicit_levels[j]) explicit_ok = 0;
            if (snk.resolved_levels[j] != tests[i].resolved[j]) resolved_ok = 0;
        }
        check(explicit_ok, "%s: explicit levels %u %u %u %u", tests[i].name, snk.explicit_levels[0], snk.explicit_levels[1],
                snk.explicit_levels[2], snk.explicit_levels[3]);
        check(resolved_ok, "%s: resolved levels %u %u %u %u", tests[i].name, snk.resolved_levels[0], snk.resolved_levels[1],
                snk.resolved_levels[2], snk.resolved_levels[3]);
    }
    if (analyzer) IDWriteTextAnalyzer_Release(analyzer);
    IDWriteFactory_Release(factory);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
