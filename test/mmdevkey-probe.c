/* A standard user's audio endpoints have properties (patches/sg/0581).
 *   mmdevkey-probe lock    make HKLM\...\MMDevices\Audio read-only for everyone,
 *                          as HKLM is to a standard user
 *   mmdevkey-probe check   in a new process: every active endpoint's property
 *                          store opens and names the device
 * Prints "mmdev=ok N", "mmdev=noprops ..." or "mmdev=nodevices". */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <aclapi.h>
#include <stdio.h>

static int lock(void)
{
    HKEY key;
    EXPLICIT_ACCESS_W ea = { 0 };
    PACL acl = NULL;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio",
                        0, NULL, 0, WRITE_DAC | READ_CONTROL, NULL, &key, NULL)) { printf("mmdev=nokey\n"); return 1; }
    ea.grfAccessPermissions = KEY_READ;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea.Trustee.ptstrName = (WCHAR *)L"EVERYONE";
    SetEntriesInAclW(1, &ea, NULL, &acl);
    if (SetSecurityInfo(key, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                        NULL, NULL, acl, NULL)) { printf("mmdev=nolock\n"); return 1; }
    printf("mmdev=locked\n");
    return 0;
}

static int check(void)
{
    IMMDeviceEnumerator *en;
    IMMDeviceCollection *col;
    UINT n = 0, i, named = 0;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IMMDeviceEnumerator, (void **)&en))) { printf("mmdev=noenum\n"); return 1; }
    if (FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eAll, DEVICE_STATE_ACTIVE, &col))) { printf("mmdev=noenum\n"); return 1; }
    IMMDeviceCollection_GetCount(col, &n);
    if (!n) { printf("mmdev=nodevices\n"); return 2; }
    for (i = 0; i < n; i++)
    {
        IMMDevice *dev;
        IPropertyStore *ps;
        PROPVARIANT pv;
        if (FAILED(IMMDeviceCollection_Item(col, i, &dev))) continue;
        if (SUCCEEDED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &ps)))
        {
            PropVariantInit(&pv);
            if (SUCCEEDED(IPropertyStore_GetValue(ps, &PKEY_Device_FriendlyName, &pv))
                && pv.vt == VT_LPWSTR) named++;
            PropVariantClear(&pv);
            IPropertyStore_Release(ps);
        }
        IMMDevice_Release(dev);
    }
    printf(named == n ? "mmdev=ok %u\n" : "mmdev=noprops %u of %u named\n", named, n);
    return named == n ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "lock")) return lock();
    return check();
}
