/* logpixelsenv-gate.sh's probe (0882): the display scale this process got
 * (aware of the DPI: the system DPI itself) and the stock GUI font's height.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    HDC dc;
    LOGFONTW lf;
    SetProcessDPIAware();
    dc = GetDC( NULL );
    GetObjectW( GetStockObject( DEFAULT_GUI_FONT ), sizeof(lf), &lf );
    printf( "dpi=%d screen=%dx%d\n", GetDeviceCaps( dc, LOGPIXELSY ), GetSystemMetrics( SM_CXSCREEN ),
            GetSystemMetrics( SM_CYSCREEN ) );
    ReleaseDC( NULL, dc );
    return 0;
}
