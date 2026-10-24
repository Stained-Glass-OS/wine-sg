/* wmwriter-probe: the Windows Media writer writes ASF files (patches/sg/6517).
 *
 * BeginWriting was E_NOTIMPL. Makes a profile with a PCM audio stream and an RGB24 video stream,
 * writes samples through WriteSample (the input formats are the streams' own) and one through
 * IWMWriterAdvanced::WriteStreamSample, with a title attribute, then reads the file back as
 * the ASF specification lays it out: the header's objects, the streams' formats, every media
 * object reassembled from the data packets and compared, the packet count and file size, the
 * simple index of the video stream. A sink of the probe's own gets the header and the packets.
 *
 * Prints name=value lines; see test/wmwriter-gate.sh.
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

static const GUID header_guid = {0x75b22630, 0x668e, 0x11cf, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static const GUID fpo_guid = {0x8cabdca1, 0xa947, 0x11cf, {0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}};
static const GUID spo_guid = {0xb7dc0791, 0xa9b7, 0x11cf, {0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}};
static const GUID cdo_guid = {0x75b22633, 0x668e, 0x11cf, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static const GUID hext_guid = {0x5fbf03b5, 0xa92e, 0x11cf, {0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}};
static const GUID data_guid = {0x75b22636, 0x668e, 0x11cf, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static const GUID index_guid = {0x33000890, 0xe5b1, 0x11cf, {0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb}};
static const GUID audio_media = {0xf8699e40, 0x5b4d, 0x11cf, {0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b}};
static const GUID video_media = {0xbc19efc0, 0x5b4d, 0x11cf, {0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b}};
DEFINE_GUID(p_WMMEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_RGB24, 0xe436eb7d, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(p_WMFORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID(p_WMFORMAT_VideoInfo, 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

#define W 32
#define H 16
#define FRAME (W * H * 3)
#define CHUNK 4410          /* 25 ms of 44.1 kHz stereo 16-bit */
#define CHUNKS 80
#define FRAMES 20           /* 10 fps: 2 s */

static HRESULT (WINAPI *pWMCreateProfileManager)( IWMProfileManager ** );
static HRESULT (WINAPI *pWMCreateWriter)( IUnknown *, IWMWriter ** );

static BYTE audio[CHUNKS * CHUNK], video[FRAMES + 1][FRAME];

/* ---- a sink of the probe's own ---- */
static LONG sink_headers, sink_units, sink_end;
static HRESULT WINAPI sink_QueryInterface( IWMWriterSink *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IWMWriterSink )) { *out = iface; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef( IWMWriterSink *iface ) { return 2; }
static ULONG WINAPI sink_Release( IWMWriterSink *iface ) { return 1; }
static HRESULT WINAPI sink_OnHeader( IWMWriterSink *iface, INSSBuffer *header )
{
    BYTE *data; DWORD length;
    INSSBuffer_GetBufferAndLength( header, &data, &length );
    if (length > 30 && !memcmp( data, &header_guid, 16 )) sink_headers++;
    return S_OK;
}
static HRESULT WINAPI sink_IsRealTime( IWMWriterSink *iface, BOOL *real_time ) { *real_time = FALSE; return S_OK; }
static HRESULT WINAPI sink_AllocateDataUnit( IWMWriterSink *iface, DWORD size, INSSBuffer **buffer );
static HRESULT WINAPI sink_OnDataUnit( IWMWriterSink *iface, INSSBuffer *unit ) { sink_units++; return S_OK; }
static HRESULT WINAPI sink_OnEndWriting( IWMWriterSink *iface ) { sink_end++; return S_OK; }
static IWMWriterSinkVtbl sink_vtbl = { sink_QueryInterface, sink_AddRef, sink_Release, sink_OnHeader, sink_IsRealTime,
                                       sink_AllocateDataUnit, sink_OnDataUnit, sink_OnEndWriting };
static IWMWriterSink sink = { &sink_vtbl };
static IWMWriter *writer;
static HRESULT WINAPI sink_AllocateDataUnit( IWMWriterSink *iface, DWORD size, INSSBuffer **buffer )
{
    return IWMWriter_AllocateSample( writer, size, buffer );
}

static HRESULT add_stream( IWMProfile *profile, const GUID *type, WORD number, const GUID *subtype,
                           const GUID *formattype, void *format, DWORD format_size, DWORD bitrate )
{
    IWMStreamConfig *config;
    IWMMediaProps *props;
    WM_MEDIA_TYPE mt = { 0 };
    HRESULT hr;

    if (FAILED(hr = IWMProfile_CreateNewStream( profile, type, &config ))) return hr;
    IWMStreamConfig_SetStreamNumber( config, number );
    IWMStreamConfig_SetBitrate( config, bitrate );
    IWMStreamConfig_SetBufferWindow( config, 1000 );
    IWMStreamConfig_SetStreamName( config, (WCHAR *)L"stream" );
    IWMStreamConfig_SetConnectionName( config, number == 1 ? (WCHAR *)L"Audio" : (WCHAR *)L"Video" );
    mt.majortype = *type;
    mt.subtype = *subtype;
    mt.bFixedSizeSamples = TRUE;
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

/* choose the input format that is the stream's own */
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

/* ---- reading the file back ---- */
struct object_out { BYTE *data; DWORD size, filled; DWORD presentation; BOOL key; DWORD packet; };
static DWORD current_packet;
static struct object_out audio_objects[CHUNKS + 4], video_objects[FRAMES + 4];
static int audio_count, video_count;
static BOOL bad_payload;

static void payload( BYTE stream, BYTE number, DWORD offset, DWORD object_size, DWORD presentation, const BYTE *data, DWORD size )
{
    struct object_out *objects = (stream & 0x7f) == 1 ? audio_objects : video_objects;
    int *count = (stream & 0x7f) == 1 ? &audio_count : &video_count;
    struct object_out *o;

    if (!offset)
    {
        if (*count >= ((stream & 0x7f) == 1 ? CHUNKS + 4 : FRAMES + 4)) { bad_payload = TRUE; return; }
        o = &objects[(*count)++];
        o->data = malloc( object_size );
        o->size = object_size;
        o->filled = 0;
        o->presentation = presentation;
        o->key = !!(stream & 0x80);
        o->packet = current_packet;
    }
    else
    {
        if (!*count) { bad_payload = TRUE; return; }
        o = &objects[*count - 1];
    }
    if (offset != o->filled || o->filled + size > o->size) { bad_payload = TRUE; return; }
    memcpy( o->data + offset, data, size );
    o->filled += size;
}

int main( void )
{
    WCHAR path[MAX_PATH];
    IWMProfileManager *manager;
    IWMProfile *profile;
    IWMWriterAdvanced *advanced;
    IWMHeaderInfo *info;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 2, 44100, 44100 * 4, 4, 16, 0 };
    BYTE vih[88] = { 0 };
    HRESULT hr;
    HANDLE file;
    DWORD size, read, i, j;
    BYTE *data;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    {
        HMODULE wmv = LoadLibraryA( "wmvcore.dll" );
        pWMCreateProfileManager = (void *)GetProcAddress( wmv, "WMCreateProfileManager" );
        pWMCreateWriter = (void *)GetProcAddress( wmv, "WMCreateWriter" );
    }
    for (i = 0; i < sizeof(audio); i++) audio[i] = (BYTE)(i * 7 + i / 300);
    for (j = 0; j <= FRAMES; j++) for (i = 0; i < FRAME; i++) video[j][i] = (BYTE)(i + j * 31);

    /* the profile */
    pWMCreateProfileManager( &manager );
    hr = IWMProfileManager_CreateEmptyProfile( manager, WMT_VER_9_0, &profile );
    if (SUCCEEDED(hr)) hr = add_stream( profile, &p_WMMEDIATYPE_Audio, 1, &p_WMMEDIASUBTYPE_PCM, &p_WMFORMAT_WaveFormatEx,
                                        &wfx, sizeof(wfx), 44100 * 32 );
    *(LONGLONG *)(vih + 40) = 1000000;              /* 10 fps */
    *(DWORD *)(vih + 48) = 40;
    *(LONG *)(vih + 52) = W;
    *(LONG *)(vih + 56) = H;
    *(WORD *)(vih + 60) = 1;
    *(WORD *)(vih + 62) = 24;
    *(DWORD *)(vih + 68) = FRAME;
    if (SUCCEEDED(hr)) hr = add_stream( profile, &p_WMMEDIATYPE_Video, 2, &p_WMMEDIASUBTYPE_RGB24, &p_WMFORMAT_VideoInfo,
                                        vih, sizeof(vih), FRAME * 8 * 10 );
    printf( "profile=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;

    /* writing */
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"wmwriter.asf" );
    pWMCreateWriter( NULL, &writer );
    IWMWriter_SetProfile( writer, profile );
    IWMWriter_SetOutputFilename( writer, path );
    IWMWriter_QueryInterface( writer, &IID_IWMWriterAdvanced, (void **)&advanced );
    IWMWriterAdvanced_AddSink( advanced, &sink );
    IWMWriter_QueryInterface( writer, &IID_IWMHeaderInfo, (void **)&info );
    IWMHeaderInfo_SetAttribute( info, 0, L"Title", WMT_TYPE_STRING, (BYTE *)L"Probe Title", sizeof(L"Probe Title") );
    hr = choose_input( writer, 0, &p_WMMEDIASUBTYPE_PCM );
    if (SUCCEEDED(hr)) hr = choose_input( writer, 1, &p_WMMEDIASUBTYPE_RGB24 );
    printf( "inputs=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    hr = IWMWriter_BeginWriting( writer );
    printf( "begin=%d hr=%#lx\n", hr == S_OK, hr );
    if (FAILED(hr)) return 1;
    {
        HRESULT hr1 = S_OK, hr2 = S_OK;
        for (i = 0; i < CHUNKS; i++)
        {
            QWORD t = (QWORD)i * CHUNK * 10000000 / wfx.nAvgBytesPerSec;
            if (FAILED(hr1 = write_sample( writer, 0, t, 0, audio + i * CHUNK, CHUNK ))) break;
            if (i % 4 == 0 && i / 4 < FRAMES &&
                FAILED(hr2 = write_sample( writer, 1, (QWORD)(i / 4) * 1000000, (i / 4) % 5 ? 0 : WM_SF_CLEANPOINT,
                                           video[i / 4], FRAME ))) break;
        }
        printf( "write_sample=%d hr=%#lx %#lx\n", SUCCEEDED(hr1) && SUCCEEDED(hr2), hr1, hr2 );
    }
    {
        INSSBuffer *buffer;
        BYTE *dst;
        DWORD length;
        IWMWriter_AllocateSample( writer, FRAME, &buffer );
        INSSBuffer_GetBufferAndLength( buffer, &dst, &length );
        memcpy( dst, video[FRAMES], FRAME );
        INSSBuffer_SetLength( buffer, FRAME );
        hr = IWMWriterAdvanced_WriteStreamSample( advanced, 2, (QWORD)FRAMES * 1000000, FRAMES * 100, 1000000, WM_SF_CLEANPOINT, buffer );
        INSSBuffer_Release( buffer );
        printf( "write_stream_sample=%d hr=%#lx\n", hr == S_OK, hr );
    }
    {
        WM_WRITER_STATISTICS stats = { 0 };
        IWMWriterAdvanced_GetStatistics( advanced, 1, &stats );
        printf( "statistics=%d samples=%llu bytes=%llu\n", stats.qwSampleCount == CHUNKS && stats.qwByteCount == sizeof(audio),
                stats.qwSampleCount, stats.qwByteCount );
    }
    hr = IWMWriter_EndWriting( writer );
    printf( "end=%d hr=%#lx\n", hr == S_OK, hr );
    printf( "sink=%d headers=%ld units=%ld end=%ld\n", sink_headers == 1 && sink_units > 0 && sink_end == 1, sink_headers,
            sink_units, sink_end );
    printf( "end_twice=%d\n", FAILED(IWMWriter_EndWriting( writer )) );

    /* reading back */
    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    size = GetFileSize( file, NULL );
    data = malloc( size );
    ReadFile( file, data, size, &read, NULL );
    CloseHandle( file );
    {
        UINT64 header_size, packets = 0, fpo_size = 0, data_packets, packet_size = 0, play = 0, preroll = 0, pos;
        DWORD count, k;
        int streams = 0, audio_ok = 0, video_ok = 0, title = 0, hext = 0;
        BOOL ok;

        ok = read == size && size > 100 && !memcmp( data, &header_guid, 16 );
        memcpy( &header_size, data + 16, 8 );
        memcpy( &count, data + 24, 4 );
        for (k = 0, pos = 30; k < count && pos + 24 <= header_size; k++)
        {
            UINT64 object_size;
            const BYTE *o = data + pos;
            memcpy( &object_size, o + 16, 8 );
            if (!memcmp( o, &fpo_guid, 16 ))
            {
                memcpy( &fpo_size, o + 40, 8 );
                memcpy( &packets, o + 56, 8 );
                memcpy( &play, o + 64, 8 );
                memcpy( &preroll, o + 80, 8 );
                packet_size = *(DWORD *)(o + 92);
            }
            else if (!memcmp( o, &spo_guid, 16 ))
            {
                DWORD ts = *(DWORD *)(o + 64);
                WORD number = *(WORD *)(o + 72) & 0x7f;
                streams++;
                if (number == 1 && !memcmp( o + 24, &audio_media, 16 ) && ts >= 18 && !memcmp( o + 78, &wfx, 16 )) audio_ok = 1;
                if (number == 2 && !memcmp( o + 24, &video_media, 16 ) && *(DWORD *)(o + 78) == W && *(DWORD *)(o + 82) == H &&
                    *(WORD *)(o + 78 + 11 + 14) == 24) video_ok = 1;
            }
            else if (!memcmp( o, &cdo_guid, 16 ))
                title = !memcmp( o + 34, L"Probe Title", sizeof(L"Probe Title") );
            else if (!memcmp( o, &hext_guid, 16 )) hext = 1;
            if (object_size < 24) break;
            pos += object_size;
        }
        printf( "header=%d objects=%lu\n", ok && streams == 2 && hext, count );
        printf( "streams=%d audio=%d video=%d\n", audio_ok && video_ok, audio_ok, video_ok );
        printf( "title=%d\n", title );
        printf( "durations=%d play=%llu preroll=%llu\n", preroll == 1000 && play >= 20000000 + preroll * 10000, play, preroll );

        /* the data object and its packets */
        ok = !memcmp( data + header_size, &data_guid, 16 );
        memcpy( &data_packets, data + header_size + 40, 8 );
        printf( "packets=%d fpo=%llu data=%llu\n", ok && packets && packets == data_packets &&
                header_size + 50 + packets * packet_size <= size, packets, data_packets );
        for (pos = header_size + 50; pos + packet_size <= header_size + 50 + data_packets * packet_size; pos += packet_size)
        {
            const BYTE *p = data + pos;
            DWORD q = 0, offset, object_size, presentation;
            WORD padding;
            BYTE flags, props, stream, number, rlen;

            if (p[q] & 0x80) q += 1 + (p[q] & 0x0f);
            flags = p[q++];
            props = p[q++];
            if (flags & 1 || props != 0x5d) { bad_payload = TRUE; break; }   /* the probe reads the one layout */
            memcpy( &padding, p + q, 2 ); q += 2;
            q += 6;    /* send time, duration */
            stream = p[q++];
            number = p[q++];
            memcpy( &offset, p + q, 4 ); q += 4;
            rlen = p[q++];
            memcpy( &object_size, p + q, 4 );
            memcpy( &presentation, p + q + 4, 4 );
            q += rlen;
            current_packet = (DWORD)((pos - header_size - 50) / packet_size);
            payload( stream, number, offset, object_size, presentation, p + q, packet_size - q - padding );
        }
        ok = !bad_payload && audio_count == CHUNKS;
        for (i = 0; ok && i < CHUNKS; i++)
            ok = audio_objects[i].size == CHUNK && audio_objects[i].filled == CHUNK && !memcmp( audio_objects[i].data, audio + i * CHUNK, CHUNK );
        printf( "audio_data=%d objects=%d bad=%d\n", ok, audio_count, bad_payload );
        ok = !bad_payload && video_count == FRAMES + 1;
        for (i = 0; ok && i <= FRAMES; i++)
            ok = video_objects[i].filled == FRAME && !memcmp( video_objects[i].data, video[i], FRAME ) &&
                 video_objects[i].key == (i == FRAMES || i % 5 == 0);
        printf( "video_data=%d objects=%d\n", ok, video_count );
        ok = video_count > 2 && video_objects[1].presentation == 100 + 1000 && video_objects[2].presentation == 200 + 1000;
        printf( "times=%d t1=%lu\n", ok, video_count > 1 ? video_objects[1].presentation : 0 );

        /* the simple index after the data */
        pos = header_size + 50 + data_packets * packet_size;
        ok = pos + 56 <= size && !memcmp( data + pos, &index_guid, 16 );
        if (ok)
        {
            DWORD entries = *(DWORD *)(data + pos + 52);
            const BYTE *e = data + pos + 56;
            ok = entries == 3 && video_count > FRAMES && *(DWORD *)e == video_objects[0].packet &&
                 *(DWORD *)(e + 6) == video_objects[10].packet && *(DWORD *)(e + 12) == video_objects[20].packet;
            printf( "index=%d entries=%lu first=%lu want=%lu\n", ok, entries, *(DWORD *)e,
                    video_count ? video_objects[0].packet : 0 );
        }
        else printf( "index=0\n" );
        printf( "file_size=%d stored=%llu size=%lu\n", fpo_size == size, fpo_size, size );
    }
    printf( "done=1\n" );
    return 0;
}
