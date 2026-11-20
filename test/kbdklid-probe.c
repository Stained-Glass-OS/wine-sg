/* Layout-id keyboard layouts (KLIDs such as 00010409, United States-Dvorak) load as their
 * 0xfXXX HKLs and the key functions follow their XKB variant (patches/sg/8212). Before, there was
 * no keyboard layout catalogue: LoadKeyboardLayout("00010409") gave plain US (0x04090409), and a
 * 0xfXXX HKL always answered with the X keyboard map. */
#include <windows.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  "); } else printf("ok    "); \
                           printf(__VA_ARGS__); printf("\n"); } while (0)

static WCHAR char_of_scan( HKL hkl, UINT scan )
{
    BYTE state[256] = {0};
    WCHAR buf[8] = {0};
    UINT vk = MapVirtualKeyExW( scan, MAPVK_VSC_TO_VK, hkl );
    int ret = ToUnicodeEx( vk, scan, state, buf, 8, 0, hkl );
    return ret == 1 ? buf[0] : (ret < 0 ? 0xffff : 0);
}

int main(void)
{
    HKL list[64], dv, intl, unknown = (HKL)(ULONG_PTR)0xf0ee0409;
    WCHAR name[KL_NAMELENGTH];
    UINT count, i;
    SHORT vk;
    WCHAR c;
    BYTE state[256] = {0};
    WCHAR buf[8];

    /* nothing loaded: the catalogue is not the loaded list */
    count = GetKeyboardLayoutList( 64, list );
    for (i = 0; i < count; i++) printf( "      loaded %p\n", list[i] );
    CHECK( count == 1, "GetKeyboardLayoutList with nothing loaded: %u layout(s)", count );

    dv = LoadKeyboardLayoutW( L"00010409", 0 );
    CHECK( (ULONG_PTR)dv == 0xf0020409, "LoadKeyboardLayout 00010409 (US-Dvorak): %p (want f0020409)", dv );
    intl = LoadKeyboardLayoutW( L"00020409", 0 );
    CHECK( (ULONG_PTR)intl == 0xf0010409, "LoadKeyboardLayout 00020409 (US-International): %p (want f0010409)", intl );

    if (ActivateKeyboardLayout( dv, 0 ) && GetKeyboardLayoutNameW( name ))
        CHECK( !lstrcmpiW( name, L"00010409" ), "GetKeyboardLayoutName with Dvorak active: %ls", name );
    else CHECK( 0, "activate Dvorak: %lu", GetLastError() );

    /* Dvorak: the Q key (scan 0x10) types an apostrophe, q is on the X key (scan 0x2d) */
    c = char_of_scan( dv, 0x10 );
    CHECK( c == '\'', "Dvorak: scan 0x10 types %#x (want ')", c );
    c = char_of_scan( dv, 0x13 );
    CHECK( c == 'p', "Dvorak: scan 0x13 types %#x (want p)", c );
    CHECK( MapVirtualKeyExW( 0x10, MAPVK_VSC_TO_VK, dv ) == VK_OEM_7, "Dvorak: scan 0x10 is VK %#x (want VK_OEM_7, as kbddv)",
           MapVirtualKeyExW( 0x10, MAPVK_VSC_TO_VK, dv ) );
    vk = VkKeyScanExW( 'q', dv );
    CHECK( vk != -1 && MapVirtualKeyExW( LOBYTE(vk), MAPVK_VK_TO_VSC, dv ) == 0x2d,
           "Dvorak: q is VK %#x on scan %#x (want 0x2d)", vk, MapVirtualKeyExW( LOBYTE(vk), MAPVK_VK_TO_VSC, dv ) );

    /* US-International: the apostrophe key is a dead key */
    memset( buf, 0, sizeof(buf) );
    i = ToUnicodeEx( MapVirtualKeyExW( 0x28, MAPVK_VSC_TO_VK, intl ), 0x28, state, buf, 8, 0, intl );
    CHECK( (int)i == -1, "US-International: scan 0x28 is a dead key (ToUnicodeEx %d, %#x)", (int)i, buf[0] );
    ToUnicodeEx( VK_SPACE, 0x39, state, buf, 8, 0, intl ); /* end the dead key */

    /* a layout id nobody has: the X map (US) answers */
    c = char_of_scan( unknown, 0x10 );
    CHECK( c == 'q', "unknown layout id: scan 0x10 types %#x (want q, the X map)", c );

    ActivateKeyboardLayout( (HKL)(ULONG_PTR)0x04090409, 0 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
