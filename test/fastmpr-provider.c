/* A network provider for test/fastmpr-gate.sh: it connects nothing. NPAddConnection3 records the local
 * name it was asked for and refuses the first FASTMPR_REFUSE ones with ERROR_ALREADY_ASSIGNED
 * (WN_ALREADY_CONNECTED); NPGetConnectionPerformance knows \\sgserver\share only. */
#include <windows.h>
#include <winnetwk.h>
#include <stdio.h>

#define WNNC_SPEC_VERSION 1
#define WNNC_NET_TYPE 2
#define WNNC_CONNECTION 6
#define WNNC_CON_ADDCONNECTION3 8
#define WNNC_CON_CANCELCONNECTION 4

static int calls;
static WCHAR last_local[16];

static void note(const WCHAR *s)
{
    HANDLE f;
    DWORD n;
    WCHAR path[MAX_PATH];
    char line[64];

    GetEnvironmentVariableW(L"FASTMPR_LOG", path, MAX_PATH);
    f = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    n = sprintf(line, "%ls\n", s);
    WriteFile(f, line, n, &n, NULL);
    CloseHandle(f);
}

DWORD WINAPI NPGetCaps(DWORD index)
{
    switch (index)
    {
    case WNNC_SPEC_VERSION: return 0x00010000;
    case WNNC_NET_TYPE: return 0x00430000;
    case WNNC_CONNECTION: return WNNC_CON_ADDCONNECTION3 | WNNC_CON_CANCELCONNECTION;
    }
    return 0;
}

DWORD WINAPI NPAddConnection3(HWND owner, LPNETRESOURCEW res, LPWSTR password, LPWSTR user, DWORD flags)
{
    WCHAR refuse[8] = L"0";
    int limit;

    GetEnvironmentVariableW(L"FASTMPR_REFUSE", refuse, 8);
    limit = _wtoi(refuse);
    lstrcpynW(last_local, res->lpLocalName ? res->lpLocalName : L"", 16);
    note(last_local);
    if (calls++ < limit) return WN_ALREADY_CONNECTED;
    return WN_SUCCESS;
}

DWORD WINAPI NPCancelConnection(LPWSTR name, BOOL force)
{
    return WN_SUCCESS;
}

DWORD WINAPI NPGetConnectionPerformance(LPCWSTR remote, LPNETCONNECTINFOSTRUCT info)
{
    if (lstrcmpiW(remote, L"\\\\sgserver\\share")) return WN_BAD_NETNAME;
    info->dwFlags = WNCON_NOTROUTED | WNCON_DYNAMIC;
    info->dwSpeed = 12340;
    info->dwDelay = 7;
    info->dwOptDataSize = 4096;
    return WN_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved) { return TRUE; }
