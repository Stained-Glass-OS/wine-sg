/* appmodel-probe: PackageCatalog and PackageManager queries (patches 0043,
 * 0044) as C++/WinRT callers use them, for test/appx-gate.sh.
 * Interfaces are declared here from their published IIDs.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 void *GetIids, *GetRuntimeClassName, *GetTrustLevel; } InspVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *OpenForCurrentPackage)(void *, void **); HRESULT (WINAPI *OpenForCurrentUser)(void *, void **); } CatalogStaticsVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *add[10])(void *, ...); } CatalogVtbl;
typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 HRESULT (WINAPI *GetWeakReference)(void *, void **); } WeakSourceVtbl;
typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 HRESULT (WINAPI *Resolve)(void *, REFIID, void **); } WeakRefVtbl;
typedef struct { InspVtbl i; void *Add, *Update, *Remove, *Stage, *Register;
                 HRESULT (WINAPI *FindPackages)(void *, void **); } ManagerVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *First)(void *, void **); } IterableVtbl;
typedef struct { InspVtbl i; void *Current; HRESULT (WINAPI *HasCurrent)(void *, boolean *); } IteratorVtbl;
typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID IID_IActivationFactory_ = {0x00000035,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IPackageCatalogStatics_ = {0xa18c9696,0xe65b,0x4634,{0xba,0x21,0x5e,0x63,0xeb,0x72,0x44,0xa7}};
static const GUID IID_IPackageCatalog_ = {0x230a3751,0x9de3,0x4445,{0xbe,0x74,0x91,0xfb,0x32,0x5a,0xbe,0xfe}};
static const GUID IID_IWeakReferenceSource_ = {0x00000038,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IPackageManager_ = {0x9a7d4b65,0x5e8f,0x4fc7,{0xa2,0xe5,0x7f,0x69,0x25,0xcb,0x8b,0x53}};

/* a do-nothing event handler: the catalog only has to hold it */
static HRESULT WINAPI h_QI( void *iface, REFIID iid, void **out ) { *out = iface; return S_OK; }
static ULONG WINAPI h_AddRef( void *iface ) { return 2; }
static ULONG WINAPI h_Release( void *iface ) { return 1; }
static HRESULT WINAPI h_Invoke( void *iface, void *sender, void *args ) { return S_OK; }
static const struct { void *qi, *ar, *rl, *inv; } handler_vtbl = { h_QI, h_AddRef, h_Release, h_Invoke };
static Obj handler = { &handler_vtbl };

static void *factory( const WCHAR *cls, const GUID *iid )
{
    HSTRING s;
    void *f = NULL;
    WindowsCreateString( cls, wcslen( cls ), &s );
    RoGetActivationFactory( s, iid, &f );
    WindowsDeleteString( s );
    return f;
}

int main( void )
{
    void *statics, *catalog = NULL, *source = NULL, *weak = NULL, *again = NULL, *af, *inst = NULL, *pm = NULL, *list = NULL, *it = NULL;
    INT64 token = 0;
    HRESULT hr;
    boolean has = 1;

    RoInitialize( RO_INIT_MULTITHREADED );
    if (!(statics = factory( L"Windows.ApplicationModel.PackageCatalog", &IID_IPackageCatalogStatics_ )))
    { printf( "catalog=ERROR\n" ); return 1; }
    hr = VT(statics, CatalogStaticsVtbl)->OpenForCurrentUser( statics, &catalog );
    printf( "OpenForCurrentUser=%#lx\n", hr );
    hr = VT(statics, CatalogStaticsVtbl)->OpenForCurrentPackage( statics, &again );
    printf( "OpenForCurrentPackage=%#lx\n", hr );
    if (!catalog) return 1;
    /* PackageStatusChanged: add is slot 8, remove slot 9 */
    hr = VT(catalog, CatalogVtbl)->add[8]( catalog, &handler, &token );
    printf( "addStatusChanged=%#lx token=%s\n", hr, token ? "nonzero" : "zero" );
    hr = VT(catalog, CatalogVtbl)->add[9]( catalog, token );
    printf( "removeStatusChanged=%#lx\n", hr );

    hr = VT(catalog, InspVtbl)->QI( catalog, &IID_IWeakReferenceSource_, &source );
    printf( "IWeakReferenceSource=%#lx\n", hr );
    if (source)
    {
        VT(source, WeakSourceVtbl)->GetWeakReference( source, &weak );
        VT(source, WeakSourceVtbl)->Release( source );
        again = NULL;
        hr = VT(weak, WeakRefVtbl)->Resolve( weak, &IID_IPackageCatalog_, &again );
        printf( "ResolveAlive=%s\n", SUCCEEDED(hr) && again ? "object" : "null" );
        if (again) VT(again, InspVtbl)->Release( again );
        VT(catalog, InspVtbl)->Release( catalog );
        again = (void *)1;
        hr = VT(weak, WeakRefVtbl)->Resolve( weak, &IID_IPackageCatalog_, &again );
        printf( "ResolveDead=%s\n", SUCCEEDED(hr) && !again ? "null" : "object" );
        VT(weak, WeakRefVtbl)->Release( weak );
    }

    af = factory( L"Windows.Management.Deployment.PackageManager", &IID_IActivationFactory_ );
    if (af) ((HRESULT (WINAPI *)(void *, void **))((void **)((Obj *)af)->vtbl)[6])( af, &inst );
    if (inst) VT(inst, InspVtbl)->QI( inst, &IID_IPackageManager_, &pm );
    hr = pm ? VT(pm, ManagerVtbl)->FindPackages( pm, &list ) : E_FAIL;
    printf( "FindPackages=%#lx\n", hr );
    if (list && SUCCEEDED(VT(list, IterableVtbl)->First( list, &it )))
    {
        VT(it, IteratorVtbl)->HasCurrent( it, &has );
        printf( "FindPackagesEmpty=%d\n", !has );
    }
    fflush( stdout );
    return 0;
}
