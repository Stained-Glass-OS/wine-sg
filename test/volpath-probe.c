/* volpath gate probe (wine-sg 8401): files below a volume device by their NT names. Prints
 * KEY=1 for each check that holds (KEY=0 otherwise), then done=1. */
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>
#include <stdio.h>

NTSTATUS WINAPI NtCreateFile(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER,
                             ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);

static NTSTATUS nt_open(const WCHAR *path, ACCESS_MASK access, ULONG disposition, ULONG options, HANDLE *ret)
{
    UNICODE_STRING str;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;

    RtlInitUnicodeString(&str, path);
    InitializeObjectAttributes(&attr, &str, OBJ_CASE_INSENSITIVE, NULL, NULL);
    return NtCreateFile(ret, access | SYNCHRONIZE, &attr, &io, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        disposition, options | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
}

static void check(const char *key, BOOL ok, const char *why)
{
    printf("%s=%d%s%s\n", key, ok ? 1 : 0, ok ? "" : " ", ok ? "" : why);
}

int main(void)
{
    WCHAR vol[MAX_PATH], path[MAX_PATH * 2], final[MAX_PATH * 2];
    char buf[64], msg[256];
    DWORD got = 0;
    NTSTATUS st;
    HANDLE h;
    BOOL ok;

    if (!QueryDosDeviceW(L"C:", vol, MAX_PATH)) { printf("volume=0 QueryDosDevice %lu\ndone=1\n", GetLastError()); return 0; }
    printf("volume: %ls\n", vol);

    /* an existing file */
    swprintf(path, ARRAYSIZE(path), L"%ls\\windows\\win.ini", vol);
    st = nt_open(path, GENERIC_READ, FILE_OPEN, FILE_NON_DIRECTORY_FILE, &h);
    snprintf(msg, sizeof(msg), "(%08lx)", st);
    check("open", !st, msg);
    if (!st) CloseHandle(h);

    /* a directory, then a new folder and a new file in it, read back by its DOS name */
    swprintf(path, ARRAYSIZE(path), L"%ls\\windows", vol);
    st = nt_open(path, FILE_LIST_DIRECTORY, FILE_OPEN, FILE_DIRECTORY_FILE, &h);
    snprintf(msg, sizeof(msg), "(%08lx)", st);
    check("opendir", !st, msg);
    if (!st) CloseHandle(h);
    RemoveDirectoryW(L"C:\\volpath\\new");
    DeleteFileW(L"C:\\volpath\\new\\settings.json");
    swprintf(path, ARRAYSIZE(path), L"%ls\\volpath", vol);
    st = nt_open(path, FILE_LIST_DIRECTORY, FILE_OPEN_IF, FILE_DIRECTORY_FILE, &h);
    if (!st) CloseHandle(h);
    swprintf(path, ARRAYSIZE(path), L"%ls\\volpath\\new", vol);
    st = nt_open(path, FILE_LIST_DIRECTORY, FILE_CREATE, FILE_DIRECTORY_FILE, &h);
    snprintf(msg, sizeof(msg), "(%08lx)", st);
    check("mkdir", !st && GetFileAttributesW(L"C:\\volpath\\new") != INVALID_FILE_ATTRIBUTES, msg);
    if (!st) CloseHandle(h);
    swprintf(path, ARRAYSIZE(path), L"%ls\\volpath\\new\\settings.json", vol);
    st = nt_open(path, GENERIC_WRITE, FILE_OVERWRITE_IF, FILE_NON_DIRECTORY_FILE, &h);
    ok = !st && WriteFile(h, "{\"a\":1}", 7, &got, NULL) && got == 7;
    if (!st) CloseHandle(h);
    h = CreateFileW(L"C:\\volpath\\new\\settings.json", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    memset(buf, 0, sizeof(buf));
    got = 0;
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, sizeof(buf) - 1, &got, NULL); CloseHandle(h); }
    snprintf(msg, sizeof(msg), "(%08lx, read '%s')", st, buf);
    check("create", ok && !strcmp(buf, "{\"a\":1}"), msg);

    /* the NT name Wine itself gives an open file opens that file */
    h = CreateFileW(L"C:\\windows\\win.ini", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    final[0] = 0;
    if (h != INVALID_HANDLE_VALUE)
    {
        GetFinalPathNameByHandleW(h, final, ARRAYSIZE(final), VOLUME_NAME_NT);
        CloseHandle(h);
    }
    st = final[0] ? nt_open(final, GENERIC_READ, FILE_OPEN, FILE_NON_DIRECTORY_FILE, &h) : STATUS_UNSUCCESSFUL;
    snprintf(msg, sizeof(msg), "(%ls: %08lx)", final, st);
    check("finalname", !st, msg);
    if (!st) CloseHandle(h);

    /* \\?\GLOBALROOT\Device\... through CreateFile */
    swprintf(path, ARRAYSIZE(path), L"\\\\?\\GLOBALROOT%ls\\windows\\win.ini", vol);
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    snprintf(msg, sizeof(msg), "(error %lu)", GetLastError());
    check("globalroot", h != INVALID_HANDLE_VALUE, msg);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);

    /* a missing file below the volume is not found; a volume no drive names is not taken for one */
    swprintf(path, ARRAYSIZE(path), L"%ls\\windows\\no-such-file.ini", vol);
    st = nt_open(path, GENERIC_READ, FILE_OPEN, 0, &h);
    snprintf(msg, sizeof(msg), "(%08lx)", st);
    check("missing", st == STATUS_OBJECT_NAME_NOT_FOUND, msg);
    if (!st) CloseHandle(h);
    st = nt_open(L"\\Device\\HarddiskVolume77\\windows\\win.ini", GENERIC_READ, FILE_OPEN, 0, &h);
    snprintf(msg, sizeof(msg), "(%08lx)", st);
    check("othervolume", st != STATUS_SUCCESS, msg);
    if (!st) CloseHandle(h);

    DeleteFileW(L"C:\\volpath\\new\\settings.json");
    RemoveDirectoryW(L"C:\\volpath\\new");
    RemoveDirectoryW(L"C:\\volpath");
    printf("done=1\n");
    return 0;
}
