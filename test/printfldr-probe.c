/* shell32 Printers folder batch (patches/sg/2010), run by test/printfldr-gate.sh.
 * Families: EnumObjects / ParseDisplayName over the registered printers,
 * GetDisplayNameOf, GetDetailsOf with a pidl (all six columns), GetDetailsEx,
 * CompareIDs by column, GetAttributesOf, BindToObject / BindToStorage,
 * GetUIObjectOf and IPersistFolder2::GetClassID.
 *
 *   printfldr-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

DEFINE_GUID(SG_CLSID_Printers, 0x2227a280, 0x3aea, 0x1069, 0xa2, 0xde, 0x08, 0x00, 0x2b, 0x30, 0x30, 0x9d);
DEFINE_GUID(SG_FMTID_Storage, 0xb725f130, 0x47ef, 0x101a, 0xa5, 0xf1, 0x02, 0x60, 0x8c, 0x9e, 0xeb, 0xac);
DEFINE_GUID(SG_IID_ExtractIconW, 0x000214fa, 0, 0, 0xc0, 0, 0, 0, 0, 0, 0, 0x46);
DEFINE_GUID(SG_FMTID_Summary, 0xf29f85e0, 0x4ff9, 0x1068, 0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

static const WCHAR *keyroot = L"System\\CurrentControlSet\\Control\\Print\\Printers\\";

static void setstr(HKEY k, const WCHAR *n, const WCHAR *v)
{
    RegSetValueExW(k, n, 0, REG_SZ, (const BYTE *)v, (lstrlenW(v) + 1) * sizeof(WCHAR));
}

static void add_printer(const WCHAR *name, const WCHAR *comment, const WCHAR *location)
{
    WCHAR path[300];
    HKEY k;
    DWORD zero = 0;
    swprintf(path, 300, L"%ls%ls", keyroot, name);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL)) return;
    setstr(k, L"Name", name);
    setstr(k, L"Port", L"FILE:");
    setstr(k, L"Printer Driver", L"Wine PostScript Driver");
    setstr(k, L"Print Processor", L"winprint");
    setstr(k, L"Description", comment);
    setstr(k, L"Location", location);
    RegSetValueExW(k, L"Attributes", 0, REG_DWORD, (BYTE *)&zero, 4);
    RegSetValueExW(k, L"Status", 0, REG_DWORD, (BYTE *)&zero, 4);
    RegCloseKey(k);
}

static void del_printer(const WCHAR *name)
{
    WCHAR path[300];
    swprintf(path, 300, L"%ls%ls", keyroot, name);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, path);
}

static int count_items(IShellFolder2 *sf, DWORD flags, LPITEMIDLIST *first, LPITEMIDLIST *second)
{
    IEnumIDList *en = NULL;
    LPITEMIDLIST pidl;
    ULONG got;
    int n = 0;
    if (first) *first = NULL;
    if (second) *second = NULL;
    if (FAILED(IShellFolder2_EnumObjects(sf, NULL, flags, &en)) || !en) return -1;
    while (IEnumIDList_Next(en, 1, &pidl, &got) == S_OK && got == 1)
    {
        if (n == 0 && first) *first = pidl;
        else if (n == 1 && second) *second = pidl;
        else CoTaskMemFree(pidl);
        n++;
    }
    IEnumIDList_Release(en);
    return n;
}

static int name_is(IShellFolder2 *sf, LPCITEMIDLIST pidl, const WCHAR *want)
{
    STRRET sr;
    WCHAR buf[MAX_PATH];
    if (FAILED(IShellFolder2_GetDisplayNameOf(sf, pidl, SHGDN_NORMAL, &sr))) return 0;
    if (FAILED(StrRetToBufW(&sr, pidl, buf, MAX_PATH))) return 0;
    return !wcscmp(buf, want);
}

static int detail_is(IShellFolder2 *sf, LPCITEMIDLIST pidl, UINT col, const WCHAR *want)
{
    SHELLDETAILS d;
    WCHAR buf[MAX_PATH];
    memset(&d, 0, sizeof(d));
    if (IShellFolder2_GetDetailsOf(sf, pidl, col, &d) != S_OK) return 0;
    if (FAILED(StrRetToBufW(&d.str, pidl, buf, MAX_PATH))) return 0;
    return !wcscmp(buf, want) && d.fmt == LVCFMT_LEFT;
}

int main(void)
{
    IShellFolder2 *sf = NULL;
    IPersistFolder2 *pf = NULL;
    LPITEMIDLIST alpha = NULL, beta = NULL, p1, p2, junk = NULL;
    HRESULT hr;
    DWORD attr;
    VARIANT v;
    SHCOLUMNID scid;
    ULONG eaten = 0;
    int n;

    CoInitialize(NULL);
    del_printer(L"SG Alpha Printer"); del_printer(L"SG Beta Printer");
    add_printer(L"SG Beta Printer", L"aaa second", L"Lab");
    add_printer(L"SG Alpha Printer", L"zzz first", L"Room 1");
    hr = CoCreateInstance(&SG_CLSID_Printers, NULL, CLSCTX_INPROC_SERVER, &IID_IShellFolder2, (void **)&sf);
    checkhr(hr, S_OK, "create the Printers folder");
    if (!sf) { printf("RESULT: FAIL\n"); return 1; }

    n = count_items(sf, SHCONTF_NONFOLDERS, &p1, &p2);
    check(n == 2, "two printers enumerated");
    check(count_items(sf, SHCONTF_FOLDERS, NULL, NULL) == 0, "the printers are not folders");
    CoTaskMemFree(p1); CoTaskMemFree(p2);

    /* parse names, case-insensitively */
    hr = IShellFolder2_ParseDisplayName(sf, NULL, NULL, (WCHAR *)L"sg alpha printer", &eaten, &alpha, NULL);
    checkhr(hr, S_OK, "ParseDisplayName finds a printer");
    check(alpha && name_is(sf, alpha, L"SG Alpha Printer"), "the parsed item has the printer's own spelling");
    check(eaten == 16, "characters eaten");
    hr = IShellFolder2_ParseDisplayName(sf, NULL, NULL, (WCHAR *)L"SG Beta Printer", NULL, &beta, NULL);
    checkhr(hr, S_OK, "ParseDisplayName second printer");
    hr = IShellFolder2_ParseDisplayName(sf, NULL, NULL, (WCHAR *)L"No Such Printer", NULL, &junk, NULL);
    checkhr(hr, HRESULT_FROM_WIN32(ERROR_INVALID_PRINTER_NAME), "ParseDisplayName of a missing printer");
    check(junk == NULL, "no pidl for a missing printer");
    hr = IShellFolder2_ParseDisplayName(sf, NULL, NULL, (WCHAR *)L"", NULL, &junk, NULL);
    checkhr(hr, E_INVALIDARG, "ParseDisplayName of an empty name");
    attr = 0xffffffff;
    hr = IShellFolder2_ParseDisplayName(sf, NULL, NULL, (WCHAR *)L"SG Alpha Printer", NULL, &p1, &attr);
    check(hr == S_OK && attr == SFGAO_CANLINK, "ParseDisplayName narrows the attributes");
    CoTaskMemFree(p1);

    /* names in every form */
    { STRRET sr; WCHAR buf[MAX_PATH]; int ok = 1; DWORD f[] = { SHGDN_NORMAL, SHGDN_INFOLDER, SHGDN_FORPARSING, SHGDN_INFOLDER | SHGDN_FORPARSING };
      int i;
      for (i = 0; i < 4; i++)
      {
          ok &= SUCCEEDED(IShellFolder2_GetDisplayNameOf(sf, alpha, f[i], &sr)) && sr.uType == STRRET_WSTR &&
                SUCCEEDED(StrRetToBufW(&sr, alpha, buf, MAX_PATH)) && !wcscmp(buf, L"SG Alpha Printer");
      }
      check(ok, "GetDisplayNameOf in four forms");
      hr = IShellFolder2_GetDisplayNameOf(sf, alpha, SHGDN_NORMAL, NULL);
      checkhr(hr, E_INVALIDARG, "GetDisplayNameOf without STRRET");
    }

    /* columns of one printer */
    check(detail_is(sf, alpha, 0, L"SG Alpha Printer"), "column 0 is the name");
    check(detail_is(sf, alpha, 1, L"0"), "column 1 is the document count");
    check(detail_is(sf, alpha, 2, L"Ready"), "column 2 is the status");
    check(detail_is(sf, alpha, 3, L"zzz first"), "column 3 is the comment");
    check(detail_is(sf, alpha, 4, L"Room 1"), "column 4 is the location");
    check(detail_is(sf, alpha, 5, L"Wine PostScript Driver"), "column 5 is the model");
    check(detail_is(sf, beta, 3, L"aaa second") && detail_is(sf, beta, 4, L"Lab"), "the second printer has its own columns");
    { SHELLDETAILS d; hr = IShellFolder2_GetDetailsOf(sf, alpha, 6, &d); checkhr(hr, E_NOTIMPL, "column 6 does not exist"); }

    /* property values */
    scid.fmtid = SG_FMTID_Storage; scid.pid = 10;
    VariantInit(&v);
    hr = IShellFolder2_GetDetailsEx(sf, alpha, &scid, &v);
    check(hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"SG Alpha Printer"), "GetDetailsEx name");
    VariantClear(&v);
    scid.fmtid = SG_FMTID_Summary; scid.pid = 6;
    hr = IShellFolder2_GetDetailsEx(sf, beta, &scid, &v);
    check(hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"aaa second"), "GetDetailsEx comment");
    VariantClear(&v);
    scid.pid = 99;
    hr = IShellFolder2_GetDetailsEx(sf, alpha, &scid, &v);
    checkhr(hr, HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "GetDetailsEx of an unknown property");

    /* ordering */
    hr = IShellFolder2_CompareIDs(sf, 0, alpha, beta);
    check(SUCCEEDED(hr) && (short)HRESULT_CODE(hr) < 0, "Alpha sorts before Beta by name");
    hr = IShellFolder2_CompareIDs(sf, 0, beta, alpha);
    check(SUCCEEDED(hr) && (short)HRESULT_CODE(hr) > 0, "Beta sorts after Alpha by name");
    hr = IShellFolder2_CompareIDs(sf, 0, alpha, alpha);
    check(hr == S_OK, "equal items compare equal");
    hr = IShellFolder2_CompareIDs(sf, 3, alpha, beta);
    check(SUCCEEDED(hr) && (short)HRESULT_CODE(hr) > 0, "by comment Alpha (zzz) sorts after Beta (aaa)");
    hr = IShellFolder2_CompareIDs(sf, 4, alpha, beta);
    check(SUCCEEDED(hr) && (short)HRESULT_CODE(hr) > 0, "by location Room 1 sorts after Lab");
    hr = IShellFolder2_CompareIDs(sf, 1, alpha, beta);
    check(SUCCEEDED(hr) && (short)HRESULT_CODE(hr) < 0, "equal document counts fall back to the name");
    hr = IShellFolder2_CompareIDs(sf, 0, alpha, NULL);
    checkhr(hr, E_INVALIDARG, "CompareIDs with a NULL pidl");
    hr = IShellFolder2_CompareIDs(sf, 9, alpha, beta);
    checkhr(hr, E_INVALIDARG, "CompareIDs by a column that does not exist");

    /* attributes */
    { LPCITEMIDLIST items[2] = { alpha, beta };
      attr = 0xffffffff;
      hr = IShellFolder2_GetAttributesOf(sf, 2, items, &attr);
      check(hr == S_OK && attr == SFGAO_CANLINK, "printer attributes");
      attr = SFGAO_FOLDER | SFGAO_CANLINK | SFGAO_STREAM;
      hr = IShellFolder2_GetAttributesOf(sf, 0, NULL, &attr);
      check(hr == S_OK && attr == (SFGAO_FOLDER | SFGAO_CANLINK), "the folder's own attributes");
      hr = IShellFolder2_GetAttributesOf(sf, 1, items, NULL);
      checkhr(hr, E_INVALIDARG, "GetAttributesOf without a mask");
    }

    /* binding: a printer is not a folder */
    { void *o = (void *)1;
      hr = IShellFolder2_BindToObject(sf, alpha, NULL, &IID_IShellFolder, &o);
      check(hr == E_NOINTERFACE && o == NULL, "BindToObject of a printer");
      o = (void *)1;
      hr = IShellFolder2_BindToStorage(sf, alpha, NULL, &IID_IStream, &o);
      check(hr == E_NOINTERFACE && o == NULL, "BindToStorage of a printer");
      hr = IShellFolder2_BindToObject(sf, NULL, NULL, &IID_IShellFolder, &o);
      checkhr(hr, E_INVALIDARG, "BindToObject without a pidl");
    }

    /* UI objects */
    { LPCITEMIDLIST items[1] = { alpha }; void *o = NULL;
      hr = IShellFolder2_GetUIObjectOf(sf, NULL, 1, items, &IID_IDataObject, NULL, &o);
      check(hr == S_OK && o, "GetUIObjectOf IDataObject");
      if (o) IDataObject_Release((IDataObject *)o);
      o = NULL;
      hr = IShellFolder2_GetUIObjectOf(sf, NULL, 1, items, &SG_IID_ExtractIconW, NULL, &o);
      check(hr == S_OK && o, "GetUIObjectOf IExtractIconW");
      if (o) IUnknown_Release((IUnknown *)o);
      o = (void *)1;
      hr = IShellFolder2_GetUIObjectOf(sf, NULL, 1, items, &IID_IShellView, NULL, &o);
      check(hr == E_NOINTERFACE && o == NULL, "GetUIObjectOf of an interface it has not");
      hr = IShellFolder2_GetUIObjectOf(sf, NULL, 0, NULL, &IID_IDataObject, NULL, &o);
      checkhr(hr, E_INVALIDARG, "GetUIObjectOf without items");
    }

    /* class id */
    IShellFolder2_QueryInterface(sf, &IID_IPersistFolder2, (void **)&pf);
    { CLSID id; memset(&id, 0, sizeof(id));
      hr = IPersistFolder2_GetClassID(pf, &id);
      check(hr == S_OK && IsEqualGUID(&id, &SG_CLSID_Printers), "GetClassID is the Printers folder");
    }
    IPersistFolder2_Release(pf);

    del_printer(L"SG Alpha Printer");
    CoTaskMemFree(alpha); CoTaskMemFree(beta);
    IShellFolder2_Release(sf);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
