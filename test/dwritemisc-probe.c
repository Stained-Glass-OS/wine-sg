/* DirectWrite odds and ends (patches/sg/1692), run by test/dwritemisc-gate.sh:
 * locality (every font is local), font face references' sizes, times and
 * equality, variations and axis values (a variable font given as argv[1]
 * when the host has one), font set axis ranges and filtering, matching by
 * axis values, expiration events, and AnalyzeNumberSubstitution. These were
 * stubs (FALSE, 0, E_NOTIMPL, NULL). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dwrite_3.h>
#include <stdio.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* a text source and sink for number substitution */
static const WCHAR text[] = L"abc 123 de45";
static IDWriteNumberSubstitution *subst;
static UINT32 runs[8][2];
static int nruns;

static HRESULT WINAPI src_QueryInterface(IDWriteTextAnalysisSource *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteTextAnalysisSource)) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI src_AddRef(IDWriteTextAnalysisSource *iface) { return 2; }
static ULONG WINAPI src_Release(IDWriteTextAnalysisSource *iface) { return 1; }
static HRESULT WINAPI src_GetTextAtPosition(IDWriteTextAnalysisSource *iface, UINT32 pos, const WCHAR **t, UINT32 *len)
{
    if (pos >= ARRAY_SIZE(text) - 1) { *t = NULL; *len = 0; return S_OK; }
    *t = text + pos;
    *len = ARRAY_SIZE(text) - 1 - pos;
    return S_OK;
}
static HRESULT WINAPI src_GetTextBeforePosition(IDWriteTextAnalysisSource *iface, UINT32 pos, const WCHAR **t, UINT32 *len)
{
    *t = text;
    *len = pos;
    return S_OK;
}
static DWRITE_READING_DIRECTION WINAPI src_GetParagraphReadingDirection(IDWriteTextAnalysisSource *iface)
{
    return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
}
static HRESULT WINAPI src_GetLocaleName(IDWriteTextAnalysisSource *iface, UINT32 pos, UINT32 *len, const WCHAR **name)
{
    *len = ARRAY_SIZE(text) - 1 - pos;
    *name = L"en-us";
    return S_OK;
}
static HRESULT WINAPI src_GetNumberSubstitution(IDWriteTextAnalysisSource *iface, UINT32 pos, UINT32 *len,
        IDWriteNumberSubstitution **s)
{
    *len = ARRAY_SIZE(text) - 1 - pos;
    *s = subst;
    if (subst) IDWriteNumberSubstitution_AddRef(subst);
    return S_OK;
}
static IDWriteTextAnalysisSourceVtbl src_vtbl = { src_QueryInterface, src_AddRef, src_Release, src_GetTextAtPosition,
    src_GetTextBeforePosition, src_GetParagraphReadingDirection, src_GetLocaleName, src_GetNumberSubstitution };
static IDWriteTextAnalysisSource source = { &src_vtbl };

static HRESULT WINAPI sink_QueryInterface(IDWriteTextAnalysisSink *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteTextAnalysisSink)) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IDWriteTextAnalysisSink *iface) { return 2; }
static ULONG WINAPI sink_Release(IDWriteTextAnalysisSink *iface) { return 1; }
static HRESULT WINAPI sink_SetScriptAnalysis(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, const DWRITE_SCRIPT_ANALYSIS *a) { return S_OK; }
static HRESULT WINAPI sink_SetLineBreakpoints(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, const DWRITE_LINE_BREAKPOINT *b) { return S_OK; }
static HRESULT WINAPI sink_SetBidiLevel(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, UINT8 e, UINT8 r) { return S_OK; }
static HRESULT WINAPI sink_SetNumberSubstitution(IDWriteTextAnalysisSink *iface, UINT32 p, UINT32 l, IDWriteNumberSubstitution *s)
{
    if (nruns < 8) { runs[nruns][0] = p; runs[nruns][1] = l; }
    nruns++;
    return S_OK;
}
static IDWriteTextAnalysisSinkVtbl sink_vtbl = { sink_QueryInterface, sink_AddRef, sink_Release, sink_SetScriptAnalysis,
    sink_SetLineBreakpoints, sink_SetBidiLevel, sink_SetNumberSubstitution };
static IDWriteTextAnalysisSink sink = { &sink_vtbl };

int main(int argc, char **argv)
{
    IDWriteFactory7 *factory;
    IDWriteFontCollection *collection;
    IDWriteFontCollection3 *collection3;
    IDWriteFontFamily *family;
    IDWriteFontFamily2 *family2;
    IDWriteFontList2 *list;
    IDWriteFont *font;
    IDWriteFont3 *font3;
    IDWriteFontFace *face;
    IDWriteFontFace5 *face5;
    IDWriteFontFaceReference *ref;
    IDWriteFontSet *set;
    IDWriteFontSet1 *set1, *filtered;
    IDWriteTextAnalyzer *analyzer;
    DWRITE_FONT_AXIS_RANGE ranges[32];
    DWRITE_FONT_AXIS_VALUE values[16];
    DWRITE_FONT_AXIS_VALUE bold = { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f };
    DWRITE_FONT_AXIS_RANGE heavy = { DWRITE_FONT_AXIS_TAG_WEIGHT, 650.0f, 1000.0f };
    FILETIME ft = { 0 };
    UINT32 index, count = 0, n = 0;
    BOOL exists = FALSE, local = FALSE;
    HANDLE event;
    HRESULT hr;
    int i;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory7, (IUnknown **)&factory);
    check(hr == S_OK, "an IDWriteFactory7");
    if (FAILED(hr)) goto done;
    IDWriteFactory7_GetSystemFontCollection(factory, FALSE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &collection3);
    IDWriteFontCollection3_QueryInterface(collection3, &IID_IDWriteFontCollection, (void **)&collection);
    event = IDWriteFontCollection3_GetExpirationEvent(collection3);
    check(event && WaitForSingleObject(event, 0) == WAIT_TIMEOUT, "GetExpirationEvent: an event, not set");

    IDWriteFontCollection_FindFamilyName(collection, L"Tahoma", &index, &exists);
    check(exists, "Tahoma");
    if (!exists) goto done;
    IDWriteFontCollection_GetFontFamily(collection, index, &family);
    IDWriteFontFamily_GetFirstMatchingFont(family, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                           DWRITE_FONT_STYLE_NORMAL, &font);
    IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3);
    check(IDWriteFont3_GetLocality(font3) == DWRITE_LOCALITY_LOCAL, "the font is local");
    IDWriteFont_CreateFontFace(font, &face);
    IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace5, (void **)&face5);
    check(IDWriteFontFace5_IsCharacterLocal(face5, 'A'), "IsCharacterLocal (was FALSE)");
    check(IDWriteFontFace5_AreCharactersLocal(face5, L"abc", 3, FALSE, &local) == S_OK && local, "AreCharactersLocal");
    check(!IDWriteFontFace5_HasVariations(face5), "Tahoma has no variations");
    n = IDWriteFontFace5_GetFontAxisValueCount(face5);
    check(n >= 2 && n <= 16 && IDWriteFontFace5_GetFontAxisValues(face5, values, n) == S_OK,
          "GetFontAxisValueCount and GetFontAxisValues (its static axes)");
    for (i = 0, exists = FALSE; i < (int)n; i++)
        if (values[i].axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT && values[i].value == 400.0f) exists = TRUE;
    check(exists, "weight 400");
    IDWriteFontFace5_GetFontFaceReference(face5, &ref);
    check(IDWriteFontFaceReference_GetFileSize(ref) > 10000 &&
          IDWriteFontFaceReference_GetLocalFileSize(ref) == IDWriteFontFaceReference_GetFileSize(ref),
          "GetFileSize and GetLocalFileSize (were 0)");
    check(IDWriteFontFaceReference_GetFileTime(ref, &ft) == S_OK && (ft.dwHighDateTime || ft.dwLowDateTime),
          "GetFileTime");
    check(IDWriteFontFaceReference_Equals(ref, ref), "Equals itself");
    check(IDWriteFontFaceReference_EnqueueFontDownloadRequest(ref) == S_OK, "nothing to download: S_OK");
    IDWriteFontFaceReference_Release(ref);

    /* matching by axis values */
    IDWriteFontFamily_QueryInterface(family, &IID_IDWriteFontFamily2, (void **)&family2);
    hr = IDWriteFontFamily2_GetMatchingFonts(family2, &bold, 1, &list);
    check(hr == S_OK, "GetMatchingFonts by axis values (was E_NOTIMPL)");
    if (hr == S_OK)
    {
        IDWriteFont *first;
        IDWriteFontList_GetFont((IDWriteFontList *)list, 0, &first);
        if (IDWriteFont_GetWeight(first) != DWRITE_FONT_WEIGHT_BOLD) printf("      weight %d of %u\n", IDWriteFont_GetWeight(first), IDWriteFontList_GetFontCount((IDWriteFontList *)list));
        check(IDWriteFont_GetWeight(first) == DWRITE_FONT_WEIGHT_BOLD, "wght 700: the bold face first");
        IDWriteFont_Release(first);
        IDWriteFontList2_Release(list);
    }

    /* font set axes */
    IDWriteFactory7_GetSystemFontSet(factory, FALSE, (IDWriteFontSet2 **)&set);
    IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet1, (void **)&set1);
    hr = IDWriteFontSet1_GetFontAxisRanges(set1, ranges, ARRAY_SIZE(ranges), &count);
    check(hr == S_OK && count >= 2, "GetFontAxisRanges of the set");
    for (i = 0, exists = FALSE; i < (int)count && i < (int)ARRAY_SIZE(ranges); i++)
        if (ranges[i].axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT && ranges[i].minValue < 400 && ranges[i].maxValue >= 700)
            exists = TRUE;
    check(exists, "its weights span light to bold");
    hr = IDWriteFontSet1_GetFilteredFonts_(set1, &heavy, 1, FALSE, &filtered);
    check(hr == S_OK && IDWriteFontSet1_GetFontCount(filtered) > 0 &&
          IDWriteFontSet1_GetFontCount(filtered) < IDWriteFontSet1_GetFontCount(set1),
          "GetFilteredFonts by a weight range: the bold ones");
    if (hr == S_OK) IDWriteFontSet1_Release(filtered);
    check(IDWriteFontSet1_GetFontAxisRanges_(set1, 0, ranges, ARRAY_SIZE(ranges), &count) == S_OK && count >= 2,
          "GetFontAxisRanges of one font");

    /* number substitution */
    IDWriteFactory7_CreateTextAnalyzer(factory, &analyzer);
    IDWriteFactory7_CreateNumberSubstitution(factory, DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL, L"ar-eg", FALSE, &subst);
    hr = IDWriteTextAnalyzer_AnalyzeNumberSubstitution(analyzer, &source, 0, ARRAY_SIZE(text) - 1, &sink);
    check(hr == S_OK && nruns == 2 && runs[0][0] == 4 && runs[0][1] == 3 && runs[1][0] == 10 && runs[1][1] == 2,
          "AnalyzeNumberSubstitution: the two runs of digits");
    if (nruns != 2) printf("      %d runs\n", nruns);

    /* a variable font */
    if (argc > 1)
    {
        WCHAR path[MAX_PATH];
        IDWriteFontFile *file;
        IDWriteFontFaceReference *vref;
        IDWriteFontFace3 *vface;
        IDWriteFontFace5 *vface5;

        MultiByteToWideChar(CP_ACP, 0, argv[1], -1, path, MAX_PATH);
        if (SUCCEEDED(IDWriteFactory7_CreateFontFileReference(factory, path, NULL, &file)) &&
            SUCCEEDED(IDWriteFactory7_CreateFontFaceReference_(factory, file, 0, DWRITE_FONT_SIMULATIONS_NONE, &vref)) &&
            SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(vref, &vface)))
        {
            IDWriteFontFace3_QueryInterface(vface, &IID_IDWriteFontFace5, (void **)&vface5);
            check(IDWriteFontFace5_HasVariations(vface5), "a variable font HasVariations (was FALSE)");
            IDWriteFontFace5_Release(vface5);
            IDWriteFontFace3_Release(vface);
        }
        else check(0, "a variable font loads");
    }
    else printf("SKIP  no variable font on this host\n");

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
