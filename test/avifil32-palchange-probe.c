/* Probe for patches/sg/4589: AVIStreamReadFormat of an 8-bit video stream
 * with palette changes ('pc' chunks) gives each frame the colour table in
 * force at that frame: a change written after frame n applies from frame
 * n + 1 on, and changes add up. The probe writes the AVI itself.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <vfw.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static BYTE buf[8192];
static DWORD pos;
static void put(const void *p, DWORD n) { memcpy(buf + pos, p, n); pos += n; }
static void put32(DWORD v) { put(&v, 4); }
static void fourcc(const char *c) { put(c, 4); }
static DWORD open_chunk(const char *id) { fourcc(id); put32(0); return pos; }
static void close_chunk(DWORD start) { *(DWORD *)(buf + start - 4) = pos - start; if (pos & 1) buf[pos++] = 0; }

/* the initial table: 4 colours, then two changes: after frame 0 entries 1-2, after frame 2 entry 3 */
static const RGBQUAD initial[4] = {{0, 0, 0, 0}, {0x10, 0x20, 0x30, 0}, {0x40, 0x50, 0x60, 0}, {0x70, 0x80, 0x90, 0}};

static void write_avi(const char *path)
{
    DWORD riff, hdrl, strl, strf, movi, chunk, idx_start, offsets[6], sizes[6], n = 0, i, written;
    static const char *ids[6] = {"00db", "00pc", "00db", "00db", "00pc", "00db"};
    BITMAPINFOHEADER bi = {sizeof(bi), 4, 4, 1, 8, BI_RGB, 16, 0, 0, 4, 4};
    HANDLE f;

    pos = 0;
    fourcc("RIFF"); put32(0); riff = pos; fourcc("AVI ");
    fourcc("LIST"); put32(0); hdrl = pos; fourcc("hdrl");
    /* avih: usec/frame, max bytes/s, padding, flags, frames, initial frames, streams, buffer, width, height, 4 reserved */
    chunk = open_chunk("avih");
    put32(100000); put32(0); put32(0); put32(AVIF_HASINDEX); put32(4); put32(0); put32(1); put32(16); put32(4); put32(4);
    put32(0); put32(0); put32(0); put32(0);
    close_chunk(chunk);
    fourcc("LIST"); put32(0); strl = pos; fourcc("strl");
    /* strh: type, handler, flags, priority+language, initial frames, scale, rate, start, length, buffer, quality, sample size, frame */
    chunk = open_chunk("strh");
    fourcc("vids"); fourcc("DIB "); put32(AVISF_VIDEO_PALCHANGES); put32(0); put32(0); put32(1); put32(10); put32(0); put32(4);
    put32(16); put32(-1); put32(0); put32(0); put32((4 << 16) | 4);
    close_chunk(chunk);
    strf = open_chunk("strf"); put(&bi, sizeof(bi)); put(initial, sizeof(initial)); close_chunk(strf);
    *(DWORD *)(buf + strl - 4) = pos - strl;
    *(DWORD *)(buf + hdrl - 4) = pos - hdrl;

    fourcc("LIST"); put32(0); movi = pos; fourcc("movi");
    for (i = 0; i < 6; i++)
    {
        offsets[i] = pos - movi;
        chunk = open_chunk(ids[i]);
        if (ids[i][3] == 'b')
        {
            BYTE frame[16];
            memset(frame, i, sizeof(frame));
            put(frame, sizeof(frame));
        }
        else if (i == 1)
        {
            BYTE change[4 + 2 * 4] = {1, 2, 0, 0, 0xa1, 0xa2, 0xa3, 0, 0xb1, 0xb2, 0xb3, 0}; /* first 1, two entries (R, G, B, flags) */
            put(change, sizeof(change));
        }
        else
        {
            BYTE change[4 + 4] = {3, 1, 0, 0, 0xc1, 0xc2, 0xc3, 0};
            put(change, sizeof(change));
        }
        sizes[i] = pos - chunk;
        close_chunk(chunk);
        n++;
    }
    *(DWORD *)(buf + movi - 4) = pos - movi;
    idx_start = open_chunk("idx1");
    for (i = 0; i < n; i++)
    {
        fourcc(ids[i]); put32(ids[i][3] == 'b' ? AVIIF_KEYFRAME : AVIIF_NOTIME); put32(offsets[i] - 4 + 4); put32(sizes[i]);
    }
    close_chunk(idx_start);
    *(DWORD *)(buf + riff - 4) = pos - riff;

    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, buf, pos, &written, NULL);
    CloseHandle(f);
}

int main(int argc, char **argv)
{
    /* the expected colour table (blue, green, red) for each frame */
    static const struct { LONG frame; RGBQUAD c[4]; } want[] =
    {
        {0, {{0, 0, 0}, {0x10, 0x20, 0x30}, {0x40, 0x50, 0x60}, {0x70, 0x80, 0x90}}},
        {1, {{0, 0, 0}, {0xa3, 0xa2, 0xa1}, {0xb3, 0xb2, 0xb1}, {0x70, 0x80, 0x90}}},
        {2, {{0, 0, 0}, {0xa3, 0xa2, 0xa1}, {0xb3, 0xb2, 0xb1}, {0x70, 0x80, 0x90}}},
        {3, {{0, 0, 0}, {0xa3, 0xa2, 0xa1}, {0xb3, 0xb2, 0xb1}, {0xc3, 0xc2, 0xc1}}},
    };
    char path[MAX_PATH];
    PAVIFILE file;
    PAVISTREAM stream;
    AVISTREAMINFOA info;
    HRESULT hr;
    unsigned int i, j;

    GetTempPathA(MAX_PATH, path);
    strcat(path, "sg-palchange.avi");
    write_avi(path);
    AVIFileInit();
    hr = AVIFileOpenA(&file, path, OF_READ, NULL);
    check(hr == AVIERR_OK, "AVIFileOpen %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }
    hr = AVIFileGetStream(file, &stream, streamtypeVIDEO, 0);
    check(hr == AVIERR_OK, "AVIFileGetStream %#lx", hr);
    AVIStreamInfoA(stream, &info, sizeof(info));
    check(info.dwFlags & AVISTREAMINFO_FORMATCHANGES, "the stream has format changes (%#lx)", info.dwFlags);

    for (i = 0; i < ARRAYSIZE(want); i++)
    {
        struct { BITMAPINFOHEADER h; RGBQUAD c[4]; } fmt;
        LONG size = sizeof(fmt);
        int same = 1;

        memset(&fmt, 0xcc, sizeof(fmt));
        hr = AVIStreamReadFormat(stream, want[i].frame, &fmt, &size);
        for (j = 0; j < 4; j++)
            if (fmt.c[j].rgbBlue != want[i].c[j].rgbBlue || fmt.c[j].rgbGreen != want[i].c[j].rgbGreen
                    || fmt.c[j].rgbRed != want[i].c[j].rgbRed) same = 0;
        check(hr == AVIERR_OK && size == sizeof(fmt) && same, "frame %ld: colour 1 %02x%02x%02x, colour 3 %02x%02x%02x",
              want[i].frame, fmt.c[1].rgbRed, fmt.c[1].rgbGreen, fmt.c[1].rgbBlue, fmt.c[3].rgbRed, fmt.c[3].rgbGreen, fmt.c[3].rgbBlue);
    }

    AVIStreamRelease(stream);
    AVIFileRelease(file);
    AVIFileExit();
    DeleteFileA(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
