/* mpr gap-A batch (patch 3222), run by test/fastmpr-gate.sh: a test network provider (fastmpr-provider.c)
 * registered in the prefix; MultinetGetConnectionPerformance and the LPT port WNetUseConnection picks. */
#include <windows.h>
#include <winnetwk.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define SVC L"System\\CurrentControlSet\\Services\\SgTestNet\\NetworkProvider"

static void register_provider(const WCHAR *dll)
{
    HKEY key;
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, SVC, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExW(key, L"Name", 0, REG_SZ, (BYTE *)L"SgTest Network", sizeof(L"SgTest Network"));
    RegSetValueExW(key, L"ProviderPath", 0, REG_EXPAND_SZ, (BYTE *)dll, (lstrlenW(dll) + 1) * sizeof(WCHAR));
    RegCloseKey(key);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExW(key, L"ProviderOrder", 0, REG_SZ, (BYTE *)L"SgTestNet", sizeof(L"SgTestNet"));
    RegCloseKey(key);
}

static void test_perf(void)
{
    NETCONNECTINFOSTRUCT info;
    NETRESOURCEW res;
    NETRESOURCEA resA;
    DWORD ret;

    memset(&res, 0, sizeof(res));
    memset(&info, 0, sizeof(info));
    info.cbStructure = sizeof(info);

    ret = MultinetGetConnectionPerformanceW(NULL, &info);
    check(ret == ERROR_INVALID_ADDRESS, "a NULL resource: ERROR_INVALID_ADDRESS");
    ret = MultinetGetConnectionPerformanceW(&res, NULL);
    check(ret == ERROR_INVALID_ADDRESS, "a NULL info: ERROR_INVALID_ADDRESS");
    info.cbStructure = 4;
    res.lpRemoteName = (WCHAR *)L"\\\\sgserver\\share";
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == ERROR_INVALID_PARAMETER, "a wrong cbStructure: ERROR_INVALID_PARAMETER");
    info.cbStructure = sizeof(info);

    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == NO_ERROR && info.dwSpeed == 12340 && info.dwDelay == 7 && info.dwOptDataSize == 4096 &&
          info.dwFlags == (WNCON_NOTROUTED | WNCON_DYNAMIC) && info.cbStructure == sizeof(info), "the provider fills in the performance");
    res.lpRemoteName = (WCHAR *)L"\\\\other\\share";
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == ERROR_BAD_NET_NAME, "a name the provider does not know: ERROR_BAD_NET_NAME");
    res.lpRemoteName = NULL;
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == ERROR_BAD_NET_NAME, "no name at all: ERROR_BAD_NET_NAME");
    res.lpLocalName = (WCHAR *)L"C:";
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == ERROR_NOT_CONNECTED, "a drive that is not redirected: ERROR_NOT_CONNECTED");
    res.lpLocalName = (WCHAR *)L"Q:";
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == ERROR_BAD_DEVICE, "a device that does not exist: ERROR_BAD_DEVICE");
    res.lpLocalName = NULL;
    res.lpRemoteName = (WCHAR *)L"\\\\sgserver\\share";
    res.lpProvider = (WCHAR *)L"No Such Provider";
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == ERROR_BAD_PROVIDER, "an unknown provider: ERROR_BAD_PROVIDER");
    res.lpProvider = (WCHAR *)L"SgTest Network";
    info.dwSpeed = 0;
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(ret == NO_ERROR && info.dwSpeed == 12340, "a named provider");
    SetLastError(0);
    res.lpRemoteName = (WCHAR *)L"\\\\other\\share";
    ret = MultinetGetConnectionPerformanceW(&res, &info);
    check(GetLastError() == ret && ret != NO_ERROR, "the error is also the last error");

    memset(&resA, 0, sizeof(resA));
    resA.lpRemoteName = (char *)"\\\\sgserver\\share";
    memset(&info, 0, sizeof(info));
    info.cbStructure = sizeof(info);
    ret = MultinetGetConnectionPerformanceA(&resA, &info);
    check(ret == NO_ERROR && info.dwDelay == 7, "the ANSI function");
    ret = MultinetGetConnectionPerformanceA(NULL, &info);
    check(ret == ERROR_INVALID_ADDRESS, "the ANSI function with a NULL resource");
}

static void read_log(WCHAR names[][16], int *n)
{
    FILE *f;
    char line[64];
    WCHAR path[MAX_PATH];
    *n = 0;
    GetEnvironmentVariableW(L"FASTMPR_LOG", path, MAX_PATH);
    f = _wfopen(path, L"r");
    if (!f) return;
    while (fgets(line, sizeof(line), f) && *n < 20)
    {
        line[strcspn(line, "\r\n")] = 0;
        MultiByteToWideChar(CP_ACP, 0, line, -1, names[*n], 16);
        (*n)++;
    }
    fclose(f);
}

static void test_printer(void)
{
    NETRESOURCEW res;
    WCHAR access[64], names[20][16];
    DWORD size, ret, result, count, i;
    int n;
    HANDLE henum;
    HKEY key;
    BYTE buffer[1024];
    WCHAR logpath[MAX_PATH];

    GetEnvironmentVariableW(L"FASTMPR_LOG", logpath, MAX_PATH);
    memset(&res, 0, sizeof(res));
    res.dwType = RESOURCETYPE_PRINT;
    res.lpRemoteName = (WCHAR *)L"\\\\sgserver\\printer";
    size = (DWORD)(sizeof(access) / sizeof(WCHAR));
    access[0] = 0;
    result = 0;
    ret = WNetUseConnectionW(NULL, &res, NULL, NULL, CONNECT_REDIRECT, access, &size, &result);
    check(ret == NO_ERROR, "a printer connection with CONNECT_REDIRECT");
    check(!wcsncmp(access, L"LPT", 3) && access[3] >= '1' && access[3] <= '9' && !access[4], "the system chose an LPT port");
    check(result == CONNECT_LOCALDRIVE, "which is reported as a redirected device");
    read_log(names, &n);
    /* the provider refused the first two names (FASTMPR_REFUSE=2): the next ports were tried in turn */
    check(n == 3 && names[0][3] + 1 == names[1][3] && names[1][3] + 1 == names[2][3] && !wcscmp(names[2], access), "refused ports are followed by the next one");

    /* remembered: HKCU\Network\LPTn with a printer connection type */
    DeleteFileW(logpath);
    size = (DWORD)(sizeof(access) / sizeof(WCHAR));
    ret = WNetUseConnectionW(NULL, &res, NULL, NULL, CONNECT_REDIRECT | CONNECT_UPDATE_PROFILE, access, &size, &result);
    check(ret == NO_ERROR, "a persistent printer connection");
    {
        WCHAR subkey[32];
        DWORD type = 0, tsize = sizeof(type);
        swprintf(subkey, 32, L"Network\\%.4ls", access);
        check(RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_READ, &key) == 0, "it is remembered under the port's name");
        check(!RegQueryValueExW(key, L"ConnectionType", NULL, NULL, (BYTE *)&type, &tsize) && type == RESOURCETYPE_PRINT, "with the printer type");
        RegCloseKey(key);
        ret = WNetOpenEnumW(RESOURCE_REMEMBERED, RESOURCETYPE_PRINT, 0, NULL, &henum);
        check(ret == NO_ERROR, "enumerate the remembered printers");
        count = -1;
        size = sizeof(buffer);
        ret = WNetEnumResourceW(henum, &count, buffer, &size);
        check(ret == NO_ERROR && count == 1, "one of them");
        if (count == 1)
        {
            NETRESOURCEW *r = (NETRESOURCEW *)buffer;
            WCHAR expect[8];
            swprintf(expect, 8, L"%.4ls:", access);
            check(r->lpLocalName && !wcscmp(r->lpLocalName, expect) && r->lpRemoteName && !wcscmp(r->lpRemoteName, L"\\\\sgserver\\printer") &&
                  r->dwType == RESOURCETYPE_PRINT, "by its port name and its remote name");
        }
        WNetCloseEnum(henum);
        RegDeleteKeyW(HKEY_CURRENT_USER, subkey);
    }
    (void)i;
}

int main(int argc, char **argv)
{
    WCHAR dll[MAX_PATH];

    if (argc < 2)
    {
        /* register the provider, then run again so that mpr loads it */
        WCHAR cmd[MAX_PATH * 2], self[MAX_PATH];
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 1;

        GetModuleFileNameW(NULL, self, MAX_PATH);
        GetFullPathNameW(L"fastmpr-provider.dll", MAX_PATH, dll, NULL);
        register_provider(dll);
        swprintf(cmd, MAX_PATH * 2, L"\"%ls\" run", self);
        SetEnvironmentVariableW(L"FASTMPR_REFUSE", L"2");
        if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { printf("FAIL  cannot run\n"); return 1; }
        WaitForSingleObject(pi.hProcess, 120000);
        GetExitCodeProcess(pi.hProcess, &code);
        return code;
    }
    test_perf();
    test_printer();
    printf("DONE\n");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
