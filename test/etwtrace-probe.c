/* Event tracing sessions, providers and consumers (patches/sg/1682), run by
 * test/etwtrace-gate.sh: a session with a log file and a real-time ring;
 * a provider told by its callback when it is enabled and disabled, its
 * EventEnabled answers, its events (and a child process's) read in real
 * time; ControlTrace and QueryAllTraces; CloseTrace during ProcessTrace;
 * the log file read back; a classic provider enabled with EnableTrace and
 * its TraceEvent. These were stubs. */
#include <windows.h>
#include <wmistr.h>
#include <initguid.h>
#include <evntrace.h>
#include <evntprov.h>
#include <evntcons.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

DEFINE_GUID(provider_guid, 0x5f1a0c33, 0x2b44, 0x4c55, 0x9d, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc);
DEFINE_GUID(classic_guid, 0x5f1a0c34, 0x2b44, 0x4c55, 0x9d, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc);
DEFINE_GUID(activity_guid, 0x11112222, 0x3333, 0x4444, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88);

static volatile LONG callbacks, last_enabled = -1, last_level;
static volatile ULONGLONG last_any;
static void WINAPI enable_callback(LPCGUID source, ULONG enabled, UCHAR level, ULONGLONG any, ULONGLONG all,
                                   PEVENT_FILTER_DESCRIPTOR filter, PVOID context)
{
    last_enabled = enabled;
    last_level = level;
    last_any = any;
    InterlockedIncrement(&callbacks);
}

static BOOL wait_for(volatile LONG *value, LONG want)
{
    int i;
    for (i = 0; i < 100 && *value != want; i++) Sleep(50);
    return *value == want;
}

/* what the real-time consumer got */
static volatile LONG rt_events, rt_strings, rt_child, rt_hello, rt_activity;
static void WINAPI rt_record(EVENT_RECORD *record)
{
    if (!IsEqualGUID(&record->EventHeader.ProviderId, &provider_guid)) return;
    if (record->EventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) InterlockedIncrement(&rt_strings);
    else if (record->EventHeader.ProcessId != GetCurrentProcessId()) InterlockedIncrement(&rt_child);
    else
    {
        if (record->UserDataLength == 6 && !memcmp(record->UserData, "hello", 6) &&
            record->EventHeader.EventDescriptor.Id == 7 && record->EventHeader.EventDescriptor.Level == 4)
            InterlockedIncrement(&rt_hello);
        if (IsEqualGUID(&record->EventHeader.ActivityId, &activity_guid)) InterlockedIncrement(&rt_activity);
    }
    InterlockedIncrement(&rt_events);
}

static TRACEHANDLE consumer;
static volatile ULONG process_result = 0xdead;
static DWORD WINAPI consumer_thread(void *arg)
{
    process_result = ProcessTrace(&consumer, 1, NULL, NULL);
    return 0;
}

/* the log file read back */
static LONG file_header, file_events, file_classic;
static void WINAPI file_event(EVENT_TRACE *event)
{
    if (IsEqualGUID(&event->Header.Guid, &EventTraceGuid)) file_header++;
    else if (IsEqualGUID(&event->Header.Guid, &provider_guid)) file_events++;
    else if (IsEqualGUID(&event->Header.Guid, &classic_guid) && event->MofLength == 4 &&
             *(DWORD *)event->MofData == 0x1234 && event->Header.Class.Type == 9)
        file_classic++;
}

static TRACEHANDLE classic_logger;
static volatile LONG classic_enabled = -1;
static ULONG WINAPI classic_request(WMIDPREQUESTCODE code, void *context, ULONG *size, void *buffer)
{
    if (code == WMI_ENABLE_EVENTS)
    {
        classic_logger = GetTraceLoggerHandle(buffer);
        classic_enabled = 1;
    }
    else if (code == WMI_DISABLE_EVENTS) classic_enabled = 0;
    return ERROR_SUCCESS;
}

static EVENT_TRACE_PROPERTIES *make_properties(const WCHAR *name, const WCHAR *file, ULONG mode)
{
    ULONG size = sizeof(EVENT_TRACE_PROPERTIES) + 2 * MAX_PATH * sizeof(WCHAR);
    EVENT_TRACE_PROPERTIES *props = calloc(1, size);
    props->Wnode.BufferSize = size;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->LogFileMode = mode;
    props->LoggerNameOffset = sizeof(*props);
    props->LogFileNameOffset = file ? sizeof(*props) + MAX_PATH * sizeof(WCHAR) : 0;
    if (file) lstrcpyW((WCHAR *)((BYTE *)props + props->LogFileNameOffset), file);
    return props;
}

static int child(void)
{
    EVENT_DESCRIPTOR desc = { 8, 0, 0, 4, 0, 0, 0x10 };
    REGHANDLE reg;
    int i;

    EventRegister(&provider_guid, NULL, NULL, &reg);
    for (i = 0; i < 40 && !EventEnabled(reg, &desc); i++) Sleep(50);
    EventWrite(reg, &desc, 0, NULL);
    EventUnregister(reg);
    return 0;
}

int main(int argc, char **argv)
{
    EVENT_TRACE_PROPERTIES *props, *query, *list[8];
    EVENT_TRACE_LOGFILEW logfile;
    EVENT_DESCRIPTOR desc = { 7, 0, 0, 4, 0, 0, 0x10 }, verbose = { 7, 0, 0, 5, 0, 0, 0x10 }, other = { 7, 0, 0, 4, 0, 0, 0x20 };
    EVENT_DATA_DESCRIPTOR data;
    WCHAR path[MAX_PATH], path2[MAX_PATH], dir[MAX_PATH];
    TRACEHANDLE session, session2, reg_handle;
    TRACE_GUID_REGISTRATION classes = { &classic_guid, NULL };
    REGHANDLE reg;
    HANDLE thread;
    GUID old;
    ULONG ret, count, i;
    char cmd[MAX_PATH + 16];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (argc > 1 && !strcmp(argv[1], "child")) return child();
    if (argc > 1 && !strcmp(argv[1], "start"))
    {
        /* a tool that starts a session and exits */
        EVENT_TRACE_PROPERTIES *p = make_properties(L"SGProbeLasting", NULL, EVENT_TRACE_REAL_TIME_MODE);
        TRACEHANDLE h;
        return StartTraceW(&h, L"SGProbeLasting", p);
    }

    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%sSGProbe-%lu.etl", dir, GetCurrentProcessId());
    swprintf(path2, MAX_PATH, L"%sSGProbeClassic-%lu.etl", dir, GetCurrentProcessId());

    props = make_properties(L"SGProbe", path, EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_FILE_MODE_SEQUENTIAL);
    ret = StartTraceW(&session, L"SGProbe", props);
    check(ret == ERROR_SUCCESS && session, "StartTraceW: a session with a file and real time");
    check(StartTraceW(&session2, L"SGProbe", props) == ERROR_ALREADY_EXISTS, "the name again: ERROR_ALREADY_EXISTS");

    /* a provider */
    check(!EventRegister(&provider_guid, enable_callback, NULL, &reg), "EventRegister");
    check(!EventEnabled(reg, &desc), "not enabled yet");
    check(!EnableTraceEx2(session, &provider_guid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0x10, 0, 0, NULL),
          "EnableTraceEx2");
    check(wait_for(&last_enabled, 1) && last_level == 4 && last_any == 0x10, "the provider's callback: enabled, level 4, keyword 0x10");
    check(EventEnabled(reg, &desc), "EventEnabled: level 4, keyword 0x10");
    check(!EventEnabled(reg, &verbose), "not for level 5");
    check(!EventEnabled(reg, &other), "nor keyword 0x20");
    check(EventProviderEnabled(reg, 3, 0x10), "EventProviderEnabled");

    /* a real-time consumer */
    memset(&logfile, 0, sizeof(logfile));
    logfile.LoggerName = (WCHAR *)L"SGProbe";
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = rt_record;
    consumer = OpenTraceW(&logfile);
    check(consumer != INVALID_PROCESSTRACE_HANDLE, "OpenTraceW: the real-time session");
    thread = CreateThread(NULL, 0, consumer_thread, NULL, 0, NULL);
    Sleep(200);

    old = activity_guid;
    EventActivityIdControl(EVENT_ACTIVITY_CTRL_SET_ID, &old);
    data.Ptr = (ULONG_PTR)"hello";
    data.Size = 6;
    data.Reserved = 0;
    check(!EventWrite(reg, &desc, 1, &data), "EventWrite");
    check(!EventWriteString(reg, 4, 0x10, L"a string"), "EventWriteString");
    EventWrite(reg, &verbose, 1, &data);  /* not wanted */
    check(wait_for(&rt_events, 2) && rt_hello == 1 && rt_strings == 1 && rt_activity == 1,
          "the consumer gets them, with their data and activity");
    Sleep(300);
    check(rt_events == 2, "and not the level 5 one");

    /* another process's events */
    GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
    cmd[0] = '"';
    strcat(cmd, "\" child");
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    check(wait_for(&rt_child, 1), "a child process's event reaches the consumer");

    /* control */
    query = make_properties(NULL, NULL, 0);
    check(!ControlTraceW(0, L"SGProbe", query, EVENT_TRACE_CONTROL_QUERY) &&
          (query->LogFileMode & EVENT_TRACE_REAL_TIME_MODE) && query->Wnode.HistoricalContext == session &&
          !lstrcmpW((WCHAR *)((BYTE *)query + query->LoggerNameOffset), L"SGProbe"), "ControlTraceW: query");
    for (i = 0; i < 8; i++) list[i] = make_properties(NULL, NULL, 0);
    count = 0;
    ret = QueryAllTracesW(list, 8, &count);
    check(!ret && count >= 1, "QueryAllTracesW");
    check(ControlTraceW(0, L"No such session", query, EVENT_TRACE_CONTROL_QUERY) == ERROR_WMI_INSTANCE_NOT_FOUND,
          "an unknown session: ERROR_WMI_INSTANCE_NOT_FOUND");

    /* CloseTrace while ProcessTrace runs */
    check(CloseTrace(consumer) == ERROR_CTX_CLOSE_PENDING, "CloseTrace during ProcessTrace: ERROR_CTX_CLOSE_PENDING");
    check(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0 && process_result == ERROR_CANCELLED,
          "ProcessTrace returns ERROR_CANCELLED");
    CloseHandle(thread);

    /* disabling */
    callbacks = 0;
    EnableTraceEx2(session, &provider_guid, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, NULL);
    check(wait_for(&last_enabled, 0), "disabled: the callback says so");
    check(!EventEnabled(reg, &desc), "EventEnabled: no longer");
    check(!ControlTraceW(session, NULL, query, EVENT_TRACE_CONTROL_STOP), "ControlTraceW: stop");
    check(ControlTraceW(session, NULL, query, EVENT_TRACE_CONTROL_QUERY) == ERROR_WMI_INSTANCE_NOT_FOUND, "stopped");
    EventUnregister(reg);

    /* a session outlives the process that started it */
    GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
    cmd[0] = '"';
    strcat(cmd, "\" start");
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        DWORD code = 99;
        WaitForSingleObject(pi.hProcess, 30000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        check(code == 0, "another process starts a session and exits");
    }
    check(!ControlTraceW(0, L"SGProbeLasting", query, EVENT_TRACE_CONTROL_QUERY), "the session is still there");
    check(!ControlTraceW(0, L"SGProbeLasting", query, EVENT_TRACE_CONTROL_STOP), "and is stopped here");

    /* the log file */
    memset(&logfile, 0, sizeof(logfile));
    logfile.LogFileName = path;
    logfile.EventCallback = file_event;
    consumer = OpenTraceW(&logfile);
    check(consumer != INVALID_PROCESSTRACE_HANDLE, "OpenTraceW: the log file");
    check(ProcessTrace(&consumer, 1, NULL, NULL) == ERROR_SUCCESS, "ProcessTrace of it");
    printf("      file: header %ld, events %ld\n", file_header, file_events);
    check(file_header == 1 && file_events == 3, "its header event and the three events wanted");
    check(CloseTrace(consumer) == ERROR_SUCCESS, "CloseTrace");

    /* a classic provider */
    check(!RegisterTraceGuidsW(classic_request, NULL, &classic_guid, 1, &classes, NULL, NULL, &reg_handle),
          "RegisterTraceGuidsW");
    free(props);
    props = make_properties(L"SGProbeClassic", path2, EVENT_TRACE_FILE_MODE_SEQUENTIAL);
    check(!StartTraceW(&session2, L"SGProbeClassic", props), "a second session");
    check(!EnableTrace(TRUE, 0x3, 5, &classic_guid, session2), "EnableTrace");
    check(wait_for(&classic_enabled, 1) && classic_logger == session2, "WMI_ENABLE_EVENTS, with the logger");
    check(GetTraceEnableLevel(classic_logger) == 5 && GetTraceEnableFlags(classic_logger) == 0x3,
          "GetTraceEnableLevel, GetTraceEnableFlags");
    {
        struct { EVENT_TRACE_HEADER header; DWORD value; } ev;
        memset(&ev, 0, sizeof(ev));
        ev.header.Size = sizeof(EVENT_TRACE_HEADER) + sizeof(DWORD);
        ev.header.Flags = WNODE_FLAG_TRACED_GUID;
        ev.header.Guid = classic_guid;
        ev.header.Class.Type = 9;
        ev.value = 0x1234;
        check(!TraceEvent(classic_logger, &ev.header), "TraceEvent");
    }
    ControlTraceW(session2, NULL, query, EVENT_TRACE_CONTROL_STOP);
    check(wait_for(&classic_enabled, 0), "stopping the session: WMI_DISABLE_EVENTS");
    UnregisterTraceGuids(reg_handle);
    memset(&logfile, 0, sizeof(logfile));
    logfile.LogFileName = path2;
    logfile.EventCallback = file_event;
    consumer = OpenTraceW(&logfile);
    ProcessTrace(&consumer, 1, NULL, NULL);
    CloseTrace(consumer);
    check(file_classic == 1, "the classic event in its file");

    DeleteFileW(path);
    DeleteFileW(path2);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
