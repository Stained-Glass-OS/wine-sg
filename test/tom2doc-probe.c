/* tom2doc-probe: ITextDocument2's Range2, RangeFromPoint2, GetSelection2 and the document font
 * (GetDocumentFont/SetDocumentFont) of a rich edit control (Stained Glass OS wine-sg 8006); all were
 * E_NOTIMPL. ITextDocument2 is not in the mingw headers: its methods are called through the vtable
 * (ITextDocument has 19 methods after IDispatch's 7, so ITextDocument2's own begin at slot 26). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>

static const GUID IID_ITextDocument2_ = {0xc241f5e0,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};
static const GUID IID_ITextDocument_ = {0x8cc497c0,0xa1df,0x11ce,{0x80,0x98,0x00,0xaa,0x00,0x47,0xbe,0x5d}};
static const GUID IID_ITextRange2_ = {0xc241f5e2,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};
static const GUID IID_ITextSelection2_ = {0xc241f5e1,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};
static const GUID IID_ITextFont2_ = {0xc241f5e3,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};

#define SLOT(o, n) ((*(void ***)(o))[n])
#define GETDOCUMENTFONT 29
#define SETDOCUMENTFONT 30
#define GETSELECTION2   38
#define RANGE2          53
#define RANGEFROMPOINT2 54
typedef HRESULT (WINAPI *fn_ptr)( void *, void ** );
typedef HRESULT (WINAPI *fn_set)( void *, void * );
typedef HRESULT (WINAPI *fn_range2)( void *, LONG, LONG, void ** );
typedef HRESULT (WINAPI *fn_point2)( void *, LONG, LONG, LONG, void ** );

static int failures;
static void check( int ok, const char *what, long v )
{
    printf( "%s %s (%#lx)\n", ok ? "ok  " : "FAIL", what, v );
    if (!ok) failures++;
}

int main( void )
{
    IRichEditOle *re = NULL;
    ITextDocument *doc = NULL;
    void *doc2 = NULL, *range2 = NULL, *sel2 = NULL, *unk = NULL;
    ITextSelection *sel = NULL;
    ITextFont *font = NULL, *rfont = NULL;
    ITextRange *r = NULL;
    CHARFORMAT2W cf;
    BSTR text = NULL, name = NULL;
    LONG v = 0, start = -1;
    float size = 0;
    HWND hwnd;
    HRESULT hr;
    LRESULT pos;

    LoadLibraryA( "riched20.dll" );
    hwnd = CreateWindowExW( 0, L"RichEdit20W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 400, 200, 0, 0, 0, 0 );
    SetWindowTextW( hwnd, L"hello world" );
    SendMessageW( hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&re );
    re->lpVtbl->QueryInterface( re, &IID_ITextDocument2_, &doc2 );
    check( doc2 != NULL, "ITextDocument2", 0 );
    if (!doc2) { printf( "RESULT: FAIL\n" ); return 1; }
    re->lpVtbl->QueryInterface( re, &IID_ITextDocument_, (void **)&doc );

    hr = ((fn_range2)SLOT( doc2, RANGE2 ))( doc2, 0, 5, &range2 );
    check( hr == S_OK && range2, "Range2(0, 5)", hr );
    if (range2)
    {
        ITextRange_GetText( (ITextRange *)range2, &text );
        check( text && !lstrcmpW( text, L"hello" ), "its text is \"hello\"", 0 );
        SysFreeString( text );
        hr = IUnknown_QueryInterface( (IUnknown *)range2, &IID_ITextRange2_, &unk );
        check( hr == S_OK && unk == range2, "it is an ITextRange2", hr );
        if (unk) IUnknown_Release( (IUnknown *)unk );
        IUnknown_Release( (IUnknown *)range2 );
    }

    hr = ((fn_ptr)SLOT( doc2, GETSELECTION2 ))( doc2, &sel2 );
    ITextDocument_GetSelection( doc, &sel );
    check( hr == S_OK && sel2 && (void *)sel == sel2, "GetSelection2 is the document's selection", hr );
    if (sel2) IUnknown_Release( (IUnknown *)sel2 );
    if (sel) ITextSelection_Release( sel );

    pos = SendMessageW( hwnd, EM_POSFROMCHAR, 6, 0 );
    range2 = NULL;
    hr = ((fn_point2)SLOT( doc2, RANGEFROMPOINT2 ))( doc2, (short)LOWORD(pos) + 1, (short)HIWORD(pos) + 2,
                                                       tomClientCoord, &range2 );
    if (range2) ITextRange_GetStart( (ITextRange *)range2, &start );
    check( hr == S_OK && range2 && start == 6, "RangeFromPoint2 in client coordinates: the character there", start );
    if (range2) IUnknown_Release( (IUnknown *)range2 );

    /* the default character format as a font */
    hr = ((fn_ptr)SLOT( doc2, GETDOCUMENTFONT ))( doc2, (void **)&font );
    check( hr == S_OK && font, "GetDocumentFont", hr );
    if (!font) { printf( "RESULT: FAIL\n" ); return 1; }
    memset( &cf, 0, sizeof(cf) );
    cf.cbSize = sizeof(cf);
    SendMessageW( hwnd, EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf );
    ITextFont_GetName( font, &name );
    check( name && !lstrcmpW( name, cf.szFaceName ), "its name is the default format's face", 0 );
    SysFreeString( name );
    ITextFont_GetSize( font, &size );
    check( (LONG)(size * 20 + 0.5) == cf.yHeight, "its size is the default format's", (long)(size * 20) );
    v = 0;
    ITextFont_GetItalic( font, &v );
    check( v == tomFalse && !(cf.dwEffects & CFE_ITALIC), "not italic, as the default format", v );
    v = 0;
    ITextFont_GetBold( font, &v );
    check( v == ((cf.dwEffects & CFE_BOLD) ? tomTrue : tomFalse), "bold as the default format is", cf.dwEffects );
    hr = IUnknown_QueryInterface( (IUnknown *)font, &IID_ITextFont2_, &unk );
    check( hr == S_OK, "it is an ITextFont2", hr );
    if (unk) IUnknown_Release( (IUnknown *)unk );

    /* made italic, it becomes the default format; text in the default style follows */
    ITextFont_SetItalic( font, tomTrue );
    hr = ((fn_set)SLOT( doc2, SETDOCUMENTFONT ))( doc2, font );
    check( hr == S_OK, "SetDocumentFont with Italic set", hr );
    memset( &cf, 0, sizeof(cf) );
    cf.cbSize = sizeof(cf);
    SendMessageW( hwnd, EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf );
    check( (cf.dwEffects & CFE_ITALIC) != 0, "EM_GETCHARFORMAT(SCF_DEFAULT) is italic", cf.dwEffects );
    ITextDocument_Range( doc, 0, 5, &r );
    ITextRange_GetFont( r, &rfont );
    v = 0;
    if (rfont) ITextFont_GetItalic( rfont, &v );
    check( v == tomTrue, "the text in the default format is italic", v );
    if (rfont) ITextFont_Release( rfont );
    ITextRange_Release( r );
    ITextFont_Release( font );

    IUnknown_Release( (IUnknown *)doc2 );
    ITextDocument_Release( doc );
    re->lpVtbl->Release( re );
    DestroyWindow( hwnd );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
