/* Probe for patches/sg/5204: RemoveFontMemResourceEx removes the fonts AddFontMemResourceEx
 * added (they stop being enumerated and CreateFont no longer finds the face), refuses a handle
 * that is not one (or one already removed), and removing one memory font leaves another.
 * Uses a TrueType font built in memory (test/sgfont.h). Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include "sgfont.h"

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int CALLBACK count_proc(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD type, LPARAM lp)
{
    (*(int *)lp)++;
    return 1;
}

static int faces(HDC dc, const char *name)
{
    int n = 0;
    EnumFontFamiliesA(dc, name, count_proc, (LPARAM)&n);
    return n;
}

static BOOL selected_face_is(HDC dc, const char *name)
{
    LOGFONTA lf = {0};
    char got[LF_FACESIZE] = "";
    HFONT font, old;

    lf.lfHeight = -20;
    lstrcpyA(lf.lfFaceName, name);
    font = CreateFontIndirectA(&lf);
    old = SelectObject(dc, font);
    GetTextFaceA(dc, sizeof(got), got);
    SelectObject(dc, old);
    DeleteObject(font);
    return !lstrcmpiA(got, name);
}

int main(void)
{
    static const char *names[] = { "SgMemFontOne", "SgMemFontTwo" };
    unsigned char *font[2];
    unsigned len[2];
    HANDLE h[2];
    DWORD count, err;
    HDC dc = GetDC(0);
    unsigned int i;
    BOOL ok;

    for (i = 0; i < 2; i++)
    {
        font[i] = sgf_build(names[i], NULL, 0, &len[i]);
        count = 0;
        h[i] = AddFontMemResourceEx(font[i], len[i], NULL, &count);
        check(h[i] && count == 1, "add %s: %p, %lu fonts", names[i], h[i], count);
        check(faces(dc, names[i]) == 1 && selected_face_is(dc, names[i]), "%s is available", names[i]);
    }
    SetLastError(0xdeadbeef);
    ok = RemoveFontMemResourceEx((HANDLE)0x1234);
    err = GetLastError();
    check(!ok, "a handle that was never returned is refused (%d, %lu)", ok, err);
    SetLastError(0xdeadbeef);
    ok = RemoveFontMemResourceEx(NULL);
    check(!ok, "NULL handle is refused (%d)", ok);

    ok = RemoveFontMemResourceEx(h[0]);
    check(ok, "remove %s: %lu", names[0], GetLastError());
    check(faces(dc, names[0]) == 0, "%s is no longer enumerated", names[0]);
    check(!selected_face_is(dc, names[0]), "%s is no longer found by CreateFont", names[0]);
    check(faces(dc, names[1]) == 1 && selected_face_is(dc, names[1]), "%s is still there", names[1]);
    SetLastError(0xdeadbeef);
    ok = RemoveFontMemResourceEx(h[0]);
    check(!ok, "removing it twice fails (%d)", ok);

    ok = RemoveFontMemResourceEx(h[1]);
    check(ok && faces(dc, names[1]) == 0, "remove %s", names[1]);
    ReleaseDC(0, dc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
