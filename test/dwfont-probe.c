/* dwrite (patches/sg/2636): what the Wine tests record from Windows. A font's reference carries
 * the weight, width, italic and slant axis values (a reference of a file carries none); the
 * alpha blend parameters of grayscale glyphs have no cleartype level; and a face can be made of a
 * file whose loader belongs to another factory. */
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

/* a loader and a stream over a file read into memory */
struct stream { IDWriteFontFileStream IDWriteFontFileStream_iface; LONG ref; BYTE *data; UINT64 size; };
static struct stream *impl_stream(IDWriteFontFileStream *iface) { return (struct stream *)iface; }
static HRESULT WINAPI stream_QI(IDWriteFontFileStream *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFileStream))
    {
        *obj = iface;
        IDWriteFontFileStream_AddRef(iface);
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI stream_AddRef(IDWriteFontFileStream *iface) { return InterlockedIncrement(&impl_stream(iface)->ref); }
static ULONG WINAPI stream_Release(IDWriteFontFileStream *iface)
{
    struct stream *s = impl_stream(iface);
    ULONG ref = InterlockedDecrement(&s->ref);

    if (!ref) { free(s->data); free(s); }
    return ref;
}
static HRESULT WINAPI stream_ReadFileFragment(IDWriteFontFileStream *iface, const void **start, UINT64 offset, UINT64 size, void **ctx)
{
    struct stream *s = impl_stream(iface);

    *ctx = NULL;
    if (offset + size > s->size) { *start = NULL; return E_FAIL; }
    *start = s->data + offset;
    return S_OK;
}
static void WINAPI stream_ReleaseFileFragment(IDWriteFontFileStream *iface, void *ctx) { }
static HRESULT WINAPI stream_GetFileSize(IDWriteFontFileStream *iface, UINT64 *size) { *size = impl_stream(iface)->size; return S_OK; }
static HRESULT WINAPI stream_GetLastWriteTime(IDWriteFontFileStream *iface, UINT64 *t) { *t = 0; return S_OK; }
static const IDWriteFontFileStreamVtbl stream_vtbl = { stream_QI, stream_AddRef, stream_Release, stream_ReadFileFragment,
    stream_ReleaseFileFragment, stream_GetFileSize, stream_GetLastWriteTime };

static WCHAR font_path[MAX_PATH];

static HRESULT WINAPI loader_QI(IDWriteFontFileLoader *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFileLoader))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI loader_AddRef(IDWriteFontFileLoader *iface) { return 2; }
static ULONG WINAPI loader_Release(IDWriteFontFileLoader *iface) { return 1; }
static HRESULT WINAPI loader_CreateStreamFromKey(IDWriteFontFileLoader *iface, const void *key, UINT32 key_size, IDWriteFontFileStream **out)
{
    struct stream *s = calloc(1, sizeof(*s));
    HANDLE file;
    DWORD read;

    *out = NULL;
    file = CreateFileW(font_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE) { free(s); return E_FAIL; }
    s->size = GetFileSize(file, NULL);
    s->data = malloc(s->size);
    ReadFile(file, s->data, s->size, &read, NULL);
    CloseHandle(file);
    s->IDWriteFontFileStream_iface.lpVtbl = &stream_vtbl;
    s->ref = 1;
    *out = &s->IDWriteFontFileStream_iface;
    return S_OK;
}
static const IDWriteFontFileLoaderVtbl loader_vtbl = { loader_QI, loader_AddRef, loader_Release, loader_CreateStreamFromKey };
static IDWriteFontFileLoader loader = { &loader_vtbl };

static BOOL has_axis(const DWRITE_FONT_AXIS_VALUE *v, unsigned int n, DWRITE_FONT_AXIS_TAG tag)
{
    unsigned int i;

    for (i = 0; i < n; i++) if (v[i].axisTag == tag) return TRUE;
    return FALSE;
}

int main(void)
{
    IDWriteFactory3 *factory3;
    IDWriteFactory *factory, *factory2;
    IDWriteFontCollection1 *collection;
    IDWriteFontFamily1 *family;
    IDWriteFontFaceReference *ref;
    IDWriteFontFaceReference1 *ref1;
    IDWriteFont3 *font;
    IDWriteFontFile *file;
    HRESULT hr;

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory3, (IUnknown **)&factory3);
    if (!factory3) { printf("FAIL  no factory\nRESULT: FAIL\n"); return 1; }

    hr = IDWriteFactory3_GetSystemFontCollection(factory3, FALSE, &collection, FALSE);
    check(hr == S_OK, "system collection (%#lx)", hr);
    IDWriteFontCollection1_GetFontFamily(collection, 0, &family);
    IDWriteFontFamily1_GetFont(family, 0, &font);
    hr = IDWriteFont3_GetFontFaceReference(font, &ref);
    check(hr == S_OK, "reference of a font (%#lx)", hr);
    if (SUCCEEDED(IDWriteFontFaceReference_QueryInterface(ref, &IID_IDWriteFontFaceReference1, (void **)&ref1)))
    {
        DWRITE_FONT_AXIS_VALUE values[16];
        unsigned int n = IDWriteFontFaceReference1_GetFontAxisValueCount(ref1);

        check(n >= 4 && n <= 16, "axis count %u", n);
        hr = IDWriteFontFaceReference1_GetFontAxisValues(ref1, values, 16);
        check(hr == S_OK && has_axis(values, n, DWRITE_FONT_AXIS_TAG_WEIGHT) && has_axis(values, n, DWRITE_FONT_AXIS_TAG_WIDTH) &&
              has_axis(values, n, DWRITE_FONT_AXIS_TAG_ITALIC) && has_axis(values, n, DWRITE_FONT_AXIS_TAG_SLANT),
              "weight, width, italic and slant (%#lx)", hr);
        IDWriteFontFaceReference1_Release(ref1);
    }
    IDWriteFontFaceReference_Release(ref);

    /* a reference of a file has none */
    {
        IDWriteFontFace *face0;
        IDWriteFontFile *file0;
        IDWriteFontFileLoader *loader0;
        IDWriteLocalFontFileLoader *local;
        UINT32 count = 1, key_size;
        const void *key;

        IDWriteFont3_CreateFontFace(font, (IDWriteFontFace3 **)&face0);
        IDWriteFontFace_GetFiles(face0, &count, &file0);
        IDWriteFontFile_GetReferenceKey(file0, &key, &key_size);
        IDWriteFontFile_GetLoader(file0, &loader0);
        IDWriteFontFileLoader_QueryInterface(loader0, &IID_IDWriteLocalFontFileLoader, (void **)&local);
        IDWriteLocalFontFileLoader_GetFilePathFromKey(local, key, key_size, font_path, MAX_PATH);
        IDWriteLocalFontFileLoader_Release(local);
        IDWriteFontFileLoader_Release(loader0);
        IDWriteFontFile_Release(file0);
        IDWriteFontFace_Release(face0);
    }
    hr = IDWriteFactory3_CreateFontFileReference(factory3, font_path, NULL, &file);
    check(hr == S_OK, "file reference (%#lx)", hr);
    hr = factory3->lpVtbl->CreateFontFaceReference_(factory3, file, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    check(hr == S_OK, "face reference (%#lx)", hr);
    if (SUCCEEDED(IDWriteFontFaceReference_QueryInterface(ref, &IID_IDWriteFontFaceReference1, (void **)&ref1)))
    {
        check(!IDWriteFontFaceReference1_GetFontAxisValueCount(ref1), "no axis values from a file");
        IDWriteFontFaceReference1_Release(ref1);
    }
    IDWriteFontFaceReference_Release(ref);
    IDWriteFontFile_Release(file);

    /* a file of a loader registered with one factory, a face made by another */
    IDWriteFactory3_QueryInterface(factory3, &IID_IDWriteFactory, (void **)&factory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&factory2);
    hr = IDWriteFactory_RegisterFontFileLoader(factory, &loader);
    check(hr == S_OK, "register loader (%#lx)", hr);
    hr = IDWriteFactory_CreateCustomFontFileReference(factory, "key", 3, &loader, &file);
    check(hr == S_OK, "custom file (%#lx)", hr);
    {
        IDWriteFontFace *face = NULL, *face2 = NULL;

        hr = IDWriteFactory_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &file, 0, 0, &face);
        check(hr == S_OK && face, "face of the registering factory (%#lx)", hr);
        hr = IDWriteFactory_CreateFontFace(factory2, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &file, 0, 0, &face2);
        check(hr == S_OK && face2, "face of the other factory (%#lx)", hr);
        if (face2)
        {
            UINT16 glyph = 0;
            UINT32 cp = 'A';

            hr = IDWriteFontFace_GetGlyphIndices(face2, &cp, 1, &glyph);
            check(hr == S_OK && glyph, "and it works (glyph %u)", glyph);
            IDWriteFontFace_Release(face2);
        }
        if (face) IDWriteFontFace_Release(face);
    }
    IDWriteFontFile_Release(file);

    /* alpha blend parameters */
    {
        IDWriteFactory2 *f2;
        IDWriteFontFace *face;
        IDWriteRenderingParams *params;
        IDWriteGlyphRunAnalysis *analysis;
        UINT16 glyph = 36;
        FLOAT advance = 10.0f;
        DWRITE_GLYPH_RUN run = { 0 };
        FLOAT gamma, contrast, level;
        struct { DWRITE_RENDERING_MODE mode; DWRITE_TEXT_ANTIALIAS_MODE aa; float want; const char *name; } modes[] =
        {
            { DWRITE_RENDERING_MODE_ALIASED, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, "aliased grayscale" },
            { DWRITE_RENDERING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, "natural grayscale" },
            { DWRITE_RENDERING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 1.0f, "natural cleartype" },
            { DWRITE_RENDERING_MODE_ALIASED, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 1.0f, "aliased cleartype" },
        };
        unsigned int i;

        IDWriteFactory3_QueryInterface(factory3, &IID_IDWriteFactory2, (void **)&f2);
        hr = IDWriteFactory3_CreateFontFileReference(factory3, font_path, NULL, &file);
        IDWriteFactory3_CreateFontFace(factory3, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &file, 0, 0, (IDWriteFontFace **)&face);
        run.fontFace = face; run.fontEmSize = 20.0f; run.glyphCount = 1; run.glyphIndices = &glyph; run.glyphAdvances = &advance;
        IDWriteFactory_CreateCustomRenderingParams(factory, 0.1f, 0.0f, 1.0f, DWRITE_PIXEL_GEOMETRY_FLAT,
                DWRITE_RENDERING_MODE_NATURAL, &params);
        for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
        {
            hr = IDWriteFactory2_CreateGlyphRunAnalysis(f2, &run, NULL, modes[i].mode, DWRITE_MEASURING_MODE_NATURAL,
                    DWRITE_GRID_FIT_MODE_DISABLED, modes[i].aa, 0.0f, 0.0f, &analysis);
            if (FAILED(hr)) { check(0, "%s analysis (%#lx)", modes[i].name, hr); continue; }
            level = -1.0f;
            hr = IDWriteGlyphRunAnalysis_GetAlphaBlendParams(analysis, params, &gamma, &contrast, &level);
            check(hr == S_OK && level == modes[i].want, "%s: cleartype level %g (want %g)", modes[i].name, level, modes[i].want);
            IDWriteGlyphRunAnalysis_Release(analysis);
        }
        IDWriteRenderingParams_Release(params);
        IDWriteFontFace_Release(face);
        IDWriteFontFile_Release(file);
        IDWriteFactory2_Release(f2);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
