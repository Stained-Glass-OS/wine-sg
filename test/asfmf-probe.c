/* Probe for mf.dll's WMContainer (ASF) objects and the ASF media sink (wine-sg 9401/9402; test/asfmf-gate.sh).
 * What Microsoft's tutorials do with them: build a profile and a ContentInfo, multiplex samples into
 * packets, write GenerateHeader's output followed by the packets ("Writing a WMA File by Using WMContainer
 * Objects"), then read the file back with GetHeaderSize/ParseHeader and the splitter ("Reading an ASF
 * File"); and the ASF file sink, directly and behind a sink writer (.wma/.asf). Every written file is
 * also read by a source reader (GStreamer's own ASF demuxer), which must give back the same audio. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <wmcontainer.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

#define RATE 22050
#define CHANNELS 2
#define SECONDS 3
#define CHUNK_MS 100

static BYTE *pcm_data(DWORD *size)
{
    DWORD i, n = RATE * SECONDS * CHANNELS;
    short *pcm = malloc(n * 2);
    for (i = 0; i < n; i++) pcm[i] = (short)((i * 7919u) ^ (i >> 3));   /* a pattern, not silence */
    *size = n * 2;
    return (BYTE *)pcm;
}

static IMFMediaType *pcm_type(void)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, CHANNELS);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, RATE);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BLOCK_ALIGNMENT, CHANNELS * 2);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, RATE * CHANNELS * 2);
    IMFMediaType_SetUINT32(t, &MF_MT_ALL_SAMPLES_INDEPENDENT, 1);
    return t;
}

static IMFSample *make_sample(const BYTE *data, DWORD size, LONGLONG time, LONGLONG duration)
{
    IMFMediaBuffer *b;
    IMFSample *s;
    BYTE *p;
    MFCreateMemoryBuffer(size, &b);
    IMFMediaBuffer_Lock(b, &p, NULL, NULL);
    memcpy(p, data, size);
    IMFMediaBuffer_Unlock(b);
    IMFMediaBuffer_SetCurrentLength(b, size);
    MFCreateSample(&s);
    IMFSample_AddBuffer(s, b);
    IMFMediaBuffer_Release(b);
    IMFSample_SetSampleTime(s, time);
    IMFSample_SetSampleDuration(s, duration);
    IMFSample_SetUINT32(s, &MFSampleExtension_CleanPoint, 1);
    return s;
}

static void write_sample_to(IMFByteStream *stream, IMFSample *sample)
{
    IMFMediaBuffer *b; BYTE *p; DWORD len; ULONG written;
    IMFSample_ConvertToContiguousBuffer(sample, &b);
    IMFMediaBuffer_Lock(b, &p, NULL, &len);
    IMFByteStream_Write(stream, p, len, &written);
    IMFMediaBuffer_Unlock(b);
    IMFMediaBuffer_Release(b);
}

static void test_profile(void)
{
    IMFASFProfile *profile, *clone;
    IMFASFStreamConfig *stream, *got;
    IMFASFMutualExclusion *mutex;
    IMFASFStreamPrioritization *prio;
    IMFMediaType *type = pcm_type();
    DWORD count = 0, record;
    WORD number = 0, streams[4], n;
    GUID major;
    HRESULT hr;

    printf("-- profile\n");
    hr = MFCreateASFProfile(&profile);
    CHECK(hr == S_OK, "MFCreateASFProfile %#lx\n", hr);
    if (FAILED(hr)) return;
    hr = IMFASFProfile_CreateStream(profile, type, &stream);
    CHECK(hr == S_OK, "CreateStream %#lx\n", hr);
    CHECK(IMFASFStreamConfig_GetStreamNumber(stream) == 0, "a new stream has no number yet\n");
    CHECK(FAILED(IMFASFProfile_SetStream(profile, stream)), "SetStream refuses a stream without a number\n");
    CHECK(FAILED(IMFASFStreamConfig_SetStreamNumber(stream, 128)), "stream numbers stop at 127\n");
    IMFASFStreamConfig_SetStreamNumber(stream, 3);
    hr = IMFASFStreamConfig_GetStreamType(stream, &major);
    CHECK(hr == S_OK && IsEqualGUID(&major, &MFMediaType_Audio), "GetStreamType is the media type's major type\n");
    IMFASFStreamConfig_SetUINT32(stream, &MF_MT_AUDIO_NUM_CHANNELS, 77);
    hr = IMFASFProfile_SetStream(profile, stream);
    CHECK(hr == S_OK, "SetStream %#lx\n", hr);
    IMFASFStreamConfig_SetStreamNumber(stream, 4);  /* the profile keeps its own copy */
    IMFASFProfile_GetStreamCount(profile, &count);
    hr = IMFASFProfile_GetStreamByNumber(profile, 3, &got);
    CHECK(count == 1 && hr == S_OK, "the copy is stream 3 (%lu streams, %#lx)\n", count, hr);
    if (SUCCEEDED(hr))
    {
        UINT32 v = 0;
        IMFASFStreamConfig_GetUINT32(got, &MF_MT_AUDIO_NUM_CHANNELS, &v);
        CHECK(v == 77, "the stream's own attributes are kept (%u)\n", v);
        IMFASFStreamConfig_Release(got);
    }
    hr = IMFASFProfile_SetStream(profile, stream);
    IMFASFProfile_GetStreamCount(profile, &count);
    CHECK(count == 2, "a second number is a second stream (%lu)\n", count);
    IMFASFStreamConfig_SetStreamNumber(stream, 3);
    IMFASFProfile_SetStream(profile, stream);
    IMFASFProfile_GetStreamCount(profile, &count);
    CHECK(count == 2, "SetStream of a known number replaces it (%lu)\n", count);
    IMFASFProfile_GetStream(profile, 1, &number, &got);
    CHECK(number == 4, "GetStream gives the number (%u)\n", number);
    IMFASFStreamConfig_Release(got);
    hr = IMFASFProfile_RemoveStream(profile, 4);
    IMFASFProfile_GetStreamCount(profile, &count);
    CHECK(hr == S_OK && count == 1, "RemoveStream by number\n");
    CHECK(IMFASFProfile_GetStreamByNumber(profile, 9, &got) == MF_E_NOT_FOUND, "an unknown number is MF_E_NOT_FOUND\n");

    IMFASFProfile_CreateMutualExclusion(profile, &mutex);
    IMFASFMutualExclusion_SetType(mutex, &MFASFMutexType_Bitrate);
    IMFASFMutualExclusion_AddRecord(mutex, &record);
    IMFASFMutualExclusion_AddStreamForRecord(mutex, record, 3);
    IMFASFMutualExclusion_AddStreamForRecord(mutex, record, 5);
    IMFASFProfile_AddMutualExclusion(profile, mutex);
    IMFASFMutualExclusion_Release(mutex);
    IMFASFProfile_CreateStreamPrioritization(profile, &prio);
    IMFASFStreamPrioritization_AddStream(prio, 3, 1);
    IMFASFProfile_AddStreamPrioritization(profile, prio);
    IMFASFStreamPrioritization_Release(prio);

    hr = IMFASFProfile_Clone(profile, &clone);
    CHECK(hr == S_OK, "Clone %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        IMFASFProfile_GetMutualExclusionCount(clone, &count);
        CHECK(count == 1, "the clone has the mutual exclusion\n");
        if (SUCCEEDED(IMFASFProfile_GetMutualExclusion(clone, 0, &mutex)))
        {
            GUID t; DWORD c = 4;
            IMFASFMutualExclusion_GetType(mutex, &t);
            IMFASFMutualExclusion_GetStreamsForRecord(mutex, 0, streams, &c);
            CHECK(IsEqualGUID(&t, &MFASFMutexType_Bitrate) && c == 2 && streams[0] == 3 && streams[1] == 5, "its record and type\n");
            IMFASFMutualExclusion_Release(mutex);
        }
        prio = NULL;
        IMFASFProfile_GetStreamPrioritization(clone, &prio);
        n = 0;
        if (prio) { IMFASFStreamPrioritization_GetStream(prio, 0, &n, NULL); IMFASFStreamPrioritization_Release(prio); }
        CHECK(n == 3, "and the prioritization\n");
        IMFASFProfile_Release(clone);
    }
    IMFASFStreamConfig_Release(stream);
    IMFMediaType_Release(type);
    IMFASFProfile_Release(profile);
}

/* reads an ASF file's audio through a source reader: GStreamer's demuxer must accept the file */
static BYTE *read_with_source_reader(const WCHAR *path, DWORD *total, LONGLONG *first_time)
{
    IMFSourceReader *reader;
    IMFMediaType *type;
    BYTE *out = NULL;
    DWORD flags;
    HRESULT hr;

    *total = 0;
    *first_time = -1;
    if (FAILED(hr = MFCreateSourceReaderFromURL(path, NULL, &reader)))
    {
        printf("      source reader %#lx\n", hr);
        return NULL;
    }
    type = pcm_type();
    hr = IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type);
    IMFMediaType_Release(type);
    for (;;)
    {
        IMFSample *s = NULL; LONGLONG ts; IMFMediaBuffer *b; BYTE *p; DWORD len;
        hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, &ts, &s);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) || !s) { if (s) IMFSample_Release(s); break; }
        if (*first_time < 0) *first_time = ts;
        IMFSample_ConvertToContiguousBuffer(s, &b);
        IMFMediaBuffer_Lock(b, &p, NULL, &len);
        out = realloc(out, *total + len);
        memcpy(out + *total, p, len);
        *total += len;
        IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_Release(b);
        IMFSample_Release(s);
    }
    IMFSourceReader_Release(reader);
    return out;
}

static void check_file_audio(const WCHAR *path, const BYTE *pcm, DWORD size, const char *what)
{
    LONGLONG first;
    DWORD total;
    BYTE *got = read_with_source_reader(path, &total, &first);
    CHECK(got && total == size && !memcmp(got, pcm, size), "%s: a source reader reads the same %lu bytes back (%lu)\n", what, size, total);
    CHECK(got && first >= 0 && first < 10000, "%s: from time 0 (%lld)\n", what, (long long)first);
    free(got);
}

static void test_wmcontainer_roundtrip(void)
{
    IMFASFContentInfo *info, *read_info;
    IMFASFProfile *profile, *read_profile;
    IMFASFStreamConfig *stream;
    IMFASFMultiplexer *mux;
    IMFASFSplitter *splitter;
    IMFPresentationDescriptor *pd;
    IMFMediaBuffer *header, *buffer;
    IMFByteStream *file;
    IMFMediaType *type = pcm_type(), *read_type;
    DWORD size, header_size = 0, status, packets = 0, pcm_size, chunk, i, length;
    DWORD bucket[2] = {RATE * CHANNELS * 16, 3000};
    UINT64 data_offset = 0, packet_count = 0, duration = 0, play = 0, preroll = 0;
    QWORD read_header_size = 0;
    WCHAR path[MAX_PATH];
    BYTE *pcm = pcm_data(&pcm_size), *got, *data;
    LONGLONG first_time = -1, last_time = -1, expect_last;
    WORD stream_number;
    HRESULT hr;

    printf("-- WMContainer: profile, ContentInfo, multiplexer -> file -> ParseHeader, splitter\n");
    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"asfmf-probe-mux.asf");
    MFCreateASFProfile(&profile);
    IMFASFProfile_CreateStream(profile, type, &stream);
    IMFASFStreamConfig_SetStreamNumber(stream, 1);
    IMFASFStreamConfig_SetBlob(stream, &MF_ASFSTREAMCONFIG_LEAKYBUCKET1, (UINT8 *)bucket, sizeof(bucket));
    IMFASFProfile_SetStream(profile, stream);
    IMFASFStreamConfig_Release(stream);
    MFCreateASFContentInfo(&info);
    hr = IMFASFContentInfo_SetProfile(info, profile);
    CHECK(hr == S_OK, "SetProfile %#lx\n", hr);
    MFCreateASFMultiplexer(&mux);
    hr = IMFASFMultiplexer_Initialize(mux, info);
    CHECK(hr == S_OK, "multiplexer Initialize %#lx\n", hr);
    IMFASFMultiplexer_SetFlags(mux, MFASF_MULTIPLEXER_AUTOADJUST_BITRATE);

    MFCreateTempFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, &file);
    chunk = RATE * CHANNELS * 2 * CHUNK_MS / 1000;
    for (i = 0; i * chunk < pcm_size; i++)
    {
        DWORD n = min(chunk, pcm_size - i * chunk);
        IMFSample *s = make_sample(pcm + i * chunk, n, (LONGLONG)i * CHUNK_MS * 10000, CHUNK_MS * 10000), *packet;
        hr = IMFASFMultiplexer_ProcessSample(mux, 1, s, 0);
        IMFSample_Release(s);
        if (FAILED(hr)) break;
        do
        {
            IMFASFMultiplexer_GetNextPacket(mux, &status, &packet);
            if (packet) { write_sample_to(file, packet); IMFSample_Release(packet); packets++; }
        } while (status & ASF_STATUSFLAGS_INCOMPLETE);
    }
    CHECK(hr == S_OK, "ProcessSample %#lx\n", hr);
    hr = IMFASFMultiplexer_Flush(mux);
    CHECK(hr == S_OK, "Flush %#lx\n", hr);
    do
    {
        IMFSample *packet;
        IMFASFMultiplexer_GetNextPacket(mux, &status, &packet);
        if (packet) { write_sample_to(file, packet); IMFSample_Release(packet); packets++; }
    } while (status & ASF_STATUSFLAGS_INCOMPLETE);
    hr = IMFASFMultiplexer_End(mux, info);
    CHECK(hr == S_OK, "End %#lx\n", hr);
    {
        ASF_MUX_STATISTICS stats = {0};
        IMFASFMultiplexer_GetStatistics(mux, 1, &stats);
        CHECK(stats.cFramesWritten == i, "statistics count the samples (%lu of %lu)\n", stats.cFramesWritten, i);
    }
    IMFASFMultiplexer_Release(mux);
    CHECK(packets > pcm_size / 3200 && packets < pcm_size / 2000, "%lu packets for %lu bytes (several payloads a packet)\n", packets, pcm_size);

    hr = IMFASFContentInfo_GenerateHeader(info, NULL, &header_size);
    CHECK(hr == S_OK && header_size > 100, "GenerateHeader(NULL) gives the size %lu\n", header_size);
    MFCreateMemoryBuffer(header_size, &header);
    size = 0;
    MFCreateMemoryBuffer(10, &buffer);
    CHECK(IMFASFContentInfo_GenerateHeader(info, buffer, &size) == MF_E_BUFFERTOOSMALL, "a small buffer: MF_E_BUFFERTOOSMALL\n");
    IMFMediaBuffer_Release(buffer);
    hr = IMFASFContentInfo_GenerateHeader(info, header, &size);
    CHECK(hr == S_OK && size == header_size, "GenerateHeader %#lx\n", hr);
    IMFASFContentInfo_GeneratePresentationDescriptor(info, &pd);
    IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_ASF_FILEPROPERTIES_PACKETS, &packet_count);
    IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_DURATION, &duration);
    CHECK(packet_count == packets, "MF_PD_ASF_FILEPROPERTIES_PACKETS %llu\n", (unsigned long long)packet_count);
    CHECK(duration == SECONDS * 10000000ull, "MF_PD_DURATION %llu\n", (unsigned long long)duration);
    IMFPresentationDescriptor_Release(pd);

    /* the file: the header, then the packets */
    {
        IMFByteStream *out;
        BYTE buf[4096]; ULONG got_bytes, written;
        DeleteFileW(path);
        MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, path, &out);
        IMFMediaBuffer_Lock(header, &data, NULL, &length);
        IMFByteStream_Write(out, data, length, &written);
        IMFMediaBuffer_Unlock(header);
        IMFByteStream_SetCurrentPosition(file, 0);
        while (SUCCEEDED(IMFByteStream_Read(file, buf, sizeof(buf), &got_bytes)) && got_bytes)
            IMFByteStream_Write(out, buf, got_bytes, &written);
        IMFByteStream_Close(out);
        IMFByteStream_Release(out);
    }
    IMFByteStream_Release(file);

    /* read back */
    MFCreateASFContentInfo(&read_info);
    hr = IMFASFContentInfo_GetHeaderSize(read_info, header, &read_header_size);
    CHECK(hr == S_OK && read_header_size + 50 == header_size, "GetHeaderSize %llu (the header object; the data object's header follows)\n",
          (unsigned long long)read_header_size);
    hr = IMFASFContentInfo_ParseHeader(read_info, header, 0);
    CHECK(hr == S_OK, "ParseHeader %#lx\n", hr);
    hr = IMFASFContentInfo_GetProfile(read_info, &read_profile);
    if (SUCCEEDED(hr))
    {
        UINT32 rate = 0, ch = 0, bits = 0;
        IMFASFProfile_GetStreamCount(read_profile, &status);
        hr = IMFASFProfile_GetStreamByNumber(read_profile, 1, &stream);
        CHECK(status == 1 && hr == S_OK, "the parsed profile has stream 1\n");
        if (SUCCEEDED(hr) && SUCCEEDED(IMFASFStreamConfig_GetMediaType(stream, &read_type)))
        {
            GUID sub = {0};
            IMFMediaType_GetGUID(read_type, &MF_MT_SUBTYPE, &sub);
            IMFMediaType_GetUINT32(read_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
            IMFMediaType_GetUINT32(read_type, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
            IMFMediaType_GetUINT32(read_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
            CHECK(IsEqualGUID(&sub, &MFAudioFormat_PCM) && rate == RATE && ch == CHANNELS && bits == 16,
                  "its type is the stream's (%u Hz, %u ch, %u bits)\n", rate, ch, bits);
            IMFMediaType_Release(read_type);
        }
        if (SUCCEEDED(hr)) IMFASFStreamConfig_Release(stream);
        IMFASFProfile_Release(read_profile);
    }
    IMFASFContentInfo_GeneratePresentationDescriptor(read_info, &pd);
    IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_ASF_DATA_START_OFFSET, &data_offset);
    IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_ASF_FILEPROPERTIES_PACKETS, &packet_count);
    IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_ASF_FILEPROPERTIES_PLAY_DURATION, &play);
    IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_ASF_FILEPROPERTIES_PREROLL, &preroll);
    IMFPresentationDescriptor_Release(pd);
    CHECK(data_offset == header_size, "MF_PD_ASF_DATA_START_OFFSET %llu\n", (unsigned long long)data_offset);
    CHECK(packet_count == packets, "the parsed packet count %llu\n", (unsigned long long)packet_count);
    CHECK(play == SECONDS * 10000000ull + preroll * 10000, "play duration %llu = duration + preroll %llu ms\n",
          (unsigned long long)play, (unsigned long long)preroll);

    MFCreateASFSplitter(&splitter);
    hr = IMFASFSplitter_Initialize(splitter, read_info);
    CHECK(hr == S_OK, "splitter Initialize %#lx\n", hr);
    stream_number = 1;
    hr = IMFASFSplitter_SelectStreams(splitter, &stream_number, 1);
    CHECK(hr == S_OK, "SelectStreams %#lx\n", hr);
    CHECK(IMFASFSplitter_SelectStreams(splitter, (stream_number = 9, &stream_number), 1) == MF_E_INVALIDSTREAMNUMBER,
          "an unknown stream cannot be selected\n");
    stream_number = 1;
    IMFASFSplitter_SelectStreams(splitter, &stream_number, 1);
    got = malloc(pcm_size + 65536);
    length = 0;
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        DWORD file_size = GetFileSize(h, NULL), read_bytes, n;
        BYTE *all = malloc(file_size);
        ReadFile(h, all, file_size, &read_bytes, NULL);
        CloseHandle(h);
        CHECK(file_size == header_size + packets * 3200, "file size %lu = header + packets\n", file_size);
        /* fed in pieces that split packets, as a reader's buffers do */
        for (n = (DWORD)data_offset; n < file_size; n += 5000)
        {
            DWORD piece = min(5000, file_size - n);
            IMFMediaBuffer *b;
            IMFSample *s;
            BYTE *p;
            MFCreateMemoryBuffer(piece, &b);
            IMFMediaBuffer_Lock(b, &p, NULL, NULL);
            memcpy(p, all + n, piece);
            IMFMediaBuffer_Unlock(b);
            IMFMediaBuffer_SetCurrentLength(b, piece);
            hr = IMFASFSplitter_ParseData(splitter, b, 0, 0);
            IMFMediaBuffer_Release(b);
            if (FAILED(hr)) break;
            do
            {
                WORD num;
                IMFASFSplitter_GetNextSample(splitter, &status, &num, &s);
                if (s)
                {
                    IMFMediaBuffer *sb; BYTE *sp; DWORD sl; LONGLONG t = -1;
                    IMFSample_GetSampleTime(s, &t);
                    if (first_time < 0) first_time = t;
                    last_time = t;
                    IMFSample_ConvertToContiguousBuffer(s, &sb);
                    IMFMediaBuffer_Lock(sb, &sp, NULL, &sl);
                    if (length + sl <= pcm_size + 65536) memcpy(got + length, sp, sl);
                    length += sl;
                    IMFMediaBuffer_Unlock(sb);
                    IMFMediaBuffer_Release(sb);
                    IMFSample_Release(s);
                }
            } while (status & ASF_STATUSFLAGS_INCOMPLETE);
        }
        free(all);
    }
    CHECK(hr == S_OK, "ParseData %#lx\n", hr);
    CHECK(length == pcm_size && !memcmp(got, pcm, pcm_size), "the splitter gives the samples back byte for byte (%lu)\n", length);
    expect_last = (LONGLONG)((pcm_size + chunk - 1) / chunk - 1) * CHUNK_MS * 10000;
    CHECK(first_time == 0 && last_time == expect_last, "with their times (%lld .. %lld)\n", (long long)first_time, (long long)last_time);
    free(got);
    IMFASFSplitter_Release(splitter);
    IMFASFContentInfo_Release(read_info);
    IMFMediaBuffer_Release(header);
    IMFASFContentInfo_Release(info);
    IMFASFProfile_Release(profile);
    IMFMediaType_Release(type);

    check_file_audio(path, pcm, pcm_size, "WMContainer file");
    free(pcm);
    DeleteFileW(path);
}

/* the ASF file sink behind a sink writer, as a program writing .wma/.asf does */
static void test_sink_writer(void)
{
    IMFSinkWriter *writer;
    IMFMediaType *type = pcm_type();
    DWORD stream, pcm_size, chunk, i;
    WCHAR path[MAX_PATH];
    BYTE *pcm = pcm_data(&pcm_size);
    HRESULT hr;

    printf("-- sink writer (.asf) -> the ASF file sink\n");
    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"asfmf-probe-writer.asf");
    DeleteFileW(path);
    hr = MFCreateSinkWriterFromURL(path, NULL, NULL, &writer);
    CHECK(hr == S_OK, "MFCreateSinkWriterFromURL(.asf) %#lx\n", hr);
    if (FAILED(hr)) { IMFMediaType_Release(type); free(pcm); return; }
    hr = IMFSinkWriter_AddStream(writer, type, &stream);
    CHECK(hr == S_OK, "AddStream %#lx\n", hr);
    hr = IMFSinkWriter_SetInputMediaType(writer, stream, type, NULL);
    CHECK(hr == S_OK, "SetInputMediaType %#lx\n", hr);
    hr = IMFSinkWriter_BeginWriting(writer);
    CHECK(hr == S_OK, "BeginWriting %#lx\n", hr);
    chunk = RATE * CHANNELS * 2 * CHUNK_MS / 1000;
    for (i = 0; SUCCEEDED(hr) && i * chunk < pcm_size; i++)
    {
        IMFSample *s = make_sample(pcm + i * chunk, min(chunk, pcm_size - i * chunk), (LONGLONG)i * CHUNK_MS * 10000, CHUNK_MS * 10000);
        hr = IMFSinkWriter_WriteSample(writer, stream, s);
        IMFSample_Release(s);
    }
    CHECK(hr == S_OK, "WriteSample %#lx\n", hr);
    hr = IMFSinkWriter_Finalize(writer);
    CHECK(hr == S_OK, "Finalize %#lx\n", hr);
    IMFSinkWriter_Release(writer);
    IMFMediaType_Release(type);

    /* the finished file's header: packet count and duration filled in */
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        BYTE head[4096]; DWORD read_bytes = 0;
        IMFASFContentInfo *info; IMFMediaBuffer *b; BYTE *p; IMFPresentationDescriptor *pd;
        UINT64 packets = 0, duration = 0;
        ReadFile(h, head, sizeof(head), &read_bytes, NULL);
        CloseHandle(h);
        MFCreateMemoryBuffer(read_bytes, &b);
        IMFMediaBuffer_Lock(b, &p, NULL, NULL); memcpy(p, head, read_bytes); IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_SetCurrentLength(b, read_bytes);
        MFCreateASFContentInfo(&info);
        hr = IMFASFContentInfo_ParseHeader(info, b, 0);
        if (SUCCEEDED(hr) && SUCCEEDED(IMFASFContentInfo_GeneratePresentationDescriptor(info, &pd)))
        {
            IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_ASF_FILEPROPERTIES_PACKETS, &packets);
            IMFPresentationDescriptor_GetUINT64(pd, &MF_PD_DURATION, &duration);
            IMFPresentationDescriptor_Release(pd);
        }
        CHECK(hr == S_OK && packets > 0 && duration == SECONDS * 10000000ull,
              "Finalize wrote the header again: %llu packets, duration %llu\n", (unsigned long long)packets, (unsigned long long)duration);
        IMFASFContentInfo_Release(info);
        IMFMediaBuffer_Release(b);
    }
    check_file_audio(path, pcm, pcm_size, "sink writer file");
    free(pcm);
    DeleteFileW(path);
}

/* MFCreateASFMediaSink: the sink is its own ContentInfo; SetProfile makes its stream sinks */
static void test_media_sink(void)
{
    IMFByteStream *bs;
    IMFMediaSink *sink;
    IMFASFContentInfo *info;
    IMFASFProfile *profile;
    IMFASFStreamConfig *stream;
    IMFStreamSink *ss;
    IMFMediaType *type = pcm_type();
    IMFActivate *activate;
    DWORD count = 0, id = 0;
    HRESULT hr;

    printf("-- MFCreateASFMediaSink / Activate\n");
    MFCreateTempFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, &bs);
    hr = MFCreateASFMediaSink(bs, &sink);
    CHECK(hr == S_OK, "MFCreateASFMediaSink %#lx\n", hr);
    if (FAILED(hr)) { IMFByteStream_Release(bs); return; }
    hr = IMFMediaSink_QueryInterface(sink, &IID_IMFASFContentInfo, (void **)&info);
    CHECK(hr == S_OK, "the sink is an IMFASFContentInfo %#lx\n", hr);
    MFCreateASFProfile(&profile);
    IMFASFProfile_CreateStream(profile, type, &stream);
    IMFASFStreamConfig_SetStreamNumber(stream, 2);
    IMFASFProfile_SetStream(profile, stream);
    IMFASFStreamConfig_SetStreamNumber(stream, 5);
    IMFASFProfile_SetStream(profile, stream);
    IMFASFStreamConfig_Release(stream);
    hr = SUCCEEDED(hr) ? IMFASFContentInfo_SetProfile(info, profile) : hr;
    CHECK(hr == S_OK, "SetProfile on the sink %#lx\n", hr);
    IMFMediaSink_GetStreamSinkCount(sink, &count);
    CHECK(count == 2, "a stream sink per profile stream (%lu)\n", count);
    if (SUCCEEDED(IMFMediaSink_GetStreamSinkByIndex(sink, 1, &ss)))
    {
        IMFStreamSink_GetIdentifier(ss, &id);
        IMFStreamSink_Release(ss);
    }
    CHECK(id == 5, "identified by ASF stream number (%lu)\n", id);
    if (SUCCEEDED(hr)) IMFASFContentInfo_Release(info);
    IMFMediaSink_Shutdown(sink);
    IMFMediaSink_Release(sink);
    IMFByteStream_Release(bs);

    MFCreateASFContentInfo(&info);
    IMFASFContentInfo_SetProfile(info, profile);
    hr = MFCreateASFMediaSinkActivate(L"asfmf-probe-activate.asf", info, &activate);
    CHECK(hr == S_OK, "MFCreateASFMediaSinkActivate %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IMFActivate_ActivateObject(activate, &IID_IMFMediaSink, (void **)&sink);
        CHECK(hr == S_OK, "it activates a sink %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            count = 0;
            IMFMediaSink_GetStreamSinkCount(sink, &count);
            CHECK(count == 2, "of the ContentInfo's streams (%lu)\n", count);
            IMFMediaSink_Release(sink);
        }
        IMFActivate_ShutdownObject(activate);
        IMFActivate_Release(activate);
    }
    DeleteFileW(L"asfmf-probe-activate.asf");
    IMFASFContentInfo_Release(info);
    IMFASFProfile_Release(profile);
    IMFMediaType_Release(type);
}

int main(void)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    test_profile();
    test_wmcontainer_roundtrip();
    test_sink_writer();
    test_media_sink();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
