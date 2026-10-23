/* wmsynth-probe: Windows.Media.SpeechSynthesis speaks (patches/sg/6510).
 *
 * The synthesizer had no voices and its streams were empty objects, so a program that said
 * something (SpeechSynthesizer.SynthesizeTextToStreamAsync, then MediaElement/MediaPlayer) got
 * silence or an error. Checks the installed voices, a spoken stream that is a WAV file read
 * through IInputStream into a Windows.Storage.Streams.Buffer, its content type, word and sentence
 * markers, SSML with a break, the speaking rate, and SSML that is not SSML.
 *
 * Prints name=value lines; see test/wmsynth-gate.sh.
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

int main( void )
{
    IInspectable *inspectable;
    IInstalledVoicesStatic *voices_static;
    IVectorView_VoiceInformation *voices;
    IVoiceInformation *voice;
    ISpeechSynthesizer *synth;
    ISpeechSynthesizer2 *synth2;
    ISpeechSynthesizerOptions *options;
    ISpeechSynthesizerOptions2 *options2;
    void *ctp;
    IAsyncOperation_SpeechSynthesisStream *op;
    ISpeechSynthesisStream *stream;
    struct spoken plain, ssml, brk, fast, bad, words;
    HSTRING name, str;
    UINT32 count = 0;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    name = hs( L"Windows.Media.SpeechSynthesis.SpeechSynthesizer" );
    hr = RoGetActivationFactory( name, &IID_IInstalledVoicesStatic, (void **)&voices_static );
    printf( "factory=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;

    /* the installed voices: SAPI's */
    hr = IInstalledVoicesStatic_get_AllVoices( voices_static, &voices );
    if (SUCCEEDED(hr)) IVectorView_VoiceInformation_get_Size( voices, &count );
    printf( "voices=%d count=%u\n", SUCCEEDED(hr) && count >= 1, count );
    if (SUCCEEDED(hr)) IVectorView_VoiceInformation_Release( voices );
    hr = IInstalledVoicesStatic_get_DefaultVoice( voices_static, &voice );
    if (SUCCEEDED(hr))
    {
        HSTRING display, lang, id;
        IVoiceInformation_get_DisplayName( voice, &display );
        IVoiceInformation_get_Language( voice, &lang );
        IVoiceInformation_get_Id( voice, &id );
        printf( "default_voice=%d name=%ls lang=%ls id=%ls\n",
                WindowsGetStringLen( display ) > 0 && WindowsGetStringLen( lang ) >= 2 && WindowsGetStringLen( id ) > 0,
                WindowsGetStringRawBuffer( display, NULL ), WindowsGetStringRawBuffer( lang, NULL ),
                WindowsGetStringRawBuffer( id, NULL ) );
        WindowsDeleteString( display );
        WindowsDeleteString( lang );
        WindowsDeleteString( id );
        IVoiceInformation_Release( voice );
    }
    else printf( "default_voice=0 hr=%#lx\n", hr );
    IInstalledVoicesStatic_Release( voices_static );

    hr = RoActivateInstance( name, &inspectable );
    WindowsDeleteString( name );
    if (FAILED(hr)) { printf( "activate=0 hr=%#lx\n", hr ); return 1; }
    IInspectable_QueryInterface( inspectable, &IID_ISpeechSynthesizer, (void **)&synth );
    IInspectable_QueryInterface( inspectable, &IID_ISpeechSynthesizer2, (void **)&synth2 );
    IInspectable_Release( inspectable );
    hr = ISpeechSynthesizer_get_Voice( synth, &voice );
    printf( "synth_voice=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (SUCCEEDED(hr)) IVoiceInformation_Release( voice );

    /* a stream that is spoken audio, a WAV file */
    speak( synth, L"Hello world. This is the synthesizer speaking.", FALSE, &plain );
    printf( "speak=%d hr=%#lx\n", SUCCEEDED(plain.hr), plain.hr );
    printf( "wav=%d size=%llu\n", plain.wav_ok, plain.size );
    printf( "audio=%d seconds=%.2f rms=%.0f\n", plain.seconds > 1.0 && plain.seconds < 10.0 && plain.rms > 200,
            plain.seconds, plain.rms );
    printf( "no_markers=%d words=%u\n", !plain.words && !plain.sentences, plain.words );

    str = hs( L"Hi." );
    ISpeechSynthesizer_SynthesizeTextToStreamAsync( synth, str, &op );
    WindowsDeleteString( str );
    stream = NULL;
    if (SUCCEEDED(wait_async( (IInspectable *)op ))) IAsyncOperation_SpeechSynthesisStream_GetResults( op, &stream );
    IAsyncOperation_SpeechSynthesisStream_Release( op );
    if (stream && SUCCEEDED(ISpeechSynthesisStream_QueryInterface( stream, &IID_IContentTypeProvider_, &ctp )))
    {
        HSTRING type = NULL;
        SLOT( ctp, 6, get_type_fn )( ctp, &type );
        printf( "content_type=%d type=%ls\n", !wcscmp( WindowsGetStringRawBuffer( type, NULL ), L"audio/wav" ),
                WindowsGetStringRawBuffer( type, NULL ) );
        WindowsDeleteString( type );
        SLOT( ctp, 2, release_fn )( ctp );
    }
    else printf( "content_type=0\n" );
    if (stream) ISpeechSynthesisStream_Release( stream );

    /* SSML, and a break in it */
    speak( synth, L"<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>"
                  L"Hello world. This is the synthesizer speaking.</speak>", TRUE, &ssml );
    speak( synth, L"<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>"
                  L"Hello world.<break time='1500ms'/>This is the synthesizer &amp; speaking.</speak>", TRUE, &brk );
    printf( "ssml=%d seconds=%.2f\n", SUCCEEDED(ssml.hr) && ssml.wav_ok && ssml.rms > 200 &&
            fabs( ssml.seconds - plain.seconds ) < 0.5, ssml.seconds );
    printf( "ssml_break=%d seconds=%.2f\n", SUCCEEDED(brk.hr) && brk.seconds > ssml.seconds + 1.0, brk.seconds );
    speak( synth, L"Plain text is not SSML.", TRUE, &bad );
    printf( "ssml_rejected=%d hr=%#lx\n", bad.hr == (HRESULT)0x80131537, bad.hr );

    /* the options: markers and rate */
    if (synth2 && SUCCEEDED(ISpeechSynthesizer2_get_Options( synth2, &options )))
    {
        ISpeechSynthesizerOptions_put_IncludeWordBoundaryMetadata( options, TRUE );
        ISpeechSynthesizerOptions_put_IncludeSentenceBoundaryMetadata( options, TRUE );
        speak( synth, L"Hello world. This is the synthesizer speaking.", FALSE, &words );
        printf( "word_markers=%d words=%u first=%ls\n", words.words == 7 && !wcscmp( words.first_word, L"Hello" ),
                words.words, words.first_word );
        printf( "marker_times=%d first=%lld last=%lld\n", words.times_increase && words.first_time >= 0 &&
                words.last_time > words.first_time && words.last_time < (INT64)(words.seconds * 1e7), words.first_time,
                words.last_time );
        printf( "sentence_markers=%d sentences=%u first=%ls\n", words.sentences == 2 &&
                !wcsncmp( words.first_sentence, L"Hello world.", 12 ), words.sentences, words.first_sentence );
        ISpeechSynthesizerOptions_put_IncludeWordBoundaryMetadata( options, FALSE );
        ISpeechSynthesizerOptions_put_IncludeSentenceBoundaryMetadata( options, FALSE );
        if (SUCCEEDED(ISpeechSynthesizerOptions_QueryInterface( options, &IID_ISpeechSynthesizerOptions2, (void **)&options2 )))
        {
            ISpeechSynthesizerOptions2_put_SpeakingRate( options2, 3.0 );
            speak( synth, L"Hello world. This is the synthesizer speaking.", FALSE, &fast );
            printf( "rate=%d seconds=%.2f\n", SUCCEEDED(fast.hr) && fast.seconds > 0.2 && fast.seconds < plain.seconds * 0.7,
                    fast.seconds );
            ISpeechSynthesizerOptions2_Release( options2 );
        }
        else printf( "rate=0\n" );
        ISpeechSynthesizerOptions_Release( options );
    }
    else printf( "word_markers=0\n" );

    if (synth2) ISpeechSynthesizer2_Release( synth2 );
    ISpeechSynthesizer_Release( synth );
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
