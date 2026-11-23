/* wine-sg 9710: SetTraceCallback / RemoveTraceCallback (advapi32, sechost; CrystalDiskMark imports them
 * statically and did not load). ProcessTrace calls the callback set for an event class (its provider GUID)
 * for each event of that class, besides the log file's own EventCallback. */
#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static const GUID provider = { 0x5e9a1d2b, 0x3c4d, 0x4e5f, { 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe6, 0xf7 } };
static const GUID other = { 0x5e9a1d2b, 0x3c4d, 0x4e5f, { 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe6, 0xf8 } };
static LONG all_events, class_events, other_events, bad_guid;

static void WINAPI on_event(EVENT_TRACE *e) { if (IsEqualGUID(&e->Header.Guid, &provider)) all_events++; }
static void WINAPI on_class(EVENT_TRACE *e) { class_events++; if (!IsEqualGUID(&e->Header.Guid, &provider)) bad_guid++; }
static void WINAPI on_other(EVENT_TRACE *e) { other_events++; }

static ULONG process(const WCHAR *path)
{
    EVENT_TRACE_LOGFILEW logfile;
    TRACEHANDLE consumer;
    ULONG ret;
    memset(&logfile, 0, sizeof(logfile));
    logfile.LogFileName = (WCHAR *)path;
    logfile.EventCallback = on_event;
    consumer = OpenTraceW(&logfile);
    if (consumer == INVALID_PROCESSTRACE_HANDLE) return GetLastError();
    ret = ProcessTrace(&consumer, 1, NULL, NULL);
    CloseTrace(consumer);
    return ret;
}

int main(void)
{
    ULONG size = sizeof(EVENT_TRACE_PROPERTIES) + 2 * MAX_PATH * sizeof(WCHAR), ret;
    EVENT_TRACE_PROPERTIES *props = calloc(1, size);
    EVENT_DESCRIPTOR desc = { 7, 0, 0, 4, 0, 0, 0x10 };
    WCHAR path[MAX_PATH];
    TRACEHANDLE session;
    REGHANDLE reg;
    int i;

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-tracecb.etl");
    DeleteFileW(path);
    props->Wnode.BufferSize = size;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    props->LoggerNameOffset = sizeof(*props);
    props->LogFileNameOffset = sizeof(*props) + MAX_PATH * sizeof(WCHAR);
    lstrcpyW((WCHAR *)((BYTE *)props + props->LogFileNameOffset), path);
    ret = StartTraceW(&session, L"SGTraceCallback", props);
    CHECK(!ret, "StartTraceW (%lu)", ret);
    EventRegister(&provider, NULL, NULL, &reg);
    EnableTraceEx2(session, &provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0x10, 0, 0, NULL);
    for (i = 0; i < 40 && !EventEnabled(reg, &desc); i++) Sleep(50);
    for (i = 0; i < 3; i++) EventWrite(reg, &desc, 0, NULL);
    EventUnregister(reg);
    ControlTraceW(session, NULL, props, EVENT_TRACE_CONTROL_STOP);

    CHECK(SetTraceCallback(NULL, on_class) == ERROR_INVALID_PARAMETER, "SetTraceCallback(NULL guid): ERROR_INVALID_PARAMETER");
    CHECK(!SetTraceCallback(&provider, on_other), "SetTraceCallback");
    CHECK(!SetTraceCallback(&provider, on_class), "SetTraceCallback again replaces the callback");
    ret = process(path);
    CHECK(!ret && all_events == 3, "ProcessTrace: the log file's callback has the three events (%lu, %ld)", ret, all_events);
    CHECK(class_events == 3 && !bad_guid && !other_events, "the class callback has them too (%ld, %ld)", class_events, other_events);
    CHECK(!RemoveTraceCallback(&provider), "RemoveTraceCallback");
    CHECK(RemoveTraceCallback(&provider) == ERROR_WMI_GUID_NOT_FOUND, "again: ERROR_WMI_GUID_NOT_FOUND");
    CHECK(!SetTraceCallback(&other, on_other), "a callback for another class");
    class_events = all_events = 0;
    process(path);
    CHECK(all_events == 3 && !class_events && !other_events, "removed: not called; another class's: not called (%ld, %ld)", class_events, other_events);
    DeleteFileW(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
