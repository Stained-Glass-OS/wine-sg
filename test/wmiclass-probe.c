/* wmiclass-probe: classes and instances an application defines in WMI (Stained Glass
 * OS wine-sg 6017): GetObject of no path, SpawnDerivedClass, Put of new properties
 * and __CLASS, a [key] qualifier, PutClass, PutInstance, queries, GetObject of an
 * instance path, DeleteInstance, DeleteClass; a second process sees what the first
 * stored.   wmiclass-probe.exe          (the test)
 *           wmiclass-probe.exe count    (prints the instance count, for the test) */
#define COBJMACROS
#include <windows.h>
#include <wbemcli.h>
#include <stdio.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static IWbemServices *svc;

static HRESULT put_str( IWbemClassObject *obj, const WCHAR *name, const WCHAR *value, CIMTYPE type )
{
    VARIANT v;
    HRESULT hr;
    if (value) { V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString( value ); }
    else V_VT(&v) = VT_NULL;
    hr = IWbemClassObject_Put( obj, name, 0, &v, type );
    VariantClear( &v );
    return hr;
}

static HRESULT put_int( IWbemClassObject *obj, const WCHAR *name, LONG value, CIMTYPE type )
{
    VARIANT v;
    V_VT(&v) = VT_I4; V_I4(&v) = value;
    return IWbemClassObject_Put( obj, name, 0, &v, type );
}

static BOOL get_str( IWbemClassObject *obj, const WCHAR *name, const WCHAR *expect )
{
    VARIANT v;
    BOOL ret;
    VariantInit( &v );
    if (IWbemClassObject_Get( obj, name, 0, &v, NULL, NULL ) != S_OK) return FALSE;
    if (!expect) ret = V_VT(&v) == VT_NULL;
    else ret = V_VT(&v) == VT_BSTR && !lstrcmpiW( V_BSTR(&v), expect );
    VariantClear( &v );
    return ret;
}

static LONG get_int( IWbemClassObject *obj, const WCHAR *name )
{
    VARIANT v;
    LONG ret = -999;
    VariantInit( &v );
    if (IWbemClassObject_Get( obj, name, 0, &v, NULL, NULL ) == S_OK && VariantChangeType( &v, &v, 0, VT_I4 ) == S_OK)
        ret = V_I4(&v);
    VariantClear( &v );
    return ret;
}

static int query_count( const WCHAR *wql, IWbemClassObject **first )
{
    IEnumWbemClassObject *e;
    IWbemClassObject *obj;
    BSTR lang = SysAllocString( L"WQL" ), q = SysAllocString( wql );
    ULONG n;
    int count = 0;
    HRESULT hr;

    if (first) *first = NULL;
    hr = IWbemServices_ExecQuery( svc, lang, q, WBEM_FLAG_FORWARD_ONLY, NULL, &e );
    SysFreeString( lang ); SysFreeString( q );
    if (hr != S_OK) return -1;
    while (IEnumWbemClassObject_Next( e, WBEM_INFINITE, 1, &obj, &n ) == S_OK && n)
    {
        if (!count && first) *first = obj;
        else IWbemClassObject_Release( obj );
        count++;
    }
    IEnumWbemClassObject_Release( e );
    return count;
}

static HRESULT get_object( const WCHAR *path, IWbemClassObject **obj )
{
    BSTR p = path ? SysAllocString( path ) : NULL;
    HRESULT hr = IWbemServices_GetObject( svc, p, 0, NULL, obj, NULL );
    SysFreeString( p );
    return hr;
}

int wmain( int argc, WCHAR **argv )
{
    IWbemLocator *loc;
    IWbemClassObject *cls = NULL, *child = NULL, *inst = NULL, *obj = NULL, *bad = NULL;
    IWbemQualifierSet *qs;
    BSTR ns;
    VARIANT v;
    HRESULT hr;
    int n;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    if (CoCreateInstance( &CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc ) != S_OK)
    {
        printf( "FAIL  no WbemLocator\nRESULT: FAIL\n" );
        return 1;
    }
    ns = SysAllocString( L"ROOT\\CIMV2" );
    hr = IWbemLocator_ConnectServer( loc, ns, NULL, NULL, NULL, 0, NULL, NULL, &svc );
    SysFreeString( ns );
    if (hr != S_OK)
    {
        printf( "FAIL  ConnectServer %#lx\nRESULT: FAIL\n", hr );
        return 1;
    }
    if (argc > 1 && !lstrcmpW( argv[1], L"count" ))
    {
        printf( "count %d\n", query_count( L"SELECT * FROM SG_GapTest", NULL ) );
        return 0;
    }

    /* a class of its own */
    hr = get_object( NULL, &cls );
    check( hr == S_OK && cls, "GetObject of no path: an empty class", hr );
    if (hr != S_OK) { printf( "RESULT: FAIL\n" ); return 1; }
    check( get_str( cls, L"__CLASS", NULL ), "its __CLASS is NULL", 0 );
    check( get_int( cls, L"__GENUS" ) == WBEM_GENUS_CLASS, "its __GENUS is a class", get_int( cls, L"__GENUS" ) );
    hr = IWbemServices_PutClass( svc, cls, 0, NULL, NULL );
    check( hr == WBEM_E_INVALID_OBJECT, "PutClass without __CLASS: WBEM_E_INVALID_OBJECT", hr );
    check( put_str( cls, L"__CLASS", L"SG_GapTest", 0 ) == S_OK, "Put __CLASS", 0 );
    check( get_str( cls, L"__CLASS", L"SG_GapTest" ), "and it reads back", 0 );
    hr = put_str( cls, L"Name", NULL, CIM_STRING );
    check( hr == S_OK, "Put a new property (Name, string, no value)", hr );
    hr = IWbemClassObject_GetPropertyQualifierSet( cls, L"Name", &qs );
    if (hr == S_OK)
    {
        V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
        hr = IWbemQualifierSet_Put( qs, L"key", &v, 0 );
        IWbemQualifierSet_Release( qs );
    }
    check( hr == S_OK, "[key] on Name", hr );
    check( put_int( cls, L"Count", 0, CIM_SINT32 ) == S_OK, "Put Count (sint32)", 0 );
    V_VT(&v) = VT_NULL;
    check( IWbemClassObject_Put( cls, L"Enabled", 0, &v, CIM_BOOLEAN ) == S_OK, "Put Enabled (boolean)", 0 );
    check( IWbemClassObject_Put( cls, L"NoType", 0, &v, 0 ) == WBEM_E_INVALID_PROPERTY_TYPE,
           "a new property without a type: WBEM_E_INVALID_PROPERTY_TYPE", 0 );
    hr = IWbemServices_PutClass( svc, cls, 0, NULL, NULL );
    check( hr == S_OK, "PutClass", hr );
    hr = IWbemServices_PutClass( svc, cls, WBEM_FLAG_CREATE_ONLY, NULL, NULL );
    check( hr == WBEM_E_ALREADY_EXISTS, "PutClass CREATE_ONLY again: WBEM_E_ALREADY_EXISTS", hr );
    IWbemClassObject_Release( cls );
    cls = NULL;

    hr = get_object( L"SG_GapTest", &cls );
    check( hr == S_OK && get_str( cls, L"__CLASS", L"SG_GapTest" ), "GetObject of the stored class", hr );
    if (hr != S_OK) { printf( "RESULT: FAIL\n" ); return 1; }
    hr = IWbemClassObject_GetPropertyQualifierSet( cls, L"Name", &qs );
    if (hr == S_OK)
    {
        VariantInit( &v );
        hr = IWbemQualifierSet_Get( qs, L"key", 0, &v, NULL );
        IWbemQualifierSet_Release( qs );
    }
    check( hr == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v), "Name is its key", hr );
    check( get_int( cls, L"__PROPERTY_COUNT" ) == 3, "three properties", get_int( cls, L"__PROPERTY_COUNT" ) );

    /* a class derived from it */
    check( IWbemClassObject_SpawnDerivedClass( cls, 1, &child ) == WBEM_E_INVALID_PARAMETER,
           "SpawnDerivedClass with a flag: WBEM_E_INVALID_PARAMETER", 0 );
    hr = IWbemClassObject_SpawnDerivedClass( cls, 0, &child );
    check( hr == S_OK && child, "SpawnDerivedClass", hr );
    if (hr == S_OK)
    {
        check( get_str( child, L"__SUPERCLASS", L"SG_GapTest" ), "its __SUPERCLASS", 0 );
        check( get_int( child, L"__PROPERTY_COUNT" ) == 3, "it has its superclass's properties",
               get_int( child, L"__PROPERTY_COUNT" ) );
        put_str( child, L"__CLASS", L"SG_GapChild", 0 );
        check( put_str( child, L"Extra", L"x", CIM_STRING ) == S_OK, "Put a property of its own", 0 );
        hr = IWbemServices_PutClass( svc, child, 0, NULL, NULL );
        check( hr == S_OK, "PutClass of the derived class", hr );
        IWbemClassObject_Release( child );
        child = NULL;
        hr = get_object( L"SG_GapChild", &child );
        check( hr == S_OK && get_str( child, L"__SUPERCLASS", L"SG_GapTest" ) &&
               get_str( child, L"__DYNASTY", L"SG_GapTest" ), "the stored derived class: superclass, dynasty", hr );
        if (child) IWbemClassObject_Release( child );
    }
    /* the system's classes stay */
    hr = IWbemClassObject_SpawnDerivedClass( cls, 0, &bad );
    if (hr == S_OK)
    {
        put_str( bad, L"__CLASS", L"Win32_Process", 0 );
        hr = IWbemServices_PutClass( svc, bad, 0, NULL, NULL );
        check( hr == WBEM_E_ACCESS_DENIED, "a class named like a system class: WBEM_E_ACCESS_DENIED", hr );
        IWbemClassObject_Release( bad );
    }

    /* instances */
    hr = IWbemClassObject_SpawnInstance( cls, 0, &inst );
    check( hr == S_OK, "SpawnInstance", hr );
    hr = IWbemServices_PutInstance( svc, inst, 0, NULL, NULL );
    check( hr == WBEM_E_ILLEGAL_NULL, "PutInstance without its key: WBEM_E_ILLEGAL_NULL", hr );
    put_str( inst, L"Name", L"alpha", 0 );
    put_int( inst, L"Count", 7, 0 );
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    IWbemClassObject_Put( inst, L"Enabled", 0, &v, 0 );
    hr = IWbemServices_PutInstance( svc, inst, 0, NULL, NULL );
    check( hr == S_OK, "PutInstance (alpha)", hr );
    hr = IWbemServices_PutInstance( svc, inst, WBEM_FLAG_CREATE_ONLY, NULL, NULL );
    check( hr == WBEM_E_ALREADY_EXISTS, "PutInstance CREATE_ONLY again: WBEM_E_ALREADY_EXISTS", hr );
    put_str( inst, L"Name", L"beta", 0 );
    put_int( inst, L"Count", 2, 0 );
    hr = IWbemServices_PutInstance( svc, inst, WBEM_FLAG_UPDATE_ONLY, NULL, NULL );
    check( hr == WBEM_E_NOT_FOUND, "PutInstance UPDATE_ONLY of a new one: WBEM_E_NOT_FOUND", hr );
    hr = IWbemServices_PutInstance( svc, inst, 0, NULL, NULL );
    check( hr == S_OK, "PutInstance (beta)", hr );
    IWbemClassObject_Release( inst );

    n = query_count( L"SELECT * FROM SG_GapTest", NULL );
    check( n == 2, "a query finds both", n );
    n = query_count( L"SELECT * FROM SG_GapTest WHERE Count = 7", &obj );
    check( n == 1 && obj && get_str( obj, L"Name", L"alpha" ) && get_int( obj, L"Enabled" ) == -1,
           "WHERE Count = 7: alpha, enabled", n );
    if (obj)
    {
        /* changed and stored again */
        put_int( obj, L"Count", 8, 0 );
        hr = IWbemServices_PutInstance( svc, obj, WBEM_FLAG_UPDATE_ONLY, NULL, NULL );
        check( hr == S_OK, "PutInstance UPDATE_ONLY of a queried instance", hr );
        IWbemClassObject_Release( obj );
        obj = NULL;
    }
    n = query_count( L"SELECT * FROM SG_GapTest WHERE Count = 8", NULL );
    check( n == 1, "the change is stored", n );
    hr = get_object( L"SG_GapTest.Name=\"beta\"", &obj );
    check( hr == S_OK && obj && get_int( obj, L"Count" ) == 2, "GetObject of an instance path", hr );
    if (obj) IWbemClassObject_Release( obj );

    /* another process sees them */
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[MAX_PATH + 16], self[MAX_PATH];
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        HANDLE rd, wr;
        char buf[64] = {0};
        DWORD got = 0;

        GetModuleFileNameW( NULL, self, MAX_PATH );
        swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" count", self );
        CreatePipe( &rd, &wr, &sa, 0 );
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = wr;
        si.hStdError = wr;
        if (CreateProcessW( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
        {
            CloseHandle( wr );
            WaitForSingleObject( pi.hProcess, 60000 );
            ReadFile( rd, buf, sizeof(buf) - 1, &got, NULL );
            CloseHandle( pi.hProcess ); CloseHandle( pi.hThread );
        }
        else CloseHandle( wr );
        CloseHandle( rd );
        check( strstr( buf, "count 2" ) != NULL, "another process finds the class and both instances", 0 );
    }

    hr = IWbemServices_DeleteInstance( svc, (BSTR)L"SG_GapTest.Name=\"beta\"", 0, NULL, NULL );
    check( hr == S_OK, "DeleteInstance", hr );
    n = query_count( L"SELECT * FROM SG_GapTest", NULL );
    check( n == 1, "one instance left", n );
    IWbemClassObject_Release( cls );

    hr = IWbemServices_DeleteClass( svc, (BSTR)L"SG_GapChild", 0, NULL, NULL );
    check( hr == S_OK, "DeleteClass of the derived class", hr );
    hr = IWbemServices_DeleteClass( svc, (BSTR)L"SG_GapTest", 0, NULL, NULL );
    check( hr == S_OK, "DeleteClass", hr );
    cls = NULL;
    hr = get_object( L"SG_GapTest", &cls );
    check( FAILED(hr), "the class is gone", hr );
    hr = IWbemServices_DeleteClass( svc, (BSTR)L"Win32_Process", 0, NULL, NULL );
    check( hr == WBEM_E_ACCESS_DENIED, "a system class is not deleted", hr );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
