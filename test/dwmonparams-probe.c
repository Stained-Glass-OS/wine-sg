/* dwmonparams-probe: dwrite CreateMonitorRenderingParams follows the monitor's ClearType settings (Stained Glass
 * OS wine-sg 8285; test/dwmonparams-gate.sh): GammaLevel, ClearTypeLevel and PixelStructure under
 * Software\Microsoft\Avalon.Graphics\<display>, else the system's font smoothing; it ignored the monitor.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <dwrite.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, double v ) { printf( "%s  %s (%g)\n", ok ? "PASS" : "FAIL", what, v ); if (!ok) failures++; }

int main( void )
{
    HRESULT (WINAPI *pCreate)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **) = (void *)GetProcAddress( LoadLibraryA( "dwrite.dll" ), "DWriteCreateFactory" );
    IDWriteFactory *factory;
    IDWriteRenderingParams *params;
    HMONITOR mon = MonitorFromPoint( (POINT){ 0, 0 }, MONITOR_DEFAULTTOPRIMARY );
    MONITORINFOEXA info = { sizeof(info) };
    char key[200];
    HKEY lm, cu;
    DWORD v;

    GetMonitorInfoA( mon, (MONITORINFO *)&info );
    sprintf( key, "Software\\Microsoft\\Avalon.Graphics\\%s", info.szDevice + 4 );
    printf( "      monitor %s -> %s\n", info.szDevice, key );
    pCreate( DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory );

    RegCreateKeyA( HKEY_LOCAL_MACHINE, key, &lm );
    RegCreateKeyA( HKEY_CURRENT_USER, key, &cu );
    v = 1500; RegSetValueExA( lm, "GammaLevel", 0, REG_DWORD, (BYTE *)&v, 4 );
    v = 2;    RegSetValueExA( lm, "PixelStructure", 0, REG_DWORD, (BYTE *)&v, 4 );
    v = 50;   RegSetValueExA( cu, "ClearTypeLevel", 0, REG_DWORD, (BYTE *)&v, 4 );
    IDWriteFactory_CreateMonitorRenderingParams( factory, mon, &params );
    check( IDWriteRenderingParams_GetGamma( params ) == 1.5f, "GammaLevel 1500: gamma 1.5", IDWriteRenderingParams_GetGamma( params ) );
    check( IDWriteRenderingParams_GetClearTypeLevel( params ) == 0.5f, "ClearTypeLevel 50: 0.5", IDWriteRenderingParams_GetClearTypeLevel( params ) );
    check( IDWriteRenderingParams_GetPixelGeometry( params ) == DWRITE_PIXEL_GEOMETRY_BGR, "PixelStructure 2: BGR", IDWriteRenderingParams_GetPixelGeometry( params ) );
    IDWriteRenderingParams_Release( params );

    RegDeleteValueA( lm, "GammaLevel" ); RegDeleteValueA( lm, "PixelStructure" ); RegDeleteValueA( cu, "ClearTypeLevel" );
    SystemParametersInfoA( SPI_SETFONTSMOOTHING, TRUE, NULL, 0 );
    SystemParametersInfoA( SPI_SETFONTSMOOTHINGTYPE, 0, (void *)FE_FONTSMOOTHINGCLEARTYPE, 0 );
    SystemParametersInfoA( SPI_SETFONTSMOOTHINGORIENTATION, 0, (void *)FE_FONTSMOOTHINGORIENTATIONRGB, 0 );
    IDWriteFactory_CreateMonitorRenderingParams( factory, mon, &params );
    check( IDWriteRenderingParams_GetGamma( params ) == 1.9f, "no settings: the default gamma 1.9", IDWriteRenderingParams_GetGamma( params ) );
    check( IDWriteRenderingParams_GetClearTypeLevel( params ) == 1.0f, "  ClearType level 1.0", IDWriteRenderingParams_GetClearTypeLevel( params ) );
    check( IDWriteRenderingParams_GetPixelGeometry( params ) == DWRITE_PIXEL_GEOMETRY_RGB, "  ClearType smoothing, RGB: RGB", IDWriteRenderingParams_GetPixelGeometry( params ) );
    IDWriteRenderingParams_Release( params );
    SystemParametersInfoA( SPI_SETFONTSMOOTHINGTYPE, 0, (void *)FE_FONTSMOOTHINGSTANDARD, 0 );
    IDWriteFactory_CreateMonitorRenderingParams( factory, mon, &params );
    check( IDWriteRenderingParams_GetPixelGeometry( params ) == DWRITE_PIXEL_GEOMETRY_FLAT, "  standard smoothing: flat", IDWriteRenderingParams_GetPixelGeometry( params ) );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
