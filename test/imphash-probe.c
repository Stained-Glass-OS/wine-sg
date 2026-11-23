/* wine-sg 9702: RtlComputeImportTableHash (ntdll; Dependencies imports it statically). Prints the hash of
 * its own file (the gate checks it against the import hash it computes from the file itself) and checks
 * the error cases. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
typedef NTSTATUS (WINAPI *hash_t)(HANDLE, char *, ULONG);

int main(int argc, char **argv)
{
    hash_t fn = (hash_t)GetProcAddress(GetModuleHandleA("ntdll"), "RtlComputeImportTableHash");
    char path[MAX_PATH], tmp[MAX_PATH], hash[16], hash2[16];
    HANDLE f;
    NTSTATUS st;
    DWORD n;
    int i;

    CHECK(fn != NULL, "ntdll exports RtlComputeImportTableHash");
    if (!fn) { printf("RESULT: FAIL\n"); return 1; }
    GetModuleFileNameA(NULL, path, sizeof(path));
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    memset(hash, 0xcc, sizeof(hash));
    st = fn(f, hash, 1);
    CHECK(st == 0, "its own file, revision 1: status %#lx", st);
    printf("HASH ");
    for (i = 0; i < 16; i++) printf("%02x", (unsigned char)hash[i]);
    printf("\n");
    memset(hash2, 0, sizeof(hash2));
    st = fn(f, hash2, 1);
    CHECK(st == 0 && !memcmp(hash, hash2, 16), "the same hash again");
    st = fn(f, hash2, 2);
    CHECK(st == (NTSTATUS)0xC0000058, "revision 2: STATUS_UNKNOWN_REVISION (got %#lx)", st);
    CloseHandle(f);

    GetTempPathA(sizeof(tmp), tmp); strcat(tmp, "sg-imphash.txt");
    f = CreateFileA(tmp, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    WriteFile(f, "not a PE file at all, just some text that is long enough", 56, &n, NULL);
    st = fn(f, hash2, 1);
    CHECK(st == (NTSTATUS)0xC000007B, "a text file: STATUS_INVALID_IMAGE_FORMAT (got %#lx)", st);
    CloseHandle(f);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
