/* dwrite: results Wine's tests record from Windows for font faces made from
 * files and references, and for the alpha textures of a glyph run analysis
 * (patches/sg/2615). The font file is the one of a system font. */
#define COBJMACROS
#include <windows.h>
#include <dwrite_3.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifndef DWRITE_E_UNSUPPORTEDOPERATION
#define DWRITE_E_UNSUPPORTEDOPERATION 0x88985009
#endif
#ifndef DWRITE_E_FILEFORMAT
#define DWRITE_E_FILEFORMAT 0x88985000
#endif
#ifndef DWRITE_E_FILENOTFOUND
#define DWRITE_E_FILENOTFOUND 0x88985003
#endif

#define IID_IDWriteLocalFontFileLoader my_iid_local
#define IID_IDWriteFactory3 my_iid_factory3
static const GUID my_iid_local = {0xb2d9f3ec, 0xc9fe, 0x4a11, {0xa2, 0xec, 0xd8, 0x62, 0x08, 0xf7, 0xc0, 0xa2}};
static const GUID my_iid_factory3 = {0x9a1b41c3, 0xd3bb, 0x466a, {0x87, 0xfc, 0xfe, 0x67, 0x55, 0x6a, 0x3b, 0x65}};

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

static IDWriteFactory3 *factory;
static IDWriteFontFile *file;
static IDWriteFontFace *base_face;
static WCHAR font_path[MAX_PATH];

static int find_system_font(void)
{
    IDWriteFontCollection *collection = NULL;
    IDWriteFontFamily *family = NULL;
    IDWriteFont *font = NULL;
    UINT32 index, count = 1;
    BOOL exists;
    HRESULT hr;
    static const WCHAR *names[] = { L"Tahoma", L"Arial", L"Segoe UI", L"Liberation Sans", L"DejaVu Sans" };
    unsigned i;

    IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)factory, &collection, FALSE);
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        if (SUCCEEDED(IDWriteFontCollection_FindFamilyName(collection, names[i], &index, &exists)) && exists) break;
        exists = FALSE;
    }
    if (!exists) index = 0;
    hr = IDWriteFontCollection_GetFontFamily(collection, index, &family);
    if (SUCCEEDED(hr)) hr = IDWriteFontFamily_GetFont(family, 0, &font);
    if (SUCCEEDED(hr)) hr = IDWriteFont_CreateFontFace(font, &base_face);
    if (SUCCEEDED(hr)) hr = IDWriteFontFace_GetFiles(base_face, &count, &file);
    if (font) IDWriteFont_Release(font);
    if (family) IDWriteFontFamily_Release(family);
    IDWriteFontCollection_Release(collection);
    return SUCCEEDED(hr) && file;
}

static BOOL get_path(void)
{
    IDWriteFontFileLoader *loader = NULL;
    IDWriteLocalFontFileLoader *local = NULL;
    const void *key;
    UINT32 key_size, len;
    BOOL ok = FALSE;

    if (FAILED(IDWriteFontFile_GetLoader(file, &loader))) return FALSE;
    if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(file, &key, &key_size)) &&
        SUCCEEDED(IDWriteFontFileLoader_QueryInterface(loader, &IID_IDWriteLocalFontFileLoader, (void **)&local)))
    {
        if (SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(local, key, key_size, &len)) && len < MAX_PATH)
            ok = SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathFromKey(local, key, key_size, font_path, MAX_PATH));
        IDWriteLocalFontFileLoader_Release(local);
    }
    IDWriteFontFileLoader_Release(loader);
    return ok;
}

static void test_faces(void)
{
    IDWriteFontFace *face;
    HRESULT hr;

    face = (void *)0xdeadbeef;
    hr = IDWriteFactory3_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_RAW_CFF, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    check(hr == (HRESULT)DWRITE_E_UNSUPPORTEDOPERATION && !face, "RAW_CFF is unsupported (%#lx, %p)", hr, face);

    face = (void *)0xdeadbeef;
    hr = IDWriteFactory3_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_TYPE1, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    check(hr == E_INVALIDARG && !face, "TYPE1 is an invalid argument (%#lx)", hr);

    face = (void *)0xdeadbeef;
    hr = IDWriteFactory3_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_BITMAP, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    check(hr == E_INVALIDARG && !face, "BITMAP is an invalid argument (%#lx)", hr);

    face = (void *)0xdeadbeef;
    hr = IDWriteFactory3_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_CFF, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    check((hr == (HRESULT)DWRITE_E_FILEFORMAT && !face) || hr == S_OK, "CFF for a file that has another type (%#lx)", hr);
    if (hr == S_OK && face) IDWriteFontFace_Release(face);

    face = (void *)0xdeadbeef;
    hr = IDWriteFactory3_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &file, 0, 0xf, &face);
    check(hr == E_INVALIDARG, "UNKNOWN with bad simulations (%#lx)", hr);

    face = NULL;
    hr = IDWriteFactory3_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    check(hr == S_OK && face, "UNKNOWN takes the type of the file (%#lx)", hr);
    if (face)
    {
        DWRITE_FONT_FACE_TYPE type = IDWriteFontFace_GetType(face), base_type = IDWriteFontFace_GetType(base_face);

        check(type == base_type && type != DWRITE_FONT_FACE_TYPE_UNKNOWN, "and the face has it (%d vs %d)", type, base_type);
        IDWriteFontFace_Release(face);
    }

    face = (void *)0xdeadbeef;
    hr = IDWriteFactory3_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &file, 1, DWRITE_FONT_SIMULATIONS_NONE, &face);
    check(hr == E_INVALIDARG && !face, "UNKNOWN with a face index (%#lx)", hr);
}

static void test_references(void)
{
    IDWriteFontFaceReference *ref = (void *)0xdeadbeef;
    IDWriteFontFace3 *face = NULL;
    HRESULT hr;

    if (!font_path[0]) { check(0, "the path of the font file"); return; }

    hr = IDWriteFactory3_CreateFontFaceReference(factory, font_path, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    check(hr == S_OK && ref, "a reference to the file (%#lx)", hr);
    if (ref)
    {
        hr = IDWriteFontFaceReference_CreateFontFace(ref, &face);
        check(hr == S_OK && face, "its face (%#lx)", hr);
        if (face) IDWriteFontFace3_Release(face);
        IDWriteFontFaceReference_Release(ref);
    }

    ref = NULL;
    hr = IDWriteFactory3_CreateFontFaceReference(factory, font_path, NULL, 1, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    check(hr == S_OK && ref, "a reference with a face index past the file's (%#lx)", hr);
    if (ref)
    {
        face = (void *)0xdeadbeef;
        hr = IDWriteFontFaceReference_CreateFontFace(ref, &face);
        check(hr == (HRESULT)DWRITE_E_FILEFORMAT, "but no face can be made of it (%#lx)", hr);
        IDWriteFontFaceReference_Release(ref);
    }

    ref = (void *)0xdeadbeef;
    hr = IDWriteFactory3_CreateFontFaceReference(factory, L"dummy", NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    check(hr == (HRESULT)DWRITE_E_FILENOTFOUND && !ref, "a reference to a file that is not there (%#lx, %p)", hr, ref);
    if (SUCCEEDED(hr) && ref) IDWriteFontFaceReference_Release(ref);

    ref = NULL;
    hr = IDWriteFactory3_CreateFontFaceReference_(factory, file, 1, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    check(hr == S_OK && ref, "a reference made of a file object needs no path (%#lx)", hr);
    if (ref) IDWriteFontFaceReference_Release(ref);
}

static void test_alpha_textures(void)
{
    IDWriteGlyphRunAnalysis *analysis = NULL;
    DWRITE_GLYPH_RUN run = {0};
    UINT16 glyph = 0;
    UINT32 cp = 'A';
    FLOAT advance = 10.0f;
    RECT rect;
    BYTE *bits, small[2];
    UINT32 size;
    HRESULT hr;

    IDWriteFontFace_GetGlyphIndices(base_face, &cp, 1, &glyph);
    run.fontFace = base_face;
    run.fontEmSize = 40.0f;
    run.glyphCount = 1;
    run.glyphIndices = &glyph;
    run.glyphAdvances = &advance;

    /* an aliased analysis */
    hr = IDWriteFactory_CreateGlyphRunAnalysis((IDWriteFactory *)factory, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED,
            DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, &analysis);
    check(hr == S_OK && analysis, "an aliased analysis (%#lx)", hr);
    if (analysis)
    {
        SetRectEmpty(&rect);
        hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(analysis, DWRITE_TEXTURE_ALIASED_1x1, &rect);
        size = (rect.right - rect.left) * (rect.bottom - rect.top);
        check(hr == S_OK && size > 4, "bounds (%#lx, %u)", hr, size);
        bits = malloc(size * 3);

        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_ALIASED_1x1, &rect, bits, size);
        check(hr == S_OK, "aliased texture (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_ALIASED_1x1, &rect, bits, size - 1);
        check(hr == E_NOT_SUFFICIENT_BUFFER, "aliased texture, buffer too small (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &rect, bits, size);
        check(hr == (HRESULT)DWRITE_E_UNSUPPORTEDOPERATION, "ClearType texture of it (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &rect, bits, size - 1);
        check(hr == (HRESULT)DWRITE_E_UNSUPPORTEDOPERATION, "ClearType texture of it, buffer too small (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &rect, bits, size * 3);
        check(hr == (HRESULT)DWRITE_E_UNSUPPORTEDOPERATION, "ClearType texture of it, buffer large (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_CLEARTYPE_3x1 + 1, &rect, bits, size);
        check(hr == E_INVALIDARG, "invalid texture type (%#lx)", hr);
        free(bits);
        IDWriteGlyphRunAnalysis_Release(analysis);
    }

    /* a ClearType analysis */
    analysis = NULL;
    hr = IDWriteFactory_CreateGlyphRunAnalysis((IDWriteFactory *)factory, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL,
            DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, &analysis);
    check(hr == S_OK && analysis, "a ClearType analysis (%#lx)", hr);
    if (analysis)
    {
        SetRectEmpty(&rect);
        hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &rect);
        size = (rect.right - rect.left) * (rect.bottom - rect.top) * 3;
        check(hr == S_OK && size > 12, "ClearType bounds (%#lx, %u)", hr, size);
        bits = malloc(size);

        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &rect, bits, size);
        check(hr == S_OK, "ClearType texture (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &rect, bits, size - 1);
        check(hr == E_NOT_SUFFICIENT_BUFFER, "ClearType texture, buffer too small (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_ALIASED_1x1, &rect, small, 2);
        check(hr == E_NOT_SUFFICIENT_BUFFER, "aliased texture of it, buffer too small (%#lx)", hr);
        hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, DWRITE_TEXTURE_ALIASED_1x1, &rect, bits, size);
        check(hr == (HRESULT)DWRITE_E_UNSUPPORTEDOPERATION, "aliased texture of it (%#lx)", hr);
        free(bits);
        IDWriteGlyphRunAnalysis_Release(analysis);
    }
}

int main(void)
{
    HRESULT hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory3, (IUnknown **)&factory);
    if (FAILED(hr)) { printf("FAIL  no factory %#lx\nRESULT: FAIL\n", hr); return 1; }
    if (!find_system_font()) { printf("FAIL  no system font\nRESULT: FAIL\n"); return 1; }
    get_path();

    test_faces();
    test_references();
    test_alpha_textures();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
