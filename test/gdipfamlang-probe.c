/* gdipfamlang-probe.c: test/gdipfamlang-gate.sh (wine-sg 9508). GdipGetFamilyName returns the family name
 * in the language asked for when the font names its family in it ("No support for handling of multiple
 * languages!": always the user-language name), else the family's own name.
 *   gdipfamlang-probe.exe FONT.ttf
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;

static GpStatus (WINAPI *pGdiplusStartup)(ULONG_PTR *, const StartupInput *, void *);
static GpStatus (WINAPI *pGdipNewPrivateFontCollection)(void **);
static GpStatus (WINAPI *pGdipPrivateAddFontFile)(void *, const WCHAR *);
static GpStatus (WINAPI *pGdipGetFontCollectionFamilyList)(void *, INT, void **, INT *);
static GpStatus (WINAPI *pGdipGetFamilyName)(void *, WCHAR *, LANGID);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

#define LOAD(name) (p##name = (void *)GetProcAddress(gdip, #name))

int main(int argc, char **argv)
{
    HMODULE gdip = LoadLibraryA("gdiplus.dll");
    StartupInput si = { 1 };
    ULONG_PTR token;
    WCHAR path[MAX_PATH], name[LF_FACESIZE];
    void *collection = NULL, *family = NULL;
    INT found = 0;
    char buf[300];
    static const struct { LANGID lang; const WCHAR *want; const char *what; } cases[] =
    {
        { MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), L"SG Lang Test", "LANG_NEUTRAL: the family's own name" },
        { 0x0409, L"SG Lang Test", "English (0x409)" },
        { 0x0407, L"SG Sprachtest", "German (0x407): the German name" },
        { 0x0411, L"SG \x8a00\x8a9e\x30c6\x30b9\x30c8", "Japanese (0x411): the Japanese name" },
        { 0x040c, L"SG Lang Test", "French (0x40c), not in the font: the family's own name" },
    };
    unsigned int i;

    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, path, MAX_PATH);
    for (i = 0; path[i]; i++) if (path[i] == '/') path[i] = '\\';
    LOAD(GdiplusStartup); LOAD(GdipNewPrivateFontCollection); LOAD(GdipPrivateAddFontFile);
    LOAD(GdipGetFontCollectionFamilyList); LOAD(GdipGetFamilyName);
    pGdiplusStartup(&token, &si, NULL);
    pGdipNewPrivateFontCollection(&collection);
    check(!pGdipPrivateAddFontFile(collection, path), "the font is added to a private collection");
    pGdipGetFontCollectionFamilyList(collection, 1, &family, &found);
    check(found == 1 && family, "it has one family");
    if (family)
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            memset(name, 0, sizeof(name));
            pGdipGetFamilyName(family, name, cases[i].lang);
            sprintf(buf, "%s (got %ls)", cases[i].what, name);
            check(!lstrcmpW(name, cases[i].want), buf);
        }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
