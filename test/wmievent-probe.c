/* wmievent-probe: WMI event queries (Stained Glass OS wine-sg 8002).
 *   wmievent-probe.exe          the checks (starts copies of itself as children)
 *   wmievent-probe.exe child N  sleeps N ms and exits (the process the events are about) */
#define COBJMACROS
#include <windows.h>
#include <wbemcli.h>
#include <stdio.h>

static int failures;
static IWbemServices *svc;
static WCHAR dir[MAX_PATH];

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static BOOL get_str( IWbemClassObject *obj, const WCHAR *name, WCHAR *out, int size )
{
    VARIANT v;
    BOOL ret = FALSE;
    VariantInit( &v );
    out[0] = 0;
    if (IWbemClassObject_Get( obj, name, 0, &v, NULL, NULL ) == S_OK && V_VT(&v) == VT_BSTR)
    {
        lstrcpynW( out, V_BSTR(&v), size );
        ret = TRUE;
    }
    VariantClear( &v );
    return ret;
}

static LONGLONG get_int( IWbemClassObject *obj, const WCHAR *name )
{
    VARIANT v;
    LONGLONG ret = -1;
    VariantInit( &v );
    if (IWbemClassObject_Get( obj, name, 0, &v, NULL, NULL ) == S_OK)
    {
        if (V_VT(&v) == VT_BSTR) ret = _wtoi64( V_BSTR(&v) );
        else if (SUCCEEDED(VariantChangeType( &v, &v, 0, VT_I8 ))) ret = V_I8(&v);
    }
    VariantClear( &v );
    return ret;
}

static IWbemClassObject *get_obj( IWbemClassObject *obj, const WCHAR *name )
{
    IWbemClassObject *ret = NULL;
    VARIANT v;
    VariantInit( &v );
    if (IWbemClassObject_Get( obj, name, 0, &v, NULL, NULL ) == S_OK && V_VT(&v) == VT_UNKNOWN && V_UNKNOWN(&v))
        IUnknown_QueryInterface( V_UNKNOWN(&v), &IID_IWbemClassObject, (void **)&ret );
    VariantClear( &v );
    return ret;
}

/* a copy of this program named name runs for ms milliseconds */
static DWORD start_child( const WCHAR *name, int ms )
{
    WCHAR self[MAX_PATH], path[MAX_PATH], cmd[2 * MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    GetModuleFileNameW( NULL, self, MAX_PATH );
    swprintf( path, MAX_PATH, L"%ls\\%ls", dir, name );
    CopyFileW( self, path, FALSE );
    swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" child %d", path, ms );
    if (!CreateProcessW( path, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return 0;
    CloseHandle( pi.hThread );
    CloseHandle( pi.hProcess );
    return pi.dwProcessId;
}

static HRESULT notification_query( const WCHAR *query, LONG flags, IEnumWbemClassObject **e )
{
    BSTR lang = SysAllocString( L"WQL" ), q = SysAllocString( query );
    HRESULT hr;
    *e = NULL;
    hr = IWbemServices_ExecNotificationQuery( svc, lang, q, flags, NULL, e );
    SysFreeString( lang );
    SysFreeString( q );
    return hr;
}

#define SEMISYNC (WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY)

static const GUID clsid_swbemsink = {0x75718c9a, 0xf029, 0x11d1, {0xa1, 0xac, 0x00, 0xc0, 0x4f, 0xb6, 0xc2, 0x23}};
static const GUID diid_sinkevents = {0x75718ca0, 0xf029, 0x11d1, {0xa1, 0xac, 0x00, 0xc0, 0x4f, 0xb6, 0xc2, 0x23}};

/* SWbemSink's connection point container and connection point enumerate */
static void test_sink_enums( void )
{
    IConnectionPointContainer *cpc = NULL;
    IEnumConnectionPoints *points = NULL;
    IEnumConnections *conns = NULL;
    IConnectionPoint *cp = NULL;
    IDispatch *disp = NULL;
    CONNECTDATA cd;
    ULONG got = 0;
    DWORD cookie = 0;
    IID iid;
    HRESULT hr;

    hr = CoCreateInstance( &clsid_swbemsink, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&disp );
    check( hr == S_OK, "SWbemSink", hr );
    if (hr != S_OK) return;
    IDispatch_QueryInterface( disp, &IID_IConnectionPointContainer, (void **)&cpc );
    hr = cpc ? IConnectionPointContainer_EnumConnectionPoints( cpc, &points ) : E_FAIL;
    check( hr == S_OK && points, "EnumConnectionPoints", hr );
    if (points)
    {
        hr = IEnumConnectionPoints_Next( points, 1, &cp, &got );
        check( hr == S_OK && got == 1 && cp, "one connection point", hr );
        if (cp)
        {
            IConnectionPoint_GetConnectionInterface( cp, &iid );
            check( IsEqualGUID( &iid, &diid_sinkevents ), "its interface is ISWbemSinkEvents", 0 );
        }
        hr = IEnumConnectionPoints_Next( points, 1, (IConnectionPoint **)&conns, &got );
        check( hr == S_FALSE && !got, "and no other", hr );
        conns = NULL;
        IEnumConnectionPoints_Release( points );
    }
    if (cp)
    {
        hr = IConnectionPoint_Advise( cp, (IUnknown *)disp, &cookie );
        check( hr == S_OK && cookie, "Advise", hr );
        hr = IConnectionPoint_EnumConnections( cp, &conns );
        check( hr == S_OK && conns, "EnumConnections", hr );
        if (conns)
        {
            hr = IEnumConnections_Next( conns, 1, &cd, &got );
            check( hr == S_OK && got == 1 && cd.dwCookie == cookie && cd.pUnk, "the connection, by its cookie", hr );
            if (got) IUnknown_Release( cd.pUnk );
            IEnumConnections_Release( conns );
        }
        IConnectionPoint_Unadvise( cp, cookie );
        IConnectionPoint_Release( cp );
    }
    if (cpc) IConnectionPointContainer_Release( cpc );
    IDispatch_Release( disp );
}

/* an IWbemObjectSink of ours for the async query */
struct sink
{
    IWbemObjectSink IWbemObjectSink_iface;
    LONG refs, indicated;
    DWORD pid;
    HRESULT status;
    WCHAR class[64];
    HANDLE got;
};

static struct sink *impl_from_sink( IWbemObjectSink *iface )
{
    return CONTAINING_RECORD( iface, struct sink, IWbemObjectSink_iface );
}

static HRESULT WINAPI sink_QueryInterface( IWbemObjectSink *iface, REFIID riid, void **obj )
{
    if (IsEqualGUID( riid, &IID_IUnknown ) || IsEqualGUID( riid, &IID_IWbemObjectSink ))
    {
        *obj = iface;
        IWbemObjectSink_AddRef( iface );
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef( IWbemObjectSink *iface ) { return InterlockedIncrement( &impl_from_sink( iface )->refs ); }
static ULONG WINAPI sink_Release( IWbemObjectSink *iface ) { return InterlockedDecrement( &impl_from_sink( iface )->refs ); }
static HRESULT WINAPI sink_Indicate( IWbemObjectSink *iface, LONG count, IWbemClassObject **objs )
{
    struct sink *s = impl_from_sink( iface );
    LONG i;
    for (i = 0; i < count; i++)
    {
        if (!s->indicated)
        {
            get_str( objs[i], L"__CLASS", s->class, 64 );
            s->pid = (DWORD)get_int( objs[i], L"ProcessID" );
        }
        InterlockedIncrement( &s->indicated );
    }
    SetEvent( s->got );
    return S_OK;
}
static HRESULT WINAPI sink_SetStatus( IWbemObjectSink *iface, LONG flags, HRESULT hr, BSTR str, IWbemClassObject *obj )
{
    impl_from_sink( iface )->status = hr;
    return S_OK;
}
static const IWbemObjectSinkVtbl sink_vtbl = { sink_QueryInterface, sink_AddRef, sink_Release, sink_Indicate, sink_SetStatus };

int wmain( int argc, WCHAR **argv )
{
    IWbemLocator *loc;
    IEnumWbemClassObject *e, *e2;
    IWbemClassObject *ev = NULL, *target;
    struct sink sink = { { &sink_vtbl }, 1 };
    WCHAR name[64], *p;
    BSTR ns, lang, q;
    ULONG n = 0;
    DWORD pid, pid2;
    HRESULT hr;

    if (argc >= 3 && !wcscmp( argv[1], L"child" ))
    {
        Sleep( _wtoi( argv[2] ) );
        return 0;
    }
    GetModuleFileNameW( NULL, dir, MAX_PATH );
    if ((p = wcsrchr( dir, '\\' ))) *p = 0;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    CoCreateInstance( &CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc );
    ns = SysAllocString( L"ROOT\\CIMV2" );
    hr = IWbemLocator_ConnectServer( loc, ns, NULL, NULL, NULL, 0, NULL, NULL, &svc );
    check( hr == S_OK, "ConnectServer", hr );
    if (hr != S_OK) return 1;

    /* the documented flags */
    hr = notification_query( L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'", 0, &e );
    check( hr == WBEM_E_INVALID_PARAMETER, "ExecNotificationQuery without RETURN_IMMEDIATELY|FORWARD_ONLY: WBEM_E_INVALID_PARAMETER", hr );
    hr = notification_query( L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'NoSuchClass'", SEMISYNC, &e );
    check( hr == WBEM_E_INVALID_CLASS, "TargetInstance ISA an unknown class: WBEM_E_INVALID_CLASS", hr );

    /* creation, filtered by the target's name */
    hr = notification_query( L"SELECT * FROM __InstanceCreationEvent WITHIN 0.5 WHERE TargetInstance ISA 'Win32_Process' "
                             L"AND TargetInstance.Name = 'sgwmichild.exe'", SEMISYNC, &e );
    check( hr == S_OK && e, "a creation event query on Win32_Process (semisynchronous)", hr );
    hr = notification_query( L"SELECT * FROM __InstanceDeletionEvent WITHIN 0.5 WHERE TargetInstance ISA \"Win32_Process\" "
                             L"AND TargetInstance.Name = 'sgwmichild.exe'", SEMISYNC, &e2 );
    check( hr == S_OK && e2, "a deletion event query", hr );
    if (!e || !e2) goto done;

    hr = IEnumWbemClassObject_Next( e, 1500, 1, &ev, &n );
    check( hr == WBEM_S_TIMEDOUT && !n, "nothing happened: Next times out (WBEM_S_TIMEDOUT)", hr );

    pid2 = start_child( L"sgwmiother.exe", 500 );
    pid = start_child( L"sgwmichild.exe", 2500 );
    check( pid && pid2, "two child processes started", pid );
    hr = IEnumWbemClassObject_Next( e, 6000, 1, &ev, &n );
    check( hr == S_OK && n == 1, "a creation event came", hr );
    if (n)
    {
        get_str( ev, L"__CLASS", name, 64 );
        check( !lstrcmpiW( name, L"__InstanceCreationEvent" ), "its class is __InstanceCreationEvent", 0 );
        check( get_int( ev, L"TIME_CREATED" ) > 0, "TIME_CREATED is set", 0 );
        target = get_obj( ev, L"TargetInstance" );
        check( target != NULL, "TargetInstance is an object", 0 );
        if (target)
        {
            get_str( target, L"Name", name, 64 );
            check( !lstrcmpiW( name, L"sgwmichild.exe" ), "TargetInstance.Name is the child's", 0 );
            check( get_int( target, L"ProcessId" ) == pid, "TargetInstance.ProcessId is the child's", (long)get_int( target, L"ProcessId" ) );
            IWbemClassObject_Release( target );
        }
        IWbemClassObject_Release( ev );
    }
    hr = IEnumWbemClassObject_Next( e, 1500, 1, &ev, &n );
    check( hr == WBEM_S_TIMEDOUT && !n, "the other process (another name) made no event", hr );
    if (n) IWbemClassObject_Release( ev );

    hr = IEnumWbemClassObject_Next( e2, 8000, 1, &ev, &n );
    check( hr == S_OK && n == 1, "the child's exit made a deletion event", hr );
    if (n)
    {
        target = get_obj( ev, L"TargetInstance" );
        check( target && get_int( target, L"ProcessId" ) == pid, "its TargetInstance is the child that ended", 0 );
        if (target) IWbemClassObject_Release( target );
        IWbemClassObject_Release( ev );
    }
    IEnumWbemClassObject_Release( e );
    IEnumWbemClassObject_Release( e2 );

    /* async: Win32_ProcessStartTrace to our sink, until cancelled */
    sink.got = CreateEventW( NULL, FALSE, FALSE, NULL );
    lang = SysAllocString( L"WQL" );
    q = SysAllocString( L"SELECT * FROM Win32_ProcessStartTrace WHERE ProcessName = 'sgwmichild.exe'" );
    hr = IWbemServices_ExecNotificationQueryAsync( svc, lang, q, 0, NULL, &sink.IWbemObjectSink_iface );
    check( hr == S_OK, "ExecNotificationQueryAsync of Win32_ProcessStartTrace", hr );
    start_child( L"sgwmiother.exe", 1000 );
    pid = start_child( L"sgwmichild.exe", 1000 );
    WaitForSingleObject( sink.got, 6000 );
    check( sink.indicated == 1, "the sink was given one event (the matching process)", sink.indicated );
    check( !lstrcmpiW( sink.class, L"Win32_ProcessStartTrace" ), "of class Win32_ProcessStartTrace", 0 );
    check( sink.pid == pid, "its ProcessID is the child's", sink.pid );
    hr = IWbemServices_CancelAsyncCall( svc, &sink.IWbemObjectSink_iface );
    check( hr == S_OK, "CancelAsyncCall", hr );
    check( sink.status == WBEM_E_CALL_CANCELLED, "the sink is told WBEM_E_CALL_CANCELLED", sink.status );
    check( sink.refs == 1, "the sink's references are given back", sink.refs );
    Sleep( 500 );
    start_child( L"sgwmichild.exe", 1000 );
    Sleep( 2500 );
    check( sink.indicated == 1, "no event after the cancel", sink.indicated );
    SysFreeString( lang );
    SysFreeString( q );

    test_sink_enums();

done:
    IWbemServices_Release( svc );
    IWbemLocator_Release( loc );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
