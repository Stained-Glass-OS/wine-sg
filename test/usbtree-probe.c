/* The USB tree as scanner software walks it (patches/sg/0643): \\.\HCD0, its
 * root hub, the hub's ports and what is connected to them. The Ambir
 * ImageScan Pro 490i's library finds its scanner so before it opens
 * \\.\UsbscanN (patches/sg/0642). Prints key=value lines. */
#include <windows.h>
#include <stdio.h>

#define USB_IOCTL(n) (0x220000 | ((n) << 2))   /* FILE_DEVICE_USB, METHOD_BUFFERED */
#define IOCTL_USB_GET_ROOT_HUB_NAME                     USB_IOCTL(258)
#define IOCTL_USB_GET_NODE_INFORMATION                  USB_IOCTL(258)
#define IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION   USB_IOCTL(260)
#define IOCTL_GET_HCD_DRIVERKEY_NAME                    USB_IOCTL(265)
#define IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX    USB_IOCTL(274)

#pragma pack(push,1)
struct connection_ex    /* USB_NODE_CONNECTION_INFORMATION_EX */
{
    ULONG index;
    BYTE device_desc[18];
    BYTE config, speed, is_hub;
    USHORT address;
    ULONG pipes, status;
    BYTE pipe_info[32 * 7];
};
struct descriptor_request   /* USB_DESCRIPTOR_REQUEST */
{
    ULONG port;
    BYTE type, request;
    USHORT value, index, length;
    BYTE data[64];
};
#pragma pack(pop)

int main( void )
{
    BYTE buf[600];
    char name[300];
    DWORD got;
    HANDLE hcd, hub;
    ULONG i, ports, connected = 0, ok = 1, desc_ok = 0;

    printf( "usbscan=%d\n", GetFileAttributesA( "C:\\windows\\system32\\drivers\\usbscan.sys" ) != INVALID_FILE_ATTRIBUTES );
    hcd = CreateFileA( "\\\\.\\HCD0", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    printf( "hcd=%d\n", hcd != INVALID_HANDLE_VALUE );
    if (hcd == INVALID_HANDLE_VALUE) return 1;

    memset( buf, 0, sizeof(buf) );
    printf( "driverkey=%d\n", DeviceIoControl( hcd, IOCTL_GET_HCD_DRIVERKEY_NAME, buf, sizeof(buf), buf, sizeof(buf), &got, NULL )
            && got > 6 && ((WCHAR *)(buf + 4))[0] );
    memset( buf, 0, sizeof(buf) );
    if (!DeviceIoControl( hcd, IOCTL_USB_GET_ROOT_HUB_NAME, buf, sizeof(buf), buf, sizeof(buf), &got, NULL )) return 1;
    printf( "roothub=%ls\n", (WCHAR *)(buf + 4) );
    snprintf( name, sizeof(name), "\\\\.\\%ls", (WCHAR *)(buf + 4) );
    hub = CreateFileA( name, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    printf( "hubopen=%d\n", hub != INVALID_HANDLE_VALUE );
    if (hub == INVALID_HANDLE_VALUE) return 1;

    memset( buf, 0, sizeof(buf) );
    if (!DeviceIoControl( hub, IOCTL_USB_GET_NODE_INFORMATION, buf, 76, buf, 76, &got, NULL )) return 1;
    ports = buf[6];   /* USB_NODE_INFORMATION: NodeType, then the hub descriptor's bNumberOfPorts */
    printf( "ports=%lu\n", ports );
    for (i = 1; i <= ports; i++)
    {
        struct connection_ex c = { i };
        struct descriptor_request d = { i, 0, 0, 0x0100, 0, 18 };

        if (!DeviceIoControl( hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, &c, sizeof(c), &c, sizeof(c), &got, NULL ))
        {
            ok = 0;
            continue;
        }
        if (c.status != 1) continue;  /* DeviceConnected */
        connected++;
        if (DeviceIoControl( hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, &d, sizeof(d), &d, sizeof(d), &got, NULL )
            && d.data[0] == 18 && d.data[1] == 1 && !memcmp( d.data, c.device_desc, 18 ))
            desc_ok++;
    }
    printf( "portsok=%lu\nconnected=%lu\ndescok=%lu\n", ok, connected, desc_ok );
    return 0;
}
