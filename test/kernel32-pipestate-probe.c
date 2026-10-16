/* Probe for patches/sg/5012: GetNamedPipeHandleState fails
 * ERROR_INVALID_PARAMETER when asked for the collection count or timeout of a
 * local pipe (they exist only for the client end of a remote pipe; the kernel32
 * pipe test checks this on Windows); the other values still come back.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define NAME "\\\\.\\pipe\\sg-pipestate"

int main(void)
{
    HANDLE server, client;
    DWORD state, instances, count, timeout, err;
    unsigned int i, j;
    BOOL ret;

    server = CreateNamedPipeA(NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 1024, 1024, 0, NULL);
    client = CreateFileA(NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    check(server != INVALID_HANDLE_VALUE && client != INVALID_HANDLE_VALUE, "pipe made");

    for (j = 0; j < 2; j++)
        for (i = 0; i < 4; i++)
        {
            HANDLE h = j ? client : server;
            DWORD *c = (i & 1) ? &count : NULL, *t = (i & 2) ? &timeout : NULL;

            state = instances = 0x55;
            SetLastError(0xdeadbeef);
            ret = (i + j) & 1 ? GetNamedPipeHandleStateA(h, &state, &instances, c, t, NULL, 0)
                              : GetNamedPipeHandleStateW(h, &state, &instances, c, t, NULL, 0);
            err = GetLastError();
            if (!i) check(ret && state == 0 && instances == 1, "%s, no collection args: %d state %lu instances %lu",
                          j ? "client" : "server", ret, state, instances);
            else check(!ret && err == ERROR_INVALID_PARAMETER, "%s, count %d timeout %d: %d %lu",
                       j ? "client" : "server", !!c, !!t, ret, err);
        }
    CloseHandle(client);
    CloseHandle(server);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
