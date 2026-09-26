/* sandboxdesk-probe: what Firefox's sandbox does for a content process -- an
 * alternate window station and desktop, a restricted low-integrity token -- and
 * a child started there that makes a window. For sandboxdesk-gate.sh.
 *   sandboxdesk-probe.exe OUTFILE      (the parent; the child writes OUTFILE)
 *   sandboxdesk-probe.exe OUTFILE child */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    WCHAR cmd[2 * MAX_PATH + 32], outw[MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HANDLE tok, rtok;
    SID_AND_ATTRIBUTES restrict_sid[1];
    TOKEN_MANDATORY_LABEL label;
    PSID restricted, low;
    HWINSTA ws, old;
    HDESK desk;
    DWORD code = 99;
    FILE *f;

    if (argc < 2) return 2;
    if (argc > 2)
    {   /* the child, sandboxed */
        HWND w = CreateWindowW(L"STATIC", L"sandboxed", WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, NULL, NULL);
        if ((f = fopen(argv[1], "w"))) { fprintf(f, "CHILD_WINDOW %d\n", w != NULL); fclose(f); }
        if (w) DestroyWindow(w);
        return w ? 0 : 1;
    }
    old = GetProcessWindowStation();
    ws = CreateWindowStationW(L"sg_sbox_winsta", 0, GENERIC_ALL, NULL);
    if (!ws) { printf("NO_WINSTA %lu\n", GetLastError()); return 1; }
    SetProcessWindowStation(ws);
    desk = CreateDesktopW(L"sg_sbox_desktop", NULL, NULL, 0, GENERIC_ALL, NULL);
    SetProcessWindowStation(old);
    if (!desk) { printf("NO_DESKTOP %lu\n", GetLastError()); return 1; }

    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tok);
    ConvertStringSidToSidW(L"S-1-5-12", &restricted);          /* RESTRICTED */
    restrict_sid[0].Sid = restricted; restrict_sid[0].Attributes = 0;
    if (!CreateRestrictedToken(tok, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 1, restrict_sid, &rtok))
    { printf("NO_TOKEN %lu\n", GetLastError()); return 1; }
    ConvertStringSidToSidW(L"S-1-16-4096", &low);                /* low integrity */
    label.Label.Sid = low; label.Label.Attributes = SE_GROUP_INTEGRITY;
    SetTokenInformation(rtok, TokenIntegrityLevel, &label, sizeof(label) + GetLengthSid(low));

    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, outw, MAX_PATH);
    GetModuleFileNameW(NULL, cmd, MAX_PATH);
    lstrcatW(cmd, L" "); lstrcatW(cmd, outw); lstrcatW(cmd, L" child");
    si.lpDesktop = L"sg_sbox_winsta\\sg_sbox_desktop";
    if (!CreateProcessAsUserW(rtok, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    { printf("NO_CHILD %lu\n", GetLastError()); return 1; }
    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &code);
    printf("CHILD_EXIT %lu\n", code);
    fflush(stdout);
    return 0;
}
