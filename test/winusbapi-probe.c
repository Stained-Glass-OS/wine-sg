/* winusbapi-probe: the WinUSB API without a device (patches/sg/6519).
 *
 * Every WinUSB function but WinUsb_Free was a stub. Checks what needs no device: the
 * descriptor parsers on a configuration of the probe's own, the error answers for bad handles
 * and for a handle that is not a WinUSB device, and that the WinUSB driver and its INF (matching
 * USB\MS_COMP_WINUSB) are installed. The device paths are test-winusb's (a gadget on a VM).
 *
 * Prints name=value lines; see test/winusbapi-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <winusb.h>
#include <stdio.h>
#include <string.h>

static BOOL (WINAPI *pInitialize)( HANDLE, PWINUSB_INTERFACE_HANDLE );
static BOOL (WINAPI *pFree)( WINUSB_INTERFACE_HANDLE );
static BOOL (WINAPI *pQueryPipe)( WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR, PWINUSB_PIPE_INFORMATION );
static BOOL (WINAPI *pReadPipe)( WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED );
static PUSB_INTERFACE_DESCRIPTOR (WINAPI *pParseConfig)( PUSB_CONFIGURATION_DESCRIPTOR, PVOID, LONG, LONG, LONG, LONG, LONG );
static PUSB_COMMON_DESCRIPTOR (WINAPI *pParseDescriptors)( PVOID, ULONG, PVOID, LONG );

/* a configuration: interface 0 (vendor, two bulk endpoints) with an alternate setting 1
 * (an interrupt endpoint), interface 1 (class 0x0a, no endpoints), a class-specific descriptor */
static const BYTE config[] =
{
    9, 2, 9 + 9 + 7 + 7 + 9 + 7 + 9 + 5, 0, 2, 1, 0, 0x80, 50,
    9, 4, 0, 0, 2, 0xff, 0x00, 0x00, 0,
    7, 5, 0x81, 2, 0x40, 0, 0,
    7, 5, 0x02, 2, 0x40, 0, 0,
    9, 4, 0, 1, 1, 0xff, 0x01, 0x02, 0,
    7, 5, 0x83, 3, 0x08, 0, 10,
    9, 4, 1, 0, 0, 0x0a, 0x00, 0x00, 0,
    5, 0x24, 0x00, 0x10, 0x01,
};

int main( void )
{
    HMODULE module;
    WINUSB_INTERFACE_HANDLE handle = (void *)0xdeadbeef;
    USB_INTERFACE_DESCRIPTOR *iface;
    USB_COMMON_DESCRIPTOR *desc;
    WCHAR path[MAX_PATH], inf[MAX_PATH];
    HANDLE file;
    BOOL ret;
    DWORD error;
    char *data;
    DWORD size, read;

    setvbuf( stdout, NULL, _IONBF, 0 );
    module = LoadLibraryA( "winusb.dll" );
    pInitialize = (void *)GetProcAddress( module, "WinUsb_Initialize" );
    pFree = (void *)GetProcAddress( module, "WinUsb_Free" );
    pQueryPipe = (void *)GetProcAddress( module, "WinUsb_QueryPipe" );
    pReadPipe = (void *)GetProcAddress( module, "WinUsb_ReadPipe" );
    pParseConfig = (void *)GetProcAddress( module, "WinUsb_ParseConfigurationDescriptor" );
    pParseDescriptors = (void *)GetProcAddress( module, "WinUsb_ParseDescriptors" );
    printf( "exports=%d\n", pInitialize && pFree && pQueryPipe && pReadPipe && pParseConfig && pParseDescriptors );
    if (!pParseConfig) return 1;

    /* the parsers */
    iface = pParseConfig( (USB_CONFIGURATION_DESCRIPTOR *)config, (void *)config, 0, 1, -1, -1, -1 );
    printf( "parse_alternate=%d\n", iface && iface->bAlternateSetting == 1 && iface->bNumEndpoints == 1 && iface->bInterfaceSubClass == 1 );
    iface = pParseConfig( (USB_CONFIGURATION_DESCRIPTOR *)config, (void *)config, -1, -1, 0x0a, -1, -1 );
    printf( "parse_class=%d\n", iface && iface->bInterfaceNumber == 1 );
    iface = pParseConfig( (USB_CONFIGURATION_DESCRIPTOR *)config, (void *)config, 2, -1, -1, -1, -1 );
    printf( "parse_none=%d\n", !iface );
    desc = pParseDescriptors( (void *)config, sizeof(config), (void *)config, USB_ENDPOINT_DESCRIPTOR_TYPE );
    printf( "parse_endpoint=%d\n", desc && ((USB_ENDPOINT_DESCRIPTOR *)desc)->bEndpointAddress == 0x81 );
    desc = pParseDescriptors( (void *)config, sizeof(config), (BYTE *)desc + desc->bLength, USB_ENDPOINT_DESCRIPTOR_TYPE );
    printf( "parse_next=%d\n", desc && ((USB_ENDPOINT_DESCRIPTOR *)desc)->bEndpointAddress == 0x02 );
    desc = pParseDescriptors( (void *)config, sizeof(config), (void *)config, 0x24 );
    printf( "parse_class_specific=%d\n", desc && desc->bLength == 5 );
    desc = pParseDescriptors( (void *)config, sizeof(config) - 3, (BYTE *)config + sizeof(config) - 5, 0x24 );
    printf( "parse_truncated=%d\n", !desc );

    /* bad handles */
    SetLastError( 0xdeadbeef );
    ret = pInitialize( INVALID_HANDLE_VALUE, &handle );
    error = GetLastError();
    printf( "init_invalid=%d ret=%d error=%lu\n", !ret && error == ERROR_INVALID_HANDLE && !handle, ret, error );
    SetLastError( 0xdeadbeef );
    ret = pFree( NULL );
    printf( "free_null=%d error=%lu\n", !ret && GetLastError() == ERROR_INVALID_HANDLE, GetLastError() );
    {
        WINUSB_PIPE_INFORMATION info;
        SetLastError( 0xdeadbeef );
        ret = pQueryPipe( NULL, 0, 0, &info );
        printf( "query_null=%d error=%lu\n", !ret && GetLastError() == ERROR_INVALID_HANDLE, GetLastError() );
    }
    {
        char not_a_handle[64] = { 0 };
        ULONG got;
        SetLastError( 0xdeadbeef );
        ret = pReadPipe( (WINUSB_INTERFACE_HANDLE)not_a_handle, 0x81, (UCHAR *)path, 4, &got, NULL );
        printf( "read_bogus=%d error=%lu\n", !ret && GetLastError() == ERROR_INVALID_HANDLE, GetLastError() );
    }

    /* a file is not a WinUSB device */
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"winusbapi.txt" );
    file = CreateFileW( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL );
    handle = (void *)0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = pInitialize( file, &handle );
    printf( "init_file=%d ret=%d error=%lu\n", !ret && !handle && GetLastError() != 0xdeadbeef, ret, GetLastError() );
    CloseHandle( file );
    DeleteFileW( path );

    /* the driver and its INF */
    GetSystemDirectoryW( path, MAX_PATH );
    wcscat( path, L"\\drivers\\winusb.sys" );
    printf( "driver=%d\n", GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES );
    GetWindowsDirectoryW( inf, MAX_PATH );
    wcscat( inf, L"\\inf\\winusb.inf" );
    file = CreateFileW( inf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    size = file != INVALID_HANDLE_VALUE ? GetFileSize( file, NULL ) : 0;
    data = calloc( 1, size + 1 );
    if (file != INVALID_HANDLE_VALUE)
    {
        ReadFile( file, data, size, &read, NULL );
        CloseHandle( file );
    }
    printf( "inf=%d\n", strstr( data, "USB\\MS_COMP_WINUSB" ) && strstr( data, "winusb.sys" ) );
    printf( "done=1\n" );
    return 0;
}
