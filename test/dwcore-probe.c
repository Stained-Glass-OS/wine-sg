/* test/dwcore-gate.sh's probe (7715): the core families in DirectWrite's system collection.
 *   dwcore-probe NAME...   for each: Found=NAME 0|1, the matching font's own Win32 family name
 *                          (FontOwn=NAME <name>) and whether it has a glyph for 'A' (HasA=NAME 0|1)
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dwrite_2.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    IDWriteFactory *factory;
    IDWriteFontCollection *collection;
    int i;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (FAILED(DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory ))) return 1;
    if (FAILED(IDWriteFactory_GetSystemFontCollection( factory, &collection, FALSE ))) return 1;
    for (i = 1; i < argc; i++)
    {
        IDWriteFontFamily *family;
        IDWriteFont *font;
        IDWriteLocalizedStrings *names;
        UINT32 index = 0;
        BOOL exists = FALSE, has = FALSE;
        WCHAR own[256] = L"";

        IDWriteFontCollection_FindFamilyName( collection, argv[i], &index, &exists );
        printf( "Found=%ls %d\n", argv[i], exists );
        if (!exists) continue;
        if (FAILED(IDWriteFontCollection_GetFontFamily( collection, index, &family ))) continue;
        if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont( family, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                              DWRITE_FONT_STYLE_NORMAL, &font )))
        {
            BOOL got = FALSE;
            if (SUCCEEDED(IDWriteFont_GetInformationalStrings( font, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &names, &got )) && got)
            {
                IDWriteLocalizedStrings_GetString( names, 0, own, ARRAYSIZE(own) );
                IDWriteLocalizedStrings_Release( names );
            }
            IDWriteFont_HasCharacter( font, 'A', &has );
            IDWriteFont_Release( font );
        }
        printf( "FontOwn=%ls %ls\nHasA=%ls %d\n", argv[i], own, argv[i], has );
        IDWriteFontFamily_Release( family );
    }
    return 0;
}
