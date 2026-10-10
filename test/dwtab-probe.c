/* dwrite (patches/sg/2647): a tab in a text layout is as wide as the distance to the next multiple of the
 * incremental tab stop, counted from the start of the line, and the glyph that is drawn advances as far
 * (the Wine tests record the cluster metrics from Windows). */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <dwrite_3.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

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

/* a text renderer that remembers where the glyph of 'b' is drawn */
struct renderer { IDWriteTextRenderer iface; float b_x; int found; };
static struct renderer *impl(IDWriteTextRenderer *i) { return (struct renderer *)i; }
static HRESULT WINAPI r_QI(IDWriteTextRenderer *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IDWriteTextRenderer) || IsEqualGUID(iid, &IID_IDWritePixelSnapping))
    { *out = iface; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI r_AddRef(IDWriteTextRenderer *iface) { return 2; }
static ULONG WINAPI r_Release(IDWriteTextRenderer *iface) { return 1; }
static HRESULT WINAPI r_IsPixelSnappingDisabled(IDWriteTextRenderer *iface, void *ctx, BOOL *disabled) { *disabled = TRUE; return S_OK; }
static HRESULT WINAPI r_GetCurrentTransform(IDWriteTextRenderer *iface, void *ctx, DWRITE_MATRIX *m)
{ m->m11 = 1; m->m12 = 0; m->m21 = 0; m->m22 = 1; m->dx = 0; m->dy = 0; return S_OK; }
static HRESULT WINAPI r_GetPixelsPerDip(IDWriteTextRenderer *iface, void *ctx, FLOAT *ppd) { *ppd = 1.0f; return S_OK; }
static HRESULT WINAPI r_DrawGlyphRun(IDWriteTextRenderer *iface, void *ctx, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mode,
        const DWRITE_GLYPH_RUN *run, const DWRITE_GLYPH_RUN_DESCRIPTION *desc, IUnknown *effect)
{
    struct renderer *r = impl(iface);
    UINT32 i, b = 0, count;
    UINT16 b_glyph = 0;
    float pos = x;
    UINT32 cp = 'b';

    IDWriteFontFace_GetGlyphIndices(run->fontFace, &cp, 1, &b_glyph);
    count = run->glyphCount;
    for (i = 0; i < count; i++)
    {
        if (run->glyphIndices[i] == b_glyph && !r->found) { r->b_x = pos; r->found = 1; b = 1; }
        pos += run->glyphAdvances[i];
    }
    (void)b;
    return S_OK;
}
static HRESULT WINAPI r_DrawUnderline(IDWriteTextRenderer *iface, void *c, FLOAT x, FLOAT y, const DWRITE_UNDERLINE *u, IUnknown *e) { return S_OK; }
static HRESULT WINAPI r_DrawStrikethrough(IDWriteTextRenderer *iface, void *c, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH *s, IUnknown *e) { return S_OK; }
static HRESULT WINAPI r_DrawInlineObject(IDWriteTextRenderer *iface, void *c, FLOAT x, FLOAT y, IDWriteInlineObject *o, BOOL s, BOOL r, IUnknown *e) { return S_OK; }
static const IDWriteTextRendererVtbl r_vtbl = { r_QI, r_AddRef, r_Release, r_IsPixelSnappingDisabled, r_GetCurrentTransform,
    r_GetPixelsPerDip, r_DrawGlyphRun, r_DrawUnderline, r_DrawStrikethrough, r_DrawInlineObject };

int main(void)
{
    IDWriteFactory *factory = NULL;
    IDWriteTextFormat *format = NULL;
    IDWriteTextLayout *layout = NULL;
    DWRITE_CLUSTER_METRICS m[16];
    struct renderer r = { { &r_vtbl }, 0, 0 };
    UINT32 n = 0;
    HRESULT hr;
    float stop;

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory);
    if (!factory) { printf("FAIL  no factory\nRESULT: FAIL\n"); return 1; }
    IDWriteFactory_CreateTextFormat(factory, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &format);
    for (stop = 40.0f; stop <= 100.0f; stop += 60.0f)
    {
        IDWriteTextFormat_SetIncrementalTabStop(format, stop);
        IDWriteFactory_CreateTextLayout(factory, L"\ta\tb", 4, format, 1000.0f, 1000.0f, &layout);
        n = 0;
        hr = IDWriteTextLayout_GetClusterMetrics(layout, m, 16, &n);
        check(hr == S_OK && n == 4, "stop %g: %u clusters (%#lx)", stop, n, hr);
        check(m[0].width == stop, "stop %g: a tab at the start is a whole stop (%g)", stop, m[0].width);
        check(fabsf(m[1].width + m[2].width - stop) < 0.001f, "stop %g: 'a' and the tab fill the next stop (%g + %g)", stop, m[1].width, m[2].width);
        r.found = 0;
        hr = IDWriteTextLayout_Draw(layout, NULL, &r.iface, 0.0f, 0.0f);
        check(hr == S_OK && r.found && fabsf(r.b_x - 2 * stop) < 0.01f, "stop %g: 'b' is drawn at %g (want %g)", stop, r.b_x, 2 * stop);
        IDWriteTextLayout_Release(layout);
    }

    /* a stop set on the layout later */
    IDWriteFactory_CreateTextLayout(factory, L"\ta\tb", 4, format, 1000.0f, 1000.0f, &layout);
    IDWriteTextLayout_SetIncrementalTabStop(layout, 55.0f);
    hr = IDWriteTextLayout_GetClusterMetrics(layout, m, 16, &n);
    check(hr == S_OK && m[0].width == 55.0f, "a stop set on the layout is used (%g)", m[0].width);
    IDWriteTextLayout_Release(layout);

    /* a new line starts at 0 again */
    IDWriteTextFormat_SetIncrementalTabStop(format, 50.0f);
    IDWriteFactory_CreateTextLayout(factory, L"abc\n\tx", 6, format, 1000.0f, 1000.0f, &layout);
    n = 0;
    IDWriteTextLayout_GetClusterMetrics(layout, m, 16, &n);
    check(n == 6 && m[4].width == 50.0f, "after a new line the tab is a whole stop (%g)", n == 6 ? m[4].width : -1.0f);
    IDWriteTextLayout_Release(layout);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
