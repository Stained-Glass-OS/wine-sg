/* wmtiming-probe: the Windows Media readers' sample times (patches/sg/6522).
 *
 * Files from the writer before 6518 came out of the readers at 40 ms a frame (no extended stream
 * properties: GStreamer's default) and started late (a simple index not counting the preroll).
 * Writes 10 fps and 30 fps WMVs (WMV 2, 2 s; with and without WMA) with the SDK's writer, reads
 * them through WMCreateSyncReader and WMCreateReader (its OnSample callback), and checks the
 * frame count and each frame's time and duration.
 *
 * Prints name=value lines; see test/wmtiming-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdkidl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

DEFINE_GUID(p_WMMEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_WMV2, 0x32564d57, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_WMAudioV8, 0x00000161, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMFORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID(p_WMFORMAT_VideoInfo, 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

#define W 64
#define H 48

static HRESULT (WINAPI *pWMCreateProfileManager)( IWMProfileManager ** );
static HRESULT (WINAPI *pWMCreateWriter)( IUnknown *, IWMWriter ** );
static HRESULT (WINAPI *pWMCreateSyncReader)( IUnknown *, DWORD, IWMSyncReader ** );
static HRESULT (WINAPI *pWMCreateReader)( IUnknown *, DWORD, IWMReader ** );

static HRESULT add_stream( IWMProfile *profile, const GUID *type, WORD number, const GUID *subtype,
                           const GUID *formattype, void *format, DWORD format_size, DWORD bitrate, BOOL fixed )
{
    IWMStreamConfig *config;
    IWMMediaProps *props;
    WM_MEDIA_TYPE mt = { 0 };
    HRESULT hr;

    if (FAILED(hr = IWMProfile_CreateNewStream( profile, type, &config ))) return hr;
    IWMStreamConfig_SetStreamNumber( config, number );
    IWMStreamConfig_SetBitrate( config, bitrate );
    IWMStreamConfig_SetBufferWindow( config, 2000 );
    IWMStreamConfig_SetStreamName( config, (WCHAR *)L"stream" );
    IWMStreamConfig_SetConnectionName( config, number == 1 ? (WCHAR *)L"Audio" : (WCHAR *)L"Video" );
    mt.majortype = *type;
    mt.subtype = *subtype;
    mt.bFixedSizeSamples = fixed;
    mt.formattype = *formattype;
    mt.cbFormat = format_size;
    mt.pbFormat = format;
    IWMStreamConfig_QueryInterface( config, &IID_IWMMediaProps, (void **)&props );
    hr = IWMMediaProps_SetMediaType( props, &mt );
    IWMMediaProps_Release( props );
    if (SUCCEEDED(hr)) hr = IWMProfile_AddStream( profile, config );
    IWMStreamConfig_Release( config );
    return hr;
}

static HRESULT choose_input( IWMWriter *writer, DWORD input, const GUID *subtype )
{
    DWORD count = 0, i;
    IWMInputMediaProps *props;

    IWMWriter_GetInputFormatCount( writer, input, &count );
    for (i = 0; i < count; i++)
    {
        WM_MEDIA_TYPE *mt;
        DWORD size = 0;
        BOOL match;

        if (FAILED(IWMWriter_GetInputFormat( writer, input, i, &props ))) continue;
        IWMInputMediaProps_GetMediaType( props, NULL, &size );
        mt = malloc( size );
        IWMInputMediaProps_GetMediaType( props, mt, &size );
        match = IsEqualGUID( &mt->subtype, subtype );
        free( mt );
        if (match)
        {
            HRESULT hr = IWMWriter_SetInputProps( writer, input, props );
            IWMInputMediaProps_Release( props );
            return hr;
        }
        IWMInputMediaProps_Release( props );
    }
    return E_FAIL;
}

static HRESULT write_sample( IWMWriter *writer, DWORD input, QWORD time, DWORD flags, const BYTE *data, DWORD size )
{
    INSSBuffer *buffer;
    BYTE *dst;
    DWORD length;
    HRESULT hr;

    if (FAILED(hr = IWMWriter_AllocateSample( writer, size, &buffer ))) return hr;
    INSSBuffer_GetBufferAndLength( buffer, &dst, &length );
    memcpy( dst, data, size );
    INSSBuffer_SetLength( buffer, size );
    hr = IWMWriter_WriteSample( writer, input, time, flags, buffer );
    INSSBuffer_Release( buffer );
    return hr;
}


static BOOL write_file( const WCHAR *path, int fps, int frames, BOOL audio )
{
    QWORD audio_time = 0;
    static BYTE frame[W * H * 3 / 2];
    IWMProfileManager *manager;
    IWMProfile *profile;
    IWMWriter *writer;
    BYTE vih[88] = { 0 };
    HRESULT hr;
    int i;
    DWORD j;

    *(LONGLONG *)(vih + 40) = 10000000 / fps;
    *(DWORD *)(vih + 32) = 300000;
    *(DWORD *)(vih + 48) = 40;
    *(LONG *)(vih + 52) = W;
    *(LONG *)(vih + 56) = H;
    *(WORD *)(vih + 60) = 1;
    *(WORD *)(vih + 62) = 24;
    *(DWORD *)(vih + 64) = 0x32564d57;
    static BYTE pcm[4410 * 4];
    BYTE wma[18 + 10] = { 0 };
    WAVEFORMATEX *wfx = (WAVEFORMATEX *)wma;

    /* a sound stream beside the video, as real WMVs have (WMA 2, 64 kbit/s) */
    wfx->wFormatTag = 0x161;
    wfx->nChannels = 2;
    wfx->nSamplesPerSec = 44100;
    wfx->nAvgBytesPerSec = 8000;
    wfx->nBlockAlign = 371;
    wfx->wBitsPerSample = 16;
    wfx->cbSize = 10;
    pWMCreateProfileManager( &manager );
    hr = IWMProfileManager_CreateEmptyProfile( manager, WMT_VER_9_0, &profile );
    if (SUCCEEDED(hr) && audio) hr = add_stream( profile, &p_WMMEDIATYPE_Audio, 2, &p_WMMEDIASUBTYPE_WMAudioV8,
                                                 &p_WMFORMAT_WaveFormatEx, wma, sizeof(wma), 64000, TRUE );
    if (SUCCEEDED(hr)) hr = add_stream( profile, &p_WMMEDIATYPE_Video, 1, &p_WMMEDIASUBTYPE_WMV2, &p_WMFORMAT_VideoInfo,
                                        vih, sizeof(vih), 300000, FALSE );
    if (FAILED(hr)) return FALSE;
    pWMCreateWriter( NULL, &writer );
    IWMWriter_SetProfile( writer, profile );
    IWMWriter_SetOutputFilename( writer, path );
    if (audio && FAILED(choose_input( writer, 0, &p_WMMEDIASUBTYPE_PCM ))) return FALSE;
    if (FAILED(choose_input( writer, audio ? 1 : 0, &p_WMMEDIASUBTYPE_I420 ))) return FALSE;
    if (FAILED(IWMWriter_BeginWriting( writer ))) return FALSE;
    for (i = 0; i < frames; i++)
    {
        QWORD t = (QWORD)i * 10000000 / fps;

        for (j = 0; j < sizeof(frame); j++) frame[j] = (BYTE)((j + i * 9) & 0xff);
        if (FAILED(write_sample( writer, audio ? 1 : 0, t, i % 10 ? 0 : WM_SF_CLEANPOINT, frame, sizeof(frame) )))
            return FALSE;
        /* the sound in 100 ms pieces, kept up with the video */
        while (audio && audio_time <= t)
        {
            for (j = 0; j < sizeof(pcm) / 2; j++) ((short *)pcm)[j] = (short)((j % 100) < 50 ? 4000 : -4000);
            if (FAILED(write_sample( writer, 0, audio_time, 0, pcm, sizeof(pcm) ))) return FALSE;
            audio_time += 1000000;
        }
    }
    hr = IWMWriter_EndWriting( writer );
    IWMWriter_Release( writer );
    IWMProfile_Release( profile );
    IWMProfileManager_Release( manager );
    return SUCCEEDED(hr);
}

struct times
{
    int count;
    QWORD pts[256], duration[256];
};

static BOOL times_ok( const struct times *t, int fps, int frames, char *why, size_t why_size )
{
    QWORD step = 10000000 / fps;
    int i;

    if (t->count != frames)
    {
        snprintf( why, why_size, "count %d", t->count );
        return FALSE;
    }
    for (i = 0; i < t->count; i++)
    {
        QWORD want = (QWORD)i * step;
        if ((t->pts[i] > want ? t->pts[i] - want : want - t->pts[i]) > 10000 ||
            (t->duration[i] > step ? t->duration[i] - step : step - t->duration[i]) > 10000)
        {
            snprintf( why, why_size, "frame %d pts %llu duration %llu", i, t->pts[i], t->duration[i] );
            return FALSE;
        }
    }
    snprintf( why, why_size, "count %d", t->count );
    return TRUE;
}

static void read_sync( const WCHAR *path, struct times *t )
{
    IWMSyncReader *reader;
    INSSBuffer *sample;
    QWORD pts, duration;
    DWORD flags, output;
    WORD stream;

    t->count = 0;
    if (FAILED(pWMCreateSyncReader( NULL, 0, &reader ))) return;
    if (SUCCEEDED(IWMSyncReader_Open( reader, path )))
    {
        GUID types[4] = { 0 };
        DWORD outputs = 0, i;
        IWMOutputMediaProps *props;

        IWMSyncReader_GetOutputCount( reader, &outputs );
        for (i = 0; i < outputs && i < 4; i++)
            if (SUCCEEDED(IWMSyncReader_GetOutputProps( reader, i, &props )))
            {
                IWMOutputMediaProps_GetType( props, &types[i] );
                IWMOutputMediaProps_Release( props );
            }
        while (t->count < 256 && IWMSyncReader_GetNextSample( reader, 0, &sample, &pts, &duration, &flags, &output, &stream ) == S_OK)
        {
            if (output < 4 && IsEqualGUID( &types[output], &p_WMMEDIATYPE_Video ))
            {
                t->pts[t->count] = pts;
                t->duration[t->count++] = duration;
            }
            INSSBuffer_Release( sample );
        }
        IWMSyncReader_Close( reader );
    }
    IWMSyncReader_Release( reader );
}

/* the asynchronous reader: OnSample per frame, OnStatus for the state changes */
struct callback
{
    IWMReaderCallback IWMReaderCallback_iface;
    struct times *times;
    HANDLE opened, ended;
    DWORD video_output;
};

static HRESULT WINAPI callback_QueryInterface( IWMReaderCallback *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IWMStatusCallback ) || IsEqualGUID( iid, &IID_IWMReaderCallback ))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI callback_AddRef( IWMReaderCallback *iface ) { return 2; }
static ULONG WINAPI callback_Release( IWMReaderCallback *iface ) { return 1; }
static HRESULT WINAPI callback_OnStatus( IWMReaderCallback *iface, WMT_STATUS status, HRESULT hr, WMT_ATTR_DATATYPE type,
                                         BYTE *value, void *context )
{
    struct callback *cb = CONTAINING_RECORD( iface, struct callback, IWMReaderCallback_iface );
    if (status == WMT_OPENED) SetEvent( cb->opened );
    if (status == WMT_EOF || status == WMT_ERROR) SetEvent( cb->ended );
    return S_OK;
}
static HRESULT WINAPI callback_OnSample( IWMReaderCallback *iface, DWORD output, QWORD time, QWORD duration, DWORD flags,
                                         INSSBuffer *sample, void *context )
{
    struct callback *cb = CONTAINING_RECORD( iface, struct callback, IWMReaderCallback_iface );
    if (output == cb->video_output && cb->times->count < 256)
    {
        cb->times->pts[cb->times->count] = time;
        cb->times->duration[cb->times->count++] = duration;
    }
    return S_OK;
}
static IWMReaderCallbackVtbl callback_vtbl = { callback_QueryInterface, callback_AddRef, callback_Release, callback_OnStatus, callback_OnSample };

static void read_async( const WCHAR *path, struct times *t )
{
    struct callback cb = { { &callback_vtbl }, t };
    IWMReaderAdvanced *advanced;
    IWMReader *reader;

    t->count = 0;
    cb.opened = CreateEventW( NULL, FALSE, FALSE, NULL );
    cb.ended = CreateEventW( NULL, FALSE, FALSE, NULL );
    if (FAILED(pWMCreateReader( NULL, 0, &reader ))) return;
    /* as fast as it can deliver, not in real time */
    if (SUCCEEDED(IWMReader_QueryInterface( reader, &IID_IWMReaderAdvanced, (void **)&advanced )))
    {
        IWMReaderAdvanced_SetUserProvidedClock( advanced, TRUE );
        IWMReaderAdvanced_Release( advanced );
    }
    cb.video_output = (DWORD)-1;
    if (SUCCEEDED(IWMReader_Open( reader, path, &cb.IWMReaderCallback_iface, NULL )) &&
        WaitForSingleObject( cb.opened, 10000 ) == WAIT_OBJECT_0)
    {
        DWORD outputs = 0, i;
        IWMOutputMediaProps *props;
        GUID type;

        IWMReader_GetOutputCount( reader, &outputs );
        for (i = 0; i < outputs; i++)
            if (SUCCEEDED(IWMReader_GetOutputProps( reader, i, &props )))
            {
                if (SUCCEEDED(IWMOutputMediaProps_GetType( props, &type )) && IsEqualGUID( &type, &p_WMMEDIATYPE_Video ))
                    cb.video_output = i;
                IWMOutputMediaProps_Release( props );
            }
    }
    if (cb.video_output != (DWORD)-1 && SUCCEEDED(IWMReader_Start( reader, 0, 0, 1.0f, NULL )))
    {
        if (SUCCEEDED(IWMReader_QueryInterface( reader, &IID_IWMReaderAdvanced, (void **)&advanced )))
        {
            IWMReaderAdvanced_DeliverTime( advanced, (QWORD)-1 );
            IWMReaderAdvanced_Release( advanced );
        }
        WaitForSingleObject( cb.ended, 20000 );
        IWMReader_Stop( reader );
    }
    IWMReader_Close( reader );
    IWMReader_Release( reader );
}

int main( void )
{
    static const int rates[] = { 10, 30 };
    WCHAR path[MAX_PATH];
    char why[128];
    struct times t;
    int r;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    {
        HMODULE wmv = LoadLibraryA( "wmvcore.dll" );
        pWMCreateProfileManager = (void *)GetProcAddress( wmv, "WMCreateProfileManager" );
        pWMCreateWriter = (void *)GetProcAddress( wmv, "WMCreateWriter" );
        pWMCreateSyncReader = (void *)GetProcAddress( wmv, "WMCreateSyncReader" );
        pWMCreateReader = (void *)GetProcAddress( wmv, "WMCreateReader" );
    }
    for (r = 0; r < 4; r++)
    {
        int fps = rates[r % 2], frames = 2 * fps;
        BOOL audio = r >= 2, ok;
        const char *tag = audio ? "av" : "";

        GetTempPathW( MAX_PATH, path );
        swprintf( path + wcslen( path ), MAX_PATH, L"timing%d%hs.wmv", fps, tag );
        printf( "write%s%d=%d\n", tag, fps, write_file( path, fps, frames, audio ) );
        read_sync( path, &t );
        ok = times_ok( &t, fps, frames, why, sizeof(why) );
        printf( "sync%s%d=%d %s\n", tag, fps, ok, why );
        read_async( path, &t );
        ok = times_ok( &t, fps, frames, why, sizeof(why) );
        printf( "async%s%d=%d %s\n", tag, fps, ok, why );
    }
    printf( "done=1\n" );
    return 0;
}
