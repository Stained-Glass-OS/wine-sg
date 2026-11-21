/* wine-sg 8411: resources of a language-neutral module come from its MUI file,
 * <dir>\<language>\<name>.mui, through FindResource/LoadResource/LoadString, for every way of
 * loading it (Notepad3 loads lng\np3lng.dll with LOAD_LIBRARY_AS_DATAFILE|AS_IMAGE_RESOURCE and
 * exits when its strings and dialogs are not found). Beside it: ln.dll, ln2.dll, plain.dll (built
 * from muires-*.rc) and sat.mui; this probe puts sat.mui where each case wants it. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static WCHAR dir[MAX_PATH];

static void check( int ok, const char *what )
{
    printf( "%s  %s\n", ok ? "PASS" : "FAIL", what );
    if (!ok) failures++;
}

static void place( const WCHAR *locale, const WCHAR *name )
{
    WCHAR d[MAX_PATH], src[MAX_PATH], dst[MAX_PATH];
    swprintf( d, MAX_PATH, L"%ls\\%ls", dir, locale );
    CreateDirectoryW( d, NULL );
    swprintf( src, MAX_PATH, L"%ls\\sat.mui", dir );
    swprintf( dst, MAX_PATH, L"%ls\\%ls.mui", d, name );
    CopyFileW( src, dst, FALSE );
}

static int data_is( HMODULE mod, int id, const char *want )
{
    HRSRC r = FindResourceW( mod, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA );
    const char *p;
    if (!r) return 0;
    p = LockResource( LoadResource( mod, r ));
    return p && SizeofResource( mod, r ) == strlen( want ) + 1 && !memcmp( p, want, strlen( want ) + 1 );
}

static void cases( const char *how, HMODULE mod, int satellite )
{
    WCHAR buf[64] = {0};
    char what[160];
    HRSRC r;

    snprintf( what, sizeof(what), "%s: loaded", how );
    check( mod != NULL, what );
    if (!mod) return;
    LoadStringW( mod, 100, buf, ARRAYSIZE(buf) );
    snprintf( what, sizeof(what), "%s: LoadString %s the MUI file (\"%ls\")", how, satellite ? "from" : "not from", buf );
    check( satellite ? !wcscmp( buf, L"from the mui" ) : !buf[0], what );
    snprintf( what, sizeof(what), "%s: FindResource+LoadResource %s the MUI file", how, satellite ? "from" : "not from" );
    check( satellite ? data_is( mod, 3, "SATDATA" ) : !FindResourceW( mod, MAKEINTRESOURCEW(3), (LPCWSTR)RT_RCDATA ), what );
    snprintf( what, sizeof(what), "%s: the module's own resource still its own", how );
    check( data_is( mod, 2, "MAINDATA" ), what );
    SetLastError( 0xdeadbeef );
    r = FindResourceW( mod, MAKEINTRESOURCEW(9), (LPCWSTR)RT_RCDATA );
    snprintf( what, sizeof(what), "%s: a resource neither has: not found, error %lu", how, GetLastError() );
    check( !r && GetLastError() == ERROR_RESOURCE_NAME_NOT_FOUND, what );
}

int main( void )
{
    WCHAR path[MAX_PATH], ui[LOCALE_NAME_MAX_LENGTH], *p;
    HMODULE mod;

    GetModuleFileNameW( NULL, dir, MAX_PATH );
    if ((p = wcsrchr( dir, '\\' ))) *p = 0;
    if (!LCIDToLocaleName( MAKELCID( GetUserDefaultUILanguage(), SORT_DEFAULT ), ui, ARRAYSIZE(ui), 0 ))
        wcscpy( ui, L"en-US" );
    printf( "UI language %ls\n", ui );
    place( ui, L"ln.dll" );
    place( ui, L"plain.dll" );
    place( L"en-US", L"ln2.dll" );

    swprintf( path, MAX_PATH, L"%ls\\ln.dll", dir );
    mod = LoadLibraryExW( path, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE );
    cases( "data file + image resource (Notepad3)", mod, 1 );
    if (mod) FreeLibrary( mod );
    mod = LoadLibraryExW( path, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE );
    cases( "loaded again after FreeLibrary", mod, 1 );
    if (mod) FreeLibrary( mod );
    mod = LoadLibraryExW( path, NULL, LOAD_LIBRARY_AS_DATAFILE );
    cases( "data file", mod, 1 );
    if (mod) FreeLibrary( mod );
    mod = LoadLibraryW( path );
    cases( "LoadLibrary", mod, 1 );
    if (mod) FreeLibrary( mod );

    swprintf( path, MAX_PATH, L"%ls\\ln2.dll", dir );
    mod = LoadLibraryExW( path, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE );
    cases( "only an en-US MUI file (the last fallback)", mod, 1 );
    if (mod) FreeLibrary( mod );

    swprintf( path, MAX_PATH, L"%ls\\plain.dll", dir );
    mod = LoadLibraryExW( path, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE );
    cases( "not language-neutral: a .mui beside it is not used", mod, 0 );
    if (mod) FreeLibrary( mod );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
