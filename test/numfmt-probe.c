/* numfmt-probe: Windows.Globalization.NumberFormatting (patches/sg/6199): DecimalFormatter
 * formatting (integer/fraction/significant digits, grouping, the language's separators, a rounder)
 * and parsing; SignificantDigitsNumberRounder and IncrementNumberRounder.
 * Prints name=value lines; see test/numfmt-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(IID_NumberFormatter, 0xa5007c49, 0x7676, 0x4db7, 0x86, 0x31, 0x1b, 0x6f, 0xf2, 0x65, 0xca, 0xa9);
DEFINE_GUID(IID_Options, 0x80332d21, 0xaee1, 0x4a39, 0xba, 0xa2, 0x07, 0xed, 0x8c, 0x96, 0xda, 0xf6);
DEFINE_GUID(IID_Parser, 0xe6659412, 0x4a13, 0x4a53, 0x83, 0xa1, 0x39, 0x2f, 0xbe, 0x4c, 0xff, 0x9f);
DEFINE_GUID(IID_RounderOption, 0x3b088433, 0x646f, 0x4efe, 0x8d, 0x48, 0x66, 0xeb, 0x2e, 0x49, 0xe7, 0x36);
DEFINE_GUID(IID_SigOption, 0x1d4dfcdd, 0x2d43, 0x4ee8, 0xbb, 0xf1, 0xc1, 0xb2, 0x6a, 0x71, 0x1a, 0x58);
DEFINE_GUID(IID_Rounder, 0x5473c375, 0x38ed, 0x4631, 0xb8, 0x0c, 0xef, 0x34, 0xfc, 0x48, 0xb7, 0xf5);
DEFINE_GUID(IID_SigRounder, 0xf5941bca, 0x6646, 0x4913, 0x8c, 0x76, 0x1b, 0x19, 0x1f, 0xf9, 0x4d, 0xfd);
DEFINE_GUID(IID_IncRounder, 0x70a64ff8, 0x66ab, 0x4155, 0x9d, 0xa1, 0x73, 0x9e, 0x46, 0x76, 0x45, 0x43);
DEFINE_GUID(IID_DecimalFactory, 0x0d018c9a, 0xe393, 0x46b8, 0xb8, 0x30, 0x7a, 0x69, 0xc8, 0xf8, 0x9f, 0xbb);

static HSTRING hs( const WCHAR *s ) { HSTRING h = NULL; WindowsCreateString( s, (UINT32)wcslen( s ), &h ); return h; }
static void *qi( void *o, REFIID iid ) { void *r = NULL; if (o) IUnknown_QueryInterface( (IUnknown *)o, iid, &r ); return r; }

static IInspectable *activate( const WCHAR *name )
{
    IInspectable *out = NULL;
    HRESULT hr = RoActivateInstance( hs( name ), &out );
    if (FAILED(hr)) printf( "activate %ls=%#lx\n", name, hr );
    return out;
}

static const WCHAR *fmt( void *formatter, double v )
{
    static WCHAR buf[8][128];
    static int n;
    HSTRING s = NULL;
    WCHAR *out = buf[n++ % 8];
    void *f = qi( formatter, &IID_NumberFormatter );
    if (!f) return L"(none)";
    SLOT( f, 8, HRESULT (WINAPI *)( void *, double, HSTRING * ) )( f, v, &s );
    lstrcpynW( out, s ? WindowsGetStringRawBuffer( s, NULL ) : L"(null)", 128 );
    WindowsDeleteString( s );
    return out;
}

static const WCHAR *parse( void *formatter, const WCHAR *text )
{
    static WCHAR buf[4][64];
    static int n;
    WCHAR *out = buf[n++ % 4];
    void *p = qi( formatter, &IID_Parser ), *ref = NULL;
    double v = 0;
    if (!p) return L"(no parser)";
    SLOT( p, 8, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( p, hs( text ), &ref );
    if (!ref) return L"null";
    SLOT( ref, 6, HRESULT (WINAPI *)( void *, double * ) )( ref, &v );
    swprintf( out, 64, L"%g", v );
    return out;
}

/* an IIterable<String> of one string */
struct one { void *vtbl; void *it_vtbl; int pos; HSTRING s; };
static HRESULT WINAPI one_qi( struct one *o, REFIID iid, void **out ) { *out = o; return S_OK; }
static ULONG WINAPI one_ref( struct one *o ) { return 1; }
static HRESULT WINAPI one_stub( struct one *o ) { return E_NOTIMPL; }
static HRESULT WINAPI one_first( struct one *o, void **it ) { o->pos = 0; *it = &o->it_vtbl; return S_OK; }
static HRESULT WINAPI it_qi( void **it, REFIID iid, void **out ) { *out = it; return S_OK; }
static ULONG WINAPI it_ref( void **it ) { return 1; }
static struct one *one_from( void **it ) { return CONTAINING_RECORD( it, struct one, it_vtbl ); }
static HRESULT WINAPI it_current( void **it, HSTRING *s ) { return WindowsDuplicateString( one_from( it )->s, s ); }
static HRESULT WINAPI it_has( void **it, boolean *b ) { *b = one_from( it )->pos == 0; return S_OK; }
static HRESULT WINAPI it_next( void **it, boolean *b ) { one_from( it )->pos++; *b = FALSE; return S_OK; }
static void *one_vtbl[] = { one_qi, one_ref, one_ref, one_stub, one_stub, one_stub, one_first };
static void *it_vtbl[] = { it_qi, it_ref, it_ref, one_stub, one_stub, one_stub, it_current, it_has, it_next, one_stub };

int main( void )
{
    IInspectable *f, *sig, *inc, *de = NULL;
    void *opt, *rounder, *factory = NULL;
    double r = 0;
    struct one langs = { one_vtbl, it_vtbl, 0, NULL };

    RoInitialize( RO_INIT_MULTITHREADED );
    f = activate( L"Windows.Globalization.NumberFormatting.DecimalFormatter" );
    opt = qi( f, &IID_Options );
    printf( "defaults=%ls|%ls|%ls|%ls\n", fmt( f, 1234.5 ), fmt( f, 0.25 ), fmt( f, -3 ), fmt( f, 0 ) );
    if (opt)
    {
        SLOT( opt, 11, HRESULT (WINAPI *)( void *, INT32 ) )( opt, 2 );          /* FractionDigits */
        SLOT( opt, 13, HRESULT (WINAPI *)( void *, boolean ) )( opt, TRUE );     /* IsGrouped */
        SLOT( opt, 9, HRESULT (WINAPI *)( void *, INT32 ) )( opt, 3 );           /* IntegerDigits */
    }
    printf( "options=%ls|%ls\n", fmt( f, 1234567.5 ), fmt( f, 7 ) );
    printf( "parse=%ls|%ls|%ls\n", parse( f, L"1,234.5" ), parse( f, L"-0.75" ), parse( f, L"abc" ) );

    sig = activate( L"Windows.Globalization.NumberFormatting.SignificantDigitsNumberRounder" );
    if (qi( sig, &IID_SigRounder )) SLOT( qi( sig, &IID_SigRounder ), 9, HRESULT (WINAPI *)( void *, UINT32 ) )( qi( sig, &IID_SigRounder ), 3 );
    rounder = qi( sig, &IID_Rounder );
    if (rounder) SLOT( rounder, 11, HRESULT (WINAPI *)( void *, double, double * ) )( rounder, 12345.678, &r );
    printf( "significant=%g\n", r );
    if (rounder) SLOT( rounder, 11, HRESULT (WINAPI *)( void *, double, double * ) )( rounder, 0.0012345, &r );
    printf( "significant_small=%g\n", r );

    inc = activate( L"Windows.Globalization.NumberFormatting.IncrementNumberRounder" );
    if (qi( inc, &IID_IncRounder )) SLOT( qi( inc, &IID_IncRounder ), 9, HRESULT (WINAPI *)( void *, double ) )( qi( inc, &IID_IncRounder ), 0.25 );
    r = 0;
    if (qi( inc, &IID_Rounder )) SLOT( qi( inc, &IID_Rounder ), 11, HRESULT (WINAPI *)( void *, double, double * ) )( qi( inc, &IID_Rounder ), 1.13, &r );
    printf( "increment=%g\n", r );

    /* a rounder on a formatter */
    f = activate( L"Windows.Globalization.NumberFormatting.DecimalFormatter" );
    if (qi( f, &IID_RounderOption ) && rounder) SLOT( qi( f, &IID_RounderOption ), 7, HRESULT (WINAPI *)( void *, void * ) )( qi( f, &IID_RounderOption ), rounder );
    printf( "rounded=%ls\n", fmt( f, 98765.4321 ) );
    if (qi( f, &IID_SigOption )) SLOT( qi( f, &IID_SigOption ), 7, HRESULT (WINAPI *)( void *, INT32 ) )( qi( f, &IID_SigOption ), 4 );
    printf( "sigdigits=%ls\n", fmt( f, 0.5 ) );

    /* German: its own separators */
    RoGetActivationFactory( hs( L"Windows.Globalization.NumberFormatting.DecimalFormatter" ), &IID_DecimalFactory, &factory );
    langs.s = hs( L"de-DE" );
    if (factory) SLOT( factory, 6, HRESULT (WINAPI *)( void *, void *, HSTRING, IInspectable ** ) )( factory, &langs, hs( L"DE" ), &de );
    if (de && qi( de, &IID_Options )) SLOT( qi( de, &IID_Options ), 13, HRESULT (WINAPI *)( void *, boolean ) )( qi( de, &IID_Options ), TRUE );
    printf( "german=%ls|%ls\n", fmt( de, 1234.5 ), parse( de, L"1.234,5" ) );
    printf( "done=1\n" );
    return 0;
}
