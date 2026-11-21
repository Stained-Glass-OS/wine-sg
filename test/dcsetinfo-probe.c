/* dcsetinfo-probe: user32 DisplayConfigSetDeviceInfo, which was a stub (Stained Glass OS wine-sg 8253;
 * test/dcsetinfo-gate.sh): target persistence accepted, the virtual resolution switch kept and read back,
 * advanced colour off accepted and on refused (SDR displays), a bad target refused.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%ld)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

typedef struct { DISPLAYCONFIG_DEVICE_INFO_HEADER header; UINT32 value; } PACKET;

int main( void )
{
    DISPLAYCONFIG_PATH_INFO paths[8];
    DISPLAYCONFIG_MODE_INFO modes[16];
    UINT32 npaths = 8, nmodes = 16;
    PACKET p;
    LONG r;

    r = QueryDisplayConfig( QDC_ONLY_ACTIVE_PATHS, &npaths, paths, &nmodes, modes, NULL );
    if (r || !npaths) { printf( "FAIL  QueryDisplayConfig %ld\nRESULT: FAIL\n", r ); return 1; }
#define INIT(t) do { memset( &p, 0, sizeof(p) ); p.header.type = (t); p.header.size = sizeof(p); \
        p.header.adapterId = paths[0].targetInfo.adapterId; p.header.id = paths[0].targetInfo.id; } while (0)

    INIT( DISPLAYCONFIG_DEVICE_INFO_SET_TARGET_PERSISTENCE ); p.value = 1;
    r = DisplayConfigSetDeviceInfo( &p.header );
    check( r == ERROR_SUCCESS, "SET_TARGET_PERSISTENCE", r );

    INIT( DISPLAYCONFIG_DEVICE_INFO_GET_SUPPORT_VIRTUAL_RESOLUTION );
    r = DisplayConfigGetDeviceInfo( &p.header );
    check( r == ERROR_SUCCESS && p.value == 0, "GET_SUPPORT_VIRTUAL_RESOLUTION: not disabled at first", r );
    INIT( DISPLAYCONFIG_DEVICE_INFO_SET_SUPPORT_VIRTUAL_RESOLUTION ); p.value = 1;
    r = DisplayConfigSetDeviceInfo( &p.header );
    check( r == ERROR_SUCCESS, "SET_SUPPORT_VIRTUAL_RESOLUTION disable", r );
    INIT( DISPLAYCONFIG_DEVICE_INFO_GET_SUPPORT_VIRTUAL_RESOLUTION ); p.value = 0xcc;
    r = DisplayConfigGetDeviceInfo( &p.header );
    check( r == ERROR_SUCCESS && p.value == 1, "  read back disabled", p.value );
    INIT( DISPLAYCONFIG_DEVICE_INFO_SET_SUPPORT_VIRTUAL_RESOLUTION ); p.value = 0;
    DisplayConfigSetDeviceInfo( &p.header );
    INIT( DISPLAYCONFIG_DEVICE_INFO_GET_SUPPORT_VIRTUAL_RESOLUTION ); p.value = 0xcc;
    r = DisplayConfigGetDeviceInfo( &p.header );
    check( r == ERROR_SUCCESS && p.value == 0, "  and enabled again", p.value );

    INIT( DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE ); p.value = 0;
    check( DisplayConfigSetDeviceInfo( &p.header ) == ERROR_SUCCESS, "advanced colour off: accepted", 0 );
    INIT( DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE ); p.value = 1;
    r = DisplayConfigSetDeviceInfo( &p.header );
    check( r == ERROR_NOT_SUPPORTED, "advanced colour on: ERROR_NOT_SUPPORTED (SDR)", r );

    INIT( DISPLAYCONFIG_DEVICE_INFO_SET_TARGET_PERSISTENCE ); p.header.id = 0xdead;
    r = DisplayConfigSetDeviceInfo( &p.header );
    check( r == ERROR_INVALID_PARAMETER, "a target that does not exist: ERROR_INVALID_PARAMETER", r );
    INIT( DISPLAYCONFIG_DEVICE_INFO_SET_TARGET_PERSISTENCE ); p.header.size = sizeof(p.header);
    check( DisplayConfigSetDeviceInfo( &p.header ) == ERROR_INVALID_PARAMETER, "a short packet: ERROR_INVALID_PARAMETER", 0 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
