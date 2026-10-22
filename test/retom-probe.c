/* retom-probe: rich edit Text Object Model (Stained Glass OS wine-sg 6004):
 * GetFormattedText / SetFormattedText, SetPoint, GetEmbeddedObject on ranges
 * and the selection, and ITextSelection::MoveUp/MoveDown by paragraph, screen
 * and window. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>

static const GUID IID_ITextDocument_ = {0x8cc497c0,0xa1df,0x11ce,{0x80,0x98,0x00,0xaa,0x00,0x47,0xbe,0x5d}};
static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %ld)\n", what, got ); failures++; }
}

static LONG start_of( ITextRange *r ) { LONG v = -1; ITextRange_GetStart( r, &v ); return v; }
static LONG end_of( ITextRange *r ) { LONG v = -1; ITextRange_GetEnd( r, &v ); return v; }

static HWND g_hwnd;

/* tomTrue when the character at cp is bold (through the window, not TOM) */
static LONG bold_at( ITextDocument *doc, LONG cp )
{
    CHARFORMAT2W cf = { sizeof(cf) };
    CHARRANGE old, cr = { cp, cp + 1 };
    SendMessageW( g_hwnd, EM_EXGETSEL, 0, (LPARAM)&old );
    SendMessageW( g_hwnd, EM_EXSETSEL, 0, (LPARAM)&cr );
    SendMessageW( g_hwnd, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf );
    SendMessageW( g_hwnd, EM_EXSETSEL, 0, (LPARAM)&old );
    return (cf.dwMask & CFM_BOLD) && (cf.dwEffects & CFE_BOLD) ? tomTrue : tomFalse;
}

static void text_is( HWND hwnd, const WCHAR *expect, const char *what )
{
    WCHAR buf[256];
    GETTEXTEX gt = { sizeof(buf), GT_DEFAULT, 1200, NULL, NULL };
    SendMessageW( hwnd, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)buf );
    check( !lstrcmpW( buf, expect ), what, buf[0] );
    if (lstrcmpW( buf, expect )) printf( "      text: %ls\n", buf );
}

int main( void )
{
    IRichEditOle *re;
    ITextDocument *doc;
    ITextSelection *sel;
    ITextRange *r, *f, *dst;
    ITextFont *font;
    IUnknown *obj;
    LONG x, y, d, s, e;
    HWND hwnd;
    HRESULT hr;

    OleInitialize( NULL );
    LoadLibraryA( "riched20.dll" );
    hwnd = g_hwnd = CreateWindowExW( 0, L"RichEdit20W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 400, 200, 0, 0, 0, 0 );
    SendMessageW( hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&re );
    re->lpVtbl->QueryInterface( re, &IID_ITextDocument_, (void **)&doc );
    check( doc != NULL, "ITextDocument", 0 );
    ITextDocument_GetSelection( doc, &sel );
    SendMessageW( hwnd, WM_SETTEXT, 0, (LPARAM)L"Hello world\rSecond para\rThird" );

    /* formatted text */
    /* the default font of a bare prefix may be a bold one: plain text first */
    ITextDocument_Range( doc, 0, 100, &r );
    ITextRange_GetFont( r, &font );
    ITextFont_SetBold( font, tomFalse );
    ITextFont_Release( font );
    ITextRange_Release( r );
    check( bold_at( doc, 8 ) == tomFalse, "plain text to start with", bold_at( doc, 8 ) );
    ITextDocument_Range( doc, 0, 5, &r );
    ITextRange_GetFont( r, &font );
    ITextFont_SetBold( font, tomTrue );
    ITextFont_Release( font );
    hr = ITextRange_GetFormattedText( r, &f );
    check( hr == S_OK && f && start_of( f ) == 0 && end_of( f ) == 5, "GetFormattedText is a range over the text", hr );
    ITextDocument_Range( doc, 12, 18, &dst );   /* "Second" */
    hr = ITextRange_SetFormattedText( dst, f );
    check( hr == S_OK, "SetFormattedText", hr );
    text_is( hwnd, L"Hello world\rHello para\rThird", "SetFormattedText replaced the text" );
    check( start_of( dst ) == 12 && end_of( dst ) == 17, "the range holds the new text", end_of( dst ) );
    check( bold_at( doc, 12 ) == tomTrue && bold_at( doc, 16 ) == tomTrue, "with its formatting (bold)", bold_at( doc, 12 ) );
    check( bold_at( doc, 18 ) != tomTrue, "and only there", bold_at( doc, 18 ) );
    ITextRange_Release( dst );

    ITextSelection_SetStart( sel, 23 );
    ITextSelection_SetEnd( sel, 28 );            /* "Third" */
    hr = ITextSelection_SetFormattedText( sel, f );
    text_is( hwnd, L"Hello world\rHello para\rHello", "ITextSelection::SetFormattedText" );
    ITextSelection_GetStart( sel, &s );
    ITextSelection_GetEnd( sel, &e );
    check( hr == S_OK && s == 23 && e == 28 && bold_at( doc, 23 ) == tomTrue, "the selection holds the new formatted text", e );
    hr = ITextSelection_GetFormattedText( sel, &dst );
    check( hr == S_OK && start_of( dst ) == 23 && end_of( dst ) == 28, "ITextSelection::GetFormattedText", hr );
    ITextRange_Release( dst );
    ITextRange_Release( f );

    /* SetPoint */
    ITextDocument_Range( doc, 6, 6, &dst );
    ITextRange_GetPoint( dst, tomStart | tomClientCoord, &x, &y );
    ITextRange_SetStart( r, 0 );
    ITextRange_SetEnd( r, 2 );
    hr = ITextRange_SetPoint( r, x + 1, y + 2, tomStart | tomClientCoord, 0 );
    check( hr == S_OK && start_of( r ) == 6 && end_of( r ) == 6, "SetPoint makes an insertion point there", start_of( r ) );
    ITextRange_SetStart( r, 0 );
    ITextRange_SetEnd( r, 2 );
    hr = ITextRange_SetPoint( r, x + 1, y + 2, tomEnd | tomClientCoord, 1 );
    check( hr == S_OK && start_of( r ) == 0 && end_of( r ) == 6, "SetPoint with Extend moves the end", end_of( r ) );
    ITextRange_SetStart( r, 8 );
    ITextRange_SetEnd( r, 10 );
    hr = ITextRange_SetPoint( r, x + 1, y + 2, tomStart | tomClientCoord, 1 );
    check( hr == S_OK && start_of( r ) == 6 && end_of( r ) == 10, "SetPoint with Extend moves the start", start_of( r ) );
    {
        POINT pt = { x + 1, y + 2 };
        ClientToScreen( hwnd, &pt );
        hr = ITextSelection_SetPoint( sel, pt.x, pt.y, tomStart, 0 );
        ITextSelection_GetStart( sel, &s );
        ITextSelection_GetEnd( sel, &e );
        check( hr == S_OK && s == 6 && e == 6, "ITextSelection::SetPoint (screen coordinates)", s );
    }
    ITextRange_Release( dst );

    /* embedded objects */
    obj = (void *)1;
    hr = ITextRange_GetEmbeddedObject( r, &obj );
    check( hr == S_FALSE && !obj, "GetEmbeddedObject with no object: S_FALSE, NULL", hr );
    OpenClipboard( hwnd );
    EmptyClipboard();
    {
        HDC hdc = GetDC( NULL );
        SetClipboardData( CF_BITMAP, CreateCompatibleBitmap( hdc, 16, 16 ) );
        ReleaseDC( NULL, hdc );
    }
    CloseClipboard();
    ITextSelection_SetStart( sel, 0 );
    ITextSelection_SetEnd( sel, 0 );
    SendMessageW( hwnd, WM_PASTE, 0, 0 );
    ITextRange_SetStart( r, 0 );
    ITextRange_SetEnd( r, 0 );
    hr = ITextRange_GetEmbeddedObject( r, &obj );
    check( hr == S_OK && obj, "GetEmbeddedObject gives the object at the start", hr );
    if (hr == S_OK && obj) IUnknown_Release( obj );
    ITextSelection_SetStart( sel, 0 );
    ITextSelection_SetEnd( sel, 1 );
    obj = NULL;
    hr = ITextSelection_GetEmbeddedObject( sel, &obj );
    check( hr == S_OK && obj, "ITextSelection::GetEmbeddedObject", hr );
    if (obj) IUnknown_Release( obj );
    ITextSelection_Delete( sel, tomCharacter, 0, NULL );

    /* MoveUp / MoveDown */
    SendMessageW( hwnd, WM_SETTEXT, 0, (LPARAM)L"Hello world\rSecond para\rThird" );
    ITextSelection_SetStart( sel, 15 );
    ITextSelection_SetEnd( sel, 15 );
    hr = ITextSelection_MoveUp( sel, tomParagraph, 1, tomMove, &d );
    ITextSelection_GetStart( sel, &s );
    check( hr == S_OK && s == 12 && d == 1, "MoveUp(tomParagraph): to the start of the paragraph", s );
    hr = ITextSelection_MoveUp( sel, tomParagraph, 1, tomMove, &d );
    ITextSelection_GetStart( sel, &s );
    check( hr == S_OK && s == 0, "MoveUp(tomParagraph) again: the paragraph before", s );
    hr = ITextSelection_MoveDown( sel, tomParagraph, 2, tomMove, &d );
    ITextSelection_GetStart( sel, &s );
    check( hr == S_OK && s == 24 && d == 2, "MoveDown(tomParagraph, 2)", s );
    ITextSelection_SetStart( sel, 2 );
    ITextSelection_SetEnd( sel, 2 );
    hr = ITextSelection_MoveDown( sel, tomParagraph, 1, tomExtend, &d );
    ITextSelection_GetStart( sel, &s );
    ITextSelection_GetEnd( sel, &e );
    check( hr == S_OK && s == 2 && e == 12, "MoveDown(tomParagraph, tomExtend) extends", e );
    ITextSelection_SetStart( sel, 0 );
    ITextSelection_SetEnd( sel, 0 );
    hr = ITextSelection_MoveDown( sel, tomWindow, 1, tomMove, &d );
    ITextSelection_GetStart( sel, &s );
    check( SUCCEEDED(hr) && s >= 24 && d == 1, "MoveDown(tomWindow): to the last line in view", s );
    hr = ITextSelection_MoveUp( sel, tomWindow, 1, tomMove, &d );
    ITextSelection_GetStart( sel, &s );
    check( SUCCEEDED(hr) && s == 0 && d == 1, "MoveUp(tomWindow): to the first line in view", s );
    {
        /* a long text, scrolled: the first line in view is line 20 */
        WCHAR big[2000] = L"";
        int i;
        LONG line20, first;
        for (i = 0; i < 60; i++) lstrcatW( big, L"line of text\r" );
        SendMessageW( hwnd, WM_SETTEXT, 0, (LPARAM)big );
        line20 = SendMessageW( hwnd, EM_LINEINDEX, 20, 0 );
        ITextSelection_SetStart( sel, SendMessageW( hwnd, EM_LINEINDEX, 25, 0 ) );
        ITextSelection_SetEnd( sel, SendMessageW( hwnd, EM_LINEINDEX, 25, 0 ) );
        SendMessageW( hwnd, EM_LINESCROLL, 0, 20 - SendMessageW( hwnd, EM_GETFIRSTVISIBLELINE, 0, 0 ) );
        first = SendMessageW( hwnd, EM_GETFIRSTVISIBLELINE, 0, 0 );
        hr = ITextSelection_MoveUp( sel, tomWindow, 1, tomMove, &d );
        ITextSelection_GetStart( sel, &s );
        check( first == 20 && SUCCEEDED(hr) && s == line20, "MoveUp(tomWindow) in a scrolled text: the first line in view", s );
    }
    hr = ITextSelection_MoveDown( sel, tomScreen, 1, tomMove, &d );
    ITextSelection_GetStart( sel, &s );
    check( SUCCEEDED(hr) && s > 0, "MoveDown(tomScreen)", s );

    ITextRange_Release( r );
    ITextSelection_Release( sel );
    ITextDocument_Release( doc );
    DestroyWindow( hwnd );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
