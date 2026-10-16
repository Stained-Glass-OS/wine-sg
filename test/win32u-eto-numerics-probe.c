/* Probe for patches/sg/5218: ExtTextOut's ETO_NUMERICSLATIN draws European digits for the digits
 * of another script, ETO_NUMERICSLOCAL draws the user locale's digits (LOCALE_SNATIVEDIGITS, here the
 * user override in Control Panel\International) for 0-9; other characters and the glyph-index
 * mode are untouched.  Compared by the pixels drawn.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define W 240
#define H 40
static HDC hdc;
static BYTE *bits;

static void draw(const WCHAR *text, UINT flags, BYTE *out)
{
    RECT r = {0, 0, W, H};
    FillRect(hdc, &r, GetStockObject(WHITE_BRUSH));
    ExtTextOutW(hdc, 4, 4, flags, NULL, text, lstrlenW(text), NULL);
    GdiFlush();
    memcpy(out, bits, W * H * 4);
}

static int same(const BYTE *a, const BYTE *b) { return !memcmp(a, b, W * H * 4); }
static int blank(const BYTE *a) { unsigned int i; for (i = 0; i < W * H * 4; i++) if (a[i] != 0xff) return 0; return 1; }

int main(void)
{
    static const WCHAR latin[] = L"1234567890", arabic[] = L"\x661\x662\x663\x664\x665\x666\x667\x668\x669\x660";
    static const WCHAR persian[] = L"\x6f1\x6f2\x6f3\x6f4\x6f5\x6f6\x6f7\x6f8\x6f9\x6f0";
    static const WCHAR mixed_a[] = L"a1b2c3", mixed_b[] = L"a\x661" L"b\x662" L"c\x663";
    static const WCHAR native[] = L"\x660\x661\x662\x663\x664\x665\x666\x667\x668\x669";
    BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB}};
    BYTE *a = malloc(W * H * 4), *b = malloc(W * H * 4), *c = malloc(W * H * 4);
    HBITMAP bmp;
    HKEY key;
    HFONT old;
    BYTE *copy;

    setvbuf(stdout, NULL, _IONBF, 0);
    hdc = CreateCompatibleDC(NULL);
    bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(hdc, bmp);
    old = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    (void)old;
    SetBkMode(hdc, TRANSPARENT);
    copy = c;

    draw(latin, 0, a);
    check(!blank(a), "the text is drawn");

    /* LATIN: other scripts' digits come out as European digits */
    draw(arabic, ETO_NUMERICSLATIN, b);
    check(same(a, b), "ETO_NUMERICSLATIN: Arabic-Indic digits drawn as 0-9");
    draw(persian, ETO_NUMERICSLATIN, b);
    check(same(a, b), "ETO_NUMERICSLATIN: Extended Arabic-Indic digits drawn as 0-9");
    draw(arabic, 0, b);
    check(!same(a, b), "without the flag they are drawn as they are");
    draw(mixed_a, 0, a);
    draw(mixed_b, ETO_NUMERICSLATIN, b);
    check(same(a, b), "only the digits are changed in a mixed string");
    draw(latin, ETO_NUMERICSLATIN, a);
    draw(latin, 0, b);
    check(same(a, b), "ETO_NUMERICSLATIN leaves 0-9 alone");

    /* LOCAL with the en-US locale: the digits are 0-9 */
    draw(latin, ETO_NUMERICSLOCAL, a);
    draw(latin, 0, b);
    check(same(a, b), "ETO_NUMERICSLOCAL with Latin digits as the locale's: unchanged");

    /* LOCAL with the user's native digits set to Arabic-Indic */
    if (!RegCreateKeyExA(HKEY_CURRENT_USER, "Control Panel\\International", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
    {
        RegSetValueExW(key, L"sNativeDigits", 0, REG_SZ, (const BYTE *)native, sizeof(native) + sizeof(WCHAR));
        draw(latin, ETO_NUMERICSLOCAL, a);
        draw(arabic, 0, b);
        draw(latin, 0, c);
        check(same(a, b), "ETO_NUMERICSLOCAL: 0-9 drawn as the native digits");
        check(!same(a, c), "and not as 0-9");
        draw(mixed_a, ETO_NUMERICSLOCAL, a);
        draw(mixed_b, 0, b);
        check(same(a, b), "only the digits are changed in a mixed string");
        draw(latin, ETO_NUMERICSLOCAL | ETO_GLYPH_INDEX, a);
        draw(latin, ETO_GLYPH_INDEX, b);
        check(same(a, b), "glyph indices are not digits");
        draw(arabic, ETO_NUMERICSLATIN, a);
        draw(latin, 0, b);
        check(same(a, b), "ETO_NUMERICSLATIN turns the native digits into 0-9");
        RegDeleteValueW(key, L"sNativeDigits");
        RegCloseKey(key);
    }
    else check(0, "registry key");
    (void)copy;

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
