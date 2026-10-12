/* winsta: session enumeration and lookups agree with wtsapi32 */
#include <windows.h>
#include <wtsapi32.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { ULONG id; WCHAR name[33]; int state; } SESSIONIDW_;

int main(void)
{
    HMODULE m = LoadLibraryA("winsta.dll");
    BOOLEAN (WINAPI *enumerate)(HANDLE, SESSIONIDW_ **, ULONG *) = (void *)GetProcAddress(m, "WinStationEnumerateW");
    BOOLEAN (WINAPI *free_mem)(void *) = (void *)GetProcAddress(m, "WinStationFreeMemory");
    BOOLEAN (WINAPI *name_w)(HANDLE, ULONG, WCHAR *) = (void *)GetProcAddress(m, "WinStationNameFromLogonIdW");
    BOOLEAN (WINAPI *name_a)(HANDLE, ULONG, char *) = (void *)GetProcAddress(m, "WinStationNameFromLogonIdA");
    BOOLEAN (WINAPI *id_w)(HANDLE, const WCHAR *, ULONG *) = (void *)GetProcAddress(m, "LogonIdFromWinStationNameW");
    BOOLEAN (WINAPI *id_a)(HANDLE, const char *, ULONG *) = (void *)GetProcAddress(m, "LogonIdFromWinStationNameA");
    BOOLEAN (WINAPI *help)(HANDLE, ULONG) = (void *)GetProcAddress(m, "WinStationIsHelpAssistantSession");
    BOOLEAN (WINAPI *get_sid)(HANDLE, ULONG, FILETIME, void *, ULONG *) = (void *)GetProcAddress(m, "WinStationGetProcessSid");
    BOOLEAN (WINAPI *reg)(HANDLE, HWND, ULONG) = (void *)GetProcAddress(m, "WinStationRegisterConsoleNotification");
    BOOLEAN (WINAPI *unreg)(HANDLE, HWND) = (void *)GetProcAddress(m, "WinStationUnRegisterConsoleNotification");
    SESSIONIDW_ *ids = NULL;
    WTS_SESSION_INFOW *wts = NULL;
    DWORD wcount = 0, i;
    ULONG count = 0, id = 99;
    WCHAR nameW[33];
    char nameA[40];
    FILETIME created, a, b, c;
    BYTE sid[SECURITY_MAX_SID_SIZE], sid2[SECURITY_MAX_SID_SIZE];
    ULONG size;
    HANDLE token;
    DWORD tsize = 0;
    TOKEN_USER *tu;
    HWND hwnd;

    CHECK(m && enumerate && free_mem && name_w && name_a && id_w && id_a && help && get_sid && reg && unreg, "exports");
    if (!m) goto end;

    CHECK(enumerate(NULL, &ids, &count), "WinStationEnumerateW %lu", GetLastError());
    CHECK(WTSEnumerateSessionsW(NULL, 0, 1, &wts, &wcount), "WTSEnumerateSessionsW");
    CHECK(count == wcount && count >= 2, "counts %lu %lu", count, wcount);
    for (i = 0; ids && wts && i < count && i < wcount; i++)
    {
        CHECK(ids[i].id == wts[i].SessionId, "id %lu vs %lu", ids[i].id, wts[i].SessionId);
        CHECK(!wcscmp(ids[i].name, wts[i].pWinStationName), "name %ls vs %ls", ids[i].name, wts[i].pWinStationName);
        CHECK(ids[i].state == (int)wts[i].State, "state %d vs %d", ids[i].state, (int)wts[i].State);
    }
    if (ids && count >= 2)
    {
        CHECK(name_w(NULL, ids[0].id, nameW) && !wcscmp(nameW, ids[0].name), "NameFromLogonIdW %ls", nameW);
        CHECK(name_a(NULL, ids[1].id, nameA) && nameA[0], "NameFromLogonIdA");
        CHECK(id_w(NULL, ids[1].name, &id) && id == ids[1].id, "LogonIdFromWinStationNameW %lu", id);
        {
            WCHAR upper[33];
            wcscpy(upper, ids[1].name);
            CharUpperW(upper);
            id = 99;
            CHECK(id_w(NULL, upper, &id) && id == ids[1].id, "case-insensitive");
        }
        id = 99;
        WideCharToMultiByte(CP_ACP, 0, ids[0].name, -1, nameA, sizeof(nameA), NULL, NULL);
        CHECK(id_a(NULL, nameA, &id) && id == ids[0].id, "LogonIdFromWinStationNameA");
    }
    SetLastError(0);
    CHECK(!name_w(NULL, 4242, nameW) && GetLastError() == ERROR_CTX_WINSTATION_NOT_FOUND, "unknown id: %lu", GetLastError());
    SetLastError(0);
    CHECK(!id_w(NULL, L"no-such-session", &id) && GetLastError() == ERROR_CTX_WINSTATION_NOT_FOUND, "unknown name: %lu", GetLastError());
    CHECK(!help(NULL, 1), "no assistant session");
    if (ids) free_mem(ids);
    if (wts) WTSFreeMemory(wts);

    /* the SID of this process */
    GetProcessTimes(GetCurrentProcess(), &created, &a, &b, &c);
    size = sizeof(sid);
    CHECK(get_sid(NULL, GetCurrentProcessId(), created, sid, &size), "WinStationGetProcessSid %lu", GetLastError());
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, NULL, 0, &tsize);
    tu = malloc(tsize);
    GetTokenInformation(token, TokenUser, tu, tsize, &tsize);
    CopySid(sizeof(sid2), sid2, tu->User.Sid);
    CHECK(size == GetLengthSid(sid2) && EqualSid(sid, sid2), "same SID as the token");
    size = 1;
    SetLastError(0);
    CHECK(!get_sid(NULL, GetCurrentProcessId(), created, sid, &size) && GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
          size == GetLengthSid(sid2), "small buffer: %lu size %lu", GetLastError(), size);
    created.dwLowDateTime ^= 1;
    size = sizeof(sid);
    CHECK(!get_sid(NULL, GetCurrentProcessId(), created, sid, &size), "wrong start time is refused");

    /* session notifications */
    hwnd = CreateWindowA("STATIC", "n", 0, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
    CHECK(hwnd != NULL, "window");
    CHECK(reg(NULL, hwnd, 0), "RegisterConsoleNotification %lu", GetLastError());
    CHECK(unreg(NULL, hwnd), "UnRegisterConsoleNotification %lu", GetLastError());
    DestroyWindow(hwnd);
end:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
