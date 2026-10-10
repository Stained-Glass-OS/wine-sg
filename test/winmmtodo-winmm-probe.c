/* winmm todo_wine groups (patches 2802-2803), run by test/winmmtodo-winmm-gate.sh:
 * mmioOpen MMIO_PARSE/MMIO_EXIST results, the mciSendString parser (duplicate and
 * incompatible flags, constants, bad words, blanks in keywords and between verb
 * and device), "all" devices, the auto-open notify rule, mixerOpen with an event
 * or thread callback, midiStream SMPTE positions and the stream buffer limit.
 * Parts that need a device are skipped (said on the line).
 *
 *   winmmtodo-winmm-probe.exe */
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
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static void test_mmio_parse(void)
{
    char name[MAX_PATH], full[MAX_PATH], longname[300], orig[300];
    WCHAR wname[MAX_PATH], wlong[300];
    MMIOINFO info;
    HMMIO h;
    char exedir[MAX_PATH];
    HANDLE f;

    GetCurrentDirectoryA(sizeof(exedir), exedir);
    f = CreateFileA("mmiopath.tmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(f);

    memset(&info, 0, sizeof(info));
    strcpy(name, "mmiopath.tmp");
    h = mmioOpenA(name, &info, MMIO_PARSE);
    GetFullPathNameA("mmiopath.tmp", sizeof(full), full, NULL);
    CHECKF(h == (HMMIO)TRUE && !strcmp(name, full), "PARSE returns TRUE and the full path (%s)", name);

    MultiByteToWideChar(CP_ACP, 0, "mmiopath.tmp", -1, wname, MAX_PATH);
    h = mmioOpenW(wname, &info, MMIO_PARSE);
    MultiByteToWideChar(CP_ACP, 0, full, -1, wlong, 300);
    CHECKF(h == (HMMIO)TRUE && !wcscmp(wname, wlong), "wide PARSE writes the full path back");

    strcpy(name, "nosuchmmiofile.tmp");
    info.wErrorRet = 0xdead;
    h = mmioOpenA(name, &info, MMIO_EXIST);
    CHECKF(h == NULL && info.wErrorRet == MMIOERR_FILENOTFOUND, "EXIST of a missing file: FILENOTFOUND (%#x)", info.wErrorRet);

    strcpy(name, "mmiopath.tmp");
    h = mmioOpenA(name, &info, MMIO_EXIST);
    CHECKF(h == (HMMIO)TRUE, "EXIST of an existing file");

    memset(longname, 'x', 200); longname[200] = 0;
    strcpy(orig, longname);
    strcpy(longname + 0, orig);
    info.wErrorRet = 0xdead;
    h = mmioOpenA(longname, &info, MMIO_PARSE);
    CHECKF(h == NULL && info.wErrorRet == MMIOERR_OUTOFMEMORY && !strcmp(longname, orig),
           "long name, ansi: OUTOFMEMORY and the name untouched (%#x)", info.wErrorRet);

    MultiByteToWideChar(CP_ACP, 0, orig, -1, wlong, 300);
    info.wErrorRet = 0xdead;
    h = mmioOpenW(wlong, &info, MMIO_PARSE);
    CHECKF(h == NULL && info.wErrorRet == MMIOERR_OUTOFMEMORY && wcslen(wlong) == 127,
           "long name, wide: OUTOFMEMORY and cut to 127 characters (%u)", (unsigned)wcslen(wlong));
    DeleteFileA("mmiopath.tmp");
}

static void test_mixer_callbacks(void)
{
    UINT n = mixerGetNumDevs(), d;
    HANDLE ev;

    if (!n) { printf("note  no mixer device: callback part skipped\n"); return; }
    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    for (d = 0; d < n; d++)
    {
        HMIXER m;
        MMRESULT mr = mixerOpen(&m, d, (DWORD_PTR)ev, 0, CALLBACK_EVENT);
        CHECKF(mr == MMSYSERR_NOERROR, "mixer %u: open with CALLBACK_EVENT (%u)", d, mr);
        if (!mr) mixerClose(m);
        mr = mixerOpen(&m, d, GetCurrentThreadId(), 0, CALLBACK_THREAD);
        CHECKF(mr == MMSYSERR_NOERROR, "mixer %u: open with CALLBACK_THREAD (%u)", d, mr);
        if (!mr) mixerClose(m);
        CHECKF(WaitForSingleObject(ev, 0) == WAIT_TIMEOUT, "mixer %u: the event stays unsignalled", d);
        mr = mixerOpen(&m, d, 0, 0, CALLBACK_FUNCTION);
        CHECKF(mr == MMSYSERR_INVALFLAG, "mixer %u: CALLBACK_FUNCTION is an invalid flag (%u)", d, mr);
    }
    CloseHandle(ev);
}

static MCIERROR mci(const char *cmd, char *buf, size_t n)
{
    if (buf) buf[0] = 0;
    return mciSendStringA(cmd, buf, buf ? (UINT)n : 0, NULL);
}

static void test_mci(void)
{
    static const struct { const char *cmd; MCIERROR err; } t[] = {
        { "open notify", MCIERR_INVALID_DEVICE_NAME },
        { "open new", MCIERR_NEW_REQUIRES_ALIAS },
        { "open all", MCIERR_CANNOT_USE_ALL },
        { "open new type waveaudio alias r shareable shareable", MCIERR_DUPLICATE_FLAGS },
        { "status x position wait wait", MCIERR_DUPLICATE_FLAGS },
        { "status x length length", MCIERR_FLAGS_NOT_COMPATIBLE },
        { "status x length position", MCIERR_FLAGS_NOT_COMPATIBLE },
        { "set x time format milliseconds time format ms", MCIERR_FLAGS_NOT_COMPATIBLE },
        { "status x 0x4", MCIERR_BAD_CONSTANT },
        { "status x nsa", MCIERR_BAD_CONSTANT },
        { "set x milliseconds", MCIERR_UNRECOGNIZED_KEYWORD },
        { "set x milliseconds ms", MCIERR_UNRECOGNIZED_KEYWORD },
        { "status x 4 notify", 0 },
        { "capability x can   save", 0 },
        { "pause  x wait", MCIERR_NONAPPLICABLE_FUNCTION },
        { "play no-such-file-exists.wav notify", MCIERR_NOTIFY_ON_AUTO_OPEN },
        { "cue all", MCIERR_UNRECOGNIZED_COMMAND },
        { "play all", MCIERR_CANNOT_USE_ALL },
        { "status all time format", MCIERR_CANNOT_USE_ALL },
    };
    char buf[128];
    unsigned i;
    MCIERROR err;

    err = mci("open new type waveaudio alias x", buf, sizeof(buf));
    if (err) { printf("note  waveaudio does not open (%lu): the mci part is skipped\n", err); return; }
    err = mci("open avivideo alias a", buf, sizeof(buf));
    CHECKF(!err, "open avivideo (%lu)", err);

    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        err = mciSendStringA(t[i].cmd, buf, sizeof(buf), NULL);
        CHECKF(err == t[i].err, "mci \"%s\" -> %lu (want %lu)", t[i].cmd, err, (unsigned long)t[i].err);
        if (!err && !strncmp(t[i].cmd, "open", 4)) mci("close r", NULL, 0);
    }
    err = mci("status x 4", buf, sizeof(buf));
    CHECKF(!err && !strcmp(buf, "stopped"), "status x 4 still returns the mode (%lu, %s)", err, buf);

    /* the devices answer differently (waveaudio: not playing, avivideo: nothing open) */
    err = mci("pause all", buf, sizeof(buf));
    CHECKF(err == MCIERR_MULTIPLE && !buf[0], "pause all with differing answers: MULTIPLE (%lu)", err);
    err = mci("close all", NULL, 0);
    CHECKF(!err, "close all (%lu)", err);
    err = mci("pause all", buf, sizeof(buf));
    CHECKF(!err, "pause all with no device open (%lu)", err);
}

static void test_midi_stream(void)
{
    HMIDISTRM hm;
    UINT dev = 0;
    MMRESULT rc;
    MMTIME t;
    MIDIPROPTIMEDIV td;
    MIDIHDR hdr;

    if (!midiOutGetNumDevs()) { printf("note  no MIDI output: the stream part is skipped\n"); return; }
    rc = midiStreamOpen(&hm, &dev, 1, 0, 0, CALLBACK_NULL);
    if (rc) { printf("note  midiStreamOpen failed (%u): the stream part is skipped\n", rc); return; }

    t.wType = TIME_SMPTE;
    rc = midiStreamPosition(hm, &t, sizeof(t));
    CHECKF(!rc && t.wType == TIME_MS, "SMPTE position of a ticks-per-beat stream converts to ms (%u, %#x)", rc, t.wType);

    td.cbStruct = sizeof(td);
    td.dwTimeDiv = 0xe204;
    rc = midiStreamProperty(hm, (LPBYTE)&td, MIDIPROP_SET | MIDIPROP_TIMEDIV);
    t.wType = TIME_SMPTE;
    rc = midiStreamPosition(hm, &t, sizeof(t));
    CHECKF(!rc && t.wType == TIME_SMPTE && t.u.smpte.fps == 30, "SMPTE position of a SMPTE-division stream (%u, %#x, fps %u)", rc, t.wType, t.u.smpte.fps);

    memset(&hdr, 0, sizeof(hdr));
    hdr.dwBufferLength = 70000;
    hdr.lpData = HeapAlloc(GetProcessHeap(), 0, hdr.dwBufferLength);
    rc = midiOutPrepareHeader((HMIDIOUT)hm, &hdr, sizeof(hdr));
    CHECKF(rc == MMSYSERR_INVALPARAM, "a stream buffer over 64KB is refused (%u)", rc);
    HeapFree(GetProcessHeap(), 0, hdr.lpData);
    midiStreamClose(hm);
}

int main(void)
{
    test_mmio_parse();
    test_mixer_callbacks();
    test_mci();
    test_midi_stream();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
