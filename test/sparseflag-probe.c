/* sparseflag-probe: a file's sparse flag (FSCTL_SET_SPARSE), for test/sparseflag-gate.sh
 * (Stained Glass OS wine-sg 9810). Prints one "NAME value" line per check. */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

static DWORD sparse_of(const WCHAR *name)
{
    DWORD a = GetFileAttributesW(name);
    return a == INVALID_FILE_ATTRIBUTES ? 0xffffffff : (a & (FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_HIDDEN));
}

static DWORD set_sparse(HANDLE h, int with_buffer, BOOLEAN value)
{
    struct { BOOLEAN SetSparse; } buf = { value };
    DWORD ret;
    if (DeviceIoControl(h, FSCTL_SET_SPARSE, with_buffer ? &buf : NULL, with_buffer ? sizeof(buf) : 0, NULL, 0, &ret, NULL))
        return 0;
    return GetLastError();
}

int main(void)
{
    static char data[65536];
    const WCHAR *f = L"C:\\sp\\f.bin", *g = L"C:\\sp\\g.bin";
    WIN32_FIND_DATAW fd;
    HANDLE h, find;
    DWORD n;
    int i;

    CreateDirectoryW(L"C:\\sp", NULL);
    memset(data, 'x', sizeof(data));
    h = CreateFileW(f, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    for (i = 0; i < 4; i++) WriteFile(h, data, sizeof(data), &n, NULL);
    printf("INIT %lx\n", sparse_of(f));
    printf("SETCALL %lu\n", set_sparse(h, 0, TRUE));
    printf("SET %lx\n", sparse_of(f));
    CloseHandle(h);

    SetFileAttributesW(f, FILE_ATTRIBUTE_NORMAL);
    printf("KEEP %lx\n", sparse_of(f));
    SetFileAttributesW(f, FILE_ATTRIBUTE_HIDDEN);
    printf("HIDDEN %lx\n", sparse_of(f));
    find = FindFirstFileW(f, &fd);
    printf("FIND %lx\n", find == INVALID_HANDLE_VALUE ? 0xffffffff : fd.dwFileAttributes & (FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_HIDDEN));
    if (find != INVALID_HANDLE_VALUE) FindClose(find);

    h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    printf("READONLY %lu\n", set_sparse(h, 1, FALSE));
    CloseHandle(h);
    printf("STILL %lx\n", sparse_of(f));

    h = CreateFileW(f, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    printf("CLEARCALL %lu\n", set_sparse(h, 1, FALSE));
    CloseHandle(h);
    printf("CLEAR %lx\n", sparse_of(f));

    h = CreateFileW(g, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_SPARSE_FILE, NULL);
    CloseHandle(h);
    printf("CREATE %lx\n", sparse_of(g));
    return 0;
}
