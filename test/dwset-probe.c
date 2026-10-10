/* dwrite (patches/sg/2637): IDWriteFontCollection1::GetFontSet makes a set that does not count as a
 * reference of the factory or of the collection, as the Wine tests record from Windows; the
 * set can still be used after both are released. */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <dwrite_3.h>
#include <stdio.h>
#include <stdarg.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static ULONG refs(IUnknown *u)
{
    IUnknown_AddRef(u);
    return IUnknown_Release(u);
}

int main(void)
{
    IDWriteFactory3 *factory3;
    IDWriteFactory *factory;
    IDWriteFontCollection1 *collection;
    IDWriteFontSet *set, *set2, *sub;
    ULONG before, after, c_before;
    UINT32 count;
    HRESULT hr;

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory3, (IUnknown **)&factory3);
    if (!factory3) { printf("FAIL  no factory\nRESULT: FAIL\n"); return 1; }
    factory = (IDWriteFactory *)factory3;

    hr = IDWriteFactory3_GetSystemFontCollection(factory3, FALSE, &collection, FALSE);
    check(hr == S_OK, "collection (%#lx)", hr);
    before = refs((IUnknown *)factory);
    c_before = refs((IUnknown *)collection);
    hr = IDWriteFontCollection1_GetFontSet(collection, &set);
    check(hr == S_OK && set, "GetFontSet (%#lx)", hr);
    after = refs((IUnknown *)factory);
    check(after == before, "factory references %lu -> %lu", before, after);
    check(refs((IUnknown *)collection) == c_before, "collection references unchanged");
    check(refs((IUnknown *)set) == 1, "set has one reference");

    hr = IDWriteFontCollection1_GetFontSet(collection, &set2);
    check(hr == S_OK && set2 != set, "another set each time");
    IDWriteFontSet_Release(set2);

    /* the system font set holds a reference */
    hr = IDWriteFactory3_GetSystemFontSet(factory3, &set2);
    after = refs((IUnknown *)factory);
    check(hr == S_OK && after == before + 1, "system font set: factory references %lu (want %lu)", after, before + 1);
    IDWriteFontSet_Release(set2);

    /* released everything else: the set is alive and works */
    IDWriteFontCollection1_Release(collection);
    IDWriteFactory3_Release(factory3);
    count = IDWriteFontSet_GetFontCount(set);
    check(count > 0, "%u fonts after the factory is gone", count);
    {
        IDWriteFontFaceReference *ref = NULL;
        IDWriteFontFace3 *face = NULL;

        hr = IDWriteFontSet_GetFontFaceReference(set, 0, &ref);
        check(hr == S_OK && ref, "reference of the first font (%#lx)", hr);
        if (ref)
        {
            hr = IDWriteFontFaceReference_CreateFontFace(ref, &face);
            check(hr == S_OK && face, "and its face (%#lx)", hr);
            if (face) IDWriteFontFace3_Release(face);
            IDWriteFontFaceReference_Release(ref);
        }
    }
    {
        IDWriteFontSet1 *set1;

        if (SUCCEEDED(IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet1, (void **)&set1)))
        {
            DWRITE_FONT_PROPERTY prop = { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"Tahoma", L"en-us" };

            hr = IDWriteFontSet_GetMatchingFonts(set, &prop, 1, &sub);
            check(hr == S_OK && sub, "subset (%#lx)", hr);
            if (sub) IDWriteFontSet_Release(sub);
            IDWriteFontSet1_Release(set1);
        }
    }
    IDWriteFontSet_Release(set);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
