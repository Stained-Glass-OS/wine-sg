/* dwrite (patches/sg/2666): a face reference taken from a font set has the four axes weight, width, italic and
 * slant (the Wine tests record that native does), a reference made by the factory has none. */
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

int main()
{
    IDWriteFactory3 *factory = NULL;
    IDWriteFontCollection1 *collection = NULL;
    IDWriteFontSet *set = NULL;
    IDWriteFontFaceReference *ref = NULL;
    IDWriteFontFaceReference1 *ref1 = NULL;
    DWRITE_FONT_AXIS_VALUE axes[4];
    HRESULT hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory3), (IUnknown **)&factory);
    check(hr == S_OK && factory, "factory (%#lx)", hr);
    if (!factory) return 1;
    hr = factory->GetSystemFontCollection(FALSE, &collection, FALSE);
    check(hr == S_OK && collection, "system collection");
    hr = factory->GetSystemFontSet(&set);
    check(hr == S_OK && set && set->GetFontCount() > 0, "a system font set (%#lx)", hr);
    if (!set) return 1;

    hr = set->GetFontFaceReference(0, &ref);
    check(hr == S_OK && ref, "a reference from the set (%#lx)", hr);
    ref->QueryInterface(__uuidof(IDWriteFontFaceReference1), (void **)&ref1);
    check(ref1 != NULL, "IDWriteFontFaceReference1");
    if (ref1)
    {
        UINT32 count = ref1->GetFontAxisValueCount();
        check(count == 4, "four axis values (%u)", count);
        if (count == 4)
        {
            ref1->GetFontAxisValues(axes, 4);
            check(axes[0].axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT && axes[0].value >= 1 && axes[0].value <= 999, "weight %g", axes[0].value);
            check(axes[1].axisTag == DWRITE_FONT_AXIS_TAG_WIDTH && axes[1].value >= 50 && axes[1].value <= 200, "width %g", axes[1].value);
            check(axes[2].axisTag == DWRITE_FONT_AXIS_TAG_ITALIC && (axes[2].value == 0 || axes[2].value == 1), "italic %g", axes[2].value);
            check(axes[3].axisTag == DWRITE_FONT_AXIS_TAG_SLANT, "slant");
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
