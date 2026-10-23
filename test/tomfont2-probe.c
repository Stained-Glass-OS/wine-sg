/* tomfont2-probe: TOM2 fonts and paragraphs (ITextFont2 / ITextPara2) of a rich edit control's
 * ranges (Stained Glass OS wine-sg 6052). ITextFont2 and ITextPara2 are not in the mingw headers:
 * their own methods are called through the vtable (ITextFont2 slot 62 on, ITextPara2 55 on). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>

static const GUID IID_ITextDocument_ = {0x8cc497c0,0xa1df,0x11ce,{0x80,0x98,0x00,0xaa,0x00,0x47,0xbe,0x5d}};
static const GUID IID_ITextRange2_ = {0xc241f5e2,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};
static const GUID IID_ITextFont2_ = {0xc241f5e3,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};
static const GUID IID_ITextPara2_ = {0xc241f5e4,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};

#define SLOT(o, n) ((*(void ***)(o))[n])
typedef HRESULT (WINAPI *fn_get)( void *, LONG * );
typedef HRESULT (WINAPI *fn_set)( void *, LONG );
typedef HRESULT (WINAPI *fn_get2)( void *, LONG *, LONG * );
typedef HRESULT (WINAPI *fn_set2)( void *, LONG, LONG );
typedef HRESULT (WINAPI *fn_ptr)( void *, void ** );
typedef HRESULT (WINAPI *fn_put)( void *, void * );
typedef HRESULT (WINAPI *fn_eq)( void *, void *, LONG * );
typedef HRESULT (WINAPI *fn_info)( void *, LONG, LONG *, LONG * );
typedef HRESULT (WINAPI *fn_getp)( void *, LONG, LONG * );
/* ITextRange2 */
#define R_GETFONT2 73
#define R_SETFONT2 74
#define R_GETPARA2 79
/* ITextFont2 */
#define F_GETCOUNT 62
#define F_GETAUTOLIGATURES 63
#define F_SETAUTOLIGATURES 64
#define F_GETCHARREP 71
#define F_SETCHARREP 72
#define F_SETDOUBLESTRIKE 78
#define F_GETDUPLICATE2 79
#define F_GETLINKTYPE 81
#define F_GETEFFECTS 100
#define F_GETPROPERTY 102
#define F_GETPROPERTYINFO 103
#define F_ISEQUAL2 104
#define F_SETEFFECTS 105
#define F_SETPROPERTY 107
/* ITextPara2 */
#define P_GETBORDERS 55
#define P_GETDUPLICATE2 56
#define P_GETHANGING 60
#define P_SETHANGING 61
#define P_GETEFFECTS 66
#define P_ISEQUAL2 68
#define P_SETEFFECTS 69

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %ld, %#lx)\n", what, got, got ); failures++; }
}

static ITextDocument *doc;

static void *range2( LONG start, LONG end )
{
    ITextRange *r = NULL;
    void *r2 = NULL;
    ITextDocument_Range( doc, start, end, &r );
    if (r) ITextRange_QueryInterface( r, &IID_ITextRange2_, &r2 );
    return r2;
}

static void *font2_of( LONG start, LONG end )
{
    void *r = range2( start, end ), *f = NULL;
    if (r) ((fn_ptr)SLOT( r, R_GETFONT2 ))( r, &f );
    return f;
}

static CHARFORMAT2W char_format( HWND hwnd, LONG start, LONG end )
{
    CHARFORMAT2W cf = { sizeof(cf) };
    CHARRANGE cr = { start, end };
    SendMessageW( hwnd, EM_EXSETSEL, 0, (LPARAM)&cr );
    SendMessageW( hwnd, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf );
    return cf;
}

int main( void )
{
    IRichEditOle *re;
    HWND hwnd;
    HRESULT hr;
    void *font, *font_mixed, *dup = NULL, *other, *para = NULL, *pdup = NULL, *r, *unk = NULL, *borders = NULL;
    ITextFont *base;
    LONG v = 0, m = 0, t = 0, eq = 0;
    CHARFORMAT2W cf;
    PARAFORMAT2 pf = { sizeof(pf) };
    CHARRANGE cr;

    OleInitialize( NULL );
    LoadLibraryA( "riched20.dll" );
    hwnd = CreateWindowExW( 0, L"RichEdit20W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 400, 200, 0, 0, 0, 0 );
    SendMessageW( hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&re );
    re->lpVtbl->QueryInterface( re, &IID_ITextDocument_, (void **)&doc );
    check( doc != NULL, "ITextDocument", 0 );
    if (!doc) { printf( "RESULT: FAIL\n" ); return 1; }
    SendMessageW( hwnd, WM_SETTEXT, 0, (LPARAM)L"Hello world link\rSecond para" );

    /* plain text to start from (the default font may be bold) */
    cr.cpMin = 0; cr.cpMax = -1;
    SendMessageW( hwnd, EM_EXSETSEL, 0, (LPARAM)&cr );
    memset( &cf, 0, sizeof(cf) ); cf.cbSize = sizeof(cf); cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
    SendMessageW( hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf );
    /* GetFont2 gives an ITextFont2, which is the range's ITextFont */
    font = font2_of( 0, 5 );
    check( font != NULL, "GetFont2 gives a font", 0 );
    if (!font) { printf( "RESULT: FAIL\n" ); return 1; }
    hr = IUnknown_QueryInterface( (IUnknown *)font, &IID_ITextFont2_, &unk );
    check( hr == S_OK && unk == font, "the font answers ITextFont2", hr );
    base = font;

    /* effects: set on the range's text, read as tom effect bits */
    hr = ((fn_set2)SLOT( font, F_SETEFFECTS ))( font, tomBold | tomItalic, tomBold | tomItalic );
    v = 0; ITextFont_GetBold( base, &v );
    cf = char_format( hwnd, 0, 5 );
    check( hr == S_OK && v == tomTrue && (cf.dwEffects & CFE_ITALIC), "SetEffects makes the text bold and italic", hr );
    hr = ((fn_get2)SLOT( font, F_GETEFFECTS ))( font, &v, &m );
    check( hr == S_OK && (v & 3) == 3 && (m & 3) == 3 && !(v & 4) && (m & 4), "GetEffects: bold and italic, not underlined", v );
    font_mixed = font2_of( 0, 11 );
    ((fn_get2)SLOT( font_mixed, F_GETEFFECTS ))( font_mixed, &v, &m );
    check( !(m & 1) && (m & 4), "over bold and plain text, bold is not in the mask", m );
    hr = ((fn_set2)SLOT( font, F_SETEFFECTS ))( font, 0, tomItalic );
    cf = char_format( hwnd, 0, 5 );
    check( hr == S_OK && !(cf.dwEffects & CFE_ITALIC) && (cf.dwEffects & CFE_BOLD), "SetEffects with a mask changes only what it names", cf.dwEffects );

    /* links and character repertoires are the text's */
    cr.cpMin = 12; cr.cpMax = 16;
    SendMessageW( hwnd, EM_EXSETSEL, 0, (LPARAM)&cr );
    memset( &cf, 0, sizeof(cf) ); cf.cbSize = sizeof(cf); cf.dwMask = CFM_LINK; cf.dwEffects = CFE_LINK;
    SendMessageW( hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf );
    other = font2_of( 12, 16 );
    v = -1; ((fn_get)SLOT( other, F_GETLINKTYPE ))( other, &v );
    t = -1; ((fn_get)SLOT( font, F_GETLINKTYPE ))( font, &t );
    check( v == 1 && t == 0, "GetLinkType: tomClientLink on a link, tomNoLink elsewhere", v );
    hr = ((fn_set)SLOT( font, F_SETCHARREP ))( font, 3 /* tomGreek */ );
    cf = char_format( hwnd, 0, 5 );
    v = -1; ((fn_get)SLOT( font, F_GETCHARREP ))( font, &v );
    check( hr == S_OK && cf.bCharSet == GREEK_CHARSET && v == 3, "SetCharRep(tomGreek) sets the Greek character set", cf.bCharSet );

    /* the options kept by the font and its duplicates */
    hr = ((fn_set)SLOT( font, F_SETAUTOLIGATURES ))( font, tomTrue );
    v = 0; ((fn_get)SLOT( font, F_GETAUTOLIGATURES ))( font, &v );
    check( hr == S_OK && v == tomTrue, "AutoLigatures is kept", v );
    hr = ((fn_set)SLOT( font, F_SETDOUBLESTRIKE ))( font, 5 );
    check( hr == E_INVALIDARG, "a switch takes only tomTrue, tomFalse or tomToggle", hr );
    hr = ((fn_set2)SLOT( font, F_SETPROPERTY ))( font, 0x33c, 7 );
    v = 0; ((fn_getp)SLOT( font, F_GETPROPERTY ))( font, 0x33c, &v );
    m = 0; ((fn_get)SLOT( font, F_GETCOUNT ))( font, &m );
    check( hr == S_OK && v == 7 && m == 1, "SetProperty/GetProperty and GetCount", v );
    v = t = 0;
    hr = ((fn_info)SLOT( font, F_GETPROPERTYINFO ))( font, 0, &t, &v );
    check( hr == S_OK && t == 0x33c && v == 7 && ((fn_info)SLOT( font, F_GETPROPERTYINFO ))( font, 1, &t, &v ) == E_INVALIDARG,
           "GetPropertyInfo lists them, E_INVALIDARG past the end", hr );
    hr = ((fn_ptr)SLOT( font, F_GETDUPLICATE2 ))( font, &dup );
    v = 0; if (dup) ((fn_get)SLOT( dup, F_GETAUTOLIGATURES ))( dup, &v );
    eq = 0; if (dup) ((fn_eq)SLOT( font, F_ISEQUAL2 ))( font, dup, &eq );
    check( hr == S_OK && dup && v == tomTrue && eq == tomTrue, "GetDuplicate2 keeps them; IsEqual2", eq );
    if (dup)
    {
        ((fn_set)SLOT( dup, F_SETAUTOLIGATURES ))( dup, tomFalse );
        eq = 1; ((fn_eq)SLOT( font, F_ISEQUAL2 ))( font, dup, &eq );
        check( eq == tomFalse, "IsEqual2 tells a different option", eq );
        ((fn_get2)SLOT( dup, F_GETEFFECTS ))( dup, &v, &m );
        check( (v & 1) && (m & 1), "a duplicate keeps the effects", v );
        r = range2( 17, 23 );
        hr = ((fn_put)SLOT( r, R_SETFONT2 ))( r, dup );
        cf = char_format( hwnd, 17, 23 );
        check( hr == S_OK && (cf.dwEffects & CFE_BOLD), "SetFont2 applies a duplicate", cf.dwEffects );
    }

    /* paragraphs */
    r = range2( 0, 3 );
    hr = ((fn_ptr)SLOT( r, R_GETPARA2 ))( r, &para );
    check( hr == S_OK && para, "GetPara2 gives a paragraph", hr );
    if (para)
    {
        hr = ((fn_set2)SLOT( para, P_SETEFFECTS ))( para, 4 /* tomParaEffectKeepNext */, 4 );
        cr.cpMin = 0; cr.cpMax = 3;
        SendMessageW( hwnd, EM_EXSETSEL, 0, (LPARAM)&cr );
        SendMessageW( hwnd, EM_GETPARAFORMAT, 0, (LPARAM)&pf );
        check( hr == S_OK && (pf.wEffects & PFE_KEEPNEXT), "ITextPara2 SetEffects keeps the paragraph with the next", pf.wEffects );
        v = m = 0; ((fn_get2)SLOT( para, P_GETEFFECTS ))( para, &v, &m );
        check( (v & 4) && (m & 4) && !(v & 1), "ITextPara2 GetEffects", v );
        hr = ((fn_ptr)SLOT( para, P_GETBORDERS ))( para, &borders );
        check( hr == E_NOTIMPL && !borders, "GetBorders is not implemented, as on Windows", hr );
        ((fn_set)SLOT( para, P_SETHANGING ))( para, tomTrue );
        hr = ((fn_ptr)SLOT( para, P_GETDUPLICATE2 ))( para, &pdup );
        v = 0; if (pdup) ((fn_get)SLOT( pdup, P_GETHANGING ))( pdup, &v );
        eq = 0; if (pdup) ((fn_eq)SLOT( para, P_ISEQUAL2 ))( para, pdup, &eq );
        check( hr == S_OK && v == tomTrue && eq == tomTrue, "a paragraph duplicate keeps HangingPunctuation; IsEqual2", v );
    }

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures ? 1 : 0;
}
