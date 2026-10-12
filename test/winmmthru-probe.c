/* midiConnect / midiDisconnect (patch 2976), run by test/winmmthru-gate.sh.
 * Table of argument checks and the connect/disconnect state machine; the data
 * flow input -> connected outputs needs a MIDI input device (skipped if none). */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

static int failures;
#define CHECKF(ok, ...) do { char b[300]; snprintf(b, sizeof(b), __VA_ARGS__); \
    printf("%s  %s\n", (ok) ? "PASS" : "FAIL", b); fflush(stdout); if (!(ok)) failures++; } while (0)

static int in_seen;
static void CALLBACK in_cb(HMIDIIN h, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2)
{
    if (msg == MIM_OPEN) in_seen++;
}

int main(void)
{
    HMIDIOUT a = NULL, b = NULL;
    HMIDIIN in = NULL;
    MMRESULT rc;
    UINT nout = midiOutGetNumDevs(), nin = midiInGetNumDevs();
    int have_two = 0;
    UINT i;

    /* handles that are not open devices */
    rc = midiConnect((HMIDI)0, (HMIDIOUT)0, NULL);
    CHECKF(rc == MMSYSERR_INVALHANDLE, "connect(NULL,NULL) = %u (INVALHANDLE)", rc);
    rc = midiDisconnect((HMIDI)0, (HMIDIOUT)0, NULL);
    CHECKF(rc == MMSYSERR_INVALHANDLE, "disconnect(NULL,NULL) = %u (INVALHANDLE)", rc);
    rc = midiConnect((HMIDI)0x1234, (HMIDIOUT)0x5678, NULL);
    CHECKF(rc == MMSYSERR_INVALHANDLE, "connect(bogus,bogus) = %u (INVALHANDLE)", rc);
    rc = midiConnect((HMIDI)0x1234, (HMIDIOUT)0x5678, (void *)1);
    CHECKF(rc == MMSYSERR_INVALPARAM, "connect with reserved != NULL = %u (INVALPARAM)", rc);

    {   /* mci OPEN with both an element name and an element id (patch 2976) */
        MCI_OPEN_PARMSA op = {0};
        op.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_WAVEFORM_AUDIO;
        op.lpstrElementName = (LPCSTR)0xDEADBEEF;
        rc = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_ELEMENT | MCI_OPEN_ELEMENT_ID | MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID,
                             (DWORD_PTR)&op);
        CHECKF(rc == MCIERR_FLAGS_NOT_COMPATIBLE, "mci OPEN element + element id = %u (FLAGS_NOT_COMPATIBLE %u)",
               rc, MCIERR_FLAGS_NOT_COMPATIBLE);
        if (!rc) mciSendCommandA(op.wDeviceID, MCI_CLOSE, 0, 0);
    }
    if (!nout) { printf("note  no MIDI output device: handle-based parts skipped\n"); goto done; }
    rc = midiOutOpen(&a, 0, 0, 0, CALLBACK_NULL);
    if (rc) { printf("note  midiOutOpen failed (%u): skipped\n", rc); goto done; }
    /* a second output: another device, the mapper, or the same one if it can be shared */
    for (rc = 1, i = 1; rc && i <= nout; i++)
        rc = midiOutOpen(&b, i < nout ? i : (UINT)MIDI_MAPPER, 0, 0, CALLBACK_NULL);
    if (rc) rc = midiOutOpen(&b, 0, 0, 0, CALLBACK_NULL);
    have_two = !rc;
    printf("note  %u outputs, %u inputs, second output %s\n", nout, nin, rc ? "unavailable" : "open");

    rc = midiConnect((HMIDI)a, (HMIDIOUT)0x5678, NULL);
    CHECKF(rc == MMSYSERR_INVALHANDLE, "connect(out, bogus out) = %u (INVALHANDLE)", rc);
    rc = midiConnect((HMIDI)0x1234, a, NULL);
    CHECKF(rc == MMSYSERR_INVALHANDLE, "connect(bogus in, out) = %u (INVALHANDLE)", rc);
    rc = midiConnect((HMIDI)a, a, NULL);
    CHECKF(rc == MMSYSERR_INVALPARAM, "connect(out, same out) = %u (INVALPARAM)", rc);
    rc = midiConnect((HMIDI)a, a, (void *)1);
    CHECKF(rc == MMSYSERR_INVALPARAM, "connect reserved != NULL with valid handles = %u (INVALPARAM)", rc);

    if (have_two)
    {
        rc = midiDisconnect((HMIDI)a, b, NULL);
        CHECKF(rc == MMSYSERR_INVALPARAM, "disconnect when not connected = %u (INVALPARAM)", rc);
        rc = midiConnect((HMIDI)a, b, NULL);
        CHECKF(rc == MMSYSERR_NOERROR, "connect(thru a, b) = %u", rc);
        rc = midiConnect((HMIDI)a, b, NULL);
        CHECKF(rc == MMSYSERR_NOERROR, "connect again = %u (idempotent)", rc);
        rc = midiOutShortMsg(a, 0x00403C90);
        CHECKF(rc == MMSYSERR_NOERROR, "short message through the thru device = %u", rc);
        rc = midiDisconnect((HMIDI)a, b, NULL);
        CHECKF(rc == MMSYSERR_NOERROR, "disconnect = %u", rc);
        rc = midiDisconnect((HMIDI)a, b, NULL);
        CHECKF(rc == MMSYSERR_INVALPARAM, "disconnect twice = %u (INVALPARAM)", rc);
        /* closing a device drops its connections */
        rc = midiConnect((HMIDI)a, b, NULL);
        CHECKF(rc == MMSYSERR_NOERROR, "reconnect = %u", rc);
        midiOutClose(b);
        rc = midiDisconnect((HMIDI)a, b, NULL);
        CHECKF(rc == MMSYSERR_INVALHANDLE, "disconnect to a closed output = %u (INVALHANDLE)", rc);
        /* the next open may reuse the handle value: the old connection must be gone */
        b = NULL;
        for (rc = 1, i = 1; rc && i <= nout; i++)
            rc = midiOutOpen(&b, i < nout ? i : (UINT)MIDI_MAPPER, 0, 0, CALLBACK_NULL);
        if (!rc)
        {
            rc = midiDisconnect((HMIDI)a, b, NULL);
            CHECKF(rc == MMSYSERR_INVALPARAM, "connection to a closed output is dropped (%u, INVALPARAM)", rc);
        }
        else b = NULL;
    }
    else printf("note  second output open failed: connect state machine skipped\n");

    for (rc = 1, i = 0; rc && i < nin; i++)
        rc = midiInOpen(&in, i, (DWORD_PTR)in_cb, 0, CALLBACK_FUNCTION);
    if (!rc)
    {
        CHECKF(in_seen == 1, "the caller's callback still gets MIM_OPEN through the thru hook (%d)", in_seen);
        rc = midiConnect((HMIDI)in, a, NULL);
        CHECKF(rc == MMSYSERR_NOERROR, "connect(input, out) = %u", rc);
        rc = midiDisconnect((HMIDI)in, a, NULL);
        CHECKF(rc == MMSYSERR_NOERROR, "disconnect(input, out) = %u", rc);
        midiInClose(in);
    }
    else printf("note  no MIDI input: input thru skipped\n");
    midiOutClose(a);
    if (b) midiOutClose(b);
done:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
