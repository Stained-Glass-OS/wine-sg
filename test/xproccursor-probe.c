/* xproccursor-probe: another process's cursor can be drawn and read (patches/sg/9107).
 *
 * On Windows a cursor is a session-wide object; under Wine its bitmaps lived only in the process
 * that made it, so a screen recorder drawing the cursor GetCursorInfo gives (another program's)
 * got nothing ("Error retrieving icon frame"). The parent makes three cursors -- a 32-bit one with
 * alpha (CreateIconIndirect), a monochrome one (CreateCursor), and an icon -- and starts itself as
 * a child with the handles. The child draws each with DrawIconEx into a bitmap of its own and reads
 * the pixels back, asks GetIconInfo (hotspot and bitmaps) and SetCursor's return; the icon stays
 * the parent's (icons are not published). After the parent destroys a cursor the child's handle
 * no longer draws.
 *
 * Prints name=value lines; see test/xproccursor-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define S 32

static void say(const char *name, int ok, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s=%d %s\n", name, ok ? 1 : 0, buf);
    fflush(stdout);
}

/* a 32-bit cursor: opaque red left half, transparent right half; hotspot 5,7 */
static HCURSOR make_alpha_cursor(void)
{
    BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER), S, -S, 1, 32, BI_RGB}};
    ICONINFO info = {FALSE, 5, 7};
    DWORD *bits;
    HCURSOR cursor;
    int x, y;

    info.hbmColor = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    for (y = 0; y < S; y++)
        for (x = 0; x < S; x++)
            bits[y * S + x] = x < S / 2 ? 0xffff0000 : 0x00000000;
    info.hbmMask = CreateBitmap(S, S, 1, 1, NULL);
    cursor = CreateIconIndirect(&info);
    DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
    return cursor;
}

/* a monochrome cursor: black top half (AND 0, XOR 0), the rest transparent (AND 1, XOR 0) */
static HCURSOR make_mono_cursor(void)
{
    BYTE and_bits[S * S / 8], xor_bits[S * S / 8];
    memset(and_bits, 0xff, sizeof(and_bits));
    memset(and_bits, 0x00, sizeof(and_bits) / 2);
    memset(xor_bits, 0, sizeof(xor_bits));
    return CreateCursor(GetModuleHandleA(NULL), 3, 4, S, S, and_bits, xor_bits);
}

static HICON make_icon(void)
{
    BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER), S, -S, 1, 32, BI_RGB}};
    ICONINFO info = {TRUE, 0, 0};
    DWORD *bits;
    HICON icon;
    int i;

    info.hbmColor = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    for (i = 0; i < S * S; i++) bits[i] = 0xff0000ff;
    info.hbmMask = CreateBitmap(S, S, 1, 1, NULL);
    icon = CreateIconIndirect(&info);
    DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
    return icon;
}

/* DrawIconEx on white; the pixels at (8,16) and (24,16) and (8,24) as 0xRRGGBB, or ~0 on failure */
static BOOL draw(HICON icon, DWORD px[3])
{
    BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER), S, -S, 1, 32, BI_RGB}};
    HDC dc = CreateCompatibleDC(NULL);
    DWORD *bits;
    HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    HGDIOBJ old = SelectObject(dc, bmp);
    BOOL ok;
    int i;

    for (i = 0; i < S * S; i++) bits[i] = 0x00ffffff;
    ok = DrawIconEx(dc, 0, 0, icon, S, S, 0, NULL, DI_NORMAL);
    GdiFlush();
    px[0] = bits[16 * S + 8] & 0xffffff;
    px[1] = bits[16 * S + 24] & 0xffffff;
    px[2] = bits[24 * S + 8] & 0xffffff;
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    return ok;
}

static int child(HCURSOR alpha, HCURSOR mono, HICON icon, HANDLE go, HANDLE done)
{
    ICONINFO info;
    DWORD px[3];
    BOOL ok;

    ok = draw(alpha, px);
    say("alpha_draw", ok && px[0] == 0xff0000 && px[1] == 0xffffff,
            "DrawIconEx of the parent's 32-bit cursor %d: left %06lx (want ff0000), right %06lx (want ffffff)", ok, px[0], px[1]);
    memset(&info, 0, sizeof(info));
    ok = GetIconInfo(alpha, &info);
    say("alpha_info", ok && !info.fIcon && info.xHotspot == 5 && info.yHotspot == 7 && info.hbmMask && info.hbmColor,
            "GetIconInfo %d: fIcon %d hotspot %lu,%lu color %p mask %p", ok, info.fIcon, info.xHotspot, info.yHotspot,
            info.hbmColor, info.hbmMask);
    if (ok)
    {
        BITMAP bm;
        GetObjectA(info.hbmColor, sizeof(bm), &bm);
        say("alpha_bitmap", bm.bmWidth == S && bm.bmHeight == S, "its color bitmap %dx%d", bm.bmWidth, bm.bmHeight);
        DeleteObject(info.hbmColor);
        DeleteObject(info.hbmMask);
    }

    ok = draw(mono, px);
    say("mono_draw", ok && px[0] == 0xffffff && px[2] == 0xffffff,
            "DrawIconEx of the parent's monochrome cursor %d: (8,16) %06lx (8,24) %06lx (want ffffff, the bottom half is transparent)",
            ok, px[0], px[2]);
    {
        DWORD top[3];
        HDC dc = CreateCompatibleDC(NULL);
        BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER), S, -S, 1, 32, BI_RGB}};
        DWORD *bits;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
        HGDIOBJ old = SelectObject(dc, bmp);
        int i;
        for (i = 0; i < S * S; i++) bits[i] = 0x00ffffff;
        DrawIconEx(dc, 0, 0, mono, S, S, 0, NULL, DI_NORMAL);
        GdiFlush();
        top[0] = bits[4 * S + 8] & 0xffffff;
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
        say("mono_black", top[0] == 0, "its top half is black: (8,4) %06lx", top[0]);
    }
    memset(&info, 0, sizeof(info));
    ok = GetIconInfo(mono, &info);
    say("mono_info", ok && info.xHotspot == 3 && info.yHotspot == 4 && !info.hbmColor && info.hbmMask,
            "GetIconInfo %d: hotspot %lu,%lu color %p mask %p", ok, info.xHotspot, info.yHotspot, info.hbmColor, info.hbmMask);
    if (ok)
    {
        BITMAP bm;
        GetObjectA(info.hbmMask, sizeof(bm), &bm);
        say("mono_mask", bm.bmWidth == S && bm.bmHeight == 2 * S, "its mask %dx%d (AND over XOR)", bm.bmWidth, bm.bmHeight);
        DeleteObject(info.hbmMask);
    }

    {
        HCURSOR prev;
        SetCursor(alpha);
        prev = SetCursor(NULL);
        say("set_cursor", prev == alpha, "SetCursor gives the parent's cursor back as the previous one: %p (want %p)", prev, alpha);
    }

    ok = GetIconInfo(icon, &info);
    if (ok) { DeleteObject(info.hbmColor); DeleteObject(info.hbmMask); }
    say("icon_private", !ok, "the parent's icon stays its own: GetIconInfo %d", ok);

    ok = GetIconInfo((HICON)(ULONG_PTR)0x7f7e, &info);
    say("bogus", !ok, "a handle that is nothing: GetIconInfo %d", ok);

    /* the parent destroys the monochrome cursor */
    SetEvent(go);
    WaitForSingleObject(done, 10000);
    ok = draw(mono, px);
    say("destroyed", !ok, "after the parent destroyed it: DrawIconEx %d", ok);
    ok = draw(alpha, px);
    say("still_there", ok && px[0] == 0xff0000, "the other one still draws: %d %06lx", ok, px[0]);
    printf("done=1\n");
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv)
{
    char cmd[MAX_PATH + 128], self[MAX_PATH];
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HCURSOR alpha, mono;
    HICON icon;
    HANDLE go, done;
    DWORD px[3];

    if (argc == 6)
        return child((HCURSOR)(ULONG_PTR)strtoull(argv[1], NULL, 16), (HCURSOR)(ULONG_PTR)strtoull(argv[2], NULL, 16),
                (HICON)(ULONG_PTR)strtoull(argv[3], NULL, 16), (HANDLE)(ULONG_PTR)strtoull(argv[4], NULL, 16),
                (HANDLE)(ULONG_PTR)strtoull(argv[5], NULL, 16));

    alpha = make_alpha_cursor();
    mono = make_mono_cursor();
    icon = make_icon();
    {
        BOOL ok = draw(alpha, px);
        say("own_draw", ok && px[0] == 0xff0000, "the parent draws its own cursor: %06lx", px[0]);
    }
    if (!alpha || !mono || !icon) { say("made", 0, "%p %p %p", alpha, mono, icon); printf("done=1\n"); return 1; }
    go = CreateEventA(&sa, FALSE, FALSE, NULL);
    done = CreateEventA(&sa, FALSE, FALSE, NULL);
    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(cmd, sizeof(cmd), "\"%s\" %p %p %p %p %p", self, alpha, mono, icon, go, done);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        say("child", 0, "CreateProcess %lu", GetLastError());
        return 1;
    }
    WaitForSingleObject(go, 30000);
    DestroyCursor(mono);
    SetEvent(done);
    WaitForSingleObject(pi.hProcess, 30000);
    DestroyCursor(alpha);
    DestroyIcon(icon);
    return 0;
}
