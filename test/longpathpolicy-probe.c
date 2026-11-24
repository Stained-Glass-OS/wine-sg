/* longpathpolicy-probe (patches/sg/9755, test/longpathpolicy-gate.sh): the machine's long path policy and
 * ntdll's RtlAreLongPathsEnabled. Prints:
 *   POLICY <value|none>     HKLM\System\CurrentControlSet\Control\FileSystem LongPathsEnabled
 *   RTL <missing|0|1>       ntdll!RtlAreLongPathsEnabled()
 *   LONG <ok|fail gle> <n>  a folder and a file <n> characters deep, without \\?\ */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    BOOLEAN (WINAPI *rtl)(void) = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlAreLongPathsEnabled");
    DWORD v = 0, size = sizeof(v);
    WCHAR p[1024] = L"C:\\lp";
    HANDLE h;
    int i;

    if (!RegGetValueW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\FileSystem", L"LongPathsEnabled",
                      RRF_RT_REG_DWORD, NULL, &v, &size)) printf("POLICY %lu\n", v);
    else printf("POLICY none\n");
    if (!rtl) printf("RTL missing\n");
    else printf("RTL %d\n", rtl() ? 1 : 0);
    CreateDirectoryW(p, NULL);
    for (i = 0; i < 10; i++)
    {
        lstrcatW(p, L"\\abcdefghijklmnopqrstuvwxyz0123456789");
        if (!CreateDirectoryW(p, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    lstrcatW(p, L"\\file.txt");
    h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { printf("LONG ok %d\n", lstrlenW(p)); CloseHandle(h); }
    else printf("LONG fail %lu %d\n", GetLastError(), lstrlenW(p));
    return 0;
}
