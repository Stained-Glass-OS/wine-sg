/* A 32-bit installer adds a 64-bit port monitor, its port and a printer of a 64-bit driver; a
 * 64-bit program prints through it (6115; CutePDF Writer). Windows' spooler is a 64-bit service;
 * here the print provider runs in the calling process:
 *  - AddMonitor of the 64-bit DLL from the 32-bit process failed (LoadLibrary 193), and so did
 *    AddPort: they go to the 64-bit print host now, which also puts the DLL in system32;
 *  - AddPrinter looked for the driver in the 32-bit environment only;
 *  - a job to a port the monitor keeps to itself found no monitor (EndDoc failed).
 *   probe install   (32-bit, with sgmon64.dll in its folder, not in system32)
 *   probe print     (64-bit) */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "install"))
    {
        DRIVER_INFO_3W di = {0};
        MONITOR_INFO_2W mi = { (WCHAR *)L"SG Test Monitor", (WCHAR *)L"Windows x64", (WCHAR *)L"sgmon64.dll" };
        PRINTER_INFO_2W pi = {0};
        HANDLE h;

        di.cVersion = 3; di.pName = (WCHAR *)L"SgX64Driver"; di.pEnvironment = (WCHAR *)L"Windows x64";
        di.pDriverPath = (WCHAR *)L"wineps.drv"; di.pDataFile = (WCHAR *)L"generic.ppd"; di.pConfigFile = (WCHAR *)L"wineps.drv";
        printf("DRIVER %lu\n", AddPrinterDriverExW(NULL, 3, (BYTE *)&di, APD_COPY_NEW_FILES) ? 0 : GetLastError());
        printf("MONITOR %lu\n", AddMonitorW(NULL, 2, (BYTE *)&mi) ? 0 : GetLastError());
        printf("PORT %lu\n", AddPortW(NULL, NULL, (WCHAR *)L"SG Test Monitor") ? 0 : GetLastError());
        pi.pPrinterName = (WCHAR *)L"SgMonPrinter"; pi.pPortName = (WCHAR *)L"SGP1:"; pi.pDriverName = (WCHAR *)L"SgX64Driver";
        pi.pPrintProcessor = (WCHAR *)L"winprint"; pi.pDatatype = (WCHAR *)L"RAW";
        h = AddPrinterW(NULL, 2, (BYTE *)&pi);
        printf("PRINTER %lu\n", h ? 0 : GetLastError());
        if (h) ClosePrinter(h);
        return 0;
    }
    else
    {
        DOC_INFO_1W doc = { (WCHAR *)L"sg monitor job", NULL, (WCHAR *)L"RAW" };
        HANDLE h;
        DWORD written = 0, job;
        BOOL ok = FALSE;
        if (OpenPrinterW((WCHAR *)L"SgMonPrinter", &h, NULL))
        {
            if ((job = StartDocPrinterW(h, 1, (BYTE *)&doc)))
            {
                StartPagePrinter(h);
                WritePrinter(h, (void *)"SGMON-DATA", 10, &written);
                EndPagePrinter(h);
                ok = EndDocPrinter(h);
            }
            ClosePrinter(h);
        }
        printf("PRINT %d %lu\n", ok, ok ? 0 : GetLastError());
    }
    return 0;
}
