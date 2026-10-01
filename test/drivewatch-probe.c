/* WM_DEVICECHANGE as programs get it when drives come and go (patches/sg/
 * 0637): a top-level window that prints each volume arrival or removal the
 * session's desktop broadcasts, for the seconds given:
 *   arrival mask=<unit mask> flags=<DBTF_*>   removal mask=... flags=...
 */
#include <windows.h>
#include <dbt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winioctl.h>

/* mount manager's Wine extension (include/ddk/mountmgr.h) */
#define IOCTL_MOUNTMGR_DEFINE_UNIX_DRIVE CTL_CODE((ULONG)'m', 32, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
struct mountmgr_unix_drive
{
    ULONG     size;
    ULONG     type;
    ULONG     fs_type;
    DWORD     serial;
    ULONGLONG unix_dev;
    WCHAR     letter;
    USHORT    mount_point_offset;
    USHORT    device_offset;
    USHORT    label_offset;
};

static int define_drive(char letter, const char *unix_path)
{
    char buf[sizeof(struct mountmgr_unix_drive) + MAX_PATH];
    struct mountmgr_unix_drive *drive = (struct mountmgr_unix_drive *)buf;
    HANDLE mgr = CreateFileW(L"\\\\.\\MountPointManager", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    BOOL ok;

    memset(buf, 0, sizeof(buf));
    drive->size = sizeof(*drive);
    drive->letter = letter | 0x20;
    if (unix_path)
    {
        drive->type = DRIVE_REMOVABLE;
        strcpy(buf + sizeof(*drive), unix_path);
        drive->mount_point_offset = sizeof(*drive);
        drive->size += strlen(unix_path) + 1;
    }
    else drive->type = DRIVE_NO_ROOT_DIR;
    if (mgr == INVALID_HANDLE_VALUE) { printf("nomgr %lu\n", GetLastError()); return 1; }
    ok = DeviceIoControl(mgr, IOCTL_MOUNTMGR_DEFINE_UNIX_DRIVE, drive, drive->size, NULL, 0, NULL, NULL);
    printf("%s %c %s\n", unix_path ? "define" : "remove", letter, ok ? "ok" : "failed");
    CloseHandle(mgr);
    return !ok;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DEVICECHANGE && (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE) && lp)
    {
        DEV_BROADCAST_HDR *hdr = (DEV_BROADCAST_HDR *)lp;
        if (hdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
        {
            DEV_BROADCAST_VOLUME *vol = (DEV_BROADCAST_VOLUME *)lp;
            printf("%s mask=%lx flags=%x\n", wp == DBT_DEVICEARRIVAL ? "arrival" : "removal",
                   vol->dbcv_unitmask, vol->dbcv_flags);
            fflush(stdout);
        }
        return TRUE;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = {0};
    DWORD start, secs = argc > 1 ? atoi(argv[1]) : 30;
    MSG msg;

    if (argc > 3 && !strcmp(argv[1], "define")) return define_drive(argv[2][0], argv[3]);
    if (argc > 2 && !strcmp(argv[1], "remove")) return define_drive(argv[2][0], NULL);

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"DriveWatchProbe";
    RegisterClassW(&wc);
    CreateWindowExW(0, L"DriveWatchProbe", L"drivewatch", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    printf("ready\n");
    fflush(stdout);
    for (start = GetTickCount(); GetTickCount() - start < secs * 1000;)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    }
    return 0;
}
