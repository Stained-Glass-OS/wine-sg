/* Font set queries (patches/sg/1621). DirectWrite font sets answered
 * E_NOTIMPL to everything past "list the fonts": no property values, no
 * matching by weight/stretch/style, no filters, no font faces, and the
 * builder could not take a font set or a font with properties of its own.
 *
 *  - GetPropertyValues lists the set's family names (Tahoma among them),
 *    and fonts answer their weight, stretch and style as numbers;
 *  - GetPropertyOccurrenceCount counts the fonts with a value;
 *  - GetMatchingFonts(family, weight, stretch, style) puts the nearest first;
 *  - GetMatchingFonts(properties) also matches weight and the typographic names;
 *  - FindFontFaceReference / FindFontFace find a font of the set;
 *  - IDWriteFontSet1: filters by properties (all/any), by indices, filter
 *    indices, first font resources, face references and font faces;
 *  - the builder adds a font set, a reference with properties, and a font;
 *  - a collection made from a set takes the family names the set gave.
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dwrite_3.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_IDWriteFactory3_sg, 0x9a1b41c3,0xd3bb,0x466a,0x87,0xfc,0xfe,0x67,0x55,0x6a,0x3b,0x65);
DEFINE_GUID(IID_IDWriteFontSet1_sg, 0x7e9fda85,0x6c92,0x4053,0xbc,0x47,0x7a,0xe3,0x53,0x0d,0xb4,0xd3);
DEFINE_GUID(IID_IDWriteFontSetBuilder2_sg, 0xee5ba612,0xb131,0x463c,0x8f,0x4f,0x31,0x89,0xb9,0x40,0x1e,0x45);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static int font_number(IDWriteFontSet *set, UINT32 index, DWRITE_FONT_PROPERTY_ID id)
{
    IDWriteLocalizedStrings *strings = NULL;
    BOOL exists = FALSE;
    WCHAR buf[32];
    int ret = -1;

    if (FAILED(IDWriteFontSet_GetPropertyValues(set, index, id, &exists, &strings)) || !exists) return -1;
    if (SUCCEEDED(IDWriteLocalizedStrings_GetString(strings, 0, buf, 32))) ret = wcstol(buf, NULL, 10);
    IDWriteLocalizedStrings_Release(strings);
    return ret;
}

static BOOL list_has(IDWriteStringList *list, const WCHAR *value)
{
    UINT32 i, n = IDWriteStringList_GetCount(list);
    WCHAR buf[128];

    for (i = 0; i < n; ++i)
        if (SUCCEEDED(IDWriteStringList_GetString(list, i, buf, 128)) && !_wcsicmp(buf, value)) return TRUE;
    return FALSE;
}

int main(void)
{
    DWRITE_FONT_PROPERTY tahoma = { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"Tahoma", L"" };
    DWRITE_FONT_PROPERTY bold = { DWRITE_FONT_PROPERTY_ID_WEIGHT, L"700", L"" };
    DWRITE_FONT_PROPERTY nothing = { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"No Such Family", L"" };
    DWRITE_FONT_PROPERTY both[2], tag = { DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG, L"sgtag", L"" };
    DWRITE_FONT_PROPERTY named = { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"SG Given Family", L"en-us" };
    IDWriteFontCollection1 *collection;
    IDWriteFontSet *set, *matched = NULL, *built = NULL;
    IDWriteFontSet1 *set1, *subset = NULL;
    IDWriteFontSetBuilder *builder0;
    IDWriteFontSetBuilder2 *builder;
    IDWriteFontFaceReference *ref = NULL;
    IDWriteFontFaceReference1 *ref1 = NULL;
    IDWriteFontFace5 *face5 = NULL;
    IDWriteFontFace *face = NULL;
    IDWriteFontFile *file = NULL;
    IDWriteStringList *list = NULL;
    IDWriteFactory3 *factory;
    UINT32 count, n = 0, index = 0, indices[512], first;
    BOOL exists = FALSE;
    HRESULT hr;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory3_sg, (IUnknown **)&factory)))
    {
        printf("FAIL  no IDWriteFactory3\nRESULT: FAIL\n");
        return 1;
    }
    hr = IDWriteFactory3_GetSystemFontSet(factory, &set);
    check(hr == S_OK, "GetSystemFontSet");
    if (FAILED(hr)) return 1;
    count = IDWriteFontSet_GetFontCount(set);
    printf("system fonts: %u\n", count);

    /* property values */
    hr = IDWriteFontSet_GetPropertyValues__(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &list);
    printf("GetPropertyValues(family): %#lx, %u values\n", hr, list ? IDWriteStringList_GetCount(list) : 0);
    check(hr == S_OK && list && list_has(list, L"Tahoma"), "GetPropertyValues lists the family names (Tahoma among them)");
    if (list) IDWriteStringList_Release(list);
    list = NULL;
    hr = IDWriteFontSet_GetPropertyValues_(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"en-us", &list);
    check(hr == S_OK && list && list_has(list, L"Tahoma"), "... and per preferred locale");
    if (list) IDWriteStringList_Release(list);

    hr = IDWriteFontSet_GetPropertyOccurrenceCount(set, &tahoma, &n);
    printf("Tahoma fonts: %#lx %u\n", hr, n);
    check(hr == S_OK && n >= 2, "GetPropertyOccurrenceCount counts Tahoma's fonts");
    hr = IDWriteFontSet_GetPropertyOccurrenceCount(set, &nothing, &n);
    check(hr == S_OK && n == 0, "... and none of a family that is not there");

    /* matching */
    hr = IDWriteFontSet_GetMatchingFonts_(set, L"Tahoma", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, &matched);
    printf("GetMatchingFonts(Tahoma, bold): %#lx\n", hr);
    if (hr == S_OK)
    {
        n = IDWriteFontSet_GetFontCount(matched);
        printf("  %u fonts, first weight %d, last weight %d\n", n, font_number(matched, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT),
                n ? font_number(matched, n - 1, DWRITE_FONT_PROPERTY_ID_WEIGHT) : -1);
        check(n >= 2 && font_number(matched, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT) == 700,
                "GetMatchingFonts(family, weight, ...) puts the bold face first");
        check(font_number(matched, n - 1, DWRITE_FONT_PROPERTY_ID_WEIGHT) == 400, "... the regular one after it");
        IDWriteFontSet_Release(matched);
    }
    else check(0, "GetMatchingFonts(family, weight, ...)");

    both[0] = tahoma;
    both[1] = bold;
    matched = NULL;
    hr = IDWriteFontSet_GetMatchingFonts(set, both, 2, &matched);
    n = matched ? IDWriteFontSet_GetFontCount(matched) : 0;
    printf("GetMatchingFonts(Tahoma + 700): %#lx %u\n", hr, n);
    check(hr == S_OK && n == 1, "GetMatchingFonts(properties) matches a weight");
    if (matched)
    {
        hr = IDWriteFontSet_GetFontFaceReference(matched, 0, &ref);
        if (hr == S_OK)
        {
            hr = IDWriteFontSet_FindFontFaceReference(set, ref, &index, &exists);
            check(hr == S_OK && exists && font_number(set, index, DWRITE_FONT_PROPERTY_ID_WEIGHT) == 700,
                    "FindFontFaceReference finds it in the system set");
            hr = IDWriteFontFaceReference_CreateFontFace(ref, (IDWriteFontFace3 **)&face);
            if (hr == S_OK)
            {
                index = 12345; exists = FALSE;
                hr = IDWriteFontSet_FindFontFace(set, face, &index, &exists);
                check(hr == S_OK && exists && index < count, "FindFontFace finds its font face");
                IDWriteFontFace_Release(face);
            }
            else check(0, "CreateFontFace");
            IDWriteFontFaceReference_GetFontFile(ref, &file);
            IDWriteFontFaceReference_Release(ref);
        }
        IDWriteFontSet_Release(matched);
    }

    /* IDWriteFontSet1 */
    hr = IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet1_sg, (void **)&set1);
    check(hr == S_OK, "the set is an IDWriteFontSet1");
    if (hr == S_OK)
    {
        hr = IDWriteFontSet1_GetFilteredFonts(set1, both, 2, FALSE, &subset);
        check(hr == S_OK && IDWriteFontSet1_GetFontCount(subset) == 1, "GetFilteredFonts(all of them)");
        if (subset) IDWriteFontSet1_Release(subset);
        subset = NULL;
        hr = IDWriteFontSet1_GetFilteredFonts(set1, both, 2, TRUE, &subset);
        n = subset ? IDWriteFontSet1_GetFontCount(subset) : 0;
        printf("any of Tahoma, 700: %u\n", n);
        check(hr == S_OK && n > 2, "GetFilteredFonts(any of them)");
        if (subset) IDWriteFontSet1_Release(subset);

        hr = IDWriteFontSet1_GetFilteredFontIndices(set1, &tahoma, 1, FALSE, indices, 512, &n);
        printf("Tahoma indices: %#lx %u\n", hr, n);
        check(hr == S_OK && n >= 2 && n < 512, "GetFilteredFontIndices");
        subset = NULL;
        hr = IDWriteFontSet1_GetFilteredFonts__(set1, indices, n, &subset);
        check(hr == S_OK && IDWriteFontSet1_GetFontCount(subset) == n, "GetFilteredFonts(indices)");
        if (subset) IDWriteFontSet1_Release(subset);
        index = count;
        subset = NULL;
        hr = IDWriteFontSet1_GetFilteredFonts__(set1, &index, 1, &subset);
        check(hr == E_INVALIDARG, "... an index past the end is refused");

        subset = NULL;
        hr = IDWriteFontSet1_GetMatchingFonts(set1, &tahoma, NULL, 0, &subset);
        check(hr == S_OK && IDWriteFontSet1_GetFontCount(subset) == n, "IDWriteFontSet1::GetMatchingFonts");
        if (subset) IDWriteFontSet1_Release(subset);

        subset = NULL;
        hr = IDWriteFontSet1_GetFirstFontResources(set1, &subset);
        first = subset ? IDWriteFontSet1_GetFontCount(subset) : 0;
        printf("first font resources: %#lx %u of %u\n", hr, first, count);
        check(hr == S_OK && first > 0 && first <= count, "GetFirstFontResources");
        if (subset) IDWriteFontSet1_Release(subset);

        hr = IDWriteFontSet1_GetFontFaceReference(set1, 0, &ref1);
        check(hr == S_OK && ref1, "GetFontFaceReference gives an IDWriteFontFaceReference1");
        if (ref1) IDWriteFontFaceReference1_Release(ref1);
        hr = IDWriteFontSet1_CreateFontFace(set1, 0, &face5);
        check(hr == S_OK && face5, "CreateFontFace gives an IDWriteFontFace5");
        if (face5) IDWriteFontFace5_Release(face5);
        IDWriteFontSet1_Release(set1);
    }

    /* builder */
    hr = IDWriteFactory3_CreateFontSetBuilder(factory, &builder0);
    if (hr == S_OK) hr = IDWriteFontSetBuilder_QueryInterface(builder0, &IID_IDWriteFontSetBuilder2_sg, (void **)&builder);
    check(hr == S_OK, "an IDWriteFontSetBuilder2");
    if (hr == S_OK)
    {
        hr = IDWriteFontSetBuilder2_AddFontSet(builder, set);
        check(hr == S_OK, "AddFontSet");
        if (file)
        {
            hr = IDWriteFactory3_CreateFontFaceReference_(factory, file, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
            if (hr == S_OK)
            {
                hr = IDWriteFontSetBuilder2_AddFontFaceReference_(builder, ref, &tag, 1);
                check(hr == S_OK, "AddFontFaceReference with properties");
                IDWriteFontFaceReference_Release(ref);
            }
            hr = IDWriteFactory3_CreateFontFaceReference_(factory, file, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
            if (hr == S_OK)
            {
                hr = IDWriteFontSetBuilder2_AddFontFaceReference_(builder, ref, &named, 1);
                IDWriteFontFaceReference_Release(ref);
            }
            check(hr == S_OK, "AddFontFaceReference with a family name");
            hr = IDWriteFontSetBuilder2_AddFont(builder, file, 0, DWRITE_FONT_SIMULATIONS_BOLD, NULL, 0, NULL, 0, &tag, 1);
            check(hr == S_OK, "AddFont");
        }
        hr = IDWriteFontSetBuilder2_CreateFontSet(builder, &built);
        n = built ? IDWriteFontSet_GetFontCount(built) : 0;
        printf("built set: %#lx %u fonts\n", hr, n);
        check(hr == S_OK && n == count + 3, "the built set holds the system set and the three fonts");
        if (built)
        {
            hr = IDWriteFontSet_GetMatchingFonts(built, &tag, 1, &matched);
            n = matched ? IDWriteFontSet_GetFontCount(matched) : 0;
            check(hr == S_OK && n == 2, "the fonts added with a property are found by it");
            if (matched) IDWriteFontSet_Release(matched);
            hr = IDWriteFactory3_CreateFontCollectionFromFontSet(factory, built, &collection);
            if (hr == S_OK)
            {
                exists = FALSE;
                hr = IDWriteFontCollection1_FindFamilyName(collection, L"SG Given Family", &index, &exists);
                printf("given family: %#lx exists %d\n", hr, exists);
                check(hr == S_OK && exists, "a collection from the set has the family name the set gave");
                IDWriteFontCollection1_Release(collection);
            }
            else check(0, "CreateFontCollectionFromFontSet");
            IDWriteFontSet_Release(built);
        }
        IDWriteFontSetBuilder2_Release(builder);
        IDWriteFontSetBuilder_Release(builder0);
    }
    if (file) IDWriteFontFile_Release(file);
    IDWriteFontSet_Release(set);
    IDWriteFactory3_Release(factory);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
