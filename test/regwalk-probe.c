/* regwalk-probe grant|write: HKLM\Software\SgRegWalk\App in the 32-bit view,
 * as a 64-bit program asks for it (KEY_WOW64_32KEY) -- Steam's client */
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    const char *path = "Software\\SgRegWalk\\App";
    HKEY k; LONG rc; DWORD v = 1;

    if (argc > 1 && !strcmp(argv[1], "grant"))
    {
        PSECURITY_DESCRIPTOR sd; PACL dacl; BOOL present, defaulted;
        rc = RegCreateKeyExA(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_32KEY, NULL, &k, NULL);
        printf("create %ld\n", rc);
        if (rc) return 1;
        /* what Steam's installer sets: full control for the machine's users */
        ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;OICI;GA;;;BU)(A;OICI;GA;;;SY)", SDDL_REVISION_1, &sd, NULL);
        GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted);
        printf("setsec %lu\n", SetSecurityInfo(k, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, dacl, NULL));
        return 0;
    }
    rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_SET_VALUE | KEY_WOW64_32KEY, &k);
    printf("open %ld\n", rc);
    if (!rc) { printf("set %ld\n", RegSetValueExA(k, "SteamPID", 0, REG_DWORD, (BYTE *)&v, 4)); RegCloseKey(k); }
    rc = RegCreateKeyExA(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_WRITE | KEY_WOW64_32KEY, NULL, &k, NULL);
    printf("create %ld\n", rc);
    if (!rc) RegCloseKey(k);
    /* and what the grant does not cover stays shut */
    rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software", 0, KEY_SET_VALUE | KEY_WOW64_32KEY, &k);
    printf("parent %ld\n", rc);
    return 0;
}
