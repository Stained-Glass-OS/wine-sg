/* deskread probe (patch 5953): a program the shell starts can read its own desktop's security.
 * Started by explorer /desktop=NAME (as the Stained Glass session starts the shell, and the shell
 * every program of the Run keys, Start and the desktop), it inherits the shell's desktop handle.
 * Chromium's sandbox (Dropbox 274, Electron) reads the desktop's DACL to make its alternate
 * desktop and CHECKs that it can: the handle lacked READ_CONTROL and every start crashed
 * (SBOX_ERROR_CANNOT_QUERY_DESKTOP_SECURITY, qa4 2026-10-22).
 *
 *   explorer /desktop=sgdeskread,800x600 C:\deskread-probe.exe   -> C:\deskread.txt */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>

typedef LONG (WINAPI *qo_t)(HANDLE, int, void *, ULONG, ULONG *);

int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR c, int d)
{
    FILE *out = fopen("C:\\deskread.txt", "w");
    HDESK desk = GetThreadDesktop(GetCurrentThreadId());
    PSECURITY_DESCRIPTOR sd = NULL;
    struct { ULONG Attributes; ACCESS_MASK GrantedAccess; ULONG pad[20]; } info = {0};
    qo_t qo = (qo_t)GetProcAddress(GetModuleHandleA("ntdll"), "NtQueryObject");
    WCHAR name[64] = {0};
    DWORD len, r;
    int fails = 0;

    if (!out) return 1;
    GetUserObjectInformationW(desk, UOI_NAME, name, sizeof(name), &len);
    qo(desk, 0, &info, sizeof(info), &len);
    fprintf(out, "# desktop '%ls' handle %p access 0x%lx\n", name, desk, info.GrantedAccess);
    if (wcscmp(name, L"sgdeskread")) { fprintf(out, "FAIL  not on the shell's desktop: '%ls'\n", name); fails++; }
    if ((info.GrantedAccess & READ_CONTROL) == READ_CONTROL)
        fprintf(out, "PASS  the desktop handle has READ_CONTROL (0x%lx)\n", info.GrantedAccess);
    else { fprintf(out, "FAIL  the desktop handle lacks READ_CONTROL (0x%lx)\n", info.GrantedAccess); fails++; }
    r = GetSecurityInfo(desk, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &sd);
    if (!r && sd) fprintf(out, "PASS  GetSecurityInfo on the desktop (Chromium's sandbox needs it)\n");
    else { fprintf(out, "FAIL  GetSecurityInfo on the desktop: %lu\n", r); fails++; }
    /* the rights a program has on its desktop are still the desktop's own: no WRITE_OWNER beyond what
       the creator had */
    fprintf(out, "RESULT: %s\n", fails ? "FAIL" : "PASS");
    fclose(out);
    return 0;
}
