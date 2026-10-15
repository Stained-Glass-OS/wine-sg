/* Probe for patches/sg/5002: NtQueryInformationProcess(ProcessSubsystemInformation)
 * reports SubsystemInformationTypeWin32 (0) for this and another process, checks
 * the length and the handle. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define ProcessSubsystemInformation 75

int main(int argc, char **argv)
{
    NTSTATUS (WINAPI *query)(HANDLE, int, void *, ULONG, ULONG *) =
            (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    char cmd[] = "cmd.exe /c exit";
    ULONG value, len;
    NTSTATUS status;
    HANDLE other = NULL;

    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) other = pi.hProcess;
    {
        struct { HANDLE h; ULONG size; NTSTATUS want; ULONG value; } cases[] =
        {
            {GetCurrentProcess(), sizeof(ULONG), STATUS_SUCCESS, 0},
            {GetCurrentProcess(), sizeof(ULONG) + 4, STATUS_INFO_LENGTH_MISMATCH, 0xdead},
            {GetCurrentProcess(), 2, STATUS_INFO_LENGTH_MISMATCH, 0xdead},
            {(HANDLE)0xdead0, sizeof(ULONG), STATUS_INVALID_HANDLE, 0xdead},
            {other, sizeof(ULONG), STATUS_SUCCESS, 0},
        };
        unsigned int i;
        for (i = 0; i < ARRAYSIZE(cases); i++)
        {
            if (!cases[i].h) continue;
            value = 0xdead; len = 0xbeef;
            status = query(cases[i].h, ProcessSubsystemInformation, &value, cases[i].size, &len);
            check(status == cases[i].want && value == cases[i].value && (status == STATUS_INVALID_HANDLE || len == sizeof(ULONG)),
                  "case %u: %#lx value %lu len %lu", i, status, value, len);
        }
    }
    if (other) { TerminateProcess(other, 0); CloseHandle(pi.hThread); CloseHandle(other); }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
