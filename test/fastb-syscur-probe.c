/* SetSystemCursor and SPI_SETCURSORS (patch 3755) */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define ID_HAND 32649
#define ID_IBEAM 32513
#define ID_ARROW 32512
#define ID_WAIT 32514

typedef struct { int ok, hx, hy, w, h; BOOL color; DWORD px; BYTE and0[8]; } INFO;

static INFO info(HCURSOR c)
{
    INFO r;
    ICONINFO ii;
    BITMAP bm;
    memset(&r, 0, sizeof(r));
    if (!GetIconInfo(c, &ii)) return r;
    r.ok = 1; r.hx = ii.xHotspot; r.hy = ii.yHotspot;
    GetObjectW(ii.hbmMask, sizeof(bm), &bm);
    r.w = bm.bmWidth; r.h = ii.hbmColor ? bm.bmHeight : bm.bmHeight / 2;
    r.color = ii.hbmColor != NULL;
    {
        BYTE rows[8 * 64];
        char buf[sizeof(BITMAPINFOHEADER) + 8] = {0};
        BITMAPINFO *bi = (BITMAPINFO *)buf;
        HDC dc = CreateCompatibleDC(0);
        bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi->bmiHeader.biWidth = r.w; bi->bmiHeader.biHeight = -r.h;
        bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = 1; bi->bmiHeader.biCompression = BI_RGB;
        memset(rows, 0xcc, sizeof(rows));
        GetDIBits(dc, ii.hbmMask, 0, r.h, rows, bi, DIB_RGB_COLORS);
        memcpy(r.and0, rows, 4); memcpy(r.and0 + 4, rows + ((r.w + 31) / 32) * 4 * 1, 4);
        if (ii.hbmColor)
        {
            DWORD px[64 * 64];
            memset(px, 0, sizeof(px));
            bi->bmiHeader.biBitCount = 32;
            if (r.w <= 64 && r.h <= 64) GetDIBits(dc, ii.hbmColor, 0, r.h, px, bi, DIB_RGB_COLORS);
            r.px = px[0];
        }
        DeleteDC(dc);
    }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
    return r;
}

static HCURSOR mono_cursor(int hx, int hy, BYTE pattern)
{
    BYTE a[128], x[128];
    memset(a, pattern, sizeof(a)); memset(x, 0x00, sizeof(x));
    return CreateCursor(GetModuleHandleA(NULL), hx, hy, 32, 32, a, x);
}

static HCURSOR color_cursor(int hx, int hy, DWORD argb)
{
    ICONINFO ii;
    BITMAPV5HEADER h;
    void *bits = NULL;
    HBITMAP color, mask;
    HDC dc = GetDC(NULL);
    HCURSOR c;
    DWORD *p;
    int i;
    memset(&h, 0, sizeof(h));
    h.bV5Size = sizeof(h); h.bV5Width = 16; h.bV5Height = -16; h.bV5Planes = 1; h.bV5BitCount = 32;
    h.bV5Compression = BI_BITFIELDS; h.bV5RedMask = 0xff0000; h.bV5GreenMask = 0xff00; h.bV5BlueMask = 0xff; h.bV5AlphaMask = 0xff000000;
    color = CreateDIBSection(dc, (BITMAPINFO *)&h, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, dc);
    p = bits;
    for (i = 0; i < 256; i++) p[i] = argb;
    mask = CreateBitmap(16, 16, 1, 1, NULL);
    memset(&ii, 0, sizeof(ii));
    ii.fIcon = FALSE; ii.xHotspot = hx; ii.yHotspot = hy; ii.hbmMask = mask; ii.hbmColor = color;
    c = CreateIconIndirect(&ii);
    DeleteObject(color); DeleteObject(mask);
    return c;
}

static int child(const char *what)
{
    HCURSOR h = LoadCursorW(NULL, MAKEINTRESOURCEW(ID_HAND));
    INFO i = info(h);
    printf("CHILD %s hot %d,%d size %dx%d\n", what, i.hx, i.hy, i.w, i.h);
    return 0;
}

static void run_child(const char *what, char *out, size_t n)
{
    char cmd[MAX_PATH + 32], path[MAX_PATH];
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE r, w;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD got = 0;
    out[0] = 0;
    CreatePipe(&r, &w, &sa, 0);
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    GetModuleFileNameA(NULL, path, sizeof(path));
    sprintf(cmd, "\"%s\" child %s", path, what);
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = w; si.hStdError = w; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    CloseHandle(w);
    ReadFile(r, out, n - 1, &got, NULL);
    out[got] = 0;
    CloseHandle(r);
}

int main(int argc, char **argv)
{
    HCURSOR h0, hand, ibeam, arrow, wait_, c1, c2, c3, again;
    INFO orig_hand, orig_ibeam, orig_arrow, i;
    char out[256];
    BOOL ok;

    if (argc >= 3 && !strcmp(argv[1], "child")) return child(argv[2]);

    h0 = LoadCursorW(NULL, MAKEINTRESOURCEW(ID_HAND));
    hand = h0;
    ibeam = LoadCursorW(NULL, MAKEINTRESOURCEW(ID_IBEAM));
    arrow = LoadCursorW(NULL, MAKEINTRESOURCEW(ID_ARROW));
    wait_ = LoadCursorW(NULL, MAKEINTRESOURCEW(ID_WAIT));
    orig_hand = info(hand); orig_ibeam = info(ibeam); orig_arrow = info(arrow);
    CHECK(h0 && ibeam && arrow && orig_hand.ok, "system cursors load");
    CHECK(!(orig_hand.hx == 5 && orig_hand.hy == 7), "the hand's hotspot is not the test's (%d,%d)", orig_hand.hx, orig_hand.hy);

    /* a monochrome cursor replaces the hand */
    c1 = mono_cursor(5, 7, 0xa5);
    CHECK(c1 != NULL, "CreateCursor");
    ok = SetSystemCursor(c1, ID_HAND);
    CHECK(ok, "SetSystemCursor: %lu", GetLastError());
    CHECK(!info(c1).ok, "the given cursor is destroyed");
    again = LoadCursorW(NULL, MAKEINTRESOURCEW(ID_HAND));
    CHECK(again == h0, "the same handle: %p / %p", again, h0);
    i = info(h0);
    CHECK(i.ok && i.hx == 5 && i.hy == 7, "new hotspot (%d,%d)", i.hx, i.hy);
    CHECK(i.ok && i.w == 32 && i.h == 32 && !i.color, "new shape %dx%d color %d", i.w, i.h, i.color);
    CHECK(i.ok && i.and0[0] == 0xa5 && i.and0[4] == 0xa5, "new AND mask %02x %02x", i.and0[0], i.and0[4]);
    i = info(ibeam);
    CHECK(i.hx == orig_ibeam.hx && i.hy == orig_ibeam.hy && !memcmp(&i.and0, &orig_ibeam.and0, 8), "the I-beam is untouched");
    i = info(arrow);
    CHECK(i.hx == orig_arrow.hx && !memcmp(&i.and0, &orig_arrow.and0, 8), "the arrow is untouched");

    /* a non-shared load gets the replacement too */
    c3 = LoadImageW(NULL, MAKEINTRESOURCEW(ID_HAND), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);
    i = info(c3);
    CHECK(c3 && c3 != h0 && i.hx == 5 && i.hy == 7, "LoadImage: (%d,%d)", i.hx, i.hy);
    if (c3) DestroyCursor(c3);

    /* a process that starts later loads the replacement */
    run_child("later", out, sizeof(out));
    CHECK(strstr(out, "hot 5,7"), "a new process: %s", out);

    /* a colour cursor with alpha replaces the I-beam */
    c2 = color_cursor(3, 4, 0x80ff8040);
    ok = SetSystemCursor(c2, ID_IBEAM);
    CHECK(ok, "SetSystemCursor colour: %lu", GetLastError());
    i = info(ibeam);
    CHECK(i.ok && i.color && i.w == 16 && i.h == 16 && i.hx == 3 && i.hy == 4, "colour cursor %dx%d (%d,%d) colour %d", i.w, i.h, i.hx, i.hy, i.color);
    CHECK((i.px >> 24) == 0x80 || (i.px & 0xffffff) == 0x40ff80 || i.px == 0x80ff8040, "pixel %08lx", i.px);

    /* the cursor can be the system one itself */
    ok = SetSystemCursor(CopyCursor(hand), ID_WAIT);
    CHECK(ok, "SetSystemCursor with a copy: %lu", GetLastError());
    i = info(wait_);
    CHECK(i.hx == 5 && i.hy == 7, "the wait cursor is the hand: (%d,%d)", i.hx, i.hy);

    /* errors */
    SetLastError(0xdeadbeef);
    c1 = mono_cursor(1, 1, 0xff);
    ok = SetSystemCursor(c1, 0x1234);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "bad id: %d %lu", ok, GetLastError());
    CHECK(info(c1).ok, "a refused cursor is not destroyed");
    SetLastError(0xdeadbeef);
    ok = SetSystemCursor(NULL, ID_HAND);
    CHECK(!ok && GetLastError() == ERROR_INVALID_CURSOR_HANDLE, "NULL cursor: %d %lu", ok, GetLastError());
    {
        BYTE a[128], x[128];
        HICON icon;
        memset(a, 0xff, sizeof(a)); memset(x, 0, sizeof(x));
        icon = CreateIcon(GetModuleHandleA(NULL), 32, 32, 1, 1, a, x);
        SetLastError(0xdeadbeef);
        ok = SetSystemCursor(icon, ID_HAND);
        CHECK(!ok && GetLastError() == ERROR_INVALID_CURSOR_HANDLE, "an icon: %d %lu", ok, GetLastError());
        DestroyIcon(icon);
    }
    DestroyCursor(c1);

    /* SPI_SETCURSORS puts the defaults back */
    ok = SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, 0);
    CHECK(ok, "SPI_SETCURSORS: %lu", GetLastError());
    i = info(h0);
    CHECK(i.ok && i.hx == orig_hand.hx && i.hy == orig_hand.hy && !memcmp(&i.and0, &orig_hand.and0, 8), "the hand is back: (%d,%d)", i.hx, i.hy);
    i = info(ibeam);
    CHECK(i.hx == orig_ibeam.hx && i.hy == orig_ibeam.hy && i.color == orig_ibeam.color && i.w == orig_ibeam.w, "the I-beam is back: (%d,%d) %dx%d", i.hx, i.hy, i.w, i.h);
    i = info(wait_);
    CHECK(i.hx != 5 || i.hy != 7, "the wait cursor is back");
    run_child("restored", out, sizeof(out));
    CHECK(!strstr(out, "hot 5,7"), "a new process after the restore: %s", out);
    c3 = LoadImageW(NULL, MAKEINTRESOURCEW(ID_HAND), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);
    i = info(c3);
    CHECK(c3 && i.hx == orig_hand.hx, "LoadImage after the restore: (%d,%d)", i.hx, i.hy);
    if (c3) DestroyCursor(c3);

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
