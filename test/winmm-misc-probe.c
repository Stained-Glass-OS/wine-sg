/* winmm's small stubs (patches/sg/2832), run by test/winmm-misc-gate.sh:
 * mixerGetLineInfo with MIXER_GETLINEINFOF_TARGETTYPE, mciGetDeviceIDFromElementID,
 * midiOutCachePatches / midiOutCacheDrumPatches, joyConfigChanged and the growth
 * of an mmio memory file (FIXMEs and stubs before). A part needs an audio
 * device or a MIDI device and is skipped without one (said on the line).
 *
 *   winmm-misc-probe.exe */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static void test_mixer_targettype(void)
{
    UINT n = mixerGetNumDevs(), i;
    int tested = 0;

    for (i = 0; i < n; i++)
    {
        MIXERCAPSW caps;
        HMIXER mixer;
        MIXERLINEW line;
        MMRESULT mr;
        UINT outs = waveOutGetNumDevs(), ins = waveInGetNumDevs(), d;

        if (mixerOpen(&mixer, i, 0, 0, 0) != MMSYSERR_NOERROR) continue;
        mixerGetDevCapsW(i, &caps, sizeof(caps));

        for (d = 0; d < outs; d++)
        {
            WAVEOUTCAPSW wc;
            waveOutGetDevCapsW(d, &wc, sizeof(wc));
            if (wcscmp(wc.szPname, caps.szPname)) continue;
            memset(&line, 0, sizeof(line));
            line.cbStruct = sizeof(line);
            line.Target.dwType = MIXERLINE_TARGETTYPE_WAVEOUT;
            line.Target.wMid = wc.wMid;
            line.Target.wPid = wc.wPid;
            line.Target.vDriverVersion = wc.vDriverVersion;
            wcscpy(line.Target.szPname, wc.szPname);
            mr = mixerGetLineInfoW((HMIXEROBJ)mixer, &line, MIXER_GETLINEINFOF_TARGETTYPE);
            CHECKF(mr == MMSYSERR_NOERROR && line.Target.dwType == MIXERLINE_TARGETTYPE_WAVEOUT &&
                   line.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,
                   "mixer %u: TARGETTYPE WAVEOUT finds the wave out source line (%u, type %lu)", i, mr, line.dwComponentType);
            tested++;

            line.Target.szPname[0] = 'X';
            mr = mixerGetLineInfoW((HMIXEROBJ)mixer, &line, MIXER_GETLINEINFOF_TARGETTYPE);
            CHECKF(mr == MIXERR_INVALLINE, "mixer %u: a different device name finds no line (%u)", i, mr);

            memset(&line, 0, sizeof(line));
            line.cbStruct = sizeof(line);
            line.Target.dwType = MIXERLINE_TARGETTYPE_WAVEIN;
            wcscpy(line.Target.szPname, wc.szPname);
            mr = mixerGetLineInfoW((HMIXEROBJ)mixer, &line, MIXER_GETLINEINFOF_TARGETTYPE);
            CHECKF(mr == MIXERR_INVALLINE, "mixer %u: WAVEIN target on a wave out mixer finds no line (%u)", i, mr);
            break;
        }
        for (d = 0; d < ins; d++)
        {
            WAVEINCAPSW wc;
            waveInGetDevCapsW(d, &wc, sizeof(wc));
            if (wcscmp(wc.szPname, caps.szPname)) continue;
            memset(&line, 0, sizeof(line));
            line.cbStruct = sizeof(line);
            line.Target.dwType = MIXERLINE_TARGETTYPE_WAVEIN;
            line.Target.wMid = wc.wMid;
            line.Target.wPid = wc.wPid;
            line.Target.vDriverVersion = wc.vDriverVersion;
            wcscpy(line.Target.szPname, wc.szPname);
            mr = mixerGetLineInfoW((HMIXEROBJ)mixer, &line, MIXER_GETLINEINFOF_TARGETTYPE);
            CHECKF(mr == MMSYSERR_NOERROR && line.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN,
                   "mixer %u: TARGETTYPE WAVEIN finds the wave in destination line (%u)", i, mr);
            tested++;
            break;
        }
        memset(&line, 0, sizeof(line));
        line.cbStruct = sizeof(line);
        line.Target.dwType = 99;
        mr = mixerGetLineInfoW((HMIXEROBJ)mixer, &line, MIXER_GETLINEINFOF_TARGETTYPE);
        CHECKF(mr == MMSYSERR_INVALPARAM, "mixer %u: an unknown target type is MMSYSERR_INVALPARAM (%u)", i, mr);
        mixerClose(mixer);
    }
    if (!tested) printf("note  no mixer with a matching wave device: TARGETTYPE not exercised\n");
}

static void test_midi_cache(void)
{
    WORD patches[128] = {0};
    HMIDIOUT out;
    UINT n = midiOutGetNumDevs();
    MMRESULT mr;

    mr = midiOutCachePatches((HMIDIOUT)0xdead0, 0, patches, MIDI_CACHE_ALL);
    CHECKF(mr == MMSYSERR_INVALHANDLE, "midiOutCachePatches on a bad handle is MMSYSERR_INVALHANDLE (%u)", mr);
    mr = midiOutCacheDrumPatches((HMIDIOUT)0xdead0, 0, patches, MIDI_CACHE_ALL);
    CHECKF(mr == MMSYSERR_INVALHANDLE, "midiOutCacheDrumPatches on a bad handle is MMSYSERR_INVALHANDLE (%u)", mr);
    if (!n || midiOutOpen(&out, 0, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        printf("note  no MIDI output device: the cache calls are only checked on a bad handle\n");
        return;
    }
    mr = midiOutCachePatches(out, 0, NULL, MIDI_CACHE_ALL);
    CHECKF(mr == MMSYSERR_INVALPARAM, "NULL patch array is MMSYSERR_INVALPARAM (%u)", mr);
    mr = midiOutCachePatches(out, 0, patches, 0);
    CHECKF(mr == MMSYSERR_INVALFLAG, "flags 0 is MMSYSERR_INVALFLAG (%u)", mr);
    mr = midiOutCachePatches(out, 1, patches, MIDI_CACHE_ALL);
    CHECKF(mr == MMSYSERR_INVALPARAM, "bank 1 is MMSYSERR_INVALPARAM (%u)", mr);
    mr = midiOutCachePatches(out, 0, patches, MIDI_CACHE_ALL);
    CHECKF(mr == MMSYSERR_NOTSUPPORTED, "a valid call is MMSYSERR_NOTSUPPORTED (%u)", mr);
    mr = midiOutCacheDrumPatches(out, 128, patches, MIDI_CACHE_ALL);
    CHECKF(mr == MMSYSERR_INVALPARAM, "drum patch 128 is MMSYSERR_INVALPARAM (%u)", mr);
    mr = midiOutCacheDrumPatches(out, 0, patches, MIDI_CACHE_ALL);
    CHECKF(mr == MMSYSERR_NOTSUPPORTED, "a valid drum call is MMSYSERR_NOTSUPPORTED (%u)", mr);
    midiOutClose(out);
}

static void test_mci_element_id(void)
{
    MCI_OPEN_PARMSW open = {0};
    MCIERROR err;
    UINT id;

    /* nothing was opened with an element id */
    id = mciGetDeviceIDFromElementIDW(7, L"waveaudio");
    CHECKF(id == 0, "no device for an element id nothing was opened with (%u)", id);

    open.lpstrDeviceType = L"waveaudio";
    open.lpstrElementName = (LPCWSTR)(ULONG_PTR)7;
    err = mciSendCommandW(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_OPEN_ELEMENT_ID, (DWORD_PTR)&open);
    if (err)
    {
        printf("note  waveaudio did not open with an element id (%lu): only the negative case checked\n", err);
        return;
    }
    id = mciGetDeviceIDFromElementIDW(7, L"waveaudio");
    CHECKF(id == open.wDeviceID, "the device opened with element id 7 is found (%u, want %u)", id, open.wDeviceID);
    id = mciGetDeviceIDFromElementIDW(7, L"WAVEAUDIO");
    CHECKF(id == open.wDeviceID, "the type is compared without case (%u)", id);
    id = mciGetDeviceIDFromElementIDW(8, L"waveaudio");
    CHECKF(id == 0, "another element id finds none (%u)", id);
    id = mciGetDeviceIDFromElementIDW(7, L"sequencer");
    CHECKF(id == 0, "another type finds none (%u)", id);
    mciSendCommandW(open.wDeviceID, MCI_CLOSE, 0, 0);
    id = mciGetDeviceIDFromElementIDW(7, L"waveaudio");
    CHECKF(id == 0, "a closed device is gone (%u)", id);
}

static void test_joy(void)
{
    CHECKF(joyConfigChanged(0) == JOYERR_NOERROR, "joyConfigChanged(0) is JOYERR_NOERROR");
    CHECKF(joyConfigChanged(1) == JOYERR_PARMS, "joyConfigChanged(1) is JOYERR_PARMS");
}

static void test_mmio_memory(void)
{
    MMIOINFO info = {0};
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, 16);
    char block[40];
    HMMIO hmmio;
    LONG n;
    char *p;
    int i, ok;

    p = GlobalLock(mem);
    memset(p, 0, 16);
    info.fccIOProc = FOURCC_MEM;
    info.pchBuffer = p;
    info.cchBuffer = 16;
    info.adwInfo[0] = 16;               /* grow by 16 bytes at a time */
    hmmio = mmioOpenA(NULL, &info, MMIO_CREATE | MMIO_READWRITE);
    check(hmmio != NULL, "mmioOpen on a memory file with an expansion size");
    if (!hmmio) return;

    for (i = 0; i < sizeof(block); i++) block[i] = 'A' + i % 26;
    n = mmioWrite(hmmio, block, sizeof(block));
    CHECKF(n == sizeof(block), "mmioWrite of 40 bytes into a 16 byte memory file grows it (%ld)", n);
    mmioGetInfo(hmmio, &info, 0);
    CHECKF(info.cchBuffer >= 40, "the buffer is now at least 40 bytes (%ld)", info.cchBuffer);
    ok = 1;
    for (i = 0; i < 40; i++) if (info.pchBuffer[i] != block[i]) ok = 0;
    check(ok, "the 40 bytes are in the (moved) buffer");
    mmioClose(hmmio, 0);
    GlobalFree(GlobalHandle(info.pchBuffer));

    /* without an expansion size a full memory file stops */
    {
        char fixed[16];
        MMIOINFO fi = {0};
        fi.fccIOProc = FOURCC_MEM;
        fi.pchBuffer = fixed;
        fi.cchBuffer = sizeof(fixed);
        hmmio = mmioOpenA(NULL, &fi, MMIO_CREATE | MMIO_READWRITE);
        n = mmioWrite(hmmio, block, sizeof(block));
        CHECKF(n == 16, "a fixed 16 byte memory file takes only 16 bytes (%ld)", n);
        mmioClose(hmmio, 0);
    }
}

int main(void)
{
    test_mixer_targettype();
    test_midi_cache();
    test_mci_element_id();
    test_joy();
    test_mmio_memory();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
