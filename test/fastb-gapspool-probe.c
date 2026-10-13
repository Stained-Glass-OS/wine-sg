/* winspool.drv: GetPrinter/SetPrinter level 3, 5, 6 and commands; connection/processor/provider deletion; datatypes A (patch 3619) */
#include <windows.h>
#include <winspool.h>
#include <sddl.h>
#include <stdio.h>

static int f;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (err %lu)\n", __LINE__, #c, GetLastError()); f++; } } while (0)

int main(void)
{
    HKEY key;
    HANDLE h = NULL;
    BYTE buf[2048];
    DWORD needed = 0, ret;
    PRINTER_INFO_3 *pi3 = (PRINTER_INFO_3 *)buf;
    PRINTER_INFO_6 pi6;
    PRINTER_INFO_5A pi5;
    char datatypes[1024];
    DWORD rc;

    setvbuf(stdout, NULL, _IONBF, 0);

    /* nothing here is a connection */
    SetLastError(0);
    CHECK(!DeletePrinterConnectionA("\\\\server\\printer") && GetLastError() == ERROR_INVALID_PRINTER_NAME);
    /* print processors and providers: the defaults stay, unknown ones are reported */
    SetLastError(0);
    CHECK(!DeletePrintProcessorA(NULL, NULL, "NoSuchProcessor") && GetLastError() != 0);
    SetLastError(0);
    CHECK(!DeletePrintProcessorA(NULL, NULL, "winprint") && GetLastError() == ERROR_ACCESS_DENIED);
    SetLastError(0);
    CHECK(!DeletePrintProvidorA(NULL, NULL, "Windows NT Local Print Providor") && GetLastError() == ERROR_ACCESS_DENIED);
    SetLastError(0);
    CHECK(!DeletePrintProvidorA(NULL, NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER);

    /* the datatypes a print processor takes, as ANSI */
    needed = rc = 0;
    memset(datatypes, 0, sizeof(datatypes));
    ret = EnumPrintProcessorDatatypesA(NULL, "winprint", 1, (BYTE *)datatypes, sizeof(datatypes), &needed, &rc);
    CHECK(ret && rc >= 1);
    if (ret && rc)
    {
        DATATYPES_INFO_1A *dt = (DATATYPES_INFO_1A *)datatypes;
        CHECK(dt[0].pName && dt[0].pName[0] && (BYTE *)dt[0].pName > (BYTE *)datatypes);
        CHECK(needed <= sizeof(datatypes));
    }
    SetLastError(0);
    CHECK(!EnumPrintProcessorDatatypesA(NULL, "winprint", 1, (BYTE *)datatypes, 1, &needed, &rc) &&
          GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed > 1);
    SetLastError(0);
    CHECK(!EnumPrintProcessorDatatypesA(NULL, "winprint", 2, (BYTE *)datatypes, sizeof(datatypes), &needed, &rc));

    /* a printer made of registry values */
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Print\\Printers\\SgGatePrinter", 0, NULL, 0,
                        KEY_ALL_ACCESS, NULL, &key, NULL))
    { puts("FAIL cannot create the printer key"); f++; goto done; }
    RegSetValueExA(key, "Name", 0, REG_SZ, (BYTE *)"SgGatePrinter", 14);
    RegSetValueExA(key, "Port", 0, REG_SZ, (BYTE *)"FILE:", 6);
    RegSetValueExA(key, "Print Processor", 0, REG_SZ, (BYTE *)"winprint", 9);
    RegSetValueExA(key, "Datatype", 0, REG_SZ, (BYTE *)"RAW", 4);
    RegCloseKey(key);
    if (!OpenPrinterA("SgGatePrinter", &h, NULL)) { printf("FAIL OpenPrinter %lu\n", GetLastError()); f++; goto done; }

    /* level 3: the default descriptor, then the one set */
    CHECK(!GetPrinterA(h, 3, buf, 1, &needed) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed > sizeof(PRINTER_INFO_3));
    CHECK(GetPrinterA(h, 3, buf, sizeof(buf), &needed));
    CHECK(pi3->pSecurityDescriptor && IsValidSecurityDescriptor(pi3->pSecurityDescriptor));
    {
        PSECURITY_DESCRIPTOR sd;
        PRINTER_INFO_3 set;
        char *sddl = NULL;
        BOOL ok;

        ok = ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:(A;;LCSWSDRCWDWO;;;WD)", SDDL_REVISION_1, &sd, NULL);
        CHECK(ok);
        set.pSecurityDescriptor = sd;
        CHECK(SetPrinterA(h, 3, (BYTE *)&set, 0));
        CHECK(GetPrinterA(h, 3, buf, sizeof(buf), &needed));
        ok = ConvertSecurityDescriptorToStringSecurityDescriptorA(pi3->pSecurityDescriptor, SDDL_REVISION_1,
                OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &sddl, NULL);
        CHECK(ok && sddl && strstr(sddl, "O:BA") && strstr(sddl, "(A;;LCSWSDRCWDWO;;;WD)"));
        if (sddl) LocalFree(sddl);
        LocalFree(sd);
        set.pSecurityDescriptor = NULL;
        SetLastError(0);
        CHECK(!SetPrinterA(h, 3, (BYTE *)&set, 0) && GetLastError() == ERROR_INVALID_PARAMETER);
    }

    /* the commands */
    CHECK(GetPrinterA(h, 6, (BYTE *)&pi6, sizeof(pi6), &needed));
    CHECK(!(pi6.dwStatus & PRINTER_STATUS_PAUSED));
    CHECK(SetPrinterA(h, 0, NULL, PRINTER_CONTROL_PAUSE));
    CHECK(GetPrinterA(h, 6, (BYTE *)&pi6, sizeof(pi6), &needed));
    CHECK(pi6.dwStatus & PRINTER_STATUS_PAUSED);
    CHECK(SetPrinterA(h, 0, NULL, PRINTER_CONTROL_RESUME));
    CHECK(GetPrinterA(h, 6, (BYTE *)&pi6, sizeof(pi6), &needed));
    CHECK(!(pi6.dwStatus & PRINTER_STATUS_PAUSED));
    CHECK(SetPrinterA(h, 0, NULL, PRINTER_CONTROL_PURGE));
    SetLastError(0);
    CHECK(!SetPrinterA(h, 0, NULL, 0x777) && GetLastError() == ERROR_INVALID_PARAMETER);
    pi6.dwStatus = PRINTER_STATUS_TONER_LOW;
    CHECK(SetPrinterA(h, 6, (BYTE *)&pi6, PRINTER_CONTROL_SET_STATUS));
    CHECK(GetPrinterA(h, 6, (BYTE *)&pi6, sizeof(pi6), &needed));
    CHECK(pi6.dwStatus & PRINTER_STATUS_TONER_LOW);

    /* level 5 */
    memset(&pi5, 0, sizeof(pi5));
    pi5.pPrinterName = "SgGatePrinter";
    pi5.pPortName = "LPT1:";
    pi5.Attributes = PRINTER_ATTRIBUTE_SHARED;
    pi5.DeviceNotSelectedTimeout = 17;
    pi5.TransmissionRetryTimeout = 91;
    CHECK(SetPrinterA(h, 5, (BYTE *)&pi5, 0));
    {
        PRINTER_INFO_5A *got = (PRINTER_INFO_5A *)buf;
        CHECK(GetPrinterA(h, 5, buf, sizeof(buf), &needed));
        CHECK(got->Attributes == PRINTER_ATTRIBUTE_SHARED && got->DeviceNotSelectedTimeout == 17 &&
              got->TransmissionRetryTimeout == 91 && got->pPortName && !strcmp(got->pPortName, "LPT1:"));
    }
    ClosePrinter(h);

done:
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Print\\Printers\\SgGatePrinter");
    puts(f ? "RESULT: FAIL" : "RESULT: PASS");
    return f;
}
