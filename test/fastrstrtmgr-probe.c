#include <windows.h>
#include <restartmanager.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char **argv)
{
    DWORD (WINAPI *pStart)(DWORD *, DWORD, WCHAR *);
    DWORD (WINAPI *pEnd)(DWORD);
    DWORD (WINAPI *pReg)(DWORD, UINT, LPCWSTR *, UINT, RM_UNIQUE_PROCESS *, UINT, LPCWSTR *);
    DWORD (WINAPI *pList)(DWORD, UINT *, UINT *, RM_PROCESS_INFO *, LPDWORD);
    DWORD (WINAPI *pShut)(DWORD, ULONG, void *);
    DWORD (WINAPI *pRestart)(DWORD, DWORD, void *);
    DWORD (WINAPI *pJoin)(DWORD *, const WCHAR *);
    HMODULE mod;
    DWORD handle, h2, ret, reasons = 0;
    WCHAR key[CCH_RM_SESSION_KEY + 1], exe[MAX_PATH];
    UINT needed = 0, count = 0;
    RM_PROCESS_INFO info[8];
    LPCWSTR files[1] = { exe };
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { sizeof(si) };
    char cmd[MAX_PATH + 16];
    RM_UNIQUE_PROCESS child = {0};
    FILETIME e, k, u;

    if (argc > 1) { Sleep(60000); return 0; }

    mod = LoadLibraryA("rstrtmgr.dll");
    CHECK(mod != NULL, "load");
    if (!mod) return 1;
    pStart = (void *)GetProcAddress(mod, "RmStartSession");
    pEnd = (void *)GetProcAddress(mod, "RmEndSession");
    pReg = (void *)GetProcAddress(mod, "RmRegisterResources");
    pList = (void *)GetProcAddress(mod, "RmGetList");
    pShut = (void *)GetProcAddress(mod, "RmShutdown");
    pRestart = (void *)GetProcAddress(mod, "RmRestart");
    pJoin = (void *)GetProcAddress(mod, "RmJoinSession");

    ret = pStart(&handle, 0, key); CHECK(ret == 0 && wcslen(key) == CCH_RM_SESSION_KEY, "start %lu", ret);
    ret = pJoin(&h2, key); CHECK(ret == 0 && h2 == handle, "join %lu", ret);
    ret = pReg(0xdeadbeef, 0, NULL, 0, NULL, 0, NULL); CHECK(ret == ERROR_INVALID_HANDLE, "bad handle %lu", ret);

    GetModuleFileNameW(NULL, exe, MAX_PATH);
    ret = pReg(handle, 1, files, 0, NULL, 0, NULL); CHECK(ret == 0, "register %lu", ret);

    ret = pList(handle, &needed, &count, NULL, &reasons);
    CHECK(ret == ERROR_MORE_DATA && needed == 1 && (reasons & RmRebootReasonDetectedSelf), "list %lu needed %u reasons %lx", ret, needed, reasons);
    count = 8;
    ret = pList(handle, &needed, &count, info, &reasons);
    CHECK(ret == 0 && count == 1 && info[0].Process.dwProcessId == GetCurrentProcessId(), "list2 %lu count %u", ret, count);

    /* a child registered by process id is found, closed by force */
    sprintf(cmd, "\"%ls\" child", exe);
    CHECK(CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi), "child");
    child.dwProcessId = pi.dwProcessId;
    GetProcessTimes(pi.hProcess, &child.ProcessStartTime, &e, &k, &u);
    ret = pReg(handle, 0, NULL, 1, &child, 0, NULL); CHECK(ret == 0, "register app %lu", ret);
    count = 8;
    ret = pList(handle, &needed, &count, info, &reasons);
    CHECK(ret == 0 && count == 2, "list3 %lu count %u", ret, count);

    ret = pShut(handle, 0, NULL); CHECK(ret == 351, "shutdown without force %lu", ret);
    CHECK(WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT, "child died");
    ret = pShut(handle, 1, NULL); CHECK(ret == 0, "forced shutdown %lu", ret);
    CHECK(WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0, "child still running");
    ret = pRestart(handle, 0, NULL); CHECK(ret == 0, "restart %lu", ret);

    ret = pEnd(handle); CHECK(ret == 0, "end %lu", ret);
    ret = pEnd(handle); CHECK(ret == ERROR_INVALID_HANDLE, "end twice %lu", ret);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
