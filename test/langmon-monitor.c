/* A language monitor of our own, for the netport gate (patches/sg/1029),
 * built as printer makers build theirs (MONITOR2 with OpenPortEx): it
 * opens the port through the port monitor it is given, wraps each job in
 * "<SGLM job N>" ... "</SGLM>", and tells the system the printer's state
 * (SetPort: toner low).  Our own code. */
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

static BOOL WINAPI lm_StartDocPort( HANDLE h, LPWSTR printer, DWORD job, DWORD level, LPBYTE info )
{
    struct lm_port *p = h;
    char buf[64];
    DWORD w;

    if (!p->mon->pfnStartDocPort( p->hport, printer, job, level, info )) return FALSE;
    snprintf( buf, sizeof(buf), "<SGLM job %lu>\n", job );
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

__declspec(dllexport) LPMONITOR2 WINAPI InitializePrintMonitor2( PMONITORINIT init, PHANDLE hmon )
{
    *hmon = (HANDLE)&lm;
    return &lm;
}
