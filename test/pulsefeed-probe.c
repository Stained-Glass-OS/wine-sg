/* test/pulsefeed-gate.sh: a tone through DirectSound, as Media Player's sound goes (quartz's
 * DirectSound renderer -> dsound's mixer -> winepulse). A looping one-second buffer of a 330 Hz
 * sine plays for argv[1] seconds. Prints "playing" once it plays. */
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    IDirectSound8 *ds;
    IDirectSoundBuffer *buf;
    DSBUFFERDESC desc = {sizeof(desc)};
    WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, 2, 48000, 48000 * 4, 4, 16, 0};
    short *p1, *p2;
    DWORD n1, n2, i;
    int secs = argc > 1 ? atoi(argv[1]) : 10;

    if (FAILED(DirectSoundCreate8(NULL, &ds, NULL))) { printf("no DirectSound\n"); return 1; }
    IDirectSound8_SetCooperativeLevel(ds, GetDesktopWindow(), DSSCL_PRIORITY);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
    desc.dwBufferBytes = wfx.nAvgBytesPerSec;
    desc.lpwfxFormat = &wfx;
    if (FAILED(IDirectSound8_CreateSoundBuffer(ds, &desc, &buf, NULL))) { printf("no buffer\n"); return 1; }
    if (FAILED(IDirectSoundBuffer_Lock(buf, 0, desc.dwBufferBytes, (void **)&p1, &n1, (void **)&p2, &n2, 0))) return 1;
    for (i = 0; i < n1 / 4; i++)
        p1[2 * i] = p1[2 * i + 1] = (short)(8000 * sin(2 * 3.14159265358979 * 330 * i / 48000.0));
    IDirectSoundBuffer_Unlock(buf, p1, n1, p2, n2);
    if (FAILED(IDirectSoundBuffer_Play(buf, 0, 0, DSBPLAY_LOOPING))) { printf("no play\n"); return 1; }
    printf("playing\n"); fflush(stdout);
    Sleep(secs * 1000);
    IDirectSoundBuffer_Stop(buf);
    printf("done\n");
    return 0;
}
