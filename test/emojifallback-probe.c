/* emojifallback-probe: for emojifallback-gate.sh (issue #15, emoji showed as boxes).
 *   emojifallback-probe.exe map CP...   DirectWrite's system font fallback (Chromium/Skia, Firefox
 *                                       and WinUI use it) for each code point (hex) the base font
 *                                       ("Segoe UI") lacks: "MAP <cp> <family> <glyph formats>"
 *                                       ("none" when it finds no font)
 *   emojifallback-probe.exe draw CP     Direct2D DrawText of the character in "Segoe UI" with colour
 *                                       fonts on, 64 px, on white: "DRAW <cp> ink N colour N"
 *                                       (pixels drawn, and of those how many have colour)
 * Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>

struct source
{
    IDWriteTextAnalysisSource iface;
    const WCHAR *text;
    UINT32 len;
};

static HRESULT WINAPI src_qi(IDWriteTextAnalysisSource *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IDWriteTextAnalysisSource) || IsEqualIID(riid, &IID_IUnknown)) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI src_addref(IDWriteTextAnalysisSource *iface) { return 2; }
static ULONG WINAPI src_release(IDWriteTextAnalysisSource *iface) { return 1; }
static HRESULT WINAPI src_text_at(IDWriteTextAnalysisSource *iface, UINT32 pos, const WCHAR **text, UINT32 *len)
{
    struct source *s = (struct source *)iface;
    if (pos >= s->len) { *text = NULL; *len = 0; return S_OK; }
    *text = s->text + pos;
    *len = s->len - pos;
    return S_OK;
}
static HRESULT WINAPI src_text_before(IDWriteTextAnalysisSource *iface, UINT32 pos, const WCHAR **text, UINT32 *len)
{
    struct source *s = (struct source *)iface;
    *text = pos && pos <= s->len ? s->text : NULL;
    *len = pos <= s->len ? pos : 0;
    return S_OK;
}
static DWRITE_READING_DIRECTION WINAPI src_dir(IDWriteTextAnalysisSource *iface) { return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT; }
static HRESULT WINAPI src_locale(IDWriteTextAnalysisSource *iface, UINT32 pos, UINT32 *len, const WCHAR **locale)
{
    struct source *s = (struct source *)iface;
    *len = s->len - pos;
    *locale = L"en-us";
    return S_OK;
}
static HRESULT WINAPI src_subst(IDWriteTextAnalysisSource *iface, UINT32 pos, UINT32 *len, IDWriteNumberSubstitution **subst)
{
    *subst = NULL;
    return S_OK;
}
static IDWriteTextAnalysisSourceVtbl src_vtbl =
    { src_qi, src_addref, src_release, src_text_at, src_text_before, src_dir, src_locale, src_subst };

static UINT32 to_utf16(unsigned int cp, WCHAR *text)
{
    if (cp > 0xffff) { text[0] = 0xd800 + ((cp - 0x10000) >> 10); text[1] = 0xdc00 + ((cp - 0x10000) & 0x3ff); text[2] = 0; return 2; }
    text[0] = cp; text[1] = 0;
    return 1;
}

static void formats_str(DWRITE_GLYPH_IMAGE_FORMATS f, char *out, size_t size)
{
    snprintf(out, size, "%s%s%s%s%s",
             f & DWRITE_GLYPH_IMAGE_FORMATS_PNG ? "png," : "",
             f & DWRITE_GLYPH_IMAGE_FORMATS_COLR ? "colr," : "",
             f & DWRITE_GLYPH_IMAGE_FORMATS_SVG ? "svg," : "",
             f & DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8 ? "bgra," : "",
             f & (DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_CFF) ? "outline," : "");
    if (!*out) snprintf(out, size, "-");
}

static void map(IDWriteFactory2 *factory, const char *arg)
{
    IDWriteFontFallback *fallback;
    IDWriteFontCollection *collection;
    WCHAR text[16];
    struct source src = { { &src_vtbl }, text, 0 };
    IDWriteFont *font = NULL;
    unsigned int cp = strtoul(arg, NULL, 16);
    const char *p;
    UINT32 mapped = 0;
    FLOAT scale = 0;
    char name[256] = "none", formats[64] = "-";

    IDWriteFactory2_GetSystemFontFallback(factory, &fallback);
    IDWriteFactory2_GetSystemFontCollection(factory, &collection, FALSE);
    /* "2764+FE0F": a sequence, one string */
    for (p = arg; p; p = strchr(p, '+') ? strchr(p, '+') + 1 : NULL)
        src.len += to_utf16(strtoul(p, NULL, 16), text + src.len);
    IDWriteFontFallback_MapCharacters(fallback, &src.iface, 0, src.len, collection, L"Segoe UI",
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped, &font, &scale);
    if (font)
    {
        IDWriteFontFamily *family;
        IDWriteLocalizedStrings *names;
        IDWriteFontFace *face;
        IDWriteFontFace4 *face4;
        WCHAR wname[128];
        UINT16 glyph = 0;

        IDWriteFont_GetFontFamily(font, &family);
        IDWriteFontFamily_GetFamilyNames(family, &names);
        IDWriteLocalizedStrings_GetString(names, 0, wname, ARRAYSIZE(wname));
        WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name), NULL, NULL);
        IDWriteFont_CreateFontFace(font, &face);
        IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &glyph);
        {
            DWRITE_GLYPH_METRICS gm = { 0 };
            IDWriteFontFace_GetDesignGlyphMetrics(face, &glyph, 1, &gm, FALSE);
            INT32 adv = -1, gdi = -1;
            IDWriteFontFace1 *f1;
            if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1)))
            {
                IDWriteFontFace1_GetDesignGlyphAdvances(f1, 1, &glyph, &adv, FALSE);
                IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(f1, 24.0f, 1.0f, NULL, FALSE, FALSE, 1, &glyph, &gdi);
            }
            printf("ADV %X glyph %u advance %u design %d gdi %d\n", cp, glyph, gm.advanceWidth, adv, gdi);
        }
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace4, (void **)&face4)))
        {
            DWRITE_GLYPH_IMAGE_FORMATS f = 0;
            IDWriteFontFace4_GetGlyphImageFormats_(face4, glyph, 0, 1000, &f);
            formats_str(f, formats, sizeof(formats));
        }
    }
    {
        /* how wide text layout makes the character: a glyph without an advance stacks on the next */
        IDWriteTextFormat *format;
        IDWriteTextLayout *layout;
        DWRITE_TEXT_METRICS tm = { 0 };
        IDWriteFactory2_CreateTextFormat(factory, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, 100.0f, L"en-us", &format);
        IDWriteFactory2_CreateTextLayout(factory, text, src.len, format, 1000.0f, 1000.0f, &layout);
        IDWriteTextLayout_GetMetrics(layout, &tm);
        printf("MAP %s %s %s width %d mapped %u lines %u\n", arg, name, formats,
               (int)(tm.widthIncludingTrailingWhitespace + 0.5f), mapped, tm.lineCount);
    }
}

static void draw(IDWriteFactory2 *factory, unsigned int cp)
{
    ID2D1Factory *d2d;
    IWICImagingFactory *wic;
    IWICBitmap *bitmap;
    ID2D1RenderTarget *rt;
    ID2D1SolidColorBrush *brush;
    IDWriteTextFormat *format;
    D2D1_RENDER_TARGET_PROPERTIES props = { D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96, 0, 0 };
    D2D1_COLOR_F white = { 1, 1, 1, 1 }, black = { 0, 0, 0, 1 };
    D2D1_RECT_F rect = { 0, 0, 128, 128 };
    WICRect lock_rect = { 0, 0, 128, 128 };
    IWICBitmapLock *lock;
    UINT stride, size;
    BYTE *bits;
    WCHAR text[3];
    UINT32 len = to_utf16(cp, text);
    unsigned int ink = 0, colour = 0, x, y;

    CoInitialize(NULL);
    HRESULT hr;
    int step = 0;
    if (FAILED(hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic)) ||
        (step = 1, FAILED(hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&d2d))) ||
        (step = 2, FAILED(hr = IWICImagingFactory_CreateBitmap(wic, 128, 128, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bitmap))) ||
        (step = 3, FAILED(hr = ID2D1Factory_CreateWicBitmapRenderTarget(d2d, bitmap, &props, &rt))))
    { printf("DRAW %X nod2d step %d hr %#lx\n", cp, step, hr); return; }
    IDWriteFactory2_CreateTextFormat(factory, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 64.0f, L"en-us", &format);
    ID2D1RenderTarget_CreateSolidColorBrush(rt, &black, NULL, &brush);
    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_Clear(rt, &white);
    ID2D1RenderTarget_DrawText(rt, text, len, format, &rect, (ID2D1Brush *)brush,
                               D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, DWRITE_MEASURING_MODE_NATURAL);
    ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    IWICBitmap_Lock(bitmap, &lock_rect, WICBitmapLockRead, &lock);
    IWICBitmapLock_GetStride(lock, &stride);
    IWICBitmapLock_GetDataPointer(lock, &size, &bits);
    for (y = 0; y < 128; y++)
        for (x = 0; x < 128; x++)
        {
            BYTE *p = bits + y * stride + x * 4;
            int b = p[0], g = p[1], r = p[2];
            int hi = max(r, max(g, b)), lo = min(r, min(g, b));
            if (hi < 230 || lo < 230) ink++;
            if (hi - lo > 60) colour++;
        }
    IWICBitmapLock_Release(lock);
    printf("DRAW %X ink %u colour %u\n", cp, ink, colour);
}

int main(int argc, char **argv)
{
    IDWriteFactory2 *factory;
    int i;

    if (argc < 3) return 2;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory2, (IUnknown **)&factory)))
    { printf("NOFACTORY\n"); return 1; }
    for (i = 2; i < argc; i++)
    {
        unsigned int cp = strtoul(argv[i], NULL, 16);
        if (!strcmp(argv[1], "map")) map(factory, argv[i]);
        else draw(factory, cp);
        fflush(stdout);
    }
    return 0;
}
