/* winegstreamer's wm_reader.c profile / stream configuration stubs
 * (patches/sg/2860), run by test/wgst-wmreader-gate.sh:
 * IWMOutputMediaProps::Get{StreamGroup,Connection}Name, INSSBuffer::GetLength,
 * IWMStreamConfig, IWMMediaProps::GetType/SetMediaType and the IWMProfile of a
 * sync reader (name, description, storage format, stream add / remove /
 * reconfigure / create, mutual exclusions, bandwidth sharing, stream
 * prioritization, expected packet count).
 *
 *   wgst-wmreader-probe.exe FILE.wmv */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
#ifndef NS_E_NO_STREAM
#define NS_E_NO_STREAM ((HRESULT)0xc00d0033)
#endif
#ifndef NS_E_INVALID_STREAM
#define NS_E_INVALID_STREAM ((HRESULT)0xc00d003c)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECKHR(hr, exp, what) CHECKF((hr) == (HRESULT)(exp), "%s: hr %#lx (want %#lx)", what, (unsigned long)(hr), (unsigned long)(HRESULT)(exp))

static HRESULT (WINAPI *pWMCreateSyncReader)(IUnknown *, DWORD, IWMSyncReader **);
static const WCHAR *path;

static IWMSyncReader *open_reader(void)
{
    IWMSyncReader *reader = NULL;
    HRESULT hr = pWMCreateSyncReader(NULL, 0, &reader);
    if (hr == S_OK) hr = IWMSyncReader_Open(reader, path);
    if (hr != S_OK) { printf("FAIL  cannot open the reader: %#lx\n", (unsigned long)hr); exit(1); }
    return reader;
}

/* ---- string properties, table driven ---------------------------------- */
typedef struct { void *obj; } objref;
struct strprop
{
    const char *name;
    HRESULT (*get)(void *obj, WCHAR *buf, DWORD *len);
    HRESULT (*set)(void *obj, const WCHAR *str);
    void *obj;
    int has_default_empty;
};
static HRESULT sn_get(void *o, WCHAR *b, DWORD *l) { WORD w = l ? *l : 0; HRESULT hr = IWMStreamConfig_GetStreamName((IWMStreamConfig *)o, b, l ? &w : NULL); if (l) *l = w; return hr; }
static HRESULT sn_set(void *o, const WCHAR *s) { return IWMStreamConfig_SetStreamName((IWMStreamConfig *)o, s); }
static HRESULT cn_get(void *o, WCHAR *b, DWORD *l) { WORD w = l ? *l : 0; HRESULT hr = IWMStreamConfig_GetConnectionName((IWMStreamConfig *)o, b, l ? &w : NULL); if (l) *l = w; return hr; }
static HRESULT cn_set(void *o, const WCHAR *s) { return IWMStreamConfig_SetConnectionName((IWMStreamConfig *)o, s); }
static HRESULT pn_get(void *o, WCHAR *b, DWORD *l) { return IWMProfile3_GetName((IWMProfile3 *)o, b, l); }
static HRESULT pn_set(void *o, const WCHAR *s) { return IWMProfile3_SetName((IWMProfile3 *)o, s); }
static HRESULT pd_get(void *o, WCHAR *b, DWORD *l) { return IWMProfile3_GetDescription((IWMProfile3 *)o, b, l); }
static HRESULT pd_set(void *o, const WCHAR *s) { return IWMProfile3_SetDescription((IWMProfile3 *)o, s); }

static void test_string_prop(const struct strprop *p)
{
    static const WCHAR hello[] = L"Hello stream", longer[] = L"A much longer name than before", empty[] = L"";
    WCHAR buf[64];
    DWORD len;
    HRESULT hr;

    len = 0xdead;
    hr = p->get(p->obj, NULL, &len);
    CHECKF(hr == S_OK && len == 1, "%s: default is empty, NULL buffer reports 1 (hr %#lx len %lu)", p->name, (unsigned long)hr, len);
    len = 1; memset(buf, 0xcc, sizeof(buf));
    hr = p->get(p->obj, buf, &len);
    CHECKF(hr == S_OK && len == 1 && !buf[0], "%s: default reads as empty string", p->name);

    hr = p->set(p->obj, NULL);
    CHECKHR(hr, E_INVALIDARG, p->name);
    hr = p->set(p->obj, hello);
    CHECKHR(hr, S_OK, p->name);
    len = 0;
    hr = p->get(p->obj, NULL, &len);
    CHECKF(hr == S_OK && len == ARRAY_SIZE(hello), "%s: NULL buffer returns length incl. NUL (%lu)", p->name, len);
    len = ARRAY_SIZE(hello) - 1; memset(buf, 0xcc, sizeof(buf));
    hr = p->get(p->obj, buf, &len);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && len == ARRAY_SIZE(hello) && buf[0] == 0xcccc,
            "%s: short buffer -> ASF_E_BUFFERTOOSMALL and the needed length (hr %#lx len %lu)", p->name, (unsigned long)hr, len);
    len = ARRAY_SIZE(buf);
    hr = p->get(p->obj, buf, &len);
    CHECKF(hr == S_OK && len == ARRAY_SIZE(hello) && !wcscmp(buf, hello), "%s: round trips", p->name);
    hr = p->get(p->obj, buf, NULL);
    CHECKHR(hr, E_INVALIDARG, p->name);

    hr = p->set(p->obj, longer);
    len = ARRAY_SIZE(buf);
    hr = p->get(p->obj, buf, &len);
    CHECKF(hr == S_OK && !wcscmp(buf, longer), "%s: a second Set replaces the first", p->name);
    hr = p->set(p->obj, empty);
    len = ARRAY_SIZE(buf);
    hr = p->get(p->obj, buf, &len);
    CHECKF(hr == S_OK && len == 1 && !buf[0], "%s: empty string can be set", p->name);
    p->set(p->obj, hello);
}

static IWMStreamConfig *get_config(IWMProfile3 *profile, DWORD index)
{
    IWMStreamConfig *c = NULL;
    HRESULT hr = IWMProfile3_GetStream(profile, index, &c);
    if (hr != S_OK) { printf("FAIL  GetStream(%lu) %#lx\n", index, (unsigned long)hr); exit(1); }
    return c;
}

static DWORD stream_count(IWMProfile3 *profile)
{
    DWORD n = 0xdead;
    IWMProfile3_GetStreamCount(profile, &n);
    return n;
}

static void get_name(IWMStreamConfig *c, WCHAR *buf, size_t n)
{
    WORD len = n;
    buf[0] = 0;
    IWMStreamConfig_GetStreamName(c, buf, &len);
}

/* ---- IWMStreamConfig and IWMMediaProps -------------------------------- */
static void test_stream_config(void)
{
    static const GUID subtype_other = {0x12345678, 0, 0, {1, 2, 3, 4, 5, 6, 7, 8}};
    IWMSyncReader *reader = open_reader();
    IWMProfile3 *profile;
    IWMStreamConfig *c, *c2;
    IWMMediaProps *props;
    char buf[2000];
    WM_MEDIA_TYPE *mt = (WM_MEDIA_TYPE *)buf;
    GUID type, type2;
    WORD number, orig;
    DWORD dw, size, size2;
    WCHAR name[64];
    HRESULT hr;
    unsigned i;

    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile3, (void **)&profile);
    c = get_config(profile, 0);
    IWMStreamConfig_GetStreamNumber(c, &orig);
    CHECKF(orig == 1, "first stream is number 1 (%u)", orig);

    {
        struct strprop tab[] = {
            {"stream name", sn_get, sn_set, c}, {"connection name", cn_get, cn_set, c},
        };
        for (i = 0; i < ARRAY_SIZE(tab); i++) test_string_prop(&tab[i]);
    }

    /* bitrate and buffer window */
    hr = IWMStreamConfig_GetBitrate(c, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetBitrate(NULL)");
    dw = 0xdead;
    hr = IWMStreamConfig_GetBitrate(c, &dw);
    CHECKF(hr == S_OK && dw == 0, "idle bitrate is 0 (hr %#lx, %lu)", (unsigned long)hr, dw);
    hr = IWMStreamConfig_SetBitrate(c, 123456);
    CHECKHR(hr, S_OK, "SetBitrate");
    IWMStreamConfig_GetBitrate(c, &dw);
    CHECKF(dw == 123456, "bitrate round trips (%lu)", dw);
    hr = IWMStreamConfig_GetBufferWindow(c, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetBufferWindow(NULL)");
    dw = 0;
    hr = IWMStreamConfig_GetBufferWindow(c, &dw);
    CHECKF(hr == S_OK && dw == 0xffffffff, "no buffer window reads as -1 (hr %#lx, %lu)", (unsigned long)hr, dw);
    hr = IWMStreamConfig_SetBufferWindow(c, 3000);
    CHECKHR(hr, S_OK, "SetBufferWindow");
    IWMStreamConfig_GetBufferWindow(c, &dw);
    CHECKF(dw == 3000, "buffer window round trips (%lu)", dw);
    IWMStreamConfig_SetBufferWindow(c, 0);
    IWMStreamConfig_GetBufferWindow(c, &dw);
    CHECKF(dw == 0, "a window of 0 is kept, not mistaken for unset (%lu)", dw);

    /* stream number: 1..63 */
    hr = IWMStreamConfig_SetStreamNumber(c, 0);
    CHECKHR(hr, E_INVALIDARG, "SetStreamNumber(0)");
    hr = IWMStreamConfig_SetStreamNumber(c, 64);
    CHECKHR(hr, E_INVALIDARG, "SetStreamNumber(64)");
    hr = IWMStreamConfig_SetStreamNumber(c, 63);
    CHECKHR(hr, S_OK, "SetStreamNumber(63)");
    IWMStreamConfig_GetStreamNumber(c, &number);
    CHECKF(number == 63, "stream number 63 round trips (%u)", number);
    IWMStreamConfig_SetStreamNumber(c, orig);

    /* the edits are on a copy until ReconfigStream */
    c2 = NULL;
    hr = IWMProfile3_GetStreamByNumber(profile, 1, &c2);
    get_name(c2, name, ARRAY_SIZE(name));
    IWMStreamConfig_GetBitrate(c2, &dw);
    CHECKF(hr == S_OK && !name[0] && dw == 0, "the profile is unchanged before ReconfigStream (name '%ls', bitrate %lu)", name, dw);
    IWMStreamConfig_Release(c2);

    /* media props */
    IWMStreamConfig_QueryInterface(c, &IID_IWMMediaProps, (void **)&props);
    hr = IWMMediaProps_GetType(props, &type);
    IWMStreamConfig_GetStreamType(c, &type2);
    CHECKF(hr == S_OK && IsEqualGUID(&type, &type2) && !IsEqualGUID(&type, &GUID_NULL), "IWMMediaProps::GetType matches GetStreamType (hr %#lx)", (unsigned long)hr);
    hr = IWMMediaProps_GetType(props, NULL);
    CHECKHR(hr, E_INVALIDARG, "IWMMediaProps::GetType(NULL)");

    hr = IWMMediaProps_SetMediaType(props, NULL);
    CHECKHR(hr, E_INVALIDARG, "SetMediaType(NULL)");
    size = sizeof(buf);
    hr = IWMMediaProps_GetMediaType(props, mt, &size);
    CHECKHR(hr, S_OK, "GetMediaType");
    {
        WM_MEDIA_TYPE bad = *mt;
        bad.majortype = subtype_other;
        hr = IWMMediaProps_SetMediaType(props, &bad);
        CHECKHR(hr, E_INVALIDARG, "SetMediaType with another major type");
    }
    mt->subtype = subtype_other;
    hr = IWMMediaProps_SetMediaType(props, mt);
    CHECKHR(hr, S_OK, "SetMediaType");
    {
        char buf2[2000];
        WM_MEDIA_TYPE *mt2 = (WM_MEDIA_TYPE *)buf2;
        size2 = sizeof(buf2);
        hr = IWMMediaProps_GetMediaType(props, mt2, &size2);
        CHECKF(hr == S_OK && size2 == size && IsEqualGUID(&mt2->subtype, &subtype_other)
                && mt2->cbFormat == mt->cbFormat && !memcmp(mt2 + 1, mt + 1, mt->cbFormat) && mt2->pbFormat == (BYTE *)(mt2 + 1),
                "the media type round trips (hr %#lx, size %lu/%lu)", (unsigned long)hr, size2, size);
        size2 = size - 1;
        hr = IWMMediaProps_GetMediaType(props, mt2, &size2);
        CHECKF(hr == ASF_E_BUFFERTOOSMALL && size2 == size, "short media type buffer (hr %#lx)", (unsigned long)hr);
        size2 = 0;
        hr = IWMMediaProps_GetMediaType(props, NULL, &size2);
        CHECKF(hr == S_OK && size2 == size, "NULL media type buffer reports the size");
    }
    IWMMediaProps_Release(props);

    /* ReconfigStream makes the edits take effect */
    hr = IWMProfile3_ReconfigStream(profile, c);
    CHECKHR(hr, S_OK, "ReconfigStream");
    c2 = NULL;
    IWMProfile3_GetStreamByNumber(profile, 1, &c2);
    get_name(c2, name, ARRAY_SIZE(name));
    IWMStreamConfig_GetBitrate(c2, &dw);
    CHECKF(!wcscmp(name, L"Hello stream") && dw == 123456, "ReconfigStream stored the name (%ls) and the bitrate (%lu)", name, dw);
    IWMStreamConfig_QueryInterface(c2, &IID_IWMMediaProps, (void **)&props);
    {
        char buf2[2000];
        WM_MEDIA_TYPE *mt2 = (WM_MEDIA_TYPE *)buf2;
        size2 = sizeof(buf2);
        IWMMediaProps_GetMediaType(props, mt2, &size2);
        CHECKF(IsEqualGUID(&mt2->subtype, &subtype_other), "ReconfigStream stored the media type");
    }
    IWMMediaProps_Release(props);
    IWMStreamConfig_Release(c2);

    /* a number that is not in the profile */
    IWMStreamConfig_SetStreamNumber(c, 40);
    hr = IWMProfile3_ReconfigStream(profile, c);
    CHECKHR(hr, NS_E_NO_STREAM, "ReconfigStream of an unknown stream number");
    hr = IWMProfile3_ReconfigStream(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "ReconfigStream(NULL)");

    IWMStreamConfig_Release(c);
    IWMProfile3_Release(profile);
    IWMSyncReader_Release(reader);
}

/* ---- output props and INSSBuffer -------------------------------------- */
static void test_output_props(void)
{
    IWMSyncReader *reader = open_reader();
    IWMOutputMediaProps *op = NULL;
    WCHAR buf[16];
    WORD len;
    HRESULT hr;
    int i;
    INSSBuffer *sample = NULL;
    QWORD ts, dur;
    DWORD flags, out_num, size, size2, max;
    WORD sn;
    BYTE *data;

    hr = IWMSyncReader_GetOutputProps(reader, 0, &op);
    CHECKHR(hr, S_OK, "GetOutputProps");
    if (hr != S_OK) return;
    for (i = 0; i < 2; i++)
    {
        const char *what = i ? "connection name" : "stream group name";
        HRESULT (STDMETHODCALLTYPE *fn)(IWMOutputMediaProps *, WCHAR *, WORD *) =
                i ? op->lpVtbl->GetConnectionName : op->lpVtbl->GetStreamGroupName;
        len = 0xdead;
        hr = fn(op, NULL, &len);
        CHECKF(hr == S_OK && len == 1, "output %s: NULL buffer reports 1 (hr %#lx, %u)", what, (unsigned long)hr, len);
        len = 1; buf[0] = 0xcccc;
        hr = fn(op, buf, &len);
        CHECKF(hr == S_OK && len == 1 && !buf[0], "output %s: empty", what);
        len = 0;
        hr = fn(op, buf, &len);
        CHECKF(hr == ASF_E_BUFFERTOOSMALL && len == 1, "output %s: zero-size buffer too small", what);
        hr = fn(op, buf, NULL);
        CHECKHR(hr, E_INVALIDARG, what);
    }
    IWMOutputMediaProps_Release(op);

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &ts, &dur, &flags, &out_num, &sn);
    if (hr != S_OK)
    {
        CHECKF(0, "GetNextSample works (hr %#lx)", (unsigned long)hr);
    }
    else
    {
        size = 0xdead;
        hr = INSSBuffer_GetLength(sample, &size);
        INSSBuffer_GetBufferAndLength(sample, &data, &size2);
        INSSBuffer_GetMaxLength(sample, &max);
        CHECKF(hr == S_OK && size == size2 && size > 0, "INSSBuffer::GetLength equals the length of GetBufferAndLength (%lu/%lu)", size, size2);
        CHECKF(size < max || size == max, "length does not exceed the maximum");
        hr = INSSBuffer_SetLength(sample, size - 1);
        INSSBuffer_GetLength(sample, &size2);
        CHECKF(hr == S_OK && size2 == size - 1, "GetLength follows SetLength (%lu)", size2);
        hr = INSSBuffer_GetLength(sample, NULL);
        CHECKHR(hr, E_INVALIDARG, "GetLength(NULL)");
        INSSBuffer_Release(sample);
    }
    IWMSyncReader_Release(reader);
}

/* ---- IWMProfile ------------------------------------------------------- */
static GUID type_of(IWMStreamConfig *c)
{
    GUID g = GUID_NULL;
    IWMStreamConfig_GetStreamType(c, &g);
    return g;
}

static void test_profile_streams(void)
{
    IWMSyncReader *reader = open_reader();
    IWMProfile3 *profile;
    IWMStreamConfig *c, *n1, *n2, *dummy;
    WMT_STORAGE_FORMAT fmt = 7;
    WMT_VERSION ver = 0;
    GUID id = {1}, g;
    DWORD count, out_count;
    WORD number;
    HRESULT hr;
    struct strprop tab[2];
    unsigned i;
    QWORD packets;

    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile3, (void **)&profile);

    hr = IWMProfile3_GetVersion(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetVersion(NULL)");
    hr = IWMProfile3_GetVersion(profile, &ver);
    CHECKF(hr == S_OK && ver == WMT_VER_9_0, "GetVersion (hr %#lx, %#x)", (unsigned long)hr, ver);
    hr = IWMProfile3_GetProfileID(profile, &id);
    CHECKF(hr == S_OK && IsEqualGUID(&id, &GUID_NULL), "GetProfileID of a file profile is GUID_NULL");
    hr = IWMProfile3_GetProfileID(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetProfileID(NULL)");

    tab[0] = (struct strprop){"profile name", pn_get, pn_set, profile};
    tab[1] = (struct strprop){"profile description", pd_get, pd_set, profile};
    for (i = 0; i < ARRAY_SIZE(tab); i++) test_string_prop(&tab[i]);

    hr = IWMProfile3_GetStorageFormat(profile, &fmt);
    CHECKF(hr == S_OK && fmt == WMT_Storage_Format_V1, "default storage format is V1 (%d)", fmt);
    hr = IWMProfile3_SetStorageFormat(profile, WMT_Storage_Format_MP3);
    CHECKHR(hr, S_OK, "SetStorageFormat(MP3)");
    IWMProfile3_GetStorageFormat(profile, &fmt);
    CHECKF(fmt == WMT_Storage_Format_MP3, "storage format round trips (%d)", fmt);
    hr = IWMProfile3_SetStorageFormat(profile, 2);
    CHECKHR(hr, E_INVALIDARG, "SetStorageFormat(2)");
    hr = IWMProfile3_GetStorageFormat(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetStorageFormat(NULL)");

    /* expected packet count follows the configured bitrates */
    packets = 0xdead;
    {
        IWMProfile3 *p3;
        IWMProfile3_QueryInterface(profile, &IID_IWMProfile3, (void **)&p3);
        hr = IWMProfile3_GetExpectedPacketCount(p3, 100000000, NULL);
        CHECKHR(hr, E_INVALIDARG, "GetExpectedPacketCount(NULL)");
        hr = IWMProfile3_GetExpectedPacketCount(p3, 100000000, &packets);
        CHECKF(hr == S_OK && packets == 0, "no bitrate, no packets (hr %#lx, %I64u)", (unsigned long)hr, packets);
        c = get_config(profile, 1);
        IWMStreamConfig_SetBitrate(c, 800000);
        IWMProfile3_ReconfigStream(profile, c);
        IWMStreamConfig_Release(c);
        hr = IWMProfile3_GetExpectedPacketCount(p3, 100000000, &packets);
        CHECKF(hr == S_OK && packets == 313, "10 s of 800 kbit/s in 3200-byte packets is 313 (hr %#lx, %I64u)", (unsigned long)hr, packets);
        IWMProfile3_Release(p3);
    }

    /* removing streams */
    count = stream_count(profile);
    CHECKF(count == 2, "the file has two streams (%lu)", count);
    hr = IWMProfile3_RemoveStreamByNumber(profile, 9);
    CHECKHR(hr, NS_E_NO_STREAM, "RemoveStreamByNumber of an unknown stream");
    hr = IWMProfile3_RemoveStream(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "RemoveStream(NULL)");
    n1 = get_config(profile, 0);
    hr = IWMProfile3_RemoveStream(profile, n1);
    CHECKHR(hr, S_OK, "RemoveStream");
    count = stream_count(profile);
    CHECKF(count == 1, "one stream left (%lu)", count);
    dummy = NULL;
    hr = IWMProfile3_GetStreamByNumber(profile, 1, &dummy);
    CHECKHR(hr, NS_E_NO_STREAM, "the removed stream is gone");
    c = get_config(profile, 0);
    IWMStreamConfig_GetStreamNumber(c, &number);
    CHECKF(number == 2, "GetStream(0) is now stream 2 (%u)", number);
    IWMStreamConfig_Release(c);
    hr = IWMProfile3_RemoveStream(profile, n1);
    CHECKHR(hr, NS_E_NO_STREAM, "removing the same stream twice");
    hr = IWMSyncReader_GetOutputCount(reader, &out_count);
    CHECKF(hr == S_OK && out_count == 2, "the reader's outputs are not affected (%lu)", out_count);

    /* creating and adding */
    hr = IWMProfile3_CreateNewStream(profile, NULL, &dummy);
    CHECKHR(hr, E_INVALIDARG, "CreateNewStream(NULL)");
    hr = IWMProfile3_CreateNewStream(profile, &GUID_NULL, &dummy);
    CHECKHR(hr, E_INVALIDARG, "CreateNewStream(GUID_NULL)");
    hr = IWMProfile3_CreateNewStream(profile, &WMMEDIATYPE_Audio, &n2);
    CHECKHR(hr, S_OK, "CreateNewStream(audio)");
    g = type_of(n2);
    IWMStreamConfig_GetStreamNumber(n2, &number);
    CHECKF(IsEqualGUID(&g, &WMMEDIATYPE_Audio) && number == 1, "new stream: audio, lowest free number 1 (%u)", number);
    {
        char buf[500];
        WM_MEDIA_TYPE *mt = (WM_MEDIA_TYPE *)buf;
        DWORD size = sizeof(buf);
        IWMMediaProps *props;
        IWMStreamConfig_QueryInterface(n2, &IID_IWMMediaProps, (void **)&props);
        hr = IWMMediaProps_GetMediaType(props, mt, &size);
        CHECKF(hr == S_OK && IsEqualGUID(&mt->majortype, &WMMEDIATYPE_Audio), "new stream's media type has its major type (hr %#lx)", (unsigned long)hr);
        IWMMediaProps_Release(props);
    }
    hr = IWMProfile3_AddStream(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "AddStream(NULL)");
    hr = IWMProfile3_AddStream(profile, n2);
    CHECKHR(hr, S_OK, "AddStream");
    count = stream_count(profile);
    CHECKF(count == 2, "two streams (%lu)", count);
    hr = IWMProfile3_AddStream(profile, n2);
    CHECKHR(hr, NS_E_INVALID_STREAM, "AddStream of a number already in use");
    IWMStreamConfig_Release(n2);
    hr = IWMProfile3_CreateNewStream(profile, &WMMEDIATYPE_Video, &n2);
    IWMStreamConfig_GetStreamNumber(n2, &number);
    CHECKF(hr == S_OK && number == 3, "the next new stream is number 3 (%u)", number);
    IWMStreamConfig_SetStreamNumber(n2, 2);
    hr = IWMProfile3_AddStream(profile, n2);
    CHECKHR(hr, NS_E_INVALID_STREAM, "AddStream with the number of an existing stream");
    IWMStreamConfig_SetStreamNumber(n2, 3);
    IWMStreamConfig_SetStreamName(n2, L"extra");
    IWMProfile3_AddStream(profile, n2);
    c = NULL;
    hr = IWMProfile3_GetStreamByNumber(profile, 3, &c);
    {
        WCHAR nm[16];
        get_name(c, nm, ARRAY_SIZE(nm));
        CHECKF(hr == S_OK && !wcscmp(nm, L"extra"), "the added stream keeps its name (%ls)", nm);
    }
    IWMStreamConfig_Release(c);
    c = get_config(profile, 2);
    IWMStreamConfig_GetStreamNumber(c, &number);
    CHECKF(number == 3, "added streams follow the file's streams in GetStream (%u)", number);
    IWMStreamConfig_Release(c);
    hr = IWMProfile3_RemoveStreamByNumber(profile, 3);
    CHECKHR(hr, S_OK, "RemoveStreamByNumber of an added stream");
    count = stream_count(profile);
    CHECKF(count == 2, "two streams again (%lu)", count);

    IWMStreamConfig_Release(n1);
    IWMStreamConfig_Release(n2);
    IWMProfile3_Release(profile);
    IWMSyncReader_Release(reader);
}

static void test_mutual_exclusion(void)
{
    IWMSyncReader *reader = open_reader();
    IWMProfile3 *profile;
    IWMMutualExclusion *mx = NULL, *mx2 = NULL;
    IWMStreamList *list = NULL;
    IUnknown *u1, *u2;
    WORD streams[4], count;
    GUID type;
    DWORD n;
    HRESULT hr;

    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile3, (void **)&profile);
    n = 0xdead;
    hr = IWMProfile3_GetMutualExclusionCount(profile, &n);
    CHECKF(hr == S_OK && n == 0, "no mutual exclusions at first (hr %#lx)", (unsigned long)hr);
    hr = IWMProfile3_GetMutualExclusionCount(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetMutualExclusionCount(NULL)");
    hr = IWMProfile3_GetMutualExclusion(profile, 0, &mx2);
    CHECKHR(hr, E_INVALIDARG, "GetMutualExclusion out of range");
    hr = IWMProfile3_CreateNewMutualExclusion(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "CreateNewMutualExclusion(NULL)");
    hr = IWMProfile3_CreateNewMutualExclusion(profile, &mx);
    CHECKHR(hr, S_OK, "CreateNewMutualExclusion");
    hr = IWMMutualExclusion_QueryInterface(mx, &IID_IWMStreamList, (void **)&list);
    CHECKHR(hr, S_OK, "mutual exclusion is an IWMStreamList");
    if (list) IWMStreamList_Release(list);
    IWMProfile3_GetMutualExclusionCount(profile, &n);
    CHECKF(n == 0, "creating does not add (%lu)", n);

    hr = IWMMutualExclusion_GetType(mx, &type);
    CHECKF(hr == S_OK && !IsEqualGUID(&type, &GUID_NULL), "default type is not GUID_NULL");
    hr = IWMMutualExclusion_SetType(mx, &WMMEDIATYPE_Audio);
    IWMMutualExclusion_GetType(mx, &type);
    CHECKF(hr == S_OK && IsEqualGUID(&type, &WMMEDIATYPE_Audio), "type round trips");
    hr = IWMMutualExclusion_GetType(mx, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetType(NULL)");

    hr = IWMMutualExclusion_AddStream(mx, 0);
    CHECKHR(hr, E_INVALIDARG, "AddStream(0)");
    hr = IWMMutualExclusion_AddStream(mx, 64);
    CHECKHR(hr, E_INVALIDARG, "AddStream(64)");
    hr = IWMMutualExclusion_AddStream(mx, 1);
    CHECKHR(hr, S_OK, "AddStream(1)");
    hr = IWMMutualExclusion_AddStream(mx, 2);
    CHECKHR(hr, S_OK, "AddStream(2)");
    hr = IWMMutualExclusion_AddStream(mx, 2);
    CHECKHR(hr, NS_E_INVALID_STREAM, "AddStream(2) twice");
    count = 0xdead;
    hr = IWMMutualExclusion_GetStreams(mx, NULL, &count);
    CHECKF(hr == S_OK && count == 2, "GetStreams(NULL) counts (%u)", count);
    count = 1;
    memset(streams, 0xcc, sizeof(streams));
    hr = IWMMutualExclusion_GetStreams(mx, streams, &count);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && count == 2 && streams[0] == 0xcccc, "GetStreams short array");
    count = 4;
    hr = IWMMutualExclusion_GetStreams(mx, streams, &count);
    CHECKF(hr == S_OK && count == 2 && streams[0] == 1 && streams[1] == 2, "GetStreams returns 1, 2");
    hr = IWMMutualExclusion_GetStreams(mx, streams, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetStreams(count NULL)");
    hr = IWMMutualExclusion_RemoveStream(mx, 1);
    CHECKHR(hr, S_OK, "RemoveStream(1)");
    hr = IWMMutualExclusion_RemoveStream(mx, 1);
    CHECKHR(hr, NS_E_INVALID_STREAM, "RemoveStream(1) twice");
    count = 4;
    IWMMutualExclusion_GetStreams(mx, streams, &count);
    CHECKF(count == 1 && streams[0] == 2, "only stream 2 left");
    IWMMutualExclusion_AddStream(mx, 1);

    hr = IWMProfile3_AddMutualExclusion(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "AddMutualExclusion(NULL)");
    hr = IWMProfile3_AddMutualExclusion(profile, mx);
    CHECKHR(hr, S_OK, "AddMutualExclusion");
    IWMProfile3_GetMutualExclusionCount(profile, &n);
    CHECKF(n == 1, "one mutual exclusion (%lu)", n);
    hr = IWMProfile3_GetMutualExclusion(profile, 0, &mx2);
    CHECKHR(hr, S_OK, "GetMutualExclusion(0)");
    IWMMutualExclusion_QueryInterface(mx, &IID_IUnknown, (void **)&u1);
    IWMMutualExclusion_QueryInterface(mx2, &IID_IUnknown, (void **)&u2);
    CHECKF(u1 == u2, "the same object comes back");
    IUnknown_Release(u1); IUnknown_Release(u2);
    IWMMutualExclusion_Release(mx2);
    hr = IWMProfile3_GetMutualExclusion(profile, 1, &mx2);
    CHECKHR(hr, E_INVALIDARG, "GetMutualExclusion(1)");
    hr = IWMProfile3_GetMutualExclusion(profile, 0, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetMutualExclusion(NULL)");

    /* removing a stream from the profile takes it out of the exclusion */
    hr = IWMProfile3_RemoveStreamByNumber(profile, 2);
    CHECKHR(hr, S_OK, "RemoveStreamByNumber(2)");
    count = 4;
    IWMMutualExclusion_GetStreams(mx, streams, &count);
    CHECKF(count == 1 && streams[0] == 1, "the removed stream left the exclusion (%u streams)", count);

    hr = IWMProfile3_RemoveMutualExclusion(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "RemoveMutualExclusion(NULL)");
    hr = IWMProfile3_RemoveMutualExclusion(profile, mx);
    CHECKHR(hr, S_OK, "RemoveMutualExclusion");
    IWMProfile3_GetMutualExclusionCount(profile, &n);
    CHECKF(n == 0, "no mutual exclusions again (%lu)", n);
    hr = IWMProfile3_RemoveMutualExclusion(profile, mx);
    CHECKHR(hr, E_INVALIDARG, "RemoveMutualExclusion twice");
    n = IWMMutualExclusion_Release(mx);
    CHECKF(n == 0, "no references left over (%lu)", n);

    IWMProfile3_Release(profile);
    IWMSyncReader_Release(reader);
}

static void test_bandwidth_and_priority(void)
{
    IWMSyncReader *reader = open_reader();
    IWMProfile3 *profile;
    IWMBandwidthSharing *bs = NULL, *bs2 = NULL;
    IWMStreamPrioritization *sp = NULL, *sp2 = NULL;
    WM_STREAM_PRIORITY_RECORD rec[3] = {{2, TRUE}, {1, FALSE}, {5, FALSE}}, out[3];
    WORD count, streams[3];
    DWORD n, bitrate, buffer;
    GUID type;
    HRESULT hr;

    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile3, (void **)&profile);

    n = 0xdead;
    hr = IWMProfile3_GetBandwidthSharingCount(profile, &n);
    CHECKF(hr == S_OK && n == 0, "no bandwidth sharing at first");
    hr = IWMProfile3_GetBandwidthSharing(profile, 0, &bs2);
    CHECKHR(hr, E_INVALIDARG, "GetBandwidthSharing out of range");
    hr = IWMProfile3_CreateNewBandwidthSharing(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "CreateNewBandwidthSharing(NULL)");
    hr = IWMProfile3_CreateNewBandwidthSharing(profile, &bs);
    CHECKHR(hr, S_OK, "CreateNewBandwidthSharing");
    hr = IWMBandwidthSharing_GetBandwidth(bs, &bitrate, &buffer);
    CHECKF(hr == S_OK && !bitrate && !buffer, "initial bandwidth is 0 / 0");
    hr = IWMBandwidthSharing_SetBandwidth(bs, 500000, 4000);
    IWMBandwidthSharing_GetBandwidth(bs, &bitrate, &buffer);
    CHECKF(hr == S_OK && bitrate == 500000 && buffer == 4000, "bandwidth round trips (%lu/%lu)", bitrate, buffer);
    hr = IWMBandwidthSharing_GetBandwidth(bs, NULL, &buffer);
    CHECKHR(hr, E_INVALIDARG, "GetBandwidth(NULL)");
    IWMBandwidthSharing_SetType(bs, &WMMEDIATYPE_Video);
    hr = IWMBandwidthSharing_GetType(bs, &type);
    CHECKF(hr == S_OK && IsEqualGUID(&type, &WMMEDIATYPE_Video), "sharing type round trips");
    IWMBandwidthSharing_AddStream(bs, 1);
    hr = IWMBandwidthSharing_AddStream(bs, 1);
    CHECKHR(hr, NS_E_INVALID_STREAM, "sharing AddStream twice");
    hr = IWMBandwidthSharing_AddStream(bs, 64);
    CHECKHR(hr, E_INVALIDARG, "sharing AddStream(64)");
    IWMBandwidthSharing_AddStream(bs, 2);
    count = 3;
    hr = IWMBandwidthSharing_GetStreams(bs, streams, &count);
    CHECKF(hr == S_OK && count == 2 && streams[0] == 1 && streams[1] == 2, "sharing streams");
    hr = IWMProfile3_AddBandwidthSharing(profile, bs);
    CHECKHR(hr, S_OK, "AddBandwidthSharing");
    hr = IWMProfile3_AddBandwidthSharing(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "AddBandwidthSharing(NULL)");
    IWMProfile3_GetBandwidthSharingCount(profile, &n);
    CHECKF(n == 1, "one bandwidth sharing (%lu)", n);
    hr = IWMProfile3_GetBandwidthSharing(profile, 0, &bs2);
    CHECKF(hr == S_OK && bs2 == bs, "the same sharing object comes back");
    if (bs2) IWMBandwidthSharing_Release(bs2);
    IWMProfile3_RemoveStreamByNumber(profile, 1);
    count = 3;
    IWMBandwidthSharing_GetStreams(bs, streams, &count);
    CHECKF(count == 1 && streams[0] == 2, "removing a stream from the profile removes it from the sharing");
    hr = IWMProfile3_RemoveBandwidthSharing(profile, bs);
    CHECKHR(hr, S_OK, "RemoveBandwidthSharing");
    hr = IWMProfile3_RemoveBandwidthSharing(profile, bs);
    CHECKHR(hr, E_INVALIDARG, "RemoveBandwidthSharing twice");
    n = IWMBandwidthSharing_Release(bs);
    CHECKF(n == 0, "no sharing references left (%lu)", n);

    /* stream prioritization */
    hr = IWMProfile3_GetStreamPrioritization(profile, &sp);
    CHECKF(hr == S_FALSE && sp == NULL, "no prioritization at first: S_FALSE and NULL (hr %#lx)", (unsigned long)hr);
    hr = IWMProfile3_GetStreamPrioritization(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetStreamPrioritization(NULL)");
    hr = IWMProfile3_SetStreamPrioritization(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "SetStreamPrioritization(NULL)");
    hr = IWMProfile3_CreateNewStreamPrioritization(profile, NULL);
    CHECKHR(hr, E_INVALIDARG, "CreateNewStreamPrioritization(NULL)");
    hr = IWMProfile3_CreateNewStreamPrioritization(profile, &sp);
    CHECKHR(hr, S_OK, "CreateNewStreamPrioritization");
    count = 0xdead;
    hr = IWMStreamPrioritization_GetPriorityRecords(sp, NULL, &count);
    CHECKF(hr == S_OK && count == 0, "no records at first");
    hr = IWMStreamPrioritization_SetPriorityRecords(sp, NULL, 1);
    CHECKHR(hr, E_INVALIDARG, "SetPriorityRecords(NULL, 1)");
    rec[2].wStreamNumber = 0;
    hr = IWMStreamPrioritization_SetPriorityRecords(sp, rec, 3);
    CHECKHR(hr, E_INVALIDARG, "SetPriorityRecords with stream 0");
    rec[2].wStreamNumber = 2;
    hr = IWMStreamPrioritization_SetPriorityRecords(sp, rec, 3);
    CHECKHR(hr, E_INVALIDARG, "SetPriorityRecords with a stream twice");
    rec[2].wStreamNumber = 5;
    hr = IWMStreamPrioritization_SetPriorityRecords(sp, rec, 3);
    CHECKHR(hr, S_OK, "SetPriorityRecords");
    count = 0xdead;
    IWMStreamPrioritization_GetPriorityRecords(sp, NULL, &count);
    CHECKF(count == 3, "three records (%u)", count);
    count = 2;
    hr = IWMStreamPrioritization_GetPriorityRecords(sp, out, &count);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && count == 3, "short record array");
    count = 3;
    memset(out, 0, sizeof(out));
    hr = IWMStreamPrioritization_GetPriorityRecords(sp, out, &count);
    CHECKF(hr == S_OK && count == 3 && out[0].wStreamNumber == 2 && out[0].fMandatory && out[1].wStreamNumber == 1
            && !out[1].fMandatory && out[2].wStreamNumber == 5, "records round trip in order");
    hr = IWMStreamPrioritization_GetPriorityRecords(sp, out, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetPriorityRecords(count NULL)");
    rec[0].wStreamNumber = 7;
    hr = IWMStreamPrioritization_SetPriorityRecords(sp, rec, 1);
    CHECKHR(hr, S_OK, "SetPriorityRecords again");
    count = 3;
    IWMStreamPrioritization_GetPriorityRecords(sp, out, &count);
    CHECKF(count == 1 && out[0].wStreamNumber == 7, "a second Set replaces the records (%u)", count);

    hr = IWMProfile3_SetStreamPrioritization(profile, sp);
    CHECKHR(hr, S_OK, "SetStreamPrioritization");
    hr = IWMProfile3_GetStreamPrioritization(profile, &sp2);
    CHECKF(hr == S_OK && sp2 == sp, "the same prioritization comes back (hr %#lx)", (unsigned long)hr);
    if (sp2) IWMStreamPrioritization_Release(sp2);
    hr = IWMProfile3_RemoveStreamPrioritization(profile);
    CHECKHR(hr, S_OK, "RemoveStreamPrioritization");
    sp2 = NULL;
    hr = IWMProfile3_GetStreamPrioritization(profile, &sp2);
    CHECKF(hr == S_FALSE && !sp2, "gone after Remove");
    hr = IWMProfile3_RemoveStreamPrioritization(profile);
    CHECKHR(hr, S_OK, "RemoveStreamPrioritization when none");
    n = IWMStreamPrioritization_Release(sp);
    CHECKF(n == 0, "no prioritization references left (%lu)", n);

    IWMProfile3_Release(profile);
    IWMSyncReader_Release(reader);
}

int main(int argc, char **argv)
{
    static WCHAR wpath[MAX_PATH];
    HMODULE wmvcore;

    if (argc < 2) { printf("usage: %s file.wmv\n", argv[0]); return 2; }
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, wpath, MAX_PATH);
    path = wpath;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    wmvcore = LoadLibraryA("wmvcore.dll");
    pWMCreateSyncReader = wmvcore ? (void *)GetProcAddress(wmvcore, "WMCreateSyncReader") : NULL;
    if (!pWMCreateSyncReader) { printf("SKIP  no wmvcore\n"); return 77; }

    test_output_props();
    test_stream_config();
    test_profile_streams();
    test_mutual_exclusion();
    test_bandwidth_and_priority();

    printf("%s\nRESULT: %s\n", failures ? "FAILURES" : "all checks passed", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
