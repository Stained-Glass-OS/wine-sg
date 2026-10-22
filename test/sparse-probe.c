/* sparse-probe: packages with external location ("sparse" packages), IPackageManager7-9 and the
 * option classes, for test/sparse-gate.sh (Stained Glass OS wine-sg 6026).
 *
 *   sparse-probe options                    the three option classes: defaults, vectors, names
 *   sparse-probe add PKG [EXTDIR] [unsigned] IPackageManager9.AddPackageByUriAsync with ExternalLocationUri
 *   sparse-probe stage PKG                  IPackageManager9.StagePackageByUriAsync
 *   sparse-probe register MANIFEST EXTDIR   IPackageManager9.RegisterPackageByUriAsync (developer mode)
 *   sparse-probe provision FAMILY           IPackageManager6.ProvisionPackageForAllUsersAsync
 *   sparse-probe deprovision FAMILY         IPackageManager8.DeprovisionPackageForAllUsersAsync
 *   sparse-probe provisioned                IPackageManager9.FindProvisionedPackages: count, full names
 *   sparse-probe volume FAMILY              the default volume's FindPackages / ByPackageFamilyName counts
 *   sparse-probe stub FAMILY                Get/SetPackageStubPreference
 *   sparse-probe request PKG                IPackageManager7.RequestAddPackageAsync
 *   sparse-probe identity                   GetCurrentPackageFullName of this process
 *   sparse-probe lnk PATH                   a shortcut's target
 *   sparse-probe remove FULLNAME            RemovePackageAsync
 *
 * Interfaces are declared here from their published IIDs (the MIT windows crate).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <appmodel.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 void *GetIids; HRESULT (WINAPI *GetRuntimeClassName)(void *, HSTRING *); void *GetTrustLevel; } InspVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *ActivateInstance)(void *, void **); } FactoryVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *CreateUri)(void *, HSTRING, void **); } UriFactoryVtbl;
typedef struct { InspVtbl i; void *AddPackageAsync, *UpdatePackageAsync;
                 HRESULT (WINAPI *RemovePackageAsync)(void *, HSTRING, void **);
                 void *StagePackageAsync, *RegisterPackageAsync, *FindPackages, *FindPackagesByUserSecurityId,
                      *FindPackagesByNamePublisher, *FindPackagesByUserSecurityIdNamePublisher, *FindUsers, *SetPackageState;
                 HRESULT (WINAPI *FindPackageByPackageFullName)(void *, HSTRING, void **); } ManagerVtbl;
typedef struct { InspVtbl i; void *AddPackageVolumeAsync, *AddPackageToVolumeAsync, *ClearPackageStatus,
                 *RegisterPackageWithAppDataVolumeAsync, *FindPackageVolumeByName, *FindPackageVolumes;
                 HRESULT (WINAPI *GetDefaultPackageVolume)(void *, void **); } Manager3Vtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *ProvisionPackageForAllUsersAsync)(void *, HSTRING, void **); } Manager6Vtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *RequestAddPackageAsync)(void *, void *, void *, UINT32, void *, void *, void *, void *,
                                                                        void **); } Manager7Vtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *DeprovisionPackageForAllUsersAsync)(void *, HSTRING, void **); } Manager8Vtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *FindProvisionedPackages)(void *, void **);
                 HRESULT (WINAPI *AddPackageByUriAsync)(void *, void *, void *, void **);
                 HRESULT (WINAPI *StagePackageByUriAsync)(void *, void *, void *, void **);
                 HRESULT (WINAPI *RegisterPackageByUriAsync)(void *, void *, void *, void **);
                 void *RegisterPackagesByFullNameAsync;
                 HRESULT (WINAPI *SetPackageStubPreference)(void *, HSTRING, int);
                 HRESULT (WINAPI *GetPackageStubPreference)(void *, HSTRING, int *); } Manager9Vtbl;
typedef struct { InspVtbl i;
                 HRESULT (WINAPI *get_DependencyPackageUris)(void *, void **);
                 HRESULT (WINAPI *get_TargetVolume)(void *, void **); HRESULT (WINAPI *put_TargetVolume)(void *, void *);
                 HRESULT (WINAPI *get_OptionalPackageFamilyNames)(void *, void **);
                 HRESULT (WINAPI *get_OptionalPackageUris)(void *, void **);
                 HRESULT (WINAPI *get_RelatedPackageUris)(void *, void **);
                 HRESULT (WINAPI *get_ExternalLocationUri)(void *, void **); HRESULT (WINAPI *put_ExternalLocationUri)(void *, void *);
                 HRESULT (WINAPI *get_StubPackageOption)(void *, int *); HRESULT (WINAPI *put_StubPackageOption)(void *, int);
                 HRESULT (WINAPI *get_DeveloperMode)(void *, boolean *); HRESULT (WINAPI *put_DeveloperMode)(void *, boolean);
                 void *g1, *p1, *g2, *p2, *g3, *p3, *g4, *p4, *g5, *p5, *g6, *p6, *g7, *p7;
                 HRESULT (WINAPI *get_AllowUnsigned)(void *, boolean *); HRESULT (WINAPI *put_AllowUnsigned)(void *, boolean); } AddOptionsVtbl;
typedef struct { InspVtbl i; void *DependencyPackageUris, *AppDataVolume, *SetAppDataVolume, *OptionalPackageFamilyNames;
                 HRESULT (WINAPI *get_ExternalLocationUri)(void *, void **); HRESULT (WINAPI *put_ExternalLocationUri)(void *, void *);
                 HRESULT (WINAPI *get_DeveloperMode)(void *, boolean *); HRESULT (WINAPI *put_DeveloperMode)(void *, boolean); } RegOptionsVtbl;
typedef struct { InspVtbl i; void *get_Progress, *put_Progress, *put_Completed, *get_Completed;
                 HRESULT (WINAPI *GetResults)(void *, void **); } OperationVtbl;
typedef struct { InspVtbl i; void *get_Id; HRESULT (WINAPI *get_Status)(void *, int *); HRESULT (WINAPI *get_ErrorCode)(void *, HRESULT *); } AsyncInfoVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_ErrorText)(void *, HSTRING *); void *get_ActivityId;
                 HRESULT (WINAPI *get_ExtendedErrorCode)(void *, HRESULT *); } ResultVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *GetAt)(void *, UINT32, void **); HRESULT (WINAPI *get_Size)(void *, UINT32 *);
                 void *GetView, *IndexOf, *SetAt, *InsertAt, *RemoveAt; HRESULT (WINAPI *Append)(void *, void *); } VectorVtbl;
typedef struct { InspVtbl i; void *IsOffline, *IsSystemVolume, *MountPoint, *Name, *PackageStorePath, *SupportsHardLinks;
                 HRESULT (WINAPI *FindPackages)(void *, void **); void *FindPackagesByNamePublisher;
                 HRESULT (WINAPI *FindPackagesByPackageFamilyName)(void *, HSTRING, void **); } VolumeVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_Id)(void *, void **); HRESULT (WINAPI *get_InstalledLocation)(void *, void **); } PackageVtbl;
typedef struct { InspVtbl i; void *Name, *Version, *Architecture, *ResourceId, *Publisher, *PublisherId;
                 HRESULT (WINAPI *get_FullName)(void *, HSTRING *); } PackageIdVtbl;
typedef struct { InspVtbl i; void *r1, *r2, *d1, *d2, *props, *name; HRESULT (WINAPI *get_Path)(void *, HSTRING *); } StorageItemVtbl;
typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID IID_IActivationFactory_ = {0x00000035,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IAsyncInfo_ = {0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IPackageManager_ = {0x9a7d4b65,0x5e8f,0x4fc7,{0xa2,0xe5,0x7f,0x69,0x25,0xcb,0x8b,0x53}};
static const GUID IID_IPackageManager3_ = {0xdaad9948,0x36f1,0x41a7,{0x91,0x88,0xbc,0x26,0x3e,0x0d,0xcb,0x72}};
static const GUID IID_IPackageManager6_ = {0x0847e909,0x53cd,0x4e4f,{0x83,0x2e,0x57,0xd1,0x80,0xf6,0xe4,0x47}};
static const GUID IID_IPackageManager7_ = {0xf28654f4,0x2ba7,0x4b80,{0x88,0xd6,0xbe,0x15,0xf9,0xa2,0x3f,0xba}};
static const GUID IID_IPackageManager8_ = {0xb8575330,0x1298,0x4ee2,{0x80,0xee,0x7f,0x65,0x9c,0x5d,0x27,0x82}};
static const GUID IID_IPackageManager9_ = {0x1aa79035,0xcc71,0x4b2e,{0x80,0xa6,0xc7,0x04,0x1d,0x85,0x79,0xa7}};
static const GUID IID_IAddPackageOptions_ = {0x05cee018,0xf68f,0x422b,{0x95,0xa4,0x66,0x67,0x9e,0xc7,0x7f,0xc0}};
static const GUID IID_IAddPackageOptions2_ = {0xee515828,0xbf33,0x40f7,{0x84,0xaf,0x1b,0x6f,0xad,0x29,0x19,0xd7}};
static const GUID IID_IStagePackageOptions_ = {0x0b110c9c,0xb95d,0x4c56,{0xbd,0x36,0x6d,0x65,0x68,0x00,0xd0,0x6b}};
static const GUID IID_IRegisterPackageOptions_ = {0x677112a7,0x50d4,0x496c,{0x84,0x15,0x06,0x02,0xb4,0xc6,0xd3,0xbf}};
static const GUID IID_IVector_Uri_ = {0x0d82bd8d,0xfe62,0x5d67,{0xa7,0xb9,0x78,0x86,0xdd,0x75,0xbc,0x4e}};
static const GUID IID_IVector_HSTRING_ = {0x98b9acc1,0x4b56,0x532e,{0xac,0x73,0x03,0xd5,0x29,0x1c,0xca,0x90}};
static const GUID IID_IVector_Package_ = {0xd1bb509e,0x6989,0x5c69,{0xb1,0xff,0xd1,0x70,0x2f,0xe8,0xac,0xa3}};
static const GUID IID_IUriRuntimeClassFactory_ = {0x44a9796f,0x723e,0x4fdf,{0xa2,0x18,0x03,0x3e,0x75,0xb0,0xc0,0x84}};
static const GUID IID_IStorageItem_ = {0x4207a996,0xca2f,0x42f7,{0xbd,0xe8,0x8b,0x10,0x45,0x7a,0x7f,0x30}};

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

static HSTRING hsa( const char *s )
{
    WCHAR w[1024];
    MultiByteToWideChar( CP_ACP, 0, s, -1, w, ARRAYSIZE(w) );
    return hs( w );
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
    UrlCreateFromPathW( w, url, &len, 0 );
    if (uri_factory) VT(uri_factory, UriFactoryVtbl)->CreateUri( uri_factory, hs( url ), &uri );
    return uri;
}

static void *manager( const GUID *iid )
{
    return activate( L"Windows.Management.Deployment.PackageManager", iid );
}

/* Waits for a deployment operation; prints its outcome. */
static int wait_op( HRESULT hr, void **opp )
{
    void *op = *opp;
    void *info = NULL, *result = NULL;
    HRESULT err = S_OK, ext = S_OK;
    HSTRING text = NULL;
    int status = 0, i;

    if (FAILED(hr) || !op)
    {
        printf( "call=%08lx\n", hr );
        return 1;
    }
    VT(op, InspVtbl)->QI( op, &IID_IAsyncInfo_, &info );
    for (i = 0; i < 1200; i++)
    {
        VT(info, AsyncInfoVtbl)->get_Status( info, &status );
        if (status) break;
        Sleep( 100 );
    }
    VT(info, AsyncInfoVtbl)->get_ErrorCode( info, &err );
    VT(op, OperationVtbl)->GetResults( op, &result );
    if (result)
    {
        VT(result, ResultVtbl)->get_ExtendedErrorCode( result, &ext );
        VT(result, ResultVtbl)->get_ErrorText( result, &text );
    }
    printf( "status=%d\nerror=%08lx\nextended=%08lx\ntext=%ls\n", status, err, ext, raw( text ) );
    return status != 1;
}

static void print_bool( const char *name, HRESULT hr, const boolean *b ) { printf( "%s=%s\n", name, FAILED(hr) ? "fail" : *b ? "true" : "false" ); }

static void print_class( const char *key, void *obj )
{
    HSTRING name = NULL;
    if (obj) VT(obj, InspVtbl)->GetRuntimeClassName( obj, &name );
    printf( "%s=%ls\n", key, raw( name ) );
}

static int cmd_options(void)
{
    void *add = activate( L"Windows.Management.Deployment.AddPackageOptions", &IID_IAddPackageOptions_ );
    void *stage = activate( L"Windows.Management.Deployment.StagePackageOptions", &IID_IStagePackageOptions_ );
    void *reg = activate( L"Windows.Management.Deployment.RegisterPackageOptions", &IID_IRegisterPackageOptions_ );
    void *vec = NULL, *vec2 = NULL, *names = NULL, *uri = NULL, *other = NULL, *got = NULL;
    UINT32 size = 99;
    boolean b = TRUE;
    int stub = -1;

    printf( "add=%s\nstage=%s\nregister=%s\n", add ? "yes" : "no", stage ? "yes" : "no", reg ? "yes" : "no" );
    if (!add || !stage || !reg) return 1;
    print_class( "add.class", add );
    print_class( "stage.class", stage );
    print_class( "register.class", reg );
    printf( "add.options2=%s\n", SUCCEEDED(VT(add, InspVtbl)->QI( add, &IID_IAddPackageOptions2_, &other )) ? "yes" : "no" );
    printf( "add.stage=%s\n", SUCCEEDED(VT(add, InspVtbl)->QI( add, &IID_IStagePackageOptions_, &other )) ? "yes" : "no" );
    VT(add, AddOptionsVtbl)->get_ExternalLocationUri( add, &got );
    printf( "add.external=%s\n", got ? "set" : "null" );
    print_bool( "add.allow_unsigned", VT(add, AddOptionsVtbl)->get_AllowUnsigned( add, &b ), &b );
    print_bool( "add.developer", VT(add, AddOptionsVtbl)->get_DeveloperMode( add, &b ), &b );
    VT(add, AddOptionsVtbl)->get_StubPackageOption( add, &stub );
    printf( "add.stub=%d\n", stub );
    printf( "add.stub_bad=%08lx\n", VT(add, AddOptionsVtbl)->put_StubPackageOption( add, 7 ) );
    VT(add, AddOptionsVtbl)->get_DependencyPackageUris( add, &vec );
    printf( "add.deps.vector=%s\n", vec && SUCCEEDED(VT(vec, InspVtbl)->QI( vec, &IID_IVector_Uri_, &other )) ? "yes" : "no" );
    if (vec) VT(vec, VectorVtbl)->get_Size( vec, &size );
    printf( "add.deps.size=%u\n", size );
    uri = make_uri( "C:\\windows" );
    if (vec && uri) VT(vec, VectorVtbl)->Append( vec, uri );
    VT(add, AddOptionsVtbl)->get_DependencyPackageUris( add, &vec2 );
    size = 99;
    if (vec2) VT(vec2, VectorVtbl)->get_Size( vec2, &size );
    printf( "add.deps.kept=%u\n", size );
    VT(add, AddOptionsVtbl)->get_OptionalPackageFamilyNames( add, &names );
    printf( "add.names.vector=%s\n", names && SUCCEEDED(VT(names, InspVtbl)->QI( names, &IID_IVector_HSTRING_, &other )) ? "yes" : "no" );
    VT(add, AddOptionsVtbl)->put_ExternalLocationUri( add, uri );
    got = NULL;
    VT(add, AddOptionsVtbl)->get_ExternalLocationUri( add, &got );
    printf( "add.external.kept=%s\n", got == uri && uri ? "yes" : "no" );
    VT(add, AddOptionsVtbl)->put_AllowUnsigned( add, TRUE );
    b = FALSE;
    print_bool( "add.allow_unsigned.kept", VT(add, AddOptionsVtbl)->get_AllowUnsigned( add, &b ), &b );
    VT(reg, RegOptionsVtbl)->put_ExternalLocationUri( reg, uri );
    got = NULL;
    VT(reg, RegOptionsVtbl)->get_ExternalLocationUri( reg, &got );
    printf( "register.external.kept=%s\n", got == uri && uri ? "yes" : "no" );
    return 0;
}

static void *options_for( const char *ext, BOOL allow_unsigned )
{
    void *add = activate( L"Windows.Management.Deployment.AddPackageOptions", &IID_IAddPackageOptions_ );
    if (!add) return NULL;
    if (ext && *ext) VT(add, AddOptionsVtbl)->put_ExternalLocationUri( add, make_uri( ext ) );
    if (allow_unsigned) VT(add, AddOptionsVtbl)->put_AllowUnsigned( add, TRUE );
    return add;
}

static int cmd_add( int argc, char **argv )
{
    void *pm = manager( &IID_IPackageManager9_ ), *op = NULL;
    void *opts = options_for( argc > 3 ? argv[3] : NULL, argc > 4 && !strcmp( argv[4], "unsigned" ) );

    printf( "pm9=%s\n", pm ? "yes" : "no" );
    if (!pm || !opts) return 1;
    return wait_op( VT(pm, Manager9Vtbl)->AddPackageByUriAsync( pm, make_uri( argv[2] ), opts, &op ), &op );
}

static int cmd_stage( char *path )
{
    void *pm = manager( &IID_IPackageManager9_ ), *op = NULL;
    void *opts = activate( L"Windows.Management.Deployment.StagePackageOptions", &IID_IStagePackageOptions_ );

    if (!pm || !opts) { printf( "pm9=no\n" ); return 1; }
    return wait_op( VT(pm, Manager9Vtbl)->StagePackageByUriAsync( pm, make_uri( path ), opts, &op ), &op );
}

static int cmd_register( char *manifest, char *ext )
{
    void *pm = manager( &IID_IPackageManager9_ ), *op = NULL;
    void *opts = activate( L"Windows.Management.Deployment.RegisterPackageOptions", &IID_IRegisterPackageOptions_ );

    if (!pm || !opts) { printf( "pm9=no\n" ); return 1; }
    VT(opts, RegOptionsVtbl)->put_DeveloperMode( opts, TRUE );
    VT(opts, RegOptionsVtbl)->put_ExternalLocationUri( opts, make_uri( ext ) );
    return wait_op( VT(pm, Manager9Vtbl)->RegisterPackageByUriAsync( pm, make_uri( manifest ), opts, &op ), &op );
}

static int cmd_request( char *path )
{
    void *pm = manager( &IID_IPackageManager7_ ), *op = NULL;
    if (!pm) { printf( "pm7=no\n" ); return 1; }
    return wait_op( VT(pm, Manager7Vtbl)->RequestAddPackageAsync( pm, make_uri( path ), NULL, 0, NULL, NULL, NULL, NULL, &op ), &op );
}

static void print_packages( const char *key, void *vec )
{
    UINT32 size = 0, i;
    void *pkg, *id;
    HSTRING name;

    if (!vec) { printf( "%s=fail\n", key ); return; }
    printf( "%s.vector=%s\n", key, SUCCEEDED(VT(vec, InspVtbl)->QI( vec, &IID_IVector_Package_, &pkg )) ? "yes" : "no" );
    VT(vec, VectorVtbl)->get_Size( vec, &size );
    printf( "%s=%u\n", key, size );
    for (i = 0; i < size; i++)
    {
        pkg = id = NULL;
        name = NULL;
        VT(vec, VectorVtbl)->GetAt( vec, i, &pkg );
        if (pkg) VT(pkg, PackageVtbl)->get_Id( pkg, &id );
        if (id) VT(id, PackageIdVtbl)->get_FullName( id, &name );
        printf( "%s.%u=%ls\n", key, i, raw( name ) );
    }
}

static int cmd_provisioned(void)
{
    void *pm = manager( &IID_IPackageManager9_ ), *vec = NULL;
    HRESULT hr;

    if (!pm) { printf( "pm9=no\n" ); return 1; }
    hr = VT(pm, Manager9Vtbl)->FindProvisionedPackages( pm, &vec );
    printf( "hr=%08lx\n", hr );
    print_packages( "provisioned", vec );
    return 0;
}

static int cmd_provision( char *family, BOOL undo )
{
    void *op = NULL, *pm;
    if (undo)
    {
        if (!(pm = manager( &IID_IPackageManager8_ ))) { printf( "pm8=no\n" ); return 1; }
        return wait_op( VT(pm, Manager8Vtbl)->DeprovisionPackageForAllUsersAsync( pm, hsa( family ), &op ), &op );
    }
    if (!(pm = manager( &IID_IPackageManager6_ ))) { printf( "pm6=no\n" ); return 1; }
    return wait_op( VT(pm, Manager6Vtbl)->ProvisionPackageForAllUsersAsync( pm, hsa( family ), &op ), &op );
}

static int cmd_volume( char *family )
{
    void *pm = manager( &IID_IPackageManager3_ ), *vol = NULL, *vec = NULL;

    if (!pm) { printf( "pm3=no\n" ); return 1; }
    VT(pm, Manager3Vtbl)->GetDefaultPackageVolume( pm, &vol );
    if (!vol) { printf( "volume=no\n" ); return 1; }
    printf( "find=%08lx\n", VT(vol, VolumeVtbl)->FindPackages( vol, &vec ) );
    print_packages( "all", vec );
    vec = NULL;
    printf( "find_family=%08lx\n", VT(vol, VolumeVtbl)->FindPackagesByPackageFamilyName( vol, hsa( family ), &vec ) );
    print_packages( "family", vec );
    return 0;
}

static int cmd_stub( char *family )
{
    void *pm = manager( &IID_IPackageManager9_ );
    int pref = -1;

    if (!pm) { printf( "pm9=no\n" ); return 1; }
    printf( "get=%08lx\n", VT(pm, Manager9Vtbl)->GetPackageStubPreference( pm, hsa( family ), &pref ) );
    printf( "preference=%d\n", pref );
    printf( "set_bad=%08lx\n", VT(pm, Manager9Vtbl)->SetPackageStubPreference( pm, hsa( family ), 5 ) );
    printf( "set_full=%08lx\n", VT(pm, Manager9Vtbl)->SetPackageStubPreference( pm, hsa( family ), 0 ) );
    return 0;
}

static int cmd_info( char *full_name )
{
    void *pm = manager( &IID_IPackageManager_ ), *pkg = NULL, *folder = NULL, *item = NULL;
    HSTRING path = NULL;

    if (!pm) return 1;
    VT(pm, ManagerVtbl)->FindPackageByPackageFullName( pm, hsa( full_name ), &pkg );
    if (!pkg) { printf( "found=no\n" ); return 1; }
    VT(pkg, PackageVtbl)->get_InstalledLocation( pkg, &folder );
    if (folder) VT(folder, InspVtbl)->QI( folder, &IID_IStorageItem_, &item );
    if (item) VT(item, StorageItemVtbl)->get_Path( item, &path );
    printf( "found=yes\nlocation=%ls\n", raw( path ) );
    return 0;
}

static int cmd_remove( char *full_name )
{
    void *pm = manager( &IID_IPackageManager_ ), *op = NULL;
    if (!pm) return 1;
    return wait_op( VT(pm, ManagerVtbl)->RemovePackageAsync( pm, hsa( full_name ), &op ), &op );
}

static int cmd_identity(void)
{
    WCHAR name[256];
    UINT32 len = ARRAYSIZE(name);
    LONG ret = GetCurrentPackageFullName( &len, name );

    printf( "identity=%ls\n", ret ? L"none" : name );
    return 0;
}

static int cmd_lnk( char *path )
{
    IShellLinkW *link;
    IPersistFile *file;
    WCHAR w[MAX_PATH], target[MAX_PATH] = L"";

    MultiByteToWideChar( CP_ACP, 0, path, -1, w, MAX_PATH );
    if (FAILED(CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link ))) return 1;
    IShellLinkW_QueryInterface( link, &IID_IPersistFile, (void **)&file );
    if (SUCCEEDED(IPersistFile_Load( file, w, STGM_READ ))) IShellLinkW_GetPath( link, target, MAX_PATH, NULL, 0 );
    printf( "target=%ls\n", target );
    return 0;
}

int main( int argc, char **argv )
{
    int ret = 2;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc < 2) return 2;
    RoInitialize( RO_INIT_MULTITHREADED );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    if (!strcmp( argv[1], "options" )) ret = cmd_options();
    else if (!strcmp( argv[1], "add" ) && argc > 2) ret = cmd_add( argc, argv );
    else if (!strcmp( argv[1], "stage" ) && argc > 2) ret = cmd_stage( argv[2] );
    else if (!strcmp( argv[1], "register" ) && argc > 3) ret = cmd_register( argv[2], argv[3] );
    else if (!strcmp( argv[1], "request" ) && argc > 2) ret = cmd_request( argv[2] );
    else if (!strcmp( argv[1], "provision" ) && argc > 2) ret = cmd_provision( argv[2], FALSE );
    else if (!strcmp( argv[1], "deprovision" ) && argc > 2) ret = cmd_provision( argv[2], TRUE );
    else if (!strcmp( argv[1], "provisioned" )) ret = cmd_provisioned();
    else if (!strcmp( argv[1], "volume" ) && argc > 2) ret = cmd_volume( argv[2] );
    else if (!strcmp( argv[1], "stub" ) && argc > 2) ret = cmd_stub( argv[2] );
    else if (!strcmp( argv[1], "info" ) && argc > 2) ret = cmd_info( argv[2] );
    else if (!strcmp( argv[1], "remove" ) && argc > 2) ret = cmd_remove( argv[2] );
    else if (!strcmp( argv[1], "identity" )) ret = cmd_identity();
    else if (!strcmp( argv[1], "lnk" ) && argc > 2) ret = cmd_lnk( argv[2] );
    return ret;
}
