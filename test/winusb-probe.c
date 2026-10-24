/* winusb-probe: a program talks to a USB device through WinUSB (patches/sg/6519-6520).
 *
 * The device is test/usbgadget-setup.sh's gadget (1209:0001, Microsoft OS descriptors naming
 * WinUSB and the interface GUID below; test/usbgadget-ffs.py serves it). As a program does: the
 * device interface found with SetupDi, the device opened, WinUSB initialized, the interface and
 * its pipes queried, descriptors read, vendor control transfers both ways, bulk writes and reads
 * (echo, a known pattern, overlapped), a pipe timeout, a pipe abort and reset.
 *
 * Prints name=value lines; see test/winusb-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <initguid.h>
#include <setupapi.h>
#include <winusb.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(gadget_interface, 0x5347a11c, 0x0b5b, 0x4f3a, 0x9c, 0x2e, 0x6d, 0x1e, 0x5b, 0x0f, 0x57, 0xa1);

#define F(name, ...) static BOOL (WINAPI *p##name)(__VA_ARGS__)
F(Initialize, HANDLE, PWINUSB_INTERFACE_HANDLE);
F(Free, WINUSB_INTERFACE_HANDLE);
F(QueryInterfaceSettings, WINUSB_INTERFACE_HANDLE, UCHAR, PUSB_INTERFACE_DESCRIPTOR);
F(QueryPipe, WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR, PWINUSB_PIPE_INFORMATION);
F(QueryDeviceInformation, WINUSB_INTERFACE_HANDLE, ULONG, PULONG, PVOID);
F(GetDescriptor, WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR, USHORT, PUCHAR, ULONG, PULONG);
F(ControlTransfer, WINUSB_INTERFACE_HANDLE, WINUSB_SETUP_PACKET, PUCHAR, ULONG, PULONG, LPOVERLAPPED);
F(ReadPipe, WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED);
F(WritePipe, WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED);
F(SetPipePolicy, WINUSB_INTERFACE_HANDLE, UCHAR, ULONG, ULONG, PVOID);
F(ResetPipe, WINUSB_INTERFACE_HANDLE, UCHAR);
F(AbortPipe, WINUSB_INTERFACE_HANDLE, UCHAR);
F(GetOverlappedResult, WINUSB_INTERFACE_HANDLE, LPOVERLAPPED, LPDWORD, BOOL);
F(GetAssociatedInterface, WINUSB_INTERFACE_HANDLE, UCHAR, PWINUSB_INTERFACE_HANDLE);

static BOOL find_device( WCHAR *path )
{
    SP_DEVICE_INTERFACE_DATA data = { sizeof(data) };
    BYTE buffer[1024];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)buffer;
    HDEVINFO set;
    BOOL found = FALSE;

    set = SetupDiGetClassDevsW( &gadget_interface, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE );
    if (set == INVALID_HANDLE_VALUE) return FALSE;
    if (SetupDiEnumDeviceInterfaces( set, NULL, &gadget_interface, 0, &data ))
    {
        detail->cbSize = sizeof(*detail);
        if (SetupDiGetDeviceInterfaceDetailW( set, &data, detail, sizeof(buffer), NULL, NULL ))
        {
            lstrcpyW( path, detail->DevicePath );
            found = TRUE;
        }
    }
    SetupDiDestroyDeviceInfoList( set );
    return found;
}

static BOOL vendor_request( WINUSB_INTERFACE_HANDLE handle, BOOL in, UCHAR request, BYTE *data, USHORT length, ULONG *done )
{
    WINUSB_SETUP_PACKET setup = { in ? 0xc1 : 0x41, request, 0, 0, length };   /* vendor, interface 0 */
    return pControlTransfer( handle, setup, data, length, done, NULL );
}

int main( void )
{
    WCHAR path[512];
    HANDLE file;
    WINUSB_INTERFACE_HANDLE handle = NULL, other;
    USB_INTERFACE_DESCRIPTOR iface;
    WINUSB_PIPE_INFORMATION pipe;
    UCHAR in_pipe = 0, out_pipe = 0, speed = 0;
    BYTE buffer[4096];
    ULONG done, length;
    int i;
    BOOL ok;

    setvbuf( stdout, NULL, _IONBF, 0 );
    {
        HMODULE m = LoadLibraryA( "winusb.dll" );
#define L(name) p##name = (void *)GetProcAddress( m, "WinUsb_" #name )
        L(Initialize); L(Free); L(QueryInterfaceSettings); L(QueryPipe); L(QueryDeviceInformation); L(GetDescriptor);
        L(ControlTransfer); L(ReadPipe); L(WritePipe); L(SetPipePolicy); L(ResetPipe); L(AbortPipe);
        L(GetOverlappedResult); L(GetAssociatedInterface);
    }

    /* the device appears once wineusb has seen it and its driver started */
    for (i = 0; i < 300 && !find_device( path ); i++) Sleep( 100 );
    printf( "found=%d path=%ls\n", i < 300, i < 300 ? path : L"" );
    if (i >= 300) return 1;

    file = CreateFileW( path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL );
    printf( "open=%d error=%lu\n", file != INVALID_HANDLE_VALUE, file != INVALID_HANDLE_VALUE ? 0 : GetLastError() );
    if (file == INVALID_HANDLE_VALUE) return 1;
    ok = pInitialize( file, &handle );
    printf( "initialize=%d error=%lu\n", ok, ok ? 0 : GetLastError() );
    if (!ok) return 1;

    length = 1;
    ok = pQueryDeviceInformation( handle, DEVICE_SPEED, &length, &speed );
    printf( "speed=%d speed=%u\n", ok && speed >= FullSpeed, speed );
    ok = pQueryInterfaceSettings( handle, 0, &iface );
    printf( "interface=%d class=%#x endpoints=%u\n", ok && iface.bInterfaceClass == 0xff && iface.bNumEndpoints == 2,
            ok ? iface.bInterfaceClass : 0, ok ? iface.bNumEndpoints : 0 );
    for (i = 0; i < 2 && pQueryPipe( handle, 0, i, &pipe ); i++)
    {
        if (pipe.PipeType == UsbdPipeTypeBulk && (pipe.PipeId & 0x80)) in_pipe = pipe.PipeId;
        if (pipe.PipeType == UsbdPipeTypeBulk && !(pipe.PipeId & 0x80)) out_pipe = pipe.PipeId;
    }
    printf( "pipes=%d in=%#x out=%#x\n", in_pipe == 0x81 && out_pipe == 0x02, in_pipe, out_pipe );
    ok = pGetAssociatedInterface( handle, 0, &other );
    printf( "no_associated=%d error=%lu\n", !ok && GetLastError() == ERROR_NO_MORE_ITEMS, GetLastError() );

    /* descriptors */
    ok = pGetDescriptor( handle, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, buffer, sizeof(USB_DEVICE_DESCRIPTOR), &done );
    printf( "device_descriptor=%d vid=%04x pid=%04x\n", ok && done == 18 && *(USHORT *)(buffer + 8) == 0x1209 &&
            *(USHORT *)(buffer + 10) == 0x0001, *(USHORT *)(buffer + 8), *(USHORT *)(buffer + 10) );
    ok = pGetDescriptor( handle, USB_STRING_DESCRIPTOR_TYPE, 2, 0x0409, buffer, 255, &done );
    printf( "string_descriptor=%d bytes=%lu text=%.18ls\n", ok && done > 2 && !memcmp( buffer + 2, L"WinUSB test gadget", 36 ),
            done, (WCHAR *)(buffer + 2) );

    /* control transfers */
    memset( buffer, 0, sizeof(buffer) );
    ok = vendor_request( handle, TRUE, 0x01, buffer, 8, &done );
    printf( "control_in=%d bytes=%lu data=%.8s\n", ok && done == 8 && !memcmp( buffer, "SGWINUSB", 8 ), done, buffer );
    ok = vendor_request( handle, FALSE, 0x02, (BYTE *)"stained glass", 13, &done );
    printf( "control_out=%d bytes=%lu\n", ok && done == 13, done );
    memset( buffer, 0, sizeof(buffer) );
    ok = vendor_request( handle, TRUE, 0x03, buffer, 13, &done );
    printf( "control_roundtrip=%d data=%.13s\n", ok && done == 13 && !memcmp( buffer, "stained glass", 13 ), buffer );
    ok = vendor_request( handle, TRUE, 0x7f, buffer, 8, &done );
    printf( "control_stall=%d error=%lu\n", !ok, ok ? 0 : GetLastError() );

    /* bulk */
    ok = pWritePipe( handle, out_pipe, (BYTE *)"hello, gadget", 13, &done, NULL );
    printf( "write=%d bytes=%lu\n", ok && done == 13, done );
    memset( buffer, 0, sizeof(buffer) );
    ok = pReadPipe( handle, in_pipe, buffer, sizeof(buffer), &done, NULL );
    printf( "echo=%d bytes=%lu data=%.13s\n", ok && done == 13 && !memcmp( buffer, "hello, gadget", 13 ), done, buffer );
    ok = pWritePipe( handle, out_pipe, (BYTE *)"PATTERN", 7, &done, NULL );
    memset( buffer, 0, sizeof(buffer) );
    ok = ok && pReadPipe( handle, in_pipe, buffer, sizeof(buffer), &done, NULL );
    for (i = 0; ok && i < 1024; i++) ok = buffer[i] == i % 251;
    printf( "pattern=%d bytes=%lu\n", ok && done == 1024, done );
    {
        OVERLAPPED ov = { 0 };
        DWORD got = 0;
        ov.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
        memset( buffer, 0, sizeof(buffer) );
        ok = pReadPipe( handle, in_pipe, buffer, sizeof(buffer), NULL, &ov );
        ok = ok || GetLastError() == ERROR_IO_PENDING;
        pWritePipe( handle, out_pipe, (BYTE *)"overlapped", 10, &done, NULL );
        ok = ok && pGetOverlappedResult( handle, &ov, &got, TRUE );
        printf( "overlapped=%d bytes=%lu\n", ok && got == 10 && !memcmp( buffer, "overlapped", 10 ), got );
        CloseHandle( ov.hEvent );
    }
    {
        ULONG timeout = 300;
        DWORD start;
        pSetPipePolicy( handle, in_pipe, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout );
        start = GetTickCount();
        ok = pReadPipe( handle, in_pipe, buffer, sizeof(buffer), &done, NULL );
        printf( "timeout=%d error=%lu ms=%lu\n", !ok && GetLastError() == ERROR_SEM_TIMEOUT && GetTickCount() - start < 3000,
                ok ? 0 : GetLastError(), GetTickCount() - start );
        timeout = 0;
        pSetPipePolicy( handle, in_pipe, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout );
    }
    {
        OVERLAPPED ov = { 0 };
        DWORD got = 0;
        ov.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
        pReadPipe( handle, in_pipe, buffer, sizeof(buffer), NULL, &ov );
        Sleep( 100 );
        ok = pAbortPipe( handle, in_pipe );
        ok = ok && WaitForSingleObject( ov.hEvent, 3000 ) == WAIT_OBJECT_0 && !pGetOverlappedResult( handle, &ov, &got, FALSE );
        printf( "abort=%d\n", ok );
        CloseHandle( ov.hEvent );
    }
    printf( "reset=%d\n", pResetPipe( handle, in_pipe ) );
    /* still working after all that */
    ok = pWritePipe( handle, out_pipe, (BYTE *)"again", 5, &done, NULL ) &&
         pReadPipe( handle, in_pipe, buffer, sizeof(buffer), &done, NULL ) && done == 5 && !memcmp( buffer, "again", 5 );
    printf( "after_reset=%d\n", ok );
    printf( "free=%d\n", pFree( handle ) );
    CloseHandle( file );
    printf( "done=1\n" );
    return 0;
}
