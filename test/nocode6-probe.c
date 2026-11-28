/* test/nocode6-gate.sh: EnumerateTraceGuidsEx (patches/sg/8020), which .NET's EventSource calls (through
 * advapi32) to learn the sessions that enable it. sechost's was a spec stub and advapi32 had no export. */
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <evntprov.h>
#include <evntrace.h>

static int failures;
#define CHECK(c, ...) do { if (c) { printf("ok    "); printf(__VA_ARGS__); printf("\n"); } \
                           else { failures++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } } while (0)

DEFINE_GUID(provider_guid, 0x5d6a8a1e, 0x2b8e, 0x4d7c, 0x9a, 0x51, 0x3e, 0x6f, 0x10, 0x22, 0x33, 0x40);
DEFINE_GUID(other_guid, 0x5d6a8a1e, 0x2b8e, 0x4d7c, 0x9a, 0x51, 0x3e, 0x6f, 0x10, 0x22, 0x33, 0x41);

typedef ULONG (WINAPI *enum_t)(TRACE_QUERY_INFO_CLASS, void *, ULONG, void *, ULONG, ULONG *);

static void WINAPI enable_callback(const GUID *g, ULONG code, UCHAR level, ULONGLONG any, ULONGLONG all, EVENT_FILTER_DESCRIPTOR *f, void *ctx) {}

static BOOL list_has(const BYTE *buf, ULONG len, const GUID *g)
{
    ULONG i;
    for (i = 0; i + sizeof(GUID) <= len; i += sizeof(GUID)) if (IsEqualGUID((const GUID *)(buf + i), g)) return TRUE;
    return FALSE;
}

int main(void)
{
    enum_t pEnum = (enum_t)GetProcAddress(LoadLibraryA("advapi32.dll"), "EnumerateTraceGuidsEx");
    enum_t pEnumS = (enum_t)GetProcAddress(LoadLibraryA("sechost.dll"), "EnumerateTraceGuidsEx");
    ULONG size = sizeof(EVENT_TRACE_PROPERTIES) + 2 * MAX_PATH * sizeof(WCHAR), len = 0, ret;
    EVENT_TRACE_PROPERTIES *props = calloc(1, size), *query = calloc(1, size);
    TRACEHANDLE session = 0;
    REGHANDLE reg = 0;
    BYTE buf[4096];
    TRACE_GUID_INFO *info = (TRACE_GUID_INFO *)buf;
    TRACE_PROVIDER_INSTANCE_INFO *inst = (TRACE_PROVIDER_INSTANCE_INFO *)(info + 1);
    TRACE_ENABLE_INFO *en = (TRACE_ENABLE_INFO *)(inst + 1);
    GUID g = provider_guid, o = other_guid;

    CHECK(pEnum && pEnumS, "advapi32 and sechost export EnumerateTraceGuidsEx");
    if (!pEnum || !pEnumS) { printf("RESULT: FAIL\n"); return 1; }

    CHECK(!EventRegister(&provider_guid, enable_callback, NULL, &reg), "EventRegister");
    ret = pEnum(TraceGuidQueryProcess, NULL, 0, buf, sizeof(buf), &len);
    CHECK(ret == 0 && list_has(buf, len, &provider_guid), "TraceGuidQueryProcess lists the provider (%lu, %lu bytes)", ret, len);
    ret = pEnum(TraceGuidQueryProcess, NULL, 0, buf, 0, &len);
    CHECK(ret == ERROR_INSUFFICIENT_BUFFER && len >= sizeof(GUID), "no buffer: ERROR_INSUFFICIENT_BUFFER, %lu bytes", len);

    ret = pEnum(TraceGuidQueryInfo, &g, sizeof(g), buf, sizeof(buf), &len);
    CHECK(ret == 0 && info->InstanceCount == 1 && inst->Pid == GetCurrentProcessId() && inst->EnableCount == 0,
          "registered, no session: one instance, this process, no enables (%lu)", ret);

    props->Wnode.BufferSize = size;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    props->LoggerNameOffset = sizeof(*props);
    ret = StartTraceW(&session, L"SGNocode6", props);
    CHECK(ret == 0 && session, "StartTraceW %lu", ret);
    ret = EnableTraceEx2(session, &provider_guid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0x30, 0x10, 0, NULL);
    CHECK(ret == 0, "EnableTraceEx2 %lu", ret);

    memset(buf, 0xcc, sizeof(buf));
    ret = pEnum(TraceGuidQueryInfo, &g, sizeof(g), buf, sizeof(buf), &len);
    CHECK(ret == 0 && len == sizeof(*info) + sizeof(*inst) + sizeof(*en), "TraceGuidQueryInfo: %lu, %lu bytes", ret, len);
    CHECK(info->InstanceCount == 1 && inst->Pid == GetCurrentProcessId() && inst->EnableCount == 1 && inst->NextOffset == 0,
          "one instance in this process, one enable (%lu %lu %lu)", info->InstanceCount, inst->Pid, inst->EnableCount);
    CHECK(en->IsEnabled && en->Level == 4 && en->MatchAnyKeyword == 0x30 && en->MatchAllKeyword == 0x10 &&
          en->LoggerId == (USHORT)session, "the session's level, keywords and logger id (%u %u %#I64x %#I64x)",
          en->Level, en->LoggerId, en->MatchAnyKeyword, en->MatchAllKeyword);
    ret = pEnum(TraceGuidQueryInfo, &g, sizeof(g), buf, sizeof(*info), &len);
    CHECK(ret == ERROR_INSUFFICIENT_BUFFER && len == sizeof(*info) + sizeof(*inst) + sizeof(*en), "a short buffer: %lu, %lu", ret, len);
    ret = pEnumS(TraceGuidQueryList, NULL, 0, buf, sizeof(buf), &len);
    CHECK(ret == 0 && list_has(buf, len, &provider_guid), "sechost TraceGuidQueryList has it (%lu)", ret);

    ret = pEnum(TraceGuidQueryInfo, &o, sizeof(o), buf, sizeof(buf), &len);
    CHECK(ret == ERROR_WMI_GUID_NOT_FOUND, "a provider nobody registered or enabled: %lu", ret);
    CHECK(pEnum(TraceGuidQueryInfo, NULL, 0, buf, sizeof(buf), &len) == ERROR_INVALID_PARAMETER, "TraceGuidQueryInfo without a GUID");
    CHECK(pEnum(TraceGuidQueryInfo, &g, sizeof(g), buf, sizeof(buf), NULL) == ERROR_INVALID_PARAMETER, "no length");
    CHECK(pEnum(TraceStackTracingInfo, NULL, 0, buf, sizeof(buf), &len) == ERROR_NOT_SUPPORTED, "another class");

    EnableTraceEx2(session, &provider_guid, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, NULL);
    query->Wnode.BufferSize = size;
    query->LoggerNameOffset = sizeof(*query);
    ControlTraceW(session, NULL, query, EVENT_TRACE_CONTROL_STOP);
    EventUnregister(reg);
    ret = pEnum(TraceGuidQueryInfo, &g, sizeof(g), buf, sizeof(buf), &len);
    CHECK(ret == ERROR_WMI_GUID_NOT_FOUND, "unregistered and the session stopped: %lu", ret);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
