/* dwlogfont-probe: IDWriteGdiInterop::CreateFontFromLOGFONT finds the font
 * GDI uses for a substituted name (patches/sg/0177).
 *
 * "Segoe UI", "MS Shell Dlg" and the like are FontSubstitutes on Stained
 * Glass (sg-shell's defaults map them to Inter); DirectWrite looked the name
 * up only as a family and failed with DWRITE_E_NOFONT, so programs that
 * shape with DirectWrite (WPF, Chromium, Pango) lost the UI font or fell
 * back to a different one than GDI draws. Prints face=gdi,dwrite,hr lines.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>

int main( int argc, char **argv )
{
    IDWriteFactory *factory;
    IDWriteGdiInterop *interop;
    HDC dc = CreateCompatibleDC( 0 );
    int i;

    DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory );
    IDWriteFactory_GetGdiInterop( factory, &interop );
    for (i = 1; i < argc; i++)
    {
        LOGFONTW lf = { 0 };
        WCHAR gdi[LF_FACESIZE] = L"", dw[LF_FACESIZE] = L"", path[MAX_PATH] = L"";
        IDWriteFont *font;
        HRESULT hr;

        lf.lfHeight = -16;
        lf.lfWeight = FW_NORMAL;
        MultiByteToWideChar( CP_ACP, 0, argv[i], -1, lf.lfFaceName, LF_FACESIZE );
        SelectObject( dc, CreateFontIndirectW( &lf ) );
        GetTextFaceW( dc, LF_FACESIZE, gdi );
        if (SUCCEEDED(hr = IDWriteGdiInterop_CreateFontFromLOGFONT( interop, &lf, &font )))
        {
            IDWriteFontFamily *family;
            IDWriteLocalizedStrings *names;
            IDWriteFont_GetFontFamily( font, &family );
            IDWriteFontFamily_GetFamilyNames( family, &names );
            IDWriteLocalizedStrings_GetString( names, 0, dw, LF_FACESIZE );
            /* the file the font comes from: what decides it is GDI's font,
             * whatever name either side reports for a substitute */
            {
                IDWriteFontFace *face;
                IDWriteFontFile *file;
                IDWriteFontFileLoader *loader;
                IDWriteLocalFontFileLoader *local;
                const void *key;
                UINT32 n = 1, keysize;
                if (SUCCEEDED(IDWriteFont_CreateFontFace( font, &face )) &&
                    SUCCEEDED(IDWriteFontFace_GetFiles( face, &n, &file )) &&
                    SUCCEEDED(IDWriteFontFile_GetReferenceKey( file, &key, &keysize )) &&
                    SUCCEEDED(IDWriteFontFile_GetLoader( file, &loader )) &&
                    SUCCEEDED(IDWriteFontFileLoader_QueryInterface( loader, &IID_IDWriteLocalFontFileLoader, (void **)&local )))
                    IDWriteLocalFontFileLoader_GetFilePathFromKey( local, key, keysize, path, MAX_PATH );
            }
        }
        printf( "%s=%ls,%ls,%#lx,%ls\n", argv[i], gdi, dw, hr, path );
    }
    return 0;
}
