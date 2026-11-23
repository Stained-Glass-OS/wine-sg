/* GetFontResourceInfoW on a font file a program adds itself: the number of
 * fonts (0), the description (1), LOGFONTW of each (2), TrueType or not (3),
 * the file (4). OpenTTD asks type 2 for the face name to create its own fonts
 * by; with nothing it used the file's name and drew with a substitute.
 * patches/sg/9302. Usage: probe FONT.ttf [COLLECTION.ttc]. Prints RESULT. */
#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef BOOL (WINAPI *info_fn)(const WCHAR *, DWORD *, void *, DWORD);

int main(int argc, char **argv)
{
    info_fn info = (info_fn)GetProcAddress(GetModuleHandleA("gdi32.dll"), "GetFontResourceInfoW");
    WCHAR font[MAX_PATH], copy[MAX_PATH], tmp[MAX_PATH], desc[512], file[MAX_PATH];
    LOGFONTW lf[8];
    DWORD size, value;
    BOOL ret;

    if (argc < 2 || !info) { printf("usage / no export\nRESULT: FAIL\n"); return 1; }
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, font, MAX_PATH);

    /* the program's own copy under another name, added privately (OpenTTD's baseset fonts) */
    GetTempPathW(MAX_PATH, tmp);
    swprintf(copy, MAX_PATH, L"%lsRenamed-Font.ttf", tmp);
    CHECK(CopyFileW(font, copy, FALSE), "font copied to %ls", copy);
    CHECK(AddFontResourceExW(copy, FR_PRIVATE, 0) > 0, "AddFontResourceExW FR_PRIVATE");

    size = 0;
    ret = info(copy, &size, NULL, 2);
    CHECK(ret && size == sizeof(LOGFONTW), "type 2 size query: %d, %lu bytes", ret, size);
    size = sizeof(lf);
    memset(lf, 0, sizeof(lf));
    ret = info(copy, &size, lf, 2);
    CHECK(ret && size == sizeof(LOGFONTW), "type 2: %d, %lu bytes", ret, size);
    CHECK(!wcscmp(lf[0].lfFaceName, L"DejaVu Sans"), "face name from the font, not the file (%ls)", lf[0].lfFaceName);
    CHECK(lf[0].lfWeight == FW_BOLD && !lf[0].lfItalic, "bold, upright (%ld, %d)", lf[0].lfWeight, lf[0].lfItalic);

    size = sizeof(value); value = 0;
    CHECK(info(copy, &size, &value, 0) && value == 1, "type 0: one font (%lu)", value);
    size = sizeof(value); value = 0;
    CHECK(info(copy, &size, &value, 3) && value == TRUE, "type 3: TrueType (%lu)", value);
    size = sizeof(desc); desc[0] = 0;
    CHECK(info(copy, &size, desc, 1) && wcsstr(desc, L"DejaVu Sans"), "type 1: description %ls", desc);
    size = sizeof(file); file[0] = 0;
    CHECK(info(copy, &size, file, 4) && !lstrcmpiW(file, copy), "type 4: the file %ls", file);

    size = 2;
    CHECK(!info(copy, &size, lf, 2) && size == sizeof(LOGFONTW), "too small a buffer: FALSE, size %lu", size);
    size = sizeof(lf);
    CHECK(!info(L"C:\\no-such-font.ttf", &size, lf, 2), "a missing file: FALSE");

    /* a collection: one LOGFONTW per font */
    if (argc > 2)
    {
        WCHAR ttc[MAX_PATH];
        LOGFONTW many[64];
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, ttc, MAX_PATH);
        size = sizeof(value); value = 0;
        ret = info(ttc, &size, &value, 0);
        CHECK(ret && value > 1, "collection: %lu fonts", value);
        size = sizeof(many);
        ret = info(ttc, &size, many, 2);
        CHECK(ret && size == value * sizeof(LOGFONTW) && many[0].lfFaceName[0], "collection: %lu LOGFONTW, first %ls",
              size / (DWORD)sizeof(LOGFONTW), many[0].lfFaceName);
    }

    RemoveFontResourceExW(copy, FR_PRIVATE, 0);
    DeleteFileW(copy);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
