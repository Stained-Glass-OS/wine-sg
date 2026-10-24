/* wmencode-probe: the Windows Media writer encodes WMA and WMV (patches/sg/6518).
 *
 * With a profile of a WMA (version 2) audio stream and a WMV (version 2) video stream, feeds
 * PCM and uncompressed frames through IWMWriter::WriteSample, then checks the file's stream
 * properties (the codecs, the WMA codec data) and plays it back through the SDK's own reader
 * (WMCreateSyncReader), counting the decoded sound and frames.
 *
 * Prints name=value lines; see test/wmencode-gate.sh.
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

static const GUID spo_guid = {0xb7dc0791, 0xa9b7, 0x11cf, {0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}};
DEFINE_GUID(p_WMMEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_WMAudioV8, 0x00000161, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_WMV2, 0x32564d57, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMFORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID(p_WMFORMAT_VideoInfo, 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

#define W 64
#define H 48
#define FRAMES 20
#define RATE 44100
#define CHUNK (RATE / 10 * 4)   /* 100 ms stereo 16-bit */

static HRESULT (WINAPI *pWMCreateProfileManager)( IWMProfileManager ** );
static HRESULT (WINAPI *pWMCreateWriter)( IUnknown *, IWMWriter ** );
static HRESULT (WINAPI *pWMCreateSyncReader)( IUnknown *, DWORD, IWMSyncReader ** );

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

int main( void )
{
    static BYTE pcm[CHUNK], frame[W * H * 3 / 2];
    WCHAR path[MAX_PATH];
    IWMProfileManager *manager;
    IWMProfile *profile;
    IWMWriter *writer;
    BYTE wma[18 + 10] = { 0 }, vih[88] = { 0 };
    WAVEFORMATEX *wfx = (WAVEFORMATEX *)wma;
    HRESULT hr;
    DWORD i, j;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    {
        HMODULE wmv = LoadLibraryA( "wmvcore.dll" );
        pWMCreateProfileManager = (void *)GetProcAddress( wmv, "WMCreateProfileManager" );
        pWMCreateWriter = (void *)GetProcAddress( wmv, "WMCreateWriter" );
        pWMCreateSyncReader = (void *)GetProcAddress( wmv, "WMCreateSyncReader" );
    }

    /* WMA 2, 128 kbit/s; WMV 2 at 300 kbit/s */
    wfx->wFormatTag = 0x161;
    wfx->nChannels = 2;
    wfx->nSamplesPerSec = RATE;
    wfx->nAvgBytesPerSec = 16000;
    wfx->nBlockAlign = 743;
    wfx->wBitsPerSample = 16;
    wfx->cbSize = 10;
    *(LONGLONG *)(vih + 40) = 1000000;   /* 10 fps */
    *(DWORD *)(vih + 32) = 300000;       /* bit rate */
    *(DWORD *)(vih + 48) = 40;
    *(LONG *)(vih + 52) = W;
    *(LONG *)(vih + 56) = H;
    *(WORD *)(vih + 60) = 1;
    *(WORD *)(vih + 62) = 24;
    *(DWORD *)(vih + 64) = 0x32564d57;   /* WMV2 */
    pWMCreateProfileManager( &manager );
    hr = IWMProfileManager_CreateEmptyProfile( manager, WMT_VER_9_0, &profile );
    if (SUCCEEDED(hr)) hr = add_stream( profile, &p_WMMEDIATYPE_Audio, 1, &p_WMMEDIASUBTYPE_WMAudioV8, &p_WMFORMAT_WaveFormatEx,
                                        wma, sizeof(wma), 128000, TRUE );
    if (SUCCEEDED(hr)) hr = add_stream( profile, &p_WMMEDIATYPE_Video, 2, &p_WMMEDIASUBTYPE_WMV2, &p_WMFORMAT_VideoInfo,
                                        vih, sizeof(vih), 300000, FALSE );
    printf( "profile=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;

    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"wmencode.wmv" );
    pWMCreateWriter( NULL, &writer );
    IWMWriter_SetProfile( writer, profile );
    IWMWriter_SetOutputFilename( writer, path );
    hr = choose_input( writer, 0, &p_WMMEDIASUBTYPE_PCM );
    if (SUCCEEDED(hr)) hr = choose_input( writer, 1, &p_WMMEDIASUBTYPE_I420 );
    printf( "inputs=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    hr = IWMWriter_BeginWriting( writer );
    printf( "begin=%d hr=%#lx\n", hr == S_OK, hr );
    if (FAILED(hr)) return 1;
    {
        HRESULT hr1 = S_OK, hr2 = S_OK;
        for (i = 0; i < FRAMES && SUCCEEDED(hr1) && SUCCEEDED(hr2); i++)
        {
            for (j = 0; j < CHUNK / 2; j++) ((short *)pcm)[j] = (short)(((i * CHUNK / 4 + j / 2) % 100) < 50 ? 6000 : -6000);
            hr1 = write_sample( writer, 0, (QWORD)i * 1000000, 0, pcm, CHUNK );
            for (j = 0; j < sizeof(frame); j++) frame[j] = (BYTE)((j + i * 9) & 0xff);
            hr2 = write_sample( writer, 1, (QWORD)i * 1000000, i % 10 ? 0 : WM_SF_CLEANPOINT, frame, sizeof(frame) );
        }
        printf( "write=%d hr=%#lx %#lx\n", SUCCEEDED(hr1) && SUCCEEDED(hr2), hr1, hr2 );
    }
    hr = IWMWriter_EndWriting( writer );
    printf( "end=%d hr=%#lx\n", hr == S_OK, hr );
    IWMWriter_Release( writer );

    /* the stream properties */
    {
        HANDLE file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
        DWORD size = GetFileSize( file, NULL ), read;
        BYTE *data = malloc( size );
        UINT64 header_size, pos;
        int audio_ok = 0, video_ok = 0;

        ReadFile( file, data, size, &read, NULL );
        CloseHandle( file );
        memcpy( &header_size, data + 16, 8 );
        for (pos = 30; pos + 24 <= header_size;)
        {
            UINT64 object_size;
            const BYTE *o = data + pos;
            memcpy( &object_size, o + 16, 8 );
            if (!memcmp( o, &spo_guid, 16 ))
            {
                WORD number = *(WORD *)(o + 72) & 0x7f;
                const BYTE *t = o + 78;
                if (number == 1)
                    audio_ok = *(WORD *)t == 0x161 && *(WORD *)(t + 16) == 10 && *(WORD *)(t + 12) > 0;
                if (number == 2)
                    video_ok = *(DWORD *)t == W && *(DWORD *)(t + 4) == H && *(DWORD *)(t + 11 + 16) == 0x32564d57;
            }
            if (object_size < 24) break;
            pos += object_size;
        }
        printf( "wma_stream=%d\n", audio_ok );
        printf( "wmv_stream=%d\n", video_ok );
        printf( "compressed=%d size=%lu\n", size > 4000 && size < FRAMES * (CHUNK + sizeof(frame)) / 2, size );
        free( data );
    }

    /* played back by the reader */
    {
        IWMSyncReader *reader;
        DWORD outputs = 0, audio_bytes = 0, frames = 0, n = 0;
        INSSBuffer *sample;
        QWORD time, duration;
        DWORD flags, output;
        WORD stream;

        hr = pWMCreateSyncReader( NULL, 0, &reader );
        if (SUCCEEDED(hr)) hr = IWMSyncReader_Open( reader, path );
        if (SUCCEEDED(hr)) IWMSyncReader_GetOutputCount( reader, &outputs );
        printf( "reader=%d hr=%#lx outputs=%lu\n", SUCCEEDED(hr) && outputs == 2, hr, outputs );
        GUID types[2] = { 0 };
        for (output = 0; SUCCEEDED(hr) && output < outputs && output < 2; output++)
        {
            IWMOutputMediaProps *props;
            if (SUCCEEDED(IWMSyncReader_GetOutputProps( reader, output, &props )))
            {
                IWMOutputMediaProps_GetType( props, &types[output] );
                IWMOutputMediaProps_Release( props );
            }
        }
        while (SUCCEEDED(hr) && n++ < 1000 && IWMSyncReader_GetNextSample( reader, 0, &sample, &time, &duration, &flags, &output, &stream ) == S_OK)
        {
            DWORD length = 0;
            INSSBuffer_GetLength( sample, &length );
            /* by the output's type: the reader numbers its streams its own way */
            if (output < 2 && IsEqualGUID( &types[output], &p_WMMEDIATYPE_Audio )) audio_bytes += length;
            else if (output < 2 && IsEqualGUID( &types[output], &p_WMMEDIATYPE_Video )) frames++;
            INSSBuffer_Release( sample );
        }
        printf( "decoded_audio=%d bytes=%lu\n", audio_bytes >= RATE * 4 * 3 / 2, audio_bytes );
        printf( "decoded_video=%d frames=%lu\n", frames >= FRAMES - 2, frames );
        if (reader) IWMSyncReader_Release( reader );
    }
    printf( "done=1\n" );
    return 0;
}
