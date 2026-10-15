#include <windows.h>
#include <winspool.h>
#include <stdio.h>
static int fails;
#define CHK(c, ...) do { if (c) printf("ok   " __VA_ARGS__); else { printf("FAIL " __VA_ARGS__); fails++; } printf("\n"); } while (0)
int main(void)
{
    WCHAR name[] = L"sgmon", good[] = L"localspl.dll", bad[] = L"does_not_exist.dll", ver[] = L"version.dll";
    WCHAR env[] = L"Windows 4.0";
    MONITOR_INFO_2W mi = { name, env, good };
    BOOL r;
    DWORD e;

    DeleteMonitorW(NULL, env, name);
    SetLastError(0xdead); r = AddMonitorW(NULL, 2, (BYTE *)&mi); e = GetLastError();
    CHK(r && e == ERROR_SUCCESS, "add good monitor (%d, %lu)", r, e);
    SetLastError(0xdead); r = AddMonitorW(NULL, 2, (BYTE *)&mi); e = GetLastError();
    CHK(!r && e == ERROR_PRINT_MONITOR_ALREADY_INSTALLED, "add twice (%d, %lu)", r, e);
    r = DeleteMonitorW(NULL, env, name);
    CHK(r, "delete monitor");
    mi.pDLLName = bad;
    SetLastError(0xdead); r = AddMonitorW(NULL, 2, (BYTE *)&mi); e = GetLastError();
    CHK(!r && e == ERROR_MOD_NOT_FOUND, "add after delete, missing dll must fail (%d, %lu)", r, e);
    mi.pDLLName = ver;
    SetLastError(0xdead); r = AddMonitorW(NULL, 2, (BYTE *)&mi); e = GetLastError();
    CHK(!r && e == ERROR_PROC_NOT_FOUND, "add after delete, dll without monitor entry must fail (%d, %lu)", r, e);
    if (r) DeleteMonitorW(NULL, env, name);
    mi.pDLLName = good;
    SetLastError(0xdead); r = AddMonitorW(NULL, 2, (BYTE *)&mi); e = GetLastError();
    CHK(r && e == ERROR_SUCCESS, "add again after delete (%d, %lu)", r, e);
    r = DeleteMonitorW(NULL, env, name);
    CHK(r, "delete again");
    r = DeleteMonitorW(NULL, env, name);
    CHK(!r && GetLastError() == ERROR_UNKNOWN_PRINT_MONITOR, "delete unknown fails (%lu)", GetLastError());
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
