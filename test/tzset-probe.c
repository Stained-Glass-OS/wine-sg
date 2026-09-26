/* tzset-probe: a Windows program sets the machine's time zone and clock
 * through sg-admind (patches/sg/0221). Prints name=result,error lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    DYNAMIC_TIME_ZONE_INFORMATION dyn = { 0 };
    TIME_ZONE_INFORMATION tzi = { 0 };
    SYSTEMTIME st = { 0 };
    BOOL ret;

    lstrcpyW( dyn.TimeZoneKeyName, L"Pacific Standard Time" );
    SetLastError( 0 ); ret = SetDynamicTimeZoneInformation( &dyn );
    printf( "dynamic=%d,%lu\n", ret, ret ? 0 : GetLastError() );

    lstrcpyW( tzi.StandardName, L"Tokyo Standard Time" );
    SetLastError( 0 ); ret = SetTimeZoneInformation( &tzi );
    printf( "standard=%d,%lu\n", ret, ret ? 0 : GetLastError() );

    lstrcpyW( dyn.TimeZoneKeyName, L"No Such Standard Time" );
    SetLastError( 0 ); ret = SetDynamicTimeZoneInformation( &dyn );
    printf( "unknown=%d,%lu\n", ret, ret ? 0 : GetLastError() );

    st.wYear = 2031; st.wMonth = 5; st.wDay = 6; st.wHour = 7; st.wMinute = 8; st.wSecond = 9;
    SetLastError( 0 ); ret = SetLocalTime( &st );
    printf( "local=%d,%lu\n", ret, ret ? 0 : GetLastError() );
    return 0;
}
