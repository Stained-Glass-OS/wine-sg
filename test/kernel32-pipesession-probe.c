/* Probe for patches/sg/5011: GetNamedPipeClientSessionId/ServerSessionId give
 * the session of the process at that end and fail like the process id
 * functions when that end is not connected: a disconnected server has no
 * client (ERROR_NOT_FOUND), its client end has no pipe (ERROR_PIPE_NOT_CONNECTED).
 * The kernel32 pipe test runs the same cases on Windows.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define NAME "\\\\.\\pipe\\sg-pipesession"

int main(void)
{
    HANDLE server, client;
    DWORD current, id, err;
    unsigned int i;
    BOOL ret;
    struct { const char *what; HANDLE *h; BOOL client_side; BOOL ok; DWORD err; } tests[] =
    {
        {"server, client id", &server, TRUE, TRUE, 0},
        {"server, server id", &server, FALSE, TRUE, 0},
        {"client, client id", &client, TRUE, TRUE, 0},
        {"client, server id", &client, FALSE, TRUE, 0},
    };
    struct { const char *what; HANDLE *h; BOOL client_side; BOOL ok; DWORD err; } after[] =
    {
        {"disconnected server, client id", &server, TRUE, FALSE, ERROR_NOT_FOUND},
        {"disconnected server, server id", &server, FALSE, TRUE, 0},
        {"its client, client id", &client, TRUE, FALSE, ERROR_PIPE_NOT_CONNECTED},
        {"its client, server id", &client, FALSE, FALSE, ERROR_PIPE_NOT_CONNECTED},
    };

    ProcessIdToSessionId(GetCurrentProcessId(), &current);
    server = CreateNamedPipeA(NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 1024, 1024, 0, NULL);
    client = CreateFileA(NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    check(server != INVALID_HANDLE_VALUE && client != INVALID_HANDLE_VALUE, "pipe made");

    for (i = 0; i < 8; i++)
    {
        const char *what = i < 4 ? tests[i].what : after[i - 4].what;
        HANDLE h = i < 4 ? *tests[i].h : *after[i - 4].h;
        BOOL client_side = i < 4 ? tests[i].client_side : after[i - 4].client_side;
        BOOL want = i < 4 ? tests[i].ok : after[i - 4].ok;
        DWORD want_err = i < 4 ? tests[i].err : after[i - 4].err;

        if (i == 4) DisconnectNamedPipe(server);
        id = 0x55;
        SetLastError(0xdeadbeef);
        ret = client_side ? GetNamedPipeClientSessionId(h, &id) : GetNamedPipeServerSessionId(h, &id);
        err = GetLastError();
        if (want) check(ret && id == current, "%s: %d id %lu (session %lu)", what, ret, id, current);
        else check(!ret && err == want_err, "%s: %d error %lu (want %lu)", what, ret, err, want_err);
    }
    SetLastError(0xdeadbeef);
    ret = GetNamedPipeServerSessionId(server, NULL);
    check(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "NULL id: %lu", GetLastError());
    CloseHandle(client);
    CloseHandle(server);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
