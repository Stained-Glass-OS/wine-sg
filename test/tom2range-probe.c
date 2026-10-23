/* tom2range-probe: TOM2 ranges and the selection (ITextRange2 / ITextSelection2) of a rich edit
 * control (Stained Glass OS wine-sg 6041). ITextRange2 is not in the mingw headers: its own methods
 * are called through the vtable (slot 68 on; 58-67 are ITextSelection's, called as such). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>

static const GUID IID_ITextDocument_ = {0x8cc497c0,0xa1df,0x11ce,{0x80,0x98,0x00,0xaa,0x00,0x47,0xbe,0x5d}};
static const GUID IID_ITextRange2_ = {0xc241f5e2,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};
static const GUID IID_ITextSelection2_ = {0xc241f5e1,0x7206,0x11d8,{0xa2,0xc7,0x00,0xa0,0xd1,0xd6,0xc6,0xb3}};

#define SLOT(o, n) ((*(void ***)(o))[n])
typedef HRESULT (WINAPI *fn_long)( void *, LONG * );
typedef HRESULT (WINAPI *fn_ptr)( void *, void ** );
typedef HRESULT (WINAPI *fn_find)( void *, void *, LONG, LONG, LONG * );
typedef HRESULT (WINAPI *fn_char2)( void *, LONG *, LONG );
typedef HRESULT (WINAPI *fn_sub)( void *, LONG, LONG *, LONG * );
typedef HRESULT (WINAPI *fn_text2)( void *, LONG, BSTR * );
typedef HRESULT (WINAPI *fn_settext2)( void *, LONG, BSTR );
typedef HRESULT (WINAPI *fn_set)( void *, LONG );
#define GETCCH 68
#define GETCOUNT 71
#define GETDUPLICATE2 72
#define GETFORMATTEDTEXT2 75
#define GETGRAVITY 77
#define SETGRAVITY 78
#define GETSTARTPARA 82
#define FIND 89
#define GETCHAR2 90
#define GETSUBRANGE 95
#define GETTEXT2 96
#define SETTEXT2 103

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %ld, %#lx)\n", what, got, got ); failures++; }
}

static LONG cch( void *r ) { LONG v = -99; ((fn_long)SLOT( r, GETCCH ))( r, &v ); return v; }

static void text_is( void *r, const WCHAR *expect, const char *what )
{
    BSTR b = NULL;
    HRESULT hr = ((fn_text2)SLOT( r, GETTEXT2 ))( r, 0, &b );
    check( hr == S_OK && b && !lstrcmpW( b, expect ), what, hr );
    if (b && lstrcmpW( b, expect )) printf( "      text: %ls\n", b );
    SysFreeString( b );
}

int main( void )
{
    IRichEditOle *re;
    ITextDocument *doc;
    ITextSelection *sel;
    ITextRange *r, *r2;
    void *range2 = NULL, *dup2 = NULL, *other2 = NULL, *sel2 = NULL;
    LONG v, first, limit, delta;
    HWND hwnd;
    HRESULT hr;
    BSTR b;

    OleInitialize( NULL );
    LoadLibraryA( "riched20.dll" );
    hwnd = CreateWindowExW( 0, L"RichEdit20W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 400, 200, 0, 0, 0, 0 );
    SendMessageW( hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&re );
    re->lpVtbl->QueryInterface( re, &IID_ITextDocument_, (void **)&doc );
    check( doc != NULL, "ITextDocument", 0 );
    if (!doc) { printf( "RESULT: FAIL\n" ); return 1; }
    SendMessageW( hwnd, WM_SETTEXT, 0, (LPARAM)L"Hello world\rSecond para\rThird" );

    ITextDocument_Range( doc, 0, 5, &r );
    hr = ITextRange_QueryInterface( r, &IID_ITextRange2_, &range2 );
    check( hr == S_OK && range2, "a range has ITextRange2", hr );
    if (!range2) { printf( "RESULT: FAIL\n" ); return 1; }
    check( cch( range2 ) == 5, "GetCch: its length", cch( range2 ) );
    v = 0;
    hr = ((fn_long)SLOT( range2, GETCOUNT ))( range2, &v );
    check( hr == S_OK && v == 1, "GetCount: one subrange", v );
    first = limit = -1;
    hr = ((fn_sub)SLOT( range2, GETSUBRANGE ))( range2, 0, &first, &limit );
    check( hr == S_OK && first == 0 && limit == 5, "GetSubrange 0: the range", limit );
    hr = ((fn_sub)SLOT( range2, GETSUBRANGE ))( range2, 1, &first, &limit );
    check( hr == E_INVALIDARG, "GetSubrange 1: E_INVALIDARG", hr );
    text_is( range2, L"Hello", "GetText2" );
    hr = ((fn_ptr)SLOT( range2, GETDUPLICATE2 ))( range2, &dup2 );
    check( hr == S_OK && dup2 && dup2 != range2 && cch( dup2 ) == 5, "GetDuplicate2: another ITextRange2 of the range", hr );
    v = -1;
    hr = ((fn_char2)SLOT( range2, GETCHAR2 ))( range2, &v, 4 );
    check( hr == S_OK && v == 'o', "GetChar2 at offset 4", v );
    hr = ((fn_char2)SLOT( range2, GETCHAR2 ))( range2, &v, 6 );
    check( hr == S_OK && v == 'w', "GetChar2 past the range's end, in the story", v );
    hr = ((fn_long)SLOT( range2, GETGRAVITY ))( range2, &v );
    check( hr == S_OK && v == 0, "GetGravity: tomGravityUI", v );
    hr = ((fn_set)SLOT( range2, SETGRAVITY ))( range2, 9 );
    check( hr == E_INVALIDARG, "SetGravity out of range: E_INVALIDARG", hr );

    /* the start of the paragraph */
    ITextDocument_Range( doc, 15, 17, &r2 );
    ITextRange_QueryInterface( r2, &IID_ITextRange2_, &other2 );
    v = -1;
    hr = ((fn_long)SLOT( other2, GETSTARTPARA ))( other2, &v );
    check( hr == S_OK && v == 12, "GetStartPara: where the second paragraph starts", v );
    ITextRange_Release( r2 );

    /* ITextSelection's methods on a range */
    v = -1;
    hr = ITextSelection_GetType( (ITextSelection *)range2, &v );
    check( hr == S_OK && v == tomSelectionNormal, "a range's GetType: tomSelectionNormal", v );
    delta = 0;
    hr = ITextSelection_MoveRight( (ITextSelection *)range2, tomCharacter, 2, tomMove, &delta );
    ITextRange_GetStart( r, &first );
    check( hr == S_OK && delta == 2 && first == 6 && cch( range2 ) == 0,
           "MoveRight 2 on a range: collapses to its end, then one more", first );
    hr = ITextSelection_MoveRight( (ITextSelection *)range2, tomWord, 1, tomExtend, &delta );
    check( hr == S_OK && cch( range2 ) == 5, "MoveRight a word, extending: the word", cch( range2 ) );
    text_is( range2, L"world", "and it is \"world\"" );
    hr = ITextSelection_TypeText( (ITextSelection *)range2, (b = SysAllocString( L"there" )) );
    SysFreeString( b );
    ITextRange_GetStart( r, &first );
    check( hr == S_OK && first == 11 && cch( range2 ) == 0, "TypeText replaces the range and follows the text", first );
    ITextDocument_Range( doc, 0, 11, &r2 );
    b = NULL;
    ITextRange_GetText( r2, &b );
    check( b && !lstrcmpW( b, L"Hello there" ), "the text is changed", 0 );
    SysFreeString( b );
    ITextRange_Release( r2 );
    hr = ITextSelection_HomeKey( (ITextSelection *)range2, tomStory, tomMove, &delta );
    ITextRange_GetStart( r, &first );
    check( hr == S_OK && first == 0, "HomeKey tomStory: the story's start", first );

    /* SetText2 and Find */
    ITextRange_SetRange( r, 0, 5 );
    hr = ((fn_settext2)SLOT( range2, SETTEXT2 ))( range2, 0, (b = SysAllocString( L"Howdy" )) );
    SysFreeString( b );
    text_is( range2, L"Howdy", "SetText2" );
    ITextRange_SetRange( r, 12, 12 );
    ITextDocument_Range( doc, 0, 0, &r2 );
    ITextRange_SetText( r2, NULL );
    ITextRange_Release( r2 );
    {
        ITextRange *word;
        void *word2 = NULL;
        ITextDocument_Range( doc, 19, 23, &word );   /* "para" */
        ITextRange_QueryInterface( word, &IID_ITextRange2_, &word2 );
        ITextRange_SetRange( r, 0, 0 );
        delta = 0;
        hr = ((fn_find)SLOT( range2, FIND ))( range2, word2, tomForward, 0, &delta );
        ITextRange_GetStart( r, &first );
        check( hr == S_OK && delta == 4 && first == 19, "Find: the other range's text", first );
        if (word2) ((IUnknown *)word2)->lpVtbl->Release( word2 );
        ITextRange_Release( word );
    }

    /* the selection */
    ITextDocument_GetSelection( doc, &sel );
    hr = ITextSelection_QueryInterface( sel, &IID_ITextSelection2_, &sel2 );
    check( hr == S_OK && sel2, "the selection has ITextSelection2", hr );
    if (sel2)
    {
        SendMessageW( hwnd, EM_SETSEL, 2, 6 );
        check( cch( sel2 ) == 4, "the selection's GetCch", cch( sel2 ) );
        text_is( sel2, L"wdy ", "the selection's GetText2" );
        ((IUnknown *)sel2)->lpVtbl->Release( sel2 );
    }
    hr = ITextSelection_QueryInterface( sel, &IID_ITextRange2_, &other2 );
    check( hr == S_OK, "and ITextRange2", hr );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
