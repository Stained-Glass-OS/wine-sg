/* dwmdefaults-probe (patches/sg/9756, test/dwmdefaults-gate.sh): a new user's DWM colour settings.
 * Prints:
 *   REG <name> <hex|none>     HKCU\Software\Microsoft\Windows\DWM values
 *   DWMAPI <hr> <hex>         DwmGetColorizationColor */
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *DGCC)(DWORD *, BOOL *);

int main(void)
{
    static const WCHAR *names[] = { L"AccentColor", L"ColorizationColor", L"ColorizationAfterglow",
                                    L"ColorizationColorBalance", L"ColorPrevalence", L"EnableAeroPeek" };
    DGCC dgcc = (DGCC)GetProcAddress(LoadLibraryA("dwmapi.dll"), "DwmGetColorizationColor");
    DWORD v, size, c = 0;
    BOOL opaque;
    unsigned i;
    HRESULT hr;

    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        size = sizeof(v);
        if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", names[i], RRF_RT_REG_DWORD, NULL, &v, &size))
            printf("REG %ls %08lx\n", names[i], v);
        else
            printf("REG %ls none\n", names[i]);
    }
    hr = dgcc ? dgcc(&c, &opaque) : E_FAIL;
    printf("DWMAPI %08lx %08lx\n", hr, c);
    return 0;
}
