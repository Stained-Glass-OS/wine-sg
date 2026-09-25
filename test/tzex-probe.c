/* tzex-probe: the dynamic time zone conversions (patches/sg/0172).
 *
 * SystemTimeToTzSpecificLocalTimeEx and TzSpecificLocalTimeToSystemTimeEx
 * convert with the rules a zone had in the year of the date (its registry
 * key's "Dynamic DST" table), where the non-Ex functions apply one fixed set
 * of rules. US Pacific time is the test: until 2006 daylight time began on
 * the first Sunday in April, since 2007 on the second Sunday in March -- so
 * 20 March 2006 12:00 UTC was 04:00 PST, not 05:00 PDT.
 *
 * Functions are looked up with GetProcAddress, so a Wine without them runs.
 * Prints name=value lines; see test/tzex-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

typedef BOOL (WINAPI *conv_fn)( const DYNAMIC_TIME_ZONE_INFORMATION *, const SYSTEMTIME *, SYSTEMTIME * );
typedef BOOL (WINAPI *set_fn)( const DYNAMIC_TIME_ZONE_INFORMATION * );

static conv_fn to_local, to_utc;

static SYSTEMTIME st( int y, int mo, int d, int h, int mi )
{
    SYSTEMTIME s = { 0 };
    s.wYear = y; s.wMonth = mo; s.wDay = d; s.wHour = h; s.wMinute = mi;
    return s;
}

static void show( const char *name, BOOL ok, const SYSTEMTIME *t )
{
    if (!ok) printf( "%s=failed(%lu)\n", name, GetLastError() );
    else printf( "%s=%04u-%02u-%02u %02u:%02u\n", name, t->wYear, t->wMonth, t->wDay, t->wHour, t->wMinute );
}

int main( void )
{
    HMODULE k32 = GetModuleHandleA( "kernel32.dll" );
    DYNAMIC_TIME_ZONE_INFORMATION pst = { 0 }, cur, custom = { 0 };
    SYSTEMTIME in, out, out2;
    TIME_ZONE_INFORMATION tzi;
    set_fn set_dyn;

    to_local = (conv_fn)GetProcAddress( k32, "SystemTimeToTzSpecificLocalTimeEx" );
    to_utc = (conv_fn)GetProcAddress( k32, "TzSpecificLocalTimeToSystemTimeEx" );
    set_dyn = (set_fn)GetProcAddress( k32, "SetDynamicTimeZoneInformation" );
    printf( "exports=%d,%d,%d\n", !!to_local, !!to_utc, !!set_dyn );
    if (!to_local || !to_utc) return 0;

    /* the zone as a dynamic zone: its key name; its own fields are not used */
    lstrcpyW( pst.TimeZoneKeyName, L"Pacific Standard Time" );
    pst.Bias = 480;

    in = st( 2006, 3, 20, 12, 0 );
    show( "pst_2006_march", to_local( &pst, &in, &out ), &out );          /* 04:00: 2006's rules */
    in = st( 2008, 3, 20, 12, 0 );
    show( "pst_2008_march", to_local( &pst, &in, &out ), &out );          /* 05:00: since 2007 */
    in = st( 2006, 7, 1, 12, 0 );
    show( "pst_2006_july", to_local( &pst, &in, &out ), &out );           /* 05:00 */
    in = st( 1995, 3, 20, 12, 0 );
    show( "pst_1995_march", to_local( &pst, &in, &out ), &out );          /* before the table: its first year's rules */
    in = st( 2030, 3, 20, 12, 0 );
    show( "pst_2030_march", to_local( &pst, &in, &out ), &out );          /* after it: its last year's */

    in = st( 2006, 3, 20, 4, 0 );
    show( "pst_2006_back", to_utc( &pst, &in, &out ), &out );             /* 12:00 UTC */
    in = st( 2008, 3, 20, 5, 0 );
    show( "pst_2008_back", to_utc( &pst, &in, &out ), &out );             /* 12:00 UTC */

    /* dynamic rules switched off: the zone's standing rules for every year */
    pst.DynamicDaylightTimeDisabled = TRUE;
    in = st( 2006, 3, 20, 12, 0 );
    show( "pst_2006_disabled", to_local( &pst, &in, &out ), &out );       /* 05:00 */
    pst.DynamicDaylightTimeDisabled = FALSE;

    /* no key: the rules it carries (UTC+1, no daylight time) */
    custom.Bias = -60;
    in = st( 2006, 3, 20, 12, 0 );
    show( "custom", to_local( &custom, &in, &out ), &out );               /* 13:00 */

    /* no zone at all: the current one, as GetDynamicTimeZoneInformation says */
    GetDynamicTimeZoneInformation( &cur );
    in = st( 2008, 3, 20, 12, 0 );
    {
        BOOL a = to_local( NULL, &in, &out ), b = to_local( &cur, &in, &out2 );
        printf( "null_is_current=%d\n", a && b && !memcmp( &out, &out2, sizeof(out) ) );
    }

    /* the non-Ex function keeps one set of rules (today's) */
    GetTimeZoneInformationForYear( 2008, &pst, &tzi );
    in = st( 2006, 3, 20, 12, 0 );
    show( "fixed_2006_march", SystemTimeToTzSpecificLocalTime( &tzi, &in, &out ), &out ); /* 05:00 */

    /* bad arguments */
    SetLastError( 0xdeadbeef );
    {
        BOOL ret = to_local( &pst, NULL, &out );
        printf( "null_args=%d,%lu\n", ret, GetLastError() );
    }
    return 0;
}
