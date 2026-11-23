/* wmssml-probe: Windows.Media.SpeechSynthesis SSML prosody, say-as and sub (patches/sg/9805).
 *
 * SynthesizeSsmlToStreamAsync read SSML down to its text and breaks only: <prosody rate/volume/pitch>,
 * <say-as interpret-as="characters"> and <sub alias> were ignored. Compares the spoken durations and
 * loudness of the same words with and without them (eSpeak NG voice, msttsengine).
 *
 * Prints name=value lines; see test/wmssml-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Media
#define WIDL_using_Windows_Media_SpeechSynthesis
#define WIDL_using_Windows_Storage_Streams
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.media.speechsynthesis.h>
#include <windows.storage.streams.h>
#include <stdio.h>
#include <math.h>

/* IBufferByteAccess (robuffer.h is C++ only in mingw) */
DEFINE_GUID(IID_IBufferByteAccess_, 0x905a0fef, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
typedef struct byte_access byte_access;
struct byte_access_vtbl
{
    HRESULT (WINAPI *QueryInterface)( byte_access *, REFIID, void ** );
    ULONG (WINAPI *AddRef)( byte_access * );
    ULONG (WINAPI *Release)( byte_access * );
    HRESULT (WINAPI *Buffer)( byte_access *, BYTE ** );
};
struct byte_access { const struct byte_access_vtbl *lpVtbl; };
/* IInputStream and its IAsyncOperationWithProgress<IBuffer, UINT32> (mingw's header lacks them) */
DEFINE_GUID(IID_IInputStream_, 0x905a0fe2, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
typedef HRESULT (WINAPI *read_async_fn)( void *, IBuffer *, UINT32, int, void ** );
typedef HRESULT (WINAPI *get_results_fn)( void *, IBuffer ** );
typedef ULONG (WINAPI *release_fn)( void * );
typedef HRESULT (WINAPI *get_type_fn)( void *, HSTRING * );
DEFINE_GUID(IID_IContentTypeProvider_, 0x97d098a5, 0x3b99, 0x4de9, 0x88, 0xa5, 0xe1, 0x1d, 0x2f, 0x50, 0xc7, 0x95);

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, wcslen( s ), &h );
    return h;
}

static HRESULT wait_async( IInspectable *op )
{
    IAsyncInfo *info;
    AsyncStatus status = Started;
    HRESULT hr = E_FAIL;
    int i;

    if (FAILED(IInspectable_QueryInterface( op, &IID_IAsyncInfo, (void **)&info ))) return E_NOINTERFACE;
    for (i = 0; i < 3000; i++)
    {
        IAsyncInfo_get_Status( info, &status );
        if (status != Started) break;
        Sleep( 10 );
    }
    if (status == Completed) hr = S_OK;
    else if (status == Error) IAsyncInfo_get_ErrorCode( info, &hr );
    else hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    IAsyncInfo_Release( info );
    return hr;
}

/* the stream's bytes, read as a program reads them */
static BYTE *read_stream( ISpeechSynthesisStream *stream, UINT64 *size_out )
{
    IActivationFactory *factory;
    IBufferFactory *buffer_factory;
    IRandomAccessStream *ras;
    void *input = NULL, *op = NULL;
    IBuffer *buffer, *result;
    byte_access *access;
    BYTE *bytes, *copy = NULL;
    UINT64 size = 0;
    UINT32 length = 0;
    HSTRING name;
    HRESULT hr;

    *size_out = 0;
    if (FAILED(ISpeechSynthesisStream_QueryInterface( stream, &IID_IRandomAccessStream, (void **)&ras ))) return NULL;
    IRandomAccessStream_get_Size( ras, &size );
    hr = IRandomAccessStream_QueryInterface( ras, &IID_IInputStream_, &input );
    IRandomAccessStream_Release( ras );
    if (FAILED(hr) || !size || size > 64 * 1024 * 1024) return NULL;

    name = hs( L"Windows.Storage.Streams.Buffer" );
    hr = RoGetActivationFactory( name, &IID_IActivationFactory, (void **)&factory );
    WindowsDeleteString( name );
    if (SUCCEEDED(hr))
    {
        hr = IActivationFactory_QueryInterface( factory, &IID_IBufferFactory, (void **)&buffer_factory );
        IActivationFactory_Release( factory );
    }
    if (SUCCEEDED(hr))
    {
        hr = IBufferFactory_Create( buffer_factory, (UINT32)size, &buffer );
        IBufferFactory_Release( buffer_factory );
    }
    if (FAILED(hr)) { SLOT( input, 2, release_fn )( input ); return NULL; }

    hr = SLOT( input, 6, read_async_fn )( input, buffer, (UINT32)size, 0 /* InputStreamOptions_None */, &op );
    SLOT( input, 2, release_fn )( input );
    if (SUCCEEDED(hr) && SUCCEEDED(hr = wait_async( (IInspectable *)op )) &&
        SUCCEEDED(hr = SLOT( op, 10, get_results_fn )( op, &result )))
    {
        IBuffer_get_Length( result, &length );
        if (SUCCEEDED(IBuffer_QueryInterface( result, &IID_IBufferByteAccess_, (void **)&access )))
        {
            if (SUCCEEDED(access->lpVtbl->Buffer( access, &bytes )) && length && (copy = malloc( length )))
            {
                memcpy( copy, bytes, length );
                *size_out = length;
            }
            access->lpVtbl->Release( access );
        }
        IBuffer_Release( result );
    }
    if (op) SLOT( op, 2, release_fn )( op );
    IBuffer_Release( buffer );
    return copy;
}

struct spoken
{
    HRESULT hr;
    BYTE *wav;
    UINT64 size;
    double seconds, rms;
    UINT32 words, sentences;
    WCHAR first_word[32], first_sentence[64];
    INT64 last_time, first_time;
    BOOL times_increase;
    BOOL wav_ok;
};

static void speak( ISpeechSynthesizer *synth, const WCHAR *text, BOOL ssml, struct spoken *out )
{
    IAsyncOperation_SpeechSynthesisStream *op = NULL;
    ISpeechSynthesisStream *stream = NULL;
    IVectorView_IMediaMarker *markers;
    HSTRING str = hs( text );
    UINT32 count = 0, i;

    memset( out, 0, sizeof(*out) );
    out->times_increase = TRUE;
    out->first_time = -1;
    out->hr = ssml ? ISpeechSynthesizer_SynthesizeSsmlToStreamAsync( synth, str, &op )
                   : ISpeechSynthesizer_SynthesizeTextToStreamAsync( synth, str, &op );
    WindowsDeleteString( str );
    if (FAILED(out->hr)) return;
    out->hr = wait_async( (IInspectable *)op );
    if (SUCCEEDED(out->hr)) out->hr = IAsyncOperation_SpeechSynthesisStream_GetResults( op, &stream );
    IAsyncOperation_SpeechSynthesisStream_Release( op );
    if (FAILED(out->hr) || !stream) return;

    out->wav = read_stream( stream, &out->size );
    if (out->wav && out->size >= 44 && !memcmp( out->wav, "RIFF", 4 ) && !memcmp( out->wav + 8, "WAVEfmt ", 8 ) &&
        !memcmp( out->wav + 36, "data", 4 ))
    {
        WORD format = *(WORD *)(out->wav + 20), channels = *(WORD *)(out->wav + 22), bits = *(WORD *)(out->wav + 34);
        DWORD rate = *(DWORD *)(out->wav + 24), data = *(DWORD *)(out->wav + 40);
        double sum = 0;
        UINT64 n;

        out->wav_ok = format == 1 && channels >= 1 && bits == 16 && rate >= 8000 && data == out->size - 44;
        if (out->wav_ok && data)
        {
            out->seconds = (double)data / (rate * channels * 2);
            for (n = 0; n < data / 2; n++) sum += (double)((INT16 *)(out->wav + 44))[n] * ((INT16 *)(out->wav + 44))[n];
            out->rms = sqrt( sum / (data / 2) );
        }
    }

    if (SUCCEEDED(ISpeechSynthesisStream_get_Markers( stream, &markers )))
    {
        IVectorView_IMediaMarker_get_Size( markers, &count );
        for (i = 0; i < count; i++)
        {
            IMediaMarker *marker;
            HSTRING type, mtext;
            TimeSpan time;

            if (FAILED(IVectorView_IMediaMarker_GetAt( markers, i, &marker ))) continue;
            IMediaMarker_get_MediaMarkerType( marker, &type );
            IMediaMarker_get_Text( marker, &mtext );
            IMediaMarker_get_Time( marker, &time );
            if (!wcscmp( WindowsGetStringRawBuffer( type, NULL ), L"SpeechWord" ))
            {
                if (!out->words++)
                {
                    lstrcpynW( out->first_word, WindowsGetStringRawBuffer( mtext, NULL ), 32 );
                    out->first_time = time.Duration;
                }
                else if (time.Duration < out->last_time) out->times_increase = FALSE;
                out->last_time = time.Duration;
            }
            else if (!wcscmp( WindowsGetStringRawBuffer( type, NULL ), L"SpeechSentence" ))
            {
                if (!out->sentences++) lstrcpynW( out->first_sentence, WindowsGetStringRawBuffer( mtext, NULL ), 64 );
            }
            WindowsDeleteString( type );
            WindowsDeleteString( mtext );
            IMediaMarker_Release( marker );
        }
        IVectorView_IMediaMarker_Release( markers );
    }
    ISpeechSynthesisStream_Release( stream );
}

#define SPEAK_OPEN L"<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>"
#define WORDS L"The quick brown fox jumps over the lazy dog near the river bank."

static void ssml( ISpeechSynthesizer *synth, const WCHAR *body, struct spoken *out )
{
    WCHAR text[1024];
    swprintf( text, ARRAYSIZE(text), L"%ls%ls</speak>", SPEAK_OPEN, body );
    speak( synth, text, TRUE, out );
}

int main( void )
{
    IInspectable *inspectable;
    ISpeechSynthesizer *synth;
    struct spoken plain, slow, fast, pct, quiet, silent, high, word, spelled, alias, sub, nested;
    HSTRING name;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    name = hs( L"Windows.Media.SpeechSynthesis.SpeechSynthesizer" );
    hr = RoActivateInstance( name, &inspectable );
    WindowsDeleteString( name );
    if (FAILED(hr)) { printf( "activate=0 hr=%#lx\n", hr ); return 1; }
    IInspectable_QueryInterface( inspectable, &IID_ISpeechSynthesizer, (void **)&synth );
    IInspectable_Release( inspectable );

    ssml( synth, WORDS, &plain );
    printf( "plain=%d seconds=%.2f rms=%.0f\n", SUCCEEDED(plain.hr) && plain.wav_ok && plain.seconds > 1.5 && plain.rms > 200,
            plain.seconds, plain.rms );
    ssml( synth, L"<prosody rate='x-slow'>" WORDS L"</prosody>", &slow );
    printf( "rate_x_slow=%d seconds=%.2f\n", SUCCEEDED(slow.hr) && slow.seconds > plain.seconds * 1.4, slow.seconds );
    ssml( synth, L"<prosody rate='x-fast'>" WORDS L"</prosody>", &fast );
    printf( "rate_x_fast=%d seconds=%.2f\n", SUCCEEDED(fast.hr) && fast.seconds < plain.seconds * 0.75, fast.seconds );
    ssml( synth, L"<prosody rate=\"+100%\">" WORDS L"</prosody>", &pct );
    printf( "rate_percent=%d seconds=%.2f\n", SUCCEEDED(pct.hr) && pct.seconds < plain.seconds * 0.75, pct.seconds );
    ssml( synth, L"<prosody volume='x-soft'>" WORDS L"</prosody>", &quiet );
    printf( "volume_x_soft=%d rms=%.0f\n", SUCCEEDED(quiet.hr) && quiet.rms < plain.rms * 0.5 && quiet.rms > 10, quiet.rms );
    ssml( synth, L"<prosody volume='silent'>" WORDS L"</prosody>", &silent );
    printf( "volume_silent=%d rms=%.0f seconds=%.2f\n", SUCCEEDED(silent.hr) && silent.rms < 20 && silent.seconds > 1.0,
            silent.rms, silent.seconds );
    ssml( synth, L"<prosody pitch='x-high'>" WORDS L"</prosody>", &high );
    printf( "pitch_x_high=%d seconds=%.2f rms=%.0f\n", SUCCEEDED(high.hr) && high.rms > 200, high.seconds, high.rms );
    ssml( synth, L"<prosody rate='x-slow'>slow <prosody rate='x-fast'>fast</prosody> again</prosody>", &nested );
    printf( "nested=%d seconds=%.2f\n", SUCCEEDED(nested.hr) && nested.rms > 200, nested.seconds );

    ssml( synth, L"internationalization", &word );
    ssml( synth, L"<say-as interpret-as='characters'>internationalization</say-as>", &spelled );
    printf( "say_as_characters=%d word=%.2f spelled=%.2f\n", SUCCEEDED(spelled.hr) && spelled.seconds > word.seconds * 2,
            word.seconds, spelled.seconds );
    ssml( synth, L"World Wide Web Consortium", &alias );
    ssml( synth, L"<sub alias='World Wide Web Consortium'>W3C</sub>", &sub );
    printf( "sub_alias=%d alias=%.2f sub=%.2f\n", SUCCEEDED(sub.hr) && fabs( sub.seconds - alias.seconds ) < 0.3 && sub.rms > 200,
            alias.seconds, sub.seconds );
    printf( "done=1\n" );
    return 0;
}
