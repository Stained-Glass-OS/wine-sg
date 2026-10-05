/* A standard user reads a device's own key (patches/sg/0872).
 * Locks HKLM\System\CurrentControlSet\Enum to read-only for everyone -- as
 * HKLM is to a standard user -- then reads every present device's hardware
 * IDs and a device property through setupapi. Prints "devkey=ok N",
 * "devkey=denied ERR" or "devkey=nodevice". */
#include <initguid.h>
#include <windows.h>
#include <setupapi.h>
#include <devpkey.h>
#include <aclapi.h>
#include <stdio.h>

int main(void)
{
    HDEVINFO set;
    SP_DEVINFO_DATA dev = { sizeof(dev) };
    HKEY root;
    EXPLICIT_ACCESS_W ea = { 0 };
    PACL acl = NULL;
    DWORD i, err = 0, n = 0, type;
    int found = 0, bad = 0;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Enum", 0,
                      WRITE_DAC | READ_CONTROL, &root)) { printf("devkey=noenum\n"); return 1; }
    ea.grfAccessPermissions = KEY_READ;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea.Trustee.ptstrName = (WCHAR *)L"EVERYONE";
    SetEntriesInAclW(1, &ea, NULL, &acl);
    SetSecurityInfo(root, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                    NULL, NULL, acl, NULL);
    RegCloseKey(root);

    set = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    for (i = 0; SetupDiEnumDeviceInfo(set, i, &dev); i++)
    {
        WCHAR ids[1024], desc[256];
        DEVPROPTYPE ptype;
        found = 1;
        if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, &type, (BYTE *)ids, sizeof(ids), NULL))
        {
            err = GetLastError();
            /* a device without hardware IDs answers ERROR_INVALID_DATA; a key that
             * could not be opened answers ERROR_INVALID_HANDLE */
            if (err == ERROR_INVALID_HANDLE) bad++;
            continue;
        }
        if (!SetupDiGetDevicePropertyW(set, &dev, &DEVPKEY_Device_DeviceDesc, &ptype, (BYTE *)desc, sizeof(desc), NULL, 0)
            && GetLastError() == ERROR_INVALID_HANDLE) { bad++; err = ERROR_INVALID_HANDLE; continue; }
        n++;
    }
    if (!found) { printf("devkey=nodevice\n"); return 1; }
    if (bad || !n) { printf("devkey=denied %lu (%d unreadable, %lu read)\n", err, bad, n); return 1; }
    printf("devkey=ok %lu\n", n);
    return 0;
}
