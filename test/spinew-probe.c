/* spinew-probe: SystemParametersInfo message duration, caret timeout and mouse wheel routing (Stained Glass
 * OS wine-sg 8292; test/spinew-gate.sh).  They were "Unknown action" (Qt and Firefox ask for them at start,
 * and windows.ui's UISettings.MessageDuration reads the first).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

#ifndef SPI_GETMESSAGEDURATION
#define SPI_GETMESSAGEDURATION 0x2016
#define SPI_SETMESSAGEDURATION 0x2017
#endif
#ifndef SPI_GETMOUSEWHEELROUTING
#define SPI_GETMOUSEWHEELROUTING 0x201C
#define SPI_SETMOUSEWHEELROUTING 0x201D
#endif
#ifndef SPI_GETCARETTIMEOUT
#define SPI_GETCARETTIMEOUT 0x2022
#define SPI_SETCARETTIMEOUT 0x2023
#endif

static int fails;
static void check( int ok, const char *what, DWORD v ) { printf( "%s  %s (%lu)\n", ok ? "PASS" : "FAIL", what, v ); if (!ok) fails++; }

static DWORD reg( const char *key, const char *name )
{
    DWORD v = 0xdead, size = sizeof(v);
    RegGetValueA( HKEY_CURRENT_USER, key, name, RRF_RT_REG_DWORD, NULL, &v, &size );
    return v;
}

int main( void )
{
    DWORD v;
    BOOL ok;

    v = 0; ok = SystemParametersInfoA( SPI_GETMESSAGEDURATION, 0, &v, 0 );
    check( ok && v == 5, "message duration: 5 seconds by default", v );
    ok = SystemParametersInfoA( SPI_SETMESSAGEDURATION, 0, (void *)(ULONG_PTR)17, SPIF_UPDATEINIFILE );
    v = 0; SystemParametersInfoA( SPI_GETMESSAGEDURATION, 0, &v, 0 );
    check( ok && v == 17, "  set to 17 (the value in pvParam) and read back", v );
    check( reg( "Control Panel\\Accessibility", "MessageDuration" ) == 17, "  kept as Control Panel\\Accessibility MessageDuration",
           reg( "Control Panel\\Accessibility", "MessageDuration" ) );

    v = 0; ok = SystemParametersInfoA( SPI_GETCARETTIMEOUT, 0, &v, 0 );
    check( ok && v == 5000, "caret timeout: 5000 ms by default", v );
    ok = SystemParametersInfoA( SPI_SETCARETTIMEOUT, 0, (void *)(ULONG_PTR)9000, SPIF_UPDATEINIFILE );
    v = 0; SystemParametersInfoA( SPI_GETCARETTIMEOUT, 0, &v, 0 );
    check( ok && v == 9000, "  set to 9000 and read back", v );

    v = 9; ok = SystemParametersInfoA( SPI_GETMOUSEWHEELROUTING, 0, &v, 0 );
    check( ok && v == 2, "wheel routing: to the window under the pointer (MOUSEWHEEL_ROUTING_MOUSE_POS)", v );
    ok = SystemParametersInfoA( SPI_SETMOUSEWHEELROUTING, 0, (void *)(ULONG_PTR)7, 0 );
    check( !ok, "  an unknown routing is refused", GetLastError() );
    ok = SystemParametersInfoA( SPI_GETMOUSEWHEELROUTING, 0, NULL, 0 );
    check( !ok, "  no buffer: fails", 0 );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
