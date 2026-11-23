/* eotsubset-probe.c: test/eotsubset-gate.sh (wine-sg 9502). TTEmbedFont / TTEmbedFontEx / TTEmbedFontFromFileA
 * with TTEMBED_SUBSET embed a subset font: the EOT says so, it is smaller, TTLoadEmbeddedFont reports
 * TTLOAD_FONT_SUBSETTED, and the loaded font draws the kept characters pixel for pixel like the original and
 * nothing for the others. A font whose fsType forbids subsetting is embedded whole (TTEMBED_SUBSETCANCEL).
 *   eotsubset-probe.exe FONT.ttf NOSUBSET.ttf OUT-CHARS.eot OUT-EX.eot OUT-GLYPHS.eot
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <t2embapi.h>
#include <stdio.h>
#include <string.h>

#ifndef TTEMBED_RAW
#define TTEMBED_RAW 0
#endif
#ifndef TTEMBED_SUBSET
#define TTEMBED_SUBSET 0x1
#endif
#ifndef TTEMBED_SUBSETCANCEL
#define TTEMBED_SUBSETCANCEL 0x4
#endif
#ifndef TTLOAD_FONT_SUBSETTED
#define TTLOAD_FONT_SUBSETTED 0x1
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

struct mem { BYTE data[1 << 20]; ULONG size, pos; };
static struct mem full, chars, ex, glyphs, nosub;

static ULONG __cdecl write_proc(void *stream, void *buf, ULONG len)
{
    struct mem *m = stream;
    if (m->size + len > sizeof(m->data)) return 0;
    memcpy(m->data + m->size, buf, len);
    m->size += len;
    return len;
}

static ULONG __cdecl read_proc(void *stream, void *buf, ULONG len)
{
    struct mem *m = stream;
    if (len > m->size - m->pos) len = m->size - m->pos;
    memcpy(buf, m->data + m->pos, len);
    m->pos += len;
    return len;
}

static void save(const struct mem *m, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(m->data, 1, m->size, f); fclose(f); }
}

static ULONG eot_flags(const struct mem *m) { return m->size >= 16 ? *(const ULONG *)(m->data + 12) : ~0u; }

static HDC dc_with(const WCHAR *face)
{
    HDC hdc = CreateCompatibleDC(NULL);
    SelectObject(hdc, CreateFontW(-40, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, face));
    return hdc;
}

static DWORD pixels[2][120 * 60];

/* draws TEXT with FACE into pixels[SLOT]; returns the ink pixel count */
static int draw(int slot, const WCHAR *face, const WCHAR *text)
{
    BITMAPINFO bi = {{sizeof(bi.bmiHeader), 120, -60, 1, 32, BI_RGB}};
    HDC hdc = dc_with(face);
    DWORD *bits;
    HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    WCHAR got[64];
    int i, n = 0;

    SelectObject(hdc, bmp);
    PatBlt(hdc, 0, 0, 120, 60, WHITENESS);
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, 10, 5, text, lstrlenW(text));
    GdiFlush();
    GetTextFaceW(hdc, 64, got);
    for (i = 0; i < 120 * 60; i++) if ((bits[i] & 0xffffff) == 0) n++;
    memcpy(pixels[slot], bits, sizeof(pixels[slot]));
    if (lstrcmpW(got, face)) n = -1;
    DeleteObject(SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
    DeleteDC(hdc);
    DeleteObject(bmp);
    return n;
}

int main(int argc, char **argv)
{
    HMODULE t2 = LoadLibraryA("t2embed.dll");
    LONG (WINAPI *pEmbedEx)(HDC,ULONG,ULONG,ULONG*,ULONG*,WRITEEMBEDPROC,LPVOID,ULONG*,USHORT,USHORT,TTEMBEDINFO*) =
        (void *)GetProcAddress(t2, "TTEmbedFontEx");
    LONG (WINAPI *pEmbed)(HDC,ULONG,ULONG,ULONG*,ULONG*,WRITEEMBEDPROC,LPVOID,USHORT*,USHORT,USHORT,TTEMBEDINFO*) =
        (void *)GetProcAddress(t2, "TTEmbedFont");
    LONG (WINAPI *pEmbedFile)(HDC,LPCSTR,USHORT,ULONG,ULONG,ULONG*,ULONG*,WRITEEMBEDPROC,LPVOID,USHORT*,USHORT,USHORT,
                              TTEMBEDINFO*) = (void *)GetProcAddress(t2, "TTEmbedFontFromFileA");
    LONG (WINAPI *pLoad)(HANDLE*,ULONG,ULONG*,ULONG,ULONG*,READEMBEDPROC,LPVOID,LPWSTR,LPSTR,TTLOADINFO*) =
        (void *)GetProcAddress(t2, "TTLoadEmbeddedFont");
    LONG (WINAPI *pDelete)(HANDLE,ULONG,ULONG*) = (void *)GetProcAddress(t2, "TTDeleteEmbeddedFont");
    WCHAR path[MAX_PATH], path2[MAX_PATH];
    USHORT code_c[] = { 'C' }, glyph_b[] = { 3 };
    ULONG code_smiley[] = { 0x1F600 }, priv, status;
    HANDLE ref = NULL;
    HDC hdc, nosub_dc;
    char buf[300];
    LONG ret;
    int n, m;

    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, path, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, path2, MAX_PATH);
    check(AddFontResourceExW(path, FR_PRIVATE, 0) == 1 && AddFontResourceExW(path2, FR_PRIVATE, 0) == 1,
          "the fonts are added");
    hdc = dc_with(L"SG Subset");
    nosub_dc = dc_with(L"SG NoSubset");

    ret = pEmbed(hdc, TTEMBED_RAW, CHARSET_UNICODE, &priv, &status, write_proc, &full, NULL, 0, 0, NULL);
    sprintf(buf, "the whole font embeds (%ld, %lu bytes)", ret, full.size);
    check(ret == E_NONE && full.size > 500, buf);

    status = 0xdead;
    ret = pEmbed(hdc, TTEMBED_SUBSET, CHARSET_UNICODE, &priv, &status, write_proc, &chars, code_c, 1, 0, NULL);
    sprintf(buf, "TTEmbedFont subset of 'C': %ld, %lu bytes (whole %lu), EOT flags %#lx, status %#lx", ret, chars.size,
            full.size, eot_flags(&chars), status);
    check(ret == E_NONE && chars.size < full.size && (eot_flags(&chars) & TTEMBED_SUBSET) && !status, buf);
    save(&chars, argv[3]);

    ret = pEmbedEx ? pEmbedEx(hdc, TTEMBED_SUBSET, CHARSET_UNICODE, &priv, &status, write_proc, &ex, code_smiley, 1, 0,
                              NULL) : -1;
    sprintf(buf, "TTEmbedFontEx subset of U+1F600 (a format 12 cmap): %ld, %lu bytes, EOT flags %#lx", ret, ex.size,
            eot_flags(&ex));
    check(ret == E_NONE && ex.size < full.size && (eot_flags(&ex) & TTEMBED_SUBSET), buf);
    save(&ex, argv[4]);

    ret = pEmbedFile(NULL, argv[1], 0, TTEMBED_SUBSET, CHARSET_GLYPHIDX, &priv, &status, write_proc, &glyphs,
                               glyph_b, 1, 0, NULL);
    sprintf(buf, "TTEmbedFontFromFileA subset of glyph 3 (B): %ld, %lu bytes, EOT flags %#lx", ret, glyphs.size,
            eot_flags(&glyphs));
    check(ret == E_NONE && glyphs.size < full.size && (eot_flags(&glyphs) & TTEMBED_SUBSET), buf);
    save(&glyphs, argv[5]);

    status = 0;
    ret = pEmbed(nosub_dc, TTEMBED_SUBSET, CHARSET_UNICODE, &priv, &status, write_proc, &nosub, code_c, 1, 0, NULL);
    sprintf(buf, "fsType 'no subsetting': whole font, TTEMBED_SUBSETCANCEL (%ld, %lu bytes, flags %#lx, status %#lx)",
            ret, nosub.size, eot_flags(&nosub), status);
    check(ret == E_NONE && nosub.size >= full.size - 64 && !(eot_flags(&nosub) & TTEMBED_SUBSET) &&
          (status & TTEMBED_SUBSETCANCEL), buf);

    ret = pLoad(&ref, 0, &priv, LICENSE_PREVIEWPRINT, &status, read_proc, &chars, (WCHAR *)L"SG Loaded",
                             NULL, NULL);
    sprintf(buf, "the subset loads, TTLOAD_FONT_SUBSETTED (%ld, status %#lx)", ret, status);
    check(ret == E_NONE && (status & TTLOAD_FONT_SUBSETTED), buf);

    n = draw(0, L"SG Subset", L"C");
    m = draw(1, L"SG Loaded", L"C");
    sprintf(buf, "'C' (a composite) from the subset is the original's, pixel for pixel (%d / %d ink)", m, n);
    check(n > 100 && m == n && !memcmp(pixels[0], pixels[1], sizeof(pixels[0])), buf);
    n = draw(0, L"SG Subset", L"B");
    m = draw(1, L"SG Loaded", L"B");
    sprintf(buf, "'B' was not kept: the original draws it (%d ink), the subset draws nothing (%d)", n, m);
    check(n > 100 && m == 0, buf);

    if (ref) pDelete(ref, 0, &status);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
