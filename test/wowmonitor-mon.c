/* test/wowmonitor-gate.sh's port monitor (64-bit): one port, "SGP1:", that it keeps to itself
 * (not under the registry's Ports key, as CutePDF Writer's monitor keeps "CPW4:"); a job sent
 * to it goes to C:\sgmon-out.bin. */
#include <windows.h>
#include <winspool.h>
#include <winsplp.h>

static const WCHAR port_name[] = L"SGP1:";
static HANDLE out_file = INVALID_HANDLE_VALUE;

static BOOL WINAPI mon_EnumPorts(HANDLE mon, LPWSTR name, DWORD level, LPBYTE buf, DWORD size, LPDWORD needed, LPDWORD returned)
{
    DWORD need = (level == 1 ? sizeof(PORT_INFO_1W) : sizeof(PORT_INFO_2W)) + sizeof(port_name);
    *needed = need;
    *returned = 0;
    if (!buf || size < need) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    memset(buf, 0, need);
    if (level == 1)
    {
        PORT_INFO_1W *pi = (PORT_INFO_1W *)buf;
        pi->pName = (WCHAR *)(pi + 1);
        memcpy(pi + 1, port_name, sizeof(port_name));
    }
    else
    {
        PORT_INFO_2W *pi = (PORT_INFO_2W *)buf;
        pi->pPortName = (WCHAR *)(pi + 1);
        memcpy(pi + 1, port_name, sizeof(port_name));
        pi->fPortType = PORT_TYPE_WRITE;
    }
    *returned = 1;
    return TRUE;
}

static BOOL WINAPI mon_OpenPort(HANDLE mon, LPWSTR name, PHANDLE handle) { *handle = (HANDLE)1; return TRUE; }

static BOOL WINAPI mon_StartDocPort(HANDLE port, LPWSTR printer, DWORD job, DWORD level, LPBYTE info)
{
    out_file = CreateFileW(L"C:\\sgmon-out.bin", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    return out_file != INVALID_HANDLE_VALUE;
}

static BOOL WINAPI mon_WritePort(HANDLE port, LPBYTE buf, DWORD size, LPDWORD written)
{
    return WriteFile(out_file, buf, size, written, NULL);
}

static BOOL WINAPI mon_EndDocPort(HANDLE port)
{
    CloseHandle(out_file);
    out_file = INVALID_HANDLE_VALUE;
    return TRUE;
}

static BOOL WINAPI mon_ClosePort(HANDLE port) { return TRUE; }
static BOOL WINAPI mon_AddPort(HANDLE mon, LPWSTR name, HWND hwnd, LPWSTR monitor) { return TRUE; }
static void WINAPI mon_Shutdown(HANDLE mon) { }

static MONITOR2 monitor = { sizeof(MONITOR2), mon_EnumPorts, mon_OpenPort, NULL, mon_StartDocPort, mon_WritePort, NULL,
                            mon_EndDocPort, mon_ClosePort, mon_AddPort, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                            mon_Shutdown };

__declspec(dllexport) LPMONITOR2 WINAPI InitializePrintMonitor2(PMONITORINIT init, PHANDLE mon)
{
    *mon = (HANDLE)1;
    return &monitor;
}
