/* wmcodecinfo-probe: the profile manager's codec information (patches/sg/9105).
 *
 * IWMCodecInfo was not there (E_NOINTERFACE), so a program that builds its profile from the
 * installed codecs -- fre:ac's WMA encoder: GetCodecInfoCount, GetCodecName, codec enumeration
 * settings, GetCodecFormat, then a writer -- had nothing to write with. Walks the audio and video
 * codecs as fre:ac does, then encodes one second of PCM with EVERY audio format the list gives
 * (an empty profile plus that stream, the writer, SetOutputFilename) and reads each file back:
 * a format the list offers must be one the writer can write.
 *
 * Prints name=value lines; see test/wmcodecinfo-gate.sh.
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
#include <math.h>

DEFINE_GUID(p_WMMEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMFORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

static HRESULT (WINAPI *pWMCreateProfileManager)(IWMProfileManager **);
static HRESULT (WINAPI *pWMCreateWriter)(IUnknown *, IWMWriter **);
static HRESULT (WINAPI *pWMCreateSyncReader)(IUnknown *, DWORD, IWMSyncReader **);

static void say(const char *name, int ok, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s=%d %s\n", name, ok ? 1 : 0, buf);
    fflush(stdout);
}

static const WAVEFORMATEX *config_wfx(IWMStreamConfig *config, BYTE *buffer, DWORD size)
{
    IWMMediaProps *props;
    WM_MEDIA_TYPE *mt = (WM_MEDIA_TYPE *)buffer;
    HRESULT hr;

    if (FAILED(IWMStreamConfig_QueryInterface(config, &IID_IWMMediaProps, (void **)&props))) return NULL;
    hr = IWMMediaProps_GetMediaType(props, mt, &size);
    IWMMediaProps_Release(props);
    if (FAILED(hr) || !IsEqualGUID(&mt->majortype, &p_WMMEDIATYPE_Audio)
            || !IsEqualGUID(&mt->formattype, &p_WMFORMAT_WaveFormatEx) || mt->cbFormat < 16)
        return NULL;
    /* a copy of the format of its own: the media type's may live in the object */
    {
        static BYTE copy[256];
        memcpy(copy, mt->pbFormat, min(mt->cbFormat, sizeof(copy)));
        return (const WAVEFORMATEX *)copy;
    }
}

/* one second of a tone in that format, written and read back; the number of PCM bytes back */
static DWORD encode(IWMProfileManager *manager, IWMStreamConfig *config, const WAVEFORMATEX *wfx, DWORD index)
{
    WAVEFORMATEX pcm = {WAVE_FORMAT_PCM, wfx->nChannels, wfx->nSamplesPerSec,
            wfx->nSamplesPerSec * wfx->nChannels * 2, wfx->nChannels * 2, 16, 0};
    DWORD chunk = pcm.nAvgBytesPerSec / 10, c, i, back = 0;
    WM_MEDIA_TYPE mt = {0};
    IWMInputMediaProps *input;
    IWMSyncReader *reader;
    IWMProfile *profile;
    IWMWriter *writer;
    WCHAR path[64];
    HRESULT hr;

    swprintf(path, 64, L"C:\\format%lu.wma", index);
    if (FAILED(IWMProfileManager_CreateEmptyProfile(manager, WMT_VER_9_0, &profile))) return 0;
    IWMStreamConfig_SetStreamNumber(config, 1);
    IWMStreamConfig_SetStreamName(config, (WCHAR *)L"Audio");
    IWMStreamConfig_SetConnectionName(config, (WCHAR *)L"Audio");
    hr = IWMProfile_AddStream(profile, config);
    if (FAILED(hr) || FAILED(pWMCreateWriter(NULL, &writer))) { IWMProfile_Release(profile); return 0; }
    hr = IWMWriter_SetProfile(writer, profile);
    IWMProfile_Release(profile);
    if (SUCCEEDED(hr) && SUCCEEDED(hr = IWMWriter_GetInputProps(writer, 0, &input)))
    {
        mt.majortype = p_WMMEDIATYPE_Audio;
        mt.subtype = p_WMMEDIASUBTYPE_PCM;
        mt.bFixedSizeSamples = TRUE;
        mt.lSampleSize = pcm.nBlockAlign;
        mt.formattype = p_WMFORMAT_WaveFormatEx;
        mt.cbFormat = sizeof(pcm);
        mt.pbFormat = (BYTE *)&pcm;
        IWMInputMediaProps_SetMediaType(input, &mt);
        hr = IWMWriter_SetInputProps(writer, 0, input);
        IWMInputMediaProps_Release(input);
    }
    if (SUCCEEDED(hr)) hr = IWMWriter_SetOutputFilename(writer, path);
    if (SUCCEEDED(hr)) hr = IWMWriter_BeginWriting(writer);
    for (c = 0; c < 10 && SUCCEEDED(hr); c++)
    {
        INSSBuffer *sample;
        BYTE *data;
        DWORD len;
        if (FAILED(hr = IWMWriter_AllocateSample(writer, chunk, &sample))) break;
        INSSBuffer_GetBufferAndLength(sample, &data, &len);
        for (i = 0; i < chunk / 2; i++)
        {
            short v = (short)(6000 * sin((c * chunk / 2 + i) / pcm.nChannels * 2 * 3.14159265 * 440 / pcm.nSamplesPerSec));
            memcpy(data + i * 2, &v, 2);
        }
        INSSBuffer_SetLength(sample, chunk);
        hr = IWMWriter_WriteSample(writer, 0, (QWORD)c * 1000000, 0, sample);
        INSSBuffer_Release(sample);
    }
    if (SUCCEEDED(hr)) hr = IWMWriter_EndWriting(writer);
    IWMWriter_Release(writer);
    if (FAILED(hr))
    {
        printf("      format %lu: writing failed, hr %#lx\n", index, hr);
        return 0;
    }

    if (SUCCEEDED(pWMCreateSyncReader(NULL, 0, &reader)))
    {
        if (SUCCEEDED(IWMSyncReader_Open(reader, path)))
        {
            QWORD time, dur;
            DWORD flags, output;
            WORD stream;
            INSSBuffer *sample;
            while (IWMSyncReader_GetNextSample(reader, 0, &sample, &time, &dur, &flags, &output, &stream) == S_OK)
            {
                DWORD len = 0;
                INSSBuffer_GetLength(sample, &len);
                back += len;
                INSSBuffer_Release(sample);
            }
            IWMSyncReader_Close(reader);
        }
        IWMSyncReader_Release(reader);
    }
    DeleteFileW(path);
    return back;
}

int main(void)
{
    BOOL vbr_supported = TRUE, false_value = FALSE, true_value = TRUE;
    DWORD one = 1, two = 2, count = 0, formats = 0, vbr_formats = 99, two_pass = 99, size, i, ok = 0, codec;
    IWMProfileManager *manager;
    IWMCodecInfo3 *info;
    WMT_ATTR_DATATYPE type;
    WCHAR name[256], desc[256];
    HMODULE wmvcore;
    HRESULT hr;
    BYTE buffer[1024];

    CoInitialize(NULL);
    wmvcore = LoadLibraryA("wmvcore.dll");
    pWMCreateProfileManager = (void *)GetProcAddress(wmvcore, "WMCreateProfileManager");
    pWMCreateWriter = (void *)GetProcAddress(wmvcore, "WMCreateWriter");
    pWMCreateSyncReader = (void *)GetProcAddress(wmvcore, "WMCreateSyncReader");

    pWMCreateProfileManager(&manager);
    hr = IWMProfileManager_QueryInterface(manager, &IID_IWMCodecInfo3, (void **)&info);
    say("interface", hr == S_OK, "IWMCodecInfo3 hr %#lx", hr);
    if (FAILED(hr)) { printf("done=1\n"); return 0; }
    {
        IWMCodecInfo *info1;
        IUnknown *unk;
        HRESULT hr1 = IWMCodecInfo3_QueryInterface(info, &IID_IWMCodecInfo, (void **)&info1);
        HRESULT hr2 = IWMCodecInfo3_QueryInterface(info, &IID_IWMProfileManager, (void **)&unk);
        if (SUCCEEDED(hr1)) IWMCodecInfo_Release(info1);
        if (SUCCEEDED(hr2)) IUnknown_Release(unk);
        say("same_object", hr1 == S_OK && hr2 == S_OK, "IWMCodecInfo %#lx, back to the manager %#lx", hr1, hr2);
    }

    /* fre:ac's GetDefaultCodec: a "Windows Media Audio" codec that is not Voice, Lossless or Pro */
    hr = IWMCodecInfo3_GetCodecInfoCount(info, &p_WMMEDIATYPE_Audio, &count);
    codec = ~0u;
    for (i = 0; i < count; i++)
    {
        size = 0;
        IWMCodecInfo3_GetCodecName(info, &p_WMMEDIATYPE_Audio, i, NULL, &size);
        size = ARRAYSIZE(name);
        IWMCodecInfo3_GetCodecName(info, &p_WMMEDIATYPE_Audio, i, name, &size);
        printf("      audio codec %lu: %ls\n", i, name);
        if (wcsstr(name, L"Windows Media Audio") && !wcsstr(name, L"Voice") && !wcsstr(name, L"Lossless")
                && !wcsstr(name, L"Pro"))
            codec = i;
    }
    say("audio_codec", hr == S_OK && codec != ~0u, "%lu audio codecs, standard WMA at %ld", count, (long)codec);
    if (codec == ~0u) { printf("done=1\n"); return 0; }

    size = sizeof(vbr_supported);
    hr = IWMCodecInfo3_GetCodecProp(info, &p_WMMEDIATYPE_Audio, codec, L"_ISVBRSUPPORTED", &type,
            (BYTE *)&vbr_supported, &size);
    IWMCodecInfo3_SetCodecEnumerationSetting(info, &p_WMMEDIATYPE_Audio, codec, L"_VBRENABLED", WMT_TYPE_BOOL,
            (BYTE *)&true_value, sizeof(BOOL));
    IWMCodecInfo3_GetCodecFormatCount(info, &p_WMMEDIATYPE_Audio, codec, &vbr_formats);
    IWMCodecInfo3_SetCodecEnumerationSetting(info, &p_WMMEDIATYPE_Audio, codec, L"_VBRENABLED", WMT_TYPE_BOOL,
            (BYTE *)&false_value, sizeof(BOOL));
    IWMCodecInfo3_SetCodecEnumerationSetting(info, &p_WMMEDIATYPE_Audio, codec, L"_PASSESUSED", WMT_TYPE_DWORD,
            (BYTE *)&two, sizeof(DWORD));
    IWMCodecInfo3_GetCodecFormatCount(info, &p_WMMEDIATYPE_Audio, codec, &two_pass);
    IWMCodecInfo3_SetCodecEnumerationSetting(info, &p_WMMEDIATYPE_Audio, codec, L"_PASSESUSED", WMT_TYPE_DWORD,
            (BYTE *)&one, sizeof(DWORD));
    /* the VBR answer and the VBR list agree: what the codec says it has, it lists */
    say("vbr_consistent", hr == S_OK && type == WMT_TYPE_BOOL && (vbr_supported ? vbr_formats > 0 : vbr_formats == 0),
            "_ISVBRSUPPORTED %d (hr %#lx), VBR formats %lu, two-pass formats %lu", vbr_supported, hr, vbr_formats, two_pass);

    hr = IWMCodecInfo3_GetCodecFormatCount(info, &p_WMMEDIATYPE_Audio, codec, &formats);
    say("cbr_formats", hr == S_OK && formats > 0, "%lu one-pass CBR formats", formats);
    {
        BOOL found128 = FALSE;
        for (i = 0; i < formats; i++)
        {
            IWMStreamConfig *config;
            const WAVEFORMATEX *wfx;
            DWORD bitrate = 0, back;
            size = ARRAYSIZE(desc);
            if (FAILED(hr = IWMCodecInfo3_GetCodecFormatDesc(info, &p_WMMEDIATYPE_Audio, codec, i, &config, desc, &size)))
            {
                printf("      format %lu: GetCodecFormatDesc hr %#lx\n", i, hr);
                continue;
            }
            IWMStreamConfig_GetBitrate(config, &bitrate);
            if (!(wfx = config_wfx(config, buffer, sizeof(buffer))))
            {
                printf("      format %lu (%ls): no audio media type\n", i, desc);
                IWMStreamConfig_Release(config);
                continue;
            }
            if (wfx->nSamplesPerSec == 44100 && wfx->nChannels == 2 && bitrate == 128000) found128 = TRUE;
            back = encode(manager, config, wfx, i);
            printf("      format %lu: %ls (tag %#x, %lu Hz, %u ch, %lu bps): %lu PCM bytes back\n", i, desc,
                    wfx->wFormatTag, wfx->nSamplesPerSec, wfx->nChannels, bitrate, back);
            /* nearly the second that went in (decoders hold back a frame or so) */
            if (back >= wfx->nSamplesPerSec * wfx->nChannels * 2 * 8 / 10) ok++;
            IWMStreamConfig_Release(config);
        }
        say("cd_format", found128, "44.1 kHz stereo 128 kbps is offered (fre:ac's default)");
        say("all_encode", formats && ok == formats, "%lu of %lu formats encode and read back", ok, formats);
    }

    hr = IWMCodecInfo3_GetCodecInfoCount(info, &p_WMMEDIATYPE_Video, &count);
    {
        IWMStreamConfig *config = NULL;
        HRESULT hr2 = E_FAIL;
        size = ARRAYSIZE(name);
        name[0] = 0;
        if (count)
        {
            IWMCodecInfo3_GetCodecName(info, &p_WMMEDIATYPE_Video, 0, name, &size);
            hr2 = IWMCodecInfo3_GetCodecFormat(info, &p_WMMEDIATYPE_Video, 0, 0, &config);
        }
        say("video_codec", hr == S_OK && count && hr2 == S_OK && wcsstr(name, L"Windows Media Video"),
                "%lu video codecs, first %ls, format hr %#lx", count, name, hr2);
        if (config) IWMStreamConfig_Release(config);
    }
    hr = IWMCodecInfo3_GetCodecFormatCount(info, &p_WMMEDIATYPE_Audio, 5, &count);
    say("bad_index", hr == E_INVALIDARG, "codec index 5: hr %#lx", hr);

    IWMCodecInfo3_Release(info);
    IWMProfileManager_Release(manager);
    printf("done=1\n");
    return 0;
}
