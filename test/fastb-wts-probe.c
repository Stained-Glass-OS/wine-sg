/* wtsapi32: WTSEnumerateProcessesEx honours the session id, WTSWaitSystemEvent waits (patch 3737) */
#include <windows.h>
#include <wtsapi32.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static BOOL (WINAPI *pEnumEx)(HANDLE, DWORD *, DWORD, WCHAR **, DWORD *);
static BOOL (WINAPI *pFreeEx)(WTS_TYPE_CLASS, void *, ULONG);

static DWORD waiter_flags = 0xdead; static BOOL waiter_ret; static DWORD waiter_done;

static DWORD WINAPI waiter(void *arg)
{
    waiter_ret = WTSWaitSystemEvent(WTS_CURRENT_SERVER_HANDLE, WTS_EVENT_ALL, &waiter_flags);
    waiter_done = GetTickCount();
    return 0;
}

int main(void)
{
    HMODULE w = LoadLibraryA("wtsapi32.dll");
    DWORD level = 0, count = 0, i, mysession = 99, flags = 0xdead;
    WTS_PROCESS_INFOW *info = NULL;
    BOOL ret, found;

    pEnumEx = (void *)GetProcAddress(w, "WTSEnumerateProcessesExW");
    pFreeEx = (void *)GetProcAddress(w, "WTSFreeMemoryExW");
    ProcessIdToSessionId(GetCurrentProcessId(), &mysession);

    ret = pEnumEx(WTS_CURRENT_SERVER_HANDLE, &level, WTS_ANY_SESSION, (WCHAR **)&info, &count);
    CHECK(ret && count > 0, "all sessions: %d %lu", ret, count);
    if (ret) { DWORD all = count; pFreeEx(WTSTypeProcessInfoLevel0, info, count);

        level = 0; info = NULL;
        ret = pEnumEx(WTS_CURRENT_SERVER_HANDLE, &level, mysession, (WCHAR **)&info, &count);
        CHECK(ret && count > 0 && count <= all, "my session: %d %lu of %lu", ret, count, all);
        found = FALSE;
        for (i = 0; ret && i < count; i++)
        {
            CHECK(info[i].SessionId == mysession, "process %lu is of session %lu, not %lu", info[i].ProcessId, info[i].SessionId, mysession);
            if (info[i].ProcessId == GetCurrentProcessId()) found = TRUE;
        }
        CHECK(found, "this process is in the list of its session");
        if (ret) pFreeEx(WTSTypeProcessInfoLevel0, info, count);

        level = 0; info = NULL;
        ret = pEnumEx(WTS_CURRENT_SERVER_HANDLE, &level, WTS_CURRENT_SESSION, (WCHAR **)&info, &count);
        CHECK(ret && count > 0, "current session: %d %lu", ret, count);
        for (i = 0; ret && i < count; i++) CHECK(info[i].SessionId == mysession, "current session entry %lu has session %lu", i, info[i].SessionId);
        if (ret) pFreeEx(WTSTypeProcessInfoLevel0, info, count);

        level = 0; info = NULL;
        ret = pEnumEx(WTS_CURRENT_SERVER_HANDLE, &level, 4242, (WCHAR **)&info, &count);
        CHECK(ret && count == 0, "a session with no processes: %d %lu", ret, count);
        if (ret) pFreeEx(WTSTypeProcessInfoLevel0, info, count);
    }
    SetLastError(0xdeadbeef);
    ret = pEnumEx(WTS_CURRENT_SERVER_HANDLE, NULL, WTS_ANY_SESSION, (WCHAR **)&info, &count);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL level: %d %lu", ret, GetLastError());

    /* WTSWaitSystemEvent */
    SetLastError(0xdeadbeef);
    ret = WTSWaitSystemEvent(WTS_CURRENT_SERVER_HANDLE, WTS_EVENT_ALL, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL flags: %d %lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = WTSWaitSystemEvent((HANDLE)0x1234, WTS_EVENT_ALL, &flags);
    CHECK(!ret && GetLastError() == ERROR_INVALID_HANDLE, "bad server: %d %lu", ret, GetLastError());
    flags = 0xdead;
    ret = WTSWaitSystemEvent(WTS_CURRENT_SERVER_HANDLE, WTS_EVENT_NONE, &flags);
    CHECK(ret && flags == WTS_EVENT_NONE, "wait for nothing: %d %#lx", ret, flags);
    {
        HANDLE th;
        DWORD start = GetTickCount(), wr;

        th = CreateThread(NULL, 0, waiter, NULL, 0, NULL);
        Sleep(400);
        CHECK(WaitForSingleObject(th, 0) == WAIT_TIMEOUT, "the waiter is still waiting");
        flags = 0xdead;
        ret = WTSWaitSystemEvent(WTS_CURRENT_SERVER_HANDLE, WTS_EVENT_FLUSH, &flags);
        CHECK(ret && flags == WTS_EVENT_FLUSH, "flush: %d %#lx", ret, flags);
        wr = WaitForSingleObject(th, 3000);
        CHECK(wr == WAIT_OBJECT_0, "the flush released the waiter (%lu)", wr);
        if (wr == WAIT_OBJECT_0)
            CHECK(waiter_ret && waiter_flags == WTS_EVENT_FLUSH && waiter_done - start >= 300, "waiter: %d %#lx after %lu ms", waiter_ret, waiter_flags, waiter_done - start);
    }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    fflush(stdout);
    ExitProcess(f != 0);
}
