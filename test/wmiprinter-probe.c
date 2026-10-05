/* Printers through WMI and the spooler (patches/sg/0870, 0871).
 *   wmiprinter-probe add NAME DRIVER PORT PPD   adds a printer (wineps.drv driver)
 *   wmiprinter-probe wmi                        Win32_Printer: "wmi NAME|DRIVER|PORT" per printer
 *   wmiprinter-probe enum                       EnumPrinters 2: "enum NAME|DRIVER|PORT" per printer
 *   wmiprinter-probe papers NAME                the printer's paper names, "paper NAME" each
 *   wmiprinter-probe hold SECONDS               loads winspool (as the desktop keeps it) and waits
 * then "count=N". */
#define COBJMACROS
#include <windows.h>
#include <winspool.h>
#include <wbemcli.h>
#include <stdio.h>

static const WCHAR *str(IWbemClassObject *o, const WCHAR *n, VARIANT *v)
{
    VariantInit(v);
    if (FAILED(IWbemClassObject_Get(o, n, 0, v, NULL, NULL)) || V_VT(v) != VT_BSTR) return L"<none>";
    return V_BSTR(v);
}

int wmain(int argc, WCHAR **argv)
{
    int count = 0;
    if (argc >= 6 && !wcscmp(argv[1], L"add"))
    {
        DRIVER_INFO_3W di = {0};
        PRINTER_INFO_2W pi = {0};
        const WCHAR *envs[] = { L"Windows x64", L"Windows NT x86" };
        HANDLE h;
        int i;
        di.cVersion = 3; di.pName = argv[3];
        di.pDriverPath = di.pConfigFile = (WCHAR *)L"wineps.drv";
        di.pDataFile = argv[5]; di.pDefaultDataType = (WCHAR *)L"RAW";
        for (i = 0; i < 2; i++)
        {
            di.pEnvironment = (WCHAR *)envs[i];
            AddPrinterDriverExW(NULL, 3, (BYTE *)&di, APD_COPY_NEW_FILES | APD_COPY_FROM_DIRECTORY);
        }
        pi.pPrinterName = argv[2]; pi.pDriverName = argv[3]; pi.pPortName = argv[4];
        pi.pPrintProcessor = (WCHAR *)L"wineps"; pi.pDatatype = (WCHAR *)L"RAW";
        pi.pParameters = pi.pShareName = pi.pSepFile = (WCHAR *)L"";
        h = AddPrinterW(NULL, 2, (BYTE *)&pi);
        printf("added=%d\n", h != NULL);
        if (h) ClosePrinter(h);
        return !h;
    }
    if (argc >= 2 && !wcscmp(argv[1], L"enum"))
    {
        DWORD needed = 0, n = 0, i;
        PRINTER_INFO_2W *pi;
        EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &needed, &n);
        pi = HeapAlloc(GetProcessHeap(), 0, needed ? needed : 1);
        if (needed && EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, (BYTE *)pi, needed, &needed, &n))
            for (i = 0; i < n; i++, count++)
                printf("enum %ls|%ls|%ls\n", pi[i].pPrinterName, pi[i].pDriverName, pi[i].pPortName);
        printf("count=%d\n", count);
        return 0;
    }
    if (argc >= 3 && !wcscmp(argv[1], L"hold"))
    {
        DWORD needed = 0, n = 0;
        EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &needed, &n);
        printf("holding\n");
        fflush(stdout);
        Sleep(_wtoi(argv[2]) * 1000);
        return 0;
    }
    if (argc >= 3 && !wcscmp(argv[1], L"papers"))
    {
        WCHAR names[64][64];
        int i, n = DeviceCapabilitiesW(argv[2], NULL, DC_PAPERNAMES, NULL, NULL);
        if (n > 0 && n <= 64 && DeviceCapabilitiesW(argv[2], NULL, DC_PAPERNAMES, (WCHAR *)names, NULL) == n)
            for (i = 0; i < n; i++) printf("paper %.64ls\n", names[i]);
        printf("count=%d\n", n);
        return 0;
    }
    if (argc >= 2 && !wcscmp(argv[1], L"wmi"))
    {
        IWbemLocator *loc; IWbemServices *svc; IEnumWbemClassObject *e; IWbemClassObject *o;
        ULONG got; BSTR ns, lang, q;
        CoInitialize(NULL);
        if (FAILED(CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc)))
        { printf("nolocator\n"); return 1; }
        ns = SysAllocString(L"root\\cimv2");
        if (FAILED(IWbemLocator_ConnectServer(loc, ns, NULL, NULL, NULL, 0, NULL, NULL, &svc))) { printf("noconnect\n"); return 1; }
        lang = SysAllocString(L"WQL"); q = SysAllocString(L"SELECT * FROM Win32_Printer");
        if (FAILED(IWbemServices_ExecQuery(svc, lang, q, WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, NULL, &e)))
        { printf("noquery\n"); return 1; }
        while (IEnumWbemClassObject_Next(e, WBEM_INFINITE, 1, &o, &got) == S_OK && got)
        {
            VARIANT a, b, c;
            printf("wmi %ls|", str(o, L"Name", &a));
            printf("%ls|", str(o, L"DriverName", &b));
            printf("%ls\n", str(o, L"PortName", &c));
            VariantClear(&a); VariantClear(&b); VariantClear(&c);
            IWbemClassObject_Release(o);
            count++;
        }
        printf("count=%d\n", count);
        return 0;
    }
    printf("usage\n");
    return 2;
}
