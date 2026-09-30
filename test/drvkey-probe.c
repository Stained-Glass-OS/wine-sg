/* A standard user may read a device's driver key (patches/sg/0580).
 * Locks HKLM\System\CurrentControlSet\Control\Class to read-only for everyone
 * -- as HKLM is to a standard user -- then opens a present device's driver key
 * read-only with SetupDiOpenDevRegKey. Prints "drvkey=ok", "drvkey=denied" or
 * "drvkey=nodevice". */
#include <windows.h>
#include <setupapi.h>
#include <aclapi.h>
#include <stdio.h>

int main(void)
{
    HDEVINFO set;
    SP_DEVINFO_DATA dev = { sizeof(dev) };
    HKEY cls, key;
    EXPLICIT_ACCESS_W ea = { 0 };
    PACL acl = NULL;
    DWORD i, err = 0;
    int found = 0;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Class", 0,
                      WRITE_DAC | READ_CONTROL, &cls)) { printf("drvkey=noclass\n"); return 1; }
    ea.grfAccessPermissions = KEY_READ;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea.Trustee.ptstrName = (WCHAR *)L"EVERYONE";
    SetEntriesInAclW(1, &ea, NULL, &acl);
    SetSecurityInfo(cls, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                    NULL, NULL, acl, NULL);
    RegCloseKey(cls);

    set = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    for (i = 0; SetupDiEnumDeviceInfo(set, i, &dev); i++)
    {
        WCHAR drv[128];
        if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_DRIVER, NULL, (BYTE *)drv, sizeof(drv), NULL))
            continue;
        found = 1;
        key = SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
        if (key != INVALID_HANDLE_VALUE) { RegCloseKey(key); printf("drvkey=ok %ls\n", drv); return 0; }
        err = GetLastError();
    }
    printf(found ? "drvkey=denied %lu\n" : "drvkey=nodevice\n", err);
    return 1;
}
