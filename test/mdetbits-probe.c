/* mdetbits-probe: IMediaDet::GetBitmapBits takes a buffer without a size (patches/sg/9417).
 *
 * Microsoft's poster-frame sample asks for the size, then the bits with no size pointer:
 *     pDet->GetBitmapBits(0, &size, NULL, w, h);  buffer = new char[size];
 *     pDet->GetBitmapBits(0, NULL, buffer, w, h);
 * Wine refused the second call (E_POINTER) and thumbnailers built that way showed nothing.
 * The probe writes an RGB24 AVI (2 s, 10 fps, each frame one colour of its own), asks for the
 * frame at 1 s both ways, checks the pixels, and that a call with neither pointer is refused.
 *
 * Prints name=value lines; see test/mdetbits-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <qedit.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(p_CLSID_MediaDet, 0x65bd0711, 0x24d2, 0x4ff7, 0x93, 0x24, 0xed, 0x2e, 0x5d, 0x3a, 0xba, 0xfa);
DEFINE_GUID(p_IID_IMediaDet, 0x65bd0710, 0x24d2, 0x4ff7, 0x93, 0x24, 0xed, 0x2e, 0x5d, 0x3a, 0xba, 0xfa);

#define W 64
#define H 48
#define FRAMES 20
#define RATE 10

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

static void put32(BYTE **p, DWORD v) { memcpy(*p, &v, 4); *p += 4; }
static void put16(BYTE **p, WORD v) { memcpy(*p, &v, 2); *p += 2; }
static void putcc(BYTE **p, const char *cc) { memcpy(*p, cc, 4); *p += 4; }

static BOOL write_avi(const char *path)
{
    static BYTE file[FRAMES * (W * H * 3 + 8) + 4096];
    DWORD frame_size = W * H * 3;
    BYTE *p = file, *riff_size, *hdrl_size, *strl_size, *movi_size, *movi, *frames[FRAMES];
    DWORD written;
    HANDLE h;
    int f, i;

    putcc(&p, "RIFF"); riff_size = p; put32(&p, 0); putcc(&p, "AVI ");
    putcc(&p, "LIST"); hdrl_size = p; put32(&p, 0); putcc(&p, "hdrl");
    putcc(&p, "avih"); put32(&p, 56);
    put32(&p, 1000000 / RATE); put32(&p, frame_size * RATE); put32(&p, 0); put32(&p, 0x10);
    put32(&p, FRAMES); put32(&p, 0); put32(&p, 1); put32(&p, frame_size);
    put32(&p, W); put32(&p, H); put32(&p, 0); put32(&p, 0); put32(&p, 0); put32(&p, 0);
    putcc(&p, "LIST"); strl_size = p; put32(&p, 0); putcc(&p, "strl");
    putcc(&p, "strh"); put32(&p, 56);
    putcc(&p, "vids"); put32(&p, 0); put32(&p, 0); put16(&p, 0); put16(&p, 0); put32(&p, 0);
    put32(&p, 1); put32(&p, RATE); put32(&p, 0); put32(&p, FRAMES); put32(&p, frame_size);
    put32(&p, 0xffffffff); put32(&p, 0);
    put16(&p, 0); put16(&p, 0); put16(&p, W); put16(&p, H);
    putcc(&p, "strf"); put32(&p, 40);
    put32(&p, 40); put32(&p, W); put32(&p, H); put16(&p, 1); put16(&p, 24);
    put32(&p, BI_RGB); put32(&p, frame_size); put32(&p, 0); put32(&p, 0); put32(&p, 0); put32(&p, 0);
    *(DWORD *)strl_size = p - strl_size - 4;
    *(DWORD *)hdrl_size = p - hdrl_size - 4;
    putcc(&p, "LIST"); movi_size = p; put32(&p, 0); movi = p; putcc(&p, "movi");
    for (f = 0; f < FRAMES; f++)
    {
        frames[f] = p;
        putcc(&p, "00db"); put32(&p, frame_size);
        for (i = 0; i < W * H; i++) { *p++ = (BYTE)(f * 12); *p++ = (BYTE)(i % W * 4); *p++ = (BYTE)(i / W * 5); }
    }
    *(DWORD *)movi_size = p - movi_size - 4;
    putcc(&p, "idx1"); put32(&p, FRAMES * 16);
    for (f = 0; f < FRAMES; f++)
    {
        putcc(&p, "00db"); put32(&p, 0x10); put32(&p, frames[f] - movi); put32(&p, frame_size);
    }
    *(DWORD *)riff_size = p - riff_size - 4;

    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    WriteFile(h, file, p - file, &written, NULL);
    CloseHandle(h);
    return written == (DWORD)(p - file);
}

int main(void)
{
    IMediaDet *det;
    BSTR name;
    long size = 0, streams = 0;
    char *buf;
    HRESULT hr;
    GUID major = GUID_NULL;
    int s, video = -1;

    CoInitialize(NULL);
    say("write", write_avi("C:\\frames.avi"), "RGB24 AVI written");
    CoCreateInstance(&p_CLSID_MediaDet, NULL, CLSCTX_INPROC_SERVER, &p_IID_IMediaDet, (void **)&det);
    name = SysAllocString(L"C:\\frames.avi");
    hr = IMediaDet_put_Filename(det, name);
    IMediaDet_get_OutputStreams(det, &streams);
    for (s = 0; s < streams; s++)
    {
        IMediaDet_put_CurrentStream(det, s);
        IMediaDet_get_StreamType(det, &major);
        if (IsEqualGUID(&major, &MEDIATYPE_Video)) { video = s; break; }
    }
    say("open", hr == S_OK && video >= 0, "put_Filename %#lx, video stream %d", hr, video);
    if (video < 0) goto done;

    hr = IMediaDet_GetBitmapBits(det, 1.0, &size, NULL, 0, 0);
    say("size", hr == S_OK && size == (long)sizeof(BITMAPINFOHEADER) + W * H * 3, "size %#lx: %ld", hr, size);
    if (hr != S_OK || size <= 0) goto done;
    buf = calloc(1, size);
    hr = IMediaDet_GetBitmapBits(det, 1.0, NULL, buf, 0, 0);
    {
        BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)buf;
        BYTE *px = (BYTE *)(bih + 1);
        /* frame 10, bottom-up rows: the file's row 0 first; pixel (x, row) = (B f*12, G x*4, R row*5) */
        int ok = hr == S_OK && bih->biWidth == W && bih->biHeight == H && bih->biBitCount == 24
                && px[0] == 120 && px[(5 * W + 7) * 3 + 1] == 28 && px[(5 * W + 7) * 3 + 2] == 25;
        say("nosize", ok, "bits with no size pointer %#lx: %ldx%ld, first pixel %d,%d,%d", hr, bih->biWidth, bih->biHeight,
                px[0], px[1], px[2]);
    }
    size = 0;
    hr = IMediaDet_GetBitmapBits(det, 1.0, NULL, NULL, 0, 0);
    say("neither", hr == E_POINTER, "neither pointer: %#lx (E_POINTER)", hr);
    free(buf);
done:
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
