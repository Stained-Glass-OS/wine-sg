/* The netport gate's program (patches/sg/1029): makes, reads and deletes
 * Standard TCP/IP ports through the monitor's Xcv interface, as printer
 * makers' installers and Windows' Add Printer do.  Our own code.
 *   netport-probe add NAME raw|lpr HOST PORT [QUEUE]
 *   netport-probe get NAME
 *   netport-probe delete NAME
 *   netport-probe ports */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <tcpxcv.h>

static HANDLE open_xcv( const WCHAR *object )
{
    PRINTER_DEFAULTSW defaults = { NULL, NULL, SERVER_ACCESS_ADMINISTER };
    WCHAR name[MAX_PATH];
    HANDLE h;

    swprintf( name, MAX_PATH, L",%ls", object );
    if (!OpenPrinterW( name, &h, &defaults )) return NULL;
    return h;
}

int wmain( int argc, WCHAR **argv )
{
    DWORD needed = 0, status = 0, count = 0, i;
    HANDLE h;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc >= 6 && !wcscmp( argv[1], L"add" ))
    {
        PORT_DATA_1 data;
        memset( &data, 0, sizeof(data) );
        lstrcpynW( data.sztPortName, argv[2], MAX_PORTNAME_LEN );
        data.dwVersion = 1;
        data.dwProtocol = !wcscmp( argv[3], L"lpr" ) ? PROTOCOL_LPR_TYPE : PROTOCOL_RAWTCP_TYPE;
        data.cbSize = sizeof(data);
        lstrcpynW( data.sztHostAddress, argv[4], MAX_NETWORKNAME_LEN );
        data.dwPortNumber = _wtoi( argv[5] );
        if (argc > 6) lstrcpynW( data.sztQueue, argv[6], MAX_QUEUENAME_LEN );
        if (!(h = open_xcv( L"XcvMonitor Standard TCP/IP Port" ))) { printf( "xcv open %lu\n", GetLastError() ); return 1; }
        if (!XcvDataW( h, L"AddPort", (BYTE *)&data, sizeof(data), NULL, 0, &needed, &status ))
            printf( "addport failed %lu\n", GetLastError() );
        else printf( "addport %lu\n", status );
        ClosePrinter( h );
        return 0;
    }
    if (argc >= 3 && !wcscmp( argv[1], L"get" ))
    {
        CONFIG_INFO_DATA_1 in;
        PORT_DATA_1 out;
        memset( &in, 0, sizeof(in) );
        in.dwVersion = 1;
        WCHAR object[MAX_PATH];
        swprintf( object, MAX_PATH, L"XcvPort %ls", argv[2] );
        if (!(h = open_xcv( object ))) { printf( "xcv open %lu\n", GetLastError() ); return 1; }
        memset( &out, 0, sizeof(out) );
        if (XcvDataW( h, L"GetConfigInfo", (BYTE *)&in, sizeof(in), (BYTE *)&out, sizeof(out), &needed, &status ) &&
            !status)
            printf( "config %ls protocol %lu host %ls port %lu queue %ls\n", out.sztPortName, out.dwProtocol,
                    out.sztHostAddress, out.dwPortNumber, out.sztQueue );
        else printf( "getconfig %lu\n", status );
        ClosePrinter( h );
        return 0;
    }
    if (argc >= 3 && !wcscmp( argv[1], L"delete" ))
    {
        DELETE_PORT_DATA_1 data;
        memset( &data, 0, sizeof(data) );
        lstrcpynW( data.psztPortName, argv[2], MAX_PORTNAME_LEN );
        data.dwVersion = 1;
        if (!(h = open_xcv( L"XcvMonitor Standard TCP/IP Port" ))) { printf( "xcv open %lu\n", GetLastError() ); return 1; }
        XcvDataW( h, L"DeletePort", (BYTE *)&data, sizeof(data), NULL, 0, &needed, &status );
        printf( "deleteport %lu\n", status );
        ClosePrinter( h );
        return 0;
    }
    if (argc >= 2 && !wcscmp( argv[1], L"ports" ))
    {
        PORT_INFO_2W *ports;
        EnumPortsW( NULL, 2, NULL, 0, &needed, &count );
        ports = malloc( needed );
        if (EnumPortsW( NULL, 2, (BYTE *)ports, needed, &needed, &count ))
            for (i = 0; i < count; i++)
                printf( "port %ls|%ls\n", ports[i].pPortName, ports[i].pMonitorName ? ports[i].pMonitorName : L"" );
        return 0;
    }
    return 1;
}
