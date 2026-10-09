/* wtsapi32's session/server/process calls (patches/sg/2400): a desktop
 * with no Terminal Services role still has the "Services" (0) and
 * "Console" sessions a real Windows machine reports, and
 * WTSQuerySessionInformation answers every class that makes sense without
 * a remote client instead of failing outright. Run by
 * test/wtssession-gate.sh.
 *
 *   wtssession-probe.exe sleep   -- (internal) just Sleep()s, so the parent
 *                                   run can WTSTerminateProcess() it. */
#include <windows.h>
#include <wtsapi32.h>
#include <lmcons.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef HANDLE (WINAPI *openserver_fn)(WCHAR *);
typedef void (WINAPI *closeserver_fn)(HANDLE);
typedef BOOL (WINAPI *enumservers_fn)(WCHAR *, DWORD, DWORD, void *, DWORD *);
typedef BOOL (WINAPI *enumsessions_fn)(HANDLE, DWORD, DWORD, void *, DWORD *);
typedef BOOL (WINAPI *enumsessionsex_fn)(HANDLE, DWORD *, DWORD, void *, DWORD *);
typedef BOOL (WINAPI *querysession_fn)(HANDLE, DWORD, WTS_INFO_CLASS, void *, DWORD *);
typedef BOOL (WINAPI *querysessiona_fn)(HANDLE, DWORD, WTS_INFO_CLASS, char **, DWORD *);
typedef void (WINAPI *freemem_fn)(void *);
typedef BOOL (WINAPI *enumprocsw_fn)(HANDLE, DWORD, DWORD, WTS_PROCESS_INFOW **, DWORD *);
typedef BOOL (WINAPI *enumprocsa_fn)(HANDLE, DWORD, DWORD, WTS_PROCESS_INFOA **, DWORD *);
typedef BOOL (WINAPI *queryuserconfig_fn)(WCHAR *, WCHAR *, WTS_CONFIG_CLASS, WCHAR **, DWORD *);
typedef BOOL (WINAPI *setuserconfig_fn)(WCHAR *, WCHAR *, WTS_CONFIG_CLASS, WCHAR *, DWORD);
typedef BOOL (WINAPI *setsessioninfo_fn)(HANDLE, DWORD, WTS_INFO_CLASS, void *, DWORD);
typedef BOOL (WINAPI *enablechild_fn)(BOOL);
typedef BOOL (WINAPI *terminate_fn)(HANDLE, DWORD, DWORD);

int main(int argc, char **argv)
{
    HMODULE wts;
    openserver_fn pOpenServerW;
    closeserver_fn pCloseServer;
    enumservers_fn pEnumServersW;
    enumsessions_fn pEnumSessionsW;
    enumsessionsex_fn pEnumSessionsExW;
    querysession_fn pQuerySessionW;
    querysessiona_fn pQuerySessionA;
    freemem_fn pFreeMemory;
    enumprocsw_fn pEnumProcessesW;
    enumprocsa_fn pEnumProcessesA;
    queryuserconfig_fn pQueryUserConfigW;
    setuserconfig_fn pSetUserConfigW;
    setsessioninfo_fn pSetSessionInformationW;
    enablechild_fn pEnableChildSessions;
    terminate_fn pTerminateProcess;

    if (argc > 1 && !strcmp(argv[1], "sleep"))
    {
        Sleep(30000);
        return 0;
    }

    wts = LoadLibraryA("wtsapi32.dll");
    if (!wts) { printf("FAIL  could not load wtsapi32.dll\n"); return 1; }

#define GET(var, name) var = (void *)GetProcAddress(wts, name); \
    if (!var) { printf("FAIL  no export %s\n", name); failures++; }
    GET(pOpenServerW, "WTSOpenServerW");
    GET(pCloseServer, "WTSCloseServer");
    GET(pEnumServersW, "WTSEnumerateServersW");
    GET(pEnumSessionsW, "WTSEnumerateSessionsW");
    GET(pEnumSessionsExW, "WTSEnumerateSessionsExW");
    GET(pQuerySessionW, "WTSQuerySessionInformationW");
    GET(pQuerySessionA, "WTSQuerySessionInformationA");
    GET(pFreeMemory, "WTSFreeMemory");
    GET(pEnumProcessesW, "WTSEnumerateProcessesW");
    GET(pEnumProcessesA, "WTSEnumerateProcessesA");
    GET(pQueryUserConfigW, "WTSQueryUserConfigW");
    GET(pSetUserConfigW, "WTSSetUserConfigW");
    GET(pSetSessionInformationW, "WTSSetSessionInformationW");
    GET(pEnableChildSessions, "WTSEnableChildSessions");
    GET(pTerminateProcess, "WTSTerminateProcess");
#undef GET
    if (failures) goto done;

    {
        WCHAR me[MAX_COMPUTERNAME_LENGTH + 1], user[UNLEN + 1];
        DWORD size = ARRAY_SIZE(me);
        DWORD current_session;
        GetComputerNameW(me, &size);
        size = ARRAY_SIZE(user);
        GetUserNameW(user, &size);
        ProcessIdToSessionId(GetCurrentProcessId(), &current_session);

        /* WTSOpenServerW/WTSCloseServer: local vs. unreachable remote */
        {
            HANDLE h = pOpenServerW(NULL);
            check(h != NULL, "WTSOpenServerW(NULL) returns a local handle");
            pCloseServer(h);

            SetLastError(0);
            h = pOpenServerW(L"no-such-terminal-server-xyz");
            check(h == NULL && GetLastError() == RPC_S_SERVER_UNAVAILABLE,
                  "WTSOpenServerW(unreachable) fails RPC_S_SERVER_UNAVAILABLE");
        }

        /* WTSEnumerateServersW: just this computer */
        {
            WTS_SERVER_INFOW *servers = NULL;
            DWORD count = 0;
            BOOL ok = pEnumServersW(NULL, 0, 1, &servers, &count);
            check(ok && count == 1 && servers && !lstrcmpiW(servers[0].pServerName, me),
                  "WTSEnumerateServersW reports this computer");
            if (servers) pFreeMemory(servers);
        }

        /* WTSEnumerateSessionsW: Services (0) + Console (active, ours) */
        {
            WTS_SESSION_INFOW *sessions = NULL;
            DWORD count = 0, i;
            int saw_services = 0, saw_console = 0;
            BOOL ok = pEnumSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count);
            check(ok && count == 2, "WTSEnumerateSessionsW returns 2 sessions");
            for (i = 0; ok && i < count; i++)
            {
                if (sessions[i].SessionId == 0 && sessions[i].State == WTSDisconnected
                        && !lstrcmpW(sessions[i].pWinStationName, L"Services"))
                    saw_services = 1;
                if (sessions[i].SessionId == current_session && sessions[i].State == WTSActive
                        && !lstrcmpW(sessions[i].pWinStationName, L"Console"))
                    saw_console = 1;
            }
            check(saw_services, "WTSEnumerateSessionsW includes session 0 \"Services\"");
            check(saw_console, "WTSEnumerateSessionsW includes the active \"Console\" session");
            if (sessions) pFreeMemory(sessions);
        }

        /* WTSEnumerateSessionsExW level 1: same two, with names */
        {
            WTS_SESSION_INFO_1W *sessions = NULL;
            DWORD level = 1, count = 0, i;
            int saw_console = 0;
            BOOL ok = pEnumSessionsExW(WTS_CURRENT_SERVER_HANDLE, &level, 0, &sessions, &count);
            check(ok && count == 2, "WTSEnumerateSessionsExW returns 2 sessions");
            for (i = 0; ok && i < count; i++)
                if (sessions[i].SessionId == current_session
                        && !lstrcmpW(sessions[i].pUserName, user)
                        && !lstrcmpiW(sessions[i].pDomainName, me))
                    saw_console = 1;
            check(saw_console, "WTSEnumerateSessionsExW's console entry has our user/domain");
            if (sessions) pFreeMemory(sessions);
        }

        /* WTSQuerySessionInformationW(WTSSessionInfo): the full struct */
        {
            WTSINFOW *info = NULL;
            DWORD count = 0;
            BOOL ok = pQuerySessionW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfo, &info, &count);
            check(ok && count == sizeof(*info), "WTSQuerySessionInformationW(WTSSessionInfo) succeeds");
            if (ok)
            {
                check(info->State == WTSActive, "  State == WTSActive");
                check(info->SessionId == current_session, "  SessionId matches");
                check(!lstrcmpW(info->WinStationName, L"Console"), "  WinStationName == \"Console\"");
                check(!lstrcmpW(info->UserName, user), "  UserName matches GetUserNameW");
                check(!lstrcmpiW(info->Domain, me), "  Domain matches GetComputerNameW");
                check(info->LogonTime.QuadPart != 0, "  LogonTime is filled in");
                check(info->LastInputTime.QuadPart != 0, "  LastInputTime is filled in");
                check(info->LogonTime.QuadPart == info->ConnectTime.QuadPart, "  ConnectTime == LogonTime");
                check(info->DisconnectTime.QuadPart == 0, "  DisconnectTime == 0 (never disconnected)");
                check(info->LogonTime.QuadPart <= info->CurrentTime.QuadPart,
                      "  LogonTime is not after CurrentTime");
                check(info->IncomingBytes == 0 && info->OutgoingBytes == 0,
                      "  no network byte counts for a console session");
            }
            if (info) pFreeMemory(info);
        }

        /* same, ANSI: the fields WTSQuerySessionInformationA used to drop */
        {
            WTSINFOA *info = NULL;
            DWORD count = 0;
            BOOL ok = pQuerySessionA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfo,
                    (char **)&info, &count);
            check(ok && info && info->LogonTime.QuadPart != 0 && !strcmp(info->WinStationName, "Console"),
                  "WTSQuerySessionInformationA(WTSSessionInfo) carries LogonTime/WinStationName too");
            if (info) pFreeMemory(info);
        }

        /* a handful of the simple classes */
        {
            WCHAR *buf;
            DWORD count;
            BOOL ok;

            ok = pQuerySessionW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSUserName, &buf, &count);
            check(ok && !lstrcmpW(buf, user), "WTSQuerySessionInformationW(WTSUserName) matches GetUserNameW");
            if (ok) pFreeMemory(buf);

            ok = pQuerySessionW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId, &buf, &count);
            check(ok && count == sizeof(DWORD) && *(DWORD *)buf == current_session,
                  "WTSQuerySessionInformationW(WTSSessionId) matches");
            if (ok) pFreeMemory(buf);

            ok = pQuerySessionW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSIsRemoteSession, &buf, &count);
            check(ok && count == sizeof(BOOL) && *(BOOL *)buf == FALSE,
                  "WTSQuerySessionInformationW(WTSIsRemoteSession) == FALSE");
            if (ok) pFreeMemory(buf);

            ok = pQuerySessionW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSClientDisplay, &buf, &count);
            check(ok && count == sizeof(WTS_CLIENT_DISPLAY), "WTSQuerySessionInformationW(WTSClientDisplay) succeeds");
            if (ok) pFreeMemory(buf);

            SetLastError(0);
            ok = pQuerySessionW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSConfigInfo, &buf, &count);
            check(!ok && GetLastError() == ERROR_NOT_SUPPORTED,
                  "WTSQuerySessionInformationW(WTSConfigInfo) fails ERROR_NOT_SUPPORTED (no TS role)");
        }

        /* WTSEnumerateProcessesW/A: our own process is in the list */
        {
            WTS_PROCESS_INFOW *procsW = NULL;
            WTS_PROCESS_INFOA *procsA = NULL;
            DWORD count = 0, i;
            int saw_selfW = 0, saw_selfA = 0;
            DWORD pid = GetCurrentProcessId();

            if (pEnumProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &procsW, &count))
            {
                for (i = 0; i < count; i++) if (procsW[i].ProcessId == pid) saw_selfW = 1;
                pFreeMemory(procsW);
            }
            check(saw_selfW, "WTSEnumerateProcessesW's list includes this process");

            count = 0;
            if (pEnumProcessesA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &procsA, &count))
            {
                for (i = 0; i < count; i++) if (procsA[i].ProcessId == pid) saw_selfA = 1;
                pFreeMemory(procsA);
            }
            check(saw_selfA, "WTSEnumerateProcessesA's list includes this process");
        }

        /* WTSQueryUserConfigW / WTSSetUserConfigW */
        {
            WCHAR *buf;
            DWORD count;
            BOOL ok = pQueryUserConfigW(NULL, NULL, WTSUserConfigfAllowLogonTerminalServer, &buf, &count);
            check(ok && count == sizeof(DWORD) && *(DWORD *)buf,
                  "WTSQueryUserConfigW(AllowLogonTerminalServer) == TRUE");
            if (ok) LocalFree(buf);

            SetLastError(0);
            ok = pSetUserConfigW(NULL, NULL, WTSUserConfigInitialProgram, L"", 2);
            check(!ok && GetLastError() == ERROR_NOT_SUPPORTED,
                  "WTSSetUserConfigW fails ERROR_NOT_SUPPORTED (no config store)");
        }

        /* WTSSetSessionInformationW: newly exported, not just "@ stub" */
        {
            DWORD v = 0;
            SetLastError(0);
            BOOL ok = pSetSessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                    WTSSessionInfo, &v, sizeof(v));
            check(!ok && GetLastError() == ERROR_NOT_SUPPORTED,
                  "WTSSetSessionInformationW is exported and answers ERROR_NOT_SUPPORTED");
        }

        check(pEnableChildSessions(TRUE) && pEnableChildSessions(FALSE),
              "WTSEnableChildSessions succeeds either way");

        /* WTSTerminateProcess: really ends a child process */
        {
            WCHAR self[MAX_PATH], cmdline[MAX_PATH + 16];
            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            GetModuleFileNameW(NULL, self, ARRAY_SIZE(self));
            wsprintfW(cmdline, L"\"%s\" sleep", self);
            if (CreateProcessW(self, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            {
                BOOL ok = pTerminateProcess(WTS_CURRENT_SERVER_HANDLE, pi.dwProcessId, 0);
                DWORD wait = ok ? WaitForSingleObject(pi.hProcess, 5000) : WAIT_FAILED;
                check(ok && wait == WAIT_OBJECT_0, "WTSTerminateProcess really ends the target process");
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            else check(0, "could not spawn a child process to terminate");
        }
    }

done:
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
