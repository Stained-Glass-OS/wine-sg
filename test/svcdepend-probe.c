/* EnumDependentServices and QueryServiceLockStatus (patches/sg/1617). The
 * stubs said no service depended on any other, and the lock status failed.
 * SGDepA is in group SGDepGroup; SGDepB depends on SGDepA, SGDepC on
 * SGDepB, SGDepD on the group; SGDepE on nothing. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static SC_HANDLE make(SC_HANDLE scm, const WCHAR *name, const WCHAR *group, const WCHAR *deps)
{
    SC_HANDLE s = OpenServiceW(scm, name, DELETE);
    if (s) { DeleteService(s); CloseServiceHandle(s); }
    return CreateServiceW(scm, name, name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                          SERVICE_ERROR_IGNORE, L"C:\\windows\\system32\\notepad.exe", group, NULL, deps, NULL, NULL);
}

int main(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS), a, b, c, d, e;
    BYTE buf[4096];
    ENUM_SERVICE_STATUSW *list = (ENUM_SERVICE_STATUSW *)buf;
    ENUM_SERVICE_STATUSA *lista = (ENUM_SERVICE_STATUSA *)buf;
    QUERY_SERVICE_LOCK_STATUSW *lock = (QUERY_SERVICE_LOCK_STATUSW *)buf;
    DWORD needed = 0, count = 99, i;
    int ib = -1, ic = -1, id = -1, ie = -1;
    BOOL ok;

    a = make(scm, L"SGDepA", L"SGDepGroup", NULL);
    b = make(scm, L"SGDepB", NULL, L"SGDepA\0");
    c = make(scm, L"SGDepC", NULL, L"SGDepB\0");
    d = make(scm, L"SGDepD", NULL, L"+SGDepGroup\0");
    e = make(scm, L"SGDepE", NULL, NULL);
    if (!a || !b || !c || !d || !e) { printf("FAIL  services not made (%lu)\nRESULT: FAIL\n", GetLastError()); return 1; }

    SetLastError(0xdeadbeef);
    ok = EnumDependentServicesW(a, SERVICE_STATE_ALL, NULL, 0, &needed, &count);
    printf("size: ok %d err %lu needed %lu\n", ok, ok ? 0 : GetLastError(), needed);
    check(!ok && GetLastError() == ERROR_MORE_DATA && needed > 3 * sizeof(ENUM_SERVICE_STATUSW), "the size needed");
    ok = EnumDependentServicesW(a, SERVICE_STATE_ALL, list, sizeof(buf), &needed, &count);
    for (i = 0; ok && i < count; i++)
    {
        printf("  %ls\n", list[i].lpServiceName);
        if (!wcscmp(list[i].lpServiceName, L"SGDepB")) ib = i;
        if (!wcscmp(list[i].lpServiceName, L"SGDepC")) ic = i;
        if (!wcscmp(list[i].lpServiceName, L"SGDepD")) id = i;
        if (!wcscmp(list[i].lpServiceName, L"SGDepE")) ie = i;
    }
    check(ok && count == 3 && ib >= 0 && ic >= 0 && id >= 0 && ie < 0, "SGDepA's dependents: B, C (through B) and D (through the group)");
    check(ic >= 0 && ib >= 0 && ic < ib, "... C before B, the order they are stopped in");
    check(ok && count && list[0].ServiceStatus.dwCurrentState == SERVICE_STOPPED, "... with their status");
    ok = EnumDependentServicesW(a, SERVICE_ACTIVE, list, sizeof(buf), &needed, &count);
    check(ok && count == 0, "none of them active");
    ok = EnumDependentServicesW(e, SERVICE_STATE_ALL, list, sizeof(buf), &needed, &count);
    check(ok && count == 0, "SGDepE has none");
    ok = EnumDependentServicesA(b, SERVICE_STATE_ALL, lista, sizeof(buf), &needed, &count);
    check(ok && count == 1 && !strcmp(lista[0].lpServiceName, "SGDepC"), "EnumDependentServicesA: SGDepB's is SGDepC");

    memset(buf, 0xcc, sizeof(buf));
    ok = QueryServiceLockStatusW(scm, lock, sizeof(buf), &needed);
    check(ok && !lock->fIsLocked && lock->lpLockOwner && !lock->lpLockOwner[0], "QueryServiceLockStatusW: not locked");

    DeleteService(e); DeleteService(d); DeleteService(c); DeleteService(b); DeleteService(a);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
