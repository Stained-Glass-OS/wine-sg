/* wmienum-probe: IWbemServices::CreateClassEnum (Stained Glass OS wine-sg 6040).
 *   wmienum-probe.exe define   stores SG_EnumBase and SG_EnumChild (derived) and checks the enumeration
 *   wmienum-probe.exe list     (another process) the stored classes are enumerated; removes them */
#define COBJMACROS
#include <windows.h>
#include <wbemcli.h>
#include <stdio.h>

#ifndef WBEM_FLAG_SHALLOW
#define WBEM_FLAG_SHALLOW 1
#endif

static int failures;
static IWbemServices *svc;

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

/* the classes CreateClassEnum gives: their count, whether "has" is among them, whether all are classes,
 * whether any has a superclass */
struct result { HRESULT hr; int count, found, all_classes, with_parent; };

static struct result enum_classes( const WCHAR *super, LONG flags, const WCHAR *has )
{
    struct result r = { 0, 0, 0, 1, 0 };
    IEnumWbemClassObject *e = NULL;
    IWbemClassObject *obj;
    BSTR s = super ? SysAllocString( super ) : NULL;
    WCHAR name[256];
    VARIANT v;
    ULONG n;

    r.hr = IWbemServices_CreateClassEnum( svc, s, flags, NULL, &e );
    SysFreeString( s );
    if (r.hr != S_OK) return r;
    while (IEnumWbemClassObject_Next( e, WBEM_INFINITE, 1, &obj, &n ) == S_OK && n)
    {
        r.count++;
        if (has && get_str( obj, L"__CLASS", name, 256 ) && !lstrcmpiW( name, has )) r.found = 1;
        VariantInit( &v );
        if (IWbemClassObject_Get( obj, L"__GENUS", 0, &v, NULL, NULL ) != S_OK || V_VT(&v) != VT_I4 ||
            V_I4(&v) != WBEM_GENUS_CLASS) r.all_classes = 0;
        if (get_str( obj, L"__SUPERCLASS", name, 256 ) && name[0]) r.with_parent = 1;
        IWbemClassObject_Release( obj );
    }
    IEnumWbemClassObject_Release( e );
    return r;
}

static HRESULT put_class( IWbemClassObject *cls, const WCHAR *name )
{
    VARIANT v;
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString( name );
    IWbemClassObject_Put( cls, L"__CLASS", 0, &v, 0 );
    VariantClear( &v );
    V_VT(&v) = VT_NULL;
    IWbemClassObject_Put( cls, L"Value", 0, &v, CIM_STRING );
    return IWbemServices_PutClass( svc, cls, 0, NULL, NULL );
}

/* a sink counting what it is given */
struct sink { IWbemObjectSink iface; LONG refs; int count; HRESULT status; BOOL done; };
static HRESULT WINAPI sink_qi( IWbemObjectSink *iface, REFIID riid, void **obj )
{
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IWbemObjectSink )) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_addref( IWbemObjectSink *iface ) { return 2; }
static ULONG WINAPI sink_release( IWbemObjectSink *iface ) { return 1; }
static HRESULT WINAPI sink_indicate( IWbemObjectSink *iface, LONG count, IWbemClassObject **objs )
{
    ((struct sink *)iface)->count += count;
    return S_OK;
}
static HRESULT WINAPI sink_setstatus( IWbemObjectSink *iface, LONG flags, HRESULT hr, BSTR param, IWbemClassObject *obj )
{
    struct sink *sink = (struct sink *)iface;
    if (flags == WBEM_STATUS_COMPLETE) { sink->status = hr; sink->done = TRUE; }
    return S_OK;
}
static IWbemObjectSinkVtbl sink_vtbl = { sink_qi, sink_addref, sink_release, sink_indicate, sink_setstatus };

int wmain( int argc, WCHAR **argv )
{
    IWbemLocator *loc;
    IWbemClassObject *cls = NULL, *child = NULL, *obj;
    struct sink sink = { { &sink_vtbl }, 1, 0, E_FAIL, FALSE };
    struct result r, deep;
    BSTR ns, bs;
    HRESULT hr;
    int i;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    if (CoCreateInstance( &CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc ) != S_OK)
    {
        printf( "FAIL  no WbemLocator\nRESULT: FAIL\n" );
        return 1;
    }
    ns = SysAllocString( L"ROOT\\CIMV2" );
    hr = IWbemLocator_ConnectServer( loc, ns, NULL, NULL, NULL, 0, NULL, NULL, &svc );
    SysFreeString( ns );
    if (hr != S_OK) { printf( "FAIL  ConnectServer %#lx\nRESULT: FAIL\n", hr ); return 1; }

    if (argc > 1 && !lstrcmpW( argv[1], L"list" ))
    {
        r = enum_classes( L"SG_EnumBase", 0, L"SG_EnumChild" );
        check( r.hr == S_OK && r.found, "another process enumerates the stored derived class", r.hr );
        r = enum_classes( NULL, 0, L"SG_EnumBase" );
        check( r.hr == S_OK && r.found, "and the stored class among all classes", r.hr );
        bs = SysAllocString( L"SG_EnumChild" );
        IWbemServices_DeleteClass( svc, bs, 0, NULL, NULL );
        SysFreeString( bs );
        bs = SysAllocString( L"SG_EnumBase" );
        IWbemServices_DeleteClass( svc, bs, 0, NULL, NULL );
        SysFreeString( bs );
        r = enum_classes( NULL, 0, L"SG_EnumBase" );
        check( r.hr == S_OK && !r.found, "a deleted class is not enumerated", r.hr );
        printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
        return failures != 0;
    }

    deep = enum_classes( NULL, 0, L"Win32_OperatingSystem" );
    check( deep.hr == S_OK, "CreateClassEnum of all classes", deep.hr );
    check( deep.count > 40, "gives the namespace's classes", deep.count );
    check( deep.found, "Win32_OperatingSystem among them", 0 );
    check( deep.all_classes, "every one a class (__GENUS 1), not an instance", 0 );
    r = enum_classes( NULL, WBEM_FLAG_SHALLOW, NULL );
    check( r.hr == S_OK && r.count > 0 && r.count < deep.count && !r.with_parent,
           "WBEM_FLAG_SHALLOW from the top: only classes without a superclass", r.count );
    r = enum_classes( L"CIM_LogicalDisk", 0, L"Win32_LogicalDisk" );
    check( r.hr == S_OK && r.found, "the subclasses of CIM_LogicalDisk have Win32_LogicalDisk", r.hr );
    r = enum_classes( L"CIM_LogicalDisk", 0, L"CIM_LogicalDisk" );
    check( r.hr == S_OK && !r.found, "but not CIM_LogicalDisk itself", r.hr );
    r = enum_classes( L"CIM_StorageExtent", 0, L"Win32_LogicalDisk" );
    check( r.hr == S_OK && r.found, "deep from an abstract class (CIM_StorageExtent): its grandchild Win32_LogicalDisk", r.hr );
    r = enum_classes( L"CIM_StorageExtent", WBEM_FLAG_SHALLOW, L"Win32_LogicalDisk" );
    check( r.hr == S_OK && !r.found, "not with WBEM_FLAG_SHALLOW", r.hr );
    r = enum_classes( L"SG_NoSuchClass", 0, NULL );
    check( r.hr == WBEM_E_INVALID_CLASS, "an unknown superclass: WBEM_E_INVALID_CLASS", r.hr );
    hr = IWbemServices_CreateClassEnum( svc, NULL, 0x4000000, NULL, (IEnumWbemClassObject **)&obj );
    check( hr == WBEM_E_INVALID_PARAMETER, "an unknown flag: WBEM_E_INVALID_PARAMETER", hr );

    /* classes an application defines */
    bs = NULL;
    hr = IWbemServices_GetObject( svc, bs, 0, NULL, &cls, NULL );
    if (hr == S_OK) hr = put_class( cls, L"SG_EnumBase" );
    check( hr == S_OK, "a class of its own is stored", hr );
    if (cls) IWbemClassObject_Release( cls );
    bs = SysAllocString( L"SG_EnumBase" );
    hr = IWbemServices_GetObject( svc, bs, 0, NULL, &cls, NULL );
    SysFreeString( bs );
    if (hr == S_OK) hr = IWbemClassObject_SpawnDerivedClass( cls, 0, &child );
    if (hr == S_OK) hr = put_class( child, L"SG_EnumChild" );
    check( hr == S_OK, "and a class derived from it", hr );
    if (child) IWbemClassObject_Release( child );
    if (cls) IWbemClassObject_Release( cls );
    r = enum_classes( L"SG_EnumBase", 0, L"SG_EnumChild" );
    check( r.hr == S_OK && r.count == 1 && r.found, "the subclasses of the new class: the derived one", r.count );
    r = enum_classes( L"SG_EnumBase", WBEM_FLAG_SHALLOW, L"SG_EnumChild" );
    check( r.hr == S_OK && r.found, "also with WBEM_FLAG_SHALLOW (a direct subclass)", r.hr );
    r = enum_classes( NULL, WBEM_FLAG_SHALLOW, L"SG_EnumBase" );
    check( r.hr == S_OK && r.found, "the new class is a top-level class", r.hr );

    /* asynchronously */
    bs = SysAllocString( L"CIM_LogicalDisk" );
    hr = IWbemServices_CreateClassEnumAsync( svc, bs, 0, NULL, &sink.iface );
    SysFreeString( bs );
    for (i = 0; i < 50 && !sink.done; i++) Sleep( 100 );
    check( hr == S_OK && sink.done && sink.status == S_OK && sink.count >= 1,
           "CreateClassEnumAsync gives the sink the classes and completes", sink.count );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
