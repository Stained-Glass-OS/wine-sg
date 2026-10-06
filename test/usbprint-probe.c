/* usbprint-probe: what a USB label printer's software asks of Windows, for
 * test/usbprint-gate.sh (patches/sg/0874-0878). Modes:
 *
 *   usb          the printer through usbprint.sys, as DYMO Connect walks it:
 *                GUID_DEVINTERFACE_USBPRINT, the IEEE 1284 ID and port
 *                status ioctls, ESC A 0 written on a write handle and the
 *                status read on a read handle (both overlapped), the USB
 *                device's hardware IDs (CM_Get_DevNode_Registry_Property),
 *                the Printer-class device and its parent (CM_Get_Parent).
 *   pnp NAME     GetPrinterDataEx(NAME, "PnPData", DeviceInstanceId/HardwareID)
 *   geo          GetGeoInfo of the user's location: GEO_NAME, GEO_FRIENDLYNAME
 *   devices NAME the [Devices] entry of NAME for this user
 *   dc NAME FILE a print ticket asking a custom size (a label's printable
 *                area, landscape) made a DEVMODE (PTConvertPrintTicketToDevMode),
 *                merged by the driver (DocumentProperties), then CreateDC(NULL,
 *                NAME) and one page printed to FILE
 *
 * Output: key=value lines. Our own code. */

#define _WIN32_WINNT 0x0a00
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <prntvpt.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#ifndef GEO_NAME
#define GEO_NAME 17
#endif

static const GUID usbprint_guid = { 0x28d78fad, 0x5a12, 0x11d1, { 0xae, 0x5b, 0x00, 0x00, 0xf8, 0x03, 0xa8, 0xc2 } };
static const GUID printer_class = { 0x4d36e979, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
#define IOCTL_USBPRINT_GET_LPT_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN, 12, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_USBPRINT_GET_1284_ID    CTL_CODE(FILE_DEVICE_UNKNOWN, 13, METHOD_BUFFERED, FILE_ANY_ACCESS)

static int usb(void)
{
    HDEVINFO set = SetupDiGetClassDevsW(&usbprint_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA iface = { sizeof(iface) };
    SP_DEVINFO_DATA dev = { sizeof(dev) };
    struct { DWORD cbSize; WCHAR path[512]; } detail;
    WCHAR id[MAX_DEVICE_ID_LEN], pid[MAX_DEVICE_ID_LEN];
    char buf[1024];
    DWORD n = 0, i, type;
    ULONG len;
    HANDLE h, w, r;
    OVERLAPPED ov = {0};
    unsigned char cmd[3] = { 0x1b, 'A', 0 };
    DEVINST parent;

    if (!SetupDiEnumDeviceInterfaces(set, NULL, &usbprint_guid, 0, &iface)) { printf("iface=none\n"); return 1; }
    detail.cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)&detail, sizeof(detail), NULL, &dev))
    { printf("iface=nodetail\n"); return 1; }
    printf("iface=%ls\n", detail.path);
    SetupDiGetDeviceInstanceIdW(set, &dev, id, ARRAY_SIZE(id), NULL);
    printf("usbdev=%ls\n", id);
    len = sizeof(buf);
    if (!CM_Get_DevNode_Registry_PropertyW(dev.DevInst, CM_DRP_HARDWAREID, &type, buf, &len, 0))
        printf("hwid=%ls\n", (WCHAR *)buf);
    else printf("hwid=error\n");

    h = CreateFileW(detail.path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("open=error %lu\n", GetLastError()); return 1; }
    if (DeviceIoControl(h, IOCTL_USBPRINT_GET_1284_ID, NULL, 0, buf, sizeof(buf) - 1, &n, NULL) && n > 2)
    { buf[n] = 0; printf("id=%s\n", buf + 2); }
    else printf("id=error %lu\n", GetLastError());
    if (DeviceIoControl(h, IOCTL_USBPRINT_GET_LPT_STATUS, NULL, 0, buf, 1, &n, NULL)) printf("lpt=0x%02x\n", (unsigned char)buf[0]);
    else printf("lpt=error %lu\n", GetLastError());
    CloseHandle(h);

    /* as DYMO Connect: a write handle and a read handle, overlapped */
    w = CreateFileW(detail.path, GENERIC_WRITE, 3, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
    r = CreateFileW(detail.path, GENERIC_READ, 3, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    n = 0;
    if (!WriteFile(w, cmd, 3, NULL, &ov) && GetLastError() != ERROR_IO_PENDING) printf("write=error %lu\n", GetLastError());
    else if (WaitForSingleObject(ov.hEvent, 10000) || !GetOverlappedResult(w, &ov, &n, FALSE)) printf("write=error %lu\n", GetLastError());
    else printf("wrote=%lu\n", n);
    ResetEvent(ov.hEvent);
    n = 0;
    if (!ReadFile(r, buf, 64, NULL, &ov) && GetLastError() != ERROR_IO_PENDING) printf("read=error %lu\n", GetLastError());
    else if (WaitForSingleObject(ov.hEvent, 10000) || !GetOverlappedResult(r, &ov, &n, FALSE)) printf("read=error %lu\n", GetLastError());
    else
    {
        printf("reply=%lu", n);
        for (i = 0; i < n; i++) printf(" %02x", (unsigned char)buf[i]);
        printf("\n");
        if (n >= 16) printf("roll=%.5s\n", buf + 11);
    }
    CloseHandle(w);
    CloseHandle(r);

    /* the Printer-class device, a child of the USB device */
    set = SetupDiGetClassDevsW(&printer_class, NULL, NULL, DIGCF_PRESENT);
    for (i = 0; SetupDiEnumDeviceInfo(set, i, &dev); i++)
    {
        SetupDiGetDeviceInstanceIdW(set, &dev, pid, ARRAY_SIZE(pid), NULL);
        printf("printerdev=%ls\n", pid);
        if (CM_Get_Parent(&parent, dev.DevInst, 0)) { printf("parent=error\n"); continue; }
        if (CM_Get_Device_IDW(parent, pid, ARRAY_SIZE(pid), 0)) { printf("parent=noid\n"); continue; }
        printf("parent=%ls\n", pid);
    }
    if (!i) printf("printerdev=none\n");
    return 0;
}

static int pnp(const WCHAR *name)
{
    static const WCHAR *values[] = { L"DeviceInstanceId", L"HardwareID", L"Manufacturer" };
    HANDLE h;
    WCHAR buf[512];
    DWORD type, size, ret, i;

    if (!OpenPrinterW((WCHAR *)name, &h, NULL)) { printf("open=error %lu\n", GetLastError()); return 1; }
    for (i = 0; i < ARRAY_SIZE(values); i++)
    {
        size = sizeof(buf);
        ret = GetPrinterDataExW(h, L"PnPData", values[i], &type, (BYTE *)buf, size, &size);
        if (ret) printf("%ls=error %lu\n", values[i], ret);
        else printf("%ls=%ls\n", values[i], buf);
    }
    ClosePrinter(h);
    return 0;
}

static int geo(void)
{
    WCHAR buf[128];
    GEOID id = GetUserGeoID(GEOCLASS_NATION);

    printf("geoid=%ld\n", (long)id);
    if (GetGeoInfoW(id, GEO_NAME, buf, ARRAY_SIZE(buf), 0)) printf("name=%ls\n", buf);
    else printf("name=error %lu\n", GetLastError());
    if (GetGeoInfoW(id, GEO_FRIENDLYNAME, buf, ARRAY_SIZE(buf), 0)) printf("friendly=%ls\n", buf);
    else printf("friendly=error %lu\n", GetLastError());
    return 0;
}

static int devices(const WCHAR *name)
{
    WCHAR buf[256];

    /* winspool loaded makes this user's printers */
    EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &(DWORD){0}, &(DWORD){0});
    if (GetProfileStringW(L"Devices", name, L"", buf, ARRAY_SIZE(buf))) printf("devices=%ls\n", buf);
    else printf("devices=none\n");
    return 0;
}

static const char ticket_xml[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<psf:PrintTicket xmlns:psf=\"http://schemas.microsoft.com/windows/2003/08/printing/printschemaframework\""
    " xmlns:psk=\"http://schemas.microsoft.com/windows/2003/08/printing/printschemakeywords\""
    " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" version=\"1\">"
    "<psf:Feature name=\"psk:PageMediaSize\"><psf:Option name=\"psk:CustomMediaSize\">"
    "<psf:ScoredProperty name=\"psk:MediaSizeWidth\"><psf:Value xsi:type=\"xsd:integer\">22944</psf:Value></psf:ScoredProperty>"
    "<psf:ScoredProperty name=\"psk:MediaSizeHeight\"><psf:Value xsi:type=\"xsd:integer\">50207</psf:Value></psf:ScoredProperty>"
    "</psf:Option></psf:Feature>"
    "<psf:Feature name=\"psk:PageOrientation\"><psf:Option name=\"psk:Landscape\"/></psf:Feature>"
    "</psf:PrintTicket>";

static int dc(const WCHAR *name, const WCHAR *file)
{
    HPTPROVIDER prov;
    IStream *stream;
    HGLOBAL mem;
    DEVMODEW *dm = NULL, *out;
    ULONG size = 0;
    BSTR err = NULL;
    HRESULT hr;
    HANDLE printer;
    LONG need;
    HDC hdc;
    DOCINFOW doc = { sizeof(doc), L"usbprint-probe" };

    CoInitialize(NULL);
    if ((hr = PTOpenProvider(name, 1, &prov))) { printf("provider=error %#lx\n", hr); return 1; }
    mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(ticket_xml) - 1);
    memcpy(GlobalLock(mem), ticket_xml, sizeof(ticket_xml) - 1);
    GlobalUnlock(mem);
    CreateStreamOnHGlobal(mem, TRUE, &stream);
    hr = PTConvertPrintTicketToDevMode(prov, stream, kUserDefaultDevmode, kPTJobScope, &size, &dm, &err);
    if (hr || !dm) { printf("ticket=error %#lx\n", hr); return 1; }
    printf("ticket=fields %#lx paper %d width %d length %d orientation %d\n", dm->dmFields,
           (dm->dmFields & DM_PAPERSIZE) ? dm->dmPaperSize : -1, dm->dmPaperWidth, dm->dmPaperLength, dm->dmOrientation);

    if (!OpenPrinterW((WCHAR *)name, &printer, NULL)) { printf("open=error %lu\n", GetLastError()); return 1; }
    need = DocumentPropertiesW(NULL, printer, (WCHAR *)name, NULL, NULL, 0);
    out = malloc(need);
    if (DocumentPropertiesW(NULL, printer, (WCHAR *)name, out, dm, DM_IN_BUFFER | DM_OUT_BUFFER) != IDOK)
    { printf("merge=error %lu\n", GetLastError()); return 1; }
    printf("merged=fields %#lx paper %d width %d length %d form %ls\n", out->dmFields,
           (out->dmFields & DM_PAPERSIZE) ? out->dmPaperSize : -1, out->dmPaperWidth, out->dmPaperLength,
           (out->dmFields & DM_FORMNAME) ? out->dmFormName : L"");
    ClosePrinter(printer);

    if (!(hdc = CreateDCW(NULL, name, NULL, out))) { printf("dc=error %lu\n", GetLastError()); return 1; }
    printf("dc=ok %dx%d\n", GetDeviceCaps(hdc, PHYSICALWIDTH), GetDeviceCaps(hdc, PHYSICALHEIGHT));
    doc.lpszOutput = file;
    if (StartDocW(hdc, &doc) <= 0) { printf("startdoc=error %lu\n", GetLastError()); return 1; }
    StartPage(hdc);
    TextOutW(hdc, 20, 20, L"Stained Glass", 13);
    EndPage(hdc);
    EndDoc(hdc);
    DeleteDC(hdc);
    printf("printed=ok\n");
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !wcscmp(argv[1], L"usb")) return usb();
    if (argc > 2 && !wcscmp(argv[1], L"pnp")) return pnp(argv[2]);
    if (argc > 1 && !wcscmp(argv[1], L"geo")) return geo();
    if (argc > 2 && !wcscmp(argv[1], L"devices")) return devices(argv[2]);
    if (argc > 3 && !wcscmp(argv[1], L"dc")) return dc(argv[2], argv[3]);
    printf("usage: usbprint-probe usb | pnp NAME | geo | devices NAME | dc NAME FILE\n");
    return 2;
}
