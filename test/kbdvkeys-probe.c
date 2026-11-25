/* kbdvkeys-probe KLID|- [Snn|Cxxxx]... (patches/sg/9152, test/kbdvkeys-gate.sh): the layout KLID loads as
 * (LoadKeyboardLayout, not activated; "-": the thread's default layout), then for each Snn the virtual
 * key of scan code nn in it (MapVirtualKeyEx) and for each Cxxxx VkKeyScanEx of character U+xxxx:
 *   HKL 041d041d SC27=c0 SC0c=bb C00f6=00c0 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, WCHAR **argv)
{
    HKL hkl = GetKeyboardLayout(0);
    int i;

    if (argc > 1 && wcscmp(argv[1], L"-") && !(hkl = LoadKeyboardLayoutW(argv[1], 0)))
    {
        printf("NOLAYOUT\n");
        return 1;
    }
    printf("HKL %08lx", (unsigned long)(ULONG_PTR)hkl);
    for (i = 2; i < argc; i++)
    {
        UINT v = wcstoul(argv[i] + 1, NULL, 16);
        if (argv[i][0] == 'S')
            printf(" SC%02x=%02x", v, MapVirtualKeyExW(v, MAPVK_VSC_TO_VK, hkl));
        else
            printf(" C%04x=%04x", v, (unsigned short)VkKeyScanExW((WCHAR)v, hkl));
    }
    printf("\n");
    fflush(stdout);
    return 0;
}
