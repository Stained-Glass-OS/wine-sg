/* The Windows Event Log API (patches/sg/1665), run by test/evtquery-gate.sh:
 * events reported to the Application log through the classic API
 * (ReportEvent) are read back through wevtapi:
 *  - EvtQuery with XPath filters (provider, level, event data, timediff,
 *    a <QueryList> with Select and Suppress), forwards and backwards; bad
 *    queries and unknown channels fail as on Windows;
 *  - EvtRender as XML, as system, user and XPath values; EvtFormatMessage
 *    (the source's message file, level, provider, keyword);
 *  - bookmarks and EvtSeek; subscriptions, pulled and pushed;
 *  - channels, log information, channel configuration, publishers and
 *    their metadata; EvtExportLog and querying the exported file;
 *    EvtClearLog.
 * These were stubs (most functions missing, the rest failing). */
#include <windows.h>
#include <winevt.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define SOURCE L"SGEvtProbe"
#define MINE L"*[System[Provider[@Name='SGEvtProbe']]]"

static int count_query(const WCHAR *path, const WCHAR *query, DWORD flags, WCHAR *first_data)
{
    EVT_HANDLE q = EvtQuery(NULL, path, query, flags), ev[16];
    DWORD got, i;
    int n = 0;

    if (!q) return -1;
    while (EvtNext(q, 16, ev, 0, 0, &got))
    {
        for (i = 0; i < got; i++)
        {
            if (!n && first_data)
            {
                EVT_HANDLE ctx = EvtCreateRenderContext(0, NULL, EvtRenderContextUser);
                BYTE buf[1024];
                DWORD used, count;
                first_data[0] = 0;
                if (EvtRender(ctx, ev[i], EvtRenderEventValues, sizeof(buf), buf, &used, &count) && count &&
                    ((EVT_VARIANT *)buf)[0].Type == EvtVarTypeString)
                    lstrcpyW(first_data, ((EVT_VARIANT *)buf)[0].StringVal);
                EvtClose(ctx);
            }
            EvtClose(ev[i]);
            n++;
        }
    }
    EvtClose(q);
    return n;
}

static EVT_HANDLE first_event(const WCHAR *query)
{
    EVT_HANDLE q = EvtQuery(NULL, L"Application", query, EvtQueryChannelPath), ev = NULL;
    DWORD got;
    if (!q) return NULL;
    if (!EvtNext(q, 1, &ev, 0, 0, &got)) ev = NULL;
    EvtClose(q);
    return ev;
}

static void report(HANDLE source, WORD type, DWORD id, const WCHAR *s1, const WCHAR *s2, const void *data, DWORD len)
{
    const WCHAR *strings[2] = { s1, s2 };
    ReportEventW(source, type, 0, id, NULL, s2 ? 2 : 1, len, strings, (void *)data);
}

static LONG pushed;
static DWORD WINAPI push_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, void *ctx, EVT_HANDLE ev)
{
    if (action == EvtSubscribeActionDeliver) InterlockedIncrement(&pushed);
    return 0;
}

int main(void)
{
    static const BYTE data[] = { 0xde, 0xad, 0xbe, 0xef };
    WCHAR buf[8192], text[512], file[MAX_PATH], path[MAX_PATH];
    BYTE vbuf[4096];
    EVT_VARIANT *v = (EVT_VARIANT *)vbuf;
    EVT_HANDLE h, ev, ctx, q, bm, sig_sub, push_sub, arr, ev2;
    DWORD used, count, got, size;
    HANDLE source, sig;
    HKEY key;
    int n, i;
    BOOL ret, found;
    FILETIME now;

    /* a source with a message file (the Event Log service's own messages) */
    RegCreateKeyW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\EventLog\\Application\\" SOURCE, &key);
    RegSetValueExW(key, L"EventMessageFile", 0, REG_EXPAND_SZ, (BYTE *)L"%SystemRoot%\\system32\\wevtsvc.dll",
                   sizeof(L"%SystemRoot%\\system32\\wevtsvc.dll"));
    RegCloseKey(key);
    source = RegisterEventSourceW(NULL, SOURCE);
    check(source != NULL, "the source is registered");
    report(source, EVENTLOG_ERROR_TYPE, 104, L"Probe-A", NULL, data, sizeof(data));
    report(source, EVENTLOG_WARNING_TYPE, 1000, L"B1", L"B2", NULL, 0);
    report(source, EVENTLOG_INFORMATION_TYPE, 104, L"Probe-C", NULL, NULL, 0);

    /* channels */
    h = EvtOpenChannelEnum(NULL, 0);
    found = FALSE;
    while (EvtNextChannelPath(h, ARRAYSIZE(buf), buf, &used)) if (!lstrcmpiW(buf, L"Application")) found = TRUE;
    check(h && found && GetLastError() == ERROR_NO_MORE_ITEMS, "the channels include Application");
    EvtClose(h);

    /* queries */
    n = count_query(L"Application", MINE, EvtQueryChannelPath, text);
    check(n == 3 && !lstrcmpW(text, L"Probe-A"), "a provider filter: three events, oldest first");
    n = count_query(L"Application", MINE, EvtQueryChannelPath | EvtQueryReverseDirection, text);
    check(n == 3 && !lstrcmpW(text, L"Probe-C"), "backwards: newest first");
    check(count_query(L"Application", L"*[System[Provider[@Name='SGEvtProbe'] and (Level=2 or Level=3)]]",
                      EvtQueryChannelPath, NULL) == 2, "levels: error or warning");
    check(count_query(L"Application", L"*[EventData[Data='Probe-C']]", EvtQueryChannelPath, NULL) == 1,
          "event data");
    check(count_query(L"Application", L"Event/System[EventID=1000]", EvtQueryChannelPath, NULL) == 1,
          "an event ID, as a path");
    check(count_query(L"Application",
                      L"*[System[Provider[@Name='SGEvtProbe'] and TimeCreated[timediff(@SystemTime) <= 3600000]]]",
                      EvtQueryChannelPath, NULL) == 3, "timediff: the last hour");
    check(count_query(L"Application", L"*[System[Provider[@Name='SGEvtProbe'] and band(Keywords,0x80000000000000)]]",
                      EvtQueryChannelPath, NULL) == 3, "band: classic keywords");
    check(count_query(NULL, L"<QueryList><Query Id='0' Path='Application'><Select Path='Application'>"
                      L"*[System[Provider[@Name='SGEvtProbe']]]</Select><Suppress Path='Application'>"
                      L"*[System[Level=3]]</Suppress></Query></QueryList>", EvtQueryChannelPath, NULL) == 2,
          "a query list: select and suppress");
    SetLastError(0);
    check(!EvtQuery(NULL, L"Application", L"*[System[", EvtQueryChannelPath) &&
          GetLastError() == ERROR_EVT_INVALID_QUERY, "a bad query: ERROR_EVT_INVALID_QUERY");
    SetLastError(0);
    check(!EvtQuery(NULL, L"SGNoSuchChannel", L"*", EvtQueryChannelPath) &&
          GetLastError() == ERROR_EVT_CHANNEL_NOT_FOUND, "an unknown channel: ERROR_EVT_CHANNEL_NOT_FOUND");

    q = EvtQuery(NULL, L"Application", MINE, EvtQueryChannelPath);
    ret = EvtGetQueryInfo(q, EvtQueryNames, sizeof(vbuf), v, &used);
    check(ret && v->Type == (EvtVarTypeString | EVT_VARIANT_TYPE_ARRAY) && v->Count == 1 &&
          !lstrcmpiW(v->StringArr[0], L"Application"), "EvtGetQueryInfo: the paths");
    check(EvtSeek(q, 2, NULL, 0, EvtSeekRelativeToFirst) && EvtNext(q, 1, &ev, 0, 0, &got), "EvtSeek from the first");
    ctx = EvtCreateRenderContext(0, NULL, EvtRenderContextUser);
    check(EvtRender(ctx, ev, EvtRenderEventValues, sizeof(vbuf), vbuf, &used, &count) && count == 1 &&
          !lstrcmpW(v[0].StringVal, L"Probe-C"), "and it is the third");
    EvtClose(ev);
    EvtClose(q);

    /* rendering */
    ev = first_event(MINE);
    check(ev != NULL, "the first event");
    ret = EvtRender(NULL, ev, EvtRenderEventXml, 0, NULL, &used, &count);
    check(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && used > 100, "EvtRender XML: the size first");
    ret = EvtRender(NULL, ev, EvtRenderEventXml, sizeof(buf), buf, &used, &count);
    check(ret && wcsstr(buf, L"<Provider Name='SGEvtProbe'/>") && wcsstr(buf, L"<EventID Qualifiers='0'>104</EventID>") &&
          wcsstr(buf, L"<Level>2</Level>") && wcsstr(buf, L"<Channel>Application</Channel>") &&
          wcsstr(buf, L"<Data>Probe-A</Data>") && wcsstr(buf, L"<Binary>DEADBEEF</Binary>") &&
          wcsstr(buf, L"<Keywords>0x80000000000000</Keywords>"), "the XML");
    if (!ret || !wcsstr(buf, L"<Binary>")) printf("      %ls\n", buf);

    ctx = EvtCreateRenderContext(0, NULL, EvtRenderContextSystem);
    ret = EvtRender(ctx, ev, EvtRenderEventValues, sizeof(vbuf), vbuf, &used, &count);
    GetSystemTimeAsFileTime(&now);
    check(ret && count == EvtSystemPropertyIdEND && v[EvtSystemEventID].Type == EvtVarTypeUInt16 &&
          v[EvtSystemEventID].UInt16Val == 104 && v[EvtSystemLevel].ByteVal == 2 &&
          !lstrcmpW(v[EvtSystemProviderName].StringVal, SOURCE) && !lstrcmpW(v[EvtSystemChannel].StringVal, L"Application") &&
          v[EvtSystemEventRecordId].Type == EvtVarTypeUInt64 && v[EvtSystemEventRecordId].UInt64Val > 0 &&
          v[EvtSystemTimeCreated].Type == EvtVarTypeFileTime &&
          (((ULONGLONG)now.dwHighDateTime << 32 | now.dwLowDateTime) - v[EvtSystemTimeCreated].FileTimeVal) < 36000000000ull,
          "system values");
    EvtClose(ctx);
    {
        const WCHAR *paths[] = { L"Event/System/EventID", L"Event/EventData/Data", L"Event/System/Nothing" };
        ctx = EvtCreateRenderContext(3, paths, EvtRenderContextValues);
        ret = EvtRender(ctx, ev, EvtRenderEventValues, sizeof(vbuf), vbuf, &used, &count);
        check(ret && count == 3 && v[0].Type == EvtVarTypeUInt16 && v[0].UInt16Val == 104 &&
              v[1].Type == EvtVarTypeString && !lstrcmpW(v[1].StringVal, L"Probe-A") && v[2].Type == EvtVarTypeNull,
              "values by XPath");
        EvtClose(ctx);
    }

    /* messages */
    ret = EvtFormatMessage(NULL, ev, 0, 0, NULL, EvtFormatMessageEvent, ARRAYSIZE(text), text, &used);
    check(ret && !lstrcmpW(text, L"The Probe-A log file was cleared."), "the message, from the source's file");
    if (!ret || lstrcmpW(text, L"The Probe-A log file was cleared.")) printf("      %lu %ls\n", GetLastError(), text);
    check(EvtFormatMessage(NULL, ev, 0, 0, NULL, EvtFormatMessageLevel, ARRAYSIZE(text), text, &used) &&
          !lstrcmpW(text, L"Error"), "the level's name");
    check(EvtFormatMessage(NULL, ev, 0, 0, NULL, EvtFormatMessageProvider, ARRAYSIZE(text), text, &used) &&
          !lstrcmpW(text, SOURCE), "the provider's name");
    check(EvtFormatMessage(NULL, ev, 0, 0, NULL, EvtFormatMessageKeyword, ARRAYSIZE(text), text, &used) &&
          !lstrcmpW(text, L"Classic"), "the keyword's name");
    ev2 = first_event(L"*[System[EventID=1000]]");
    SetLastError(0);
    check(!EvtFormatMessage(NULL, ev2, 0, 0, NULL, EvtFormatMessageEvent, ARRAYSIZE(text), text, &used) &&
          GetLastError() == ERROR_EVT_MESSAGE_NOT_FOUND, "no message: ERROR_EVT_MESSAGE_NOT_FOUND");
    EvtClose(ev2);

    /* bookmarks */
    bm = EvtCreateBookmark(NULL);
    check(bm && EvtUpdateBookmark(bm, ev), "a bookmark on the first event");
    ret = EvtRender(NULL, bm, EvtRenderBookmark, sizeof(buf), buf, &used, &count);
    check(ret && wcsstr(buf, L"Channel='Application'") && wcsstr(buf, L"RecordId="), "rendered");
    EvtClose(bm);
    bm = EvtCreateBookmark(buf);
    check(bm != NULL, "and read back");
    q = EvtQuery(NULL, L"Application", MINE, EvtQueryChannelPath);
    ctx = EvtCreateRenderContext(0, NULL, EvtRenderContextUser);
    check(EvtSeek(q, 1, bm, 0, EvtSeekRelativeToBookmark) && EvtNext(q, 1, &ev2, 0, 0, &got) &&
          EvtRender(ctx, ev2, EvtRenderEventValues, sizeof(vbuf), vbuf, &used, &count) && count == 2 &&
          !lstrcmpW(v[0].StringVal, L"B1"), "EvtSeek past the bookmark");
    EvtClose(ev2);
    EvtClose(q);
    EvtClose(ctx);
    EvtClose(ev);

    /* subscriptions */
    sig = CreateEventW(NULL, TRUE, FALSE, NULL);
    sig_sub = EvtSubscribe(NULL, sig, L"Application", MINE, NULL, NULL, NULL, EvtSubscribeToFutureEvents);
    check(sig_sub != NULL, "a pull subscription to future events");
    push_sub = EvtSubscribe(NULL, NULL, L"Application", MINE, bm, NULL, push_callback, EvtSubscribeStartAfterBookmark);
    check(push_sub != NULL, "a push subscription after the bookmark");
    Sleep(500);
    report(source, EVENTLOG_INFORMATION_TYPE, 104, L"Probe-D", NULL, NULL, 0);
    check(WaitForSingleObject(sig, 15000) == WAIT_OBJECT_0, "the pull subscription is signalled");
    ret = EvtNext(sig_sub, 4, &ev, 0, 0, &got);
    ctx = EvtCreateRenderContext(0, NULL, EvtRenderContextUser);
    check(ret && got == 1 && EvtRender(ctx, ev, EvtRenderEventValues, sizeof(vbuf), vbuf, &used, &count) &&
          !lstrcmpW(v[0].StringVal, L"Probe-D"), "with the new event only");
    if (ret) EvtClose(ev);
    EvtClose(ctx);
    for (i = 0; i < 100 && pushed < 3; i++) Sleep(100);
    check(pushed == 3, "the push subscription delivers the three after the bookmark");
    EvtClose(sig_sub);
    EvtClose(push_sub);
    EvtClose(bm);

    /* logs and channel configuration */
    h = EvtOpenLog(NULL, L"Application", EvtOpenChannelPath);
    check(h && EvtGetLogInfo(h, EvtLogNumberOfLogRecords, sizeof(vbuf), v, &used) && v->Type == EvtVarTypeUInt64 &&
          v->UInt64Val >= 4, "EvtGetLogInfo: the records");
    EvtClose(h);
    h = EvtOpenChannelConfig(NULL, L"Application", 0);
    check(h && EvtGetChannelConfigProperty(h, EvtChannelConfigClassicEventlog, 0, sizeof(vbuf), v, &used) &&
          v->Type == EvtVarTypeBoolean && v->BooleanVal, "Application is a classic log");
    check(EvtGetChannelConfigProperty(h, EvtChannelPublisherList, 0, sizeof(vbuf), v, &used) &&
          v->Type == (EvtVarTypeString | EVT_VARIANT_TYPE_ARRAY) && v->Count >= 1, "its publishers");
    {
        EVT_VARIANT set;
        set.Type = EvtVarTypeUInt64;
        set.UInt64Val = 1024 * 1024;
        check(EvtSetChannelConfigProperty(h, EvtChannelLoggingConfigMaxSize, 0, &set) && EvtSaveChannelConfig(h, 0),
              "its maximum size is set and saved");
        size = 0;
        used = sizeof(size);
        RegGetValueW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\EventLog\\Application", L"MaxSize",
                     RRF_RT_REG_DWORD, NULL, &size, &used);
        check(size == 1024 * 1024, "in the registry");
        set.Type = EvtVarTypeString;
        set.StringVal = L"x";
        check(!EvtSetChannelConfigProperty(h, EvtChannelLoggingConfigMaxSize, 0, &set) &&
              GetLastError() == ERROR_EVT_INVALID_CHANNEL_PROPERTY_VALUE, "a wrong type is refused");
    }
    EvtClose(h);

    /* publishers */
    h = EvtOpenPublisherEnum(NULL, 0);
    found = FALSE;
    while (EvtNextPublisherId(h, ARRAYSIZE(buf), buf, &used)) if (!lstrcmpW(buf, SOURCE)) found = TRUE;
    check(h && found, "the publishers include the source");
    EvtClose(h);
    h = EvtOpenPublisherMetadata(NULL, SOURCE, NULL, 0, 0);
    check(h && EvtGetPublisherMetadataProperty(h, EvtPublisherMetadataMessageFilePath, 0, sizeof(vbuf), v, &used) &&
          v->Type == EvtVarTypeString && wcsstr(v->StringVal, L"wevtsvc.dll"), "its message file");
    ret = EvtGetPublisherMetadataProperty(h, EvtPublisherMetadataLevels, 0, sizeof(vbuf), v, &used);
    arr = ret ? v->EvtHandleVal : NULL;
    check(arr && EvtGetObjectArraySize(arr, &size) && size == 5 &&
          EvtGetObjectArrayProperty(arr, EvtPublisherMetadataLevelName, 1, 0, sizeof(vbuf), v, &used) &&
          !lstrcmpW(v->StringVal, L"win:Error"), "its levels");
    if (arr) EvtClose(arr);
    EvtClose(h);

    /* export */
    GetTempPathW(MAX_PATH, path);
    swprintf(file, MAX_PATH, L"%lsevtquery-export.sgevt", path);
    DeleteFileW(file);
    check(EvtExportLog(NULL, L"Application", MINE, file, EvtExportLogChannelPath), "EvtExportLog");
    SetLastError(0);
    check(!EvtExportLog(NULL, L"Application", MINE, file, EvtExportLogChannelPath) &&
          GetLastError() == ERROR_FILE_EXISTS, "not over a file without EvtExportLogOverwrite");
    check(count_query(file, L"*", EvtQueryFilePath, text) == 4 && !lstrcmpW(text, L"Probe-A"),
          "the exported file is queried");
    check(EvtArchiveExportedLog(NULL, file, 0, 0), "EvtArchiveExportedLog");

    /* clearing */
    check(EvtClearLog(NULL, L"Application", NULL, 0), "EvtClearLog");
    check(count_query(L"Application", MINE, EvtQueryChannelPath, NULL) == 0, "the log is empty");

    DeregisterEventSource(source);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
