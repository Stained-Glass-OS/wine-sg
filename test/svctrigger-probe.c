/* Probe for service trigger info (patches/sg/0520): ChangeServiceConfig2 and
 * QueryServiceConfig2 with SERVICE_CONFIG_TRIGGER_INFO, as Microsoft
 * OneDrive's installer uses for its "OneDrive Updater Service" (Wine's RPC
 * had no arm for level 8: RPC_S_INVALID_TAG, and the install failed with
 * 0x800706c5).  One "key value" line per check. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* Windows SDK winsvc.h layout; mingw-w64 has the constants but not the structures */
#ifndef SERVICE_CONFIG_TRIGGER_INFO
#define SERVICE_CONFIG_TRIGGER_INFO 8
#endif
typedef struct { DWORD dwDataType; DWORD cbData; BYTE *pData; } SG_TRIGGER_ITEM;
typedef struct { DWORD dwTriggerType; DWORD dwAction; GUID *pTriggerSubtype; DWORD cDataItems;
                 SG_TRIGGER_ITEM *pDataItems; } SG_TRIGGER;
typedef struct { DWORD cTriggers; SG_TRIGGER *pTriggers; BYTE *pReserved; } SG_TRIGGER_INFO;
#define SERVICE_TRIGGER_SPECIFIC_DATA_ITEM SG_TRIGGER_ITEM
#define SERVICE_TRIGGER SG_TRIGGER
#define SERVICE_TRIGGER_INFO SG_TRIGGER_INFO
#ifndef SERVICE_TRIGGER_ACTION_SERVICE_START
#define SERVICE_TRIGGER_ACTION_SERVICE_START 1
#define SERVICE_TRIGGER_ACTION_SERVICE_STOP 2
#endif

static const GUID ip_arrival = { 0x4f27f2de, 0x14e2, 0x430b, { 0xa5, 0x49, 0x7c, 0xd4, 0x8c, 0xbc, 0x82, 0x45 } };
static const GUID custom_guid = { 0x12345678, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } };

static SERVICE_TRIGGER_INFO *query( SC_HANDLE svc, DWORD *needed_out, DWORD *first_err )
{
    DWORD needed = 0;
    BOOL r = QueryServiceConfig2W( svc, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &needed );
    SERVICE_TRIGGER_INFO *info;

    *first_err = r ? 0 : GetLastError();
    *needed_out = needed;
    if (!needed) return NULL;
    info = malloc( needed );
    if (!QueryServiceConfig2W( svc, SERVICE_CONFIG_TRIGGER_INFO, (BYTE *)info, needed, &needed ))
    {
        printf( "queryfail %lu\n", GetLastError() );
        free( info );
        return NULL;
    }
    return info;
}

int main(void)
{
    SC_HANDLE scm, svc;
    SERVICE_TRIGGER triggers[2];
    SERVICE_TRIGGER_SPECIFIC_DATA_ITEM items[2];
    SERVICE_TRIGGER_INFO set = { 0 }, *info;
    BYTE bin[3] = { 0xa1, 0xb2, 0xc3 };
    WCHAR str[] = L"abc\0";   /* multi-sz: abc\0\0 */
    DWORD needed, err;
    HKEY key;

    scm = OpenSCManagerW( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if (!scm) { printf( "scm %lu\n", GetLastError() ); return 0; }
    svc = CreateServiceW( scm, L"sgtrigtest", L"SG trigger test", SERVICE_ALL_ACCESS,
                          SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
                          L"C:\\windows\\system32\\sgnothere.exe", NULL, NULL, NULL, NULL, NULL );
    if (!svc) { printf( "create %lu\n", GetLastError() ); return 0; }

    /* no triggers yet: just the structure */
    info = query( svc, &needed, &err );
    printf( "empty %lu %d %lu %d\n", err, needed == sizeof(SERVICE_TRIGGER_INFO),
            info ? info->cTriggers : 99, info ? info->pTriggers == NULL : -1 );
    free( info );

    memset( triggers, 0, sizeof(triggers) );
    triggers[0].dwTriggerType = SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY;
    triggers[0].dwAction = SERVICE_TRIGGER_ACTION_SERVICE_START;
    triggers[0].pTriggerSubtype = (GUID *)&ip_arrival;
    items[0].dwDataType = SERVICE_TRIGGER_DATA_TYPE_BINARY;
    items[0].cbData = sizeof(bin);
    items[0].pData = bin;
    items[1].dwDataType = SERVICE_TRIGGER_DATA_TYPE_STRING;
    items[1].cbData = sizeof(str);
    items[1].pData = (BYTE *)str;
    triggers[1].dwTriggerType = SERVICE_TRIGGER_TYPE_CUSTOM;
    triggers[1].dwAction = SERVICE_TRIGGER_ACTION_SERVICE_STOP;
    triggers[1].pTriggerSubtype = (GUID *)&custom_guid;
    triggers[1].cDataItems = 2;
    triggers[1].pDataItems = items;
    set.cTriggers = 2;
    set.pTriggers = triggers;
    printf( "set %d\n", ChangeServiceConfig2W( svc, SERVICE_CONFIG_TRIGGER_INFO, &set ) ? 0 : (int)GetLastError() );

    info = query( svc, &needed, &err );
    if (info)
    {
        SERVICE_TRIGGER *t = info->pTriggers;
        printf( "count %lu %lu\n", err, info->cTriggers );
        printf( "t0 %lu %lu %d %lu\n", t[0].dwTriggerType, t[0].dwAction,
                t[0].pTriggerSubtype && IsEqualGUID( t[0].pTriggerSubtype, &ip_arrival ), t[0].cDataItems );
        printf( "t1 %lu %lu %d %lu\n", t[1].dwTriggerType, t[1].dwAction,
                t[1].pTriggerSubtype && IsEqualGUID( t[1].pTriggerSubtype, &custom_guid ), t[1].cDataItems );
        printf( "d0 %lu %lu %d\n", t[1].pDataItems[0].dwDataType, t[1].pDataItems[0].cbData,
                !memcmp( t[1].pDataItems[0].pData, bin, sizeof(bin) ) );
        printf( "d1 %lu %lu %d\n", t[1].pDataItems[1].dwDataType, t[1].pDataItems[1].cbData,
                !memcmp( t[1].pDataItems[1].pData, str, sizeof(str) ) );
        /* everything lies inside the caller's buffer */
        printf( "inside %d\n", (BYTE *)t[1].pDataItems[1].pData + t[1].pDataItems[1].cbData <= (BYTE *)info + needed );
        free( info );
    }

    /* the A form: string data in the A code page */
    {
        DWORD n = 0;
        BYTE *buf;
        QueryServiceConfig2A( svc, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &n );
        buf = malloc( n );
        if (QueryServiceConfig2A( svc, SERVICE_CONFIG_TRIGGER_INFO, buf, n, &n ))
        {
            SERVICE_TRIGGER_SPECIFIC_DATA_ITEM *d = &((SERVICE_TRIGGER_INFO *)buf)->pTriggers[1].pDataItems[1];
            printf( "ansi %lu %d\n", d->cbData, !memcmp( d->pData, "abc\0", 5 ) );
        }
        else printf( "ansi fail %lu\n", GetLastError() );
        free( buf );
    }

    /* where Windows keeps them */
    if (!RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\sgtrigtest\\TriggerInfo\\0",
                        0, KEY_READ, &key ))
    {
        DWORD type = 0, action = 0, len = sizeof(DWORD);
        GUID g;
        RegQueryValueExW( key, L"Type", NULL, NULL, (BYTE *)&type, &len );
        len = sizeof(DWORD);
        RegQueryValueExW( key, L"Action", NULL, NULL, (BYTE *)&action, &len );
        len = sizeof(g);
        RegQueryValueExW( key, L"GUID", NULL, NULL, (BYTE *)&g, &len );
        printf( "registry %lu %lu %d\n", type, action, IsEqualGUID( &g, &ip_arrival ) );
        RegCloseKey( key );
    }
    else printf( "registry none\n" );

    /* validation */
    triggers[0].dwAction = 3;
    SetLastError( 0 );
    printf( "badaction %lu\n", ChangeServiceConfig2W( svc, SERVICE_CONFIG_TRIGGER_INFO, &set ) ? 0 : GetLastError() );
    triggers[0].dwAction = SERVICE_TRIGGER_ACTION_SERVICE_START;
    triggers[0].dwTriggerType = 99;
    printf( "badtype %lu\n", ChangeServiceConfig2W( svc, SERVICE_CONFIG_TRIGGER_INFO, &set ) ? 0 : GetLastError() );
    triggers[0].dwTriggerType = SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY;

    /* the A form of set: string data converted */
    {
        SERVICE_TRIGGER_SPECIFIC_DATA_ITEM ia = { SERVICE_TRIGGER_DATA_TYPE_STRING, 4, (BYTE *)"xy\0" };
        SERVICE_TRIGGER ta = { SERVICE_TRIGGER_TYPE_CUSTOM, SERVICE_TRIGGER_ACTION_SERVICE_START, (GUID *)&custom_guid, 1, &ia };
        SERVICE_TRIGGER_INFO sa = { 1, &ta, NULL };
        BOOL r = ChangeServiceConfig2A( svc, SERVICE_CONFIG_TRIGGER_INFO, &sa );
        info = query( svc, &needed, &err );
        printf( "seta %d %lu %d\n", r, info ? info->pTriggers[0].pDataItems[0].cbData : 0,
                info && !memcmp( info->pTriggers[0].pDataItems[0].pData, L"xy\0", 8 ) );
        free( info );
    }

    /* none: removes them all */
    set.cTriggers = 0;
    set.pTriggers = NULL;
    ChangeServiceConfig2W( svc, SERVICE_CONFIG_TRIGGER_INFO, &set );
    info = query( svc, &needed, &err );
    printf( "cleared %lu %d\n", info ? info->cTriggers : 99,
            RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\sgtrigtest\\TriggerInfo",
                           0, KEY_READ, &key ) == ERROR_FILE_NOT_FOUND );
    free( info );

    DeleteService( svc );
    CloseServiceHandle( svc );
    CloseServiceHandle( scm );
    return 0;
}
