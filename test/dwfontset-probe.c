/* dwfontset-gate.sh's probe (0503): the system font collection as a font
 * set (IDWriteFontCollection1::GetFontSet), and its entries' family names. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dwrite_3.h>
#include <stdio.h>

DEFINE_GUID(IID_IDWriteFactory3_sg, 0x9a1b41c3,0xd3bb,0x466a,0x87,0xfc,0xfe,0x67,0x55,0x6a,0x3b,0x65);
DEFINE_GUID(IID_IDWriteFontCollection1_sg, 0x53585141,0xd9f8,0x4095,0x83,0x21,0xd7,0x3c,0xf6,0xbd,0x11,0x6c);

int main(void)
{
    IDWriteFactory3 *factory;
    IDWriteFontCollection1 *collection;
    IDWriteFontSet *set;
    UINT32 count = 0, families, nnames = 0, i;
    HRESULT hr;
    BOOL tahoma = FALSE;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory3_sg, (IUnknown **)&factory)))
        return 1;
    IDWriteFactory3_GetSystemFontCollection(factory, FALSE, &collection, FALSE);
    families = IDWriteFontCollection1_GetFontFamilyCount(collection);
    hr = IDWriteFontCollection1_GetFontSet(collection, &set);
    printf("getfontset %08lx\n", hr);
    if (FAILED(hr)) return 0;
    count = IDWriteFontSet_GetFontCount(set);
    printf("fonts %d\n", count >= families && count > 0);
    hr = S_OK;
    for (i = 0; i < count; i++)
    {
        IDWriteLocalizedStrings *strings = NULL;
        BOOL exists = FALSE;
        WCHAR buf[128];

        if (FAILED(hr = IDWriteFontSet_GetPropertyValues(set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME,
                                                         &exists, &strings)))
            break;
        if (!exists || !strings) continue;
        nnames++;
        if (SUCCEEDED(IDWriteLocalizedStrings_GetString(strings, 0, buf, ARRAYSIZE(buf))) && !lstrcmpiW(buf, L"Tahoma"))
            tahoma = TRUE;
        IDWriteLocalizedStrings_Release(strings);
    }
    printf("familynames %08lx %d %d\n", hr, nnames > 0, tahoma);
    return 0;
}
