/* wmvcore's exports, profile manager and writer configuration
 * (patches/sg/2856), run by test/wmv-misc-gate.sh: WMCheckURLExtension and
 * WMCheckURLScheme tables, WMIsContentProtected on generated ASF headers,
 * the profile manager's system profile version, and the writer's sync
 * tolerance, live source, non-blocking mode and sink list.
 *
 *   wmv-misc-probe.exe [path-of-sample.wmv] */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static HMODULE wmvcore;
static FARPROC proc(const char *name)
{
    FARPROC p;
    if (!wmvcore) wmvcore = LoadLibraryW(L"wmvcore.dll");
    p = wmvcore ? GetProcAddress(wmvcore, name) : NULL;
    if (!p) CHECKF(0, "wmvcore.dll exports %s", name);
    return p;
}

static void test_url_checks(void)
{
    HRESULT (WINAPI *ext)(const WCHAR *) = (void *)proc("WMCheckURLExtension");
    HRESULT (WINAPI *scheme)(const WCHAR *) = (void *)proc("WMCheckURLScheme");
    static const struct { const WCHAR *url; HRESULT hr; } urls[] = {
        {L"test.mp3", S_OK}, {L"test.MP3", S_OK}, {L"test.asf", S_OK}, {L"test.wm", S_OK}, {L"test.wma", S_OK},
        {L"test.wmv", S_OK}, {L"C:\\music\\a.WMA", S_OK}, {L"abcd://test/test.wmv", S_OK},
        {L"http://test/t.asf?alt=t.mkv", S_OK}, {L"x.wmv#fragment", S_OK}, {L"a.b.c.wmv", S_OK}, {L".wmv", S_OK},
        {L"test.mkv", NS_E_INVALID_NAME}, {L"test", NS_E_INVALID_NAME}, {L"", NS_E_INVALID_NAME},
        {L"a.wmv.txt", NS_E_INVALID_NAME}, {L"dir.wmv/file", NS_E_INVALID_NAME}, {L"dir.wmv\\file", NS_E_INVALID_NAME},
        {L"http://h/a.mkv?x.wmv", NS_E_INVALID_NAME}, {L"file.wmvx", NS_E_INVALID_NAME}, {L"file.w", NS_E_INVALID_NAME},
        {L"test.", NS_E_INVALID_NAME},
    };
    static const struct { const WCHAR *scheme; HRESULT hr; } schemes[] = {
        {L"file", S_OK}, {L"http", S_OK}, {L"HTTP", S_OK}, {L"mms", S_OK}, {L"rtsp", S_OK}, {L"mmst", S_OK}, {L"rtspu", S_OK},
        {L"ftp", NS_E_INVALID_NAME}, {L"gopher", NS_E_INVALID_NAME}, {L"", NS_E_INVALID_NAME}, {L"htt", NS_E_INVALID_NAME},
        {L"https2", NS_E_INVALID_NAME},
    };
    unsigned int i;
    HRESULT hr;

    if (!ext || !scheme) return;
    hr = ext(NULL);
    CHECKF(hr == E_INVALIDARG, "WMCheckURLExtension(NULL) = E_INVALIDARG (%08lx)", hr);
    for (i = 0; i < ARRAY_SIZE(urls); i++)
    {
        hr = ext(urls[i].url);
        CHECKF(hr == urls[i].hr, "WMCheckURLExtension(%ls) = %08lx (%08lx)", urls[i].url, urls[i].hr, hr);
    }
    hr = scheme(NULL);
    CHECKF(hr == E_INVALIDARG, "WMCheckURLScheme(NULL) = E_INVALIDARG (%08lx)", hr);
    for (i = 0; i < ARRAY_SIZE(schemes); i++)
    {
        hr = scheme(schemes[i].scheme);
        CHECKF(hr == schemes[i].hr, "WMCheckURLScheme(%ls) = %08lx (%08lx)", schemes[i].scheme, schemes[i].hr, hr);
    }
}

/* ---- an ASF header with a list of header sub-objects ---- */

static const BYTE asf_header_guid[16] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
static const BYTE file_props_guid[16] = {0xa1, 0xdc, 0xab, 0x8c, 0x47, 0xa9, 0xcf, 0x11, 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
static const BYTE content_enc_guid[16] = {0xfb, 0xb3, 0x11, 0x22, 0x23, 0xbd, 0xd2, 0x11, 0xb4, 0xb7, 0x00, 0xa0, 0xc9, 0x55, 0xfc, 0x6e};
static const BYTE ext_content_enc_guid[16] = {0x14, 0xe6, 0x8a, 0x29, 0x22, 0x26, 0x17, 0x4c, 0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c};

static void put_object(BYTE **p, const BYTE *guid, DWORD data_size)
{
    QWORD size = 24 + data_size;
    memcpy(*p, guid, 16); *p += 16;
    memcpy(*p, &size, 8); *p += 8;
    memset(*p, 0xab, data_size); *p += data_size;
}

/* the sub-objects in order, 1 = file properties, 2 = content encryption, 3 = extended content encryption */
static void write_asf(const WCHAR *path, const int *objects, int count, BOOL truncate)
{
    BYTE buf[512], *p = buf;
    QWORD total;
    DWORD n = count, w;
    HANDLE f;
    int i;

    memset(buf, 0, sizeof(buf));
    memcpy(p, asf_header_guid, 16); p += 16;
    p += 8; /* size, below */
    memcpy(p, &n, 4); p += 4;
    p[0] = 1; p[1] = 2; p += 2;
    for (i = 0; i < count; i++)
        put_object(&p, objects[i] == 1 ? file_props_guid : objects[i] == 2 ? content_enc_guid : ext_content_enc_guid, 10 + i);
    total = p - buf;
    memcpy(buf + 16, &total, 8);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, buf, truncate ? 40 : (DWORD)total, &w, NULL);
    CloseHandle(f);
}

static void test_content_protected(const WCHAR *sample)
{
    HRESULT (WINAPI *prot)(const WCHAR *, BOOL *) = (void *)proc("WMIsContentProtected");
    WCHAR path[MAX_PATH];
    BOOL drm;
    HRESULT hr;
    unsigned int i;
    static const struct { const char *name; int objects[4]; int count; BOOL trunc; BOOL protected; } cases[] = {
        {"a header with only file properties", {1}, 1, FALSE, FALSE},
        {"an empty header", {0}, 0, FALSE, FALSE},
        {"a content encryption object", {1, 2}, 2, FALSE, TRUE},
        {"a content encryption object first", {2, 1}, 2, FALSE, TRUE},
        {"an extended content encryption object", {1, 3}, 2, FALSE, TRUE},
        {"an encryption object after other objects", {1, 1, 1, 2}, 4, FALSE, TRUE},
        {"a header truncated before the encryption object", {1, 2}, 2, TRUE, FALSE},
    };

    if (!prot) return;
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"wmv-misc-probe.wma");

    drm = 5; hr = prot(NULL, &drm);
    CHECKF(hr == E_INVALIDARG, "WMIsContentProtected(NULL file) = E_INVALIDARG (%08lx)", hr);
    hr = prot(L"x.wma", NULL);
    CHECKF(hr == E_INVALIDARG, "WMIsContentProtected(NULL out) = E_INVALIDARG (%08lx)", hr);
    drm = 5; hr = prot(L"C:\\no\\such\\file.wma", &drm);
    CHECKF(hr == S_FALSE && drm == FALSE, "a missing file is not protected, S_FALSE (%08lx %d)", hr, drm);

    for (i = 0; i < ARRAY_SIZE(cases); i++)
    {
        write_asf(path, cases[i].objects, cases[i].count, cases[i].trunc);
        drm = 5; hr = prot(path, &drm);
        CHECKF(hr == (cases[i].protected ? S_OK : S_FALSE) && drm == cases[i].protected, "%s: %s (%08lx %d)", cases[i].name,
               cases[i].protected ? "protected, S_OK" : "not protected, S_FALSE", hr, drm);
    }
    {
        /* not an ASF file at all */
        DWORD w;
        HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        BYTE junk[64];
        memset(junk, 0x41, sizeof(junk));
        WriteFile(f, junk, sizeof(junk), &w, NULL);
        CloseHandle(f);
        drm = 5; hr = prot(path, &drm);
        CHECKF(hr == S_FALSE && drm == FALSE, "a file that is not ASF is not protected (%08lx %d)", hr, drm);
        f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(f);
        drm = 5; hr = prot(path, &drm);
        CHECKF(hr == S_FALSE && drm == FALSE, "an empty file is not protected (%08lx %d)", hr, drm);
    }
    DeleteFileW(path);
    if (sample)
    {
        drm = 5; hr = prot(sample, &drm);
        CHECKF(hr == S_FALSE && drm == FALSE, "the sample video is not protected (%08lx %d)", hr, drm);
    }
}

static void test_profile_manager(void)
{
    HRESULT (WINAPI *create)(IWMProfileManager **) = (void *)proc("WMCreateProfileManager");
    IWMProfileManager *mgr = NULL;
    IWMProfileManager2 *mgr2 = NULL;
    IWMProfile *profile = (IWMProfile *)1;
    WMT_VERSION version = 99;
    static const WMT_VERSION versions[] = {WMT_VER_4_0, WMT_VER_7_0, WMT_VER_8_0, WMT_VER_9_0};
    DWORD count = 5;
    WCHAR str[8];
    DWORD len = 8;
    GUID guid = {1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 11}};
    HRESULT hr;
    unsigned int i;

    if (!create) return;
    hr = create(&mgr);
    CHECKF(hr == S_OK && mgr, "WMCreateProfileManager (%08lx)", hr);
    if (!mgr) return;
    hr = IWMProfileManager_QueryInterface(mgr, &IID_IWMProfileManager2, (void **)&mgr2);
    CHECKF(hr == S_OK, "IWMProfileManager2 (%08lx)", hr);
    if (!mgr2) { IWMProfileManager_Release(mgr); return; }

    hr = IWMProfileManager2_GetSystemProfileVersion(mgr2, &version);
    CHECKF(hr == S_OK && version == WMT_VER_8_0, "the system profile version defaults to 8.0 (%08lx %08x)", hr, version);
    for (i = 0; i < ARRAY_SIZE(versions); i++)
    {
        WMT_VERSION got = 99;
        hr = IWMProfileManager2_SetSystemProfileVersion(mgr2, versions[i]);
        IWMProfileManager2_GetSystemProfileVersion(mgr2, &got);
        CHECKF(hr == S_OK && got == versions[i], "version %08x round trip (%08lx %08x)", versions[i], hr, got);
    }
    hr = IWMProfileManager2_SetSystemProfileVersion(mgr2, 0x12345);
    CHECKF(hr == E_INVALIDARG, "an unknown version = E_INVALIDARG (%08lx)", hr);
    hr = IWMProfileManager2_SetSystemProfileVersion(mgr2, 0);
    CHECKF(hr == E_INVALIDARG, "version 0 = E_INVALIDARG (%08lx)", hr);
    IWMProfileManager2_GetSystemProfileVersion(mgr2, &version);
    CHECKF(version == WMT_VER_9_0, "a refused version leaves the old one (%08x)", version);
    hr = IWMProfileManager2_GetSystemProfileVersion(mgr2, NULL);
    CHECKF(hr == E_INVALIDARG, "GetSystemProfileVersion(NULL) = E_INVALIDARG (%08lx)", hr);

    /* argument checks of the calls that need profile objects */
    hr = IWMProfileManager2_CreateEmptyProfile(mgr2, WMT_VER_8_0, NULL);
    CHECKF(hr == E_INVALIDARG, "CreateEmptyProfile(NULL) = E_INVALIDARG (%08lx)", hr);
    profile = (IWMProfile *)1;
    hr = IWMProfileManager2_CreateEmptyProfile(mgr2, 0x777, &profile);
    CHECKF(hr == E_INVALIDARG && !profile, "CreateEmptyProfile(bad version) = E_INVALIDARG, NULL (%08lx)", hr);
    hr = IWMProfileManager2_LoadProfileByID(mgr2, &guid, NULL);
    CHECKF(hr == E_INVALIDARG, "LoadProfileByID(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMProfileManager2_LoadProfileByData(mgr2, NULL, &profile);
    CHECKF(hr == E_INVALIDARG, "LoadProfileByData(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMProfileManager2_LoadProfileByData(mgr2, L"<profile/>", NULL);
    CHECKF(hr == E_INVALIDARG, "LoadProfileByData(.., NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMProfileManager2_SaveProfile(mgr2, NULL, str, &len);
    CHECKF(hr == E_INVALIDARG, "SaveProfile(NULL profile) = E_INVALIDARG (%08lx)", hr);
    hr = IWMProfileManager2_GetSystemProfileCount(mgr2, NULL);
    CHECKF(hr == E_INVALIDARG, "GetSystemProfileCount(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMProfileManager2_LoadSystemProfile(mgr2, 0, NULL);
    CHECKF(hr == E_INVALIDARG, "LoadSystemProfile(NULL) = E_INVALIDARG (%08lx)", hr);
    (void)count;

    IWMProfileManager2_Release(mgr2);
    IWMProfileManager_Release(mgr);
}

/* ---- a writer sink ---- */

struct sink { IWMWriterSink iface; LONG ref; };
static struct sink *impl_sink(IWMWriterSink *i) { return CONTAINING_RECORD(i, struct sink, iface); }
static HRESULT WINAPI sink_QI(IWMWriterSink *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWMWriterSink)) { *ppv = i; IWMWriterSink_AddRef(i); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IWMWriterSink *i) { return InterlockedIncrement(&impl_sink(i)->ref); }
static ULONG WINAPI sink_Release(IWMWriterSink *i) { return InterlockedDecrement(&impl_sink(i)->ref); }
static HRESULT WINAPI sink_OnHeader(IWMWriterSink *i, INSSBuffer *h) { return S_OK; }
static HRESULT WINAPI sink_IsRealTime(IWMWriterSink *i, BOOL *r) { *r = FALSE; return S_OK; }
static HRESULT WINAPI sink_Allocate(IWMWriterSink *i, DWORD s, INSSBuffer **b) { return E_NOTIMPL; }
static HRESULT WINAPI sink_OnDataUnit(IWMWriterSink *i, INSSBuffer *b) { return S_OK; }
static HRESULT WINAPI sink_OnEnd(IWMWriterSink *i) { return S_OK; }
static const IWMWriterSinkVtbl sink_vtbl = {sink_QI, sink_AddRef, sink_Release, sink_OnHeader, sink_IsRealTime, sink_Allocate, sink_OnDataUnit, sink_OnEnd};

static void test_writer(void)
{
    HRESULT (WINAPI *create)(IUnknown *, IWMWriter **) = (void *)proc("WMCreateWriter");
    IWMWriter *writer = NULL;
    IWMWriterAdvanced3 *adv = NULL;
    struct sink s[3];
    IWMWriterSink *got;
    DWORD d, i;
    BOOL b;
    GUID bad = {0x12345678, 0x1234, 0x1234, {1, 2, 3, 4, 5, 6, 7, 8}};
    IUnknown *unk;
    HRESULT hr;

    if (!create) return;
    hr = create(NULL, &writer);
    CHECKF(hr == S_OK && writer, "WMCreateWriter (%08lx)", hr);
    if (!writer) return;
    hr = IWMWriter_QueryInterface(writer, &IID_IWMWriterAdvanced3, (void **)&adv);
    CHECKF(hr == S_OK, "IWMWriterAdvanced3 (%08lx)", hr);
    unk = (IUnknown *)1;
    hr = IWMWriter_QueryInterface(writer, &bad, (void **)&unk);
    CHECKF(hr == E_NOINTERFACE && !unk, "an unknown interface = E_NOINTERFACE, NULL");
    if (!adv) { IWMWriter_Release(writer); return; }

    d = 0; hr = IWMWriterAdvanced3_GetSyncTolerance(adv, &d);
    CHECKF(hr == S_OK && d == 3000, "sync tolerance defaults to 3000 ms (%08lx %lu)", hr, d);
    hr = IWMWriterAdvanced3_SetSyncTolerance(adv, 250);
    IWMWriterAdvanced3_GetSyncTolerance(adv, &d);
    CHECKF(hr == S_OK && d == 250, "sync tolerance round trip (%lu)", d);
    CHECKF(IWMWriterAdvanced3_GetSyncTolerance(adv, NULL) == E_INVALIDARG, "GetSyncTolerance(NULL) = E_INVALIDARG");

    b = 5; hr = IWMWriterAdvanced3_IsRealTime(adv, &b);
    CHECKF(hr == S_OK && b == FALSE, "the writer is not real time by default (%08lx %d)", hr, b);
    hr = IWMWriterAdvanced3_SetLiveSource(adv, TRUE);
    b = 5; IWMWriterAdvanced3_IsRealTime(adv, &b);
    CHECKF(hr == S_OK && b == TRUE, "a live source makes it real time (%08lx %d)", hr, b);
    IWMWriterAdvanced3_SetLiveSource(adv, 5);
    b = 5; IWMWriterAdvanced3_IsRealTime(adv, &b);
    CHECKF(b == TRUE, "a non-zero live source flag is TRUE (%d)", b);
    IWMWriterAdvanced3_SetLiveSource(adv, FALSE);
    b = 5; IWMWriterAdvanced3_IsRealTime(adv, &b);
    CHECKF(b == FALSE, "and off again (%d)", b);
    CHECKF(IWMWriterAdvanced3_IsRealTime(adv, NULL) == E_INVALIDARG, "IsRealTime(NULL) = E_INVALIDARG");
    CHECKF(IWMWriterAdvanced3_SetNonBlocking(adv) == S_OK, "SetNonBlocking");

    /* the sink list */
    for (i = 0; i < 3; i++) { s[i].iface.lpVtbl = (IWMWriterSinkVtbl *)&sink_vtbl; s[i].ref = 1; }
    d = 99; hr = IWMWriterAdvanced3_GetSinkCount(adv, &d);
    CHECKF(hr == S_OK && d == 0, "no sinks at first (%08lx %lu)", hr, d);
    CHECKF(IWMWriterAdvanced3_GetSinkCount(adv, NULL) == E_INVALIDARG, "GetSinkCount(NULL) = E_INVALIDARG");
    got = (IWMWriterSink *)1;
    hr = IWMWriterAdvanced3_GetSink(adv, 0, &got);
    CHECKF(hr == E_INVALIDARG && !got, "GetSink(0) of none = E_INVALIDARG, NULL (%08lx)", hr);
    CHECKF(IWMWriterAdvanced3_AddSink(adv, NULL) == E_INVALIDARG, "AddSink(NULL) = E_INVALIDARG");
    CHECKF(IWMWriterAdvanced3_RemoveSink(adv, NULL) == E_INVALIDARG, "RemoveSink(NULL) = E_INVALIDARG");
    for (i = 0; i < 3; i++)
    {
        hr = IWMWriterAdvanced3_AddSink(adv, &s[i].iface);
        CHECKF(hr == S_OK && s[i].ref == 2, "AddSink %lu holds the sink (%08lx %ld)", i, hr, s[i].ref);
    }
    IWMWriterAdvanced3_GetSinkCount(adv, &d);
    CHECKF(d == 3, "three sinks (%lu)", d);
    for (i = 0; i < 3; i++)
    {
        got = NULL; hr = IWMWriterAdvanced3_GetSink(adv, i, &got);
        CHECKF(hr == S_OK && got == &s[i].iface && s[i].ref == 3, "GetSink(%lu) is the %luth added and AddRefs it (%08lx %ld)", i, i, hr, s[i].ref);
        if (got) IWMWriterSink_Release(got);
    }
    got = (IWMWriterSink *)1;
    hr = IWMWriterAdvanced3_GetSink(adv, 3, &got);
    CHECKF(hr == E_INVALIDARG && !got, "GetSink(3) = E_INVALIDARG, NULL (%08lx)", hr);
    CHECKF(IWMWriterAdvanced3_GetSink(adv, 0, NULL) == E_INVALIDARG, "GetSink(.., NULL) = E_INVALIDARG");
    hr = IWMWriterAdvanced3_RemoveSink(adv, &s[1].iface);
    CHECKF(hr == S_OK && s[1].ref == 1, "RemoveSink releases the sink (%08lx %ld)", hr, s[1].ref);
    hr = IWMWriterAdvanced3_RemoveSink(adv, &s[1].iface);
    CHECKF(hr == E_INVALIDARG, "RemoveSink of a sink that is not there = E_INVALIDARG (%08lx)", hr);
    IWMWriterAdvanced3_GetSinkCount(adv, &d);
    CHECKF(d == 2, "two sinks left (%lu)", d);
    got = NULL; IWMWriterAdvanced3_GetSink(adv, 1, &got);
    CHECKF(got == &s[2].iface, "the list closed up: sink 1 is the third added");
    if (got) IWMWriterSink_Release(got);
    got = NULL; IWMWriterAdvanced3_GetSink(adv, 0, &got);
    CHECKF(got == &s[0].iface, "and sink 0 is still the first");
    if (got) IWMWriterSink_Release(got);

    /* the writer lets the sinks go when it is released */
    IWMWriterAdvanced3_Release(adv);
    IWMWriter_Release(writer);
    CHECKF(s[0].ref == 1 && s[2].ref == 1, "releasing the writer releases its sinks (%ld %ld)", s[0].ref, s[2].ref);
}

int main(int argc, char **argv)
{
    WCHAR sample[MAX_PATH];

    CoInitialize(NULL);
    test_url_checks();
    test_content_protected(argc > 1 && MultiByteToWideChar(CP_ACP, 0, argv[1], -1, sample, MAX_PATH) ? sample : NULL);
    test_profile_manager();
    test_writer();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
