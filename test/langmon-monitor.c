/* A language monitor of our own, for the netport gate (patches/sg/1029),
 * built as printer makers build theirs (MONITOR2 with OpenPortEx): it
 * opens the port through the port monitor it is given, wraps each job in
 * "<SGLM job N>" ... "</SGLM>", and tells the system the printer's state
 * (SetPort: toner low).  Like Kyocera's, it keeps a thread of its own
 * running in the DLL, so it must stay loaded (1034); it counts its
 * initializations in HKCU\Software\SG Test LM, value Inits.  It keeps the
 * MONITORINIT it is given and reads its own settings through it when a job
 * starts ("reg=ok" in its wrap), as Zebra's does.  Our own code. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <winsplp.h>

struct lm_port
{
    MONITOR2 *mon;
    HANDLE hport;
    WCHAR name[MAX_PATH];
};

static BOOL WINAPI lm_OpenPortEx( HANDLE hmon, HANDLE hmon_port, LPWSTR port_name, LPWSTR printer, PHANDLE handle,
                                  MONITOR2 *mon )
{
    struct lm_port *p = calloc( 1, sizeof(*p) );
    if (!p || !mon || !mon->pfnOpenPort || !mon->pfnOpenPort( hmon_port, port_name, &p->hport ))
    {
        free( p );
        return FALSE;
    }
    p->mon = mon;
    lstrcpynW( p->name, port_name, MAX_PATH );
    *handle = p;
    return TRUE;
}

static MONITORINIT *lm_init;  /* kept, as makers' monitors keep it (Zebra's) */

/* its own settings, through the spooler's registry functions */
static const char *lm_settings( void )
{
    WCHAR driver[MAX_PATH];
    DWORD type, size = sizeof(driver);

    if (!lm_init || !lm_init->pMonitorReg || !lm_init->pMonitorReg->fpQueryValue) return "none";
    if (lm_init->pMonitorReg->fpQueryValue( lm_init->hckRegistryRoot, L"Driver", &type, (BYTE *)driver, &size,
                                            lm_init->hSpooler )) return "bad";
    return wcsstr( driver, L"sglm" ) || wcsstr( driver, L"SGLM" ) ? "ok" : "bad";
}

static BOOL WINAPI lm_StartDocPort( HANDLE h, LPWSTR printer, DWORD job, DWORD level, LPBYTE info )
{
    struct lm_port *p = h;
    char buf[64];
    DWORD w;

    if (!p->mon->pfnStartDocPort( p->hport, printer, job, level, info )) return FALSE;
    snprintf( buf, sizeof(buf), "<SGLM job %lu reg=%s>\n", job, lm_settings() );
    return p->mon->pfnWritePort( p->hport, (BYTE *)buf, strlen( buf ), &w );
}

static BOOL WINAPI lm_WritePort( HANDLE h, LPBYTE buf, DWORD size, LPDWORD written )
{
    struct lm_port *p = h;
    return p->mon->pfnWritePort( p->hport, buf, size, written );
}

static BOOL WINAPI lm_ReadPort( HANDLE h, LPBYTE buf, DWORD size, LPDWORD read )
{
    struct lm_port *p = h;
    *read = 0;
    return p->mon->pfnReadPort ? p->mon->pfnReadPort( p->hport, buf, size, read ) : TRUE;
}

static BOOL WINAPI lm_EndDocPort( HANDLE h )
{
    struct lm_port *p = h;
    PORT_INFO_3W pi3 = { PORT_STATUS_TONER_LOW, (WCHAR *)L"SG toner low", PORT_STATUS_TYPE_WARNING };
    DWORD w;

    p->mon->pfnWritePort( p->hport, (BYTE *)"</SGLM>\n", 8, &w );
    SetPortW( NULL, p->name, 3, (BYTE *)&pi3 );
    return p->mon->pfnEndDocPort( p->hport );
}

static BOOL WINAPI lm_ClosePort( HANDLE h )
{
    struct lm_port *p = h;
    p->mon->pfnClosePort( p->hport );
    free( p );
    return TRUE;
}

static void WINAPI lm_Shutdown( HANDLE hmon ) {}

static MONITOR2 lm =
{
    sizeof(MONITOR2), NULL, NULL, lm_OpenPortEx, lm_StartDocPort, lm_WritePort, lm_ReadPort, lm_EndDocPort,
    lm_ClosePort, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, lm_Shutdown,
};

static volatile LONG heartbeat;

static DWORD WINAPI watch_printer( void *arg )
{
    for (;;)
    {
        Sleep( 1 );
        InterlockedIncrement( &heartbeat );
    }
    return 0;
}

__declspec(dllexport) LPMONITOR2 WINAPI InitializePrintMonitor2( PMONITORINIT init, PHANDLE hmon )
{
    DWORD inits = 0, size = sizeof(inits);
    HKEY key;

    if (!RegCreateKeyExW( HKEY_CURRENT_USER, L"Software\\SG Test LM", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL ))
    {
        RegQueryValueExW( key, L"Inits", NULL, NULL, (BYTE *)&inits, &size );
        inits++;
        RegSetValueExW( key, L"Inits", 0, REG_DWORD, (BYTE *)&inits, sizeof(inits) );
        RegCloseKey( key );
    }
    CloseHandle( CreateThread( NULL, 0, watch_printer, NULL, 0, NULL ) );
    lm_init = init;
    *hmon = (HANDLE)&lm;
    return &lm;
}
