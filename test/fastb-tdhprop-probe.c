/* tdh property functions on events described by a loaded manifest (patch 3743). Where Windows' exact text is
 * not documented the expected text is the one this implementation documents (see the notes of the patch). */
#include <windows.h>
#include <wmistr.h>
#include <initguid.h>
#include <evntrace.h>
#include <evntprov.h>
#include <evntcons.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { ULONGLONG PropertyName; ULONG ArrayIndex; ULONG Reserved; } PDD;
typedef struct { ULONG OutputOffset; ULONG Value; } MAP_ENTRY;
typedef struct { ULONG NameOffset, Flag, EntryCount, ValueType; MAP_ENTRY Entries[1]; } MAP_INFO;
typedef struct { ULONG NumberOfEvents, Reserved; EVENT_DESCRIPTOR Descs[1]; } PROV_EVENTS;

static ULONG (WINAPI *pLoad)(WCHAR *);
static ULONG (WINAPI *pSize)(EVENT_RECORD *, ULONG, void *, ULONG, PDD *, ULONG *);
static ULONG (WINAPI *pGet)(EVENT_RECORD *, ULONG, void *, ULONG, PDD *, ULONG, BYTE *);
static ULONG (WINAPI *pFormat)(void *, void *, ULONG, USHORT, USHORT, USHORT, USHORT, BYTE *, ULONG *, WCHAR *, USHORT *);
static ULONG (WINAPI *pMap)(EVENT_RECORD *, WCHAR *, void *, ULONG *);
static ULONG (WINAPI *pEnum)(GUID *, void *, ULONG *);

DEFINE_GUID(prov_guid, 0x7c3b9f10, 0x2222, 0x4222, 0x93, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);
DEFINE_GUID(some_guid, 0x01020304, 0x0506, 0x0708, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10);
DEFINE_GUID(other_guid, 0x7c3b9f10, 0x9999, 0x4222, 0x93, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);

static const char manifest[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<instrumentationManifest xmlns=\"http://schemas.microsoft.com/win/2004/08/events\" xmlns:win=\"http://manifests.microsoft.com/win/2004/08/windows/events\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
" <instrumentation><events>\n"
"  <provider name=\"SG-Prop-Provider\" guid=\"{7c3b9f10-2222-4222-9333-445566778899}\" symbol=\"P\" resourceFileName=\"x.dll\" messageFileName=\"x.dll\">\n"
"   <levels><level name=\"Chatty\" value=\"17\"/></levels>\n"
"   <maps>\n"
"    <valueMap name=\"Level\"><map value=\"0\" message=\"$(string.Off)\"/><map value=\"1\" message=\"$(string.On)\"/></valueMap>\n"
"    <bitMap name=\"Flags\"><map value=\"0x1\" message=\"$(string.Read)\"/><map value=\"0x2\" message=\"$(string.Write)\"/><map value=\"0x4\" message=\"$(string.Exec)\"/></bitMap>\n"
"   </maps>\n"
"   <templates>\n"
"    <template tid=\"T1\"><data name=\"Name\" inType=\"win:UnicodeString\"/><data name=\"Code\" inType=\"win:UInt32\" outType=\"win:HexInt32\"/>\n"
"      <data name=\"Len\" inType=\"win:UInt16\"/><data name=\"Blob\" inType=\"win:Binary\" length=\"Len\"/>\n"
"      <data name=\"N\" inType=\"win:UInt8\"/><data name=\"Items\" inType=\"win:Int32\" count=\"N\"/>\n"
"      <data name=\"Tail\" inType=\"win:AnsiString\" length=\"4\"/>\n"
"      <data name=\"State\" inType=\"win:UInt8\" map=\"Level\"/><data name=\"Perm\" inType=\"win:UInt32\" map=\"Flags\"/>\n"
"      <data name=\"Id\" inType=\"win:GUID\"/><data name=\"Ip\" inType=\"win:UInt32\" outType=\"win:IPv4\"/>\n"
"      <data name=\"Flag\" inType=\"win:Boolean\"/><data name=\"Neg\" inType=\"win:Int16\"/></template>\n"
"   </templates>\n"
"   <events>\n"
"    <event value=\"10\" version=\"0\" level=\"win:Warning\" template=\"T1\" symbol=\"E10\"/>\n"
"    <event value=\"12\" version=\"2\" level=\"Chatty\" opcode=\"win:Start\" symbol=\"E12\"/>\n"
"   </events>\n"
"  </provider>\n"
" </events></instrumentation>\n"
" <localization><resources culture=\"en-US\"><stringTable>\n"
"  <string id=\"Off\" value=\"Off\"/><string id=\"On\" value=\"On\"/><string id=\"Read\" value=\"Read\"/>\n"
"  <string id=\"Write\" value=\"Write\"/><string id=\"Exec\" value=\"Exec\"/></stringTable></resources></localization>\n"
"</instrumentationManifest>\n";

static BYTE payload[256];
static ULONG plen;
static void put(const void *p, ULONG n) { memcpy(payload + plen, p, n); plen += n; }

static ULONG prop_size(EVENT_RECORD *rec, const WCHAR *name, ULONG idx, ULONG *size)
{
    PDD d = { (ULONG_PTR)name, idx, 0 };
    return pSize(rec, 0, NULL, 1, &d, size);
}

static ULONG prop_get(EVENT_RECORD *rec, const WCHAR *name, ULONG idx, void *buf, ULONG len)
{
    PDD d = { (ULONG_PTR)name, idx, 0 };
    return pGet(rec, 0, NULL, 1, &d, len, buf);
}

/* one value through TdhFormatProperty */
static const WCHAR *fmt(void *map, USHORT in, USHORT out, USHORT length, const void *data, USHORT dlen, ULONG ptr, USHORT *consumed, ULONG *ret)
{
    static WCHAR buf[256];
    ULONG size = sizeof(buf);
    buf[0] = 0;
    *ret = pFormat(NULL, map, ptr, in, out, length, dlen, (BYTE *)data, &size, buf, consumed);
    return buf;
}

int main(void)
{
    HMODULE tdh = LoadLibraryA("tdh.dll");
    WCHAR path[MAX_PATH] = L"C:\\sgprop.man";
    HANDLE file;
    DWORD got;
    EVENT_RECORD rec = { 0 };
    ULONG size, ret, i;
    BYTE buf[64];
    USHORT consumed;
    const WCHAR *text;
    ULONG idx;
    BYTE map_level[512], map_flags[512];
    ULONG map_level_size = sizeof(map_level), map_flags_size = sizeof(map_flags);
    GUID g = some_guid;
    WORD w;
    DWORD d;

    pLoad = (void *)GetProcAddress(tdh, "TdhLoadManifest");
    pSize = (void *)GetProcAddress(tdh, "TdhGetPropertySize");
    pGet = (void *)GetProcAddress(tdh, "TdhGetProperty");
    pFormat = (void *)GetProcAddress(tdh, "TdhFormatProperty");
    pMap = (void *)GetProcAddress(tdh, "TdhGetEventMapInformation");
    pEnum = (void *)GetProcAddress(tdh, "TdhEnumerateManifestProviderEvents");
    CHECK(pLoad && pSize && pGet && pFormat && pMap && pEnum, "exports");

    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, manifest, sizeof(manifest) - 1, &got, NULL);
    CloseHandle(file);
    CHECK(pLoad(path) == ERROR_SUCCESS, "load");

    /* the event: Name "Hi", Code 0x1F, Blob(3), Items(2), Tail "abcd", State 1, Perm 3, Id, Ip 127.0.0.1, Flag, Neg -2 */
    put(L"Hi", 6);
    d = 0x1f; put(&d, 4);
    w = 3; put(&w, 2);
    put("\xaa\xbb\xcc", 3);
    payload[plen++] = 2;
    d = (DWORD)-5; put(&d, 4);
    d = 70000; put(&d, 4);
    put("abcd", 4);
    payload[plen++] = 1;
    d = 3; put(&d, 4);
    put(&g, 16);
    d = 0x0100007f; put(&d, 4);
    d = 1; put(&d, 4);
    w = (WORD)-2; put(&w, 2);
    rec.EventHeader.ProviderId = prov_guid;
    rec.EventHeader.EventDescriptor.Id = 10;
    rec.EventHeader.Flags = EVENT_HEADER_FLAG_64_BIT_HEADER;
    rec.UserData = payload;
    rec.UserDataLength = plen;

    /* sizes */
    ret = prop_size(&rec, L"Name", ~0u, &size); CHECK(ret == 0 && size == 6, "Name: %lu %lu", ret, size);
    ret = prop_size(&rec, L"Code", ~0u, &size); CHECK(ret == 0 && size == 4, "Code: %lu %lu", ret, size);
    ret = prop_size(&rec, L"Blob", ~0u, &size); CHECK(ret == 0 && size == 3, "Blob (length from Len): %lu %lu", ret, size);
    ret = prop_size(&rec, L"Items", 1, &size); CHECK(ret == 0 && size == 4, "Items[1]: %lu %lu", ret, size);
    ret = prop_size(&rec, L"Items", ~0u, &size); CHECK(ret == 0 && size == 8, "all Items (count from N): %lu %lu", ret, size);
    ret = prop_size(&rec, L"Tail", ~0u, &size); CHECK(ret == 0 && size == 4, "Tail (fixed length): %lu %lu", ret, size);
    ret = prop_size(&rec, L"Neg", ~0u, &size); CHECK(ret == 0 && size == 2, "Neg after the fixed strings: %lu %lu", ret, size);
    ret = prop_size(&rec, L"Nope", ~0u, &size); CHECK(ret == ERROR_NOT_FOUND, "unknown name: %lu", ret);
    ret = prop_size(&rec, L"Items", 5, &size); CHECK(ret != 0, "Items[5] is past the array: %lu", ret);
    ret = pSize(&rec, 0, NULL, 0, NULL, &size); CHECK(ret == ERROR_INVALID_PARAMETER, "no descriptors: %lu", ret);
    {
        PDD two[2] = { { (ULONG_PTR)L"Name", ~0u, 0 }, { (ULONG_PTR)L"Code", ~0u, 0 } };
        ret = pSize(&rec, 0, NULL, 2, two, &size); CHECK(ret == 0 && size == 10, "two properties: %lu %lu", ret, size);
    }
    {
        EVENT_RECORD unknown = rec;
        unknown.EventHeader.ProviderId = other_guid;
        ret = prop_size(&unknown, L"Name", ~0u, &size); CHECK(ret == ERROR_NOT_FOUND, "event of no loaded manifest: %lu", ret);
    }
    {
        EVENT_RECORD cut = rec;
        cut.UserDataLength = 4;     /* the string is not terminated in the data */
        ret = prop_size(&cut, L"Code", ~0u, &size); CHECK(ret == ERROR_EVT_INVALID_EVENT_DATA, "short data: %lu", ret);
    }

    /* data */
    memset(buf, 0, sizeof(buf));
    ret = prop_get(&rec, L"Code", ~0u, buf, 4); CHECK(ret == 0 && *(DWORD *)buf == 0x1f, "Code value: %lu %lx", ret, *(DWORD *)buf);
    ret = prop_get(&rec, L"Code", ~0u, buf, 3); CHECK(ret == ERROR_INSUFFICIENT_BUFFER, "small buffer: %lu", ret);
    ret = prop_get(&rec, L"Items", 1, buf, 4); CHECK(ret == 0 && *(DWORD *)buf == 70000, "Items[1]: %lu", ret);
    ret = prop_get(&rec, L"Items", 0, buf, 4); CHECK(ret == 0 && *(LONG *)buf == -5, "Items[0]: %lu", ret);
    ret = prop_get(&rec, L"Blob", ~0u, buf, 3); CHECK(ret == 0 && !memcmp(buf, "\xaa\xbb\xcc", 3), "Blob bytes: %lu", ret);
    ret = prop_get(&rec, L"Id", ~0u, buf, 16); CHECK(ret == 0 && !memcmp(buf, &g, 16), "GUID: %lu", ret);
    ret = prop_get(&rec, L"Tail", ~0u, buf, 4); CHECK(ret == 0 && !memcmp(buf, "abcd", 4), "Tail: %lu", ret);
    for (idx = 0; idx < 2; idx++) { /* the two Items in one call as the whole array */ }
    ret = prop_get(&rec, L"Items", ~0u, buf, 8); CHECK(ret == 0 && ((LONG *)buf)[0] == -5 && ((LONG *)buf)[1] == 70000, "whole array: %lu", ret);

    /* maps */
    ret = pMap(&rec, L"Level", map_level, &map_level_size);
    CHECK(ret == 0 && ((MAP_INFO *)map_level)->EntryCount == 2 && (((MAP_INFO *)map_level)->Flag & 1), "Level map: %lu entries %lu", ret, ((MAP_INFO *)map_level)->EntryCount);
    if (!ret)
    {
        MAP_INFO *m = (MAP_INFO *)map_level;
        CHECK(!wcscmp((WCHAR *)(map_level + m->Entries[1].OutputOffset), L"On") && m->Entries[1].Value == 1, "entry 1");
        CHECK(!wcscmp((WCHAR *)(map_level + m->NameOffset), L"Level"), "map name");
    }
    ret = pMap(&rec, L"Flags", map_flags, &map_flags_size);
    CHECK(ret == 0 && ((MAP_INFO *)map_flags)->EntryCount == 3 && (((MAP_INFO *)map_flags)->Flag & 2), "Flags map: %lu", ret);
    size = 4;
    ret = pMap(&rec, L"Flags", buf, &size); CHECK(ret == ERROR_INSUFFICIENT_BUFFER && size > 4, "small map buffer: %lu %lu", ret, size);
    size = 100;
    ret = pMap(&rec, L"Nope", buf, &size); CHECK(ret == ERROR_NOT_FOUND, "unknown map: %lu", ret);

    /* formatting */
    d = 0x1f;
    text = fmt(NULL, 8, 18, 0, &d, 4, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"0x1F") && consumed == 4, "hex: %lu %ls", ret, text);
    text = fmt(NULL, 8, 8, 0, &d, 4, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"31"), "uint32: %ls", text);
    w = (WORD)-2;
    text = fmt(NULL, 5, 5, 0, &w, 2, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"-2") && consumed == 2, "int16: %ls", text);
    d = 1;
    text = fmt(NULL, 13, 13, 0, &d, 4, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"true"), "boolean: %ls", text);
    d = 0x0100007f;
    text = fmt(NULL, 8, 23, 0, &d, 4, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"127.0.0.1"), "ipv4: %ls", text);
    text = fmt(NULL, 15, 14, 0, &g, 16, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"{01020304-0506-0708-090A-0B0C0D0E0F10}") && consumed == 16, "guid: %ls", text);
    text = fmt(NULL, 1, 1, 0, L"Hi", 6, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"Hi") && consumed == 6, "unicode string: %ls %u", text, consumed);
    text = fmt(NULL, 2, 1, 0, "Ok", 3, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"Ok") && consumed == 3, "ansi string: %ls", text);
    text = fmt(NULL, 14, 15, 3, "\xaa\xbb\xcc", 3, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"0xAABBCC") && consumed == 3, "binary: %ls", text);
    {
        ULONGLONG p = 0x1234;
        text = fmt(NULL, 16, 0, 0, &p, 8, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"0x1234") && consumed == 8, "pointer: %ls", text);
        text = fmt(NULL, 16, 0, 0, &p, 4, 4, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"0x1234") && consumed == 4, "32-bit pointer: %ls", text);
    }
    d = 1;
    text = fmt(map_level, 4, 4, 0, &d, 1, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"On"), "value map: %ls", text);
    d = 7;
    text = fmt(map_level, 4, 4, 0, &d, 1, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"7"), "value not in the map: %ls", text);
    d = 3;
    text = fmt(map_flags, 8, 8, 0, &d, 4, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"Read | Write"), "bit map: %ls", text);
    d = 4;
    text = fmt(map_flags, 8, 8, 0, &d, 4, 8, &consumed, &ret); CHECK(ret == 0 && !wcscmp(text, L"Exec"), "bit map single: %ls", text);
    {
        FILETIME ft;
        SYSTEMTIME st = { 2020, 2, 0, 29, 13, 45, 7, 123 };
        SystemTimeToFileTime(&st, &ft);
        text = fmt(NULL, 17, 2, 0, &ft, 8, 8, &consumed, &ret);
        CHECK(ret == 0 && !wcscmp(text, L"2020-02-29T13:45:07.123Z") && consumed == 8, "filetime: %ls", text);
    }
    /* buffer too small: the size needed, nothing consumed wrongly */
    {
        WCHAR small[2];
        ULONG sz = sizeof(small);
        d = 0x1f;
        ret = pFormat(NULL, NULL, 8, 8, 18, 0, 4, (BYTE *)&d, &sz, small, &consumed);
        CHECK(ret == ERROR_INSUFFICIENT_BUFFER && sz == 5 * sizeof(WCHAR), "small format buffer: %lu %lu", ret, sz);
        ret = pFormat(NULL, NULL, 3, 8, 18, 0, 4, (BYTE *)&d, &sz, small, &consumed);
        CHECK(ret == ERROR_INVALID_PARAMETER, "bad pointer size: %lu", ret);
        sz = 100;
        ret = pFormat(NULL, NULL, 8, 8, 8, 0, 2, (BYTE *)&d, &sz, (WCHAR *)buf, &consumed);
        CHECK(ret != 0, "data shorter than the type: %lu", ret);
    }

    /* the events of a provider */
    {
        PROV_EVENTS *pe;
        size = 0;
        ret = pEnum(&prov_guid, NULL, &size);
        CHECK(ret == ERROR_INSUFFICIENT_BUFFER && size >= 8 + 2 * sizeof(EVENT_DESCRIPTOR), "enum size: %lu %lu", ret, size);
        pe = calloc(1, size);
        ret = pEnum(&prov_guid, pe, &size);
        CHECK(ret == 0 && pe->NumberOfEvents == 2, "enum: %lu count %lu", ret, pe->NumberOfEvents);
        if (!ret && pe->NumberOfEvents == 2)
        {
            CHECK(pe->Descs[0].Id == 10 && pe->Descs[0].Version == 0 && pe->Descs[0].Level == 3, "event 10: id %u level %u", pe->Descs[0].Id, pe->Descs[0].Level);
            CHECK(pe->Descs[1].Id == 12 && pe->Descs[1].Version == 2 && pe->Descs[1].Level == 17 && pe->Descs[1].Opcode == 1,
                  "event 12: id %u ver %u level %u opcode %u", pe->Descs[1].Id, pe->Descs[1].Version, pe->Descs[1].Level, pe->Descs[1].Opcode);
        }
        free(pe);
        size = 100;
        ret = pEnum(&other_guid, buf, &size); CHECK(ret == ERROR_NOT_FOUND, "unknown provider: %lu", ret);
        ret = pEnum(NULL, buf, &size); CHECK(ret == ERROR_INVALID_PARAMETER, "NULL guid: %lu", ret);
    }
    DeleteFileW(path);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
