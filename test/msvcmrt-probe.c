/* msvcmrt-probe.c: msvcm80/msvcm90's C++/CLI runtime helpers, for test/msvcmrt-gate.sh (Stained Glass OS
 * wine-sg 9455): the std library forwards (_Mtx*, _Once, _Cosh/_Sinh, __Wcrtomb_lk), the managed new,
 * terminate and unexpected handlers and the default application domain of a loaded runtime.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <objbase.h>

typedef struct { void *qi, *addref, *release, *get_runtime; } metahost_vtbl;
typedef struct { void *qi, *addref, *release, *version, *dir, *is_loaded, *error_string, *load_library, *proc;
                 HRESULT (WINAPI *get_interface)( void *, REFCLSID, REFIID, void ** ); } runtimeinfo_vtbl;
typedef struct { const metahost_vtbl *vtbl; } ICLRMetaHost;
typedef struct { const runtimeinfo_vtbl *vtbl; } ICLRRuntimeInfo;
static const GUID CLSID_CLRMetaHost = {0x9280188d,0x0e8e,0x4867,{0xb3,0x0c,0x7f,0xa8,0x38,0x84,0xe8,0xde}};
static const GUID IID_ICLRMetaHost = {0xd332db9e,0xb9b3,0x4125,{0x82,0x07,0xa1,0x48,0x84,0xf5,0x32,0x16}};
static const GUID IID_ICLRRuntimeInfo = {0xbd39d1d2,0xba2f,0x486a,{0x89,0xb0,0xb4,0xb0,0xcb,0x46,0x68,0x91}};
static const GUID CLSID_CorRuntimeHost = {0xcb2f6723,0xab3a,0x11d2,{0x9c,0x40,0x00,0xc0,0x4f,0xa3,0x0a,0x3e}};
static const GUID IID_ICorRuntimeHost = {0xcb2f6722,0xab3a,0x11d2,{0x9c,0x40,0x00,0xc0,0x4f,0xa3,0x0a,0x3e}};

#ifdef _WIN64
#define P "E"
#define NH_SET "?_set_new_handler@@YAP6MH_K@ZP6MH0@Z@Z"
#define NH_QUERY "?__query_new_handler_m@@YAP6MH_K@ZXZ"
#define WCRTOMB "?__Wcrtomb_lk@@YAHPEAD_WPEAHPEBU_Cvtvec@@@Z"
#else
#define P ""
#define NH_SET "?_set_new_handler@@YAP6MHI@ZP6MHI@Z@Z"
#define NH_QUERY "?__query_new_handler_m@@YAP6MHI@ZXZ"
#define WCRTOMB "?__Wcrtomb_lk@@YAHPAD_WPAHPBU_Cvtvec@@@Z"
#endif

static int once_count;
static void __cdecl once_func( void ) { once_count++; }

int main( int argc, char **argv )
{
    const char *dll = argc > 1 ? argv[1] : "msvcm90.dll";
    HMODULE m = LoadLibraryA( dll );
    void (__cdecl *mtx)( CRITICAL_SECTION * );
    void (__cdecl *once)( LONG *, void (__cdecl *)(void) );
    double (__cdecl *cosh_)( double, double ), (__cdecl *sinh_)( double, double );
    void *(__cdecl *set)( void * ), *(__cdecl *query)( void );
    HRESULT (__cdecl *get_domain)( IUnknown ** );
    void (__cdecl *release_domain)( IUnknown * );
    int (__cdecl *wcrtomb_lk)( char *, WCHAR, int *, const void * );
    CRITICAL_SECTION cs;
    LONG flag = 0;
    IUnknown *domain = NULL;
    HRESULT hr;

    printf( "loaded=%d\n", m != NULL );
    if (!m) return 1;

    /* the critical section helpers of the C++ library */
    memset( &cs, 0xcc, sizeof(cs) );
    if ((mtx = (void *)GetProcAddress( m, "?_Mtxinit@@YAXP" P "AU_RTL_CRITICAL_SECTION@@@Z" ))) mtx( &cs );
    printf( "mtxinit=%d %ld\n", mtx != NULL, mtx ? cs.LockCount : 99 );
    if ((mtx = (void *)GetProcAddress( m, "?_Mtxlock@@YAXP" P "AU_RTL_CRITICAL_SECTION@@@Z" ))) mtx( &cs );
    printf( "mtxlock=%d %ld\n", mtx != NULL, mtx ? cs.RecursionCount : 99 );
    if ((mtx = (void *)GetProcAddress( m, "?_Mtxunlock@@YAXP" P "AU_RTL_CRITICAL_SECTION@@@Z" ))) mtx( &cs );
    printf( "mtxunlock=%d %ld\n", mtx != NULL, mtx ? cs.RecursionCount : 99 );
    if ((mtx = (void *)GetProcAddress( m, "?_Mtxdst@@YAXP" P "AU_RTL_CRITICAL_SECTION@@@Z" ))) mtx( &cs );
    printf( "mtxdst=%d\n", mtx != NULL );

    once = (void *)GetProcAddress( m, "?_Once@@YAXP" P "AJP6AXXZ@Z" );
    if (once) { once( &flag, once_func ); once( &flag, once_func ); }
    printf( "once=%d %d %ld\n", once != NULL, once_count, flag );

    cosh_ = (void *)GetProcAddress( m, "?_Cosh@@YANNN@Z" );
    sinh_ = (void *)GetProcAddress( m, "?_Sinh@@YANNN@Z" );
    printf( "cosh=%d %.4f\n", cosh_ != NULL, cosh_ ? cosh_( 1.0, 2.0 ) : 0.0 );
    printf( "sinh=%d %.4f\n", sinh_ != NULL, sinh_ ? sinh_( 1.0, 2.0 ) : 0.0 );

    if ((wcrtomb_lk = (void *)GetProcAddress( m, WCRTOMB )))
    {
        char buf[8] = { 0 };
        int state = 0, n = wcrtomb_lk( buf, 'A', &state, NULL );
        printf( "wcrtomb=1 %d %s\n", n, buf );
    }
    else printf( "wcrtomb=0\n" );

    /* the managed handlers are kept and handed back */
    set = (void *)GetProcAddress( m, NH_SET );
    query = (void *)GetProcAddress( m, NH_QUERY );
    if (set && query)
    {
        void *before = query(), *old = set( (void *)0x1234 ), *now = query(), *back = set( NULL );
        printf( "newhandler=%d %d %d %d\n", before == NULL, old == NULL, now == (void *)0x1234, back == (void *)0x1234 );
    }
    else printf( "newhandler=missing\n" );
    set = (void *)GetProcAddress( m, "?set_terminate@@YAP6MXXZP6MXXZ@Z" );
    if (set) { void *a = set( (void *)0x10 ), *b = set( a ); printf( "terminate=%d %d\n", a == NULL, b == (void *)0x10 ); }
    else printf( "terminate=missing\n" );

    /* the default application domain: none without a runtime, then the runtime's */
    get_domain = (void *)GetProcAddress( m, "?__get_default_appdomain@@YAJP" P "AP" P "AUIUnknown@@@Z" );
    release_domain = (void *)GetProcAddress( m, "?__release_appdomain@@YAXP" P "AUIUnknown@@@Z" );
    if (get_domain && release_domain)
    {
        HRESULT (WINAPI *create)( REFCLSID, REFIID, void ** );
        HMODULE mscoree = LoadLibraryA( "mscoree.dll" );
        ICLRMetaHost *metahost = NULL;
        ICLRRuntimeInfo *info = NULL;
        IUnknown *host = NULL;

        hr = get_domain( &domain );
        printf( "nodomain=%d\n", FAILED(hr) && !domain );
        create = (void *)GetProcAddress( mscoree, "CLRCreateInstance" );
        if (create && SUCCEEDED(create( &CLSID_CLRMetaHost, &IID_ICLRMetaHost, (void **)&metahost )) &&
            SUCCEEDED(((HRESULT (WINAPI *)( void *, const WCHAR *, REFIID, void ** ))metahost->vtbl->get_runtime)(
                metahost, L"v4.0.30319", &IID_ICLRRuntimeInfo, (void **)&info )) &&
            SUCCEEDED(info->vtbl->get_interface( info, &CLSID_CorRuntimeHost, &IID_ICorRuntimeHost, (void **)&host )))
        {
            hr = get_domain( &domain );
            printf( "domain=%d %d\n", hr == S_OK, domain != NULL );
            if (domain)
            {
                IUnknown *again = NULL;
                hr = domain->lpVtbl->QueryInterface( domain, &IID_IUnknown, (void **)&again );
                printf( "domain_unk=%d\n", hr == S_OK && again != NULL );
                if (again) again->lpVtbl->Release( again );
                release_domain( domain );
            }
        }
        else printf( "domain=no runtime\n" );
    }
    else printf( "domain=missing\n" );
    printf( "done=1\n" );
    return 0;
}
