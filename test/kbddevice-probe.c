/* kbddevice-probe KLID (patches/sg/9901, test/kbddevice-gate.sh): the thread's default layout (HKL and
 * name) and what scan codes 0x10 and 0x15 (Q and Y on a US keyboard) are in it, then the same for the
 * layout KLID loads as (LoadKeyboardLayout, not activated):
 *   HKL 04070409 NAME 00000407 SC10 51 SC15 5a
 *   LOADED 04090409 SC10 51 SC15 59 */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    WCHAR name[KL_NAMELENGTH] = {0};
    HKL hkl = GetKeyboardLayout(0), other;

    GetKeyboardLayoutNameW(name);
    printf("HKL %08lx NAME %ls SC10 %02x SC15 %02x\n", (unsigned long)(ULONG_PTR)hkl, name,
           MapVirtualKeyExW(0x10, MAPVK_VSC_TO_VK, hkl), MapVirtualKeyExW(0x15, MAPVK_VSC_TO_VK, hkl));
    if (argc > 1 && (other = LoadKeyboardLayoutW(argv[1], 0)))
        printf("LOADED %08lx SC10 %02x SC15 %02x\n", (unsigned long)(ULONG_PTR)other,
               MapVirtualKeyExW(0x10, MAPVK_VSC_TO_VK, other), MapVirtualKeyExW(0x15, MAPVK_VSC_TO_VK, other));
    fflush(stdout);
    return 0;
}
