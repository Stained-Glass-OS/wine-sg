/* ttsvoice-probe: the SAPI voice speaks (Stained Glass OS wine-sg 6508): SpVoice through the
 * default voice token and msttsengine, into WAV files: speech at the file's own format, word and
 * sentence boundary events where the words are, a bookmark, a faster rate in less time. Prints
 * name=value lines; see test/ttsvoice-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <sapi.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(probe_SPDFID_WaveFormatEx, 0xc31adbae, 0x527f, 0x4ff5, 0xa2, 0x30, 0xf6, 0x2b, 0xb6, 0x1f, 0xf7, 0x0c);

/* the PCM data of a WAV file: sample count, RMS */
static void wav_stats( const WCHAR *path, DWORD *rate, DWORD *samples, double *rms )
{
    HANDLE f = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    DWORD size, got, i;
    BYTE *buf;
    double sum = 0;

    *rate = *samples = 0; *rms = 0;
    if (f == INVALID_HANDLE_VALUE) return;
    size = GetFileSize( f, NULL );
    buf = malloc( size );
    ReadFile( f, buf, size, &got, NULL );
    CloseHandle( f );
    if (got < 44) { free( buf ); return; }
    *rate = *(DWORD *)(buf + 24);
    for (i = 12; i + 8 <= got; i += 8 + *(DWORD *)(buf + i + 4))
        if (!memcmp( buf + i, "data", 4 ))
        {
            DWORD len = min( *(DWORD *)(buf + i + 4), got - i - 8 ), k;
            short *s = (short *)(buf + i + 8);
            *samples = len / 2;
            for (k = 0; k < *samples; k++) sum += (double)s[k] * s[k];
            *rms = *samples ? sqrt( sum / *samples ) : 0;
            break;
        }
    free( buf );
}

static ISpVoice *voice_to_file( const WCHAR *path, ISpStream **stream )
{
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0 };
    ISpVoice *voice = NULL;

    if (FAILED(CoCreateInstance( &CLSID_SpVoice, NULL, CLSCTX_ALL, &IID_ISpVoice, (void **)&voice ))) return NULL;
    if (FAILED(CoCreateInstance( &CLSID_SpStream, NULL, CLSCTX_ALL, &IID_ISpStream, (void **)stream ))) return NULL;
    if (FAILED(ISpStream_BindToFile( *stream, path, SPFM_CREATE_ALWAYS, &probe_SPDFID_WaveFormatEx, &wfx, 0 ))) return NULL;
    ISpVoice_SetOutput( voice, (IUnknown *)*stream, TRUE );
    return voice;
}

int main( void )
{
    static const WCHAR text[] = L"Hello world. Second sentence here.";
    ISpVoice *voice;
    ISpStream *stream;
    SPEVENT events[64];
    ULONG fetched = 0, i, words = 0, sentences = 0;
    DWORD rate, samples, fast_samples;
    double rms;
    HRESULT hr;
    int first_ok = 0, second_ok = 0, bookmark = 0;

    CoInitialize( NULL );
    if (!(voice = voice_to_file( L"C:\\normal.wav", &stream ))) { printf( "no_voice=1\n" ); return 1; }
    ISpVoice_SetInterest( voice, SPFEI(SPEI_WORD_BOUNDARY) | SPFEI(SPEI_SENTENCE_BOUNDARY),
                          SPFEI(SPEI_WORD_BOUNDARY) | SPFEI(SPEI_SENTENCE_BOUNDARY) );
    hr = ISpVoice_Speak( voice, text, SPF_DEFAULT, NULL );
    printf( "speak=%d hr %#lx\n", hr == S_OK, hr );
    ISpVoice_GetEvents( voice, ARRAYSIZE(events), events, &fetched );
    for (i = 0; i < fetched; i++)
    {
        if (events[i].eEventId == SPEI_WORD_BOUNDARY)
        {
            if (!words && events[i].lParam == 0 && events[i].wParam == 5) first_ok = 1;
            if (words == 2 && events[i].lParam == 13 && events[i].wParam == 6) second_ok = 1;
            words++;
        }
        else if (events[i].eEventId == SPEI_SENTENCE_BOUNDARY) sentences++;
    }
    ISpStream_Close( stream );
    ISpStream_Release( stream );
    ISpVoice_Release( voice );
    wav_stats( L"C:\\normal.wav", &rate, &samples, &rms );
    printf( "audio=%d rate %lu samples %lu rms %.0f\n", rate == 16000 && samples > 8000 && rms > 300, rate, samples, rms );
    printf( "words=%d %lu\n", words == 5, words );
    printf( "word_positions=%d first %d third %d\n", first_ok && second_ok, first_ok, second_ok );
    printf( "sentences=%d %lu\n", sentences == 2, sentences );

    /* a bookmark in markup */
    voice = voice_to_file( L"C:\\mark.wav", &stream );
    ISpVoice_SetInterest( voice, SPFEI(SPEI_TTS_BOOKMARK), SPFEI(SPEI_TTS_BOOKMARK) );
    hr = ISpVoice_Speak( voice, L"Before <bookmark mark=\"42\"/> after.", SPF_IS_XML, NULL );
    fetched = 0;
    ISpVoice_GetEvents( voice, ARRAYSIZE(events), events, &fetched );
    for (i = 0; i < fetched; i++)
        if (events[i].eEventId == SPEI_TTS_BOOKMARK && events[i].wParam == 42 && events[i].ullAudioStreamOffset > 0) bookmark = 1;
    printf( "bookmark=%d hr %#lx events %lu\n", bookmark, hr, fetched );
    ISpStream_Close( stream );
    ISpStream_Release( stream );
    ISpVoice_Release( voice );

    /* the fastest rate: in less time */
    voice = voice_to_file( L"C:\\fast.wav", &stream );
    ISpVoice_SetRate( voice, 10 );
    ISpVoice_Speak( voice, text, SPF_DEFAULT, NULL );
    ISpStream_Close( stream );
    ISpStream_Release( stream );
    ISpVoice_Release( voice );
    wav_stats( L"C:\\fast.wav", &rate, &fast_samples, &rms );
    printf( "rate_fast=%d %lu vs %lu\n", fast_samples && fast_samples < samples * 6 / 10, fast_samples, samples );
    printf( "done=1\n" );
    CoUninitialize();
    return 0;
}
