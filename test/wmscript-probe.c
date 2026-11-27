/* wmscript-probe: a script stream written by the WM writer (patches/sg/9261). A profile with a
 * script stream (WMMEDIATYPE_Script: captions, URLs, markers as type/command string pairs) could
 * not be written at all: BeginWriting failed with NS_E_INVALID_STREAM. Windows' writer writes it
 * as an ASF command media stream. The probe writes a file with a script stream only (no encoder
 * involved), two commands, and reads the file back. Prints key=1|0 lines. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#include <stdio.h>
#include <string.h>

static const GUID MT_Script = {0x73636d64, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID ST_TwoStrings = {0x82f38a70, 0xc29f, 0x11d1, {0x97, 0xad, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50}};
static const GUID FT_Script = {0x5c8510f2, 0xdebe, 0x4ca7, {0xbb, 0xa5, 0xf0, 0x7a, 0x10, 0x4f, 0x8d, 0xff}};
static const BYTE command_media[16] = {0xc0, 0xcf, 0xda, 0x59, 0xe6, 0x59, 0xd0, 0x11, 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6};
static const BYTE stream_props[16] = {0x91, 0x07, 0xdc, 0xb7, 0xb7, 0xa9, 0xcf, 0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
static const BYTE data_object[16] = {0x36, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};

static BYTE *find(BYTE *hay, DWORD n, const void *needle, DWORD m)
{
    DWORD i;
    for (i = 0; i + m <= n; i++) if (!memcmp(hay + i, needle, m)) return hay + i;
    return NULL;
}

static HRESULT write_command(IWMWriter *writer, QWORD time, const WCHAR *type, const WCHAR *command)
{
    DWORD a = (wcslen(type) + 1) * sizeof(WCHAR), b = (wcslen(command) + 1) * sizeof(WCHAR);
    INSSBuffer *buf; BYTE *data; HRESULT hr;

    if (FAILED(hr = IWMWriter_AllocateSample(writer, a + b, &buf))) return hr;
    INSSBuffer_GetBuffer(buf, &data);
    memcpy(data, type, a); memcpy(data + a, command, b);
    INSSBuffer_SetLength(buf, a + b);
    hr = IWMWriter_WriteSample(writer, 0, time, WM_SF_CLEANPOINT, buf);
    INSSBuffer_Release(buf);
    return hr;
}

int main(void)
{
    HRESULT (WINAPI *create_mgr)(IWMProfileManager **);
    HRESULT (WINAPI *create_writer)(IUnknown *, IWMWriter **);
    HMODULE wmvcore = LoadLibraryA("wmvcore.dll");
    IWMProfileManager *mgr; IWMProfile *profile; IWMStreamConfig *config; IWMMediaProps *props;
    IWMWriter *writer; IWMInputMediaProps *input;
    WM_MEDIA_TYPE mt = {0}; struct { GUID scriptType; } format;
    HANDLE file; BYTE *bytes, *p, *dataobj; DWORD size, got; HRESULT hr;
    static const WCHAR path[] = L"C:\\wmscript.wmv";
    int stream_ok = 0;

    create_mgr = (void *)GetProcAddress(wmvcore, "WMCreateProfileManager");
    create_writer = (void *)GetProcAddress(wmvcore, "WMCreateWriter");
    if (!create_mgr || !create_writer || FAILED(create_mgr(&mgr)) || FAILED(create_writer(NULL, &writer)))
    {
        printf("created=0\n");
        return 1;
    }
    printf("created=1\n");
    IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_9_0, &profile);
    hr = IWMProfile_CreateNewStream(profile, &MT_Script, &config);
    printf("new_stream=%d hr %#lx\n", hr == S_OK, hr);
    if (FAILED(hr)) return 1;
    format.scriptType = ST_TwoStrings;
    mt.majortype = MT_Script; mt.subtype = ST_TwoStrings; mt.formattype = FT_Script;
    mt.cbFormat = sizeof(format); mt.pbFormat = (BYTE *)&format;
    IWMStreamConfig_QueryInterface(config, &IID_IWMMediaProps, (void **)&props);
    IWMMediaProps_SetMediaType(props, &mt);
    IWMMediaProps_Release(props);
    IWMStreamConfig_SetStreamNumber(config, 1);
    IWMStreamConfig_SetConnectionName(config, L"Script");
    IWMStreamConfig_SetBitrate(config, 2000);
    IWMStreamConfig_SetBufferWindow(config, 3000);
    hr = IWMProfile_AddStream(profile, config);
    IWMStreamConfig_Release(config);
    printf("add_stream=%d hr %#lx\n", hr == S_OK, hr);

    hr = IWMWriter_SetProfile(writer, profile);
    printf("set_profile=%d hr %#lx\n", hr == S_OK, hr);
    IWMWriter_SetOutputFilename(writer, path);
    if (SUCCEEDED(IWMWriter_GetInputFormat(writer, 0, 0, &input)))
    {
        IWMWriter_SetInputProps(writer, 0, input);
        IWMInputMediaProps_Release(input);
    }
    hr = IWMWriter_BeginWriting(writer);
    printf("begin=%d hr %#lx\n", hr == S_OK, hr);
    if (FAILED(hr)) return 1;
    hr = write_command(writer, 0, L"URL", L"https://example.org/first");
    printf("write1=%d hr %#lx\n", hr == S_OK, hr);
    hr = write_command(writer, 20000000, L"TEXT", L"SG-SECOND-COMMAND");
    printf("write2=%d hr %#lx\n", hr == S_OK, hr);
    hr = IWMWriter_EndWriting(writer);
    printf("end=%d hr %#lx\n", hr == S_OK, hr);
    IWMWriter_Release(writer);

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    size = GetFileSize(file, NULL);
    bytes = malloc(size ? size : 1);
    ReadFile(file, bytes, size, &got, NULL);
    CloseHandle(file);
    printf("file=%d size %lu\n", file != INVALID_HANDLE_VALUE && got == size && size > 0, size);
    /* the stream properties object names command media for stream 1 */
    p = bytes;
    while ((p = find(p, size - (p - bytes), stream_props, 16)))
    {
        if (p + 78 <= bytes + size && !memcmp(p + 24, command_media, 16) && (*(WORD *)(p + 72) & 0x7f) == 1) stream_ok = 1;
        p += 16;
    }
    printf("command_media=%d\n", stream_ok);
    dataobj = find(bytes, size, data_object, 16);
    printf("commands_in_data=%d\n", dataobj && find(dataobj, size - (dataobj - bytes), L"https://example.org/first", 50)
            && find(dataobj, size - (dataobj - bytes), L"SG-SECOND-COMMAND", 34));
    return 0;
}
