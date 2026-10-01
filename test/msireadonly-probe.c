/* msireadonly-probe.exe: a component registered for the machine (as Office's
 * are), its Installer keys readable but not writable by this process (as for
 * a standard user, or once SYSTEM has re-registered it). MsiGetComponentPathW
 * must still find it. Prints "state N path P". (msireadonly-gate.sh, 0701) */
#include <windows.h>
#include <msi.h>
#include <sddl.h>
#include <stdio.h>

static const WCHAR product[] = L"{11111111-2222-3333-4444-555555555555}";
static const WCHAR component[] = L"{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}";

/* msi's packed GUID: each group reversed, the last two groups by byte */
static void squash(const WCHAR *g, WCHAR *out)
{
    static const int map[] = { 8, 7, 6, 5, 4, 3, 2, 1, 13, 12, 11, 10, 18, 17, 16, 15,
                               21, 20, 23, 22, 26, 25, 28, 27, 30, 29, 32, 31, 34, 33, 36, 35 };
    int i;
    for (i = 0; i < 32; i++) out[i] = g[map[i]];
    out[32] = 0;
}

static void read_only(HKEY root, const WCHAR *path)
{
    PSECURITY_DESCRIPTOR sd; HKEY k;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"O:SYD:(A;;KR;;;WD)", SDDL_REVISION_1, &sd, NULL);
    if (!RegOpenKeyExW(root, path, 0, WRITE_DAC | WRITE_OWNER | KEY_WOW64_64KEY, &k)) {
        RegSetKeySecurity(k, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd);
        RegCloseKey(k);
    }
    LocalFree(sd);
}

int wmain(void)
{
    WCHAR sp[33], sc[33], comp[512], props[512], file[MAX_PATH], buf[MAX_PATH];
    DWORD one = 1, n = MAX_PATH;
    HKEY k; HANDLE h; INSTALLSTATE st;
    squash(product, sp); squash(component, sc);
    GetSystemDirectoryW(file, MAX_PATH); lstrcatW(file, L"\\sg-msi-probe.dat");
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    swprintf(comp, 512, L"Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Components\\%ls", sc);
    swprintf(props, 512, L"Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Products\\%ls\\InstallProperties", sp);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, comp, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &k, NULL);
    RegSetValueExW(k, sp, 0, REG_SZ, (BYTE *)file, (lstrlenW(file) + 1) * sizeof(WCHAR));
    RegCloseKey(k);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, props, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &k, NULL);
    RegSetValueExW(k, L"WindowsInstaller", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
    RegCloseKey(k);
    /* before: writable */
    st = MsiGetComponentPathW(product, component, buf, &n);
    printf("writable: state %d path %ls\n", st, n ? buf : L"");
    read_only(HKEY_LOCAL_MACHINE, comp);
    read_only(HKEY_LOCAL_MACHINE, props);
    {
        LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, comp, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &k);
        printf("all access now: %ld\n", r);
        if (!r) RegCloseKey(k);
    }
    n = MAX_PATH; buf[0] = 0;
    st = MsiGetComponentPathW(product, component, buf, &n);
    printf("read-only: state %d path %ls\n", st, buf);
    return 0;
}
