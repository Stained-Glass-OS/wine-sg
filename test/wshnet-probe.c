/* WScript.Network's drives and printers (patches/sg/1650).
 *
 * MapNetworkDrive, RemoveNetworkDrive, EnumNetworkDrives,
 * AddPrinterConnection, RemovePrinterConnection, EnumPrinterConnections,
 * SetDefaultPrinter and AddWindowsPrinterConnection were E_NOTIMPL. The
 * probe adds two printers, then uses the object through IDispatch, as
 * scripts do: the printers are listed (port, name), one becomes the
 * default, a share that is not there gives a network error, the drive list
 * is a list of pairs.
 */
#define COBJMACROS
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const char ppd[] =
    "*PPD-Adobe: \"4.3\"\n*FormatVersion: \"4.3\"\n*FileVersion: \"1.0\"\n*LanguageVersion: English\n"
    "*LanguageEncoding: ISOLatin1\n*PCFileName: \"SGNET.PPD\"\n*Manufacturer: \"Stained Glass OS\"\n"
    "*ModelName: \"SG Net PS\"\n*NickName: \"SG Net PS\"\n*ShortNickName: \"SG Net PS\"\n"
    "*PSVersion: \"(3010.000) 0\"\n*LanguageLevel: \"3\"\n*ColorDevice: True\n*DefaultColorSpace: RGB\n"
    "*OpenUI *PageSize/Media Size: PickOne\n*DefaultPageSize: Letter\n"
    "*PageSize Letter/Letter: \"<</PageSize[612 792]>>setpagedevice\"\n*CloseUI: *PageSize\n"
    "*DefaultImageableArea: Letter\n*ImageableArea Letter/Letter: \"18 18 594 774\"\n"
    "*DefaultPaperDimension: Letter\n*PaperDimension Letter/Letter: \"612 792\"\n*DefaultResolution: 300dpi\n";

static void add_printer(const WCHAR *name)
{
    PRINTER_INFO_2W pi = { 0 };
    HANDLE printer;

    pi.pPrinterName = (WCHAR *)name;
    pi.pDriverName = L"SG Net PS";
    pi.pPortName = L"FILE:";
    pi.pPrintProcessor = L"WinPrint";
    pi.pDatatype = L"RAW";
    printer = AddPrinterW(NULL, 2, (BYTE *)&pi);
    printf("AddPrinter %ls: %p (%lu)\n", name, printer, printer ? 0 : GetLastError());
    if (printer) ClosePrinter(printer);
}

static HRESULT call(IDispatch *disp, const WCHAR *name, UINT argc, VARIANT *argv, VARIANT *result)
{
    DISPPARAMS params = { argv, NULL, argc, 0 };
    DISPID id;
    HRESULT hr;
    WCHAR *n = (WCHAR *)name;
    EXCEPINFO ei;

    if (result) VariantInit(result);
    memset(&ei, 0, sizeof(ei));
    if (FAILED(hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &n, 1, 0, &id))) return hr;
    hr = IDispatch_Invoke(disp, id, &IID_NULL, 0, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &params, result, &ei, NULL);
    if (hr == DISP_E_EXCEPTION) hr = ei.scode;
    return hr;
}

static VARIANT bstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], sys[MAX_PATH], out[MAX_PATH], def[256];
    DRIVER_INFO_3W di = { 3 };
    IDispatch *net, *coll;
    VARIANT r, args[5], index;
    DWORD size, written;
    CLSID clsid;
    HANDLE f;
    HRESULT hr;
    LONG count = 0, i;
    BOOL found_a = FALSE;

    CoInitialize(NULL);

    GetPrinterDriverDirectoryW(NULL, NULL, 1, (BYTE *)dir, sizeof(dir), &size);
    CreateDirectoryW(dir, NULL);
    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(path, MAX_PATH, L"%ls\\wineps.drv", sys);
    swprintf(out, MAX_PATH, L"%ls\\wineps.drv", dir);
    CopyFileW(path, out, FALSE);
    swprintf(out, MAX_PATH, L"%ls\\sgnet.ppd", dir);
    f = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, ppd, sizeof(ppd) - 1, &written, NULL);
    CloseHandle(f);
    di.pName = L"SG Net PS";
    di.pDriverPath = L"wineps.drv";
    di.pDataFile = L"sgnet.ppd";
    di.pConfigFile = L"wineps.drv";
    AddPrinterDriverW(NULL, 3, (BYTE *)&di);
    add_printer(L"SG Net A");
    add_printer(L"SG Net B");

    hr = CLSIDFromProgID(L"WScript.Network", &clsid);
    if (SUCCEEDED(hr)) hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&net);
    check(hr == S_OK, "WScript.Network");
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }

    /* printers */
    hr = call(net, L"EnumPrinterConnections", 0, NULL, &r);
    check(hr == S_OK && V_VT(&r) == VT_DISPATCH, "EnumPrinterConnections");
    if (hr == S_OK && V_VT(&r) == VT_DISPATCH)
    {
        coll = V_DISPATCH(&r);
        call(coll, L"Count", 0, NULL, &r);
        count = V_I4(&r);
        for (i = 0; i + 1 < count; i += 2)
        {
            VARIANT port, name;
            V_VT(&index) = VT_I4; V_I4(&index) = i;
            call(coll, L"Item", 1, &index, &port);
            V_I4(&index) = i + 1;
            call(coll, L"Item", 1, &index, &name);
            printf("  %ls -> %ls\n", V_BSTR(&port), V_BSTR(&name));
            if (!wcscmp(V_BSTR(&name), L"SG Net A") && !wcscmp(V_BSTR(&port), L"FILE:")) found_a = TRUE;
            VariantClear(&port);
            VariantClear(&name);
        }
        IDispatch_Release(coll);
    }
    check(count >= 4 && !(count & 1) && found_a, "... lists port and printer pairs, ours among them");

    args[0] = bstr(L"SG Net B");
    hr = call(net, L"SetDefaultPrinter", 1, args, NULL);
    size = ARRAYSIZE(def);
    GetDefaultPrinterW(def, &size);
    printf("default: hr %#lx %ls\n", hr, def);
    check(hr == S_OK && !wcscmp(def, L"SG Net B"), "SetDefaultPrinter");
    args[0] = bstr(L"No Such Printer");
    hr = call(net, L"SetDefaultPrinter", 1, args, NULL);
    check(FAILED(hr) && hr != E_NOTIMPL, "SetDefaultPrinter of no printer fails");

    /* drives */
    hr = call(net, L"EnumNetworkDrives", 0, NULL, &r);
    check(hr == S_OK && V_VT(&r) == VT_DISPATCH, "EnumNetworkDrives");
    if (hr == S_OK && V_VT(&r) == VT_DISPATCH)
    {
        call(V_DISPATCH(&r), L"Count", 0, NULL, &index);
        check(V_VT(&index) == VT_I4 && !(V_I4(&index) & 1), "... a list of pairs");
        IDispatch_Release(V_DISPATCH(&r));
    }
    /* arguments in reverse: password, user, update profile, remote, local */
    V_VT(&args[4]) = VT_BSTR; V_BSTR(&args[4]) = SysAllocString(L"Q:");
    V_VT(&args[3]) = VT_BSTR; V_BSTR(&args[3]) = SysAllocString(L"\\\\sg-no-such-host\\share");
    V_VT(&args[2]) = VT_ERROR; V_ERROR(&args[2]) = DISP_E_PARAMNOTFOUND;
    V_VT(&args[1]) = VT_ERROR; V_ERROR(&args[1]) = DISP_E_PARAMNOTFOUND;
    V_VT(&args[0]) = VT_ERROR; V_ERROR(&args[0]) = DISP_E_PARAMNOTFOUND;
    hr = call(net, L"MapNetworkDrive", 5, args, NULL);
    printf("MapNetworkDrive to no host: %#lx\n", hr);
    check(FAILED(hr) && hr != E_NOTIMPL, "MapNetworkDrive to a share that is not there: a network error");
    V_VT(&args[2]) = VT_BSTR; V_BSTR(&args[2]) = SysAllocString(L"Q:");
    V_VT(&args[1]) = VT_BOOL; V_BOOL(&args[1]) = VARIANT_TRUE;
    V_VT(&args[0]) = VT_BOOL; V_BOOL(&args[0]) = VARIANT_FALSE;
    hr = call(net, L"RemoveNetworkDrive", 3, args, NULL);
    printf("RemoveNetworkDrive of none: %#lx\n", hr);
    check(FAILED(hr) && hr != E_NOTIMPL, "RemoveNetworkDrive of no connection: an error");

    IDispatch_Release(net);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
