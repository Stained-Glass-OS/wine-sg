/* kbdhkl-probe: MapVirtualKeyEx, VkKeyScanEx, ToUnicodeEx and key input
 * follow the keyboard layout (HKL), not the X keyboard map (Stained Glass OS
 * wine-sg 6000). The X server runs a US map.
 *   kbdhkl-probe.exe            table checks
 *   kbdhkl-probe.exe keys FILE  activate French, write READY to FILE, report the keys typed */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check( int ok, const char *what, unsigned int got, unsigned int expect )
{
    if (ok) printf( "ok    %s (%#x)\n", what, got );
    else { printf( "FAIL  %s: got %#x, expected %#x\n", what, got, expect ); failures++; }
}

static HKL load( const char *klid )
{
    HKL hkl = LoadKeyboardLayoutA( klid, 0 );
    if (!hkl) { printf( "FAIL  LoadKeyboardLayout %s: error %lu\n", klid, GetLastError() ); failures++; }
    return hkl;
}

/* the virtual key and scan code a character comes from */
static void scan_char( HKL hkl, const char *name, WCHAR ch, unsigned int vk, unsigned int scan )
{
    char what[128];
    SHORT r = VkKeyScanExW( ch, hkl );
    UINT s;

    snprintf( what, sizeof(what), "%s: VkKeyScanEx(U+%04X)", name, ch );
    check( r == (SHORT)vk, what, (USHORT)r, vk );
    s = MapVirtualKeyExW( LOBYTE(r), MAPVK_VK_TO_VSC, hkl );
    snprintf( what, sizeof(what), "%s: MapVirtualKeyEx(VK %#x, VK_TO_VSC)", name, LOBYTE(r) );
    check( s == scan, what, s, scan );
    s = MapVirtualKeyExW( scan, MAPVK_VSC_TO_VK, hkl );
    snprintf( what, sizeof(what), "%s: MapVirtualKeyEx(scan %#x, VSC_TO_VK)", name, scan );
    check( s == (vk & 0xff), what, s, vk & 0xff );
}

static void unicode( HKL hkl, const char *name, UINT vk, UINT scan, BYTE *state, int expect_ret, WCHAR expect )
{
    WCHAR buf[4] = {0};
    char what[128];
    int r = ToUnicodeEx( vk, scan, state, buf, 4, 0, hkl );

    snprintf( what, sizeof(what), "%s: ToUnicodeEx(VK %#x) returns", name, vk );
    check( r == expect_ret, what, r, expect_ret );
    snprintf( what, sizeof(what), "%s: ToUnicodeEx(VK %#x) character", name, vk );
    check( buf[0] == expect, what, buf[0], expect );
}

static HKL fr;
static int got_vk, got_char;

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_KEYDOWN && !got_vk) got_vk = wp;
    if (msg == WM_CHAR && !got_char) got_char = wp;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static int keys( const char *file )
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    FILE *f;
    MSG msg;
    DWORD start;
    INPUT in[2] = {0};

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.lpszClassName = L"kbdhkl";
    RegisterClassW( &wc );
    hwnd = CreateWindowW( L"kbdhkl", L"kbdhkl", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0 );
    SetForegroundWindow( hwnd );
    SetFocus( hwnd );
    if (!(fr = load( "0000040c" ))) return 1;
    ActivateKeyboardLayout( fr, 0 );
    start = GetTickCount();
    while (GetTickCount() - start < 1000)
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }

    /* injected by scan code: the layout's key */
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wScan = in[1].ki.wScan = 0x10;
    in[0].ki.dwFlags = KEYEVENTF_SCANCODE;
    in[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput( 2, in, sizeof(INPUT) );
    start = GetTickCount();
    while (GetTickCount() - start < 2000 && !got_char)
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }
    check( got_vk == 'A', "fr: SendInput scan 0x10: WM_KEYDOWN", got_vk, 'A' );
    check( got_char == 'a', "fr: SendInput scan 0x10: WM_CHAR", got_char, 'a' );

    /* typed on the X server (its map is US: the key there is "q") */
    got_vk = got_char = 0;
    f = fopen( file, "w" ); fputs( "READY\n", f ); fclose( f );
    start = GetTickCount();
    while (GetTickCount() - start < 15000 && !got_char)
    {
        MsgWaitForMultipleObjects( 0, NULL, FALSE, 100, QS_ALLINPUT );
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }
    }
    check( got_vk == 'A', "fr: X key q: WM_KEYDOWN", got_vk, 'A' );
    check( got_char == 'a', "fr: X key q: WM_CHAR", got_char, 'a' );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}

int main( int argc, char **argv )
{
    HKL us, de, ru;
    BYTE state[256] = {0};
    WCHAR name[64];
    UINT r;

    if (argc > 2 && !strcmp( argv[1], "keys" )) return keys( argv[2] );

    us = GetKeyboardLayout( 0 );
    printf( "default layout %p\n", us );
    if (!(fr = load( "0000040c" )) || !(de = load( "00000407" )) || !(ru = load( "00000419" )))
    {
        printf( "RESULT: FAIL\n" );
        return 1;
    }

    /* the session's own layout keeps the X map: US */
    scan_char( us, "us", 'a', 'A', 0x1e );
    scan_char( us, "us", ';', VK_OEM_1, 0x27 );
    scan_char( us, "us", 'q', 'Q', 0x10 );

    /* French (AZERTY, kbdfr's virtual keys) */
    scan_char( fr, "fr", 'a', 'A', 0x10 );
    scan_char( fr, "fr", 'q', 'Q', 0x1e );
    scan_char( fr, "fr", 'z', 'Z', 0x11 );
    scan_char( fr, "fr", 'w', 'W', 0x2c );
    scan_char( fr, "fr", 'm', 'M', 0x27 );
    scan_char( fr, "fr", '$', VK_OEM_1, 0x1b );
    scan_char( fr, "fr", ':', VK_OEM_2, 0x34 );
    scan_char( fr, "fr", ',', VK_OEM_COMMA, 0x32 );
    scan_char( fr, "fr", '!', VK_OEM_8, 0x35 );
    scan_char( fr, "fr", '&', '1', 0x02 );
    scan_char( fr, "fr", '1', 0x100 | '1', 0x02 );
    scan_char( fr, "fr", 0xe9 /* e acute */, '2', 0x03 );
    scan_char( fr, "fr", '^', VK_OEM_6, 0x1a );
    r = MapVirtualKeyExW( VK_OEM_6, MAPVK_VK_TO_CHAR, fr );
    check( r == 0x8000005e, "fr: MapVirtualKeyEx(VK_OEM_6, VK_TO_CHAR) is the dead ^", r, 0x8000005e );
    r = MapVirtualKeyExA( VK_OEM_6, MAPVK_VK_TO_CHAR, fr );
    check( r == 0x8000005e, "fr: MapVirtualKeyExA(VK_OEM_6, VK_TO_CHAR) keeps the dead mark", r, 0x8000005e );
    r = MapVirtualKeyExW( 'A', MAPVK_VK_TO_CHAR, fr );
    check( r == 'A', "fr: MapVirtualKeyEx('A', VK_TO_CHAR)", r, 'A' );
    r = MapVirtualKeyExW( VK_OEM_1, MAPVK_VK_TO_CHAR, fr );
    check( r == '$', "fr: MapVirtualKeyEx(VK_OEM_1, VK_TO_CHAR)", r, '$' );
    unicode( fr, "fr", 'A', 0x10, state, 1, 'a' );
    unicode( fr, "fr", VK_OEM_6, 0x1a, state, -1, '^' );
    unicode( fr, "fr", '2', 0x03, state, 1, 0xe9 );
    state[VK_SHIFT] = 0x80;
    unicode( fr, "fr", '2', 0x03, state, 1, '2' );
    unicode( fr, "fr", 'A', 0x10, state, 1, 'A' );
    state[VK_SHIFT] = 0;
    state[VK_CONTROL] = state[VK_MENU] = 0x80;  /* AltGr */
    unicode( fr, "fr", VK_OEM_PLUS, 0x0d, state, 1, '}' );
    state[VK_CONTROL] = state[VK_MENU] = 0;
    state[VK_CAPITAL] = 1;
    unicode( fr, "fr", 'A', 0x10, state, 1, 'A' );
    state[VK_CAPITAL] = 0;

    /* German (QWERTZ, kbdgr's virtual keys) */
    scan_char( de, "de", 'z', 'Z', 0x15 );
    scan_char( de, "de", 'y', 'Y', 0x2c );
    scan_char( de, "de", 0xfc /* u diaeresis */, VK_OEM_1, 0x1a );
    scan_char( de, "de", 0xf6 /* o diaeresis */, VK_OEM_3, 0x27 );
    scan_char( de, "de", 0xdf /* sharp s */, VK_OEM_4, 0x0c );
    scan_char( de, "de", '#', VK_OEM_2, 0x2b );
    scan_char( de, "de", '+', VK_OEM_PLUS, 0x1b );
    scan_char( de, "de", '-', VK_OEM_MINUS, 0x35 );
    unicode( de, "de", VK_OEM_1, 0x1a, state, 1, 0xfc );
    state[VK_CONTROL] = state[VK_MENU] = 0x80;
    unicode( de, "de", 'Q', 0x10, state, 1, '@' );
    state[VK_CONTROL] = state[VK_MENU] = 0;

    /* Russian: Cyrillic letters on the US virtual keys */
    scan_char( ru, "ru", 0x0444 /* ef */, 'A', 0x1e );
    unicode( ru, "ru", 'A', 0x1e, state, 1, 0x0444 );

    /* the US layout is untouched by the others */
    unicode( us, "us", VK_OEM_1, 0x27, state, 1, ';' );
    unicode( us, "us", 'A', 0x1e, state, 1, 'a' );

    /* key names follow the active layout */
    ActivateKeyboardLayout( fr, 0 );
    memset( name, 0, sizeof(name) );
    GetKeyNameTextW( 0x10 << 16, name, 64 );
    check( !wcscmp( name, L"A" ), "fr active: GetKeyNameText(scan 0x10) is A", name[0], 'A' );
    memset( name, 0, sizeof(name) );
    GetKeyNameTextW( 0x01 << 16, name, 64 );
    check( name[0] && name[0] != 0x1b, "fr active: GetKeyNameText(scan 0x01) is a name", name[0], 'E' );
    ActivateKeyboardLayout( us, 0 );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
