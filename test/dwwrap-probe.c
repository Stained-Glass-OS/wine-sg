/* dwrite: what the Wine tests record from Windows for a text layout (patches/sg/2635): a word
 * wider than the layout is broken between clusters (one line per character in a layout
 * a pixel wide), unless words are kept whole or the line is trimmed; a combining mark is one
 * cluster with its base character even where the font has to be replaced for it. */
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

static IDWriteTextLayout *make_layout(IDWriteFactory *factory, const WCHAR *str, float width, int wrapping, int trim)
{
    IDWriteTextFormat *format;
    IDWriteTextLayout *layout = NULL;

    IDWriteFactory_CreateTextFormat(factory, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &format);
    IDWriteTextFormat_SetWordWrapping(format, wrapping);
    if (trim)
    {
        DWRITE_TRIMMING t = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        IDWriteInlineObject *sign;

        IDWriteFactory_CreateEllipsisTrimmingSign(factory, format, &sign);
        IDWriteTextFormat_SetTrimming(format, &t, sign);
        IDWriteInlineObject_Release(sign);
    }
    IDWriteFactory_CreateTextLayout(factory, str, lstrlenW(str), format, width, 100.0f, &layout);
    IDWriteTextFormat_Release(format);
    return layout;
}

int main(void)
{
    static const struct { const WCHAR *str; float width; int wrapping, trim; unsigned int lines; const char *name; } wraps[] =
    {
        { L"string", 5.0f, DWRITE_WORD_WRAPPING_WRAP, 0, 6, "wrap: a line per character" },
        { L"string", 5.0f, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK, 0, 6, "emergency break" },
        { L"string", 5.0f, DWRITE_WORD_WRAPPING_WHOLE_WORD, 0, 1, "whole word is kept" },
        { L"string", 5.0f, DWRITE_WORD_WRAPPING_NO_WRAP, 0, 1, "no wrap" },
        { L"string", 5.0f, DWRITE_WORD_WRAPPING_WRAP, 1, 1, "trimmed line is not broken" },
        { L"string", 1000.0f, DWRITE_WORD_WRAPPING_WRAP, 0, 1, "fits" },
        { L"ab cd", 1000.0f, DWRITE_WORD_WRAPPING_WRAP, 0, 1, "fits with space" },
    };
    static const WCHAR marked[] = { 'a', 'e', 0x0300, 'd', 0 };
    IDWriteFactory *factory;
    DWRITE_CLUSTER_METRICS clusters[8];
    DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout *layout;
    unsigned int i, count;

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory);
    if (!factory) { printf("FAIL  no factory\nRESULT: FAIL\n"); return 1; }

    for (i = 0; i < sizeof(wraps) / sizeof(wraps[0]); i++)
    {
        layout = make_layout(factory, wraps[i].str, wraps[i].width, wraps[i].wrapping, wraps[i].trim);
        memset(&tm, 0, sizeof(tm));
        IDWriteTextLayout_GetMetrics(layout, &tm);
        check(tm.lineCount == wraps[i].lines, "%s: %u lines (want %u)", wraps[i].name, tm.lineCount, wraps[i].lines);
        IDWriteTextLayout_Release(layout);
    }

    /* the lines of the broken word hold one character each and nothing is lost */
    layout = make_layout(factory, L"string", 5.0f, DWRITE_WORD_WRAPPING_WRAP, 0);
    {
        DWRITE_LINE_METRICS lines[8];
        unsigned int n = 0, total = 0;

        IDWriteTextLayout_GetLineMetrics(layout, lines, 8, &n);
        for (i = 0; i < n; i++) total += lines[i].length;
        check(n == 6 && total == 6 && lines[0].length == 1 && lines[5].length == 1, "lines of one character (%u lines, %u characters)", n, total);
    }
    IDWriteTextLayout_Release(layout);

    layout = make_layout(factory, marked, 1000.0f, DWRITE_WORD_WRAPPING_WRAP, 0);
    count = 0;
    IDWriteTextLayout_GetClusterMetrics(layout, clusters, 8, &count);
    check(count == 3 && clusters[0].length == 1 && clusters[1].length == 2 && clusters[2].length == 1,
          "mark stays with its base: %u clusters (%u, %u, %u)", count, clusters[0].length, clusters[1].length, clusters[2].length);
    IDWriteTextLayout_Release(layout);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
