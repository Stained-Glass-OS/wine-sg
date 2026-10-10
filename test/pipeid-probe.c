/* GetNamedPipe{Client,Server}ProcessId / SessionId error semantics
 * (patches/sg/2210): NULL result pointer is ERROR_INSUFFICIENT_BUFFER, a
 * server that is not connected has no client (ERROR_NOT_FOUND), and the client
 * of a disconnected pipe is ERROR_PIPE_NOT_CONNECTED.
 *
 *   probe.exe [OTHER.exe]    checks, then a second process connects as client
 *   probe.exe --client PID   connect, compare both process ids; never spawns */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PIPE "\\\\.\\pipe\\sgpipeid_probe"

static BOOL (WINAPI *pClientPid)(HANDLE, ULONG *);
static BOOL (WINAPI *pServerPid)(HANDLE, ULONG *);
static BOOL (WINAPI *pClientSession)(HANDLE, ULONG *);
static BOOL (WINAPI *pServerSession)(HANDLE, ULONG *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkerr(BOOL ret, DWORD err, DWORD want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (ret %d, error %lu)", what, ret, (unsigned long)err);
    check(!ret && err == want, buf);
}

static HANDLE make_server(BOOL overlapped)
{
    return CreateNamedPipeA(PIPE, PIPE_ACCESS_DUPLEX | (overlapped ? FILE_FLAG_OVERLAPPED : 0),
                            PIPE_TYPE_BYTE | PIPE_WAIT, 4, 4096, 4096, 0, NULL);
}

static HANDLE connect_pair(HANDLE *server)
{
    HANDLE client;

    *server = make_server(FALSE);
    client = CreateFileA(PIPE, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (*server != INVALID_HANDLE_VALUE && client != INVALID_HANDLE_VALUE) ConnectNamedPipe(*server, NULL);
    return client;
}

static int child(DWORD parent)
{
    HANDLE c = CreateFileA(PIPE, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    ULONG sp = 0, cp = 0;

    if (c == INVALID_HANDLE_VALUE) return 10;
    if (!pServerPid(c, &sp) || !pClientPid(c, &cp)) return 11;
    if (sp != parent) return 12;
    if (cp != GetCurrentProcessId()) return 13;
    return 0;
}

int main(int argc, char **argv)
{
    HMODULE k = GetModuleHandleA("kernel32.dll");
    HANDLE server, client, s2;
    ULONG pid;
    DWORD cur = GetCurrentProcessId(), code = 99;
    BOOL ret;
    int i;

    pClientPid = (void *)GetProcAddress(k, "GetNamedPipeClientProcessId");
    pServerPid = (void *)GetProcAddress(k, "GetNamedPipeServerProcessId");
    pClientSession = (void *)GetProcAddress(k, "GetNamedPipeClientSessionId");
    pServerSession = (void *)GetProcAddress(k, "GetNamedPipeServerSessionId");
    if (argc >= 3 && !strcmp(argv[1], "--client")) return child(strtoul(argv[2], NULL, 10));

    client = connect_pair(&server);
    check(server != INVALID_HANDLE_VALUE && client != INVALID_HANDLE_VALUE, "a connected pipe");

    SetLastError(0xdeadbeef);
    ret = pClientPid(server, NULL);
    checkerr(ret, GetLastError(), ERROR_INSUFFICIENT_BUFFER, "ClientProcessId(NULL) is ERROR_INSUFFICIENT_BUFFER");
    SetLastError(0xdeadbeef);
    ret = pServerPid(server, NULL);
    checkerr(ret, GetLastError(), ERROR_INSUFFICIENT_BUFFER, "ServerProcessId(NULL) is ERROR_INSUFFICIENT_BUFFER");
    SetLastError(0xdeadbeef);
    ret = pServerSession(server, NULL);
    checkerr(ret, GetLastError(), ERROR_INSUFFICIENT_BUFFER, "ServerSessionId(NULL) is ERROR_INSUFFICIENT_BUFFER");
    SetLastError(0xdeadbeef);
    ret = pClientSession(server, NULL);
    checkerr(ret, GetLastError(), ERROR_INSUFFICIENT_BUFFER, "ClientSessionId(NULL) is ERROR_INSUFFICIENT_BUFFER");

    pid = 0;
    check(pClientPid(server, &pid) && pid == cur, "connected: the server sees its client's process");
    pid = 0;
    check(pServerPid(client, &pid) && pid == cur, "connected: the client sees its server's process");
    pid = 0;
    check(pClientPid(client, &pid) && pid == cur && pServerPid(server, &pid) && pid == cur,
          "connected: either end can ask for either id");

    /* a second process connects and sees us, and we see it */
    CloseHandle(client);
    DisconnectNamedPipe(server);
    CloseHandle(server);
    server = make_server(TRUE);
    for (i = 1; i <= argc; i++)
    {
        const char *exe = i < argc ? argv[i] : argv[0];
        char cmd[MAX_PATH + 64];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        OVERLAPPED ov = { 0 };

        ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (!ConnectNamedPipe(server, &ov) && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_PIPE_CONNECTED)
        {
            check(0, "a listening server for the second process");
            CloseHandle(ov.hEvent);
            break;
        }
        snprintf(cmd, sizeof(cmd), "\"%s\" --client %lu", exe, (unsigned long)cur);
        code = 99;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            if (WaitForSingleObject(ov.hEvent, 20000) == WAIT_OBJECT_0 || GetLastError() == ERROR_PIPE_CONNECTED)
            {
                pid = 0;
                check(pClientPid(server, &pid) && pid == pi.dwProcessId, "[other process] the server sees the client's process id");
            }
            else
                check(0, "[other process] connected in time");
            if (WaitForSingleObject(pi.hProcess, 30000) != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 98);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        check(code == 0, "[other process] the client saw the server's process id and its own");
        CloseHandle(ov.hEvent);
        DisconnectNamedPipe(server);
    }
    CloseHandle(server);

    /* a server that listens has no client */
    server = make_server(FALSE);
    SetLastError(0xdeadbeef);
    ret = pClientPid(server, &pid);
    checkerr(ret, GetLastError(), ERROR_NOT_FOUND, "a listening server has no client (ERROR_NOT_FOUND)");
    pid = 0;
    check(pServerPid(server, &pid) && pid == cur, "...but still names its own process");
    CloseHandle(server);

    /* disconnected */
    client = connect_pair(&server);
    DisconnectNamedPipe(server);
    SetLastError(0xdeadbeef);
    ret = pClientPid(server, &pid);
    checkerr(ret, GetLastError(), ERROR_NOT_FOUND, "disconnected: the server has no client (ERROR_NOT_FOUND)");
    pid = 0;
    check(pServerPid(server, &pid) && pid == cur, "disconnected: the server still names its process");
    SetLastError(0xdeadbeef);
    ret = pClientPid(client, &pid);
    checkerr(ret, GetLastError(), ERROR_PIPE_NOT_CONNECTED, "disconnected: the client asking for the client id is ERROR_PIPE_NOT_CONNECTED");
    SetLastError(0xdeadbeef);
    ret = pServerPid(client, &pid);
    checkerr(ret, GetLastError(), ERROR_PIPE_NOT_CONNECTED, "disconnected: the client asking for the server id too");
    CloseHandle(client);
    CloseHandle(server);

    /* closed ends: the other end still answers */
    client = connect_pair(&server);
    CloseHandle(client);
    pid = 0;
    check(pClientPid(server, &pid) && pid == cur && pServerPid(server, &pid) && pid == cur, "client closed: the server end still answers both");
    CloseHandle(server);
    client = connect_pair(&server);
    CloseHandle(server);
    pid = 0;
    check(pClientPid(client, &pid) && pid == cur && pServerPid(client, &pid) && pid == cur, "server closed: the client end still answers both");
    CloseHandle(client);

    (void)s2;
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
