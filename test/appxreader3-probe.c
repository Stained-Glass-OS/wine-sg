/* appxreader3-probe: IAppxManifestReader2/3 of appxpackaging (Stained Glass OS wine-sg 8234;
 * test/appxreader3-gate.sh): the qualified resources (language, scale, DirectX feature level), the
 * capabilities by class (general, restricted, Windows, custom) and the target device families of a
 * manifest read by IAppxFactory::CreateManifestReader. A manifest reader had none of these interfaces.
 * Interfaces declared here from their published IIDs (Windows SDK appxpackaging.h, MSIX SDK).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>
#include <stdio.h>

typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *); } UnkVtbl;
typedef struct { UnkVtbl u; void *CreatePackageWriter; void *CreatePackageReader;
                 HRESULT (WINAPI *CreateManifestReader)(void *, IStream *, void **); } FactoryVtbl;
typedef struct { UnkVtbl u; void *m[9];
                 HRESULT (WINAPI *GetQualifiedResources)(void *, void **);
                 HRESULT (WINAPI *GetCapabilitiesByCapabilityClass)(void *, int, void **);
                 HRESULT (WINAPI *GetTargetDeviceFamilies)(void *, void **); } Reader3Vtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetCurrent)(void *, void *); HRESULT (WINAPI *GetHasCurrent)(void *, BOOL *);
                 HRESULT (WINAPI *MoveNext)(void *, BOOL *); } EnumVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetLanguage)(void *, WCHAR **); HRESULT (WINAPI *GetScale)(void *, UINT32 *);
                 HRESULT (WINAPI *GetDXFeatureLevel)(void *, int *); } QresVtbl;
typedef struct { UnkVtbl u; HRESULT (WINAPI *GetName)(void *, WCHAR **); HRESULT (WINAPI *GetMinVersion)(void *, UINT64 *);
                 HRESULT (WINAPI *GetMaxVersionTested)(void *, UINT64 *); } FamilyVtbl;
typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID CLSID_AppxFactory_ = {0x5842a140,0xff9f,0x4166,{0x8f,0x5c,0x62,0xf5,0xb7,0xb0,0xc7,0x81}};
static const GUID IID_IAppxFactory_ = {0xbeb94909,0xe451,0x438b,{0xb5,0xa7,0xd7,0x9e,0x76,0x7b,0x75,0xd8}};
static const GUID IID_IAppxManifestReader2_ = {0xd06f67bc,0xb31d,0x4eba,{0xa8,0xaf,0x63,0x8e,0x73,0xe7,0x7b,0x4d}};
static const GUID IID_IAppxManifestReader3_ = {0xc43825ab,0x69b7,0x400a,{0x97,0x09,0xcc,0x37,0xf5,0xa7,0x2d,0x24}};

static const char manifest[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<Package xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\"\n"
    "  xmlns:uap=\"http://schemas.microsoft.com/appx/manifest/uap/windows10\"\n"
    "  xmlns:uap4=\"http://schemas.microsoft.com/appx/manifest/uap/windows10/4\"\n"
    "  xmlns:rescap=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities\"\n"
    "  xmlns:wincap=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10/windowscapabilities\">\n"
    "  <Identity Name=\"SG.Reader3\" Publisher=\"CN=SG\" Version=\"1.2.3.4\" ProcessorArchitecture=\"x64\"/>\n"
    "  <Properties><DisplayName>R3</DisplayName><PublisherDisplayName>SG</PublisherDisplayName><Logo>a.png</Logo></Properties>\n"
    "  <Dependencies>\n"
    "    <TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.17763.0\" MaxVersionTested=\"10.0.22621.0\"/>\n"
    "    <TargetDeviceFamily Name=\"Windows.Universal\" MinVersion=\"10.0.10240.0\" MaxVersionTested=\"10.0.19041.1\"/>\n"
    "  </Dependencies>\n"
    "  <Resources><Resource Language=\"en-us\"/><Resource uap:Scale=\"200\"/><Resource uap:DXFeatureLevel=\"dx11\"/></Resources>\n"
    "  <Capabilities>\n"
    "    <Capability Name=\"internetClient\"/>\n"
    "    <uap:Capability Name=\"picturesLibrary\"/>\n"
    "    <rescap:Capability Name=\"runFullTrust\"/>\n"
    "    <wincap:Capability Name=\"packageQuery\"/>\n"
    "    <uap4:CustomCapability Name=\"SG.Custom_8wekyb3d8bbwe\"/>\n"
    "    <DeviceCapability Name=\"webcam\"/>\n"
    "  </Capabilities>\n"
    "</Package>\n";

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

/* the capabilities of a class, joined by commas */
static void caps( void *r3, int class, char *out )
{
    void *e = NULL;
    BOOL has = FALSE;
    out[0] = 0;
    if (VT(r3, Reader3Vtbl)->GetCapabilitiesByCapabilityClass( r3, class, &e ) != S_OK) { strcpy( out, "error" ); return; }
    for (VT(e, EnumVtbl)->GetHasCurrent( e, &has ); has; VT(e, EnumVtbl)->MoveNext( e, &has ))
    {
        WCHAR *name = NULL;
        VT(e, EnumVtbl)->GetCurrent( e, &name );
        sprintf( out + strlen( out ), "%s%ls", out[0] ? "," : "", name );
        CoTaskMemFree( name );
    }
    VT(e, EnumVtbl)->u.Release( e );
}

int main( void )
{
    void *factory = NULL, *reader = NULL, *r2 = NULL, *r3 = NULL, *e = NULL;
    IStream *stream;
    char buf[512];
    BOOL has = FALSE;
    int n;
    HRESULT hr;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = CoCreateInstance( &CLSID_AppxFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxFactory_, &factory );
    if (FAILED(hr)) { printf( "FAIL  AppxFactory (%#lx)\nRESULT: FAIL\n", hr ); return 1; }
    stream = SHCreateMemStream( (const BYTE *)manifest, sizeof(manifest) - 1 );
    hr = VT(factory, FactoryVtbl)->CreateManifestReader( factory, stream, &reader );
    check( hr == S_OK, "CreateManifestReader", hr );
    if (!reader) goto done;
    hr = VT(reader, UnkVtbl)->QI( reader, &IID_IAppxManifestReader2_, &r2 );
    check( hr == S_OK, "the reader is an IAppxManifestReader2", hr );
    hr = VT(reader, UnkVtbl)->QI( reader, &IID_IAppxManifestReader3_, &r3 );
    check( hr == S_OK, "the reader is an IAppxManifestReader3", hr );
    if (!r3) goto done;

    /* qualified resources: en-us; scale 200; dx11 */
    hr = VT(r3, Reader3Vtbl)->GetQualifiedResources( r3, &e );
    buf[0] = 0; n = 0;
    if (hr == S_OK)
        for (VT(e, EnumVtbl)->GetHasCurrent( e, &has ); has; VT(e, EnumVtbl)->MoveNext( e, &has ), n++)
        {
            void *q = NULL; WCHAR *lang = NULL; UINT32 scale = 0; int dx = 0;
            VT(e, EnumVtbl)->GetCurrent( e, &q );
            VT(q, QresVtbl)->GetLanguage( q, &lang );
            VT(q, QresVtbl)->GetScale( q, &scale );
            VT(q, QresVtbl)->GetDXFeatureLevel( q, &dx );
            sprintf( buf + strlen( buf ), "[%ls %u %d]", lang ? lang : L"-", scale, dx );
            CoTaskMemFree( lang );
            VT(q, UnkVtbl)->Release( q );
        }
    printf( "      qualified resources: %s\n", buf );
    check( hr == S_OK && !strcmp( buf, "[en-us 0 0][- 200 0][- 0 3]" ), "GetQualifiedResources: language, scale, DirectX level", n );
    if (e) VT(e, UnkVtbl)->Release( e );

    caps( r3, 1, buf ); printf( "      general: %s\n", buf );
    check( !strcmp( buf, "internetClient,picturesLibrary" ), "general capabilities (default and uap namespaces)", 0 );
    caps( r3, 0, buf );
    check( !strcmp( buf, "internetClient,picturesLibrary" ), "APPX_CAPABILITY_CLASS_DEFAULT: the general ones", 0 );
    caps( r3, 2, buf );
    check( !strcmp( buf, "runFullTrust" ), "restricted capabilities", 0 );
    caps( r3, 4, buf );
    check( !strcmp( buf, "packageQuery" ), "Windows capabilities", 0 );
    caps( r3, 7, buf ); printf( "      all: %s\n", buf );
    check( !strcmp( buf, "internetClient,picturesLibrary,runFullTrust,packageQuery" ), "all classes (custom and device ones not)", 0 );
    caps( r3, 8, buf );
    check( !strcmp( buf, "SG.Custom_8wekyb3d8bbwe" ), "custom capabilities", 0 );

    e = NULL;
    hr = VT(r3, Reader3Vtbl)->GetTargetDeviceFamilies( r3, &e );
    buf[0] = 0; n = 0;
    if (hr == S_OK)
        for (VT(e, EnumVtbl)->GetHasCurrent( e, &has ); has; VT(e, EnumVtbl)->MoveNext( e, &has ), n++)
        {
            void *f = NULL; WCHAR *name = NULL; UINT64 min = 0, max = 0;
            VT(e, EnumVtbl)->GetCurrent( e, &f );
            VT(f, FamilyVtbl)->GetName( f, &name );
            VT(f, FamilyVtbl)->GetMinVersion( f, &min );
            VT(f, FamilyVtbl)->GetMaxVersionTested( f, &max );
            sprintf( buf + strlen( buf ), "[%ls %I64x %I64x]", name, min, max );
            CoTaskMemFree( name );
            VT(f, UnkVtbl)->Release( f );
        }
    printf( "      device families: %s\n", buf );
    check( hr == S_OK && !strcmp( buf, "[Windows.Desktop a000045630000 a0000585d0000][Windows.Universal a000028000000 a00004a610001]" ),
           "GetTargetDeviceFamilies: names and versions", n );
    if (e) VT(e, UnkVtbl)->Release( e );
done:
    if (r2) VT(r2, UnkVtbl)->Release( r2 );
    if (r3) VT(r3, UnkVtbl)->Release( r3 );
    if (reader) VT(reader, UnkVtbl)->Release( reader );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
