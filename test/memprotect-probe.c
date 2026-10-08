/* CryptProtectMemory and RtlEncryptMemory (patches/sg/1603). They did
 * nothing: "protected" secrets stayed in clear in memory.
 *
 *  - protecting changes the memory, unprotecting restores it, per scope;
 *  - a length not a multiple of the block size is refused
 *    (ERROR_INVALID_PARAMETER; RtlEncryptMemory: 8-byte blocks), and so
 *    is an unknown scope;
 *  - CRYPTPROTECTMEMORY_CROSS_PROCESS and _SAME_LOGON memory is decrypted
 *    by another process; _SAME_PROCESS memory is not.
 */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *rtl_crypt_t)(void *, ULONG, ULONG);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static const char secret[32] = "correct horse battery staple!!";

static void to_hex(const BYTE *data, int len, char *out)
{
    int i;
    for (i = 0; i < len; i++) sprintf(out + 2 * i, "%02x", data[i]);
}

static void from_hex(const char *in, BYTE *data, int len)
{
    int i;
    unsigned int v;
    for (i = 0; i < len; i++) { sscanf(in + 2 * i, "%2x", &v); data[i] = v; }
}

/* decrypt in a child process; TRUE when it gets the secret back */
static BOOL child_decrypts(const char *exe, DWORD scope, const BYTE *cipher)
{
    char cmd[512], hex[65], out[128] = {0};
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HANDLE rd, wr;
    DWORD got = 0;

    to_hex(cipher, 32, hex);
    snprintf(cmd, sizeof(cmd), "\"%s\" child %lu %s", exe, scope, hex);
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return FALSE;
    CloseHandle(wr);
    WaitForSingleObject(pi.hProcess, 30000);
    ReadFile(rd, out, sizeof(out) - 1, &got, NULL);
    CloseHandle(rd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    printf("child (scope %lu) says: %s\n", scope, out);
    return !strncmp(out, "SECRET", 6);
}

int main(int argc, char **argv)
{
    rtl_crypt_t pRtlEncryptMemory = (void *)GetProcAddress(LoadLibraryA("advapi32.dll"), "SystemFunction040");
    rtl_crypt_t pRtlDecryptMemory = (void *)GetProcAddress(LoadLibraryA("advapi32.dll"), "SystemFunction041");
    BYTE buf[32], keep[3][32], small[8];
    DWORD scopes[3] = { CRYPTPROTECTMEMORY_SAME_PROCESS, CRYPTPROTECTMEMORY_CROSS_PROCESS, CRYPTPROTECTMEMORY_SAME_LOGON };
    const char *names[3] = { "SAME_PROCESS", "CROSS_PROCESS", "SAME_LOGON" };
    char what[128];
    LONG status;
    BOOL ok;
    int i;

    if (argc > 3 && !strcmp(argv[1], "child"))
    {
        from_hex(argv[3], buf, 32);
        ok = CryptUnprotectMemory(buf, 32, atoi(argv[2]));
        printf("%s\n", ok && !memcmp(buf, secret, 32) ? "SECRET" : "garbage");
        return 0;
    }

    for (i = 0; i < 3; i++)
    {
        memcpy(buf, secret, 32);
        ok = CryptProtectMemory(buf, 32, scopes[i]);
        snprintf(what, sizeof(what), "%s: protecting changes the memory", names[i]);
        check(ok && memcmp(buf, secret, 32), what);
        memcpy(keep[i], buf, 32);
        ok = CryptUnprotectMemory(buf, 32, scopes[i]);
        snprintf(what, sizeof(what), "%s: unprotecting restores it", names[i]);
        check(ok && !memcmp(buf, secret, 32), what);
    }
    check(memcmp(keep[0], keep[1], 32) && memcmp(keep[1], keep[2], 32), "each scope has its own key");

    memcpy(buf, secret, 32);
    SetLastError(0xdeadbeef);
    ok = CryptProtectMemory(buf, 15, CRYPTPROTECTMEMORY_SAME_PROCESS);
    printf("15 bytes: ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "a length not a multiple of 16 is refused");
    check(!memcmp(buf, secret, 32), "... and the memory is left alone");
    SetLastError(0xdeadbeef);
    ok = CryptProtectMemory(buf, 32, 0x10);
    check(!ok, "an unknown scope is refused");

    memcpy(small, secret, 8);
    status = pRtlEncryptMemory(small, 8, 0);
    check(!status && memcmp(small, secret, 8), "RtlEncryptMemory encrypts 8 bytes");
    status = pRtlDecryptMemory(small, 8, 0);
    check(!status && !memcmp(small, secret, 8), "RtlDecryptMemory decrypts them");
    status = pRtlEncryptMemory(buf, 12, 0);
    check(status == (LONG)0xc000000d, "RtlEncryptMemory refuses 12 bytes (STATUS_INVALID_PARAMETER)");

    check(!child_decrypts(argv[0], CRYPTPROTECTMEMORY_SAME_PROCESS, keep[0]), "another process cannot decrypt SAME_PROCESS memory");
    check(child_decrypts(argv[0], CRYPTPROTECTMEMORY_CROSS_PROCESS, keep[1]), "another process decrypts CROSS_PROCESS memory");
    check(child_decrypts(argv[0], CRYPTPROTECTMEMORY_SAME_LOGON, keep[2]), "another process of the same logon decrypts SAME_LOGON memory");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
