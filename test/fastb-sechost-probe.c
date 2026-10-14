/* sechost gate: QueryServiceConfig2 level 9 (preferred node), TraceSetInformation / TraceQueryInformation */
#include <windows.h>
#include <wmistr.h>
#include <initguid.h>
#include <evntrace.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { USHORT usPreferredNode; BOOLEAN fDelete; } PREF;

static EVENT_TRACE_PROPERTIES *make_properties(ULONG mode, const GUID *guid)
{
    ULONG size = sizeof(EVENT_TRACE_PROPERTIES) + 2 * MAX_PATH * sizeof(WCHAR);
    EVENT_TRACE_PROPERTIES *props = calloc(1, size);
    props->Wnode.BufferSize = size;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    if (guid) props->Wnode.Guid = *guid;
    props->LogFileMode = mode;
    props->LoggerNameOffset = sizeof(*props);
    return props;
}

static void test_service(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS), svc;
    PREF info = { 0, FALSE }, out;
    DWORD need = 0, err;
    BOOL ret;

    CHECK(scm != NULL, "no SCM: %lu", GetLastError());
    if (!scm) return;
    DeleteService(OpenServiceW(scm, L"SgPrefNode", DELETE));
    svc = CreateServiceW(scm, L"SgPrefNode", L"SgPrefNode", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, L"C:\\windows\\system32\\notepad.exe", NULL, NULL, NULL, NULL, NULL);
    CHECK(svc != NULL, "CreateService: %lu", GetLastError());
    if (!svc) { CloseServiceHandle(scm); return; }

    SetLastError(0);
    ret = QueryServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, NULL, 0, &need);
    err = GetLastError();
    CHECK(!ret && err == ERROR_INSUFFICIENT_BUFFER && need == sizeof(PREF), "size query: %d err %lu need %lu", ret, err, need);
    memset(&out, 0x55, sizeof(out));
    ret = QueryServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, (BYTE *)&out, sizeof(out), &need);
    CHECK(ret && out.fDelete && out.usPreferredNode == 0, "unset: %d node %u delete %u err %lu", ret, out.usPreferredNode, out.fDelete, GetLastError());

    info.usPreferredNode = 0; info.fDelete = FALSE;
    ret = ChangeServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, &info);
    CHECK(ret, "set node 0: %lu", GetLastError());
    memset(&out, 0x55, sizeof(out));
    ret = QueryServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, (BYTE *)&out, sizeof(out), &need);
    CHECK(ret && !out.fDelete && out.usPreferredNode == 0, "after set: %d node %u delete %u", ret, out.usPreferredNode, out.fDelete);
    memset(&out, 0x55, sizeof(out));
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_PREFERRED_NODE, (BYTE *)&out, sizeof(out), &need);
    CHECK(ret && !out.fDelete && out.usPreferredNode == 0, "ANSI query: %d err %lu", ret, GetLastError());

    info.usPreferredNode = 200; info.fDelete = FALSE;
    SetLastError(0);
    ret = ChangeServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, &info);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "invalid node: %d err %lu", ret, GetLastError());

    info.usPreferredNode = 0; info.fDelete = TRUE;
    ret = ChangeServiceConfig2A(svc, SERVICE_CONFIG_PREFERRED_NODE, &info);
    CHECK(ret, "ANSI delete: %lu", GetLastError());
    memset(&out, 0x55, sizeof(out));
    ret = QueryServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, (BYTE *)&out, sizeof(out), &need);
    CHECK(ret && out.fDelete, "after delete: %d delete %u", ret, out.fDelete);

    DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

static void test_trace(void)
{
    EVENT_TRACE_PROPERTIES *p = make_properties(EVENT_TRACE_REAL_TIME_MODE, NULL), *k;
    TRACEHANDLE h = 0, kh = 0;
    ULONG ret, len;
    BOOLEAN b;
    CLASSIC_EVENT_ID ids[2] = { 0 };
    ULONG vals[2] = { 1, 2 };
    struct { ULONG source, interval; } prof = { 0, 10000 };

    ret = StartTraceW(&h, L"SGSetInfo", p);
    CHECK(ret == ERROR_SUCCESS, "StartTrace %lu", ret);

    CHECK(TraceSetInformation(h, MaxTraceSetInfoClass, &b, 1) == ERROR_INVALID_PARAMETER, "class out of range");
    CHECK(TraceSetInformation(h, TraceGuidQueryList, &b, 1) == ERROR_NOT_SUPPORTED, "query-only class");
    CHECK(TraceSetInformation(h, TraceStackTracingInfo, ids, 5) == ERROR_BAD_LENGTH, "stack tracing odd length");
    CHECK(TraceSetInformation(h, TraceStackTracingInfo, ids, sizeof(ids)) == ERROR_INVALID_PARAMETER, "stack tracing on a user session");
    CHECK(TraceSetInformation(h, TraceSampledProfileIntervalInfo, &prof, 4) == ERROR_BAD_LENGTH, "profile interval short");
    CHECK(TraceSetInformation(h, TraceSystemTraceEnableFlagsInfo, vals, 3) == ERROR_BAD_LENGTH, "enable flags odd length");
    CHECK(TraceSetInformation(h, TraceProviderBinaryTracking, NULL, 0) == ERROR_BAD_LENGTH, "binary tracking length 0");
    CHECK(TraceSetInformation(h, TraceProviderBinaryTracking, NULL, 1) == ERROR_INVALID_PARAMETER, "binary tracking NULL buffer");
    CHECK(TraceSetInformation(0xdead, TraceProviderBinaryTracking, &b, 1) == ERROR_WMI_INSTANCE_NOT_FOUND, "unknown session");

    b = 1;
    CHECK(TraceSetInformation(h, TraceProviderBinaryTracking, &b, 1) == ERROR_SUCCESS, "binary tracking on");
    b = 0; len = 0;
    ret = TraceQueryInformation(h, TraceProviderBinaryTracking, &b, 1, &len);
    CHECK(ret == ERROR_SUCCESS && b == 1 && len == 1, "binary tracking query: %lu b %u len %lu", ret, b, len);
    b = 0;
    CHECK(TraceSetInformation(h, TraceProviderBinaryTracking, &b, 1) == ERROR_SUCCESS, "binary tracking off");
    b = 7;
    TraceQueryInformation(h, TraceProviderBinaryTracking, &b, 1, &len);
    CHECK(b == 0, "binary tracking after off: %u", b);
    b = 1;
    CHECK(TraceSetInformation(h, TraceCompressionInfo, &b, 1) == ERROR_SUCCESS, "compression on");
    b = 0;
    TraceQueryInformation(h, TraceCompressionInfo, &b, 1, &len);
    CHECK(b == 1, "compression query %u", b);
    b = 7;
    TraceQueryInformation(h, TraceProviderBinaryTracking, &b, 1, &len);
    CHECK(b == 0, "compression leaked into binary tracking: %u", b);
    CHECK(TraceQueryInformation(h, TraceProviderBinaryTracking, &b, 0, &len) == ERROR_BAD_LENGTH, "query length 0");
    CHECK(TraceQueryInformation(0xdead, TraceCompressionInfo, &b, 1, &len) == ERROR_WMI_INSTANCE_NOT_FOUND, "query unknown session");
    ret = 0;
    CHECK(TraceQueryInformation(0, TraceMaxLoggersQuery, &ret, 4, &len) == ERROR_SUCCESS && ret == 64, "max loggers %lu", ret);
    CHECK(TraceQueryInformation(0, TraceGuidQueryList, &ret, 4, &len) == ERROR_NOT_SUPPORTED, "unsupported query class");

    /* the kernel logger takes the kernel capture classes */
    k = make_properties(EVENT_TRACE_REAL_TIME_MODE, &SystemTraceControlGuid);
    ret = StartTraceW(&kh, KERNEL_LOGGER_NAMEW, k);
    CHECK(ret == ERROR_SUCCESS, "kernel logger start %lu", ret);
    if (!ret)
    {
        CHECK(TraceSetInformation(kh, TraceStackTracingInfo, ids, sizeof(ids)) == ERROR_SUCCESS, "stack tracing on the kernel logger");
        CHECK(TraceSetInformation(kh, TraceStackTracingInfo, NULL, 0) == ERROR_SUCCESS, "stack tracing disabled (length 0)");
        CHECK(TraceSetInformation(kh, TraceSampledProfileIntervalInfo, &prof, sizeof(prof)) == ERROR_SUCCESS, "profile interval on the kernel logger");
        ControlTraceW(kh, NULL, k, EVENT_TRACE_CONTROL_STOP);
    }
    ControlTraceW(h, NULL, p, EVENT_TRACE_CONTROL_STOP);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    test_service();
    test_trace();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
