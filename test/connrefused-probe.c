/* connrefused-probe: a refused TCP connection takes Windows' time (patches/sg/6527).
 *
 * Windows' TCP answers a reset to its SYN by trying twice more (half a second, then a second), so
 * a closed port takes about one and a half seconds to refuse, and a server that starts listening
 * meanwhile is connected to. Checks a blocking connect, a non-blocking one with select(), an
 * overlapped ConnectEx, a listener that starts 0.3 s and 1.2 s after the connect, and that an
 * open port still connects at once.
 *
 * Prints name=value lines; see test/connrefused-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <stdio.h>

static unsigned short free_port( void )
{
    struct sockaddr_in addr = { AF_INET };
    int len = sizeof(addr);
    SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );

    addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    bind( s, (struct sockaddr *)&addr, sizeof(addr) );
    getsockname( s, (struct sockaddr *)&addr, &len );
    closesocket( s );
    return addr.sin_port;
}

static struct sockaddr_in loopback( unsigned short port )
{
    struct sockaddr_in addr = { AF_INET };
    addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    addr.sin_port = port;
    return addr;
}

static SOCKET listen_on( unsigned short port )
{
    struct sockaddr_in addr = loopback( port );
    SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );

    if (bind( s, (struct sockaddr *)&addr, sizeof(addr) ) || listen( s, 5 )) printf( "listen failed %d\n", WSAGetLastError() );
    return s;
}

struct late { unsigned short port; DWORD delay; SOCKET s; };

static DWORD WINAPI late_listener( void *arg )
{
    struct late *late = arg;
    Sleep( late->delay );
    late->s = listen_on( late->port );
    return 0;
}

static double now( void )
{
    LARGE_INTEGER c, f;
    QueryPerformanceCounter( &c );
    QueryPerformanceFrequency( &f );
    return (double)c.QuadPart / f.QuadPart;
}

static void blocking_refused( void )
{
    struct sockaddr_in addr = loopback( free_port() );
    SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );
    double t = now();
    int r = connect( s, (struct sockaddr *)&addr, sizeof(addr) ), err = WSAGetLastError();

    t = now() - t;
    printf( "blocking=%d ret=%d err=%d time=%.2f\n", r == SOCKET_ERROR && err == WSAECONNREFUSED && t >= 1.2 && t < 4.0, r, err, t );
    closesocket( s );
}

static void nonblocking_refused( void )
{
    struct sockaddr_in addr = loopback( free_port() );
    SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );
    u_long one = 1;
    fd_set wr, ex;
    struct timeval tv = { 10, 0 };
    int r, err, soerr = -1, len = sizeof(soerr), n;
    double t = now();

    ioctlsocket( s, FIONBIO, &one );
    r = connect( s, (struct sockaddr *)&addr, sizeof(addr) );
    err = WSAGetLastError();
    FD_ZERO( &wr ); FD_ZERO( &ex ); FD_SET( s, &wr ); FD_SET( s, &ex );
    n = select( 0, NULL, &wr, &ex, &tv );
    t = now() - t;
    getsockopt( s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &len );
    printf( "nonblocking=%d ret=%d err=%d select=%d except=%d soerr=%d time=%.2f\n",
            r == SOCKET_ERROR && err == WSAEWOULDBLOCK && n == 1 && FD_ISSET( s, &ex ) && soerr == WSAECONNREFUSED &&
            t >= 1.2 && t < 4.0, r, err, n, FD_ISSET( s, &ex ), soerr, t );
    closesocket( s );
}

static void connectex_refused( void )
{
    GUID guid = WSAID_CONNECTEX;
    LPFN_CONNECTEX pConnectEx = NULL;
    struct sockaddr_in addr = loopback( free_port() ), local = loopback( 0 );
    SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );
    OVERLAPPED ov = { 0 };
    DWORD size, sent = 0, flags;
    BOOL ok;
    int err;
    double t;

    WSAIoctl( s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pConnectEx, sizeof(pConnectEx), &size, NULL, NULL );
    bind( s, (struct sockaddr *)&local, sizeof(local) );
    ov.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
    t = now();
    ok = pConnectEx( s, (struct sockaddr *)&addr, sizeof(addr), NULL, 0, &sent, &ov );
    err = WSAGetLastError();
    if (!ok && err == WSA_IO_PENDING)
    {
        WaitForSingleObject( ov.hEvent, 10000 );
        ok = WSAGetOverlappedResult( s, &ov, &sent, FALSE, &flags );
        err = WSAGetLastError();
    }
    t = now() - t;
    printf( "connectex=%d ok=%d err=%d time=%.2f\n", !ok && err == WSAECONNREFUSED && t >= 1.2 && t < 4.0, ok, err, t );
    CloseHandle( ov.hEvent );
    closesocket( s );
}

static void late_server( const char *name, DWORD delay )
{
    struct late late = { free_port(), delay, INVALID_SOCKET };
    struct sockaddr_in addr = loopback( late.port );
    SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );
    HANDLE thread = CreateThread( NULL, 0, late_listener, &late, 0, NULL );
    double t = now();
    int r = connect( s, (struct sockaddr *)&addr, sizeof(addr) ), err = WSAGetLastError();

    t = now() - t;
    printf( "%s=%d ret=%d err=%d time=%.2f\n", name, r == 0 && t * 1000 >= delay, r, r ? err : 0, t );
    WaitForSingleObject( thread, INFINITE );
    CloseHandle( thread );
    closesocket( s );
    closesocket( late.s );
}

static void open_port( void )
{
    unsigned short port = free_port();
    SOCKET l = listen_on( port ), s = socket( AF_INET, SOCK_STREAM, 0 );
    struct sockaddr_in addr = loopback( port );
    double t = now();
    int r = connect( s, (struct sockaddr *)&addr, sizeof(addr) );

    t = now() - t;
    printf( "open_port=%d ret=%d time=%.3f\n", r == 0 && t < 0.3, r, t );
    closesocket( s );
    closesocket( l );
}

int main( void )
{
    WSADATA data;

    setvbuf( stdout, NULL, _IONBF, 0 );
    WSAStartup( MAKEWORD( 2, 2 ), &data );
    open_port();
    blocking_refused();
    nonblocking_refused();
    connectex_refused();
    late_server( "late_300ms", 300 );
    late_server( "late_1200ms", 1200 );
    printf( "done=1\n" );
    WSACleanup();
    return 0;
}
