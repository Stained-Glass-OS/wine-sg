/* dlldirchild-probe (patches/sg/9757, test/dlldirchild-gate.sh), three programs in one source:
 *   -DHELPER  helper.dll, exporting helper_value()
 *   -DCHILD   child.exe, which imports helper.dll statically and exits with helper_value() (42)
 *   (none)    the parent: starts C:\dd\app\child.exe twice, first with no DLL directory, then after
 *             SetDllDirectoryW(L"C:\\dd\\lib") (where helper.dll is), and prints
 *               PLAIN <exit code>   (Windows: 0xc0000135, the DLL is not found)
 *               DLLDIR <exit code>  (Windows: 42, the child's search path has the parent's DLL directory) */
#include <windows.h>
#include <stdio.h>

#if defined(HELPER)
__declspec(dllexport) int helper_value(void) { return 42; }
#elif defined(CHILD)
__declspec(dllimport) int helper_value(void);
int main(void) { return helper_value(); }
#else
static DWORD run(void)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR cmd[] = L"C:\\dd\\app\\child.exe";
    DWORD code = 0xdeadbeef;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, L"C:\\", &si, &pi)) return GetLastError();
    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

int main(void)
{
    printf("PLAIN %08lx\n", run());
    SetDllDirectoryW(L"C:\\dd\\lib");
    printf("DLLDIR %08lx\n", run());
    return 0;
}
#endif
