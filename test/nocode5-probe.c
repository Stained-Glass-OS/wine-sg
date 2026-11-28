/* test/nocode5-gate.sh: GetNamedPipeClientComputerNameW in kernelbase, its length in bytes
 * (patches/sg/8019). kernelbase's was a spec stub; kernel32's took the length in characters, so a
 * buffer of N bytes got up to 2N written. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) { printf("ok    "); printf(__VA_ARGS__); printf("\n"); } \
                           else { failures++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    BOOL (WINAPI *pBaseW)(HANDLE, WCHAR *, ULONG) = (void *)GetProcAddress(LoadLibraryA("kernelbase.dll"), "GetNamedPipeClientComputerNameW");
    BOOL (WINAPI *pW)(HANDLE, WCHAR *, ULONG) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNamedPipeClientComputerNameW");
    BOOL (WINAPI *pA)(HANDLE, char *, ULONG) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNamedPipeClientComputerNameA");
    WCHAR computer[MAX_COMPUTERNAME_LENGTH + 1], buf[64];
    char bufA[64];
    DWORD csize = ARRAYSIZE(computer), need;
    HANDLE server, client;
    BOOL ok;
    int i, guard;

    CHECK(pBaseW && pW && pA, "kernelbase and kernel32 export them");
    if (!pBaseW || !pW || !pA) { printf("RESULT: FAIL\n"); return 1; }
    GetComputerNameW(computer, &csize);
    need = (csize + 1) * sizeof(WCHAR);

    server = CreateNamedPipeW(L"\\\\.\\pipe\\sg-nocode5", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    client = CreateFileW(L"\\\\.\\pipe\\sg-nocode5", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(server != INVALID_HANDLE_VALUE && client != INVALID_HANDLE_VALUE, "a pipe and its client");

    memset(buf, 0x55, sizeof(buf));
    ok = pBaseW(server, buf, need);
    CHECK(ok && !wcscmp(buf, computer), "kernelbase: %ls in a buffer of %lu bytes", ok ? buf : L"", need);
    memset(buf, 0x55, sizeof(buf));
    SetLastError(0xdeadbeef);
    ok = pBaseW(server, buf, need - 1);
    CHECK(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "one byte short: %d %lu", ok, GetLastError());
    memset(buf, 0x55, sizeof(buf));
    SetLastError(0xdeadbeef);
    ok = pW(server, buf, csize + 1);
    for (i = 0, guard = 1; i < (int)ARRAYSIZE(buf); i++) if (buf[i] != 0x5555) guard = 0;
    CHECK(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER && guard,
          "kernel32: a length of characters (%lu) is too few bytes, nothing written: %d %lu", csize + 1, ok, GetLastError());
    ok = pW(server, buf, sizeof(buf));
    CHECK(ok && !wcscmp(buf, computer), "kernel32 with sizeof the buffer");
    memset(bufA, 0, sizeof(bufA));
    ok = pA(server, bufA, csize + 1);
    CHECK(ok && strlen(bufA) == csize, "ANSI: %s in %lu bytes", bufA, csize + 1);
    SetLastError(0xdeadbeef);
    ok = pBaseW(client, buf, sizeof(buf));
    CHECK(ok, "the client end too: %lu", ok ? 0 : GetLastError());
    SetLastError(0xdeadbeef);
    ok = pBaseW(GetCurrentProcess(), buf, sizeof(buf));
    CHECK(!ok, "not a pipe: %lu", GetLastError());
    CloseHandle(client);
    CloseHandle(server);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
