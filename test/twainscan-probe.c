/* twainscan-probe: scanning from a Windows program through TWAIN (sane.ds)
 * from SANE's "test" device, as IrfanView's Acquire does.
 *
 *   twainscan-probe.exe [ui] [dsm]   list the sources, scan one page without the
 *                                source's own window (or with it, its Scan button
 *                                pressed: "ui"), through twain_32.dll (or as a TWAIN 2
 *                                program through twaindsm.dll: "dsm", events by callback)
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
static int callbacks, callback_ready, callback_close;

/* TWAIN 2: the source manager calls this with the source's events */
static TW_UINT16 WINAPI callback(pTW_IDENTITY origin, pTW_IDENTITY dest, TW_UINT32 dg, TW_UINT16 dat,
                                 TW_UINT16 msg, TW_MEMREF data)
{
    callbacks++;
    if (dg == DG_CONTROL && dat == DAT_NULL && msg == MSG_XFERREADY) callback_ready = 1;
    if (dg == DG_CONTROL && dat == DAT_NULL && msg == MSG_CLOSEDSREQ) callback_close = 1;
    return TWRC_SUCCESS;
}

/* "ui": the source's window -- a property sheet with its option pages -- has its Scan button pressed */
static int ui_tabs = -1;
static DWORD WINAPI press_scan(void *arg)
{
    DWORD pid = GetCurrentProcessId(), owner;
    HWND sheet = NULL, tabs;
    int i;

    for (i = 0; i < 200 && !sheet; i++)
    {
        HWND w = NULL;
        while ((w = FindWindowExA(NULL, w, "#32770", NULL)))
        {
            GetWindowThreadProcessId(w, &owner);
            if (owner == pid && IsWindowVisible(w) && FindWindowExA(w, NULL, "SysTabControl32", NULL)) { sheet = w; break; }
        }
        if (!sheet) Sleep(50);
    }
    if (!sheet) return 1;
    Sleep(500);
    tabs = FindWindowExA(sheet, NULL, "SysTabControl32", NULL);
    ui_tabs = SendMessageA(tabs, 0x1304 /* TCM_GETITEMCOUNT */, 0, 0);
    PostMessageA(sheet, WM_COMMAND, IDOK, 0);
    return 0;
}

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

        if (callback_ready) return 1;
        if (callback_close) return 0;
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
    BOOL two = (argc > 1 && !strcmp(argv[argc - 1], "dsm"));
    HMODULE twain = LoadLibraryA(two ? "twaindsm.dll" : "twain_32.dll");
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
    HANDLE presser = NULL;

    dsm = twain ? (DSMENTRYPROC)GetProcAddress(twain, "DSM_Entry") : NULL;
    CHECK(dsm != NULL, "%s's DSM_Entry", two ? "twaindsm.dll" : "twain_32.dll");
    if (!dsm) goto done;
    hwnd = CreateWindowA("STATIC", "scan", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    memset(&app, 0, sizeof(app));
    app.Version.MajorNum = 1;
    app.ProtocolMajor = TWON_PROTOCOLMAJOR;
    app.ProtocolMinor = TWON_PROTOCOLMINOR;
    app.SupportedGroups = DG_IMAGE | DG_CONTROL | (two ? DF_APP2 : 0);
    strcpy(app.Manufacturer, "Stained Glass");
    strcpy(app.ProductFamily, "Gates");
    strcpy(app.ProductName, "twainscan-probe");
    rc = dsm(&app, NULL, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, &hwnd);
    CHECK(rc == TWRC_SUCCESS, "the source manager opens (%u)", rc);
    if (rc) goto done;
    if (two)
    {
        TW_ENTRYPOINT entry;

        CHECK(app.SupportedGroups & DF_DSM2, "it says it is a TWAIN 2 manager (%#lx)", (unsigned long)app.SupportedGroups);
        memset(&entry, 0, sizeof(entry));
        entry.Size = sizeof(entry);
        rc = dsm(&app, NULL, DG_CONTROL, DAT_ENTRYPOINT, MSG_GET, &entry);
        CHECK(rc == TWRC_SUCCESS && entry.DSM_Entry && entry.DSM_MemAllocate && entry.DSM_MemLock,
              "it gives its entry points (%u)", rc);
        if (rc == TWRC_SUCCESS && entry.DSM_MemAllocate)
        {
            HANDLE mem = entry.DSM_MemAllocate(64);
            char *p = mem ? entry.DSM_MemLock(mem) : NULL;
            CHECK(p != NULL, "and its memory functions work");
            if (p) entry.DSM_MemUnlock(mem);
            if (mem) entry.DSM_MemFree(mem);
        }
    }

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
    if (two)
    {
        TW_CALLBACK2 cb;

        memset(&cb, 0, sizeof(cb));
        cb.CallBackProc = (TW_MEMREF)callback;
        rc = dsm(&app, &test, DG_CONTROL, DAT_CALLBACK2, MSG_REGISTER_CALLBACK, &cb);
        CHECK(rc == TWRC_SUCCESS, "a callback for the source's events is taken (%u)", rc);
    }
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
    if (show) presser = CreateThread(NULL, 0, press_scan, NULL, 0, NULL);
    callback_ready = callback_close = 0;    /* a TWAIN 2 source may call back before ENABLEDS returns */
    rc = dsm(&app, &test, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &ui);
    CHECK(rc == TWRC_SUCCESS || rc == TWRC_CHECKSTATUS, "the scan starts (%u)", rc);
    ready = wait_ready(&test);
    CHECK(ready, "the source says a page is ready");
    if (two) CHECK(callbacks > 0, "through the callback (%d calls)", callbacks);
    if (show)
    {
        WaitForSingleObject(presser, 5000);
        CHECK(ui_tabs >= 2, "the source's window has its option pages (%d tabs) and a Scan button", ui_tabs);
    }
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
    callback_ready = callback_close = 0;
    if (show) presser = CreateThread(NULL, 0, press_scan, NULL, 0, NULL);
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
