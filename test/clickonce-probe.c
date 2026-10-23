/* clickonce-probe: ICLRRuntimeHost::ExecuteApplication and CoEEShutDownCOM (Stained Glass OS wine-sg 6071), for
 * test/clickonce-gate.sh. The gate puts app.application, app\app.exe.manifest and app\App.exe in C:\.
 * Prints KEY=VALUE lines. */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

static const GUID CLSID_CLRMetaHost_ = {0x9280188d,0x0e8e,0x4867,{0xb3,0x0c,0x7f,0xa8,0x38,0x84,0xe8,0xde}};
static const GUID IID_ICLRMetaHost_ = {0xd332db9e,0xb9b3,0x4125,{0x82,0x07,0xa1,0x48,0x84,0xf5,0x32,0x16}};
static const GUID IID_ICLRRuntimeInfo_ = {0xbd39d1d2,0xba2f,0x486a,{0x89,0xb0,0xb4,0xb0,0xcb,0x46,0x68,0x91}};
static const GUID CLSID_CLRRuntimeHost_ = {0x90f1a06e,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_ICLRRuntimeHost_ = {0x90f1a06c,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
#define SLOT(o, n, t) ((t)((*(void ***)(o))[n]))
typedef HRESULT (WINAPI *fn_exec)( void *, const WCHAR *, DWORD, const WCHAR **, DWORD, const WCHAR **, int * );

int main( void )
{
    HRESULT (WINAPI *create)(REFCLSID, REFIID, void **);
    void (WINAPI *shutdown)(void);
    HMODULE m = LoadLibraryA( "mscoree.dll" );
    void *meta = NULL, *info = NULL, *host = NULL;
    const WCHAR *manifests[2] = { L"C:\\app.application", L"C:\\app\\app.exe.manifest" };
    const WCHAR *nothing[1] = { L"C:\\app.application" };
    const WCHAR *activation[2] = { L"one", L"two" };
    const WCHAR *name = L"http://example.invalid/app.application#app.application, Version=1.0.0.0";
    char args[256] = "";
    int ret = -1;
    HRESULT hr;
    FILE *f;

    setvbuf( stdout, NULL, _IONBF, 0 );
    create = (void *)GetProcAddress( m, "CLRCreateInstance" );
    shutdown = (void *)GetProcAddress( m, "CoEEShutDownCOM" );
    shutdown();     /* before any runtime: nothing to do */
    printf( "ShutdownEarly=1\n" );
    create( &CLSID_CLRMetaHost_, &IID_ICLRMetaHost_, &meta );
    if (!meta) return 1;
    /* ICLRMetaHost::GetRuntime, ICLRRuntimeInfo::GetInterface */
    SLOT( meta, 3, HRESULT (WINAPI *)( void *, const WCHAR *, REFIID, void ** ) )( meta, L"v4.0.30319", &IID_ICLRRuntimeInfo_, &info );
    if (!info) return 1;
    SLOT( info, 9, HRESULT (WINAPI *)( void *, REFCLSID, REFIID, void ** ) )( info, &CLSID_CLRRuntimeHost_, &IID_ICLRRuntimeHost_, &host );
    if (!host) return 1;
    SLOT( host, 3, HRESULT (WINAPI *)( void * ) )( host );     /* Start */

    hr = SLOT( host, 10, fn_exec )( host, name, 2, manifests, 2, activation, NULL );
    printf( "NullReturn=%#lx\n", hr );
    hr = SLOT( host, 10, fn_exec )( host, name, 0, NULL, 0, NULL, &ret );
    printf( "NoManifests=%#lx\n", hr );
    hr = SLOT( host, 10, fn_exec )( host, name, 1, nothing, 0, NULL, &ret );
    printf( "NoEntryPoint=%#lx\n", hr );
    hr = SLOT( host, 10, fn_exec )( host, name, 2, manifests, 2, activation, &ret );
    if ((f = fopen( "C:\\app-args.txt", "r" ))) { fgets( args, sizeof(args), f ); fclose( f ); }
    printf( "Execute=%#lx %d %s\n", hr, ret, args );
    printf( "FinalizedBefore=%d\n", GetFileAttributesA( "C:\\finalized.flag" ) != INVALID_FILE_ATTRIBUTES );
    shutdown();
    printf( "FinalizedAfter=%d\n", GetFileAttributesA( "C:\\finalized.flag" ) != INVALID_FILE_ATTRIBUTES );
    printf( "done=1\n" );
    fflush( stdout );
    ExitProcess( 0 );
}
