/* volume attributes and holes: the FILE_SUPPORTS_* flags a volume reports, FSCTL_QUERY_ALLOCATED_RANGES
 * and FSCTL_SET_ZERO_DATA (patch 6235) */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); f++; } } while (0)
static int f;

typedef struct { LARGE_INTEGER off, len; } RANGE;
typedef struct { LARGE_INTEGER off, beyond; } ZERO;

int main(void)
{
    static BYTE buf[1 << 20];
    WCHAR fs[64];
    DWORD flags = 0, serial, maxlen, n, i, want;
    RANGE q, r[4];
    ZERO z;
    HANDLE h, ro;
    BOOL ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    ok = GetVolumeInformationW(L"C:\\", NULL, 0, &serial, &maxlen, &flags, fs, 64);
    printf("C: %ls flags %#lx\n", fs, flags);
    CHECK(ok && !lstrcmpW(fs, L"NTFS"));
    want = FILE_CASE_PRESERVED_NAMES | FILE_PERSISTENT_ACLS | FILE_UNICODE_ON_DISK | FILE_SUPPORTS_SPARSE_FILES
         | FILE_SUPPORTS_REPARSE_POINTS | FILE_SUPPORTS_OBJECT_IDS | FILE_SUPPORTS_HARD_LINKS | FILE_SUPPORTS_OPEN_BY_FILE_ID;
    CHECK((flags & want) == want);
    CHECK(!(flags & (FILE_NAMED_STREAMS | FILE_SUPPORTS_USN_JOURNAL | FILE_SUPPORTS_TRANSACTIONS | FILE_SUPPORTS_ENCRYPTION)));

    /* the same through a handle to a folder on the volume (GetVolumeInformationByHandleW) */
    h = CreateFileW(L"C:\\windows", 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, NULL);
    flags = 0;
    ok = GetVolumeInformationByHandleW(h, NULL, 0, NULL, NULL, &flags, fs, 64);
    printf("by handle: %ls flags %#lx\n", fs, flags);
    CHECK(ok && !lstrcmpW(fs, L"NTFS") && (flags & want) == want);
    CloseHandle(h);

    h = CreateFileW(L"C:\\holes.bin", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    for (i = 0; i < sizeof(buf); i++) buf[i] = (BYTE)(i % 251 + 1);
    CHECK(WriteFile(h, buf, sizeof(buf), &n, NULL) && n == sizeof(buf));
    CHECK(DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &n, NULL));

    /* zero the middle half: a hole there, the data around it kept, the size unchanged */
    z.off.QuadPart = 256 << 10; z.beyond.QuadPart = 768 << 10;
    CHECK(DeviceIoControl(h, FSCTL_SET_ZERO_DATA, &z, sizeof(z), NULL, 0, &n, NULL));
    z.off.QuadPart = (1 << 20) - 16; z.beyond.QuadPart = 4 << 20; /* past the end: zeroes the tail, no growth */
    CHECK(DeviceIoControl(h, FSCTL_SET_ZERO_DATA, &z, sizeof(z), NULL, 0, &n, NULL));
    CHECK(GetFileSize(h, NULL) == sizeof(buf));
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    CHECK(ReadFile(h, buf, sizeof(buf), &n, NULL) && n == sizeof(buf));
    for (i = 0; i < sizeof(buf); i++)
    {
        BYTE w = (i >= (256 << 10) && i < (768 << 10)) || i >= (1 << 20) - 16 ? 0 : (BYTE)(i % 251 + 1);
        if (buf[i] != w) { printf("byte %lu is %u\n", i, buf[i]); CHECK(!"content"); break; }
    }

    /* the allocated ranges skip the hole; the handle's position is kept */
    SetFilePointer(h, 100, NULL, FILE_BEGIN);
    q.off.QuadPart = 0; q.len.QuadPart = 1 << 20;
    memset(r, 0, sizeof(r));
    ok = DeviceIoControl(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, sizeof(q), r, sizeof(r), &n, NULL);
    printf("ranges %lu: %lld+%lld %lld+%lld\n", n / (DWORD)sizeof(RANGE), r[0].off.QuadPart, r[0].len.QuadPart,
           r[1].off.QuadPart, r[1].len.QuadPart);
    CHECK(ok && n == 2 * sizeof(RANGE));
    CHECK(r[0].off.QuadPart == 0 && r[0].len.QuadPart == 256 << 10);
    CHECK(r[1].off.QuadPart == 768 << 10 && r[1].off.QuadPart + r[1].len.QuadPart == 1 << 20);
    CHECK(SetFilePointer(h, 0, NULL, FILE_CURRENT) == 100);

    /* a window inside the hole has no ranges; one inside data is clipped to the window */
    q.off.QuadPart = 300 << 10; q.len.QuadPart = 100 << 10;
    ok = DeviceIoControl(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, sizeof(q), r, sizeof(r), &n, NULL);
    CHECK(ok && n == 0);
    q.off.QuadPart = 1000; q.len.QuadPart = 5000;
    ok = DeviceIoControl(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, sizeof(q), r, sizeof(r), &n, NULL);
    CHECK(ok && n == sizeof(RANGE) && r[0].off.QuadPart == 1000 && r[0].len.QuadPart == 5000);

    /* room for one range of two: more data; room for none: too small */
    q.off.QuadPart = 0; q.len.QuadPart = 1 << 20;
    ok = DeviceIoControl(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, sizeof(q), r, sizeof(RANGE), &n, NULL);
    CHECK(!ok && GetLastError() == ERROR_MORE_DATA && n == sizeof(RANGE));
    ok = DeviceIoControl(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, sizeof(q), r, 8, &n, NULL);
    CHECK(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    ok = DeviceIoControl(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, 8, r, sizeof(r), &n, NULL);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER);
    q.len.QuadPart = -1;
    ok = DeviceIoControl(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, sizeof(q), r, sizeof(r), &n, NULL);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER);

    /* zeroing needs write access */
    ro = CreateFileW(L"C:\\holes.bin", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(ro != INVALID_HANDLE_VALUE);
    z.off.QuadPart = 0; z.beyond.QuadPart = 10;
    ok = DeviceIoControl(ro, FSCTL_SET_ZERO_DATA, &z, sizeof(z), NULL, 0, &n, NULL);
    CHECK(!ok && GetLastError() == ERROR_ACCESS_DENIED);
    CloseHandle(ro);
    z.off.QuadPart = 10; z.beyond.QuadPart = 5;
    ok = DeviceIoControl(h, FSCTL_SET_ZERO_DATA, &z, sizeof(z), NULL, 0, &n, NULL);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER);
    CloseHandle(h);
    DeleteFileW(L"C:\\holes.bin");

    printf("RESULT: %s\n", f ? "FAIL" : "PASS");
    return f != 0;
}
