/* NtSetInformationFile leaves the status block alone when the request is refused
 * (an error status), as Windows does, and writes it when the request goes through
 * (patches/sg/2217).  The conformance test dlls/ntdll/tests/file.c records the
 * same.  64-bit and 32-bit (wow64) probes. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG NTSTATUS;
typedef struct { union { NTSTATUS Status; PVOID Pointer; }; ULONG_PTR Information; } IOSB;
typedef struct { BOOLEAN ReplaceIfExists; HANDLE RootDirectory; ULONG FileNameLength; WCHAR FileName[1]; } RENAMEINFO;

static NTSTATUS (WINAPI *pNtSetInformationFile)(HANDLE, IOSB *, void *, ULONG, ULONG);

#define STATUS_OBJECT_NAME_COLLISION_ ((NTSTATUS)0xC0000035)
#define STATUS_NT_ERROR(s) (((ULONG)(s) >> 30) == 3)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE open_rw(const char *name)
{
    return CreateFileA(name, GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, CREATE_ALWAYS, 0, NULL);
}

int main(void)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    HANDLE a, b;
    IOSB io;
    NTSTATUS st;
    WCHAR full[MAX_PATH];
    struct { RENAMEINFO ri; WCHAR more[MAX_PATH]; } rename_buf;
    ULONG len;
    char pos[8];
    LARGE_INTEGER position;

    pNtSetInformationFile = (void *)GetProcAddress(nt, "NtSetInformationFile");
    DeleteFileA("sgiosb_a.tmp");
    DeleteFileA("sgiosb_b.tmp");
    a = open_rw("sgiosb_a.tmp");
    b = open_rw("sgiosb_b.tmp");
    if (a == INVALID_HANDLE_VALUE || b == INVALID_HANDLE_VALUE) { printf("FAIL  cannot create files\n"); return 1; }
    CloseHandle(b);

    /* rename onto an existing file without ReplaceIfExists */
    GetFullPathNameW(L"sgiosb_b.tmp", MAX_PATH, full + 4, NULL);
    memcpy(full, L"\\??\\", 4 * sizeof(WCHAR));
    memset(&rename_buf, 0, sizeof(rename_buf));
    rename_buf.ri.ReplaceIfExists = FALSE;
    len = lstrlenW(full) * sizeof(WCHAR);
    rename_buf.ri.FileNameLength = len;
    memcpy(rename_buf.ri.FileName, full, len);
    io.Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    st = pNtSetInformationFile(a, &io, &rename_buf, FIELD_OFFSET(RENAMEINFO, FileName) + len, 10 /* FileRenameInformation */);
    check(st == STATUS_OBJECT_NAME_COLLISION_, "renaming onto an existing file is STATUS_OBJECT_NAME_COLLISION");
    check((ULONG)io.Status == 0xdeadbeef && io.Information == 0xdeadbeef, "...and the status block is left alone");

    /* an information class that does not exist */
    io.Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    st = pNtSetInformationFile(a, &io, pos, sizeof(pos), 250);
    check(STATUS_NT_ERROR(st), "an unknown information class is an error");
    check((ULONG)io.Status == 0xdeadbeef && io.Information == 0xdeadbeef, "...and the status block is left alone");

    /* a buffer too short for the class */
    io.Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    st = pNtSetInformationFile(a, &io, pos, 1, 4 /* FileBasicInformation */);
    check(STATUS_NT_ERROR(st), "a buffer too short for FileBasicInformation is an error");
    check((ULONG)io.Status == 0xdeadbeef && io.Information == 0xdeadbeef, "...and the status block is left alone");
    io.Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    st = pNtSetInformationFile(a, &io, pos, 1, 14 /* FilePositionInformation */);
    check(STATUS_NT_ERROR(st), "...also for FilePositionInformation");
    check((ULONG)io.Status == 0xdeadbeef && io.Information == 0xdeadbeef, "...and the status block is left alone");

    /* a handle that is not a file */
    io.Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    st = pNtSetInformationFile((HANDLE)0xdeadbeef, &io, &position, sizeof(position), 14);
    check(STATUS_NT_ERROR(st), "an invalid handle is an error");
    check((ULONG)io.Status == 0xdeadbeef && io.Information == 0xdeadbeef, "...and the status block is left alone");

    /* a request that goes through writes the block */
    position.QuadPart = 5;
    io.Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    st = pNtSetInformationFile(a, &io, &position, sizeof(position), 14);
    check(st == 0, "setting the position works");
    check(io.Status == 0 && io.Information == 0, "...and writes the status block (status 0, information 0)");

    CloseHandle(a);
    DeleteFileA("sgiosb_a.tmp");
    DeleteFileA("sgiosb_b.tmp");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
