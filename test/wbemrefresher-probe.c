/* wbemrefresher-probe.c (wine-sg 6103): the WMI refresher (CLSID_WbemRefresher): IWbemConfigureRefresher
 * AddEnum on Win32_Process, IWbemHiPerfEnum GetObjects, IWbemObjectAccess handles, Refresh; and
 * AddObjectByPath on a process, refreshed in place. KEY=VALUE lines.
 *   wbemrefresher-probe.exe            -- the test
 *   wbemrefresher-probe.exe child      -- sleeps 30 s (the process the test watches) */
#define COBJMACROS
#include <windows.h>
#include <wbemcli.h>
#include <stdio.h>
#include <string.h>

static const CLSID clsid_refresher = {0xc71566f2,0x561e,0x11d1,{0xad,0x87,0x00,0xc0,0x4f,0xd8,0xfd,0xff}};

static int find_pid( IWbemHiPerfEnum *he, DWORD pid, ULONG *total )
{
    IWbemObjectAccess *objs[512];
    ULONG got = 0, i;
    LONG handle;
    CIMTYPE type;
    int found = 0;
    HRESULT hr = IWbemHiPerfEnum_GetObjects( he, 0, 512, objs, &got );

    *total = got;
    if (FAILED(hr) || !got) return -1;
    if (FAILED(IWbemObjectAccess_GetPropertyHandle( objs[0], L"ProcessId", &type, &handle ))) found = -2;
    for (i = 0; i < got; i++)
    {
        DWORD value = 0;
        if (found >= 0 && SUCCEEDED(IWbemObjectAccess_ReadDWORD( objs[i], handle, &value )) && value == pid) found = 1;
        IWbemObjectAccess_Release( objs[i] );
    }
    return found;
}

int main( int argc, char **argv )
{
    IWbemLocator *locator;
    IWbemServices *services;
    IWbemRefresher *refresher;
    IWbemConfigureRefresher *config;
    IWbemHiPerfEnum *he;
    IWbemClassObject *obj;
    IWbemObjectAccess *access;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 16];
    WCHAR path[64];
    BSTR ns;
    LONG id, handle;
    CIMTYPE type;
    ULONG total;
    HRESULT hr;
    DWORD value;
    UINT64 qw;
    VARIANT var;

    if (argc > 1 && !strcmp( argv[1], "child" )) { Sleep( 30000 ); return 0; }
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    CoCreateInstance( &CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&locator );
    ns = SysAllocString( L"ROOT\\CIMV2" );
    hr = IWbemLocator_ConnectServer( locator, ns, NULL, NULL, NULL, 0, NULL, NULL, &services );
    printf( "connect=%#lx\n", hr );
    hr = CoCreateInstance( &clsid_refresher, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemRefresher, (void **)&refresher );
    printf( "refresher=%#lx\n", hr );
    if (FAILED(hr)) { printf( "done=1\n" ); return 0; }
    hr = IWbemRefresher_QueryInterface( refresher, &IID_IWbemConfigureRefresher, (void **)&config );
    printf( "configure=%#lx\n", hr );

    hr = IWbemConfigureRefresher_AddEnum( config, services, L"Win32_Process", 0, NULL, &he, &id );
    printf( "addenum=%#lx\n", hr );
    if (FAILED(hr)) { printf( "done=1\n" ); return 0; }

    GetModuleFileNameA( NULL, cmd, MAX_PATH );
    strcat( cmd, " child" );
    CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi );
    printf( "before_refresh=%d\n", find_pid( he, pi.dwProcessId, &total ) );
    printf( "refresh=%#lx\n", IWbemRefresher_Refresh( refresher, 0 ) );
    printf( "after_refresh=%d\n", find_pid( he, pi.dwProcessId, &total ) );
    printf( "count=%d\n", total > 1 );

    /* the child itself, by path, refreshed in place */
    swprintf( path, ARRAYSIZE(path), L"Win32_Process.Handle=\"%lu\"", pi.dwProcessId );
    hr = IWbemConfigureRefresher_AddObjectByPath( config, services, path, 0, NULL, &obj, &id );
    printf( "addobject=%#lx\n", hr );
    if (SUCCEEDED(hr))
    {
        hr = IWbemClassObject_QueryInterface( obj, &IID_IWbemObjectAccess, (void **)&access );
        printf( "access=%#lx\n", hr );
        value = 0;
        if (SUCCEEDED(hr) && SUCCEEDED(IWbemObjectAccess_GetPropertyHandle( access, L"ProcessId", &type, &handle )))
            IWbemObjectAccess_ReadDWORD( access, handle, &value );
        printf( "object_pid=%d\n", value == pi.dwProcessId );
        qw = 0;
        if (SUCCEEDED(hr) && SUCCEEDED(IWbemObjectAccess_GetPropertyHandle( access, L"WorkingSetSize", &type, &handle )))
            IWbemObjectAccess_ReadQWORD( access, handle, &qw );
        printf( "qword=%d\n", qw > 0 );
        printf( "object_refresh=%#lx\n", IWbemRefresher_Refresh( refresher, 0 ) );
        VariantInit( &var );
        IWbemClassObject_Get( obj, L"Handle", 0, &var, NULL, NULL );
        printf( "object_after=%d\n", V_VT( &var ) == VT_BSTR && _wtoi( V_BSTR( &var ) ) == (int)pi.dwProcessId );
        VariantClear( &var );
        IWbemConfigureRefresher_Remove( config, id, 0 );
    }

    TerminateProcess( pi.hProcess, 0 );
    WaitForSingleObject( pi.hProcess, 5000 );
    IWbemRefresher_Refresh( refresher, 0 );
    printf( "after_exit=%d\n", find_pid( he, pi.dwProcessId, &total ) );
    printf( "done=1\n" );
    return 0;
}
