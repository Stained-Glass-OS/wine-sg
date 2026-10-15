/* HKLM writes by token (analysis for /var/tmp/stubs-agent/HKLM-TOKEN.md).
 *   hklmtok.exe info | sd | write | restricted */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>

static void info(void)
{
    HANDLE token;
    char buf[512];
    DWORD len;
    PSID admins;
    BOOL member = FALSE;
    SID_IDENTIFIER_AUTHORITY nt = {SECURITY_NT_AUTHORITY};
    LPWSTR str;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    if (GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len) && ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &str))
        printf("USER %ls\n", str);
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &admins);
    CheckTokenMembership(NULL, admins, &member);
    printf("ADMINMEMBER %d\n", member);
    {
        TOKEN_ELEVATION e = {0};
        if (GetTokenInformation(token, TokenElevation, &e, sizeof(e), &len)) printf("ELEVATED %lu\n", e.TokenIsElevated);
    }
    printf("RESTRICTED %d\n", IsTokenRestricted(token));
    if (GetTokenInformation(token, TokenGroups, buf, sizeof(buf), &len))
    {
        TOKEN_GROUPS *g = (TOKEN_GROUPS *)buf;
        DWORD i;
        for (i = 0; i < g->GroupCount; i++)
            if (ConvertSidToStringSidW(g->Groups[i].Sid, &str))
                printf("GROUP %ls attrs %#lx\n", str, g->Groups[i].Attributes);
    }
    CloseHandle(token);
}

static void sd(void)
{
    static const WCHAR *keys[] = { L"Software", L"Software\\Microsoft", L"Software\\Microsoft\\Windows\\CurrentVersion",
        L"Software\\Policies", L"Software\\Wine", L"System\\CurrentControlSet\\Services", L"System\\CurrentControlSet\\Control", NULL };
    int i;
    for (i = 0; keys[i]; i++)
    {
        HKEY k;
        PSECURITY_DESCRIPTOR psd = NULL;
        LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keys[i], 0, READ_CONTROL, &k);
        if (r) { printf("SD %ls open error %ld\n", keys[i], r); continue; }
        r = GetSecurityInfo(k, SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &psd);
        if (r) printf("SD %ls GetSecurityInfo error %ld\n", keys[i], r);
        else
        {
            LPWSTR s = NULL;
            ConvertSecurityDescriptorToStringSecurityDescriptorW(psd, SDDL_REVISION_1, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &s, NULL);
            printf("SD %ls %ls\n", keys[i], s ? s : L"(none)");
        }
        RegCloseKey(k);
    }
}

static void trywrite(const char *label, HKEY root, const WCHAR *path)
{
    HKEY k = NULL;
    DWORD disp = 0, v = 1;
    LONG r = RegCreateKeyExW(root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &disp);
    printf("CREATE %s %ls -> %ld\n", label, path, r);
    if (r == 0)
    {
        r = RegSetValueExW(k, L"SgTok", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
        printf("SETVALUE %s %ls -> %ld\n", label, path, r);
        RegCloseKey(k);
    }
}

static void tryopen(const char *label, HKEY root, const WCHAR *path)
{
    HKEY k = NULL;
    DWORD v = 2;
    LONG r = RegOpenKeyExW(root, path, 0, KEY_SET_VALUE, &k);
    printf("OPENW %s %ls -> %ld\n", label, path, r);
    if (r == 0)
    {
        r = RegSetValueExW(k, L"SgTokOpen", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
        printf("SETVALUE-EXISTING %s %ls -> %ld\n", label, path, r);
        RegDeleteValueW(k, L"SgTokOpen");
        RegCloseKey(k);
    }
}

static void write_tests(void)
{
    trywrite("hklm", HKEY_LOCAL_MACHINE, L"Software\\SgTokProbe");
    trywrite("hklm", HKEY_LOCAL_MACHINE, L"Software\\Policies\\SgTokProbe");
    trywrite("hklm", HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\SgTokProbe");
    tryopen("hklm", HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion");
    tryopen("hklm", HKEY_LOCAL_MACHINE, L"Software\\Microsoft");
    tryopen("hklm", HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion");
    tryopen("hklm", HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    tryopen("hklm", HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Session Manager\\Environment");
    tryopen("hklm", HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Session Manager");
    tryopen("hklm", HKEY_LOCAL_MACHINE, L"Software\\Wine");
    trywrite("hkcu", HKEY_CURRENT_USER, L"Software\\SgTokProbe");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\SgTokProbe");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\Policies\\SgTokProbe");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\SgTokProbe");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\SgTokProbe");
}

static void run_child(HANDLE token, const char *label)
{
    WCHAR cmd[MAX_PATH * 2], self[MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE r, w;
    char buf[4096];
    DWORD n, total = 0;

    CreatePipe(&r, &w, &sa, 0);
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = w; si.hStdError = w; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    GetModuleFileNameW(NULL, self, MAX_PATH);
    swprintf(cmd, ARRAY_SIZE(cmd), L"\"%ls\" info", self);
    wcscat(cmd, L"");
    /* info then write: run both through one child with mode "iw" */
    swprintf(cmd, ARRAY_SIZE(cmd), L"\"%ls\" iw", self);
    if (!CreateProcessAsUserW(token, NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        printf("[%s] CreateProcessAsUser failed %lu\n", label, GetLastError());
        return;
    }
    CloseHandle(w);
    while (ReadFile(r, buf + total, sizeof(buf) - 1 - total, &n, NULL) && n) total += n;
    buf[total] = 0;
    WaitForSingleObject(pi.hProcess, 60000);
    {
        char *p = buf, *e;
        while (*p)
        {
            e = strchr(p, '\n');
            if (e) *e = 0;
            if (strncmp(p, "GROUP", 5)) printf("[%s] %s\n", label, p);
            if (!e) break;
            p = e + 1;
        }
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

static void restricted(void)
{
    HANDLE token, r = NULL;
    SID_IDENTIFIER_AUTHORITY nt = {SECURITY_NT_AUTHORITY};
    PSID admins, users;
    SID_AND_ATTRIBUTES disable, restrict_sid;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &admins);
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0,0,0,0,0,0, &users);
    disable.Sid = admins; disable.Attributes = 0;
    restrict_sid.Sid = users; restrict_sid.Attributes = 0;

    if (CreateRestrictedToken(token, DISABLE_MAX_PRIVILEGE, 1, &disable, 0, NULL, 0, NULL, &r)) { run_child(r, "disable-admins+nopriv"); CloseHandle(r); }
    else printf("CreateRestrictedToken(disable) failed %lu\n", GetLastError());
    r = NULL;
    if (CreateRestrictedToken(token, 0, 0, NULL, 0, NULL, 1, &restrict_sid, &r)) { run_child(r, "restricting-sid=Users"); CloseHandle(r); }
    else printf("CreateRestrictedToken(restrict) failed %lu\n", GetLastError());
    r = NULL;
    if (CreateRestrictedToken(token, WRITE_RESTRICTED, 0, NULL, 0, NULL, 1, &restrict_sid, &r)) { run_child(r, "write-restricted,Users"); CloseHandle(r); }
    else printf("CreateRestrictedToken(write) failed %lu\n", GetLastError());
    r = NULL;
    if (CreateRestrictedToken(token, DISABLE_MAX_PRIVILEGE, 1, &disable, 0, NULL, 1, &restrict_sid, &r)) { run_child(r, "chrome-like"); CloseHandle(r); }
    else printf("CreateRestrictedToken(chrome) failed %lu\n", GetLastError());
}

int main(int argc, char **argv)
{
    const char *m = argc > 1 ? argv[1] : "info";
    if (!strcmp(m, "info")) info();
    else if (!strcmp(m, "sd")) sd();
    else if (!strcmp(m, "write")) write_tests();
    else if (!strcmp(m, "iw")) { info(); write_tests(); }
    else if (!strcmp(m, "disp"))
    {
        /* the display configuration keys are session scratch: a user writes them */
        trywrite("hklm", HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Video\\SgTokProbe");
        trywrite("hklm", HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\GraphicsDrivers\\SgTokProbe");
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Video\\SgTokProbe");
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\GraphicsDrivers\\SgTokProbe");
    }
    else if (!strcmp(m, "restricted")) restricted();
    fflush(stdout);
    return 0;
}
