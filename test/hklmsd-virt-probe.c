/* registry virtualization probe (patches/sg/3764): run as a standard user.
 *   regvirt.exe     prints "NAME code" lines */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define R(name, expr) do { LONG _r = (expr); printf("%s %ld\n", name, _r); } while (0)

static LONG getdw(HKEY k, const WCHAR *name, DWORD *v)
{
    DWORD type, size = sizeof(*v);
    return RegQueryValueExW(k, name, NULL, &type, (BYTE *)v, &size);
}

int main(int argc, char **argv)
{
    HKEY k, k2;
    DWORD v = 7, got = 0, nvals = 0, nkeys = 0, idx;
    WCHAR name[64];
    DWORD nlen;
    LONG r;

    printf("BITS %d\n", (int)(sizeof(void *) * 8));
    if (argc > 1 && !strcmp(argv[1], "peek"))
    {
        r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\SgVirtTest", 0, KEY_READ, &k);
        printf("PEEK %ld\n", r);
        if (!r) RegCloseKey(k);
        return 0;
    }
    r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\SgVirtTest", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    printf("CREATE_HKLM_SOFTWARE %ld\n", r);
    if (!r)
    {
        R("SET_VALUE", RegSetValueExW(k, L"v", 0, REG_DWORD, (BYTE *)&v, sizeof(v)));
        RegCloseKey(k);
    }
    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\SgVirtTest", 0, KEY_READ, &k);
    printf("REOPEN_READ %ld\n", r);
    if (!r)
    {
        r = getdw(k, L"v", &got);
        printf("READBACK %ld %lu\n", r, got);
        RegCloseKey(k);
    }
    r = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Wow6432Node\\SgVirtTest", 0, KEY_READ, &k);
    printf("STORE_WOW %ld\n", r);
    if (!r) RegCloseKey(k);
    r = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\SgVirtTest", 0, KEY_READ, &k);
    printf("STORE_PLAIN %ld\n", r);
    if (!r) RegCloseKey(k);

    /* a key that exists on the machine: the write is virtualized, the machine's values stay visible */
    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft", 0, KEY_SET_VALUE, &k);
    printf("OPEN_EXISTING_WRITE %ld\n", r);
    if (!r)
    {
        v = 5;
        R("SET_EXISTING", RegSetValueExW(k, L"SgVirtV", 0, REG_DWORD, (BYTE *)&v, sizeof(v)));
        RegCloseKey(k);
    }
    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft", 0, KEY_READ, &k);
    printf("OPEN_EXISTING_READ %ld\n", r);
    if (!r)
    {
        r = getdw(k, L"SgVirtV", &got);
        printf("READ_VIRTUAL %ld %lu\n", r, got);
        for (idx = 0; ; idx++)
        {
            nlen = 64;
            if (RegEnumKeyExW(k, idx, name, &nlen, NULL, NULL, NULL, NULL)) break;
            nkeys++;
        }
        printf("SUBKEYS_SEEN %d\n", nkeys > 0);
        for (idx = 0; ; idx++)
        {
            nlen = 64;
            if (RegEnumValueW(k, idx, name, &nlen, NULL, NULL, NULL, NULL)) break;
            if (!wcscmp(name, L"SgVirtV")) nvals = 1;
        }
        printf("ENUM_SEES_VIRTUAL %lu\n", nvals);
        RegCloseKey(k);
    }
    /* never virtualized */
    r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\SgVirtTest", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    printf("CREATE_WINDOWSNT %ld\n", r);
    if (!r) RegCloseKey(k);
    r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run\\SgVirtTest", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    printf("CREATE_RUN %ld\n", r);
    if (!r) RegCloseKey(k);
    r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\SgVirtTest", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    printf("CREATE_CLASSES %ld\n", r);
    if (!r) RegCloseKey(k);
    r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\SgVirtTest", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    printf("CREATE_SYSTEM %ld\n", r);
    if (!r) RegCloseKey(k);
    /* below a virtualized handle: a subkey that is only on the machine opens, a new one goes to the store */
    r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\SgVirtTest", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    if (!r)
    {
        r = RegCreateKeyExW(k, L"Child", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k2, NULL);
        printf("CREATE_CHILD %ld\n", r);
        if (!r) { v = 9; R("SET_CHILD", RegSetValueExW(k2, L"c", 0, REG_DWORD, (BYTE *)&v, sizeof(v))); RegCloseKey(k2); }
        RegCloseKey(k);
        r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\SgVirtTest\\Child", 0, KEY_READ, &k);
        printf("REOPEN_CHILD %ld\n", r);
        if (!r) { r = getdw(k, L"c", &got); printf("READ_CHILD %ld %lu\n", r, got); RegCloseKey(k); }
    }
    if (argc > 1 && !strcmp(argv[1], "keep")) return 0;
    /* delete again from the store */
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Wow6432Node\\SgVirtTest\\Child");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Wow6432Node\\SgVirtTest");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\SgVirtTest");
    fflush(stdout);
    return 0;
}
