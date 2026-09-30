/* Starting a Unix program leaks nothing (patches/sg/0584).
 * Starts /bin/true 50 times and compares this process's handle count and its
 * open Unix file descriptors (\\?\unix\proc\self\fd) before and after.
 * Prints "handles=<growth> fds=<growth>". */
#include <windows.h>
#include <stdio.h>

static int count_fds(void)
{
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(L"\\\\?\\unix\\proc\\self\\fd\\*", &fd); int n = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    do { if (fd.cFileName[0] != '.') n++; } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

int main(void)
{
    DWORD before, after; int fds_before, fds_after, i;
    Sleep(500);
    GetProcessHandleCount(GetCurrentProcess(), &before);
    fds_before = count_fds();
    for (i = 0; i < 50; i++)
    {
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi;
        WCHAR cmd[] = L"\\\\?\\unix\\bin\\true";
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        {
            if (pi.hThread) CloseHandle(pi.hThread);
            if (pi.hProcess) CloseHandle(pi.hProcess);
        }
    }
    Sleep(500);
    GetProcessHandleCount(GetCurrentProcess(), &after);
    fds_after = count_fds();
    printf("handles=%ld fds=%d\n", (long)after - (long)before, fds_after - fds_before);
    return 0;
}
