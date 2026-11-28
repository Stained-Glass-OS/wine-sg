/* twainscan-probe: scanning from a Windows program through TWAIN (sane.ds)
 * from SANE's "test" device, as IrfanView's Acquire does.
 *
 *   twainscan-probe.exe [ui]     list the sources, scan one page without the
 *                                source's own window (or with it: "ui")
 * Prints "PASS ..." / "FAIL ..." lines and "RESULT: PASS" when all held. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "twainscan-twain.h"

static int failures;
#define CHECK(c, ...) do { int ok_ = !!(c); printf(ok_ ? "PASS  " : "FAIL  "); printf(__VA_ARGS__); printf("\n"); \
                           failures += !ok_; fflush(stdout); } while (0)

static DSMENTRYPROC dsm;
static TW_IDENTITY app;

static TW_UINT16 set_one(TW_IDENTITY *src, TW_UINT16 cap, TW_UINT16 type, TW_UINT32 value)
{
    TW_CAPABILITY c;
    pTW_ONEVALUE one;
    TW_UINT16 rc;

    c.Cap = cap;
    c.ConType = TWON_ONEVALUE;
    c.hContainer = GlobalAlloc(GHND, sizeof(TW_ONEVALUE));
    one = GlobalLock(c.hContainer);
    one->ItemType = type;
    one->Item = value;
    GlobalUnlock(c.hContainer);
    rc = dsm(&app, src, DG_CONTROL, DAT_CAPABILITY, MSG_SET, &c);
    GlobalFree(c.hContainer);
    return rc;
}

/* the source's events until a page is ready (1) or it asks to be closed (0) */
static int wait_ready(TW_IDENTITY *src)
{
    DWORD start = GetTickCount();
    MSG msg;

    while (GetTickCount() - start < 60000)
    {
        TW_EVENT ev;

        if (!PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { Sleep(20); continue; }
        ev.pEvent = &msg;
        ev.TWMessage = MSG_NULL;
        if (dsm(&app, src, DG_CONTROL, DAT_EVENT, MSG_PROCESSEVENT, &ev) == TWRC_DSEVENT)
        {
            if (ev.TWMessage == MSG_XFERREADY) return 1;
            if (ev.TWMessage == MSG_CLOSEDSREQ) return 0;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

int main(int argc, char **argv)
{
    HMODULE twain = LoadLibraryA("twain_32.dll");
    TW_IDENTITY src, test;
    TW_USERINTERFACE ui;
    TW_IMAGEINFO info;
    TW_PENDINGXFERS pend;
    /* TW_HANDLE: pointer-sized (TWAIN 2), as 64-bit programs pass it; what was there before
     * (a program's stack) must all be overwritten */
    HANDLE handle = (HANDLE)(ULONG_PTR)0xa5a5a5a5a5a5a5a5ull;
    TW_UINT16 rc;
    HWND hwnd;
    int found = 0, nsrc = 0, ready = 0;
    BOOL show = argc > 1 && !strcmp(argv[1], "ui");

    dsm = twain ? (DSMENTRYPROC)GetProcAddress(twain, "DSM_Entry") : NULL;
    CHECK(dsm != NULL, "twain_32.dll's DSM_Entry");
    if (!dsm) goto done;
    hwnd = CreateWindowA("STATIC", "scan", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    memset(&app, 0, sizeof(app));
    app.Version.MajorNum = 1;
    app.ProtocolMajor = TWON_PROTOCOLMAJOR;
    app.ProtocolMinor = TWON_PROTOCOLMINOR;
    app.SupportedGroups = DG_IMAGE | DG_CONTROL;
    strcpy(app.Manufacturer, "Stained Glass");
    strcpy(app.ProductFamily, "Gates");
    strcpy(app.ProductName, "twainscan-probe");
    rc = dsm(&app, NULL, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, &hwnd);
    CHECK(rc == TWRC_SUCCESS, "the source manager opens (%u)", rc);
    if (rc) goto done;

    memset(&src, 0, sizeof(src));
    for (rc = dsm(&app, NULL, DG_CONTROL, DAT_IDENTITY, MSG_GETFIRST, &src); rc == TWRC_SUCCESS;
         rc = dsm(&app, NULL, DG_CONTROL, DAT_IDENTITY, MSG_GETNEXT, &src))
    {
        printf("      source: %s | %s | %s\n", src.ProductName, src.Manufacturer, src.ProductFamily);
        nsrc++;
        if (!found && strstr(src.ProductName, "test")) { test = src; found = 1; }
    }
    CHECK(found, "SANE's test device is a TWAIN source (%d sources)", nsrc);
    if (!found) goto close_dsm;

    rc = dsm(&app, NULL, DG_CONTROL, DAT_IDENTITY, MSG_OPENDS, &test);
    CHECK(rc == TWRC_SUCCESS, "it opens (%u)", rc);
    if (rc) goto close_dsm;
    /* the glass, as a program scanning one page chooses it (the source starts on its feeder) */
    rc = set_one(&test, CAP_FEEDERENABLED, TWTY_BOOL, FALSE);
    CHECK(rc == TWRC_SUCCESS || rc == TWRC_CHECKSTATUS, "the flatbed is chosen (%u)", rc);
    rc = set_one(&test, ICAP_PIXELTYPE, TWTY_UINT16, TWPT_RGB);
    CHECK(rc == TWRC_SUCCESS || rc == TWRC_CHECKSTATUS, "colour is chosen (%u)", rc);
    rc = set_one(&test, ICAP_XRESOLUTION, TWTY_FIX32, 75);
    rc = set_one(&test, ICAP_YRESOLUTION, TWTY_FIX32, 75);

    memset(&ui, 0, sizeof(ui));
    ui.ShowUI = show;
    ui.hParent = hwnd;
    rc = dsm(&app, &test, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &ui);
    CHECK(rc == TWRC_SUCCESS || rc == TWRC_CHECKSTATUS, "the scan starts (%u)", rc);
    ready = wait_ready(&test);
    CHECK(ready, "the source says a page is ready");
    if (ready)
    {
        rc = dsm(&app, &test, DG_IMAGE, DAT_IMAGEINFO, MSG_GET, &info);
        /* 80 x 100 mm (the gate's test.conf) at the 75 dpi asked for */
        CHECK(rc == TWRC_SUCCESS && info.ImageWidth == 236 && info.ImageLength == 295,
              "the page's size is the 75 dpi asked for (%ldx%ld, %d bits)", (long)info.ImageWidth,
              (long)info.ImageLength, info.BitsPerPixel);
        CHECK(!info.Planar, "its colour is interleaved per pixel, not planar (%u)", info.Planar);
        rc = dsm(&app, &test, DG_IMAGE, DAT_IMAGENATIVEXFER, MSG_GET, &handle);
        CHECK(rc == TWRC_XFERDONE && handle, "the page arrives as a picture (%u)", rc);
        if (rc == TWRC_XFERDONE)
        {
            BITMAPINFOHEADER *bih = GlobalLock(handle);
            CHECK(bih && bih->biWidth == info.ImageWidth && abs(bih->biHeight) == info.ImageLength,
                  "a DIB of the page (%ldx%ld)", bih ? (long)bih->biWidth : 0, bih ? (long)bih->biHeight : 0);
            GlobalUnlock(handle);
            GlobalFree(handle);
        }
        memset(&pend, 0, sizeof(pend));
        rc = dsm(&app, &test, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, &pend);
        CHECK(rc == TWRC_SUCCESS && pend.Count == 0, "a flatbed has no more pages (%u, count %d)", rc, (short)pend.Count);
    }
    dsm(&app, &test, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, &ui);

    /* again, in strips (a memory transfer, as NAPS2 scans): each strip's rows are what it holds, below
     * the ones before; together the page */
    {
        /* the first scan's close request, handled */
        DWORD t = GetTickCount();
        MSG m;
        while (GetTickCount() - t < 500)
        {
            TW_EVENT ev;
            if (!PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { Sleep(20); continue; }
            ev.pEvent = &m;
            ev.TWMessage = MSG_NULL;
            if (dsm(&app, &test, DG_CONTROL, DAT_EVENT, MSG_PROCESSEVENT, &ev) == TWRC_DSEVENT) continue;
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }
    rc = set_one(&test, ICAP_XFERMECH, TWTY_UINT16, TWSX_MEMORY);
    rc = dsm(&app, &test, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &ui);
    if (wait_ready(&test))
    {
        TW_IMAGEMEMXFER mx;
        DWORD bpr, rows = 0, strips = 0, bad = 0, size;
        BYTE *buf;

        dsm(&app, &test, DG_IMAGE, DAT_IMAGEINFO, MSG_GET, &info);
        bpr = info.ImageWidth * 3;
        size = bpr * 10 + bpr / 2;      /* ten and a half rows: the last strip is short */
        buf = malloc(size);
        do
        {
            memset(&mx, 0, sizeof(mx));
            mx.Memory.Flags = TWMF_APPOWNS | TWMF_POINTER;
            mx.Memory.Length = size;
            mx.Memory.TheMem = buf;
            rc = dsm(&app, &test, DG_IMAGE, DAT_IMAGEMEMXFER, MSG_GET, &mx);
            if (rc != TWRC_SUCCESS && rc != TWRC_XFERDONE) break;
            strips++;
            if (mx.YOffset != rows || mx.BytesWritten != mx.Rows * mx.BytesPerRow) bad++;
            rows += mx.Rows;
        } while (rc == TWRC_SUCCESS && strips < 1000);
        free(buf);
        CHECK(rc == TWRC_XFERDONE, "the strips end with the page (%u, %lu strips)", rc, strips);
        CHECK(!bad, "each strip's rows are the bytes it holds, placed below the strips before (%lu bad)", bad);
        CHECK(rows == (DWORD)info.ImageLength, "the strips make the page (%lu of %ld rows)", rows, (long)info.ImageLength);
        memset(&pend, 0, sizeof(pend));
        dsm(&app, &test, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, &pend);
        CHECK(pend.Count == 0, "one page (count %d)", (short)pend.Count);
    }
    else CHECK(0, "the memory transfer's page is ready");
    dsm(&app, &test, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, &ui);
    dsm(&app, NULL, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, &test);
close_dsm:
    dsm(&app, NULL, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, &hwnd);
done:
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
