/* Probe for patches/sg/5070: the registry quota.
 * - SystemRegistryQuotaInformation's RegistryQuotaUsed is the size of the registry, here the
 *   bytes of the prefix's system.reg, user.reg and userdef.reg (the gate passes their sum in
 *   SG_REG_BYTES, taken after the server saved them); the quota allowed is at least that
 *   and at least 32 MB;
 * - GetSystemRegistryQuota reports the same two numbers (it made up its own), and takes
 *   NULL for either (kernel32 process tests).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { ULONG RegistryQuotaAllowed; ULONG RegistryQuotaUsed; PVOID Reserved1; } quota_info;
typedef NTSTATUS (WINAPI *qsi_t)(int, void *, ULONG, ULONG *);
typedef BOOL (WINAPI *grq_t)(DWORD *, DWORD *);

int main(void)
{
    qsi_t pNtQuerySystemInformation = (qsi_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    grq_t pGetSystemRegistryQuota = (grq_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetSystemRegistryQuota");
    const char *bytes = getenv("SG_REG_BYTES");
    unsigned long expect = bytes ? strtoul(bytes, NULL, 10) : 0;
    DWORD allowed = 0, used = 0;
    quota_info q;
    NTSTATUS st;
    ULONG len;
    BOOL ret;

    st = pNtQuerySystemInformation(37 /* SystemRegistryQuotaInformation */, &q, sizeof(q), &len);
    check(st == 0 && len == sizeof(q), "NtQuerySystemInformation %#lx", st);
    check(expect && q.RegistryQuotaUsed == expect, "used %lu, the registry files %lu", q.RegistryQuotaUsed, expect);
    check(q.RegistryQuotaAllowed >= q.RegistryQuotaUsed && q.RegistryQuotaAllowed >= 0x2000000, "allowed %lu", q.RegistryQuotaAllowed);

    ret = pGetSystemRegistryQuota(&allowed, &used);
    check(ret && allowed == q.RegistryQuotaAllowed && used == q.RegistryQuotaUsed,
          "GetSystemRegistryQuota %d: %lu %lu", ret, allowed, used);
    check(pGetSystemRegistryQuota(NULL, NULL) && pGetSystemRegistryQuota(&allowed, NULL) && pGetSystemRegistryQuota(NULL, &used),
          "NULL arguments");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
