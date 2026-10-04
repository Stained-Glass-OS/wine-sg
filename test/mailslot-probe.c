/* mailslot-gate.sh's probe: a mailslot as programs use it */
#include <windows.h>
#include <stdio.h>
int main(void)
{
    HANDLE ms, w, port; DWORD n, next, count, timeout; char buf[64]; OVERLAPPED ov = { 0 }, *pov; ULONG_PTR key; BOOL ok;
    ms = CreateMailslotA("\\\\.\\mailslot\\sg_gate_ms", 0, 200, NULL);
    if (ms == INVALID_HANDLE_VALUE) { printf("CREATE %lu\n", GetLastError()); return 1; }
    w = CreateFileA("\\\\.\\mailslot\\sg_gate_ms", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    /* a blocking read of a message already there */
    WriteFile(w, "hello", 5, &n, NULL);
    ok = ReadFile(ms, buf, sizeof(buf), &n, NULL);
    printf("READ %d %.*s\n", ok, ok ? (int)n : 0, buf);
    /* the read timeout (200 ms) with nothing there */
    n = GetTickCount(); ok = ReadFile(ms, buf, sizeof(buf), &count, NULL);
    printf("TIMEOUT %d %lu %s\n", ok, GetLastError(), GetTickCount() - n >= 150 ? "waited" : "early");
    ok = GetMailslotInfo(ms, NULL, &next, &count, &timeout);
    printf("INFO %d %lu %lu %lu\n", ok, next == MAILSLOT_NO_MESSAGE ? 0 : next, count, timeout);
    /* a completion port, as Opera's installer gives it */
    port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    printf("PORT %s %lu\n", CreateIoCompletionPort(ms, port, 77, 1) ? "yes" : "no", GetLastError());
    ov.hEvent = NULL;
    ok = ReadFile(ms, buf, sizeof(buf), NULL, &ov);
    printf("OVREAD %d %lu\n", ok, ok ? 0 : GetLastError());
    WriteFile(w, "world", 5, &n, NULL);
    ok = GetQueuedCompletionStatus(port, &n, &key, &pov, 2000);
    printf("COMPLETED %d %lu %lu %.*s\n", ok, (unsigned long)key, n, ok ? (int)n : 0, buf);
    printf("DONE\n");
    return 0;
}
