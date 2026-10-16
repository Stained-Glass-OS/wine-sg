#include <windows.h>
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
    HMODULE m = LoadLibraryA("prntvpt.dll"), w = LoadLibraryA("winspool.drv");
    HRESULT (WINAPI *q)(const WCHAR *, DWORD *) = (void *)GetProcAddress(m, "PTQuerySchemaVersionSupport");
    BOOL (WINAPI *defp)(WCHAR *, DWORD *) = (void *)GetProcAddress(w, "GetDefaultPrinterW");
    WCHAR name[256]; DWORD len = 256, ver = 99; HRESULT hr;
    CHECK(q != NULL, "export");
    hr = q(NULL, &ver); CHECK(hr == E_INVALIDARG, "NULL printer %08lx", hr);
    hr = q(L"No Such Printer", NULL); CHECK(hr == E_INVALIDARG, "NULL version %08lx", hr);
    ver = 99; hr = q(L"No Such Printer", &ver);
    CHECK(FAILED(hr) && ver == 99, "unknown printer %08lx ver %lu", hr, ver);
    if (defp(name, &len))
    {
        hr = q(name, &ver); CHECK(hr == S_OK && ver == 1, "default printer %08lx ver %lu", hr, ver);
    }
    else
    {
        /* a queue known only by its registry key, no real printer */
        HKEY k;
        if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Print\\Printers\\SGPrnTest", 0, NULL, 0,
                             KEY_ALL_ACCESS, NULL, &k, NULL))
        {
            RegSetValueExW(k, L"Name", 0, REG_SZ, (BYTE *)L"SGPrnTest", sizeof(L"SGPrnTest"));
            RegSetValueExW(k, L"Port", 0, REG_SZ, (BYTE *)L"FILE:", sizeof(L"FILE:"));
            RegSetValueExW(k, L"Print Processor", 0, REG_SZ, (BYTE *)L"winprint", sizeof(L"winprint"));
            RegSetValueExW(k, L"Datatype", 0, REG_SZ, (BYTE *)L"RAW", sizeof(L"RAW"));
            RegCloseKey(k);
            ver = 99; hr = q(L"SGPrnTest", &ver); CHECK(hr == S_OK && ver == 1, "registered printer %08lx ver %lu", hr, ver);
        }
        else printf("note: no registry access\n");
    }
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
