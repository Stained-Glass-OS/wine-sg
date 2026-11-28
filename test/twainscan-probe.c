/* twainscan-probe: scanning from a Windows program through TWAIN (sane.ds)
 * from SANE's "test" device, as IrfanView's Acquire does.
 *
 *   twainscan-probe.exe [ui]     list the sources, scan one page without the
 *                                source's own window (or with it: "ui")
 * Prints "PASS ..." / "FAIL ..." lines and "RESULT: PASS" when all held. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
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
    MSG msg;
    int found = 0, nsrc = 0, ready = 0;
    DWORD start;
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
    rc = set_one(&test, ICAP_PIXELTYPE, TWTY_UINT16, TWPT_RGB);
    CHECK(rc == TWRC_SUCCESS || rc == TWRC_CHECKSTATUS, "colour is chosen (%u)", rc);
    rc = set_one(&test, ICAP_XRESOLUTION, TWTY_FIX32, 75);
    rc = set_one(&test, ICAP_YRESOLUTION, TWTY_FIX32, 75);

    memset(&ui, 0, sizeof(ui));
    ui.ShowUI = show;
    ui.hParent = hwnd;
    rc = dsm(&app, &test, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &ui);
    CHECK(rc == TWRC_SUCCESS || rc == TWRC_CHECKSTATUS, "the scan starts (%u)", rc);
    start = GetTickCount();
    while (!ready && GetTickCount() - start < 60000)
    {
        if (!PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { Sleep(20); continue; }
        {
            TW_EVENT ev = { &msg, MSG_NULL };
            if (dsm(&app, &test, DG_CONTROL, DAT_EVENT, MSG_PROCESSEVENT, &ev) == TWRC_DSEVENT)
            {
                if (ev.TWMessage == MSG_XFERREADY) ready = 1;
                else if (ev.TWMessage == MSG_CLOSEDSREQ) break;
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    CHECK(ready, "the source says a page is ready");
    if (ready)
    {
        rc = dsm(&app, &test, DG_IMAGE, DAT_IMAGEINFO, MSG_GET, &info);
        CHECK(rc == TWRC_SUCCESS && info.ImageWidth > 0 && info.ImageLength > 0,
              "the page's size (%ldx%ld, %d bits)", (long)info.ImageWidth, (long)info.ImageLength, info.BitsPerPixel);
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
        dsm(&app, &test, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, &pend);
    }
    dsm(&app, &test, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, &ui);
    dsm(&app, NULL, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, &test);
close_dsm:
    dsm(&app, NULL, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, &hwnd);
done:
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
