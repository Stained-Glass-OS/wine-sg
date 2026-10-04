/* openurl-gate.sh's https handler: appends the URL it was given to C:\opened.txt */
#include <windows.h>
#include <stdio.h>
int wmain(int argc, WCHAR **argv)
{
    FILE *f = _wfopen(L"C:\\opened.txt", L"a");
    if (!f) return 1;
    fwprintf(f, L"%ls\n", argc > 1 ? argv[argc - 1] : L"(none)");
    fclose(f);
    return 0;
}
