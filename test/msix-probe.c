/* msix-probe: MSIX deployment through PackageManager, as programs (and
 * winget) use it, for test/msix-gate.sh.
 *
 *   msix-probe add PATH [DEP...]    AddPackageAsync(PATH, dependencies DEP...), then wait
 *   msix-probe request PATH         IPackageManager6.RequestAddPackageAsync (winget's call)
 *   msix-probe remove FULLNAME      RemovePackageAsync
 *   msix-probe info FULLNAME        FindPackageByPackageFullName: kind, date, location, dependencies
 *   msix-probe list TYPES           FindPackagesWithPackageTypes(TYPES): one full name per line
 *   msix-probe uri STRING           Windows.Foundation.Uri(STRING).AbsoluteUri
 *   msix-probe pkgpath              GetCurrentPackagePath and PATH, as a packaged process sees them
 *
 * Interfaces are declared here from their published IIDs.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <shlwapi.h>
#include <appmodel.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 void *GetIids, *GetRuntimeClassName, *GetTrustLevel; } InspVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *ActivateInstance)(void *, void **); } FactoryVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *CreateUri)(void *, HSTRING, void **); } UriFactoryVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_AbsoluteUri)(void *, HSTRING *); } UriVtbl;
typedef struct { InspVtbl i;
                 HRESULT (WINAPI *AddPackageAsync)(void *, void *, void *, UINT32, void **);
                 void *UpdatePackageAsync;
                 HRESULT (WINAPI *RemovePackageAsync)(void *, HSTRING, void **);
                 void *StagePackageAsync, *RegisterPackageAsync, *FindPackages, *FindPackagesByUserSecurityId,
                      *FindPackagesByNamePublisher, *FindPackagesByUserSecurityIdNamePublisher, *FindUsers, *SetPackageState;
                 HRESULT (WINAPI *FindPackageByPackageFullName)(void *, HSTRING, void **); } ManagerVtbl;
typedef struct { InspVtbl i; void *RemovePackageWithOptionsAsync, *StagePackageWithOptionsAsync, *RegisterPackageByFullNameAsync;
                 HRESULT (WINAPI *FindPackagesWithPackageTypes)(void *, UINT32, void **); } Manager2Vtbl;
typedef struct { InspVtbl i; void *Provision, *AddByAppInstaller, *RequestAddByAppInstaller, *AddToVolumeAndRelatedSet,
                 *StageToVolumeAndRelatedSet;
                 HRESULT (WINAPI *RequestAddPackageAsync)(void *, void *, void *, UINT32, void *, void *, void *, void **); } Manager6Vtbl;
typedef struct { InspVtbl i; void *put_Progress, *get_Progress, *put_Completed, *get_Completed;
                 HRESULT (WINAPI *GetResults)(void *, void **); } OperationVtbl;
typedef struct { InspVtbl i; void *get_Id; HRESULT (WINAPI *get_Status)(void *, int *); HRESULT (WINAPI *get_ErrorCode)(void *, HRESULT *); } AsyncInfoVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_ErrorText)(void *, HSTRING *); void *get_ActivityId;
                 HRESULT (WINAPI *get_ExtendedErrorCode)(void *, HRESULT *); } ResultVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *First)(void *, void **); } IterableVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_Current)(void *, void **); HRESULT (WINAPI *get_HasCurrent)(void *, boolean *);
                 HRESULT (WINAPI *MoveNext)(void *, boolean *); } IteratorVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *GetAt)(void *, UINT32, void **); HRESULT (WINAPI *get_Size)(void *, UINT32 *); } VectorViewVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_Id)(void *, void **); HRESULT (WINAPI *get_InstalledLocation)(void *, void **);
                 HRESULT (WINAPI *get_IsFramework)(void *, boolean *); HRESULT (WINAPI *get_Dependencies)(void *, void **); } PackageVtbl;
typedef struct { InspVtbl i; void *DisplayName, *PublisherDisplayName, *Description, *Logo;
                 HRESULT (WINAPI *get_IsResourcePackage)(void *, boolean *); } Package2Vtbl;
typedef struct { InspVtbl i; void *Status; HRESULT (WINAPI *get_InstalledDate)(void *, INT64 *); } Package3Vtbl;
typedef struct { InspVtbl i; void *Name, *Version, *Architecture, *ResourceId, *Publisher, *PublisherId;
                 HRESULT (WINAPI *get_FullName)(void *, HSTRING *); } PackageIdVtbl;
typedef struct { InspVtbl i; void *r1, *r2, *d1, *d2, *props, *name; HRESULT (WINAPI *get_Path)(void *, HSTRING *); } StorageItemVtbl;
typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID IID_IActivationFactory_ = {0x00000035,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IAsyncInfo_ = {0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IPackageManager_ = {0x9a7d4b65,0x5e8f,0x4fc7,{0xa2,0xe5,0x7f,0x69,0x25,0xcb,0x8b,0x53}};
static const GUID IID_IPackageManager2_ = {0xf7aad08d,0x0840,0x46f2,{0xb5,0xd8,0xca,0xd4,0x76,0x93,0xa0,0x95}};
static const GUID IID_IPackageManager6_ = {0x0847e909,0x53cd,0x4e4f,{0x83,0x2e,0x57,0xd1,0x80,0xf6,0xe4,0x47}};
static const GUID IID_IPackage2_ = {0xa6612fb6,0x7688,0x4ace,{0x95,0xfb,0x35,0x95,0x38,0xe7,0xaa,0x01}};
static const GUID IID_IPackage3_ = {0x5f738b61,0xf86a,0x4917,{0x93,0xd1,0xf1,0xee,0x9d,0x3b,0x35,0xd9}};
static const GUID IID_IUriRuntimeClassFactory_ = {0x44a9796f,0x723e,0x4fdf,{0xa2,0x18,0x03,0x3e,0x75,0xb0,0xc0,0x84}};
static const GUID IID_IStorageItem_ = {0x4207a996,0xca2f,0x42f7,{0xbd,0xe8,0x8b,0x10,0x45,0x7a,0x7f,0x30}};
static const GUID IID_IIterable_Uri_ = {0xb0d63b78,0x78ad,0x5e31,{0xb6,0xd8,0xe3,0x2a,0x0e,0x16,0xc4,0x47}};
static const GUID IID_IIterator_Uri_ = {0x1c157d0f,0x5efe,0x5cec,{0xbb,0xd6,0x0c,0x6c,0xe9,0xaf,0x07,0xa5}};
static const GUID IID_IUnknown_ = {0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IInspectable_ = {0xaf86e2e0,0xb12d,0x4c6a,{0x9c,0x5a,0xd7,0xaa,0x65,0x10,0x1e,0x90}};

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, lstrlenW( s ), &h );
    return h;
}

static const WCHAR *raw( HSTRING h )
{
    const WCHAR *s = h ? WindowsGetStringRawBuffer( h, NULL ) : NULL;
    return s ? s : L"";
}

static void *factory( const WCHAR *class, const GUID *iid )
{
    void *f = NULL, *out = NULL;
    if (FAILED(RoGetActivationFactory( hs( class ), &IID_IActivationFactory_, &f ))) return NULL;
    VT(f, InspVtbl)->QI( f, iid, &out );
    return out;
}

static void *activate( const WCHAR *class, const GUID *iid )
{
    void *f = factory( class, &IID_IActivationFactory_ ), *inst = NULL, *out = NULL;
    if (f && SUCCEEDED(VT(f, FactoryVtbl)->ActivateInstance( f, &inst ))) VT(inst, InspVtbl)->QI( inst, iid, &out );
    return out;
}

static void *make_uri( const char *path )
{
    void *uri_factory = factory( L"Windows.Foundation.Uri", &IID_IUriRuntimeClassFactory_ ), *uri = NULL;
    WCHAR w[MAX_PATH], url[MAX_PATH * 3];
    DWORD len = ARRAYSIZE(url);

    MultiByteToWideChar( CP_ACP, 0, path, -1, w, MAX_PATH );
    if (!strncmp( path, "http", 4 )) lstrcpyW( url, w );
    else UrlCreateFromPathW( w, url, &len, 0 );
    if (uri_factory) VT(uri_factory, UriFactoryVtbl)->CreateUri( uri_factory, hs( url ), &uri );
    return uri;
}

/* ---- a minimal IIterable<Uri> for the dependency URIs ---------------------------------- */

struct uri_list { const void *vtbl; LONG ref; void **uris; int count; };
struct uri_iter { const void *vtbl; LONG ref; struct uri_list *list; int index; };

static HRESULT WINAPI list_QI( void *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown_ ) || IsEqualGUID( iid, &IID_IInspectable_ ) || IsEqualGUID( iid, &IID_IIterable_Uri_ ))
    {
        *out = iface;
        ((struct uri_list *)iface)->ref++;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI list_AddRef( void *iface ) { return ++((struct uri_list *)iface)->ref; }
static ULONG WINAPI list_Release( void *iface ) { return --((struct uri_list *)iface)->ref; }
static HRESULT WINAPI stub( void *iface ) { return E_NOTIMPL; }
static HRESULT WINAPI iter_QI( void *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown_ ) || IsEqualGUID( iid, &IID_IInspectable_ ) || IsEqualGUID( iid, &IID_IIterator_Uri_ ))
    {
        *out = iface;
        ((struct uri_iter *)iface)->ref++;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static HRESULT WINAPI iter_get_Current( void *iface, void **value )
{
    struct uri_iter *it = iface;
    if (it->index >= it->list->count) return 0x8000000b; /* E_BOUNDS */
    *value = it->list->uris[it->index];
    VT(*value, InspVtbl)->AddRef( *value );
    return S_OK;
}
static HRESULT WINAPI iter_get_HasCurrent( void *iface, boolean *value )
{
    struct uri_iter *it = iface;
    *value = it->index < it->list->count;
    return S_OK;
}
static HRESULT WINAPI iter_MoveNext( void *iface, boolean *value )
{
    struct uri_iter *it = iface;
    if (it->index < it->list->count) it->index++;
    *value = it->index < it->list->count;
    return S_OK;
}
static const struct { InspVtbl i; void *get_Current, *get_HasCurrent, *MoveNext, *GetMany; } iter_vtbl =
    { { iter_QI, list_AddRef, list_Release, stub, stub, stub }, iter_get_Current, iter_get_HasCurrent, iter_MoveNext, stub };
static HRESULT WINAPI list_First( void *iface, void **value )
{
    struct uri_iter *it = calloc( 1, sizeof(*it) );
    it->vtbl = &iter_vtbl;
    it->ref = 1;
    it->list = iface;
    *value = it;
    return S_OK;
}
static const struct { InspVtbl i; void *First; } list_vtbl =
    { { list_QI, list_AddRef, list_Release, stub, stub, stub }, list_First };

/* ---------------------------------------------------------------------------------------------- */

/* Wait for a deployment operation; print its status, error and result. */
static int wait_op( const char *what, void *op )
{
    void *info = NULL, *result = NULL;
    HRESULT error = S_OK, extended = S_OK;
    HSTRING text = NULL;
    int status = 0, i;

    VT(op, InspVtbl)->QI( op, &IID_IAsyncInfo_, &info );
    for (i = 0; i < 3000 && info; i++)
    {
        VT(info, AsyncInfoVtbl)->get_Status( info, &status );
        if (status) break;
        Sleep( 100 );
    }
    if (info) VT(info, AsyncInfoVtbl)->get_ErrorCode( info, &error );
    printf( "%sStatus=%d\n%sError=0x%08lx\n", what, status, what, (unsigned long)error );
    if (SUCCEEDED(VT(op, OperationVtbl)->GetResults( op, &result )) && result)
    {
        VT(result, ResultVtbl)->get_ExtendedErrorCode( result, &extended );
        VT(result, ResultVtbl)->get_ErrorText( result, &text );
        printf( "%sExtended=0x%08lx\n%sText=%ls\n", what, (unsigned long)extended, what, raw( text ) );
    }
    return status == 1 ? 0 : 1;
}

static void print_package( void *package )
{
    void *id = NULL, *folder = NULL, *item = NULL, *deps = NULL, *p2 = NULL, *p3 = NULL;
    boolean flag = FALSE;
    HSTRING str = NULL;
    UINT32 n = 0, i;
    INT64 date = 0;

    if (SUCCEEDED(VT(package, PackageVtbl)->get_Id( package, &id )) && SUCCEEDED(VT(id, PackageIdVtbl)->get_FullName( id, &str )))
        printf( "FullName=%ls\n", raw( str ) );
    if (SUCCEEDED(VT(package, PackageVtbl)->get_InstalledLocation( package, &folder )) &&
        SUCCEEDED(VT(folder, InspVtbl)->QI( folder, &IID_IStorageItem_, &item )) &&
        SUCCEEDED(VT(item, StorageItemVtbl)->get_Path( item, &str )))
        printf( "Location=%ls\n", raw( str ) );
    VT(package, PackageVtbl)->get_IsFramework( package, &flag );
    printf( "IsFramework=%d\n", flag );
    if (SUCCEEDED(VT(package, InspVtbl)->QI( package, &IID_IPackage2_, &p2 )))
    {
        flag = FALSE;
        VT(p2, Package2Vtbl)->get_IsResourcePackage( p2, &flag );
        printf( "IsResourcePackage=%d\n", flag );
    }
    if (SUCCEEDED(VT(package, InspVtbl)->QI( package, &IID_IPackage3_, &p3 )) &&
        SUCCEEDED(VT(p3, Package3Vtbl)->get_InstalledDate( p3, &date )))
        printf( "InstalledDate=%s\n", date > 0 ? "set" : "unset" );
    if (SUCCEEDED(VT(package, PackageVtbl)->get_Dependencies( package, &deps )))
    {
        VT(deps, VectorViewVtbl)->get_Size( deps, &n );
        printf( "Dependencies=%u\n", n );
        for (i = 0; i < n; i++)
        {
            void *dep = NULL, *dep_id = NULL;
            if (SUCCEEDED(VT(deps, VectorViewVtbl)->GetAt( deps, i, &dep )) &&
                SUCCEEDED(VT(dep, PackageVtbl)->get_Id( dep, &dep_id )) &&
                SUCCEEDED(VT(dep_id, PackageIdVtbl)->get_FullName( dep_id, &str )))
                printf( "Dependency=%ls\n", raw( str ) );
        }
    }
    else printf( "Dependencies=error\n" );
}

int main( int argc, char **argv )
{
    void *manager, *op = NULL;
    WCHAR w[MAX_PATH];
    HRESULT hr;

    if (argc == 2 && !strcmp( argv[1], "pkgpath" ))
    {
        WCHAR path[MAX_PATH], env[8192];
        UINT32 len = MAX_PATH;
        LONG r = GetCurrentPackagePath( &len, path );
        printf( "PackagePath=%ls\n", r ? L"(none)" : path );
        GetEnvironmentVariableW( L"PATH", env, ARRAYSIZE(env) );
        printf( "PATH=%ls\n", env );
        return 0;
    }

    RoInitialize( RO_INIT_MULTITHREADED );
    if (argc == 3 && !strcmp( argv[1], "uri" ))
    {
        void *uri_factory = factory( L"Windows.Foundation.Uri", &IID_IUriRuntimeClassFactory_ ), *uri = NULL;
        HSTRING str = NULL;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, w, MAX_PATH );
        hr = uri_factory ? VT(uri_factory, UriFactoryVtbl)->CreateUri( uri_factory, hs( w ), &uri ) : E_FAIL;
        printf( "CreateUri=0x%08lx\n", (unsigned long)hr );
        if (SUCCEEDED(hr) && SUCCEEDED(VT(uri, UriVtbl)->get_AbsoluteUri( uri, &str ))) printf( "AbsoluteUri=%ls\n", raw( str ) );
        return 0;
    }
    if (!(manager = activate( L"Windows.Management.Deployment.PackageManager", &IID_IPackageManager_ )))
    {
        printf( "PackageManager=none\n" );
        return 1;
    }

    if (argc >= 3 && !strcmp( argv[1], "add" ))
    {
        struct uri_list list = { &list_vtbl, 1 };
        void *uri = make_uri( argv[2] );
        int i;

        list.uris = calloc( argc, sizeof(void *) );
        for (i = 3; i < argc; i++) list.uris[list.count++] = make_uri( argv[i] );
        hr = VT(manager, ManagerVtbl)->AddPackageAsync( manager, uri, list.count ? &list : NULL, 0, &op );
        printf( "AddPackageAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Add", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "request" ))
    {
        void *m6 = NULL, *uri = make_uri( argv[2] );
        hr = VT(manager, InspVtbl)->QI( manager, &IID_IPackageManager6_, &m6 );
        printf( "IPackageManager6=0x%08lx\n", (unsigned long)hr );
        if (FAILED(hr)) return 1;
        hr = VT(m6, Manager6Vtbl)->RequestAddPackageAsync( m6, uri, NULL, 0, NULL, NULL, NULL, &op );
        printf( "RequestAddPackageAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Add", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "remove" ))
    {
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, w, MAX_PATH );
        hr = VT(manager, ManagerVtbl)->RemovePackageAsync( manager, hs( w ), &op );
        printf( "RemovePackageAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Remove", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "info" ))
    {
        void *package = NULL;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, w, MAX_PATH );
        hr = VT(manager, ManagerVtbl)->FindPackageByPackageFullName( manager, hs( w ), &package );
        printf( "Find=0x%08lx\n", (unsigned long)hr );
        if (SUCCEEDED(hr) && package) print_package( package );
        else printf( "Found=0\n" );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "list" ))
    {
        void *m2 = NULL, *list = NULL, *iter = NULL, *package, *id;
        boolean has = FALSE;
        HSTRING str;

        VT(manager, InspVtbl)->QI( manager, &IID_IPackageManager2_, &m2 );
        hr = VT(m2, Manager2Vtbl)->FindPackagesWithPackageTypes( m2, strtoul( argv[2], NULL, 0 ), &list );
        if (SUCCEEDED(hr) && SUCCEEDED(VT(list, IterableVtbl)->First( list, &iter )))
        {
            VT(iter, IteratorVtbl)->get_HasCurrent( iter, &has );
            while (has)
            {
                if (SUCCEEDED(VT(iter, IteratorVtbl)->get_Current( iter, &package )) &&
                    SUCCEEDED(VT(package, PackageVtbl)->get_Id( package, &id )) &&
                    SUCCEEDED(VT(id, PackageIdVtbl)->get_FullName( id, &str )))
                    printf( "Package=%ls\n", raw( str ) );
                VT(iter, IteratorVtbl)->MoveNext( iter, &has );
            }
        }
        printf( "List=0x%08lx\n", (unsigned long)hr );
        return 0;
    }
    printf( "usage: see the source\n" );
    return 2;
}
