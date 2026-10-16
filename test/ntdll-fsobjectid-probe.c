/* Probe for patches/sg/5069: NtQueryVolumeInformationFile(FileFsObjectIdInformation).
 * The gate passes the UUID of the Unix file system under the prefix (findmnt) in
 * SG_FS_UUID; the volume's ObjectId is that UUID's bytes in the order they are written,
 * ExtendedInfo is zero, a handle to another file on the volume gives the same id.
 * With no UUID it fails with STATUS_INVALID_DEVICE_REQUEST. A buffer smaller than the
 * structure is STATUS_INFO_LENGTH_MISMATCH.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define FileFsObjectIdInformation_ 8
typedef NTSTATUS (WINAPI *qvi_t)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int);
typedef struct { UCHAR ObjectId[16]; UCHAR ExtendedInfo[48]; } objid_info;

static int parse(const char *text, UCHAR id[16])
{
    int n = 0, high = 1, d;
    memset(id, 0, 16);
    for (; *text && n < 16; text++)
    {
        if (*text >= '0' && *text <= '9') d = *text - '0';
        else if (*text >= 'a' && *text <= 'f') d = *text - 'a' + 10;
        else if (*text >= 'A' && *text <= 'F') d = *text - 'A' + 10;
        else continue;
        if (high) id[n] = d << 4; else id[n++] |= d;
        high = !high;
    }
    return n;
}

int main(void)
{
    qvi_t pNtQueryVolumeInformationFile = (qvi_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryVolumeInformationFile");
    static const WCHAR *const paths[] = {L"C:\\", L"C:\\windows\\system32\\kernel32.dll"};
    const char *uuid = getenv("SG_FS_UUID");
    UCHAR expect[16], zero[48] = {0};
    objid_info info, first;
    IO_STATUS_BLOCK io;
    unsigned int i;
    NTSTATUS st;
    HANDLE h;

    printf("SG_FS_UUID=%s\n", uuid ? uuid : "(none)");
    for (i = 0; i < ARRAYSIZE(paths); i++)
    {
        h = CreateFileW(paths[i], 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS, NULL);
        check(h != INVALID_HANDLE_VALUE, "open %ls", paths[i]);
        if (h == INVALID_HANDLE_VALUE) continue;
        memset(&info, 0xcc, sizeof(info));
        io.Information = 0xdead;
        st = pNtQueryVolumeInformationFile(h, &io, &info, sizeof(info), FileFsObjectIdInformation_);
        if (uuid && *uuid && parse(uuid, expect))
        {
            check(st == 0 && io.Information == sizeof(info) && !memcmp(info.ObjectId, expect, 16) &&
                  !memcmp(info.ExtendedInfo, zero, 48), "%ls: %#lx, the file system's UUID", paths[i], st);
            if (!i) first = info;
            else check(!memcmp(first.ObjectId, info.ObjectId, 16), "the same id through another file");
        }
        else check(st == (NTSTATUS)0xc0000010, "%ls without a UUID: %#lx", paths[i], st);
        st = pNtQueryVolumeInformationFile(h, &io, &info, sizeof(info) - 1, FileFsObjectIdInformation_);
        check(st == (NTSTATUS)0xc0000004, "%ls: a short buffer %#lx", paths[i], st);
        CloseHandle(h);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
