/* resloader-probe: exercise Windows.ApplicationModel.Resources.ResourceLoader
 * the way applications do, for test/resources-gate.sh.
 *
 *   resloader-probe default KEY...        ActivateInstance ("Resources")
 *   resloader-probe byname MAP KEY...     IResourceLoaderFactory
 *   resloader-probe indep MAP KEY...      IResourceLoaderStatics2::GetForViewIndependentUse(name)
 *   resloader-probe uri URI...            IResourceLoaderStatics::GetStringForReference
 *   resloader-probe uriparts URI [REL]    Windows.Foundation.Uri's parsed parts
 *
 * Prints KEY=VALUE (UTF-8) per key; an empty value prints KEY=.
 * The interfaces are declared here from their published IIDs so the probe
 * needs no WinRT headers.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stddef.h>

typedef struct IInspectableVtblX
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *GetIids)(void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRuntimeClassName)(void *, HSTRING *);
    HRESULT (WINAPI *GetTrustLevel)(void *, int *);
} IInspectableVtblX;

typedef struct { IInspectableVtblX i; HRESULT (WINAPI *ActivateInstance)(void *, void **); } FactoryVtbl;
typedef struct { IInspectableVtblX i; HRESULT (WINAPI *GetString)(void *, HSTRING, HSTRING *); } LoaderVtbl;
typedef struct { IInspectableVtblX i; HRESULT (WINAPI *CreateByName)(void *, HSTRING, void **); } LoaderFactoryVtbl;
typedef struct { IInspectableVtblX i; HRESULT (WINAPI *GetStringForReference)(void *, void *, HSTRING *); } StaticsVtbl;
typedef struct
{
    IInspectableVtblX i;
    HRESULT (WINAPI *GetForCurrentView)(void *, void **);
    HRESULT (WINAPI *GetForCurrentViewWithName)(void *, HSTRING, void **);
    HRESULT (WINAPI *GetForViewIndependentUse)(void *, void **);
    HRESULT (WINAPI *GetForViewIndependentUseWithName)(void *, HSTRING, void **);
} Statics2Vtbl;
typedef struct
{
    IInspectableVtblX i;
    HRESULT (WINAPI *CreateUri)(void *, HSTRING, void **);
    HRESULT (WINAPI *CreateWithRelativeUri)(void *, HSTRING, HSTRING, void **);
} UriFactoryVtbl;
typedef HRESULT (WINAPI *StrGetter)(void *, HSTRING *);
typedef struct
{
    IInspectableVtblX i;
    StrGetter AbsoluteUri, DisplayUri, Domain, Extension, Fragment, Host, Password, Path, Query;
    void *QueryParsed;
    StrGetter RawUri, SchemeName, UserName;
    HRESULT (WINAPI *Port)(void *, INT32 *);
    HRESULT (WINAPI *Suspicious)(void *, boolean *);
    HRESULT (WINAPI *Equals)(void *, void *, boolean *);
    HRESULT (WINAPI *CombineUri)(void *, HSTRING, void **);
} UriVtbl;

typedef struct { const void *vtbl; } Obj;
#define VT(o, T) ((const T *)((Obj *)(o))->vtbl)

static const GUID IID_IActivationFactory_ = {0x00000035,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID IID_IResourceLoader_ = {0x08524908,0x16ef,0x45ad,{0xa6,0x02,0x29,0x36,0x37,0xd7,0xe6,0x1a}};
static const GUID IID_IResourceLoaderFactory_ = {0xc33a3603,0x69dc,0x4285,{0xa0,0x77,0xd5,0xc0,0xe4,0x7c,0xcb,0xe8}};
static const GUID IID_IResourceLoaderStatics_ = {0xbf777ce1,0x19c8,0x49c2,{0x95,0x3c,0x47,0xe9,0x22,0x7b,0x33,0x4e}};
static const GUID IID_IResourceLoaderStatics2_ = {0x0cc04141,0x6466,0x4989,{0x94,0x94,0x0b,0x82,0xdf,0xc5,0x3f,0x1f}};
static const GUID IID_IUriRuntimeClassFactory_ = {0x44a9796f,0x723e,0x4fdf,{0xa2,0x18,0x03,0x3e,0x75,0xb0,0xc0,0x84}};

static HSTRING hs( const char *s )
{
    WCHAR buf[1024];
    HSTRING h = NULL;
    int n = MultiByteToWideChar( CP_UTF8, 0, s, -1, buf, 1024 );
    WindowsCreateString( buf, n > 0 ? n - 1 : 0, &h );
    return h;
}

static void print( const char *key, HSTRING value )
{
    char out[4096];
    UINT32 len;
    const WCHAR *w = WindowsGetStringRawBuffer( value, &len );
    int n = WideCharToMultiByte( CP_UTF8, 0, w, len, out, sizeof(out) - 1, NULL, NULL );
    out[n > 0 ? n : 0] = 0;
    printf( "%s=%s\n", key, out );
}

static void *factory( const char *cls, const GUID *iid )
{
    void *f = NULL;
    HSTRING name = hs( cls );
    HRESULT hr = RoGetActivationFactory( name, iid, &f );
    WindowsDeleteString( name );
    if (FAILED(hr)) { printf( "ERROR factory %s: %#lx\n", cls, hr ); exit( 1 ); }
    return f;
}

static void get_all( void *loader, int argc, char **argv, int first )
{
    for (int i = first; i < argc; i++)
    {
        HSTRING key = hs( argv[i] ), value = NULL;
        HRESULT hr = VT(loader, LoaderVtbl)->GetString( loader, key, &value );
        if (FAILED(hr)) printf( "%s=ERROR %#lx\n", argv[i], hr );
        else print( argv[i], value );
        WindowsDeleteString( key );
        WindowsDeleteString( value );
    }
}

int main( int argc, char **argv )
{
    const char *cls = "Windows.ApplicationModel.Resources.ResourceLoader";
    void *loader = NULL;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    if (argc < 3) { printf( "usage: see source\n" ); return 2; }

    if (!strcmp( argv[1], "default" ))
    {
        void *f = factory( cls, &IID_IActivationFactory_ ), *insp = NULL;
        if (FAILED(hr = VT(f, FactoryVtbl)->ActivateInstance( f, &insp ))) { printf( "ERROR activate %#lx\n", hr ); return 1; }
        VT(insp, IInspectableVtblX)->QueryInterface( insp, &IID_IResourceLoader_, &loader );
        get_all( loader, argc, argv, 2 );
    }
    else if (!strcmp( argv[1], "byname" ) || !strcmp( argv[1], "indep" ))
    {
        HSTRING map = hs( argv[2] );
        if (!strcmp( argv[1], "byname" ))
        {
            void *f = factory( cls, &IID_IResourceLoaderFactory_ );
            hr = VT(f, LoaderFactoryVtbl)->CreateByName( f, map, &loader );
        }
        else
        {
            void *f = factory( cls, &IID_IResourceLoaderStatics2_ );
            hr = VT(f, Statics2Vtbl)->GetForViewIndependentUseWithName( f, map, &loader );
        }
        if (FAILED(hr)) { printf( "ERROR create %#lx\n", hr ); return 1; }
        get_all( loader, argc, argv, 3 );
    }
    else if (!strcmp( argv[1], "uri" ))
    {
        void *f = factory( cls, &IID_IResourceLoaderStatics_ );
        void *uf = factory( "Windows.Foundation.Uri", &IID_IUriRuntimeClassFactory_ );
        for (int i = 2; i < argc; i++)
        {
            HSTRING s = hs( argv[i] ), value = NULL;
            void *uri = NULL;
            if (FAILED(hr = VT(uf, UriFactoryVtbl)->CreateUri( uf, s, &uri ))) printf( "%s=ERROR uri %#lx\n", argv[i], hr );
            else if (FAILED(hr = VT(f, StaticsVtbl)->GetStringForReference( f, uri, &value ))) printf( "%s=ERROR %#lx\n", argv[i], hr );
            else print( argv[i], value );
            WindowsDeleteString( s );
            WindowsDeleteString( value );
        }
    }
    else if (!strcmp( argv[1], "uriparts" ))
    {
        void *uf = factory( "Windows.Foundation.Uri", &IID_IUriRuntimeClassFactory_ ), *uri = NULL, *other = NULL;
        HSTRING s = hs( argv[2] ), v = NULL;
        struct { const char *name; size_t off; } parts[] = {
#define P(n) { #n, offsetof(UriVtbl, n) }
            P(AbsoluteUri), P(DisplayUri), P(Domain), P(Extension), P(Fragment), P(Host), P(Password),
            P(Path), P(Query), P(RawUri), P(SchemeName), P(UserName),
#undef P
        };
        INT32 port = -1;
        boolean eq = 0;
        if (FAILED(hr = VT(uf, UriFactoryVtbl)->CreateUri( uf, s, &uri ))) { printf( "create=ERROR %#lx\n", hr ); return 0; }
        for (size_t i = 0; i < ARRAYSIZE(parts); i++)
        {
            StrGetter get = *(StrGetter *)((char *)VT(uri, UriVtbl) + parts[i].off);
            if (FAILED(hr = get( uri, &v ))) printf( "%s=ERROR %#lx\n", parts[i].name, hr );
            else print( parts[i].name, v );
            WindowsDeleteString( v ); v = NULL;
        }
        VT(uri, UriVtbl)->Port( uri, &port );
        printf( "Port=%d\n", port );
        VT(uf, UriFactoryVtbl)->CreateUri( uf, s, &other );
        VT(uri, UriVtbl)->Equals( uri, other, &eq );
        printf( "EqualsSelf=%d\n", eq );
        if (argc > 3)
        {
            HSTRING rel = hs( argv[3] );
            void *comb = NULL;
            if (FAILED(hr = VT(uri, UriVtbl)->CombineUri( uri, rel, &comb ))) printf( "Combined=ERROR %#lx\n", hr );
            else { VT(comb, UriVtbl)->AbsoluteUri( comb, &v ); print( "Combined", v ); }
        }
    }
    fflush( stdout );
    return 0;
}
