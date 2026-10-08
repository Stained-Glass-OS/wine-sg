/* Small stubs (patches/sg/1608).
 *
 *  - RtlGetDeviceFamilyInfoEnum gives the version (major, minor, build,
 *    revision in 16-bit fields) -- it gave 0 -- and the desktop family;
 *  - EtwEventSetInformation checks the provider traits and descriptor type
 *    as Windows does and refuses classes it does not know;
 *  - GetKeyboardLayout of another existing thread gives the layout, and of
 *    a thread that does not exist 0.
 */
#include <windows.h>
#include <stdio.h>

typedef void (WINAPI *devfam_t)(ULONGLONG *, DWORD *, DWORD *);
typedef ULONG (WINAPI *etwset_t)(ULONGLONG, int, void *, ULONG);
typedef LONG (WINAPI *rtlver_t)(OSVERSIONINFOW *);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static DWORD WINAPI idle(void *arg)
{
    WaitForSingleObject(arg, INFINITE);
    return 0;
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    devfam_t pRtlGetDeviceFamilyInfoEnum = (void *)GetProcAddress(ntdll, "RtlGetDeviceFamilyInfoEnum");
    etwset_t pEtwEventSetInformation = (void *)GetProcAddress(ntdll, "EtwEventSetInformation");
    rtlver_t pRtlGetVersion = (void *)GetProcAddress(ntdll, "RtlGetVersion");
    OSVERSIONINFOW ver = { sizeof(ver) };
    ULONGLONG uap = 0;
    DWORD family = 0, form = 99, tid;
    BYTE traits[8] = { 8, 0, 'a', 'p', 'p', 0, 0, 0 };
    BOOLEAN flag = TRUE;
    HANDLE stop, thread;
    HKL mine, other, none;

    pRtlGetVersion(&ver);
    pRtlGetDeviceFamilyInfoEnum(&uap, &family, &form);
    printf("device family %lu form %lu version %llx (%lu.%lu.%lu)\n", family, form, uap,
           ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber);
    check((uap >> 48) == ver.dwMajorVersion && ((uap >> 32) & 0xffff) == ver.dwMinorVersion &&
          ((uap >> 16) & 0xffff) == (ver.dwBuildNumber & 0xffff), "RtlGetDeviceFamilyInfoEnum gives the version");
    check(family == 3, "... and the desktop family");

    check(pEtwEventSetInformation(1, 2, traits, sizeof(traits)) == ERROR_SUCCESS, "EtwEventSetInformation takes provider traits");
    traits[0] = 20;
    check(pEtwEventSetInformation(1, 2, traits, sizeof(traits)) == ERROR_INVALID_PARAMETER, "... and refuses traits whose size is wrong");
    check(pEtwEventSetInformation(1, 3, &flag, sizeof(flag)) == ERROR_SUCCESS, "it takes the descriptor type");
    check(pEtwEventSetInformation(1, 3, &flag, 4) == ERROR_INVALID_PARAMETER, "... of a BOOLEAN's size only");
    check(pEtwEventSetInformation(1, 9, &flag, sizeof(flag)) == ERROR_NOT_SUPPORTED, "an unknown class is not supported");

    stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, idle, stop, 0, &tid);
    mine = GetKeyboardLayout(0);
    other = GetKeyboardLayout(tid);
    printf("layouts: mine %p, other thread's %p\n", mine, other);
    check(other && other == mine, "GetKeyboardLayout of another thread gives the layout");
    SetEvent(stop);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    none = GetKeyboardLayout(0x7ffffff0);
    check(!none, "... and of a thread that does not exist, 0");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
