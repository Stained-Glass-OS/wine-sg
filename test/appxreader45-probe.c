/* appxreader45-probe: IAppxManifestReader4/5 of appxpackaging (Stained Glass OS wine-sg 8279;
 * test/appxreader45-gate.sh): an optional package's main package (Reader4 GetOptionalPackageInfo) and its
 * main package dependencies with family names (Reader5); a main package is not optional and has none.
 * IIDs from the MSIX SDK's AppxPackaging.hpp (MIT).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>
#include <stdio.h>

typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *); } UnkVtbl;
typedef struct { UnkVtbl u; void *CreatePackageWriter; void *CreatePackageReader;
                 HRESULT (WINAPI *CreateManifestReader)(void *, IStream *, void **); } FactoryVtbl;
typedef struct { UnkVtbl u; void *m[12]; HRESULT (WINAPI *GetOptionalPackageInfo)(void *, void **); } Reader4Vtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetMainPackageDependencies)(void *, void **); } Reader5Vtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetIsOptionalPackage)(void *, BOOL *); HRESULT (WINAPI *GetMainPackageName)(void *, WCHAR **); } OptVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetCurrent)(void *, void **); HRESULT (WINAPI *GetHasCurrent)(void *, BOOL *);
                 HRESULT (WINAPI *MoveNext)(void *, BOOL *); } EnumVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetName)(void *, WCHAR **); HRESULT (WINAPI *GetPublisher)(void *, WCHAR **);
                 HRESULT (WINAPI *GetPackageFamilyName)(void *, WCHAR **); } DepVtbl;
typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID CLSID_AppxFactory_ = {0x5842a140,0xff9f,0x4166,{0x8f,0x5c,0x62,0xf5,0xb7,0xb0,0xc7,0x81}};
static const GUID IID_IAppxFactory_ = {0xbeb94909,0xe451,0x438b,{0xb5,0xa7,0xd7,0x9e,0x76,0x7b,0x75,0xd8}};
static const GUID IID_IAppxManifestReader4_ = {0x4579bb7c,0x741d,0x4161,{0xb5,0xa1,0x47,0xbd,0x3b,0x78,0xad,0x9b}};
static const GUID IID_IAppxManifestReader5_ = {0x8d7ae132,0xa690,0x4c00,{0xb7,0x5a,0x6a,0xae,0x1f,0xea,0xac,0x80}};

#define HEAD "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" \
    "<Package xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\"\n" \
    "  xmlns:uap3=\"http://schemas.microsoft.com/appx/manifest/uap/windows10/3\">\n"
#define TAIL "  <Properties><DisplayName>X</DisplayName><PublisherDisplayName>SG</PublisherDisplayName><Logo>a.png</Logo></Properties>\n" \
    "</Package>\n"
static const char optional[] = HEAD
    "  <Identity Name=\"SG.Addon\" Publisher=\"CN=SG\" Version=\"1.0.0.0\" ProcessorArchitecture=\"x64\"/>\n"
    "  <Dependencies>\n"
    "    <TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.17763.0\" MaxVersionTested=\"10.0.22621.0\"/>\n"
    "    <uap3:MainPackageDependency Name=\"SG.MainApp\" Publisher=\"CN=SG\"/>\n"
    "  </Dependencies>\n" TAIL;
static const char plain[] = HEAD
    "  <Identity Name=\"SG.MainApp\" Publisher=\"CN=SG\" Version=\"1.0.0.0\" ProcessorArchitecture=\"x64\"/>\n"
    "  <Dependencies><TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.17763.0\" MaxVersionTested=\"10.0.22621.0\"/></Dependencies>\n" TAIL;

static int failures;
static void check( int ok, const char *what ) { printf( "%s  %s\n", ok ? "PASS" : "FAIL", what ); if (!ok) failures++; }

static void *read_manifest( void *factory, const char *xml )
{
    IStream *stream = SHCreateMemStream( (const BYTE *)xml, strlen( xml ) );
    void *reader = NULL;
    VT(factory, FactoryVtbl)->CreateManifestReader( factory, stream, &reader );
    IStream_Release( stream );
    return reader;
}

int main( void )
{
    void *factory = NULL, *reader, *r4 = NULL, *r5 = NULL, *opt = NULL, *e = NULL, *dep = NULL, *pid;
    WCHAR *name = NULL, *pub = NULL, *fam = NULL, *own = NULL;
    BOOL is_opt = 2, has = FALSE;

    CoInitialize( NULL );
    CoCreateInstance( &CLSID_AppxFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxFactory_, &factory );
    reader = factory ? read_manifest( factory, optional ) : NULL;
    if (!reader) { printf( "FAIL  a manifest reader\nRESULT: FAIL\n" ); return 1; }
    VT(reader, UnkVtbl)->QI( reader, &IID_IAppxManifestReader4_, &r4 );
    VT(reader, UnkVtbl)->QI( reader, &IID_IAppxManifestReader5_, &r5 );
    check( r4 && r5, "the reader has IAppxManifestReader4 and 5" );
    if (!r4 || !r5) { printf( "RESULT: FAIL\n" ); return 1; }
    VT(r4, Reader4Vtbl)->GetOptionalPackageInfo( r4, &opt );
    if (opt) { VT(opt, OptVtbl)->GetIsOptionalPackage( opt, &is_opt ); VT(opt, OptVtbl)->GetMainPackageName( opt, &name ); }
    check( opt && is_opt == TRUE && name && !lstrcmpW( name, L"SG.MainApp" ), "an optional package: its main package is SG.MainApp" );
    CoTaskMemFree( name ); name = NULL;
    VT(r5, Reader5Vtbl)->GetMainPackageDependencies( r5, &e );
    if (e) VT(e, EnumVtbl)->GetHasCurrent( e, &has );
    if (has) VT(e, EnumVtbl)->GetCurrent( e, &dep );
    if (dep)
    {
        VT(dep, DepVtbl)->GetName( dep, &name );
        VT(dep, DepVtbl)->GetPublisher( dep, &pub );
        VT(dep, DepVtbl)->GetPackageFamilyName( dep, &fam );
    }
    /* the family name of the main package, as its own package id computes it */
    {
        void *main_reader = read_manifest( factory, plain );
        typedef struct { UnkVtbl u; HRESULT (WINAPI *GetPackageId)(void *, void **); } RVtbl;
        typedef struct { UnkVtbl u; void *m[7]; HRESULT (WINAPI *GetPackageFamilyName)(void *, WCHAR **); } IdVtbl;
        VT(main_reader, RVtbl)->GetPackageId( main_reader, &pid );
        VT(pid, IdVtbl)->GetPackageFamilyName( pid, &own );
        printf( "      dependency %ls / %ls / %ls (the main package's own: %ls)\n", name, pub, fam, own );
        check( name && !lstrcmpW( name, L"SG.MainApp" ) && pub && !lstrcmpW( pub, L"CN=SG" ) && fam && own && !lstrcmpW( fam, own ),
               "Reader5: one main package dependency, its name, publisher and family name" );
        if (e) { has = TRUE; VT(e, EnumVtbl)->MoveNext( e, &has ); }
        check( !has, "  and no second one" );

        VT(main_reader, UnkVtbl)->QI( main_reader, &IID_IAppxManifestReader4_, &r4 );
        opt = NULL; is_opt = 2;
        VT(r4, Reader4Vtbl)->GetOptionalPackageInfo( r4, &opt );
        if (opt) VT(opt, OptVtbl)->GetIsOptionalPackage( opt, &is_opt );
        check( opt && is_opt == FALSE, "a main package is not optional" );
        VT(main_reader, UnkVtbl)->QI( main_reader, &IID_IAppxManifestReader5_, &r5 );
        e = NULL; has = TRUE;
        VT(r5, Reader5Vtbl)->GetMainPackageDependencies( r5, &e );
        if (e) VT(e, EnumVtbl)->GetHasCurrent( e, &has );
        check( e && !has, "  and has no main package dependencies" );
    }
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
