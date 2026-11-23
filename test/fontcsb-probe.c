/* fontcsb-probe.c: test/fontcsb-gate.sh (wine-sg 9500). A TrueType font whose OS/2 ulCodePageRange1 is
 * empty gets its character sets from its Unicode ranges, so it is listed and chosen for its script.
 *   fontcsb-probe.exe FONT.ttf
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static int charsets[64], ncharsets;

static int CALLBACK enum_proc(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lparam)
{
    if (ncharsets < 64) charsets[ncharsets++] = lf->lfCharSet;
    return 1;
}

static int has_charset(int cs)
{
    int i;
    for (i = 0; i < ncharsets; i++) if (charsets[i] == cs) return 1;
    return 0;
}

static HFONT make_font(int charset)
{
    LOGFONTW lf = {0};
    lf.lfHeight = -40;
    lf.lfCharSet = charset;
    lstrcpyW(lf.lfFaceName, L"SG NoCodepage");
    return CreateFontIndirectW(&lf);
}

static DWORD reference[200 * 60];

/* draws text (BYTE text, or WTEXT when given) with the font of CHARSET; returns the ink pixel count
 * and, with SAVE, keeps the pixels as the reference; otherwise -1 when they differ from it */
static int ink(HDC hdc, int charset, const char *text, const WCHAR *wtext, int save, int *got_charset,
               FONTSIGNATURE *fs, WCHAR *face)
{
    BITMAPINFO bi = {{sizeof(bi.bmiHeader), 200, -60, 1, 32, BI_RGB}};
    DWORD *bits;
    HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    HFONT font = make_font(charset), old_font;
    HGDIOBJ old_bmp = SelectObject(hdc, bmp);
    int i, n = 0;

    old_font = SelectObject(hdc, font);
    PatBlt(hdc, 0, 0, 200, 60, WHITENESS);
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);
    if (wtext) TextOutW(hdc, 10, 5, wtext, lstrlenW(wtext));
    else TextOutA(hdc, 10, 5, text, lstrlenA(text));
    GdiFlush();
    for (i = 0; i < 200 * 60; i++) if ((bits[i] & 0xffffff) == 0) n++;
    if (save) memcpy(reference, bits, sizeof(reference));
    else if (memcmp(reference, bits, sizeof(reference))) n = -1;
    *got_charset = GetTextCharsetInfo(hdc, fs, 0);
    GetTextFaceW(hdc, 64, face);
    SelectObject(hdc, old_font);
    SelectObject(hdc, old_bmp);
    DeleteObject(font);
    DeleteObject(bmp);
    return n;
}

int main(int argc, char **argv)
{
    WCHAR path[MAX_PATH], face[64];
    LOGFONTW lf = {0};
    HDC hdc = CreateCompatibleDC(0);
    FONTSIGNATURE fs;
    int cs, n;
    char buf[200];

    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, path, MAX_PATH);
    check(AddFontResourceExW(path, FR_PRIVATE, 0) == 1, "the font is added");

    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpyW(lf.lfFaceName, L"SG NoCodepage");
    EnumFontFamiliesExW(hdc, &lf, enum_proc, 0, 0);
    sprintf(buf, "enumerated with Arabic among %d character sets (first %d)", ncharsets, ncharsets ? charsets[0] : -1);
    check(has_charset(ARABIC_CHARSET), buf);
    check(!has_charset(ANSI_CHARSET), "not enumerated as ANSI (no Latin range)");
    check(!has_charset(OEM_CHARSET), "not enumerated as OEM_CHARSET (Wine's mark for a face with no code page)");

    ncharsets = 0;
    lf.lfCharSet = ARABIC_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, enum_proc, 0, 0);
    check(ncharsets == 1 && charsets[0] == ARABIC_CHARSET, "enumerating ARABIC_CHARSET lists it once");

    /* the reference: alef + beh drawn as Unicode */
    n = ink(hdc, ARABIC_CHARSET, NULL, L"\x0627\x0628", 1, &cs, &fs, face);
    sprintf(buf, "alef + beh drawn as Unicode leave ink (%d pixels)", n);
    check(n > 300, buf);

    /* cp1256: 0xC7 alef, 0xC8 beh */
    n = ink(hdc, ARABIC_CHARSET, "\xc7\xc8", NULL, 0, &cs, &fs, face);
    sprintf(buf, "ARABIC_CHARSET picks it: face %ls, charset %d, fsCsb0 %08lx", face, cs, fs.fsCsb[0]);
    check(!lstrcmpW(face, L"SG NoCodepage") && cs == ARABIC_CHARSET && (fs.fsCsb[0] & FS_ARABIC), buf);
    sprintf(buf, "ANSI bytes C7 C8 drawn through code page 1256 are alef + beh, pixel for pixel (%d)", n);
    check(n > 300, buf);

    n = ink(hdc, DEFAULT_CHARSET, "\xc7\xc8", NULL, 0, &cs, &fs, face);
    sprintf(buf, "DEFAULT_CHARSET gives the font's own charset (%d) and draws alef + beh (%d)", cs, n);
    check(cs == ARABIC_CHARSET && n > 300, buf);

    RemoveFontResourceExW(path, FR_PRIVATE, 0);
    DeleteDC(hdc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
