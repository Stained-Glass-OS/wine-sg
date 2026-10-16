/* Probe for patches/sg/5021: PlayEnhMetaFileRecord plays EMR_SMALLTEXTOUT
 * (MS-EMF 2.3.5.37): text at (x, y) with 16-bit characters, or 8-bit ones
 * with ETO_SMALL_CHARS, which draw the same; the clip rectangle is there
 * unless ETO_NO_RECT, and ETO_OPAQUE fills it. It was ignored.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef EMR_SMALLTEXTOUT
#define EMR_SMALLTEXTOUT 108
#endif
#define W 96
#define H 32
static DWORD *bits;
static HDC target;
static BYTE record[256];

static int CALLBACK play(HDC hdc, HANDLETABLE *table, const ENHMETARECORD *rec, int n, LPARAM param)
{
    if (rec->iType == EMR_HEADER)
        PlayEnhMetaFileRecord(target, table, (const ENHMETARECORD *)record, n);
    return 1;
}

static int ink(int left, int top, int right, int bottom)
{
    int x, y, n = 0;
    for (y = top; y < bottom; y++) for (x = left; x < right; x++) if ((bits[y * W + x] & 0xffffff) != 0xffffff) n++;
    return n;
}

/* build the record: x, y, chars, options, graphics mode, two scales, [rect], text */
static void build(int x, int y, DWORD options, const RECTL *rc, const char *text)
{
    EMR *emr = (EMR *)record;
    DWORD *d = (DWORD *)(emr + 1), n = strlen(text), i;
    BYTE *p;
    FLOAT one = 1.0f;

    d[0] = x; d[1] = y; d[2] = n; d[3] = options; d[4] = GM_COMPATIBLE;
    memcpy(&d[5], &one, 4); memcpy(&d[6], &one, 4);
    p = (BYTE *)&d[7];
    if (rc) { memcpy(p, rc, sizeof(*rc)); p += sizeof(*rc); }
    for (i = 0; i < n; i++)
    {
        if (options & 0x200) *p++ = text[i];
        else { *(WCHAR *)p = text[i]; p += 2; }
    }
    emr->iType = EMR_SMALLTEXTOUT;
    emr->nSize = ((p - record) + 3) & ~3;
}

static void run(void)
{
    HDC mf = CreateEnhMetaFileA(NULL, NULL, NULL, NULL);
    HENHMETAFILE emf = CloseEnhMetaFile(mf);
    memset(bits, 0xff, W * H * 4);
    EnumEnhMetaFile(NULL, emf, play, NULL, NULL);
    DeleteEnhMetaFile(emf);
}

int main(void)
{
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB}};
    HBITMAP bmp;
    DWORD wide[W * H];
    RECTL rc = {60, 4, 80, 20};
    int a, b;

    target = CreateCompatibleDC(NULL);
    bmp = CreateDIBSection(target, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(target, bmp);
    SelectObject(target, GetStockObject(SYSTEM_FONT));
    SetTextColor(target, RGB(0, 0, 0));
    SetBkMode(target, TRANSPARENT);
    SetBkColor(target, RGB(0, 0, 0));

    build(2, 2, 0x100, NULL, "HMH");
    run();
    a = ink(0, 0, 56, H);
    check(a > 20, "16-bit characters, no rectangle: drawn (%d pixels)", a);
    check(!ink(56, 0, W, H), "and nothing past the text");
    memcpy(wide, bits, sizeof(wide));

    build(2, 2, 0x100 | 0x200, NULL, "HMH");
    run();
    b = ink(0, 0, 56, H);
    check(b == a && !memcmp(wide, bits, sizeof(wide)), "8-bit characters draw the same (%d pixels)", b);

    build(2, 2, ETO_OPAQUE, &rc, "HMH");
    run();
    a = ink(rc.left, rc.top, rc.right, rc.bottom);
    check(a == (rc.right - rc.left) * (rc.bottom - rc.top), "the rectangle is read and filled (%d pixels)", a);
    check(ink(0, 0, 56, H) > 20, "and the text after it drawn");

    build(2, 2, 0x200, &rc, "HMH");
    run();
    check(ink(0, 0, 56, H) > 20 && !ink(56, 0, W, H), "8-bit characters after a rectangle drawn");

    DeleteDC(target);
    DeleteObject(bmp);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
