/* ptcaps-probe: a printer's capabilities as programs ask for them
 *
 *   ptcaps-probe devcaps PRINTER   DeviceCapabilities: papers (id|name|width x
 *                                  length in 0.1 mm), bins, duplex, orientation,
 *                                  resolutions, copies, collate, colour, extents
 *   ptcaps-probe caps PRINTER      PTGetPrintCapabilities' document, as written
 *   ptcaps-probe status PRINTER    GetPrinter's status, levels 2 and 6:
 *                                  "status 0xN 0xN"
 *   ptcaps-probe ticket PRINTER OPTION
 *                                  a job ticket naming the PageMediaSize option
 *                                  OPTION (as a program takes it from the
 *                                  capabilities, with its MediaSizeWidth and
 *                                  MediaSizeHeight), converted to a DEVMODE:
 *                                  "devmode paper N width W length L"
 */
#define COBJMACROS
#include <windows.h>
#include <winspool.h>
#include <prntvpt.h>
#include <objbase.h>
#include <stdio.h>
#include <wchar.h>

static int devcaps(const WCHAR *printer)
{
    int n, i;
    WORD *ids; WCHAR (*names)[64]; POINT *sizes; WCHAR (*binnames)[24]; LONG *res;
    DWORD ext;

    n = DeviceCapabilitiesW(printer, NULL, DC_PAPERS, NULL, NULL);
    printf("papers %d\n", n);
    if (n > 0)
    {
        ids = calloc(n, sizeof(*ids)); names = calloc(n, sizeof(*names)); sizes = calloc(n, sizeof(*sizes));
        DeviceCapabilitiesW(printer, NULL, DC_PAPERS, (WCHAR *)ids, NULL);
        DeviceCapabilitiesW(printer, NULL, DC_PAPERNAMES, (WCHAR *)names, NULL);
        DeviceCapabilitiesW(printer, NULL, DC_PAPERSIZE, (WCHAR *)sizes, NULL);
        for (i = 0; i < n; i++)
            printf("paper %u|%.64ls|%ldx%ld\n", ids[i], names[i], sizes[i].x, sizes[i].y);
    }
    n = DeviceCapabilitiesW(printer, NULL, DC_BINS, NULL, NULL);
    printf("bins %d\n", n);
    if (n > 0)
    {
        ids = calloc(n, sizeof(*ids)); binnames = calloc(n, sizeof(*binnames));
        DeviceCapabilitiesW(printer, NULL, DC_BINS, (WCHAR *)ids, NULL);
        DeviceCapabilitiesW(printer, NULL, DC_BINNAMES, (WCHAR *)binnames, NULL);
        for (i = 0; i < n; i++) printf("bin %u|%.24ls\n", ids[i], binnames[i]);
    }
    printf("duplex %d\n", DeviceCapabilitiesW(printer, NULL, DC_DUPLEX, NULL, NULL));
    printf("orientation %d\n", DeviceCapabilitiesW(printer, NULL, DC_ORIENTATION, NULL, NULL));
    printf("copies %d\n", DeviceCapabilitiesW(printer, NULL, DC_COPIES, NULL, NULL));
    printf("collate %d\n", DeviceCapabilitiesW(printer, NULL, DC_COLLATE, NULL, NULL));
    printf("color %d\n", DeviceCapabilitiesW(printer, NULL, DC_COLORDEVICE, NULL, NULL));
    ext = DeviceCapabilitiesW(printer, NULL, DC_MINEXTENT, NULL, NULL);
    printf("minextent %ux%u\n", LOWORD(ext), HIWORD(ext));
    ext = DeviceCapabilitiesW(printer, NULL, DC_MAXEXTENT, NULL, NULL);
    printf("maxextent %ux%u\n", LOWORD(ext), HIWORD(ext));
    n = DeviceCapabilitiesW(printer, NULL, DC_ENUMRESOLUTIONS, NULL, NULL);
    if (n > 0)
    {
        res = calloc(n, 2 * sizeof(LONG));
        DeviceCapabilitiesW(printer, NULL, DC_ENUMRESOLUTIONS, (WCHAR *)res, NULL);
        for (i = 0; i < n; i++) printf("resolution %ldx%ld\n", res[2 * i], res[2 * i + 1]);
    }
    return 0;
}

static int caps(const WCHAR *printer)
{
    HPTPROVIDER prov;
    IStream *out;
    HGLOBAL mem;
    HRESULT hr;
    char *p;
    STATSTG st;

    hr = PTOpenProvider(printer, 1, &prov);
    if (hr != S_OK) { printf("open=0x%08lx\n", hr); return 1; }
    CreateStreamOnHGlobal(NULL, TRUE, &out);
    hr = PTGetPrintCapabilities(prov, NULL, out, NULL);
    if (hr != S_OK) { printf("caps=0x%08lx\n", hr); return 1; }
    IStream_Stat(out, &st, STATFLAG_NONAME);
    GetHGlobalFromStream(out, &mem);
    p = GlobalLock(mem);
    fwrite(p, 1, st.cbSize.LowPart, stdout);
    printf("\n");
    GlobalUnlock(mem);
    IStream_Release(out);
    PTCloseProvider(prov);
    return 0;
}

static int ticket(const WCHAR *printer, const WCHAR *option, int width, int height)
{
    static const char fmt[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<psf:PrintTicket xmlns:psf=\"http://schemas.microsoft.com/windows/2003/08/printing/printschemaframework\" "
        "xmlns:psk=\"http://schemas.microsoft.com/windows/2003/08/printing/printschemakeywords\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" %s version=\"1\">"
        "<psf:Feature name=\"psk:PageMediaSize\"><psf:Option name=\"%ls\">"
        "<psf:ScoredProperty name=\"psk:MediaSizeWidth\"><psf:Value xsi:type=\"xsd:integer\">%d</psf:Value></psf:ScoredProperty>"
        "<psf:ScoredProperty name=\"psk:MediaSizeHeight\"><psf:Value xsi:type=\"xsd:integer\">%d</psf:Value></psf:ScoredProperty>"
        "</psf:Option></psf:Feature></psf:PrintTicket>";
    char xml[4096], ns[512] = "";
    HPTPROVIDER prov;
    IStream *in;
    DEVMODEW *dm;
    ULONG size;
    HRESULT hr;
    LARGE_INTEGER zero = {{0}};
    const WCHAR *colon = wcschr(option, ':');

    hr = PTOpenProvider(printer, 1, &prov);
    if (hr != S_OK) { printf("open=0x%08lx\n", hr); return 1; }
    /* the private namespace, as the capabilities declared it */
    if (colon && wcsncmp(option, L"psk:", 4))
    {
        IStream *out;
        HGLOBAL mem;
        STATSTG st;
        char *p, *q, prefix[64], key[80];

        snprintf(prefix, sizeof(prefix), "%.*ls", (int)(colon - option), option);
        CreateStreamOnHGlobal(NULL, TRUE, &out);
        PTGetPrintCapabilities(prov, NULL, out, NULL);
        IStream_Stat(out, &st, STATFLAG_NONAME);
        GetHGlobalFromStream(out, &mem);
        p = GlobalLock(mem);
        p[st.cbSize.LowPart - 1] = 0;
        snprintf(key, sizeof(key), "xmlns:%s=\"", prefix);
        if ((q = strstr(p, key)))
        {
            char *e = strchr(q + strlen(key), '"');
            if (e) snprintf(ns, sizeof(ns), "%.*s", (int)(e - q + 1), q);
        }
        GlobalUnlock(mem);
        IStream_Release(out);
    }
    snprintf(xml, sizeof(xml), fmt, ns, option, width, height);
    CreateStreamOnHGlobal(NULL, TRUE, &in);
    IStream_Write(in, xml, strlen(xml), NULL);
    IStream_Seek(in, zero, STREAM_SEEK_SET, NULL);
    hr = PTConvertPrintTicketToDevMode(prov, in, kUserDefaultDevmode, kPTJobScope, &size, &dm, NULL);
    if (hr != S_OK) { printf("convert=0x%08lx\n", hr); return 1; }
    printf("devmode paper %d width %d length %d fields 0x%lx\n",
           (dm->dmFields & DM_PAPERSIZE) ? dm->dmPaperSize : -1, dm->dmPaperWidth, dm->dmPaperLength, dm->dmFields);
    PTReleaseMemory(dm);
    PTCloseProvider(prov);
    return 0;
}

static int status(const WCHAR *printer)
{
    HANDLE h;
    BYTE buf[16384];
    DWORD needed;
    PRINTER_INFO_2W *pi2 = (PRINTER_INFO_2W *)buf;
    PRINTER_INFO_6 pi6 = { 0 };

    if (!OpenPrinterW((WCHAR *)printer, &h, NULL)) { printf("open=%lu\n", GetLastError()); return 1; }
    if (!GetPrinterW(h, 2, buf, sizeof(buf), &needed)) { printf("level2=%lu\n", GetLastError()); return 1; }
    if (!GetPrinterW(h, 6, (BYTE *)&pi6, sizeof(pi6), &needed)) { printf("level6=%lu\n", GetLastError()); return 1; }
    printf("status 0x%lx 0x%lx\n", pi2->Status, pi6.dwStatus);
    ClosePrinter(h);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    CoInitialize(NULL);
    if (argc >= 3 && !wcscmp(argv[1], L"devcaps")) return devcaps(argv[2]);
    if (argc >= 3 && !wcscmp(argv[1], L"caps")) return caps(argv[2]);
    if (argc >= 3 && !wcscmp(argv[1], L"status")) return status(argv[2]);
    if (argc >= 6 && !wcscmp(argv[1], L"ticket")) return ticket(argv[2], argv[3], _wtoi(argv[4]), _wtoi(argv[5]));
    printf("usage\n");
    return 2;
}
