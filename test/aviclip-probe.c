/* aviclip-probe: AVI files on the clipboard (patches/sg/6511).
 *
 * AVIPutFileOnClipboard, AVIGetFromClipboard, AVIMakeStreamFromClipboard and AVIClearClipboard
 * were stubs. Writes a small AVI (two video frames, half a second of sound), puts it on the
 * clipboard, and reads it back as this process (the file itself), as the plain clipboard (CF_DIB,
 * CF_WAVE) and as another process (AVIGetFromClipboard in a child); makes streams of a DIB and
 * a WAVE; clears.
 *
 * Prints name=value lines; see test/aviclip-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <vfw.h>
#include <stdio.h>
#include <string.h>

/* (mingw's IID_IAVIEditStream resolves to the import library's thunk) */
static const GUID iid_edit_stream = { 0x00020024, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };

#define W 32
#define H 16
#define RATE 8000
#define AUDIO_SAMPLES 4000

static BYTE frame_bits[2][W * H * 3];

static void make_frames(void)
{
    int i, f;
    for (f = 0; f < 2; f++)
        for (i = 0; i < W * H; i++)
        {
            /* BGR: frame 0 is blue-ish (200, 40, 10), frame 1 green */
            frame_bits[f][i * 3 + 0] = f ? 10 : 200;
            frame_bits[f][i * 3 + 1] = f ? 220 : 40;
            frame_bits[f][i * 3 + 2] = 10;
        }
}

static HRESULT write_avi(const char *path)
{
    AVISTREAMINFOA vinfo = { 0 }, ainfo = { 0 };
    BITMAPINFOHEADER bih = { sizeof(bih), W, H, 1, 24, BI_RGB, W * H * 3 };
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, RATE, RATE * 2, 2, 16, 0 };
    PAVIFILE file;
    PAVISTREAM video, audio;
    static short samples[AUDIO_SAMPLES];
    HRESULT hr;
    int i;

    for (i = 0; i < AUDIO_SAMPLES; i++) samples[i] = (short)((i % 40) < 20 ? 8000 : -8000);
    DeleteFileA(path);
    if (FAILED(hr = AVIFileOpenA(&file, path, OF_CREATE | OF_WRITE, NULL))) return hr;
    vinfo.fccType = streamtypeVIDEO;
    vinfo.fccHandler = mmioFOURCC('D','I','B',' ');
    vinfo.dwScale = 1;
    vinfo.dwRate = 10;
    vinfo.dwSuggestedBufferSize = W * H * 3;
    SetRect(&vinfo.rcFrame, 0, 0, W, H);
    ainfo.fccType = streamtypeAUDIO;
    ainfo.dwScale = 2;
    ainfo.dwRate = RATE * 2;
    ainfo.dwSampleSize = 2;
    /* the streams first, then their data */
    if (FAILED(hr = AVIFileCreateStreamA(file, &video, &vinfo))) goto done;
    if (FAILED(hr = AVIFileCreateStreamA(file, &audio, &ainfo))) { AVIStreamRelease(video); goto done; }
    AVIStreamSetFormat(video, 0, &bih, sizeof(bih));
    AVIStreamSetFormat(audio, 0, &wfx, sizeof(wfx));
    for (i = 0; i < 2; i++) AVIStreamWrite(video, i, 1, frame_bits[i], sizeof(frame_bits[i]), AVIIF_KEYFRAME, NULL, NULL);
    AVIStreamWrite(audio, 0, AUDIO_SAMPLES, samples, sizeof(samples), 0, NULL, NULL);
    AVIStreamRelease(video);
    AVIStreamRelease(audio);
done:
    AVIFileRelease(file);
    return hr;
}

/* a file's streams: is its video frame 0 frame 0, its audio the length written */
static void check_file(const char *prefix, PAVIFILE file)
{
    PAVISTREAM video = NULL, audio = NULL;
    PGETFRAME frame;
    BITMAPINFOHEADER *bih;
    int video_ok = 0, audio_ok = 0;

    if (SUCCEEDED(AVIFileGetStream(file, &video, streamtypeVIDEO, 0)))
    {
        if ((frame = AVIStreamGetFrameOpen(video, NULL)))
        {
            if ((bih = AVIStreamGetFrame(frame, AVIStreamStart(video))))
            {
                BYTE *bits = (BYTE *)bih + bih->biSize + bih->biClrUsed * 4;
                video_ok = bih->biWidth == W && bih->biHeight == H && bits[0] == 200 && bits[1] == 40 && bits[2] == 10;
            }
            AVIStreamGetFrameClose(frame);
        }
        AVIStreamRelease(video);
    }
    if (SUCCEEDED(AVIFileGetStream(file, &audio, streamtypeAUDIO, 0)))
    {
        audio_ok = AVIStreamLength(audio) == AUDIO_SAMPLES;
        if (!audio_ok)
        {
            AVISTREAMINFOA ai;
            AVIStreamInfoA(audio, &ai, sizeof(ai));
            printf("%saudio_length=%ld samplesize=%lu scale=%lu rate=%lu\n", prefix, AVIStreamLength(audio),
                   ai.dwSampleSize, ai.dwScale, ai.dwRate);
        }
        AVIStreamRelease(audio);
    }
    printf("%svideo=%d\n%saudio=%d\n", prefix, video_ok, prefix, audio_ok);
}

static int child(void)
{
    PAVIFILE file = NULL;
    HRESULT hr;

    AVIFileInit();
    hr = AVIGetFromClipboard(&file);
    printf("child_get=%d hr=%#lx\n", SUCCEEDED(hr) && file, hr);
    if (file)
    {
        check_file("child_", file);
        AVIFileRelease(file);
    }
    fflush(stdout);
    return 0;
}

static void pump_until(HANDLE process)
{
    MSG msg;
    while (MsgWaitForMultipleObjects(1, &process, FALSE, 30000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
}

int main(int argc, char **argv)
{
    char path[MAX_PATH], cmd[MAX_PATH * 2];
    PAVIFILE file, got = NULL;
    PAVISTREAM stream;
    AVISTREAMINFOW info;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HGLOBAL global;
    HRESULT hr;
    BYTE *mem;
    int ok;

    if (argc > 1 && !strcmp(argv[1], "child")) return child();
    setvbuf(stdout, NULL, _IONBF, 0);
    AVIFileInit();
    make_frames();
    GetTempPathA(MAX_PATH, path);
    strcat(path, "aviclip.avi");
    hr = write_avi(path);
    printf("written=%d hr=%#lx\n", SUCCEEDED(hr), hr);
    if (FAILED(hr = AVIFileOpenA(&file, path, OF_READ, NULL))) { printf("open=0 hr=%#lx\n", hr); return 1; }

    check_file("orig_", file);

    /* on the clipboard */
    hr = AVIPutFileOnClipboard(file);
    printf("put=%d hr=%#lx\n", hr == AVIERR_OK, hr);
    hr = AVIGetFromClipboard(&got);
    printf("same_file=%d hr=%#lx\n", hr == AVIERR_OK && got == file, hr);
    if (got) AVIFileRelease(got);

    /* as every program sees it */
    ok = 0;
    if (OpenClipboard(NULL))
    {
        if ((global = GetClipboardData(CF_DIB)) && (mem = GlobalLock(global)))
        {
            BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)mem;
            BYTE *bits = mem + bih->biSize;
            ok = bih->biWidth == W && bih->biHeight == H && bih->biBitCount == 24 && bits[0] == 200 && bits[1] == 40;
            GlobalUnlock(global);
        }
        printf("clip_dib=%d\n", ok);
        ok = 0;
        if ((global = GetClipboardData(CF_WAVE)) && (mem = GlobalLock(global)))
        {
            ok = !memcmp(mem, "RIFF", 4) && !memcmp(mem + 8, "WAVEfmt ", 8) && ((WAVEFORMATEX *)(mem + 20))->nSamplesPerSec == RATE;
            if (ok)
            {
                DWORD fmt = *(DWORD *)(mem + 16);
                BYTE *data = mem + 20 + fmt + (fmt & 1);
                ok = !memcmp(data, "data", 4) && *(DWORD *)(data + 4) == AUDIO_SAMPLES * 2 && *(short *)(data + 8) == 8000;
            }
            GlobalUnlock(global);
        }
        printf("clip_wave=%d\n", ok);
        CloseClipboard();
    }
    else printf("clip_dib=0 open_clipboard=0\n");

    /* as another process sees it */
    sprintf(cmd, "\"%s\" child", argv[0]);
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        pump_until(pi.hProcess);
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else printf("child_get=0 create=%lu\n", GetLastError());

    /* streams of a DIB and of a WAVE */
    global = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + sizeof(frame_bits[1]));
    mem = GlobalLock(global);
    {
        BITMAPINFOHEADER bih = { sizeof(bih), W, H, 1, 24, BI_RGB, 0 };
        memcpy(mem, &bih, sizeof(bih));
        memcpy(mem + sizeof(bih), frame_bits[1], sizeof(frame_bits[1]));
    }
    GlobalUnlock(global);
    stream = NULL;
    hr = AVIMakeStreamFromClipboard(CF_DIB, global, &stream);
    ok = 0;
    if (SUCCEEDED(hr) && stream)
    {
        BYTE buf[sizeof(frame_bits[1])];
        LONG bytes = 0;
        IAVIEditStream *edit;

        AVIStreamInfoW(stream, &info, sizeof(info));
        ok = info.fccType == streamtypeVIDEO && AVIStreamLength(stream) == 1 &&
             SUCCEEDED(AVIStreamRead(stream, 0, 1, buf, sizeof(buf), &bytes, NULL)) && bytes == sizeof(buf) &&
             !memcmp(buf, frame_bits[1], sizeof(buf));
        if (!ok) printf("dib_detail type=%d length=%ld bytes=%ld\n", info.fccType == streamtypeVIDEO,
                        AVIStreamLength(stream), bytes);
        if (SUCCEEDED(stream->lpVtbl->QueryInterface(stream, &iid_edit_stream, (void **)&edit)))
            edit->lpVtbl->Release(edit);
        else { ok = 0; printf("dib_detail not_editable\n"); }
        AVIStreamRelease(stream);
    }
    printf("dib_stream=%d hr=%#lx\n", ok, hr);
    GlobalFree(global);

    {
        static const BYTE wave[] =
        {
            'R','I','F','F', 36 + 8, 0, 0, 0, 'W','A','V','E', 'f','m','t',' ', 16, 0, 0, 0,
            1, 0, 1, 0, 0x40, 0x1f, 0, 0, 0x80, 0x3e, 0, 0, 2, 0, 16, 0,
            'd','a','t','a', 8, 0, 0, 0, 1, 0, 2, 0, 3, 0, 4, 0,
        };
        global = GlobalAlloc(GMEM_MOVEABLE, sizeof(wave));
        memcpy(GlobalLock(global), wave, sizeof(wave));
        GlobalUnlock(global);
        stream = NULL;
        hr = AVIMakeStreamFromClipboard(CF_WAVE, global, &stream);
        ok = 0;
        if (SUCCEEDED(hr) && stream)
        {
            short buf[4] = { 0 };
            AVIStreamInfoW(stream, &info, sizeof(info));
            ok = info.fccType == streamtypeAUDIO && AVIStreamLength(stream) == 4 &&
                 SUCCEEDED(AVIStreamRead(stream, 1, 2, buf, sizeof(buf), NULL, NULL)) && buf[0] == 2 && buf[1] == 3;
            AVIStreamRelease(stream);
        }
        printf("wave_stream=%d hr=%#lx\n", ok, hr);
        stream = (PAVISTREAM)0xdeadbeef;
        hr = AVIMakeStreamFromClipboard(CF_TEXT, global, &stream);
        printf("other_format=%d hr=%#lx\n", FAILED(hr) && !stream, hr);
        GlobalFree(global);
    }

    /* cleared */
    hr = AVIClearClipboard();
    got = NULL;
    ok = hr == AVIERR_OK && !IsClipboardFormatAvailable(CF_DIB) && FAILED(AVIGetFromClipboard(&got)) && !got;
    printf("cleared=%d hr=%#lx\n", ok, hr);

    AVIFileRelease(file);
    AVIFileExit();
    DeleteFileA(path);
    printf("done=1\n");
    return 0;
}
