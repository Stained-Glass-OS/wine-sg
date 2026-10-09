/* Firmware / boot environment (patches/sg/2206): NtQuerySystemInformation
 * SystemFlagsInformation (9), SystemRangeStartInformation (50),
 * SystemBootEnvironmentInformation (90), SystemSecureBootInformation (145),
 * kernel32 GetFirmwareType, and GetFirmwareEnvironmentVariable{A,W,ExA,ExW}
 * reading UEFI variables.  The expected answers are read from the host's
 * own /sys (seen as Z:\sys) so the probe is right on a BIOS box too. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *NtQSI)(ULONG, void *, ULONG, ULONG *);
static NtQSI pNtQSI;
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef struct { GUID id; ULONG type; ULONGLONG flags; } BOOTENV;

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

static int read_file(const char *path, unsigned char *buf, int max)
{
    HANDLE h = CreateFileA( path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (!ReadFile( h, buf, max, &n, NULL )) n = 0;
    CloseHandle( h );
    return n;
}

int main(void)
{
    ULONG retlen, i;
    LONG st;
    BOOTENV boot;
    int uefi;
    unsigned char sb[2], file[4096];
    SYSTEM_INFO si;
    ULONG_PTR range;
    ULONG flags;
    FIRMWARE_TYPE ft;
    DWORD ret;

    pNtQSI = (NtQSI)GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "NtQuerySystemInformation" );
    uefi = GetFileAttributesA( "Z:\\sys\\firmware\\efi" ) != INVALID_FILE_ATTRIBUTES;
    printf( "host firmware: %s\n", uefi ? "UEFI" : "BIOS" );

    /* SystemBootEnvironmentInformation */
    memset( &boot, 0xcc, sizeof(boot) );
    retlen = 0;
    st = pNtQSI( 90, &boot, sizeof(boot), &retlen );
    check( st == 0 && retlen == sizeof(boot) && sizeof(boot) == 32, "BootEnvironment: success, 32 bytes" );
    check( boot.type == (uefi ? 2 : 1), "BootEnvironment: firmware type matches the host (2=UEFI, 1=BIOS)" );
    check( boot.flags == 0, "BootEnvironment: no boot flags" );
    {
        unsigned char txt[64];
        int n = read_file( "Z:\\proc\\sys\\kernel\\random\\boot_id", txt, 63 ), j = 0, k;
        unsigned char b[16];
        if (n >= 32)
        {
            for (k = 0; k < n && j < 16; k++)
            {
                if (txt[k] == '-') continue;
                b[j++] = hexv( txt[k] ) << 4 | hexv( txt[k + 1] );
                k++;
            }
            check( boot.id.Data1 == (ULONG)(b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]) && boot.id.Data2 == (b[4] << 8 | b[5]) &&
                   boot.id.Data3 == (b[6] << 8 | b[7]) && !memcmp( boot.id.Data4, b + 8, 8 ),
                   "BootEnvironment: boot identifier is the kernel's boot id" );
        }
        else check( 1, "BootEnvironment: (no boot_id to compare)" );
    }
    retlen = 0;
    st = pNtQSI( 90, &boot, sizeof(boot) - 1, &retlen );
    check( st == (LONG)0xC0000004 && retlen == sizeof(boot), "BootEnvironment: short buffer is STATUS_INFO_LENGTH_MISMATCH with the size" );

    /* GetFirmwareType */
    ft = 99;
    check( GetFirmwareType( &ft ) && ft == (FIRMWARE_TYPE)(uefi ? 2 : 1), "GetFirmwareType agrees (Uefi/Bios, not Unknown)" );
    SetLastError( 0 );
    check( !GetFirmwareType( NULL ) && GetLastError() == ERROR_INVALID_PARAMETER, "GetFirmwareType(NULL) fails with ERROR_INVALID_PARAMETER" );

    /* SystemFlagsInformation */
    flags = 0xcccccccc; retlen = 0;
    st = pNtQSI( 9, &flags, sizeof(flags), &retlen );
    check( st == 0 && retlen == 4 && flags == 0, "SystemFlagsInformation: 4 bytes, no flags" );
    st = pNtQSI( 9, &flags, 3, &retlen );
    check( st == (LONG)0xC0000004 && retlen == 4, "SystemFlagsInformation: short buffer" );

    /* SystemRangeStartInformation */
    range = 0; retlen = 0;
    GetSystemInfo( &si );
    st = pNtQSI( 50, &range, sizeof(range), &retlen );
    check( st == 0 && retlen == sizeof(range), "SystemRangeStart: success with pointer size" );
    check( range > (ULONG_PTR)si.lpMaximumApplicationAddress, "SystemRangeStart: above the highest user address" );
#ifdef _WIN64
    check( range == 0xffff080000000000ull, "SystemRangeStart: the 64-bit system range start" );
#else
    check( range >= 0x7fff0000 && range == (ULONG_PTR)si.lpMaximumApplicationAddress + 1, "SystemRangeStart: one past the 32-bit user space" );
#endif
    st = pNtQSI( 50, &range, 2, &retlen );
    check( st == (LONG)0xC0000004, "SystemRangeStart: short buffer" );

    /* SystemSecureBootInformation */
    {
        int n = read_file( "Z:\\sys\\firmware\\efi\\efivars\\SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c", file, 16 );
        int cap = n >= 5, en = cap && file[4] == 1;
        memset( sb, 0xcc, sizeof(sb) ); retlen = 0;
        st = pNtQSI( 145, sb, 2, &retlen );
        check( st == 0 && retlen == 2, "SecureBoot: success, 2 bytes" );
        check( sb[0] == en && sb[1] == cap, "SecureBoot: enabled/capable match the SecureBoot EFI variable" );
        st = pNtQSI( 145, sb, 1, &retlen );
        check( st == (LONG)0xC0000004, "SecureBoot: short buffer" );
    }

    /* UEFI variables: as on Windows, only with SeSystemEnvironmentPrivilege
     * enabled (integrator); first without it */
    {
        unsigned char tmp[64];
        SetLastError( 0 );
        ret = GetFirmwareEnvironmentVariableA( "Boot0000", "{8be4df61-93ca-11d2-aa0d-00e098032b8c}", tmp, sizeof(tmp) );
        check( ret == 0 && GetLastError() == ERROR_PRIVILEGE_NOT_HELD,
               "without SeSystemEnvironmentPrivilege enabled: ERROR_PRIVILEGE_NOT_HELD" );
    }
    {
        TOKEN_PRIVILEGES tp = { 1 };
        HANDLE token;
        BOOL ok = FALSE;
        if (OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token ))
        {
            LookupPrivilegeValueA( NULL, "SeSystemEnvironmentPrivilege", &tp.Privileges[0].Luid );
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            ok = AdjustTokenPrivileges( token, FALSE, &tp, 0, NULL, NULL ) && GetLastError() == ERROR_SUCCESS;
            CloseHandle( token );
        }
        check( ok, "SeSystemEnvironmentPrivilege enabled (an administrator)" );
    }
    {
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA( "Z:\\sys\\firmware\\efi\\efivars\\*", &fd );
        char name[200], guid[64], path[400];
        WCHAR nameW[200], guidW[64];
        unsigned char got[4096];
        int found = 0, n;

        if (!uefi)
        {
            SetLastError( 0 );
            ret = GetFirmwareEnvironmentVariableA( "Boot0000", "{8be4df61-93ca-11d2-aa0d-00e098032b8c}", got, sizeof(got) );
            check( ret == 0 && GetLastError() == ERROR_INVALID_FUNCTION, "BIOS: firmware variables fail with ERROR_INVALID_FUNCTION" );
        }
        if (uefi && h != INVALID_HANDLE_VALUE)
        {
            do
            {
                size_t len = strlen( fd.cFileName );
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                if (len < 38 || fd.cFileName[len - 37] != '-') continue;
                sprintf( path, "Z:\\sys\\firmware\\efi\\efivars\\%s", fd.cFileName );
                n = read_file( path, file, sizeof(file) );
                if (n < 4 || n >= (int)sizeof(file)) continue;   /* unreadable or too big for this probe */
                memcpy( name, fd.cFileName, len - 37 ); name[len - 37] = 0;
                sprintf( guid, "{%s}", fd.cFileName + len - 36 );
                found = 1;
                break;
            } while (FindNextFileA( h, &fd ));
            FindClose( h );
        }
        if (found)
        {
            DWORD attr = 0xdeadbeef;
            printf( "variable under test: %s %s (%d data bytes)\n", name, guid, n - 4 );
            MultiByteToWideChar( CP_ACP, 0, name, -1, nameW, 200 );
            MultiByteToWideChar( CP_ACP, 0, guid, -1, guidW, 64 );

            memset( got, 0xcc, sizeof(got) );
            ret = GetFirmwareEnvironmentVariableA( name, guid, got, sizeof(got) );
            check( ret == (DWORD)(n - 4) && !memcmp( got, file + 4, n - 4 ), "GetFirmwareEnvironmentVariableA returns the variable's data" );
            memset( got, 0xcc, sizeof(got) );
            ret = GetFirmwareEnvironmentVariableW( nameW, guidW, got, sizeof(got) );
            check( ret == (DWORD)(n - 4) && !memcmp( got, file + 4, n - 4 ), "GetFirmwareEnvironmentVariableW returns the same" );
            ret = GetFirmwareEnvironmentVariableExW( nameW, guidW, got, sizeof(got), &attr );
            check( ret == (DWORD)(n - 4) && attr == (DWORD)(file[0] | file[1] << 8 | file[2] << 16 | file[3] << 24), "...ExW also returns the attributes" );
            attr = 0xdeadbeef;
            ret = GetFirmwareEnvironmentVariableExA( name, guid, got, sizeof(got), &attr );
            check( ret == (DWORD)(n - 4) && attr == (DWORD)(file[0] | file[1] << 8 | file[2] << 16 | file[3] << 24), "...ExA also returns the attributes" );
            if (n > 4)
            {
                SetLastError( 0 );
                ret = GetFirmwareEnvironmentVariableW( nameW, guidW, got, n - 5 );
                check( ret == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "a buffer one byte short is ERROR_INSUFFICIENT_BUFFER" );
            }
            /* GUID letters in the other case name the same variable */
            for (i = 0; guidW[i]; i++) if (guidW[i] >= 'a' && guidW[i] <= 'f') guidW[i] -= 32;
            ret = GetFirmwareEnvironmentVariableW( nameW, guidW, got, sizeof(got) );
            check( ret == (DWORD)(n - 4), "an upper-case GUID string works" );
        }
        else if (uefi) printf( "(no readable EFI variable here: data cases skipped)\n" );

        if (uefi)
        {
            SetLastError( 0 );
            ret = GetFirmwareEnvironmentVariableW( L"NoSuchVariableHere", L"{8be4df61-93ca-11d2-aa0d-00e098032b8c}", file, 10 );
            check( ret == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND, "a missing variable is ERROR_ENVVAR_NOT_FOUND" );
            SetLastError( 0 );
            ret = GetFirmwareEnvironmentVariableW( L"../../../etc/passwd", L"{8be4df61-93ca-11d2-aa0d-00e098032b8c}", file, 10 );
            check( ret == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND, "a name with a path separator is just not found" );
        }
        SetLastError( 0 );
        ret = GetFirmwareEnvironmentVariableW( L"Boot0000", L"not a guid", file, 10 );
        check( ret == 0 && GetLastError() == ERROR_INVALID_PARAMETER, "a malformed GUID string is ERROR_INVALID_PARAMETER" );
        SetLastError( 0 );
        ret = GetFirmwareEnvironmentVariableW( NULL, L"{8be4df61-93ca-11d2-aa0d-00e098032b8c}", file, 10 );
        check( ret == 0 && GetLastError() == ERROR_INVALID_PARAMETER, "a NULL name is ERROR_INVALID_PARAMETER" );
    }

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures ? 1 : 0;
}
