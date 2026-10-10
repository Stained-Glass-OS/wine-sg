/* wmvcore writer header information and preprocessing (patch 2866), run by
 * test/wmvw-header-gate.sh: IWMHeaderInfo, IWMHeaderInfo2 and IWMHeaderInfo3
 * of an IWMWriter (attributes, markers, scripts, codec info), and the
 * IWMWriterPreprocess argument and state checks.
 *
 *   wmvw-header-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#include <stdio.h>
#include <string.h>

#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
#define E_ASF_NOTFOUND ((HRESULT)0xc00d07f0)
#define E_INVALIDPROFILE ((HRESULT)0xc00d0bc6)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[400]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECK_HR(hr, exp, what) do { HRESULT _h = (hr); CHECKF(_h == (HRESULT)(exp), "%s = %08lx (got %08lx)", what, (unsigned long)(exp), (unsigned long)_h); } while (0)

static void test_attributes(IWMHeaderInfo3 *h)
{
    WORD count, stream, len, idx[4], n, lang, nlen;
    WMT_ATTR_DATATYPE type;
    BYTE buf[64];
    WCHAR name[32];
    DWORD dw = 0x12345678, dlen;
    QWORD qw = 0x1122334455667788ull;
    GUID guid = {1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 11}};
    HRESULT hr;
    static const struct { const WCHAR *name; WMT_ATTR_DATATYPE type; WORD len; HRESULT hr; const char *what; } sets[] = {
        {L"A/word", WMT_TYPE_WORD, 2, S_OK, "WORD of 2 bytes"}, {L"A/word", WMT_TYPE_WORD, 4, E_INVALIDARG, "WORD of 4 bytes"},
        {L"A/dword", WMT_TYPE_DWORD, 4, S_OK, "DWORD of 4 bytes"}, {L"A/dword2", WMT_TYPE_DWORD, 3, E_INVALIDARG, "DWORD of 3 bytes"},
        {L"A/bool", WMT_TYPE_BOOL, 4, S_OK, "BOOL of 4 bytes"}, {L"A/bool2", WMT_TYPE_BOOL, 2, E_INVALIDARG, "BOOL of 2 bytes"},
        {L"A/qword", WMT_TYPE_QWORD, 8, S_OK, "QWORD of 8 bytes"}, {L"A/qword2", WMT_TYPE_QWORD, 4, E_INVALIDARG, "QWORD of 4 bytes"},
        {L"A/guid", WMT_TYPE_GUID, 16, S_OK, "GUID of 16 bytes"}, {L"A/guid2", WMT_TYPE_GUID, 8, E_INVALIDARG, "GUID of 8 bytes"},
        {L"A/str", WMT_TYPE_STRING, 4, S_OK, "STRING of 4 bytes"}, {L"A/str2", WMT_TYPE_STRING, 3, E_INVALIDARG, "STRING of 3 bytes"},
        {L"A/bin", WMT_TYPE_BINARY, 3, S_OK, "BINARY of 3 bytes"}, {L"A/bin2", WMT_TYPE_BINARY, 0, S_OK, "BINARY of 0 bytes"},
        {L"", WMT_TYPE_DWORD, 4, E_INVALIDARG, "empty name"}, {L"A/type", (WMT_ATTR_DATATYPE)9, 4, E_INVALIDARG, "unknown type"},
    };
    unsigned int i;

    count = 9;
    CHECK_HR(IWMHeaderInfo3_GetAttributeCount(h, 0, &count), S_OK, "GetAttributeCount");
    CHECKF(count == 0, "no attributes at first (%u)", count);
    CHECK_HR(IWMHeaderInfo3_GetAttributeCount(h, 0, NULL), E_INVALIDARG, "GetAttributeCount(NULL)");

    CHECK_HR(IWMHeaderInfo3_SetAttribute(h, 0, L"Title", WMT_TYPE_STRING, (BYTE *)L"Hello", 12), S_OK, "SetAttribute(Title)");
    IWMHeaderInfo3_GetAttributeCount(h, 0, &count);
    CHECKF(count == 1, "one attribute (%u)", count);
    stream = 0; len = 0;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, &stream, L"Title", &type, NULL, &len), S_OK, "GetAttributeByName size");
    CHECKF(len == 12 && type == WMT_TYPE_STRING, "size 12 and type STRING (%u, %d)", len, type);
    len = 11;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, &stream, L"Title", &type, buf, &len), ASF_E_BUFFERTOOSMALL, "GetAttributeByName short buffer");
    CHECKF(len == 12, "short buffer reports 12 (%u)", len);
    memset(buf, 0, sizeof(buf));
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, &stream, L"tItLe", &type, buf, &len), S_OK, "GetAttributeByName (other case)");
    CHECKF(!wcscmp((WCHAR *)buf, L"Hello"), "value Hello");
    len = 12;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, &stream, L"Nothing", &type, buf, &len), E_ASF_NOTFOUND, "GetAttributeByName(absent)");
    stream = 1;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, &stream, L"Title", &type, buf, &len), E_ASF_NOTFOUND, "GetAttributeByName(Title of stream 1)");
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, NULL, L"Title", &type, buf, &len), E_INVALIDARG, "GetAttributeByName(NULL stream)");
    stream = 0;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, &stream, NULL, &type, buf, &len), E_INVALIDARG, "GetAttributeByName(NULL name)");

    CHECK_HR(IWMHeaderInfo3_SetAttribute(h, 0, L"TITLE", WMT_TYPE_STRING, (BYTE *)L"Hi", 6), S_OK, "SetAttribute(TITLE) replaces");
    IWMHeaderInfo3_GetAttributeCount(h, 0, &count);
    CHECKF(count == 1, "still one attribute (%u)", count);
    len = 64;
    IWMHeaderInfo3_GetAttributeByName(h, &stream, L"Title", &type, buf, &len);
    CHECKF(len == 6 && !wcscmp((WCHAR *)buf, L"Hi"), "new value Hi");
    CHECK_HR(IWMHeaderInfo3_SetAttribute(h, 0, NULL, WMT_TYPE_DWORD, (BYTE *)&dw, 4), E_INVALIDARG, "SetAttribute(NULL name)");

    for (i = 0; i < sizeof(sets) / sizeof(sets[0]); i++)
    {
        BYTE data[16] = {1, 2, 3, 4, 5, 6, 7, 8};
        hr = IWMHeaderInfo3_SetAttribute(h, 0, sets[i].name, sets[i].type, data, sets[i].len);
        CHECKF(hr == sets[i].hr, "SetAttribute: %s = %08lx (got %08lx)", sets[i].what, (unsigned long)sets[i].hr, (unsigned long)hr);
    }
    CHECK_HR(IWMHeaderInfo3_SetAttribute(h, 0, L"X", WMT_TYPE_DWORD, NULL, 4), E_INVALIDARG, "SetAttribute(NULL value)");
    IWMHeaderInfo3_SetAttribute(h, 0, L"A/dword", WMT_TYPE_DWORD, (BYTE *)&dw, 4);
    IWMHeaderInfo3_SetAttribute(h, 0, L"A/qword", WMT_TYPE_QWORD, (BYTE *)&qw, 8);
    IWMHeaderInfo3_SetAttribute(h, 0, L"A/guid", WMT_TYPE_GUID, (BYTE *)&guid, 16);
    len = 64;
    IWMHeaderInfo3_GetAttributeByName(h, &stream, L"A/dword", &type, buf, &len);
    CHECKF(len == 4 && type == WMT_TYPE_DWORD && *(DWORD *)buf == dw, "DWORD value round trip");
    len = 64;
    IWMHeaderInfo3_GetAttributeByName(h, &stream, L"A/qword", &type, buf, &len);
    CHECKF(len == 8 && type == WMT_TYPE_QWORD && *(QWORD *)buf == qw, "QWORD value round trip");
    len = 64;
    IWMHeaderInfo3_GetAttributeByName(h, &stream, L"A/guid", &type, buf, &len);
    CHECKF(len == 16 && type == WMT_TYPE_GUID && !memcmp(buf, &guid, 16), "GUID value round trip");
    len = 64;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByName(h, &stream, L"A/bin2", &type, buf, &len), S_OK, "empty BINARY");
    CHECKF(len == 0 && type == WMT_TYPE_BINARY, "empty BINARY has length 0 (%u)", len);

    /* stream attributes */
    CHECK_HR(IWMHeaderInfo3_SetAttribute(h, 1, L"S/one", WMT_TYPE_DWORD, (BYTE *)&dw, 4), S_OK, "SetAttribute(stream 1)");
    IWMHeaderInfo3_GetAttributeCount(h, 1, &count);
    CHECKF(count == 1, "one attribute of stream 1 (%u)", count);
    IWMHeaderInfo3_GetAttributeCount(h, 0, &count);
    n = count;
    IWMHeaderInfo3_GetAttributeCountEx(h, 0xffff, &count);
    CHECKF(count == n + 1, "GetAttributeCountEx(all) counts the stream's too (%u, %u)", count, n);
    IWMHeaderInfo3_GetAttributeCountEx(h, 1, &count);
    CHECKF(count == 1, "GetAttributeCountEx(1) = 1 (%u)", count);

    /* by index */
    stream = 0; nlen = 32; len = 64;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByIndex(h, 0, &stream, name, &nlen, &type, buf, &len), S_OK, "GetAttributeByIndex(0)");
    CHECKF(!wcscmp(name, L"Title") && nlen == 6 && type == WMT_TYPE_STRING && len == 6, "attribute 0 is Title (%ls)", name);
    nlen = 3; len = 64;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByIndex(h, 0, &stream, name, &nlen, &type, buf, &len), ASF_E_BUFFERTOOSMALL, "GetAttributeByIndex short name");
    CHECKF(nlen == 6, "short name reports 6 (%u)", nlen);
    nlen = 32;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByIndex(h, 99, &stream, name, &nlen, &type, buf, &len), E_INVALIDARG, "GetAttributeByIndex(99)");
    stream = 1; nlen = 32; len = 64;
    CHECK_HR(IWMHeaderInfo3_GetAttributeByIndex(h, 0, &stream, name, &nlen, &type, buf, &len), S_OK, "GetAttributeByIndex(stream 1)");
    CHECKF(!wcscmp(name, L"S/one"), "stream 1 attribute 0 is S/one (%ls)", name);

    /* Ex calls */
    {
        WORD index = 99;
        BYTE lb[2] = {5, 0};

        CHECK_HR(IWMHeaderInfo3_AddAttribute(h, 0, L"Title", &index, WMT_TYPE_STRING, 7, (BYTE *)L"Bonjour", 16), S_OK, "AddAttribute (second language)");
        IWMHeaderInfo3_GetAttributeCount(h, 0, &count);
        CHECKF(index == count - 1, "AddAttribute index is the last (%u of %u)", index, count);
        n = 4;
        lang = 7;
        CHECK_HR(IWMHeaderInfo3_GetAttributeIndices(h, 0, L"title", &lang, idx, &n), S_OK, "GetAttributeIndices(Title, language 7)");
        CHECKF(n == 1 && idx[0] == index, "one match, the added one (%u, %u)", n, idx[0]);
        n = 4;
        CHECK_HR(IWMHeaderInfo3_GetAttributeIndices(h, 0, L"title", NULL, idx, &n), S_OK, "GetAttributeIndices(Title)");
        CHECKF(n == 2 && idx[0] == 0 && idx[1] == index, "two matches (%u)", n);
        n = 1;
        CHECK_HR(IWMHeaderInfo3_GetAttributeIndices(h, 0, L"title", NULL, idx, &n), ASF_E_BUFFERTOOSMALL, "GetAttributeIndices short");
        CHECKF(n == 2, "short reports 2 (%u)", n);
        nlen = 32; dlen = 64; lang = 0;
        CHECK_HR(IWMHeaderInfo3_GetAttributeByIndexEx(h, 0, index, name, &nlen, &type, &lang, buf, &dlen), S_OK, "GetAttributeByIndexEx");
        CHECKF(!wcscmp(name, L"Title") && lang == 7 && dlen == 16 && !wcscmp((WCHAR *)buf, L"Bonjour"), "language 7 Bonjour");
        CHECK_HR(IWMHeaderInfo3_ModifyAttribute(h, 0, index, WMT_TYPE_BINARY, 9, lb, 2), S_OK, "ModifyAttribute");
        nlen = 32; dlen = 64;
        IWMHeaderInfo3_GetAttributeByIndexEx(h, 0, index, name, &nlen, &type, &lang, buf, &dlen);
        CHECKF(type == WMT_TYPE_BINARY && lang == 9 && dlen == 2 && buf[0] == 5, "modified to binary, language 9");
        CHECK_HR(IWMHeaderInfo3_ModifyAttribute(h, 0, 99, WMT_TYPE_BINARY, 9, lb, 2), E_INVALIDARG, "ModifyAttribute(99)");
        CHECK_HR(IWMHeaderInfo3_DeleteAttribute(h, 0, index), S_OK, "DeleteAttribute");
        CHECK_HR(IWMHeaderInfo3_DeleteAttribute(h, 0, index), E_INVALIDARG, "DeleteAttribute again");
        IWMHeaderInfo3_GetAttributeCount(h, 0, &n);
        CHECKF(n == count - 1, "one attribute less (%u)", n);
    }
}

static void test_markers_scripts(IWMHeaderInfo3 *h)
{
    WORD count, nlen, tlen, clen;
    WCHAR name[16], cmd[16];
    QWORD time;
    HRESULT hr;

    CHECK_HR(IWMHeaderInfo3_GetMarkerCount(h, NULL), E_INVALIDARG, "GetMarkerCount(NULL)");
    IWMHeaderInfo3_GetMarkerCount(h, &count);
    CHECKF(count == 0, "no markers");
    CHECK_HR(IWMHeaderInfo3_AddMarker(h, L"B", 20000000), S_OK, "AddMarker(B)");
    CHECK_HR(IWMHeaderInfo3_AddMarker(h, L"A", 10000000), S_OK, "AddMarker(A)");
    CHECK_HR(IWMHeaderInfo3_AddMarker(h, L"C", 30000000), S_OK, "AddMarker(C)");
    CHECK_HR(IWMHeaderInfo3_AddMarker(h, NULL, 0), E_INVALIDARG, "AddMarker(NULL)");
    IWMHeaderInfo3_GetMarkerCount(h, &count);
    CHECKF(count == 3, "three markers");
    nlen = 16;
    hr = IWMHeaderInfo3_GetMarker(h, 0, name, &nlen, &time);
    CHECKF(hr == S_OK && !wcscmp(name, L"A") && time == 10000000 && nlen == 2, "marker 0 is A at 1 s (kept in time order)");
    IWMHeaderInfo3_GetMarker(h, 2, name, &nlen, &time);
    CHECKF(!wcscmp(name, L"C") && time == 30000000, "marker 2 is C");
    nlen = 1;
    CHECK_HR(IWMHeaderInfo3_GetMarker(h, 1, name, &nlen, &time), ASF_E_BUFFERTOOSMALL, "GetMarker short");
    CHECKF(nlen == 2, "short reports 2 (%u)", nlen);
    nlen = 16;
    CHECK_HR(IWMHeaderInfo3_GetMarker(h, 3, name, &nlen, &time), E_INVALIDARG, "GetMarker(3)");
    CHECK_HR(IWMHeaderInfo3_RemoveMarker(h, 3), E_INVALIDARG, "RemoveMarker(3)");
    CHECK_HR(IWMHeaderInfo3_RemoveMarker(h, 1), S_OK, "RemoveMarker(1)");
    IWMHeaderInfo3_GetMarker(h, 1, name, &nlen, &time);
    CHECKF(!wcscmp(name, L"C"), "C follows A");

    IWMHeaderInfo3_GetScriptCount(h, &count);
    CHECKF(count == 0, "no scripts");
    CHECK_HR(IWMHeaderInfo3_AddScript(h, L"URL", L"http://x", 50), S_OK, "AddScript");
    CHECK_HR(IWMHeaderInfo3_AddScript(h, L"TEXT", L"first", 10), S_OK, "AddScript(earlier)");
    CHECK_HR(IWMHeaderInfo3_AddScript(h, NULL, L"x", 10), E_INVALIDARG, "AddScript(NULL type)");
    tlen = clen = 16;
    CHECK_HR(IWMHeaderInfo3_GetScript(h, 0, name, &tlen, cmd, &clen, &time), S_OK, "GetScript(0)");
    CHECKF(!wcscmp(name, L"TEXT") && !wcscmp(cmd, L"first") && time == 10 && tlen == 5 && clen == 6, "script 0 is TEXT/first at 10");
    tlen = 2; clen = 16;
    CHECK_HR(IWMHeaderInfo3_GetScript(h, 1, name, &tlen, cmd, &clen, &time), ASF_E_BUFFERTOOSMALL, "GetScript short type");
    CHECKF(tlen == 4 && clen == 9, "short type reports both sizes (%u, %u)", tlen, clen);
    CHECK_HR(IWMHeaderInfo3_GetScript(h, 2, name, &tlen, cmd, &clen, &time), E_INVALIDARG, "GetScript(2)");
    CHECK_HR(IWMHeaderInfo3_RemoveScript(h, 0), S_OK, "RemoveScript(0)");
    IWMHeaderInfo3_GetScriptCount(h, &count);
    CHECKF(count == 1, "one script left");
    CHECK_HR(IWMHeaderInfo3_RemoveScript(h, 1), E_INVALIDARG, "RemoveScript(1)");
}

static void test_codecs(IWMHeaderInfo3 *h)
{
    DWORD count;
    WORD nlen = 16, dlen = 32, ilen = 8;
    WCHAR name[16], desc[32];
    BYTE info[8], data[3] = {7, 8, 9};
    WMT_CODEC_INFO_TYPE type;

    IWMHeaderInfo3_GetCodecInfoCount(h, &count);
    CHECKF(count == 0, "no codec info");
    CHECK_HR(IWMHeaderInfo3_AddCodecInfo(h, L"Codec", L"A codec", WMT_CODECINFO_AUDIO, 3, data), S_OK, "AddCodecInfo");
    CHECK_HR(IWMHeaderInfo3_AddCodecInfo(h, NULL, L"A codec", WMT_CODECINFO_AUDIO, 3, data), E_INVALIDARG, "AddCodecInfo(NULL name)");
    CHECK_HR(IWMHeaderInfo3_AddCodecInfo(h, L"x", L"y", WMT_CODECINFO_AUDIO, 3, NULL), E_INVALIDARG, "AddCodecInfo(NULL info)");
    IWMHeaderInfo3_GetCodecInfoCount(h, &count);
    CHECKF(count == 1, "one codec");
    CHECK_HR(IWMHeaderInfo3_GetCodecInfo(h, 0, &nlen, name, &dlen, desc, &type, &ilen, info), S_OK, "GetCodecInfo");
    CHECKF(!wcscmp(name, L"Codec") && nlen == 6 && !wcscmp(desc, L"A codec") && dlen == 8 && type == WMT_CODECINFO_AUDIO && ilen == 3 && info[0] == 7 && info[2] == 9,
            "codec name, description, type and data");
    nlen = 2; dlen = 32; ilen = 8;
    CHECK_HR(IWMHeaderInfo3_GetCodecInfo(h, 0, &nlen, name, &dlen, desc, &type, &ilen, info), ASF_E_BUFFERTOOSMALL, "GetCodecInfo short name");
    CHECKF(nlen == 6, "short name reports 6 (%u)", nlen);
    CHECK_HR(IWMHeaderInfo3_GetCodecInfo(h, 1, &nlen, name, &dlen, desc, &type, &ilen, info), E_INVALIDARG, "GetCodecInfo(1)");
}

static void test_preprocess(IWMWriter *writer, IWMProfileManager *mgr)
{
    IWMWriterPreprocess *pre;
    IWMProfile *profile;
    IWMStreamConfig *config;
    DWORD passes = 9;
    INSSBuffer *buf;

    CHECK_HR(IWMWriter_QueryInterface(writer, &IID_IWMWriterPreprocess, (void **)&pre), S_OK, "QI IWMWriterPreprocess");
    CHECK_HR(IWMWriterPreprocess_GetMaxPreprocessingPasses(pre, 0, 0, &passes), E_INVALIDPROFILE, "GetMaxPreprocessingPasses (no profile)");
    IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_9_0, &profile);
    IWMProfile_CreateNewStream(profile, &WMMEDIATYPE_Audio, &config);
    IWMProfile_AddStream(profile, config);
    IWMStreamConfig_Release(config);
    IWMWriter_SetProfile(writer, profile);
    IWMProfile_Release(profile);

    CHECK_HR(IWMWriterPreprocess_GetMaxPreprocessingPasses(pre, 0, 0, &passes), S_OK, "GetMaxPreprocessingPasses");
    CHECKF(passes == 0, "no passes for a PCM input (%lu)", passes);
    CHECK_HR(IWMWriterPreprocess_GetMaxPreprocessingPasses(pre, 0, 0, NULL), E_INVALIDARG, "GetMaxPreprocessingPasses(NULL)");
    CHECK_HR(IWMWriterPreprocess_GetMaxPreprocessingPasses(pre, 1, 0, &passes), E_INVALIDARG, "GetMaxPreprocessingPasses(input 1)");
    CHECK_HR(IWMWriterPreprocess_GetMaxPreprocessingPasses(pre, 0, 1, &passes), E_INVALIDARG, "GetMaxPreprocessingPasses(flags 1)");
    CHECK_HR(IWMWriterPreprocess_SetNumPreprocessingPasses(pre, 0, 0, 0), S_OK, "SetNumPreprocessingPasses(0)");
    CHECK_HR(IWMWriterPreprocess_SetNumPreprocessingPasses(pre, 0, 0, 1), E_INVALIDARG, "SetNumPreprocessingPasses(1)");
    CHECK_HR(IWMWriterPreprocess_SetNumPreprocessingPasses(pre, 3, 0, 0), E_INVALIDARG, "SetNumPreprocessingPasses(input 3)");
    CHECK_HR(IWMWriterPreprocess_BeginPreprocessingPass(pre, 0, 0), NS_E_INVALID_REQUEST, "BeginPreprocessingPass");
    CHECK_HR(IWMWriterPreprocess_BeginPreprocessingPass(pre, 2, 0), E_INVALIDARG, "BeginPreprocessingPass(input 2)");
    IWMWriter_AllocateSample(writer, 10, &buf);
    CHECK_HR(IWMWriterPreprocess_PreprocessSample(pre, 0, 0, 0, buf), NS_E_INVALID_REQUEST, "PreprocessSample");
    CHECK_HR(IWMWriterPreprocess_PreprocessSample(pre, 0, 0, 0, NULL), E_INVALIDARG, "PreprocessSample(NULL)");
    INSSBuffer_Release(buf);
    CHECK_HR(IWMWriterPreprocess_EndPreprocessingPass(pre, 0, 0), NS_E_INVALID_REQUEST, "EndPreprocessingPass");
    IWMWriterPreprocess_Release(pre);
}

int main(void)
{
    HRESULT (WINAPI *create_mgr)(IWMProfileManager **);
    HRESULT (WINAPI *create_writer)(IUnknown *, IWMWriter **);
    IWMProfileManager *mgr = NULL;
    IWMWriter *writer = NULL;
    IWMHeaderInfo *h1;
    IWMHeaderInfo2 *h2;
    IWMHeaderInfo3 *h3;
    ULONG ref, ref2;
    HMODULE mod;

    CoInitialize(NULL);
    mod = LoadLibraryW(L"wmvcore.dll");
    create_mgr = (void *)GetProcAddress(mod, "WMCreateProfileManager");
    create_writer = (void *)GetProcAddress(mod, "WMCreateWriter");
    if (!create_mgr || !create_writer) { check(0, "wmvcore.dll exports"); goto out; }
    create_mgr(&mgr);
    create_writer(NULL, &writer);
    if (!writer || !mgr) { check(0, "creation"); goto out; }

    CHECK_HR(IWMWriter_QueryInterface(writer, &IID_IWMHeaderInfo, (void **)&h1), S_OK, "QI IWMHeaderInfo");
    CHECK_HR(IWMWriter_QueryInterface(writer, &IID_IWMHeaderInfo2, (void **)&h2), S_OK, "QI IWMHeaderInfo2");
    CHECK_HR(IWMWriter_QueryInterface(writer, &IID_IWMHeaderInfo3, (void **)&h3), S_OK, "QI IWMHeaderInfo3");
    ref = IWMWriter_AddRef(writer);
    IWMHeaderInfo3_AddRef(h3);
    ref2 = IWMHeaderInfo3_AddRef(h3);
    CHECKF(ref2 == ref + 2, "the header object shares the writer's reference count (%lu, %lu)", ref, ref2);
    IWMHeaderInfo3_Release(h3);
    IWMHeaderInfo3_Release(h3);
    IWMWriter_Release(writer);

    test_attributes(h3);
    test_markers_scripts(h3);
    test_codecs(h3);
    IWMHeaderInfo_Release(h1);
    IWMHeaderInfo2_Release(h2);
    IWMHeaderInfo3_Release(h3);
    test_preprocess(writer, mgr);
    IWMWriter_Release(writer);
    IWMProfileManager_Release(mgr);
out:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
