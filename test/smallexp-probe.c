/* smallexp-probe: exports that Chrome, VS Code and Firefox import and that were stubs (they crash a
 * program that calls them): combase IsErrorPropagationEnabled, RoReportFailedDelegate; bthprops
 * BluetoothEnableIncomingConnections; tbs Tbsip_Submit_Command, Tbsip_Context_Close,
 * Tbsip_Cancel_Commands (Stained Glass OS wine-sg 6022).   smallexp-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

typedef BOOL (WINAPI *errprop_fn)(void);
typedef HRESULT (WINAPI *setflags_fn)(UINT32);
typedef HRESULT (WINAPI *delegate_fn)(IUnknown *, IUnknown *);
typedef BOOL (WINAPI *originate_fn)(HRESULT, UINT, const WCHAR *);
typedef HRESULT (WINAPI *get_error_fn)(IUnknown **);
typedef BOOL (WINAPI *incoming_fn)(HANDLE, BOOL);
typedef UINT32 (WINAPI *submit_fn)(void *, UINT32, UINT32, const BYTE *, UINT32, BYTE *, UINT32 *);
typedef UINT32 (WINAPI *ctx_fn)(void *);

#define RO_ERROR_REPORTING_USESETERRORINFO 4
#define RO_ERROR_REPORTING_SUPPRESSSETERRORINFO 8
#define TBS_E_BAD_PARAMETER 0x80284002
#define TBS_E_INVALID_CONTEXT 0x80284004

static HRESULT WINAPI unk_qi( IUnknown *iface, REFIID riid, void **out ) { *out = iface; return S_OK; }
static ULONG WINAPI unk_addref( IUnknown *iface ) { return 2; }
static ULONG WINAPI unk_release( IUnknown *iface ) { return 1; }
static IUnknownVtbl unk_vtbl = { unk_qi, unk_addref, unk_release };
static IUnknown delegate = { &unk_vtbl };

int main( void )
{
    HMODULE combase = LoadLibraryA( "combase.dll" ), bth = LoadLibraryA( "bthprops.cpl" ), tbs = LoadLibraryA( "tbs.dll" );
    errprop_fn errprop = (errprop_fn)GetProcAddress( combase, "IsErrorPropagationEnabled" );
    setflags_fn setflags = (setflags_fn)GetProcAddress( combase, "RoSetErrorReportingFlags" );
    delegate_fn report = (delegate_fn)GetProcAddress( combase, "RoReportFailedDelegate" );
    originate_fn originate = (originate_fn)GetProcAddress( combase, "RoOriginateErrorW" );
    get_error_fn get_error = (get_error_fn)GetProcAddress( combase, "GetRestrictedErrorInfo" );
    incoming_fn incoming = (incoming_fn)GetProcAddress( bth, "BluetoothEnableIncomingConnections" );
    submit_fn submit = (submit_fn)GetProcAddress( tbs, "Tbsip_Submit_Command" );
    ctx_fn close_ctx = (ctx_fn)GetProcAddress( tbs, "Tbsip_Context_Close" );
    ctx_fn cancel = (ctx_fn)GetProcAddress( tbs, "Tbsip_Cancel_Commands" );
    IUnknown *error = NULL;
    BYTE cmd[10] = {0x80, 0x01, 0, 0, 0, 10, 0, 0, 0x01, 0x7b}, result[64];
    UINT32 result_size = sizeof(result);
    HRESULT hr;

    if (!errprop || !setflags || !report || !originate || !get_error || !incoming || !submit || !close_ctx || !cancel)
    {
        printf( "FAIL  an export is missing\nRESULT: FAIL\n" );
        return 1;
    }

    check( errprop(), "error propagation is on by default", 0 );
    setflags( RO_ERROR_REPORTING_SUPPRESSSETERRORINFO );
    check( !errprop(), "and off with SUPPRESSSETERRORINFO", 0 );
    setflags( RO_ERROR_REPORTING_USESETERRORINFO );
    check( errprop(), "and on again", 0 );

    hr = report( NULL, NULL );
    check( hr == E_INVALIDARG, "RoReportFailedDelegate without a delegate: E_INVALIDARG", hr );
    originate( E_FAIL, 0, L"a failed delegate" );
    get_error( &error );
    hr = report( &delegate, error );
    check( hr == S_OK, "RoReportFailedDelegate of a failed delegate", hr );
    if (error) IUnknown_Release( error );

    check( !incoming( NULL, TRUE ), "BluetoothEnableIncomingConnections: no radio", 0 );

    check( submit( NULL, 0, 200, cmd, sizeof(cmd), result, &result_size ) == TBS_E_INVALID_CONTEXT,
           "Tbsip_Submit_Command on no context: TBS_E_INVALID_CONTEXT", 0 );
    check( submit( NULL, 0, 200, NULL, 0, result, &result_size ) == TBS_E_BAD_PARAMETER,
           "no command: TBS_E_BAD_PARAMETER", 0 );
    check( close_ctx( NULL ) == TBS_E_INVALID_CONTEXT, "Tbsip_Context_Close: TBS_E_INVALID_CONTEXT", 0 );
    check( cancel( NULL ) == TBS_E_INVALID_CONTEXT, "Tbsip_Cancel_Commands: TBS_E_INVALID_CONTEXT", 0 );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
