/* tdh gate: TdhLoadManifest / TdhUnloadManifest / TdhGetEventInformation from an instrumentation manifest */
#include <windows.h>
#include <wmistr.h>
#include <initguid.h>
#include <evntrace.h>
#include <evntprov.h>
#include <evntcons.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { USHORT Flags_lo; } dummy;
typedef struct
{
    ULONG Flags, NameOffset;
    union { struct { USHORT InType, OutType; ULONG MapNameOffset; } n; struct { USHORT StructStartIndex, NumOfStructMembers; ULONG padding; } s; } u;
    USHORT count, length;
    ULONG Reserved;
} PROP;
typedef struct
{
    GUID ProviderGuid, EventGuid;
    EVENT_DESCRIPTOR EventDescriptor;
    ULONG DecodingSource, ProviderNameOffset, LevelNameOffset, ChannelNameOffset, KeywordsNameOffset, TaskNameOffset,
          OpcodeNameOffset, EventMessageOffset, ProviderMessageOffset, BinaryXMLOffset, BinaryXMLSize,
          ActivityIDNameOffset, RelatedActivityIDNameOffset, PropertyCount, TopLevelPropertyCount, Flags;
    PROP props[1];
} INFO;

ULONG WINAPI TdhLoadManifest(WCHAR *);
ULONG WINAPI TdhUnloadManifest(WCHAR *);
ULONG WINAPI TdhLoadManifestFromBinary(WCHAR *);
ULONG WINAPI TdhGetEventInformation(EVENT_RECORD *, ULONG, void *, INFO *, ULONG *);

DEFINE_GUID(prov_guid, 0x7c3b9f10, 0x1111, 0x4222, 0x93, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);

static const char manifest[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<instrumentationManifest xmlns=\"http://schemas.microsoft.com/win/2004/08/events\" xmlns:win=\"http://manifests.microsoft.com/win/2004/08/windows/events\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
" <instrumentation><events>\n"
"  <provider name=\"SG-Test-Provider\" guid=\"{7c3b9f10-1111-4222-9333-445566778899}\" symbol=\"P\" resourceFileName=\"x.dll\" messageFileName=\"x.dll\" message=\"$(string.Provider)\">\n"
"   <channels><channel name=\"SG-Test-Provider/Operational\" chid=\"c1\" type=\"Operational\" enabled=\"true\"/>\n"
"             <channel name=\"SG-Test-Provider/Admin\" chid=\"c2\" type=\"Admin\" enabled=\"true\" value=\"20\"/></channels>\n"
"   <keywords><keyword name=\"Alpha\" mask=\"0x1\" message=\"$(string.Alpha)\"/><keyword name=\"Beta\" mask=\"0x10\"/></keywords>\n"
"   <tasks><task name=\"Work\" value=\"7\" message=\"$(string.Work)\"/></tasks>\n"
"   <opcodes><opcode name=\"Poke\" value=\"11\" message=\"$(string.Poke)\"/></opcodes>\n"
"   <levels><level name=\"Chatty\" value=\"17\" message=\"$(string.Chatty)\"/></levels>\n"
"   <templates>\n"
"    <template tid=\"T1\"><data name=\"Name\" inType=\"win:UnicodeString\"/><data name=\"Code\" inType=\"win:UInt32\" outType=\"win:HexInt32\"/>\n"
"      <data name=\"Len\" inType=\"win:UInt16\"/><data name=\"Blob\" inType=\"win:Binary\" length=\"Len\"/>\n"
"      <data name=\"N\" inType=\"win:UInt8\"/><data name=\"Items\" inType=\"win:Int32\" count=\"N\"/>\n"
"      <struct name=\"Pair\" count=\"2\"><data name=\"A\" inType=\"win:Int64\"/><data name=\"B\" inType=\"win:GUID\" map=\"MyMap\"/></struct>\n"
"      <data name=\"Tail\" inType=\"win:AnsiString\" length=\"8\"/></template>\n"
"    <template tid=\"T2\"><data name=\"Only\" inType=\"win:Boolean\"/></template>\n"
"   </templates>\n"
"   <events>\n"
"    <event value=\"10\" version=\"0\" level=\"win:Warning\" task=\"Work\" opcode=\"Poke\" channel=\"c1\" keywords=\"Alpha Beta\" template=\"T1\" message=\"$(string.Ev10)\" symbol=\"E10\"/>\n"
"    <event value=\"11\" version=\"1\" level=\"Chatty\" opcode=\"win:Start\" channel=\"c2\" keywords=\"win:AuditSuccess\" template=\"T2\" symbol=\"E11\"/>\n"
"    <event value=\"12\" version=\"0\" message=\"$(string.OnlyDe)\" symbol=\"E12\"/>\n"
"   </events>\n"
"  </provider>\n"
" </events></instrumentation>\n"
" <localization><resources culture=\"en-US\"><stringTable>\n"
"  <string id=\"Provider\" value=\"SG Test Provider message\"/><string id=\"Alpha\" value=\"Alpha keyword\"/>\n"
"  <string id=\"Work\" value=\"Work task\"/><string id=\"Poke\" value=\"Poke opcode\"/><string id=\"Chatty\" value=\"Chatty level\"/>\n"
"  <string id=\"Ev10\" value=\"Event ten %1 code %2\"/></stringTable></resources>\n"
"  <resources culture=\"de-DE\"><stringTable><string id=\"Ev10\" value=\"Ereignis zehn\"/><string id=\"OnlyDe\" value=\"nur deutsch\"/></stringTable></resources></localization>\n"
"</instrumentationManifest>\n";

static const WCHAR *str(INFO *info, ULONG off) { return off ? (const WCHAR *)((BYTE *)info + off) : NULL; }

static ULONG get(USHORT id, UCHAR ver, INFO **out)
{
    EVENT_RECORD rec = { 0 };
    ULONG size = 0, ret;
    INFO *info;

    rec.EventHeader.ProviderId = prov_guid;
    rec.EventHeader.EventDescriptor.Id = id;
    rec.EventHeader.EventDescriptor.Version = ver;
    ret = TdhGetEventInformation(&rec, 0, NULL, NULL, &size);
    if (ret != ERROR_INSUFFICIENT_BUFFER) return ret;
    info = calloc(1, size);
    memset(info, 0xcc, size);
    ret = TdhGetEventInformation(&rec, 0, NULL, info, &size);
    *out = info;
    return ret;
}

int main(void)
{
    WCHAR path[MAX_PATH] = L"C:\\sgtdh.man", bad[MAX_PATH] = L"C:\\sgtdh-bad.man";
    HANDLE f;
    DWORD got;
    INFO *info = NULL;
    EVENT_RECORD rec = { 0 };
    ULONG ret, size;
    const WCHAR *kw;

    setvbuf(stdout, NULL, _IONBF, 0);

    CHECK(TdhLoadManifest(NULL) == ERROR_INVALID_PARAMETER, "NULL manifest");
    CHECK(TdhLoadManifest(L"C:\\no\\such.man") == ERROR_FILE_NOT_FOUND, "missing manifest");
    f = CreateFileW(bad, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "this is <not xml", 16, &got, NULL);
    CloseHandle(f);
    CHECK(TdhLoadManifest(bad) == ERROR_XML_PARSE_ERROR, "invalid xml");
    f = CreateFileW(bad, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "<a><b/></a>", 11, &got, NULL);
    CloseHandle(f);
    CHECK(TdhLoadManifest(bad) == ERROR_XML_PARSE_ERROR, "xml without a provider");

    ret = get(10, 0, &info);
    CHECK(ret == ERROR_NOT_FOUND, "event before the manifest is loaded: %lu", ret);

    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, manifest, sizeof(manifest) - 1, &got, NULL);
    CloseHandle(f);
    ret = TdhLoadManifest(path);
    CHECK(ret == ERROR_SUCCESS, "TdhLoadManifest %lu", ret);

    ret = get(10, 0, &info);
    CHECK(ret == ERROR_SUCCESS && info, "TdhGetEventInformation(10) %lu", ret);
    if (ret == ERROR_SUCCESS)
    {
        CHECK(IsEqualGUID(&info->ProviderGuid, &prov_guid), "provider guid");
        CHECK(info->EventDescriptor.Id == 10 && info->EventDescriptor.Version == 0, "descriptor id/version");
        CHECK(info->EventDescriptor.Level == 3, "level %u", info->EventDescriptor.Level);
        CHECK(info->EventDescriptor.Task == 7, "task %u", info->EventDescriptor.Task);
        CHECK(info->EventDescriptor.Opcode == 11, "opcode %u", info->EventDescriptor.Opcode);
        CHECK(info->EventDescriptor.Channel == 16, "channel %u", info->EventDescriptor.Channel);
        CHECK(info->EventDescriptor.Keyword == 0x11, "keywords %llx", (unsigned long long)info->EventDescriptor.Keyword);
        CHECK(str(info, info->ProviderNameOffset) && !wcscmp(str(info, info->ProviderNameOffset), L"SG-Test-Provider"), "provider name");
        CHECK(str(info, info->ProviderMessageOffset) && !wcscmp(str(info, info->ProviderMessageOffset), L"SG Test Provider message"), "provider message");
        CHECK(str(info, info->LevelNameOffset) && !wcscmp(str(info, info->LevelNameOffset), L"Warning"), "level name");
        CHECK(str(info, info->TaskNameOffset) && !wcscmp(str(info, info->TaskNameOffset), L"Work task"), "task name");
        CHECK(str(info, info->OpcodeNameOffset) && !wcscmp(str(info, info->OpcodeNameOffset), L"Poke opcode"), "opcode name");
        CHECK(str(info, info->ChannelNameOffset) && !wcscmp(str(info, info->ChannelNameOffset), L"SG-Test-Provider/Operational"), "channel name");
        CHECK(str(info, info->EventMessageOffset) && !wcscmp(str(info, info->EventMessageOffset), L"Event ten %1 code %2"), "event message (first culture)");
        kw = str(info, info->KeywordsNameOffset);
        CHECK(kw && !wcscmp(kw, L"Alpha keyword") && !wcscmp(kw + wcslen(kw) + 1, L"Beta") && !kw[wcslen(kw) + 1 + 5], "keyword names");
        CHECK(info->DecodingSource == 0 && info->Flags == 1, "decoding source %lu flags %lu", info->DecodingSource, info->Flags);
        CHECK(info->TopLevelPropertyCount == 8 && info->PropertyCount == 10, "property counts %lu/%lu", info->TopLevelPropertyCount, info->PropertyCount);
        /* Name, Code, Len, Blob, N, Items, Pair(struct), Tail; the struct's members follow */
        CHECK(!wcscmp(str(info, info->props[0].NameOffset), L"Name") && info->props[0].u.n.InType == 1 && info->props[0].u.n.OutType == 1, "Name property");
        CHECK(info->props[1].u.n.InType == 8 && info->props[1].u.n.OutType == 18, "Code: UInt32 as HexInt32: %u/%u", info->props[1].u.n.InType, info->props[1].u.n.OutType);
        CHECK(info->props[3].u.n.InType == 14 && (info->props[3].Flags & 2) && info->props[3].length == 2, "Blob length from property #2: flags %lx len %u", info->props[3].Flags, info->props[3].length);
        CHECK(info->props[5].u.n.InType == 7 && (info->props[5].Flags & 4) && info->props[5].count == 4, "Items count from property #4: flags %lx count %u", info->props[5].Flags, info->props[5].count);
        CHECK((info->props[6].Flags & 1) && info->props[6].u.s.StructStartIndex == 8 && info->props[6].u.s.NumOfStructMembers == 2 && info->props[6].count == 2,
              "struct Pair: flags %lx start %u members %u count %u", info->props[6].Flags, info->props[6].u.s.StructStartIndex, info->props[6].u.s.NumOfStructMembers, info->props[6].count);
        CHECK(!wcscmp(str(info, info->props[8].NameOffset), L"A") && info->props[8].u.n.InType == 9 && info->props[8].u.n.OutType == 9, "struct member A");
        CHECK(info->props[9].u.n.InType == 15 && info->props[9].u.n.OutType == 14 && str(info, info->props[9].u.n.MapNameOffset) && !wcscmp(str(info, info->props[9].u.n.MapNameOffset), L"MyMap"), "struct member B with map");
        CHECK(info->props[7].u.n.InType == 2 && info->props[7].length == 8 && (info->props[7].Flags & 0x10), "Tail fixed length: flags %lx len %u", info->props[7].Flags, info->props[7].length);
    }
    free(info);

    ret = get(11, 1, &info);
    CHECK(ret == ERROR_SUCCESS, "event 11 v1 %lu", ret);
    if (ret == ERROR_SUCCESS)
    {
        CHECK(info->EventDescriptor.Level == 17 && info->EventDescriptor.Opcode == 1 && info->EventDescriptor.Channel == 20, "custom level/std opcode/explicit channel: %u %u %u",
              info->EventDescriptor.Level, info->EventDescriptor.Opcode, info->EventDescriptor.Channel);
        CHECK(info->EventDescriptor.Keyword == 0x0020000000000000ull, "standard keyword %llx", (unsigned long long)info->EventDescriptor.Keyword);
        CHECK(str(info, info->LevelNameOffset) && !wcscmp(str(info, info->LevelNameOffset), L"Chatty level"), "custom level name");
        CHECK(str(info, info->OpcodeNameOffset) && !wcscmp(str(info, info->OpcodeNameOffset), L"Start"), "standard opcode name");
        CHECK(info->PropertyCount == 1 && info->props[0].u.n.InType == 13, "T2 property");
    }
    free(info);
    CHECK(get(11, 0, &info) == ERROR_NOT_FOUND, "event 11 version 0 does not exist");
    ret = get(12, 0, &info);
    CHECK(ret == ERROR_SUCCESS && info->PropertyCount == 0, "event without a template: %lu", ret);
    CHECK(ret == ERROR_SUCCESS && info->EventMessageOffset == 0, "message from a later culture was used");
    free(info);
    CHECK(get(99, 0, &info) == ERROR_NOT_FOUND, "unknown event id");

    /* parameters */
    rec.EventHeader.ProviderId = prov_guid;
    rec.EventHeader.EventDescriptor.Id = 10;
    size = 4;
    CHECK(TdhGetEventInformation(&rec, 0, NULL, (INFO *)&rec, &size) == ERROR_INSUFFICIENT_BUFFER && size > 4, "short buffer reports the size");
    CHECK(TdhGetEventInformation(NULL, 0, NULL, NULL, &size) == ERROR_INVALID_PARAMETER, "NULL event");
    CHECK(TdhGetEventInformation(&rec, 0, NULL, NULL, NULL) == ERROR_INVALID_PARAMETER, "NULL size");
    rec.EventHeader.Flags = EVENT_HEADER_FLAG_TRACE_MESSAGE;
    size = 0;
    CHECK(TdhGetEventInformation(&rec, 0, NULL, NULL, &size) == ERROR_NOT_FOUND, "WPP event");

    CHECK(TdhUnloadManifest(NULL) == ERROR_INVALID_PARAMETER, "unload NULL");
    CHECK(TdhUnloadManifest(L"C:\\other.man") == ERROR_NOT_FOUND, "unload a manifest that is not loaded");
    CHECK(TdhUnloadManifest(path) == ERROR_SUCCESS, "unload");
    CHECK(get(10, 0, &info) == ERROR_NOT_FOUND, "event after unload");

    /* the binary variant: documented error codes */
    CHECK(TdhLoadManifestFromBinary(NULL) == ERROR_INVALID_PARAMETER, "binary NULL");
    CHECK(TdhLoadManifestFromBinary(L"C:\\no\\such.dll") == ERROR_FILE_NOT_FOUND, "binary missing");
    CHECK(TdhLoadManifestFromBinary(L"C:\\windows\\system32\\kernel32.dll") == ERROR_RESOURCE_NOT_FOUND, "binary without WEVT_TEMPLATE");

    DeleteFileW(path);
    DeleteFileW(bad);
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
