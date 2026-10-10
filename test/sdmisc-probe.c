/* IsValidRelativeSecurityDescriptor, Get/SetSecurityDescriptorRMControl and
 * GetNamedPipeClientComputerName (patches/sg/2241). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static BOOL (WINAPI *pIsValidRelative)(PSECURITY_DESCRIPTOR, ULONG, SECURITY_INFORMATION);
static DWORD (WINAPI *pGetRM)(PSECURITY_DESCRIPTOR, PUCHAR);
static DWORD (WINAPI *pSetRM)(PSECURITY_DESCRIPTOR, PUCHAR);
static BOOL (WINAPI *pPipeComputerW)(HANDLE, WCHAR *, ULONG);
static BOOL (WINAPI *pPipeComputerA)(HANDLE, char *, ULONG);

int main(void)
{
    HMODULE advapi = LoadLibraryA("advapi32.dll"), kernel = GetModuleHandleA("kernel32.dll");
    SECURITY_DESCRIPTOR abs;
    BYTE rel_buf[512], copy[512];
    SECURITY_DESCRIPTOR_RELATIVE *rel = (void *)copy;
    BYTE sid_buf[SECURITY_MAX_SID_SIZE], acl_buf[256];
    DWORD sid_size = sizeof(sid_buf), size = sizeof(rel_buf);
    ACL *acl = (ACL *)acl_buf;
    PSID world = (PSID)sid_buf;
    UCHAR rm;
    HANDLE server, client;
    WCHAR name[32], computer[32];
    char nameA[32];
    DWORD csize = 32, ret;
    BOOL ok;

    pIsValidRelative = (void *)GetProcAddress(advapi, "IsValidRelativeSecurityDescriptor");
    pGetRM = (void *)GetProcAddress(advapi, "GetSecurityDescriptorRMControl");
    pSetRM = (void *)GetProcAddress(advapi, "SetSecurityDescriptorRMControl");
    pPipeComputerW = (void *)GetProcAddress(kernel, "GetNamedPipeClientComputerNameW");
    pPipeComputerA = (void *)GetProcAddress(kernel, "GetNamedPipeClientComputerNameA");
    CHECK(pIsValidRelative && pGetRM && pSetRM && pPipeComputerW && pPipeComputerA, "exports present");
    if (!(pIsValidRelative && pGetRM && pSetRM && pPipeComputerW && pPipeComputerA)) { printf("RESULT: FAIL\n"); return 1; }

    CreateWellKnownSid(WinWorldSid, NULL, world, &sid_size);
    InitializeAcl(acl, sizeof(acl_buf), ACL_REVISION);
    AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, world);
    InitializeSecurityDescriptor(&abs, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&abs, world, FALSE);
    SetSecurityDescriptorGroup(&abs, world, FALSE);
    SetSecurityDescriptorDacl(&abs, TRUE, acl, FALSE);
    ok = MakeSelfRelativeSD(&abs, rel_buf, &size);
    CHECK(ok, "MakeSelfRelativeSD %lu", GetLastError());
    memcpy(copy, rel_buf, size);

    CHECK(pIsValidRelative(copy, size, 0), "valid descriptor");
    CHECK(pIsValidRelative(copy, size, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION),
          "valid, all that is there required");
    CHECK(!pIsValidRelative(copy, size, SACL_SECURITY_INFORMATION), "SACL required but absent");
    {
        DWORD end = 0, e;
        rel = (void *)copy;
        e = rel->Owner + GetLengthSid(world); if (e > end) end = e;
        e = rel->Group + GetLengthSid(world); if (e > end) end = e;
        e = rel->Dacl + acl->AclSize; if (e > end) end = e;   /* AclSize as stored: equals acl_buf's */
        CHECK(pIsValidRelative(copy, end, 0), "exactly as long as its parts (%lu of %lu)", end, size);
        CHECK(!pIsValidRelative(copy, end - 1, 0), "one byte short of its last part");
    }
    CHECK(!pIsValidRelative(copy, 19, 0), "shorter than the header");
    CHECK(!pIsValidRelative(NULL, size, 0), "NULL");

    memcpy(copy, rel_buf, size);
    rel->Revision = 2;
    CHECK(!pIsValidRelative(copy, size, 0), "revision 2");
    memcpy(copy, rel_buf, size);
    rel->Control &= ~SE_SELF_RELATIVE;
    CHECK(!pIsValidRelative(copy, size, 0), "not self-relative");
    memcpy(copy, rel_buf, size);
    rel->Owner += 1;
    CHECK(!pIsValidRelative(copy, size, 0), "owner offset not aligned");
    memcpy(copy, rel_buf, size);
    rel->Owner = size + 4;
    CHECK(!pIsValidRelative(copy, size, 0), "owner offset past the end");
    memcpy(copy, rel_buf, size);
    rel->Dacl = size - 2;
    CHECK(!pIsValidRelative(copy, size, 0), "dacl offset near the end");
    memcpy(copy, rel_buf, size);
    rel->Owner = 0;
    CHECK(pIsValidRelative(copy, size, 0), "no owner is fine when not required");
    CHECK(!pIsValidRelative(copy, size, OWNER_SECURITY_INFORMATION), "owner required but absent");
    memcpy(copy, rel_buf, size);
    rel->Dacl = 0;   /* NULL DACL, present */
    CHECK(pIsValidRelative(copy, size, DACL_SECURITY_INFORMATION), "NULL DACL counts as present");

    /* RM control */
    InitializeSecurityDescriptor(&abs, SECURITY_DESCRIPTOR_REVISION);
    rm = 0xee;
    ret = pGetRM(&abs, &rm);
    CHECK(ret == ERROR_INVALID_DATA && rm == 0xee, "no RM control yet: %lu", ret);
    rm = 0x5a;
    ret = pSetRM(&abs, &rm);
    CHECK(ret == ERROR_SUCCESS && (abs.Control & SE_RM_CONTROL_VALID), "set: %lu control %#x", ret, abs.Control);
    rm = 0;
    ret = pGetRM(&abs, &rm);
    CHECK(ret == ERROR_SUCCESS && rm == 0x5a, "get back %#x (%lu)", rm, ret);
    ret = pSetRM(&abs, NULL);
    CHECK(ret == ERROR_SUCCESS && !(abs.Control & SE_RM_CONTROL_VALID) && !abs.Sbz1, "cleared");
    CHECK(pGetRM(&abs, &rm) == ERROR_INVALID_DATA, "invalid again");

    /* the pipe's client */
    server = CreateNamedPipeW(L"\\\\.\\pipe\\sg-sdmisc", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    CHECK(server != INVALID_HANDLE_VALUE, "pipe %lu", GetLastError());
    client = CreateFileW(L"\\\\.\\pipe\\sg-sdmisc", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(client != INVALID_HANDLE_VALUE, "client %lu", GetLastError());
    GetComputerNameW(computer, &csize);
    memset(name, 0, sizeof(name));
    ok = pPipeComputerW(server, name, 32);
    CHECK(ok && !wcscmp(name, computer), "client computer %ls / %ls (%lu)", name, computer, GetLastError());
    SetLastError(0);
    ok = pPipeComputerW(server, name, 1);
    CHECK(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "short buffer: %d %lu", ok, GetLastError());
    memset(nameA, 0, sizeof(nameA));
    ok = pPipeComputerA(server, nameA, 32);
    CHECK(ok && strlen(nameA) == wcslen(computer), "ANSI name %s", nameA);
    SetLastError(0);
    ok = pPipeComputerW((HANDLE)(ULONG_PTR)0xdeadbeef, name, 32);
    CHECK(!ok && GetLastError() == ERROR_INVALID_HANDLE, "bad handle: %d %lu", ok, GetLastError());
    SetLastError(0);
    ok = pPipeComputerW(GetCurrentProcess(), name, 32);
    CHECK(!ok, "not a pipe");
    CloseHandle(client);
    CloseHandle(server);

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
