/* A scanner's INF (patches/sg/0641, 0642): Include=/Needs= and the still
 * image class's directives.
 *
 *   stiinf-probe.exe <dir>
 * <dir>\probe.inf is a device INF the way scanner INFs are (the Ambir
 * ImageScan Pro 490i's): its section Includes sti.inf and Needs
 * STI.USBSection, its .Services section Needs STI.USBSection.Services (the
 * usbscan driver), and it has the class's SubClass/DeviceType/DeviceData/
 * Events lines. The probe installs it on a root-enumerated device and prints
 * what the driver key and the device got, then installs a plain section that
 * Needs another (an Include'd INF's) and prints its values. */
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>

static const GUID image_class = { 0x6bdd1fc6, 0x810f, 0x11d0, { 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f } };

static void print_sz( HKEY key, const WCHAR *sub, const WCHAR *name, const char *label )
{
    WCHAR buf[256];
    DWORD size = sizeof(buf);
    if (RegGetValueW( key, sub, name, RRF_RT_REG_SZ, NULL, buf, &size )) buf[0] = 0;
    printf( "%s=%ls\n", label, buf );
}

static void print_dword( HKEY key, const WCHAR *name, const char *label )
{
    DWORD value = 0xdeadbeef, size = sizeof(value);
    RegGetValueW( key, NULL, name, RRF_RT_REG_DWORD, NULL, &value, &size );
    printf( "%s=%#lx\n", label, value );
}

int wmain( int argc, WCHAR **argv )
{
    SP_DEVINFO_DATA dev = { sizeof(dev) };
    SP_DEVINSTALL_PARAMS_W params = { sizeof(params) };
    WCHAR inf[MAX_PATH], buf[256];
    static const WCHAR hwid[] = L"ROOT\\SGPROBESCANNER\0";
    HDEVINFO set;
    HINF hinf;
    HKEY key;
    DWORD size;

    if (argc < 2) return 2;
    swprintf( inf, ARRAYSIZE(inf), L"%ls\\probe.inf", argv[1] );

    set = SetupDiCreateDeviceInfoList( &image_class, NULL );
    if (!SetupDiCreateDeviceInfoW( set, L"SGPROBE", &image_class, NULL, NULL, DICD_GENERATE_ID, &dev ) ||
        !SetupDiSetDeviceRegistryPropertyW( set, &dev, SPDRP_HARDWAREID, (const BYTE *)hwid, sizeof(hwid) ) ||
        !SetupDiRegisterDeviceInfo( set, &dev, 0, NULL, NULL, NULL ))
    {
        printf( "create=%lu\n", GetLastError() );
        return 1;
    }
    SetupDiGetDeviceInstallParamsW( set, &dev, &params );
    lstrcpyW( params.DriverPath, inf );
    params.Flags |= DI_ENUMSINGLEINF;
    SetupDiSetDeviceInstallParamsW( set, &dev, &params );
    if (!SetupDiBuildDriverInfoList( set, &dev, SPDIT_COMPATDRIVER ) ||
        !SetupDiSelectBestCompatDrv( set, &dev ))
    {
        printf( "driver=%lu\n", GetLastError() );
        return 1;
    }
    printf( "install=%d\n", SetupDiInstallDevice( set, &dev ) );

    key = SetupDiOpenDevRegKey( set, &dev, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ );
    if (key == INVALID_HANDLE_VALUE) printf( "drvkey=%lu\n", GetLastError() );
    else
    {
        print_sz( key, NULL, L"SubClass", "subclass" );
        print_dword( key, L"DeviceType", "devicetype" );
        print_dword( key, L"DeviceSubType", "devicesubtype" );
        print_dword( key, L"Capabilities", "capabilities" );
        print_sz( key, L"DeviceData", L"TwainDS", "twainds" );
        print_sz( key, L"Events\\ScanButton", NULL, "event" );
        print_sz( key, L"Events\\ScanButton", L"GUID", "eventguid" );
        print_sz( key, NULL, L"FromInclude", "fromneeds" );
        RegCloseKey( key );
    }
    size = 0;
    buf[0] = 0;
    SetupDiGetDeviceRegistryPropertyW( set, &dev, SPDRP_SERVICE, NULL, (BYTE *)buf, sizeof(buf), &size );
    printf( "service=%ls\n", buf );
    SetupDiCallClassInstaller( DIF_REMOVE, set, &dev );
    SetupDiDestroyDeviceInfoList( set );

    /* a plain section's Needs, of an Include'd INF's section */
    hinf = SetupOpenInfFileW( inf, NULL, INF_STYLE_WIN4, NULL );
    printf( "plain=%d\n", SetupInstallFromInfSectionW( NULL, hinf, L"Plain", SPINST_REGISTRY, NULL, NULL, 0,
                                                       NULL, NULL, NULL, NULL ) );
    SetupCloseInfFile( hinf );
    print_sz( HKEY_CURRENT_USER, L"Software\\SgStiProbe", L"Own", "plainown" );
    print_sz( HKEY_CURRENT_USER, L"Software\\SgStiProbe", L"Needed", "plainneeded" );
    return 0;
}
