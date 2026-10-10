/* dwrite: IDWriteTextLayout::HitTestTextRange, HitTestTextPosition and
 * HitTestPoint (patches/sg/2617). The rectangles are checked against the
 * cluster metrics and line metrics of the same layout. */
#define COBJMACROS
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#define IID_IDWriteFactory my_iid_factory
static const GUID my_iid_factory = {0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};

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

static int approx(float a, float b) { return fabsf(a - b) < 0.02f; }

static IDWriteFactory *factory;

static IDWriteTextLayout *make_layout(const WCHAR *text, float width, float height, float size, int rtl)
{
    IDWriteTextFormat *format = NULL;
    IDWriteTextLayout *layout = NULL;
    HRESULT hr;

    hr = IDWriteFactory_CreateTextFormat(factory, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &format);
    if (FAILED(hr)) return NULL;
    if (rtl) IDWriteTextFormat_SetReadingDirection(format, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    hr = IDWriteFactory_CreateTextLayout(factory, text, wcslen(text), format, width, height, &layout);
    IDWriteTextFormat_Release(format);
    return SUCCEEDED(hr) ? layout : NULL;
}

/* ---- one line of text -------------------------------------------------- */

static void test_single_line(void)
{
    static const WCHAR text[] = L"Hello, world";
    DWRITE_CLUSTER_METRICS clusters[32];
    DWRITE_LINE_METRICS line;
    DWRITE_HIT_TEST_METRICS m[8], pm;
    IDWriteTextLayout *layout = make_layout(text, 1000.0f, 200.0f, 20.0f, 0);
    UINT32 cluster_count = 0, line_count = 0, count, i;
    float x, y, left, expected_left;
    BOOL trailing, inside;
    HRESULT hr;

    if (!layout) { check(0, "single line layout"); return; }
    IDWriteTextLayout_GetClusterMetrics(layout, clusters, 32, &cluster_count);
    IDWriteTextLayout_GetLineMetrics(layout, &line, 1, &line_count);
    check(cluster_count == 12 && line_count == 1, "%u clusters on %u line", cluster_count, line_count);

    /* the whole text */
    count = 0;
    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 12, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count == 1, "HitTestTextRange(all) = %#lx, %u", hr, count);
    {
        float total = 0.0f;

        for (i = 0; i < cluster_count; i++) total += clusters[i].width;
        check(m[0].textPosition == 0 && m[0].length == 12 && m[0].isText && !m[0].isTrimmed, "covers the text (%u,%u,%d)",
              m[0].textPosition, m[0].length, m[0].isText);
        check(approx(m[0].left, 0.0f) && approx(m[0].width, total), "left 0 and the width of the clusters (%g, %g vs %g)", m[0].left, m[0].width, total);
        check(approx(m[0].top, 0.0f) && approx(m[0].height, line.height), "top 0 and the line height (%g, %g vs %g)", m[0].top, m[0].height, line.height);
        check(m[0].bidiLevel == 0, "bidi level 0");
    }

    /* a part */
    hr = IDWriteTextLayout_HitTestTextRange(layout, 3, 4, 0.0f, 0.0f, m, 8, &count);
    expected_left = clusters[0].width + clusters[1].width + clusters[2].width;
    check(hr == S_OK && count == 1 && m[0].textPosition == 3 && m[0].length == 4, "a part (%u,%u)", m[0].textPosition, m[0].length);
    check(approx(m[0].left, expected_left) && approx(m[0].width, clusters[3].width + clusters[4].width + clusters[5].width + clusters[6].width),
          "at the sum of the clusters before it (%g vs %g)", m[0].left, expected_left);

    /* origin */
    hr = IDWriteTextLayout_HitTestTextRange(layout, 3, 4, 10.0f, 20.0f, m, 8, &count);
    check(approx(m[0].left, expected_left + 10.0f) && approx(m[0].top, 20.0f), "the origin is added (%g, %g)", m[0].left, m[0].top);

    /* trimmed to the text, an empty range, and one past the end */
    hr = IDWriteTextLayout_HitTestTextRange(layout, 8, 100, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count == 1 && m[0].textPosition == 8 && m[0].length == 4, "the length is trimmed (%u,%u)", m[0].textPosition, m[0].length);
    hr = IDWriteTextLayout_HitTestTextRange(layout, 8, 0xffffffffu, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count == 1 && m[0].textPosition == 8 && m[0].length == 4, "an enormous length is cut to the text (%#lx, %u)", hr, count);
    count = 99;
    hr = IDWriteTextLayout_HitTestTextRange(layout, 3, 0, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count == 0, "an empty range has no rectangle (%#lx, %u)", hr, count);
    hr = IDWriteTextLayout_HitTestTextRange(layout, 20, 5, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count == 1 && m[0].textPosition == 12 && m[0].length == 0 && m[0].isText && m[0].width == 0.0f,
          "past the end: an empty rectangle at the end (%#lx, %u, %u,%u)", hr, count, m[0].textPosition, m[0].length);
    check(approx(m[0].left, expected_left + clusters[3].width + clusters[4].width + clusters[5].width + clusters[6].width +
                clusters[7].width + clusters[8].width + clusters[9].width + clusters[10].width + clusters[11].width),
          "which is at the end of the text (%g)", m[0].left);

    /* buffers */
    count = 0;
    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 12, 0.0f, 0.0f, NULL, 0, &count);
    check(hr == E_NOT_SUFFICIENT_BUFFER && count == 1, "no buffer: the count (%#lx, %u)", hr, count);
    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 12, 0.0f, 0.0f, m, 8, NULL);
    check(hr == E_INVALIDARG, "no count pointer = %#lx", hr);

    /* position */
    for (i = 0, left = 0.0f; i < cluster_count; i++)
    {
        memset(&pm, 0, sizeof(pm));
        hr = IDWriteTextLayout_HitTestTextPosition(layout, i, FALSE, &x, &y, &pm);
        if (hr != S_OK || !approx(x, left) || !approx(y, 0.0f) || pm.textPosition != i || pm.length != 1 || !approx(pm.width, clusters[i].width))
            break;
        hr = IDWriteTextLayout_HitTestTextPosition(layout, i, TRUE, &x, &y, &pm);
        if (hr != S_OK || !approx(x, left + clusters[i].width))
            break;
        left += clusters[i].width;
    }
    check(i == cluster_count, "HitTestTextPosition: leading and trailing edge of each of the %u clusters (stopped at %u)", cluster_count, i);
    hr = IDWriteTextLayout_HitTestTextPosition(layout, 50, FALSE, &x, &y, &pm);
    check(hr == S_OK && pm.textPosition == 11 && approx(x, left), "a position past the end is the end of the last cluster (%u, %g vs %g)", pm.textPosition, x, left);
    hr = IDWriteTextLayout_HitTestTextPosition(layout, 2, FALSE, NULL, &y, &pm);
    check(hr == E_INVALIDARG, "no x pointer = %#lx", hr);

    /* point */
    hr = IDWriteTextLayout_HitTestPoint(layout, clusters[0].width + clusters[1].width + clusters[2].width * 0.25f, 3.0f, &trailing, &inside, &pm);
    check(hr == S_OK && pm.textPosition == 2 && inside && !trailing, "HitTestPoint in the first half of cluster 2 (%#lx, %u, %d, %d)", hr, pm.textPosition, inside, trailing);
    hr = IDWriteTextLayout_HitTestPoint(layout, clusters[0].width + clusters[1].width + clusters[2].width * 0.75f, 3.0f, &trailing, &inside, &pm);
    check(hr == S_OK && pm.textPosition == 2 && inside && trailing, "and in the second half (%u, %d, %d)", pm.textPosition, inside, trailing);
    hr = IDWriteTextLayout_HitTestPoint(layout, left + 100.0f, 3.0f, &trailing, &inside, &pm);
    check(hr == S_OK && pm.textPosition == 11 && !inside && trailing, "right of the text: the last cluster, outside (%u, %d, %d)", pm.textPosition, inside, trailing);
    hr = IDWriteTextLayout_HitTestPoint(layout, -50.0f, 3.0f, &trailing, &inside, &pm);
    check(hr == S_OK && pm.textPosition == 0 && !inside && !trailing, "left of the text: the first cluster, outside (%u, %d, %d)", pm.textPosition, inside, trailing);
    hr = IDWriteTextLayout_HitTestPoint(layout, 10.0f, 5000.0f, &trailing, &inside, &pm);
    check(hr == S_OK && !inside, "below the text: outside (%d)", inside);
    hr = IDWriteTextLayout_HitTestPoint(layout, 10.0f, 3.0f, NULL, &inside, &pm);
    check(hr == E_INVALIDARG, "no trailing pointer = %#lx", hr);

    IDWriteTextLayout_Release(layout);
}

/* ---- two sizes in one line, and a line break ------------------------------- */

static void test_runs_and_lines(void)
{
    static const WCHAR text[] = L"alpha beta gamma delta epsilon zeta";
    DWRITE_LINE_METRICS lines[8];
    DWRITE_HIT_TEST_METRICS m[16];
    DWRITE_TEXT_RANGE range = {2, 4};
    IDWriteTextLayout *layout = make_layout(text, 1000.0f, 400.0f, 18.0f, 0);
    UINT32 line_count = 0, count = 0, i, total;
    HRESULT hr;

    if (!layout) { check(0, "layout for runs"); return; }

    /* a different size for a part: two runs, one rectangle */
    IDWriteTextLayout_SetFontSize(layout, 30.0f, range);
    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 12, 0.0f, 0.0f, m, 16, &count);
    check(hr == S_OK && count == 1 && m[0].textPosition == 0 && m[0].length == 12, "runs of two sizes are one rectangle (%#lx, %u)", hr, count);
    IDWriteTextLayout_GetLineMetrics(layout, lines, 8, &line_count);
    check(approx(m[0].height, lines[0].height), "as high as the line (%g vs %g)", m[0].height, lines[0].height);
    IDWriteTextLayout_Release(layout);

    /* wrapped */
    layout = make_layout(text, 110.0f, 400.0f, 18.0f, 0);
    IDWriteTextLayout_GetLineMetrics(layout, lines, 8, &line_count);
    check(line_count >= 3, "the text wraps over %u lines", line_count);
    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 35, 0.0f, 0.0f, m, 16, &count);
    check(hr == S_OK && count == line_count, "one rectangle per line (%#lx, %u of %u)", hr, count, line_count);
    for (i = 0, total = 0; i < count; i++)
    {
        if (m[i].textPosition != total || !m[i].length) break;
        if (i && !approx(m[i].top, m[i - 1].top + m[i - 1].height)) break;
        if (!approx(m[i].height, lines[i].height)) break;
        total += m[i].length;
    }
    check(i == count && total == 35, "they follow each other in the text and in height (%u rectangles, %u characters)", i, total);

    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 35, 0.0f, 0.0f, m, 1, &count);
    check(hr == E_NOT_SUFFICIENT_BUFFER && count == line_count && m[0].textPosition == 0, "a buffer for one: the count and the first (%#lx, %u)", hr, count);

    {
        float x, y;
        DWRITE_HIT_TEST_METRICS pm;
        BOOL trailing, inside;

        IDWriteTextLayout_HitTestTextPosition(layout, 34, FALSE, &x, &y, &pm);
        check(approx(y, lines[0].height * (line_count - 1) + 0.0f) || y > lines[0].height, "a position on the last line is down there (y %g)", y);
        {
            UINT32 first_of_last = 0;

            for (i = 0; i + 1 < line_count; i++) first_of_last += lines[i].length;
            hr = IDWriteTextLayout_HitTestPoint(layout, 1.0f, y + 1.0f, &trailing, &inside, &pm);
            check(hr == S_OK && inside && pm.textPosition == first_of_last, "and a point at its left edge finds the first character of that line (%u vs %u)", pm.textPosition, first_of_last);
        }
    }
    IDWriteTextLayout_Release(layout);
}

/* ---- inline object --------------------------------------------------------- */

static void test_inline_object(void)
{
    DWRITE_HIT_TEST_METRICS m[8];
    DWRITE_TEXT_RANGE range = {3, 3};
    IDWriteTextFormat *format = NULL;
    IDWriteInlineObject *sign = NULL;
    IDWriteTextLayout *layout = make_layout(L"string", 300.0f, 100.0f, 20.0f, 0);
    UINT32 count;
    HRESULT hr;

    if (!layout) { check(0, "layout for the inline object"); return; }
    IDWriteFactory_CreateTextFormat(factory, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &format);
    IDWriteFactory_CreateEllipsisTrimmingSign(factory, format, &sign);
    hr = IDWriteTextLayout_SetInlineObject(layout, sign, range);
    check(hr == S_OK, "SetInlineObject = %#lx", hr);

    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 6, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count == 2, "text and object are two rectangles (%#lx, %u)", hr, count);
    check(m[0].textPosition == 0 && m[0].length == 3 && m[0].isText, "the text first (%u,%u,%d)", m[0].textPosition, m[0].length, m[0].isText);
    check(m[1].textPosition == 3 && m[1].length == 3 && !m[1].isText, "the object after it (%u,%u,%d)", m[1].textPosition, m[1].length, m[1].isText);
    check(approx(m[1].left, m[0].left + m[0].width) && m[1].width > 0.0f, "right behind it (%g after %g)", m[1].left, m[0].left + m[0].width);

    hr = IDWriteTextLayout_HitTestTextRange(layout, 9, 3, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count == 1 && m[0].textPosition == 6 && m[0].length == 0 && !m[0].isText, "past the end after an object: not text (%u,%u,%d)", m[0].textPosition, m[0].length, m[0].isText);

    IDWriteInlineObject_Release(sign);
    IDWriteTextFormat_Release(format);
    IDWriteTextLayout_Release(layout);
}

/* ---- right to left ------------------------------------------------------------ */

static void test_rtl(void)
{
    static const WCHAR text[] = L"\x0627\x0644\x0633\x0644\x0627\x0645 \x0639\x0644\x064a\x0643\x0645";
    DWRITE_CLUSTER_METRICS clusters[32];
    DWRITE_HIT_TEST_METRICS m[8], pm;
    IDWriteTextLayout *layout = make_layout(text, 400.0f, 100.0f, 20.0f, 1);
    UINT32 cluster_count = 0, count;
    float total = 0.0f, x, y;
    BOOL trailing, inside;
    UINT32 i;
    HRESULT hr;

    if (!layout) { check(0, "rtl layout"); return; }
    IDWriteTextLayout_GetClusterMetrics(layout, clusters, 32, &cluster_count);
    for (i = 0; i < cluster_count; i++) total += clusters[i].width;

    hr = IDWriteTextLayout_HitTestTextRange(layout, 0, 12, 0.0f, 0.0f, m, 8, &count);
    check(hr == S_OK && count >= 1, "HitTestTextRange (%#lx, %u)", hr, count);
    check(m[0].bidiLevel & 1, "right to left level (%u)", m[0].bidiLevel);
    check(approx(m[0].left + m[0].width, 400.0f), "the text ends at the right edge of the layout (%g + %g)", m[0].left, m[0].width);
    check(approx(m[0].width, total) || count > 1, "and is as wide as the clusters (%g vs %g)", m[0].width, total);

    /* the first character is at the right end */
    hr = IDWriteTextLayout_HitTestTextPosition(layout, 0, FALSE, &x, &y, &pm);
    check(hr == S_OK && approx(x, 400.0f), "position 0, leading edge, is the right edge (%g)", x);
    hr = IDWriteTextLayout_HitTestTextPosition(layout, 0, TRUE, &x, &y, &pm);
    check(hr == S_OK && approx(x, 400.0f - clusters[0].width), "and its trailing edge is to the left of it (%g)", x);
    hr = IDWriteTextLayout_HitTestPoint(layout, 399.0f, 3.0f, &trailing, &inside, &pm);
    check(hr == S_OK && pm.textPosition == 0 && inside && !trailing, "a point at the right edge hits position 0, leading (%u, %d, %d)", pm.textPosition, inside, trailing);

    IDWriteTextLayout_Release(layout);
}

int main(void)
{
    HRESULT hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory);
    if (FAILED(hr)) { printf("FAIL  no factory %#lx\nRESULT: FAIL\n", hr); return 1; }

    test_single_line();
    test_runs_and_lines();
    test_inline_object();
    test_rtl();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
