/* Files and processes (patches/sg/1691), run by test/fileflags-gate.sh:
 * FindFirstFileExW's FIND_FIRST_EX_CASE_SENSITIVE (only the mask's case)
 * and FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY, an unknown flag refused;
 * GetFileInformationByHandleEx(FileRemoteProtocolInfo) on a local file;
 * CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST) and Heap32ListFirst/Next,
 * Heap32First/Next over this process's heaps; SetupDi*DevRegKey with
 * DICS_FLAG_CONFIGSPECIFIC (the hardware profile's keys). These were FIXMEs
 * and stubs. */
#include <windows.h>
#include <tlhelp32.h>
#include <setupapi.h>
#include <stdio.h>

#ifndef FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY
#define FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY 4
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int count_matches(const WCHAR *pattern, DWORD flags)
{
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(pattern, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, flags);
    int n = 0;

    if (h == INVALID_HANDLE_VALUE) return 0;
    do n++; while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    FILE_REMOTE_PROTOCOL_INFO remote;
    WIN32_FIND_DATAW fd;
    HEAPLIST32 hl = { sizeof(hl) };
    HEAPENTRY32 he = { sizeof(he) };
    HANDLE h, snap, heap;
    void *block;
    int heaps = 0, blocks = 0, found_block = 0, has_default = 0;

    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"sg-fileflags");
    CreateDirectoryW(dir, NULL);
    _snwprintf(path, MAX_PATH, L"%ls\\Report.TXT", dir);
    CloseHandle(CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    _snwprintf(path, MAX_PATH, L"%ls\\notes.txt", dir);
    CloseHandle(CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));

    _snwprintf(path, MAX_PATH, L"%ls\\*.txt", dir);
    check(count_matches(path, 0) == 2, "*.txt without the flag: both files");
    check(count_matches(path, FIND_FIRST_EX_CASE_SENSITIVE) == 1, "FIND_FIRST_EX_CASE_SENSITIVE: only notes.txt");
    _snwprintf(path, MAX_PATH, L"%ls\\*.TXT", dir);
    check(count_matches(path, FIND_FIRST_EX_CASE_SENSITIVE) == 1, "*.TXT case-sensitive: only Report.TXT");
    _snwprintf(path, MAX_PATH, L"%ls\\report.txt", dir);
    check(count_matches(path, FIND_FIRST_EX_CASE_SENSITIVE) == 0, "report.txt case-sensitive: none");
    check(count_matches(path, 0) == 1, "report.txt otherwise: Report.TXT");
    _snwprintf(path, MAX_PATH, L"%ls\\*", dir);
    check(count_matches(path, FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY) >= 2, "FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY");
    SetLastError(0xdeadbeef);
    h = FindFirstFileExW(path, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0x100);
    check(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown flag: ERROR_INVALID_PARAMETER");

    _snwprintf(path, MAX_PATH, L"%ls\\notes.txt", dir);
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    SetLastError(0xdeadbeef);
    check(!GetFileInformationByHandleEx(h, FileRemoteProtocolInfo, &remote, sizeof(remote)) &&
          GetLastError() == ERROR_INVALID_PARAMETER, "FileRemoteProtocolInfo on a local file: ERROR_INVALID_PARAMETER");
    CloseHandle(h);

    /* heaps */
    heap = HeapCreate(0, 0, 0);
    block = HeapAlloc(heap, 0, 1234);
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, 0);
    check(snap != INVALID_HANDLE_VALUE, "CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST)");
    if (snap != INVALID_HANDLE_VALUE && Heap32ListFirst(snap, &hl))
    {
        do
        {
            heaps++;
            if (hl.dwFlags & HF32_DEFAULT) has_default = 1;
            if (hl.th32HeapID == (ULONG_PTR)heap && Heap32First(&he, hl.th32ProcessID, hl.th32HeapID))
            {
                do
                {
                    blocks++;
                    if (he.dwAddress == (ULONG_PTR)block && he.dwBlockSize >= 1234 && he.dwFlags == LF32_FIXED)
                        found_block = 1;
                } while (Heap32Next(&he) && blocks < 100000);
            }
        } while (Heap32ListNext(snap, &hl));
    }
    check(heaps >= 2 && has_default, "the heaps, the default one marked HF32_DEFAULT");
    check(found_block, "Heap32First/Next walk to the block allocated");
    SetLastError(0xdeadbeef);
    check(!Heap32ListNext(snap, &hl) && GetLastError() == ERROR_NO_MORE_FILES, "then ERROR_NO_MORE_FILES");
    CloseHandle(snap);
    HeapDestroy(heap);

    {
        static const GUID guid = { 0x6a3f1b2c, 0x1234, 0x4abc, { 0x9d, 0x8e, 0x7f, 0x60, 0x51, 0x42, 0x33, 0x24 } };
        SP_DEVINFO_DATA dev = { sizeof(dev) };
        HDEVINFO set = SetupDiCreateDeviceInfoList(&guid, NULL);
        DWORD value = 7, size = sizeof(value);
        HKEY key;

        check(SetupDiCreateDeviceInfoA(set, "Root\\LEGACY_SGPROBE\\0000", &guid, NULL, NULL, 0, &dev) &&
              SetupDiRegisterDeviceInfo(set, &dev, 0, NULL, NULL, NULL), "a device");
        key = SetupDiCreateDevRegKeyW(set, &dev, DICS_FLAG_CONFIGSPECIFIC, 0, DIREG_DEV, NULL, NULL);
        if (key == INVALID_HANDLE_VALUE) printf("      error %lu\n", GetLastError());
        check(key != INVALID_HANDLE_VALUE, "SetupDiCreateDevRegKey(DICS_FLAG_CONFIGSPECIFIC, DIREG_DEV)");
        if (key != INVALID_HANDLE_VALUE)
        {
            RegSetValueExW(key, L"SgProfileValue", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
            RegCloseKey(key);
        }
        key = SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        value = 0;
        if (key != INVALID_HANDLE_VALUE)
        {
            RegQueryValueExW(key, L"SgProfileValue", NULL, NULL, (BYTE *)&value, &size);
            RegCloseKey(key);
        }
        check(value != 7, "not the global key");
        key = SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_CONFIGSPECIFIC, 0, DIREG_DEV, KEY_READ);
        value = 0;
        size = sizeof(value);
        if (key != INVALID_HANDLE_VALUE)
        {
            RegQueryValueExW(key, L"SgProfileValue", NULL, NULL, (BYTE *)&value, &size);
            RegCloseKey(key);
        }
        check(value == 7, "SetupDiOpenDevRegKey(DICS_FLAG_CONFIGSPECIFIC) reads it back");
        check(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Hardware Profiles\\Current\\System"
                            L"\\CurrentControlSet\\Enum\\ROOT\\LEGACY_SGPROBE\\0000", 0, KEY_READ, &key) == 0,
              "in the current hardware profile's branch");
        RegCloseKey(key);
        check(SetupDiDeleteDevRegKey(set, &dev, DICS_FLAG_CONFIGSPECIFIC, 0, DIREG_DEV), "SetupDiDeleteDevRegKey");
        check(SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_CONFIGSPECIFIC, 0, DIREG_DEV, KEY_READ) == INVALID_HANDLE_VALUE,
              "gone");
        SetupDiRemoveDevice(set, &dev);
        SetupDiDestroyDeviceInfoList(set);
    }

    _snwprintf(path, MAX_PATH, L"%ls\\Report.TXT", dir);
    DeleteFileW(path);
    _snwprintf(path, MAX_PATH, L"%ls\\notes.txt", dir);
    DeleteFileW(path);
    RemoveDirectoryW(dir);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
