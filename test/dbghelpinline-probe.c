/* dbghelp reads a PDB whose inline sites it cannot resolve (patches/sg/0634):
 * loads the module given (with its PDB beside it) and prints
 *   symbols=<n enumerated> before=<before_inline found as a function>
 *   after=<after_inline found as a function: the record after the sites>
 */
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
static int n;
static BOOL CALLBACK cb(PSYMBOL_INFO s, ULONG size, void *ctx) { n++; return TRUE; }
int main(int argc, char **argv)
{
    HANDLE p = (HANDLE)(ULONG_PTR)0x1234;
    char buf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO *si = (SYMBOL_INFO *)buf;
    DWORD64 base;
    SymSetOptions(SYMOPT_DEFERRED_LOADS);
    SymInitialize(p, NULL, FALSE);
    base = SymLoadModuleEx(p, NULL, argv[1], NULL, 0x10000000, 0, NULL, 0);
    SymEnumSymbols(p, base, "*", cb, NULL);
    si->SizeOfStruct = sizeof(SYMBOL_INFO); si->MaxNameLen = 255;
    printf("symbols=%d\n", n);
    printf("before=%d\n", SymFromName(p, "before_inline", si) && si->Tag == 5 /* SymTagFunction */);
    printf("after=%d\n", SymFromName(p, "after_inline", si) && si->Tag == 5 /* SymTagFunction */);
    return 0;
}
