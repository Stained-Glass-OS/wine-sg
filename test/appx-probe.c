/* appx-probe: exercise the AppX packaging API (appxpackaging.dll) the way
 * package tools -- winget among them -- use it, for test/appx-gate.sh.
 *
 *   appx-probe info PACKAGE                identity, properties, files
 *   appx-probe extract PACKAGE NAME OUT    write a payload file to OUT
 *   appx-probe bundle PACKAGE              what CreateBundleReader says
 *
 * Prints KEY=VALUE lines; HRESULTs as hex. The interfaces are declared here
 * from their published IIDs so the probe needs no SDK headers.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>
#include <stdio.h>

typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *); } UnkVtbl;
typedef struct { UnkVtbl u; void *CreatePackageWriter; HRESULT (WINAPI *CreatePackageReader)(void *, IStream *, void **);
                 HRESULT (WINAPI *CreateManifestReader)(void *, IStream *, void **); } FactoryVtbl;
typedef struct { UnkVtbl u; void *CreateBundleWriter; HRESULT (WINAPI *CreateBundleReader)(void *, IStream *, void **); } BundleFactoryVtbl;
typedef struct { UnkVtbl u; void *GetBlockMap; HRESULT (WINAPI *GetFootprintFile)(void *, int, void **);
                 HRESULT (WINAPI *GetPayloadFile)(void *, const WCHAR *, void **); HRESULT (WINAPI *GetPayloadFiles)(void *, void **);
                 HRESULT (WINAPI *GetManifest)(void *, void **); } ReaderVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetCompressionOption)(void *, int *); HRESULT (WINAPI *GetContentType)(void *, WCHAR **);
                 HRESULT (WINAPI *GetName)(void *, WCHAR **); HRESULT (WINAPI *GetSize)(void *, UINT64 *);
                 HRESULT (WINAPI *GetStream)(void *, IStream **); } FileVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetCurrent)(void *, void **); HRESULT (WINAPI *GetHasCurrent)(void *, BOOL *);
                 HRESULT (WINAPI *MoveNext)(void *, BOOL *); } EnumVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetPackageId)(void *, void **); HRESULT (WINAPI *GetProperties)(void *, void **);
                 void *GetPackageDependencies; HRESULT (WINAPI *GetCapabilities)(void *, int *); void *GetResources;
                 void *GetDeviceCapabilities; HRESULT (WINAPI *GetPrerequisite)(void *, const WCHAR *, UINT64 *);
                 HRESULT (WINAPI *GetApplications)(void *, void **); HRESULT (WINAPI *GetStream)(void *, IStream **); } ManifestVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetName)(void *, WCHAR **); HRESULT (WINAPI *GetArchitecture)(void *, int *);
                 HRESULT (WINAPI *GetPublisher)(void *, WCHAR **); HRESULT (WINAPI *GetVersion)(void *, UINT64 *);
                 HRESULT (WINAPI *GetResourceId)(void *, WCHAR **); HRESULT (WINAPI *ComparePublisher)(void *, const WCHAR *, BOOL *);
                 HRESULT (WINAPI *GetPackageFullName)(void *, WCHAR **); HRESULT (WINAPI *GetPackageFamilyName)(void *, WCHAR **); } IdVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetBoolValue)(void *, const WCHAR *, BOOL *);
                 HRESULT (WINAPI *GetStringValue)(void *, const WCHAR *, WCHAR **); } PropsVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetStringValue)(void *, const WCHAR *, WCHAR **);
                 HRESULT (WINAPI *GetAppUserModelId)(void *, WCHAR **); } AppVtbl;

typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID CLSID_AppxFactory_ = {0x5842a140,0xff9f,0x4166,{0x8f,0x5c,0x62,0xf5,0xb7,0xb0,0xc7,0x81}};
static const GUID CLSID_AppxBundleFactory_ = {0x378e0446,0x5384,0x43b7,{0x88,0x77,0xe7,0xdb,0xdd,0x88,0x34,0x46}};
static const GUID IID_IAppxFactory_ = {0xbeb94909,0xe451,0x438b,{0xb5,0xa7,0xd7,0x9e,0x76,0x7b,0x75,0xd8}};
static const GUID IID_IAppxBundleFactory_ = {0xbba65864,0x965f,0x4a5f,{0x85,0x5f,0xf0,0x74,0xbd,0xbf,0x3a,0x7b}};

static void printw( const char *key, const WCHAR *w )
{
    char buf[2048];
    WideCharToMultiByte( CP_UTF8, 0, w ? w : L"(null)", -1, buf, sizeof(buf), NULL, NULL );
    printf( "%s=%s\n", key, buf );
}

static IStream *open_file( const char *path, BOOL write )
{
    WCHAR wpath[MAX_PATH];
    IStream *s = NULL;
    MultiByteToWideChar( CP_UTF8, 0, path, -1, wpath, MAX_PATH );
    SHCreateStreamOnFileEx( wpath, write ? STGM_CREATE | STGM_WRITE : STGM_READ | STGM_SHARE_DENY_WRITE,
                            FILE_ATTRIBUTE_NORMAL, write, NULL, &s );
    return s;
}

int main( int argc, char **argv )
{
    void *factory = NULL, *reader = NULL;
    IStream *input;
    HRESULT hr;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    if (argc < 3) return 2;
    if (!(input = open_file( argv[2], FALSE ))) { printf( "open=ERROR\n" ); return 1; }

    if (!strcmp( argv[1], "bundle" ))
    {
        void *bundle = NULL;
        if (FAILED(hr = CoCreateInstance( &CLSID_AppxBundleFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxBundleFactory_, &factory )))
        { printf( "bundlefactory=ERROR %#lx\n", hr ); return 1; }
        hr = VT(factory, BundleFactoryVtbl)->CreateBundleReader( factory, input, &bundle );
        printf( "CreateBundleReader=%#lx\n", hr );
        return 0;
    }

    if (FAILED(hr = CoCreateInstance( &CLSID_AppxFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxFactory_, &factory )))
    { printf( "factory=ERROR %#lx\n", hr ); return 1; }
    if (FAILED(hr = VT(factory, FactoryVtbl)->CreatePackageReader( factory, input, &reader )))
    { printf( "CreatePackageReader=%#lx\n", hr ); return 0; }
    printf( "CreatePackageReader=0\n" );

    if (!strcmp( argv[1], "info" ))
    {
        void *manifest, *id, *props, *files, *apps, *sig;
        WCHAR *s;
        UINT64 v;
        int arch;
        BOOL has;
        int count = 0;

        VT(reader, ReaderVtbl)->GetManifest( reader, &manifest );
        VT(manifest, ManifestVtbl)->GetPackageId( manifest, &id );
        VT(id, IdVtbl)->GetName( id, &s ); printw( "Name", s );
        VT(id, IdVtbl)->GetPublisher( id, &s ); printw( "Publisher", s );
        VT(id, IdVtbl)->GetVersion( id, &v );
        printf( "Version=%u.%u.%u.%u\n", (UINT)(v >> 48), (UINT)(v >> 32) & 0xffff, (UINT)(v >> 16) & 0xffff, (UINT)v & 0xffff );
        VT(id, IdVtbl)->GetArchitecture( id, &arch ); printf( "Architecture=%d\n", arch );
        VT(id, IdVtbl)->GetPackageFullName( id, &s ); printw( "FullName", s );
        VT(id, IdVtbl)->GetPackageFamilyName( id, &s ); printw( "FamilyName", s );
        VT(manifest, ManifestVtbl)->GetProperties( manifest, &props );
        VT(props, PropsVtbl)->GetStringValue( props, L"DisplayName", &s ); printw( "DisplayName", s );
        VT(props, PropsVtbl)->GetStringValue( props, L"PublisherDisplayName", &s ); printw( "PublisherDisplayName", s );
        VT(manifest, ManifestVtbl)->GetPrerequisite( manifest, L"OSMinVersion", &v );
        printf( "OSMinVersion=%u.%u.%u.%u\n", (UINT)(v >> 48), (UINT)(v >> 32) & 0xffff, (UINT)(v >> 16) & 0xffff, (UINT)v & 0xffff );
        VT(manifest, ManifestVtbl)->GetApplications( manifest, &apps );
        for (VT(apps, EnumVtbl)->GetHasCurrent( apps, &has ); has; VT(apps, EnumVtbl)->MoveNext( apps, &has ))
        {
            void *app;
            VT(apps, EnumVtbl)->GetCurrent( apps, &app );
            VT(app, AppVtbl)->GetAppUserModelId( app, &s ); printw( "AUMID", s );
            VT(app, AppVtbl)->GetStringValue( app, L"DisplayName", &s ); printw( "AppDisplayName", s );
        }
        hr = VT(reader, ReaderVtbl)->GetFootprintFile( reader, 2, &sig );
        if (SUCCEEDED(hr))
        {
            IStream *st;
            STATSTG stat;
            char head[4];
            ULONG got = 0;
            VT(sig, FileVtbl)->GetStream( sig, &st );
            IStream_Stat( st, &stat, STATFLAG_NONAME );
            IStream_Read( st, head, 4, &got );
            printf( "SignatureSize=%lu\nSignatureHead=%.4s\n", stat.cbSize.LowPart, got == 4 ? head : "" );
        }
        else printf( "Signature=%#lx\n", hr );
        VT(reader, ReaderVtbl)->GetPayloadFiles( reader, &files );
        for (VT(files, EnumVtbl)->GetHasCurrent( files, &has ); has; VT(files, EnumVtbl)->MoveNext( files, &has ))
        {
            void *f;
            WCHAR *ct;
            VT(files, EnumVtbl)->GetCurrent( files, &f );
            VT(f, FileVtbl)->GetName( f, &s );
            VT(f, FileVtbl)->GetContentType( f, &ct );
            if (!count) { printw( "FirstPayload", s ); printw( "FirstPayloadType", ct ); }
            count++;
        }
        printf( "PayloadCount=%d\n", count );
    }
    else if (!strcmp( argv[1], "extract" ) && argc > 4)
    {
        WCHAR name[MAX_PATH];
        void *file;
        IStream *st, *out;
        ULARGE_INTEGER copied;
        ULARGE_INTEGER all;
        UINT64 size;

        MultiByteToWideChar( CP_UTF8, 0, argv[3], -1, name, MAX_PATH );
        if (FAILED(hr = VT(reader, ReaderVtbl)->GetPayloadFile( reader, name, &file ))) { printf( "GetPayloadFile=%#lx\n", hr ); return 0; }
        VT(file, FileVtbl)->GetSize( file, &size );
        printf( "Size=%llu\n", (unsigned long long)size );
        if (FAILED(hr = VT(file, FileVtbl)->GetStream( file, &st ))) { printf( "GetStream=%#lx\n", hr ); return 0; }
        out = open_file( argv[4], TRUE );
        all.QuadPart = ~0ull;
        hr = IStream_CopyTo( st, out, all, NULL, &copied );
        IStream_Release( out );
        printf( "Copied=%llu\n", (unsigned long long)copied.QuadPart );
    }
    fflush( stdout );
    return 0;
}
