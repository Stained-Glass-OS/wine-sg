/* printui command line verbs (patch 3296), run by test/fastprintui-gate.sh. A fake driver and printers are
 * made through the spooler API in the scratch prefix; no real printer is involved. Each verb is run as
 * "rundll32 printui.dll,PrintUIEntry ..." and the spooler state is read back with the spooler API. */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void printui(const WCHAR *args)
{
    WCHAR cmd[600];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    swprintf(cmd, 600, L"rundll32.exe printui.dll,PrintUIEntry %ls", args);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("note: cannot run rundll32\n"); return; }
    if (WaitForSingleObject(pi.hProcess, 30000) != WAIT_OBJECT_0) { printf("note: rundll32 timed out\n"); TerminateProcess(pi.hProcess, 1); }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

static BOOL has_printer(const WCHAR *name)
{
    DWORD need = 0, n = 0, i;
    BYTE *buf;
    BOOL found = FALSE;
    EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &need, &n);
    if (!need || !(buf = malloc(need))) return FALSE;
    if (EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, buf, need, &need, &n))
        for (i = 0; i < n; i++) if (!wcscmp(((PRINTER_INFO_2W *)buf)[i].pPrinterName, name)) found = TRUE;
    free(buf);
    return found;
}
static BOOL has_driver(const WCHAR *name)
{
    DWORD need = 0, n = 0, i;
    BYTE *buf;
    BOOL found = FALSE;
    EnumPrinterDriversW(NULL, NULL, 2, NULL, 0, &need, &n);
    if (!need || !(buf = malloc(need))) return FALSE;
    if (EnumPrinterDriversW(NULL, NULL, 2, buf, need, &need, &n))
        for (i = 0; i < n; i++) if (!wcscmp(((DRIVER_INFO_2W *)buf)[i].pName, name)) found = TRUE;
    free(buf);
    return found;
}
static void add_printer(WCHAR *name)
{
    PRINTER_INFO_2W pi = {0};
    HANDLE h;
    pi.pPrinterName = name; pi.pPortName = (WCHAR *)L"FILE:"; pi.pDriverName = (WCHAR *)L"SgFakeDriver";
    pi.pPrintProcessor = (WCHAR *)L"winprint"; pi.pDatatype = (WCHAR *)L"RAW";
    if ((h = AddPrinterW(NULL, 2, (BYTE *)&pi))) ClosePrinter(h);
}

int main(void)
{
    DRIVER_INFO_3W di = {0};
    WCHAR def[200];
    DWORD len = 200;

    di.cVersion = 3; di.pName = (WCHAR *)L"SgFakeDriver"; di.pDriverPath = (WCHAR *)L"wineps.drv";
    di.pDataFile = (WCHAR *)L"generic.ppd"; di.pConfigFile = (WCHAR *)L"wineps.drv";
    check(AddPrinterDriverExW(NULL, 3, (BYTE *)&di, APD_COPY_NEW_FILES), "fake driver installed");
    add_printer((WCHAR *)L"SgPrinterOne");
    add_printer((WCHAR *)L"SgPrinterTwo");
    check(has_printer(L"SgPrinterOne") && has_printer(L"SgPrinterTwo"), "two fake printers exist");

    printui(L"/y /n \"SgPrinterTwo\"");
    check(GetDefaultPrinterW(def, &len) && !wcscmp(def, L"SgPrinterTwo"), "/y /n sets the default printer");

    printui(L"/dl /n \"SgPrinterOne\" /q");
    check(!has_printer(L"SgPrinterOne"), "/dl deletes the named printer");
    check(has_printer(L"SgPrinterTwo"), "/dl leaves the other printer");

    printui(L"/dl /n \"SgNoSuchPrinter\" /q");
    check(has_printer(L"SgPrinterTwo"), "/dl of an unknown printer changes nothing");

    printui(L"/dd /m \"SgNoSuchDriver\" /h x64 /q");
    check(has_driver(L"SgFakeDriver"), "/dd of an unknown driver changes nothing");
    printui(L"/dd /m \"SgFakeDriver\" /h x86 /q");
    check(has_driver(L"SgFakeDriver"), "/dd with another architecture leaves the driver");
    printui(L"/dd /m \"SgFakeDriver\" /h x64 /v 2 /q");
    check(has_driver(L"SgFakeDriver"), "/dd with another driver version leaves the driver");

    printui(L"/dl /n \"SgPrinterTwo\" /q");
    check(!has_printer(L"SgPrinterTwo"), "second printer deleted");
    printui(L"/dd /m \"SgFakeDriver\" /h x64 /v 3 /q");
    check(!has_driver(L"SgFakeDriver"), "/dd deletes the driver");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
