/* msix2-probe: staging, registering by name, users, state, status, integrity, cleanup, provisioning,
 * App Installer files, cancellation and the display information of an application, for
 * test/msix2-gate.sh.  (The helpers are those of msix-probe.c.)
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


typedef struct { InspVtbl i; void *AddPackageAsync, *UpdatePackageAsync, *RemovePackageAsync;
                 HRESULT (WINAPI *StagePackageAsync)(void *, void *, void *, void **);
                 void *RegisterPackageAsync, *FindPackages, *FindPackagesByUserSecurityId, *FindPackagesByNamePublisher,
                      *FindPackagesByUserSecurityIdNamePublisher;
                 HRESULT (WINAPI *FindUsers)(void *, HSTRING, void **);
                 HRESULT (WINAPI *SetPackageState)(void *, HSTRING, int);
                 HRESULT (WINAPI *FindPackageByPackageFullName)(void *, HSTRING, void **);
                 HRESULT (WINAPI *CleanupPackageForUserAsync)(void *, HSTRING, HSTRING, void **);
                 void *FindPackagesByPackageFamilyName, *FindPackagesByUserSecurityIdPackageFamilyName,
                      *FindPackageByUserSecurityIdPackageFullName; } ManagerAllVtbl;
typedef struct { InspVtbl i; void *RemovePackageWithOptionsAsync;
                 HRESULT (WINAPI *StagePackageWithOptionsAsync)(void *, void *, void *, UINT32, void **);
                 HRESULT (WINAPI *RegisterPackageByFullNameAsync)(void *, HSTRING, void *, UINT32, void **);
                 void *FindPackagesWithPackageTypes, *f1, *f2, *f3, *f4, *f5;
                 HRESULT (WINAPI *StageUserDataAsync)(void *, HSTRING, void **); } Manager2AllVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *ProvisionPackageForAllUsersAsync)(void *, HSTRING, void **);
                 HRESULT (WINAPI *AddPackageByAppInstallerFileAsync)(void *, void *, UINT32, void *, void **); } Manager6AllVtbl;
typedef struct { InspVtbl i; void *get_Id, *get_InstalledLocation, *get_IsFramework, *get_Dependencies; } Package1Vtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_Status)(void *, void **); void *get_InstalledDate;
                 HRESULT (WINAPI *GetAppListEntriesAsync)(void *, void **); } Package3AllVtbl;
typedef struct { InspVtbl i; void *get_SignatureKind, *get_IsOptional; HRESULT (WINAPI *VerifyContentIntegrityAsync)(void *, void **); } Package4Vtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *VerifyIsOK)(void *, boolean *);
                 HRESULT (WINAPI *get[11])(void *, boolean *); } StatusVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_UserSecurityId)(void *, HSTRING *); HRESULT (WINAPI *get_InstallState)(void *, int *); } UserInfoVtbl;
typedef struct { InspVtbl i; void *put_Completed, *get_Completed; HRESULT (WINAPI *GetResults)(void *, void *); } BoolOpVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_DisplayInfo)(void *, void **); } EntryVtbl;
typedef struct { float w, h; } Size;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_DisplayName)(void *, HSTRING *); HRESULT (WINAPI *get_Description)(void *, HSTRING *);
                 HRESULT (WINAPI *GetLogo)(void *, Size, void **); } DisplayInfoVtbl;
typedef struct { InspVtbl i; void *f[3]; HRESULT (WINAPI *Cancel)(void *); } AsyncInfoCancelVtbl;
static const GUID IID_IPackage4_ = {0x65aed1ae,0xb95b,0x450c,{0x88,0x2b,0x62,0x55,0x18,0x7f,0x39,0x7e}};

/* a list of HSTRINGs for the iterable parameter */
struct str_list { const void *vtbl; LONG ref; HSTRING *s; int count; };
struct str_iter { const void *vtbl; LONG ref; struct str_list *list; int index; };
static HRESULT WINAPI sl_QI( void *iface, REFIID iid, void **out ) { *out = iface; return S_OK; }
static ULONG WINAPI sl_AddRef( void *iface ) { return 2; }
static ULONG WINAPI sl_Release( void *iface ) { return 1; }
static HRESULT WINAPI sl_stub( void *iface ) { return E_NOTIMPL; }
static HRESULT WINAPI si_get_Current( void *iface, HSTRING *value )
{
    struct str_iter *it = iface;
    if (it->index >= it->list->count) return 0x8000000b;
    return WindowsDuplicateString( it->list->s[it->index], value );
}
static HRESULT WINAPI si_get_HasCurrent( void *iface, boolean *value )
{
    struct str_iter *it = iface;
    *value = it->index < it->list->count;
    return S_OK;
}
static HRESULT WINAPI si_MoveNext( void *iface, boolean *value )
{
    struct str_iter *it = iface;
    if (it->index < it->list->count) it->index++;
    *value = it->index < it->list->count;
    return S_OK;
}
static const struct { InspVtbl i; void *get_Current, *get_HasCurrent, *MoveNext, *GetMany; } str_iter_vtbl =
    { { sl_QI, sl_AddRef, sl_Release, sl_stub, sl_stub, sl_stub }, si_get_Current, si_get_HasCurrent, si_MoveNext, sl_stub };
static HRESULT WINAPI sl_First( void *iface, void **value )
{
    struct str_iter *it = calloc( 1, sizeof(*it) );
    it->vtbl = &str_iter_vtbl;
    it->ref = 1;
    it->list = iface;
    *value = it;
    return S_OK;
}
static const struct { InspVtbl i; void *First; } str_list_vtbl =
    { { sl_QI, sl_AddRef, sl_Release, sl_stub, sl_stub, sl_stub }, sl_First };

static HSTRING H( const char *s )
{
    WCHAR w[MAX_PATH];
    MultiByteToWideChar( CP_ACP, 0, s, -1, w, MAX_PATH );
    return hs( w );
}

/* Wait for any async operation; return its status (1 completed, 2 canceled, 3 error). */
static int wait_status( void *op )
{
    void *info = NULL;
    int status = 0, i;

    VT(op, InspVtbl)->QI( op, &IID_IAsyncInfo_, &info );
    for (i = 0; i < 3000 && info; i++)
    {
        VT(info, AsyncInfoVtbl)->get_Status( info, &status );
        if (status) break;
        Sleep( 50 );
    }
    return status;
}

static void print_op_name( void *op )
{
    HSTRING name = NULL;
    HRESULT hr = VT(op, InspVtbl)->GetRuntimeClassName ? ((HRESULT (WINAPI *)(void *, HSTRING *))VT(op, InspVtbl)->GetRuntimeClassName)( op, &name ) : E_FAIL;
    printf( "OperationClass=0x%08lx %ls\n", (unsigned long)hr, raw( name ) );
}

int main( int argc, char **argv )
{
    void *manager, *m2 = NULL, *m6 = NULL, *op = NULL;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    if (!(manager = activate( L"Windows.Management.Deployment.PackageManager", &IID_IPackageManager_ ))) { printf( "PackageManager=none\n" ); return 1; }
    VT(manager, InspVtbl)->QI( manager, &IID_IPackageManager2_, &m2 );
    VT(manager, InspVtbl)->QI( manager, &IID_IPackageManager6_, &m6 );

    if (argc >= 3 && (!strcmp( argv[1], "stage" ) || !strcmp( argv[1], "stage2" )))
    {
        struct uri_list list = { &list_vtbl, 1 };
        void *uri = make_uri( argv[2] );
        int i;

        list.uris = calloc( argc, sizeof(void *) );
        for (i = 3; i < argc; i++) list.uris[list.count++] = make_uri( argv[i] );
        if (!strcmp( argv[1], "stage" ))
            hr = VT(manager, ManagerAllVtbl)->StagePackageAsync( manager, uri, list.count ? &list : NULL, &op );
        else
            hr = VT(m2, Manager2AllVtbl)->StagePackageWithOptionsAsync( m2, uri, list.count ? &list : NULL, 0, &op );
        printf( "StagePackageAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Stage", op ) : 1;
    }
    if (argc >= 3 && !strcmp( argv[1], "registerfull" ))
    {
        struct str_list list = { &str_list_vtbl, 1 };
        int i;

        list.s = calloc( argc, sizeof(HSTRING) );
        for (i = 3; i < argc; i++) list.s[list.count++] = H( argv[i] );
        hr = VT(m2, Manager2AllVtbl)->RegisterPackageByFullNameAsync( m2, H( argv[2] ), list.count ? &list : NULL, 0, &op );
        printf( "RegisterPackageByFullNameAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Register", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "remove" ))
    {
        hr = VT(manager, ManagerVtbl)->RemovePackageAsync( manager, H( argv[2] ), &op );
        printf( "RemovePackageAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Remove", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "add" ))
    {
        void *uri = make_uri( argv[2] );
        hr = VT(manager, ManagerVtbl)->AddPackageAsync( manager, uri, NULL, 0, &op );
        printf( "AddPackageAsync=0x%08lx\n", (unsigned long)hr );
        if (FAILED(hr)) return 1;
        return wait_op( "Add", op );
    }
    if (argc == 3 && !strcmp( argv[1], "addcancel" ))
    {
        /* the operation is cancelled as soon as it exists */
        void *uri = make_uri( argv[2] ), *info = NULL;
        int status;

        hr = VT(manager, ManagerVtbl)->AddPackageAsync( manager, uri, NULL, 0, &op );
        printf( "AddPackageAsync=0x%08lx\n", (unsigned long)hr );
        if (FAILED(hr)) return 1;
        VT(op, InspVtbl)->QI( op, &IID_IAsyncInfo_, &info );
        hr = ((AsyncInfoCancelVtbl *)((Obj *)info)->vtbl)->Cancel( info );
        printf( "Cancel=0x%08lx\n", (unsigned long)hr );
        status = wait_status( op );
        printf( "CancelStatus=%d\n", status );
        /* cancelling the finished operation again does nothing */
        hr = ((AsyncInfoCancelVtbl *)((Obj *)info)->vtbl)->Cancel( info );
        printf( "CancelAgain=0x%08lx\n", (unsigned long)hr );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "opname" ))
    {
        void *uri = make_uri( argv[2] );
        hr = VT(manager, ManagerVtbl)->AddPackageAsync( manager, uri, NULL, 0, &op );
        if (FAILED(hr)) return 1;
        print_op_name( op );
        wait_status( op );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "find" ))
    {
        void *package = NULL;
        hr = VT(manager, ManagerVtbl)->FindPackageByPackageFullName( manager, H( argv[2] ), &package );
        printf( "Found=%d\n", SUCCEEDED(hr) && package ? 1 : 0 );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "listuser" ))
    {
        /* the packages of the user: how many have the name in their full name */
        void *list = NULL, *iter = NULL, *package, *id;
        boolean has = FALSE;
        HSTRING str;
        int n = 0;

        hr = ((HRESULT (WINAPI *)(void *, void **))((void **)(((Obj *)manager)->vtbl))[11])( manager, &list );
        if (SUCCEEDED(hr) && SUCCEEDED(VT(list, IterableVtbl)->First( list, &iter )))
        {
            VT(iter, IteratorVtbl)->get_HasCurrent( iter, &has );
            while (has)
            {
                if (SUCCEEDED(VT(iter, IteratorVtbl)->get_Current( iter, &package )) &&
                    SUCCEEDED(VT(package, PackageVtbl)->get_Id( package, &id )) &&
                    SUCCEEDED(VT(id, PackageIdVtbl)->get_FullName( id, &str )) && wcsstr( raw( str ), L"StainedGlass." ))
                    n++;
                VT(iter, IteratorVtbl)->MoveNext( iter, &has );
            }
        }
        printf( "Listed=%d\n", n );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "users" ))
    {
        void *list = NULL, *iter = NULL, *user = NULL;
        boolean has = FALSE;
        HSTRING sid = NULL;
        int state = -1, n = 0;

        hr = VT(manager, ManagerAllVtbl)->FindUsers( manager, H( argv[2] ), &list );
        printf( "FindUsers=0x%08lx\n", (unsigned long)hr );
        if (SUCCEEDED(hr) && SUCCEEDED(VT(list, IterableVtbl)->First( list, &iter )))
        {
            VT(iter, IteratorVtbl)->get_HasCurrent( iter, &has );
            while (has)
            {
                if (SUCCEEDED(VT(iter, IteratorVtbl)->get_Current( iter, &user )))
                {
                    VT(user, UserInfoVtbl)->get_UserSecurityId( user, &sid );
                    VT(user, UserInfoVtbl)->get_InstallState( user, &state );
                    printf( "User=%ls state=%d\n", raw( sid ), state );
                    n++;
                }
                VT(iter, IteratorVtbl)->MoveNext( iter, &has );
            }
        }
        printf( "Users=%d\n", n );
        return 0;
    }
    if (argc == 4 && !strcmp( argv[1], "setstate" ))
    {
        hr = VT(manager, ManagerAllVtbl)->SetPackageState( manager, H( argv[2] ), atoi( argv[3] ) );
        printf( "SetPackageState=0x%08lx\n", (unsigned long)hr );
        return 0;
    }
    if (argc == 3 && (!strcmp( argv[1], "status" ) || !strcmp( argv[1], "verify" ) || !strcmp( argv[1], "entries" )))
    {
        void *package = NULL, *p3 = NULL, *p4 = NULL;

        hr = VT(manager, ManagerAllVtbl)->FindPackageByPackageFullName( manager, H( argv[2] ), &package );
        if (FAILED(hr) || !package) { printf( "Found=0\n" ); return 0; }
        VT(package, InspVtbl)->QI( package, &IID_IPackage3_, &p3 );
        VT(package, InspVtbl)->QI( package, &IID_IPackage4_, &p4 );
        if (!strcmp( argv[1], "status" ))
        {
            void *status = NULL;
            boolean ok = 0, v[11];
            int i;
            static const char *names[] = { "NotAvailable", "PackageOffline", "DataOffline", "Disabled", "NeedsRemediation", "LicenseIssue",
                                           "Modified", "Tampered", "DependencyIssue", "Servicing", "DeploymentInProgress" };

            hr = VT(p3, Package3AllVtbl)->get_Status( p3, &status );
            printf( "get_Status=0x%08lx\n", (unsigned long)hr );
            if (FAILED(hr)) return 0;
            VT(status, StatusVtbl)->VerifyIsOK( status, &ok );
            for (i = 0; i < 11; i++) { v[i] = 0; VT(status, StatusVtbl)->get[i]( status, &v[i] ); printf( "%s=%d\n", names[i], v[i] ); }
            printf( "VerifyIsOK=%d\n", ok );
        }
        else if (!strcmp( argv[1], "verify" ))
        {
            void *vop = NULL;
            boolean result = 0;

            hr = VT(p4, Package4Vtbl)->VerifyContentIntegrityAsync( p4, &vop );
            printf( "VerifyContentIntegrityAsync=0x%08lx\n", (unsigned long)hr );
            if (FAILED(hr)) return 0;
            printf( "VerifyStatus=%d\n", wait_status( vop ) );
            VT(vop, BoolOpVtbl)->GetResults( vop, &result );
            printf( "Intact=%d\n", result );
        }
        else
        {
            void *eop = NULL, *view = NULL, *entry = NULL, *display = NULL, *logo = NULL;
            UINT32 n = 0;
            HSTRING str = NULL;
            Size size = { 44, 44 };

            hr = VT(p3, Package3AllVtbl)->GetAppListEntriesAsync( p3, &eop );
            printf( "GetAppListEntriesAsync=0x%08lx\n", (unsigned long)hr );
            if (FAILED(hr)) return 0;
            wait_status( eop );
            VT(eop, BoolOpVtbl)->GetResults( eop, &view );
            VT(view, VectorViewVtbl)->get_Size( view, &n );
            printf( "Entries=%u\n", n );
            if (n && SUCCEEDED(VT(view, VectorViewVtbl)->GetAt( view, 0, &entry )))
            {
                hr = VT(entry, EntryVtbl)->get_DisplayInfo( entry, &display );
                printf( "get_DisplayInfo=0x%08lx\n", (unsigned long)hr );
                if (SUCCEEDED(hr))
                {
                    VT(display, DisplayInfoVtbl)->get_DisplayName( display, &str );
                    printf( "DisplayName=%ls\n", raw( str ) );
                    VT(display, DisplayInfoVtbl)->get_Description( display, &str );
                    printf( "Description=%ls\n", raw( str ) );
                    hr = VT(display, DisplayInfoVtbl)->GetLogo( display, size, &logo );
                    printf( "GetLogo44=0x%08lx %s\n", (unsigned long)hr, logo ? "stream" : "none" );
                    size.w = size.h = 150;
                    logo = NULL;
                    hr = VT(display, DisplayInfoVtbl)->GetLogo( display, size, &logo );
                    printf( "GetLogo150=0x%08lx %s\n", (unsigned long)hr, logo ? "stream" : "none" );
                }
            }
        }
        return 0;
    }
    if (argc == 4 && !strcmp( argv[1], "cleanup" ))
    {
        hr = VT(manager, ManagerAllVtbl)->CleanupPackageForUserAsync( manager, H( argv[2] ), H( argv[3] ), &op );
        printf( "CleanupPackageForUserAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Cleanup", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "provision" ))
    {
        hr = VT(m6, Manager6AllVtbl)->ProvisionPackageForAllUsersAsync( m6, H( argv[2] ), &op );
        printf( "ProvisionPackageForAllUsersAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "Provision", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "stageuser" ))
    {
        hr = VT(m2, Manager2AllVtbl)->StageUserDataAsync( m2, H( argv[2] ), &op );
        printf( "StageUserDataAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "StageUser", op ) : 1;
    }
    if (argc == 3 && !strcmp( argv[1], "appinstaller" ))
    {
        void *uri = make_uri( argv[2] );
        hr = VT(m6, Manager6AllVtbl)->AddPackageByAppInstallerFileAsync( m6, uri, 0, NULL, &op );
        printf( "AddPackageByAppInstallerFileAsync=0x%08lx\n", (unsigned long)hr );
        return SUCCEEDED(hr) ? wait_op( "AppInstaller", op ) : 1;
    }
    printf( "usage: see the source\n" );
    return 2;
}
