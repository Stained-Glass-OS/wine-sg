/* tcnone-probe: traffic.dll without a traffic control subsystem (Stained Glass OS wine-sg 6065), for
 * test/tcnone-gate.sh. Prints KEY=VALUE lines. */
#include <windows.h>
#include <stdio.h>

typedef struct { void *notify, *add, *modify, *del; } funcs;
static void CALLBACK notify( HANDLE a, HANDLE b, ULONG c, HANDLE d, ULONG e, void *f ) {}

int main( void )
{
    HMODULE m = LoadLibraryA( "traffic.dll" );
    ULONG (WINAPI *reg)(ULONG, HANDLE, funcs *, HANDLE *) = (void *)GetProcAddress( m, "TcRegisterClient" );
    ULONG (WINAPI *dereg)(HANDLE) = (void *)GetProcAddress( m, "TcDeregisterClient" );
    ULONG (WINAPI *enumifc)(HANDLE, ULONG *, void *) = (void *)GetProcAddress( m, "TcEnumerateInterfaces" );
    ULONG (WINAPI *openifc)(WCHAR *, HANDLE, HANDLE, HANDLE *) = (void *)GetProcAddress( m, "TcOpenInterfaceW" );
    ULONG (WINAPI *queryflow)(WCHAR *, GUID *, ULONG *, void *) = (void *)GetProcAddress( m, "TcQueryFlowW" );
    ULONG (WINAPI *delflow)(HANDLE) = (void *)GetProcAddress( m, "TcDeleteFlow" );
    funcs f = { notify, NULL, NULL, NULL }, none = { NULL };
    HANDLE h = (HANDLE)0x55, ifc;
    ULONG size = 0;
    GUID g = { 0 };

    printf( "exports=%d\n", reg && dereg && enumifc && openifc && queryflow && delflow );
    printf( "RegisterNull=%lu\n", reg( 2, NULL, NULL, &h ) );
    printf( "RegisterNoNotify=%lu\n", reg( 2, NULL, &none, &h ) );
    {
        ULONG r = reg( 1, NULL, &f, &h );
        printf( "RegisterVersion=%lu %d\n", r, h == NULL );
        h = (HANDLE)0x55;
        r = reg( 2, NULL, &f, &h );
        printf( "Register=%lu %d\n", r, h == NULL );
    }
    printf( "Deregister=%lu\n", dereg( (HANDLE)0x1234 ) );
    printf( "EnumInterfaces=%lu\n", enumifc( (HANDLE)0x1234, &size, NULL ) );
    printf( "OpenInterface=%lu\n", openifc( (WCHAR *)L"eth0", (HANDLE)0x1234, NULL, &ifc ) );
    printf( "QueryFlow=%lu\n", queryflow( (WCHAR *)L"flow", &g, &size, NULL ) );
    printf( "QueryFlowNull=%lu\n", queryflow( NULL, &g, &size, NULL ) );
    printf( "DeleteFlow=%lu\n", delflow( (HANDLE)0x1234 ) );
    printf( "done=1\n" );
    return 0;
}
