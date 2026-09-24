/* deploy-probe: MSIX deployment through PackageManager (patch 0060), as a
 * program uses it, for test/appx-gate.sh.
 *
 *   deploy-probe add PATH       AddPackageAsync(file URI), then wait
 *   deploy-probe find NAME PUB  FindPackagesByNamePublisher: count, full name, location
 *   deploy-probe remove FULL    RemovePackageAsync, then wait
 *
 * Interfaces are declared here from their published IIDs.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <shlwapi.h>
#include <stdio.h>

typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 void *GetIids, *GetRuntimeClassName, *GetTrustLevel; } InspVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *ActivateInstance)(void *, void **); } FactoryVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *CreateUri)(void *, HSTRING, void **); } UriFactoryVtbl;
typedef struct { InspVtbl i;
                 HRESULT (WINAPI *AddPackageAsync)(void *, void *, void *, UINT32, void **);
                 void *UpdatePackageAsync;
                 HRESULT (WINAPI *RemovePackageAsync)(void *, HSTRING, void **);
                 void *StagePackageAsync, *RegisterPackageAsync, *FindPackages, *FindPackagesByUserSecurityId;
                 HRESULT (WINAPI *FindPackagesByNamePublisher)(void *, HSTRING, HSTRING, void **); } ManagerVtbl;
typedef struct { InspVtbl i; void *put_Progress, *get_Progress, *put_Completed, *get_Completed;
                 HRESULT (WINAPI *GetResults)(void *, void **); } OperationVtbl;
typedef struct { InspVtbl i; void *get_Id; HRESULT (WINAPI *get_Status)(void *, int *); HRESULT (WINAPI *get_ErrorCode)(void *, HRESULT *); } AsyncInfoVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_ErrorText)(void *, HSTRING *); void *get_ActivityId;
                 HRESULT (WINAPI *get_ExtendedErrorCode)(void *, HRESULT *); } ResultVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *First)(void *, void **); } IterableVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_Current)(void *, void **); HRESULT (WINAPI *get_HasCurrent)(void *, boolean *);
                 HRESULT (WINAPI *MoveNext)(void *, boolean *); } IteratorVtbl;
typedef struct { InspVtbl i; HRESULT (WINAPI *get_Id)(void *, void **); HRESULT (WINAPI *get_InstalledLocation)(void *, void **); } PackageVtbl;
typedef struct { InspVtbl i; void *Name, *Version, *Architecture, *ResourceId, *Publisher, *PublisherId;
                 HRESULT (WINAPI *get_FullName)(void *, HSTRING *); } PackageIdVtbl;
typedef struct { InspVtbl i; void *r1, *r2, *d1, *d2, *props, *name; HRESULT (WINAPI *get_Path)(void *, HSTRING *); } StorageItemVtbl;
typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID IID_IActivationFactory_ = {0x00000035,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IAsyncInfo_ = {0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IPackageManager_ = {0x9a7d4b65,0x5e8f,0x4fc7,{0xa2,0xe5,0x7f,0x69,0x25,0xcb,0x8b,0x53}};
static const GUID IID_IUriRuntimeClassFactory_ = {0x44a9796f,0x723e,0x4fdf,{0xa2,0x18,0x03,0x3e,0x75,0xb0,0xc0,0x84}};
static const GUID IID_IStorageItem_ = {0x4207a996,0xca2f,0x42f7,{0xbd,0xe8,0x8b,0x10,0x45,0x7a,0x7f,0x30}};

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, lstrlenW( s ), &h );
    return h;
}

static void *activate( const WCHAR *class, const GUID *iid )
{
    void *factory = NULL, *inst = NULL, *out = NULL;
    HSTRING name = hs( class );
    if (FAILED(RoGetActivationFactory( name, &IID_IActivationFactory_, &factory ))) return NULL;
    if (iid == &IID_IUriRuntimeClassFactory_)
    {
        VT(factory, InspVtbl)->QI( factory, iid, &out );
        return out;
    }
    if (SUCCEEDED(VT(factory, FactoryVtbl)->ActivateInstance( factory, &inst ))) VT(inst, InspVtbl)->QI( inst, iid, &out );
    return out;
}

/* Wait for a deployment operation; print its status, error and result. */
static void wait_op( const char *what, void *op )
{
    void *info = NULL, *result = NULL;
    HRESULT error = S_OK, extended = S_OK;
    HSTRING text = NULL;
    int status = 0, i;

    VT(op, InspVtbl)->QI( op, &IID_IAsyncInfo_, &info );
    for (i = 0; i < 1200 && info; i++)
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
        printf( "%sExtended=0x%08lx\n%sText=%ls\n", what, (unsigned long)extended, what,
                text ? WindowsGetStringRawBuffer( text, NULL ) : L"" );
    }
}

int main( int argc, char **argv )
{
    WCHAR a1[MAX_PATH], a2[MAX_PATH], url[MAX_PATH * 3];
    void *manager, *op = NULL;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    if (!(manager = activate( L"Windows.Management.Deployment.PackageManager", &IID_IPackageManager_ )))
    {
        printf( "PackageManager=none\n" );
        return 1;
    }
    if (argc > 2) MultiByteToWideChar( CP_ACP, 0, argv[2], -1, a1, MAX_PATH );
    if (argc > 3) MultiByteToWideChar( CP_ACP, 0, argv[3], -1, a2, MAX_PATH );

    if (argc == 3 && !strcmp( argv[1], "add" ))
    {
        void *uri_factory = activate( L"Windows.Foundation.Uri", &IID_IUriRuntimeClassFactory_ ), *uri = NULL;
        DWORD len = ARRAYSIZE(url);

        UrlCreateFromPathW( a1, url, &len, 0 );
        if (!uri_factory || FAILED(VT(uri_factory, UriFactoryVtbl)->CreateUri( uri_factory, hs( url ), &uri )))
        {
            printf( "Uri=none\n" );
            return 1;
        }
        hr = VT(manager, ManagerVtbl)->AddPackageAsync( manager, uri, NULL, 0, &op );
        printf( "AddPackageAsync=0x%08lx\n", (unsigned long)hr );
        if (SUCCEEDED(hr)) wait_op( "Add", op );
    }
    else if (argc == 4 && !strcmp( argv[1], "find" ))
    {
        void *list = NULL, *iter = NULL, *package, *id, *folder, *item;
        boolean has = FALSE;
        HSTRING str;
        int count = 0;

        hr = VT(manager, ManagerVtbl)->FindPackagesByNamePublisher( manager, hs( a1 ), hs( a2 ), &list );
        printf( "Find=0x%08lx\n", (unsigned long)hr );
        if (SUCCEEDED(hr) && SUCCEEDED(VT(list, IterableVtbl)->First( list, &iter )))
        {
            VT(iter, IteratorVtbl)->get_HasCurrent( iter, &has );
            while (has)
            {
                count++;
                if (SUCCEEDED(VT(iter, IteratorVtbl)->get_Current( iter, &package )))
                {
                    if (SUCCEEDED(VT(package, PackageVtbl)->get_Id( package, &id )) &&
                        SUCCEEDED(VT(id, PackageIdVtbl)->get_FullName( id, &str )))
                        printf( "FullName=%ls\n", WindowsGetStringRawBuffer( str, NULL ) );
                    if (SUCCEEDED(VT(package, PackageVtbl)->get_InstalledLocation( package, &folder )) &&
                        SUCCEEDED(VT(folder, InspVtbl)->QI( folder, &IID_IStorageItem_, &item )) &&
                        SUCCEEDED(VT(item, StorageItemVtbl)->get_Path( item, &str )))
                        printf( "Location=%ls\n", WindowsGetStringRawBuffer( str, NULL ) );
                }
                VT(iter, IteratorVtbl)->MoveNext( iter, &has );
            }
        }
        printf( "Count=%d\n", count );
    }
    else if (argc == 3 && !strcmp( argv[1], "remove" ))
    {
        hr = VT(manager, ManagerVtbl)->RemovePackageAsync( manager, hs( a1 ), &op );
        printf( "RemovePackageAsync=0x%08lx\n", (unsigned long)hr );
        if (SUCCEEDED(hr)) wait_op( "Remove", op );
    }
    else
    {
        printf( "usage: deploy-probe add PATH | find NAME PUBLISHER | remove FULLNAME\n" );
        return 2;
    }
    return 0;
}
