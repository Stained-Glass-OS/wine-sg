/* Probe for patches/sg/5048: D3DKMTOpenAdapterFromDeviceName opens the GPU a
 * GUID_DISPLAY_DEVICE_ARRIVAL interface path names, with that GPU's LUID
 * (DEVPROPKEY_GPU_LUID), and refuses GUID_DEVINTERFACE_DISPLAY_ADAPTER paths
 * and other names with STATUS_INVALID_PARAMETER (gdi32 driver tests, run on
 * Windows). It opened the first adapter for any name.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <setupapi.h>
#include <devpropdef.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef UINT D3DKMT_HANDLE;
typedef struct { const WCHAR *pDeviceName; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; } D3DKMT_OPENADAPTERFROMDEVICENAME;
typedef struct { D3DKMT_HANDLE hAdapter; } D3DKMT_CLOSEADAPTER;

static const GUID GUID_DEVINTERFACE_DISPLAY_ADAPTER_ = {0x5b45201d, 0xf2f2, 0x4f3b, {0x85, 0xbb, 0x30, 0xff, 0x1f, 0x95, 0x35, 0x99}};
static const GUID GUID_DISPLAY_DEVICE_ARRIVAL_ = {0x1ca05180, 0xa699, 0x450a, {0x9a, 0x0c, 0xde, 0x4f, 0xbe, 0x3d, 0xdd, 0x89}};
static const DEVPROPKEY DEVPROPKEY_GPU_LUID_ = {{0x60b193cb, 0x5276, 0x4d0f, {0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6}}, 2};
#define STATUS_INVALID_PARAMETER_ ((NTSTATUS)0xc000000d)

static NTSTATUS (WINAPI *pOpen)(D3DKMT_OPENADAPTERFROMDEVICENAME *);
static NTSTATUS (WINAPI *pClose)(const D3DKMT_CLOSEADAPTER *);

static unsigned int try_interfaces(const GUID *guid, const char *what, BOOL want_open)
{
    BYTE buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + 256 * sizeof(WCHAR)];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)buffer;
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
    SP_DEVINFO_DATA dev = {sizeof(dev)};
    HDEVINFO set = SetupDiGetClassDevsW(guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    unsigned int i;

    for (i = 0; SetupDiEnumDeviceInterfaces(set, NULL, guid, i, &iface); i++)
    {
        D3DKMT_OPENADAPTERFROMDEVICENAME desc = {0};
        DEVPROPTYPE type;
        LUID luid = {0};
        NTSTATUS st;

        detail->cbSize = sizeof(*detail);
        SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, sizeof(buffer), NULL, &dev);
        SetupDiGetDevicePropertyW(set, &dev, &DEVPROPKEY_GPU_LUID_, &type, (BYTE *)&luid, sizeof(luid), NULL, 0);
        desc.pDeviceName = detail->DevicePath;
        st = pOpen(&desc);
        if (want_open)
            check(!st && desc.hAdapter && !memcmp(&desc.AdapterLuid, &luid, sizeof(luid)), "%s %u: %#lx, luid %08lx:%08lx (device %08lx:%08lx)",
                  what, i, st, desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart, luid.HighPart, luid.LowPart);
        else
            check(st == STATUS_INVALID_PARAMETER_, "%s %u: %#lx", what, i, st);
        if (!st) { D3DKMT_CLOSEADAPTER c = {desc.hAdapter}; pClose(&c); }
    }
    SetupDiDestroyDeviceInfoList(set);
    return i;
}

int main(void)
{
    HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
    D3DKMT_OPENADAPTERFROMDEVICENAME desc = {L"\\\\?\\NOSUCH#DEVICE#0#{1ca05180-a699-450a-9a0c-de4fbe3ddd89}"};
    unsigned int n;

    pOpen = (void *)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromDeviceName");
    pClose = (void *)GetProcAddress(gdi32, "D3DKMTCloseAdapter");
    check(pOpen && pClose, "gdi32 exports");
    if (!pOpen) { printf("RESULT: FAIL\n"); return 1; }
    SendMessageW(GetDesktopWindow(), WM_NULL, 0, 0);

    n = try_interfaces(&GUID_DISPLAY_DEVICE_ARRIVAL_, "display device arrival", TRUE);
    check(n >= 1, "%u display device interfaces", n);
    try_interfaces(&GUID_DEVINTERFACE_DISPLAY_ADAPTER_, "display adapter", FALSE);
    check(pOpen(&desc) == STATUS_INVALID_PARAMETER_, "an unknown device");
    desc.pDeviceName = L"\\\\.\\DISPLAY1";
    check(pOpen(&desc) == STATUS_INVALID_PARAMETER_, "a display name");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
