/* kbdklf-probe (patches/sg/9163, test/kbdklf-gate.sh): ActivateKeyboardLayout's flags.
 *   LIST hkl...      GetKeyboardLayoutList after loading German and French
 *   REORDER hkl...   the list after ActivateKeyboardLayout(French, KLF_REORDER)
 *   ATTR n           HKCU\Keyboard Layout "Attributes" after KLF_RESET | KLF_SHIFTLOCK, then after KLF_RESET */
#include <windows.h>
#include <stdio.h>

static void list(const char *what)
{
    HKL l[16];
    UINT n = GetKeyboardLayoutList(16, l), i;
    printf("%s", what);
    for (i = 0; i < n; i++) printf(" %08lx", (unsigned long)(ULONG_PTR)l[i]);
    printf("\n");
}

static void attr(void)
{
    DWORD v = 0xdead, size = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Keyboard Layout", L"Attributes", RRF_RT_REG_DWORD, NULL, &v, &size);
    printf("ATTR %lx\n", (unsigned long)v);
}

int main(void)
{
    HKL de = LoadKeyboardLayoutW(L"00000407", 0), fr = LoadKeyboardLayoutW(L"0000040c", 0);
    printf("LOADED %08lx %08lx\n", (unsigned long)(ULONG_PTR)de, (unsigned long)(ULONG_PTR)fr);
    list("LIST");
    printf("ACT %d\n", ActivateKeyboardLayout(fr, KLF_REORDER) != 0);
    list("REORDER");
    ActivateKeyboardLayout(de, KLF_RESET | KLF_SHIFTLOCK);
    attr();
    ActivateKeyboardLayout(de, KLF_RESET);
    attr();
    fflush(stdout);
    return 0;
}
