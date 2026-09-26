/* rasapi32 on sg-netctl (patches/sg/0400): what a program sees of the VPN
 * connections -- RasEnumEntries, RasEnumConnections with the caller's
 * structure size, RasGetConnectStatus, RasDialA with a password. */
#include <windows.h>
#include <ras.h>
#include <raserror.h>
#include <stdio.h>

int main( int argc, char **argv )
{
    RASENTRYNAMEW names[8];
    RASCONNW conns[8];
    RASCONNA conna[8];
    RASCONNSTATUSW st = { sizeof(st) };
    DWORD size, count, ret, i;

    if (argc > 1 && !strcmp( argv[1], "dial" ))
    {
        RASDIALPARAMSA p = { sizeof(p) };
        HRASCONN h = NULL;
        strcpy( p.szEntryName, argv[2] );
        if (argc > 3) strcpy( p.szPassword, argv[3] );
        ret = RasDialA( NULL, NULL, &p, 0, NULL, &h );
        printf( "dial=%lu handle=%s\n", ret, h ? "yes" : "no" );
        return 0;
    }
    size = 0; count = 99;
    ret = RasEnumEntriesW( NULL, NULL, NULL, &size, &count );
    printf( "entries-size-query=%lu size=%lu count=%lu\n", ret, size, count );
    names[0].dwSize = sizeof(names[0]);
    size = sizeof(names);
    ret = RasEnumEntriesW( NULL, NULL, names, &size, &count );
    printf( "entries=%lu count=%lu", ret, count );
    for (i = 0; i < count && !ret; i++) printf( " [%ls]", names[i].szEntryName );
    printf( "\n" );
    conns[0].dwSize = 123;
    size = sizeof(conns);
    printf( "badsize=%lu\n", RasEnumConnectionsW( conns, &size, &count ) );
    conns[0].dwSize = sizeof(conns[0]);
    size = sizeof(conns);
    ret = RasEnumConnectionsW( conns, &size, &count );
    printf( "connections=%lu count=%lu", ret, count );
    for (i = 0; i < count && !ret; i++)
    {
        printf( " [%ls|%ls|%ls]", conns[i].szEntryName, conns[i].szDeviceType, conns[i].szDeviceName );
        ret = RasGetConnectStatusW( conns[i].hrasconn, &st );
        printf( " status=%lu state=%s", ret, st.rasconnstate == RASCS_Connected ? "connected" : "other" );
    }
    printf( "\n" );
    conna[0].dwSize = sizeof(conna[0]);
    size = sizeof(conna);
    ret = RasEnumConnectionsA( conna, &size, &count );
    printf( "connectionsA=%lu count=%lu%s%s\n", ret, count, count ? " " : "", count ? conna[0].szEntryName : "" );
    printf( "status-bogus=%lu\n", RasGetConnectStatusW( (HRASCONN)0x1234, &st ) );
    return 0;
}
