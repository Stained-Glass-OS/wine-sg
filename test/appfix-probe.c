/* appfix-probe: what popular installers need of Wine (patches/sg/0235-0239).
 *
 *   appfix-probe service-install   register a COM class served by a service
 *                                  whose name is longer than a GUID, with
 *                                  ServiceParameters (as Google's updater does)
 *   appfix-probe localservice      CoCreateInstance of it: the service starts,
 *                                  is given its parameters, serves the class
 *   appfix-probe keydacl           a key opened for writing has its DACL set
 *   appfix-probe tlbstring         a typelib-marshalled interface with [string]
 *                                  parameters is called across apartments
 *   appfix-probe urlshortcut       an internet shortcut is saved while its
 *                                  property set is held open (WiX does)
 *   appfix-probe groupaffinity     GetProcessGroupAffinity
 *   appfix-probe --service         (the service itself)
 *
 * Prints name=value lines; see test/appfix-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <aclapi.h>
#include <intshcut.h>
#include <oleauto.h>
#include <stdio.h>

static const CLSID CLSID_Probe = { 0x5e8f3a1c, 0x7d2b, 0x4c9e, { 0x9a, 0x10, 0x53, 0x47, 0x46, 0x58, 0x44, 0x02 } };
static const IID IID_IProbeEcho = { 0x5e8f3a1c, 0x7d2b, 0x4c9e, { 0x9a, 0x10, 0x53, 0x47, 0x46, 0x58, 0x44, 0x03 } };
static const GUID LIBID_Probe = { 0x5e8f3a1c, 0x7d2b, 0x4c9e, { 0x9a, 0x10, 0x53, 0x47, 0x46, 0x58, 0x44, 0x04 } };
#define SERVICE_NAME L"StainedGlassProbeComService1.2.3.4-with-a-long-name"
#define ARGS_FILE L"C:\\sg-appfix-service-args.txt"

/* ---- a class factory for the service: a plain IUnknown object ---- */
static HRESULT WINAPI unk_qi( IUnknown *iface, REFIID iid, void **out )
{
    if (IsEqualIID( iid, &IID_IUnknown )) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI unk_addref( IUnknown *iface ) { return 2; }
static ULONG WINAPI unk_release( IUnknown *iface ) { return 1; }
static IUnknownVtbl unk_vtbl = { unk_qi, unk_addref, unk_release };
static IUnknown the_object = { &unk_vtbl };

static HRESULT WINAPI cf_qi( IClassFactory *iface, REFIID iid, void **out )
{
    if (IsEqualIID( iid, &IID_IUnknown ) || IsEqualIID( iid, &IID_IClassFactory )) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref( IClassFactory *iface ) { return 2; }
static ULONG WINAPI cf_release( IClassFactory *iface ) { return 1; }
static HRESULT WINAPI cf_create( IClassFactory *iface, IUnknown *outer, REFIID iid, void **out )
{
    return IUnknown_QueryInterface( &the_object, iid, out );
}
static HRESULT WINAPI cf_lock( IClassFactory *iface, BOOL lock ) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static IClassFactory the_factory = { &cf_vtbl };

static SERVICE_STATUS_HANDLE status_handle;
static HANDLE stop_event;

static DWORD WINAPI handler( DWORD control, DWORD type, void *data, void *context )
{
    SERVICE_STATUS status = { SERVICE_WIN32_OWN_PROCESS, SERVICE_STOP_PENDING };
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN)
    {
        SetServiceStatus( status_handle, &status );
        SetEvent( stop_event );
    }
    return NO_ERROR;
}

static void WINAPI service_main( DWORD argc, WCHAR **argv )
{
    SERVICE_STATUS status = { SERVICE_WIN32_OWN_PROCESS, SERVICE_RUNNING, SERVICE_ACCEPT_STOP };
    DWORD cookie, i, written;
    HANDLE file;
    char line[512];

    status_handle = RegisterServiceCtrlHandlerExW( SERVICE_NAME, handler, NULL );
    stop_event = CreateEventW( NULL, TRUE, FALSE, NULL );
    /* what the service was started with, for the client to check */
    file = CreateFileW( ARGS_FILE, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL );
    for (i = 1; i < argc; i++)
    {
        int n = snprintf( line, sizeof(line), "%ls\n", argv[i] );
        WriteFile( file, line, n, &written, NULL );
    }
    CloseHandle( file );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    CoRegisterClassObject( &CLSID_Probe, (IUnknown *)&the_factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie );
    SetServiceStatus( status_handle, &status );
    WaitForSingleObject( stop_event, 60000 );
    CoRevokeClassObject( cookie );
    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus( status_handle, &status );
}

static void set_sz( HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *value )
{
    HKEY key;
    if (RegCreateKeyExW( root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL )) return;
    RegSetValueExW( key, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW( value ) + 1) * sizeof(WCHAR) );
    RegCloseKey( key );
}

static void service_install( void )
{
    WCHAR self[MAX_PATH], cmd[MAX_PATH + 32];
    SC_HANDLE scm, svc;

    GetModuleFileNameW( NULL, self, MAX_PATH );
    swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" --service", self );
    scm = OpenSCManagerW( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    svc = CreateServiceW( scm, SERVICE_NAME, L"Probe", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                          SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, cmd, NULL, NULL, NULL, NULL, NULL );
    printf( "service_created=%d\n", svc != NULL || GetLastError() == ERROR_SERVICE_EXISTS );
    if (svc) CloseServiceHandle( svc );
    CloseServiceHandle( scm );
    set_sz( HKEY_LOCAL_MACHINE, L"Software\\Classes\\CLSID\\{5E8F3A1C-7D2B-4C9E-9A10-534746584402}", L"AppID",
            L"{5E8F3A1C-7D2B-4C9E-9A10-534746584402}" );
    set_sz( HKEY_LOCAL_MACHINE, L"Software\\Classes\\AppID\\{5E8F3A1C-7D2B-4C9E-9A10-534746584402}", L"LocalService",
            SERVICE_NAME );
    set_sz( HKEY_LOCAL_MACHINE, L"Software\\Classes\\AppID\\{5E8F3A1C-7D2B-4C9E-9A10-534746584402}",
            L"ServiceParameters", L"--com-service \"two words\"" );
}

static void localservice( void )
{
    IUnknown *unk = NULL;
    char args[512] = "";
    DWORD read = 0;
    HANDLE file;
    HRESULT hr;

    DeleteFileW( ARGS_FILE );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = CoCreateInstance( &CLSID_Probe, NULL, CLSCTX_LOCAL_SERVER, &IID_IUnknown, (void **)&unk );
    printf( "localservice_hr=%#lx\n", hr );
    printf( "localservice=%d\n", SUCCEEDED( hr ) && unk );
    if (unk) IUnknown_Release( unk );
    file = CreateFileW( ARGS_FILE, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    if (file != INVALID_HANDLE_VALUE)
    {
        ReadFile( file, args, sizeof(args) - 1, &read, NULL );
        args[read] = 0;
        CloseHandle( file );
    }
    printf( "service_args=%d\n", !strcmp( args, "--com-service\ntwo words\n" ) );
    {
        SC_HANDLE scm = OpenSCManagerW( NULL, NULL, SC_MANAGER_ALL_ACCESS );
        SC_HANDLE svc = OpenServiceW( scm, SERVICE_NAME, SERVICE_STOP );
        SERVICE_STATUS status;
        if (svc) { ControlService( svc, SERVICE_CONTROL_STOP, &status ); CloseServiceHandle( svc ); }
        CloseServiceHandle( scm );
    }
}

static void keydacl( void )
{
    PSECURITY_DESCRIPTOR sd = NULL;
    PACL dacl = NULL;
    HKEY key;
    LONG ret;

    ret = RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"Software\\StainedGlassProbe\\ClientStateMedium", 0, NULL, 0,
                           KEY_WRITE, NULL, &key, NULL );
    if (ret) { printf( "keydacl=create-%ld\n", ret ); return; }
    ret = GetSecurityInfo( key, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd );
    if (!ret) ret = SetSecurityInfo( key, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, dacl, NULL );
    printf( "keydacl=%d (%ld)\n", !ret, ret );
    LocalFree( sd );
    RegCloseKey( key );
    RegDeleteKeyW( HKEY_LOCAL_MACHINE, L"Software\\StainedGlassProbe\\ClientStateMedium" );
}

/* ---- tlbstring: an object with Echo([in, string] LPWSTR, [out] LPWSTR *) ---- */
typedef struct IProbeEcho IProbeEcho;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)( IProbeEcho *, REFIID, void ** );
    ULONG (WINAPI *AddRef)( IProbeEcho * );
    ULONG (WINAPI *Release)( IProbeEcho * );
    HRESULT (WINAPI *Echo)( IProbeEcho *, LPWSTR in, LPWSTR *out );
} IProbeEchoVtbl;
struct IProbeEcho { const IProbeEchoVtbl *lpVtbl; };

static HRESULT WINAPI echo_qi( IProbeEcho *iface, REFIID iid, void **out )
{
    if (IsEqualIID( iid, &IID_IUnknown ) || IsEqualIID( iid, &IID_IProbeEcho )) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI echo_addref( IProbeEcho *iface ) { return 2; }
static ULONG WINAPI echo_release( IProbeEcho *iface ) { return 1; }
static HRESULT WINAPI echo_echo( IProbeEcho *iface, LPWSTR in, LPWSTR *out )
{
    size_t len = in ? wcslen( in ) : 0;
    *out = CoTaskMemAlloc( (len + 2) * sizeof(WCHAR) );
    if (in) memcpy( *out, in, len * sizeof(WCHAR) );
    (*out)[len] = '!';
    (*out)[len + 1] = 0;
    return S_OK;
}
static const IProbeEchoVtbl echo_vtbl = { echo_qi, echo_addref, echo_release, echo_echo };
static IProbeEcho the_echo = { &echo_vtbl };

static BOOL make_typelib( const WCHAR *path )
{
    TYPEDESC lpwstr = { .vt = VT_LPWSTR }, ptr_lpwstr = { .vt = VT_PTR };
    ELEMDESC params[2] = { 0 };
    FUNCDESC func = { 0 };
    ICreateTypeLib2 *ctl;
    ICreateTypeInfo *cti;
    ITypeLib *stdole;
    ITypeInfo *unknown_info;
    HREFTYPE href;
    BSTR names[3];
    HRESULT hr;

    if (FAILED( CreateTypeLib2( sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32, path, &ctl ) )) return FALSE;
    ICreateTypeLib2_SetGuid( ctl, &LIBID_Probe );
    ICreateTypeLib2_SetVersion( ctl, 1, 0 );
    ICreateTypeLib2_SetName( ctl, (WCHAR *)L"ProbeLib" );
    if (FAILED( ICreateTypeLib2_CreateTypeInfo( ctl, (WCHAR *)L"IProbeEcho", TKIND_INTERFACE, &cti ) )) return FALSE;
    ICreateTypeInfo_SetGuid( cti, &IID_IProbeEcho );
    ICreateTypeInfo_SetTypeFlags( cti, TYPEFLAG_FOLEAUTOMATION );
    LoadRegTypeLib( &IID_StdOle, 2, 0, LOCALE_NEUTRAL, &stdole );
    ITypeLib_GetTypeInfoOfGuid( stdole, &IID_IUnknown, &unknown_info );
    ICreateTypeInfo_AddRefTypeInfo( cti, unknown_info, &href );
    ICreateTypeInfo_AddImplType( cti, 0, href );

    ptr_lpwstr.lptdesc = &lpwstr;
    params[0].tdesc = lpwstr;
    params[0].paramdesc.wParamFlags = PARAMFLAG_FIN;
    params[1].tdesc = ptr_lpwstr;
    params[1].paramdesc.wParamFlags = PARAMFLAG_FOUT;
    func.memid = 0x60010000;
    func.funckind = FUNC_PUREVIRTUAL;
    func.invkind = INVOKE_FUNC;
    func.callconv = CC_STDCALL;
    func.cParams = 2;
    func.lprgelemdescParam = params;
    func.elemdescFunc.tdesc.vt = VT_HRESULT;
    func.oVft = 3 * sizeof(void *);
    hr = ICreateTypeInfo_AddFuncDesc( cti, 0, &func );
    names[0] = SysAllocString( L"Echo" );
    names[1] = SysAllocString( L"in" );
    names[2] = SysAllocString( L"out" );
    ICreateTypeInfo_SetFuncAndParamNames( cti, 0, names, 3 );
    ICreateTypeInfo_LayOut( cti );
    ICreateTypeInfo_Release( cti );
    hr = ICreateTypeLib2_SaveAllChanges( ctl );
    ICreateTypeLib2_Release( ctl );
    return SUCCEEDED( hr );
}

static IStream *echo_stream;
static DWORD WINAPI sta_thread( void *arg )
{
    IProbeEcho *proxy = NULL;
    LPWSTR out = NULL;
    HRESULT hr;

    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    hr = CoGetInterfaceAndReleaseStream( echo_stream, &IID_IProbeEcho, (void **)&proxy );
    printf( "tlb_unmarshal=%#lx\n", hr );
    if (SUCCEEDED( hr ))
    {
        hr = proxy->lpVtbl->Echo( proxy, (LPWSTR)L"hello", &out );
        printf( "tlbstring=%d (%#lx %ls)\n", SUCCEEDED( hr ) && out && !wcscmp( out, L"hello!" ), hr,
                out ? out : L"(null)" );
        CoTaskMemFree( out );
        proxy->lpVtbl->Release( proxy );
    }
    else printf( "tlbstring=0\n" );
    CoUninitialize();
    return 0;
}

static void tlbstring( void )
{
    WCHAR path[MAX_PATH], sub[128], guid[40];
    ITypeLib *tl;
    HANDLE thread;
    HRESULT hr;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"sg-appfix-probe.tlb" );
    if (!make_typelib( path )) { printf( "tlbstring=no-typelib\n" ); return; }
    if (FAILED( hr = LoadTypeLibEx( path, REGKIND_NONE, &tl ) )) { printf( "tlbstring=load-%#lx\n", hr ); return; }
    ITypeLib_Release( tl );
    /* the library's registration, by hand */
    StringFromGUID2( &LIBID_Probe, guid, 40 );
    swprintf( sub, 128, L"Software\\Classes\\TypeLib\\%ls\\1.0\\0\\%ls", guid,
              sizeof(void *) == 8 ? L"win64" : L"win32" );
    set_sz( HKEY_LOCAL_MACHINE, sub, NULL, path );
    /* the interface's marshaller: the type library's */
    StringFromGUID2( &IID_IProbeEcho, guid, 40 );
    swprintf( sub, 128, L"Software\\Classes\\Interface\\%ls\\ProxyStubClsid32", guid );
    set_sz( HKEY_LOCAL_MACHINE, sub, NULL, L"{00020424-0000-0000-C000-000000000046}" );
    swprintf( sub, 128, L"Software\\Classes\\Interface\\%ls\\TypeLib", guid );
    StringFromGUID2( &LIBID_Probe, guid, 40 );
    set_sz( HKEY_LOCAL_MACHINE, sub, NULL, guid );
    set_sz( HKEY_LOCAL_MACHINE, sub, L"Version", L"1.0" );

    hr = CoMarshalInterThreadInterfaceInStream( &IID_IProbeEcho, (IUnknown *)&the_echo, &echo_stream );
    printf( "tlb_marshal=%#lx\n", hr );
    if (FAILED( hr )) { printf( "tlbstring=0\n" ); return; }
    thread = CreateThread( NULL, 0, sta_thread, NULL, 0, NULL );
    WaitForSingleObject( thread, 30000 );
}

/* the internet shortcut's property set (intshcut.h) */
static const GUID probe_FMTID_Intshcut = { 0x000214a0, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };

static void urlshortcut( void )
{
    IUnknown *url;
    IPropertySetStorage *pss;
    IPropertyStorage *ps = NULL;
    IPersistFile *pf;
    WCHAR path[MAX_PATH];
    char data[256] = "";
    DWORD read = 0;
    HANDLE file;
    HRESULT hr;

    CoInitialize( NULL );
    if (FAILED( CoCreateInstance( &CLSID_InternetShortcut, NULL, CLSCTX_INPROC_SERVER, &IID_IUniformResourceLocatorW,
                                  (void **)&url ) ))
    {
        printf( "urlshortcut=no-class\n" );
        return;
    }
    /* IUniformResourceLocatorW::SetURL, its first method */
    ((HRESULT (WINAPI *)( void *, const WCHAR *, DWORD ))(*(void ***)url)[3])( url, L"https://example.org/", 0 );
    /* as WiX's internet shortcut action: the property set held open while saving */
    IUnknown_QueryInterface( url, &IID_IPropertySetStorage, (void **)&pss );
    IPropertySetStorage_Open( pss, &probe_FMTID_Intshcut, STGM_WRITE, &ps );
    IUnknown_QueryInterface( url, &IID_IPersistFile, (void **)&pf );
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"sg-appfix-probe.url" );
    hr = IPersistFile_Save( pf, path, TRUE );
    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if (file != INVALID_HANDLE_VALUE) { ReadFile( file, data, sizeof(data) - 1, &read, NULL ); CloseHandle( file ); }
    printf( "urlshortcut=%d (%#lx)\n", hr == S_OK && strstr( data, "URL=https://example.org/" ) != NULL, hr );
    if (ps) IPropertyStorage_Release( ps );
    IPersistFile_Release( pf );
    IPropertySetStorage_Release( pss );
    IUnknown_Release( url );
}

static void groupaffinity( void )
{
    USHORT groups[4] = { 7, 7, 7, 7 }, count = 4, none = 0;
    BOOL ret = GetProcessGroupAffinity( GetCurrentProcess(), &count, groups ), ret2;
    DWORD err;

    ret2 = GetProcessGroupAffinity( GetCurrentProcess(), &none, NULL );
    err = GetLastError();
    printf( "groupaffinity=%d (%d %u %u; %d %lu %u)\n", ret && count == 1 && groups[0] == 0 && !ret2
            && err == ERROR_INSUFFICIENT_BUFFER && none == 1, ret, count, groups[0], ret2, err, none );
}

int wmain( int argc, WCHAR **argv )
{
    if (argc >= 2 && !lstrcmpW( argv[1], L"--service" ))
    {
        SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)SERVICE_NAME, service_main }, { NULL, NULL } };
        StartServiceCtrlDispatcherW( table );
        return 0;
    }
    if (argc < 2) return 2;
    if (!lstrcmpW( argv[1], L"service-install" )) service_install();
    else if (!lstrcmpW( argv[1], L"localservice" )) localservice();
    else if (!lstrcmpW( argv[1], L"keydacl" )) keydacl();
    else if (!lstrcmpW( argv[1], L"tlbstring" )) tlbstring();
    else if (!lstrcmpW( argv[1], L"urlshortcut" )) urlshortcut();
    else if (!lstrcmpW( argv[1], L"groupaffinity" )) groupaffinity();
    else return 2;
    fflush( stdout );
    return 0;
}
