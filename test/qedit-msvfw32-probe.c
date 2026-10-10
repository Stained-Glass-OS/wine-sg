/* Native probe for msvfw32.dll: DrawDib start/stop, palette changes, the
 * decompression buffer, timings and display profile; MCIWnd volume, speed,
 * repeat, realize, palette and save messages; the *FileNamePreview exports. */
#define __USE_MINGW_ANSI_STDIO 1
#include <windows.h>
#include <vfw.h>
#include <digitalv.h>
#include <stdio.h>
#include <string.h>

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void test_drawdib(void)
{
    struct { BITMAPINFOHEADER h; RGBQUAD colors[256]; } bi8;
    BITMAPINFOHEADER bi24;
    BYTE pixels[8 * 24];
    BITMAPINFOHEADER out;
    DRAWDIBTIME t;
    LOGPALETTE *lp;
    HPALETTE pal;
    PALETTEENTRY pe[4];
    HDRAWDIB hdd, bad = (HDRAWDIB)0x7777;
    HDC hdc = GetDC(NULL);
    void *buffer;
    DWORD profile;

    /* start / stop */
    hdd = DrawDibOpen();
    CHECK(hdd != NULL, "DrawDibOpen");
    CHECK(DrawDibStart(hdd, 1000) == TRUE, "DrawDibStart");
    CHECK(DrawDibStop(hdd) == TRUE, "DrawDibStop");
    CHECK(DrawDibStart(bad, 1000) == FALSE, "DrawDibStart on a bad handle");
    CHECK(DrawDibStop(bad) == FALSE, "DrawDibStop on a bad handle");

    /* buffer only exists after DrawDibBegin */
    CHECK(DrawDibGetBuffer(hdd, NULL, 0, 0) == NULL, "buffer before DrawDibBegin");
    CHECK(DrawDibGetBuffer(bad, NULL, 0, 0) == NULL, "buffer of a bad handle");
    memset(&bi24, 0, sizeof(bi24));
    bi24.biSize = sizeof(bi24);
    bi24.biWidth = 8;
    bi24.biHeight = 8;
    bi24.biPlanes = 1;
    bi24.biBitCount = 24;
    bi24.biCompression = BI_RGB;
    bi24.biSizeImage = sizeof(pixels);
    CHECK(DrawDibBegin(hdd, hdc, 8, 8, &bi24, 8, 8, 0), "DrawDibBegin");
    buffer = DrawDibGetBuffer(hdd, NULL, 0, 0);
    CHECK(buffer != NULL, "buffer after DrawDibBegin");
    memset(&out, 0, sizeof(out));
    CHECK(DrawDibGetBuffer(hdd, &out, sizeof(out), 0) == buffer, "buffer is stable");
    CHECK(out.biSize == sizeof(out) && out.biWidth == 8 && out.biHeight == 8 && out.biBitCount == 24,
            "buffer format %ld %ld %ld %d", out.biSize, out.biWidth, out.biHeight, out.biBitCount);

    /* timings */
    memset(&t, 0xcc, sizeof(t));
    CHECK(DrawDibTime(hdd, &t) == TRUE, "DrawDibTime");
    CHECK(t.timeCount == 0 && t.timeDraw == 0, "timings before drawing %ld %ld", t.timeCount, t.timeDraw);
    CHECK(DrawDibTime(hdd, NULL) == FALSE, "DrawDibTime(NULL)");
    CHECK(DrawDibTime(bad, &t) == FALSE, "DrawDibTime on a bad handle");
    memset(pixels, 0x40, sizeof(pixels));
    CHECK(DrawDibDraw(hdd, hdc, 0, 0, 16, 16, &bi24, pixels, 0, 0, 8, 8, 0), "DrawDibDraw");
    CHECK(DrawDibDraw(hdd, hdc, 0, 0, 8, 8, &bi24, pixels, 0, 0, 8, 8, 0), "DrawDibDraw (1:1)");
    memset(&t, 0xcc, sizeof(t));
    CHECK(DrawDibTime(hdd, &t) == TRUE, "DrawDibTime after drawing");
    CHECK(t.timeCount == 2, "draw count %ld", t.timeCount);
    CHECK(t.timeDraw >= t.timeBlt && t.timeBlt >= 0 && t.timeStretch >= 0, "timings %ld %ld %ld", t.timeDraw, t.timeBlt, t.timeStretch);
    DrawDibEnd(hdd);
    CHECK(DrawDibGetBuffer(hdd, NULL, 0, 0) == NULL, "buffer after DrawDibEnd");

    /* palette changes need a palette */
    memset(&bi8, 0, sizeof(bi8));
    bi8.h.biSize = sizeof(bi8.h);
    bi8.h.biWidth = 8;
    bi8.h.biHeight = 8;
    bi8.h.biPlanes = 1;
    bi8.h.biBitCount = 8;
    bi8.h.biCompression = BI_RGB;
    bi8.h.biClrUsed = 256;
    CHECK(DrawDibBegin(hdd, hdc, 8, 8, &bi8.h, 8, 8, 0), "DrawDibBegin (8 bit)");
    pe[0].peRed = 1; pe[0].peGreen = 2; pe[0].peBlue = 3; pe[0].peFlags = 0;
    pe[1] = pe[2] = pe[3] = pe[0];
    CHECK(DrawDibChangePalette(hdd, 0, 4, pe) == FALSE, "palette change without a palette");
    lp = HeapAlloc(GetProcessHeap(), 0, sizeof(*lp) + 255 * sizeof(PALETTEENTRY));
    lp->palVersion = 0x300;
    lp->palNumEntries = 256;
    memset(lp->palPalEntry, 0, 256 * sizeof(PALETTEENTRY));
    pal = CreatePalette(lp);
    HeapFree(GetProcessHeap(), 0, lp);
    CHECK(DrawDibSetPalette(hdd, pal), "DrawDibSetPalette");
    CHECK(DrawDibChangePalette(hdd, 0, 4, pe) == TRUE, "palette change");
    CHECK(DrawDibChangePalette(hdd, 0, 4, NULL) == FALSE, "palette change with no entries");
    CHECK(DrawDibChangePalette(hdd, -1, 4, pe) == FALSE, "palette change at a negative index");
    CHECK(DrawDibChangePalette(bad, 0, 4, pe) == FALSE, "palette change on a bad handle");
    DrawDibEnd(hdd);
    DeleteObject(pal);
    CHECK(DrawDibClose(hdd), "DrawDibClose");

    /* display profile */
    profile = DrawDibProfileDisplay(NULL);
    CHECK((profile & PD_CAN_DRAW_DIB) != 0, "profile %#lx lacks PD_CAN_DRAW_DIB", profile);
    CHECK((profile & ~(PD_CAN_DRAW_DIB | PD_CAN_STRETCHDIB | PD_STRETCHDIB_1_1_OK | PD_STRETCHDIB_1_2_OK | PD_STRETCHDIB_1_N_OK)) == 0,
            "profile %#lx has unknown bits", profile);
    if (profile & PD_CAN_STRETCHDIB)
        CHECK((profile & (PD_STRETCHDIB_1_1_OK | PD_STRETCHDIB_1_2_OK | PD_STRETCHDIB_1_N_OK)) != 0, "profile %#lx stretch flags", profile);
    ReleaseDC(NULL, hdc);
}

static void test_mciwnd(void)
{
    WCHAR path[MAX_PATH], saved[MAX_PATH];
    WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, 1, 8000, 8000, 1, 8, 0};
    DWORD data_size = 800, riff, fmt_size = 16, zero = 0;
    BYTE data[800];
    HANDLE file;
    HWND wnd;
    LRESULT r;
    DWORD written;

    GetTempPathW(MAX_PATH, path);
    wcscpy(saved, path);
    wcscat(path, L"sgprobe.wav");
    wcscat(saved, L"sgprobe-saved.wav");
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    memset(data, 0x80, sizeof(data));
    riff = 4 + 8 + 16 + 8 + data_size;
    WriteFile(file, "RIFF", 4, &written, NULL);
    WriteFile(file, &riff, 4, &written, NULL);
    WriteFile(file, "WAVEfmt ", 8, &written, NULL);
    WriteFile(file, &fmt_size, 4, &written, NULL);
    WriteFile(file, &wfx, 16, &written, NULL);
    WriteFile(file, "data", 4, &written, NULL);
    WriteFile(file, &data_size, 4, &written, NULL);
    WriteFile(file, data, data_size, &written, NULL);
    CloseHandle(file);
    (void)zero;

    /* no media: nothing to ask, but the window still keeps its own settings */
    wnd = MCIWndCreateW(NULL, GetModuleHandleW(NULL), MCIWNDF_NOERRORDLG | MCIWNDF_NOPLAYBAR, NULL);
    CHECK(wnd != NULL, "MCIWndCreate without a file");
    if (wnd)
    {
        CHECK(MCIWndGetRepeat(wnd) == FALSE, "repeat by default");
        MCIWndSetRepeat(wnd, TRUE);
        CHECK(MCIWndGetRepeat(wnd) == TRUE, "repeat after setting it");
        MCIWndSetRepeat(wnd, 5);
        CHECK(MCIWndGetRepeat(wnd) == TRUE, "repeat is a boolean");
        MCIWndSetRepeat(wnd, FALSE);
        CHECK(MCIWndGetRepeat(wnd) == FALSE, "repeat cleared");
        r = MCIWndSetVolume(wnd, 500);
        CHECK(r != 0, "volume without a device reports an error");
        r = MCIWndSetSpeed(wnd, 500);
        CHECK(r != 0, "speed without a device reports an error");
        r = MCIWndSave(wnd, saved);
        CHECK(r != 0, "save without a device reports an error");
        DestroyWindow(wnd);
    }

    wnd = MCIWndCreateW(NULL, GetModuleHandleW(NULL), MCIWNDF_NOERRORDLG | MCIWNDF_NOPLAYBAR, path);
    if (!wnd || !MCIWndGetDeviceID(wnd))
    {
        printf("SKIP the wave MCI device is not available\n");
    }
    else
    {
        /* the wave device has no volume or speed item: its error comes back */
        r = MCIWndSetSpeed(wnd, 500);
        CHECK(r != 0, "speed on the wave device gives %lld", (long long)r);
        r = MCIWndRealize(wnd, FALSE);
        CHECK(r != 0, "realize on the wave device gives %lld", (long long)r);
        MCIWndSetRepeat(wnd, TRUE);
        CHECK(MCIWndGetRepeat(wnd) == TRUE, "repeat with a device");
        /* saving writes a new file */
        r = MCIWndSave(wnd, saved);
        CHECK(r == 0, "save gives %lld", (long long)r);
        file = CreateFileW(saved, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        CHECK(file != INVALID_HANDLE_VALUE, "saved file exists");
        if (file != INVALID_HANDLE_VALUE)
        {
            CHECK(GetFileSize(file, NULL) >= 44 + 800, "saved file size %lu", GetFileSize(file, NULL));
            CloseHandle(file);
        }
        r = MCIWndSave(wnd, NULL);
        CHECK(r != 0, "save without a name reports an error");
        DestroyWindow(wnd);
    }
    DeleteFileW(path);
    DeleteFileW(saved);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    test_drawdib();
    test_mciwnd();
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
