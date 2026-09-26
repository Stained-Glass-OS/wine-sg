/* The custom actions of msica-gate.sh's package (0304):
 * Throw raises a C++ exception out of the custom action, as Foxit PDF
 * Reader's installer does; Mark, sequenced after it, leaves C:\ca-marker.txt. */
#include <windows.h>
#include <msiquery.h>

UINT __stdcall Throw(MSIHANDLE install)
{
    ULONG_PTR args[3] = {0x19930520, 0, 0};
    RaiseException(0xe06d7363, EXCEPTION_NONCONTINUABLE, 3, args);
    return ERROR_SUCCESS;
}

UINT __stdcall Mark(MSIHANDLE install)
{
    HANDLE f = CreateFileA("C:\\ca-marker.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written;

    if (f == INVALID_HANDLE_VALUE) return ERROR_INSTALL_FAILURE;
    WriteFile(f, "marked", 6, &written, NULL);
    CloseHandle(f);
    return ERROR_SUCCESS;
}
