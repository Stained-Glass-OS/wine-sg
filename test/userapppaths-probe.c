/* userapppaths-gate.sh's program: records how it was started (C:\started.txt) */
#include <windows.h>
#include <stdio.h>
int wmain(int argc, WCHAR **argv)
{
    FILE *f = _wfopen(L"C:\\started.txt", L"a");
    if (!f) return 1;
    fwprintf(f, L"%ls\n", wcschr(GetCommandLineW() + 1, '"') ? wcschr(GetCommandLineW() + 1, '"') + 1 : GetCommandLineW());
    fclose(f);
    (void)argc; (void)argv;
    return 0;
}
