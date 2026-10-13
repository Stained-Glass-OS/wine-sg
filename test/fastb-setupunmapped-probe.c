#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
static DWORD test(HDEVINFO set, SP_DEVINFO_DATA *dev, DWORD prop, BOOL wide)
{
    BYTE buf[64];
    BOOL ret;
    SetLastError(0xdeadbeef);
    if (wide) ret = SetupDiGetDeviceRegistryPropertyW(set, dev, prop, NULL, buf, sizeof(buf), NULL);
    else ret = SetupDiGetDeviceRegistryPropertyA(set, dev, prop, NULL, buf, sizeof(buf), NULL);
    return ret ? 0 : GetLastError();
}
int main(void)
{
    GUID guid = {0x1234abcd, 0x1, 0x2, {3, 4, 5, 6, 7, 8, 9, 10}};
    SP_DEVINFO_DATA dev = {sizeof(dev)};
    HDEVINFO set = SetupDiGetClassDevsA(&guid, NULL, 0, DIGCF_DEVICEINTERFACE);
    int w;

    SetupDiCreateDeviceInfoA(set, "LEGACY_UNMAPPED", &guid, NULL, NULL, DICD_GENERATE_ID, &dev);
    for (w = 0; w < 2; ++w)
    {
        const char *c = w ? "W" : "A";
        /* known to Windows, absent from the device: ERROR_INVALID_DATA */
        printf("devtype%s=%lu\n", c, test(set, &dev, SPDRP_DEVTYPE, w));
        printf("security%s=%lu\n", c, test(set, &dev, SPDRP_SECURITY, w));
        printf("removal%s=%lu\n", c, test(set, &dev, SPDRP_REMOVAL_POLICY, w));
        printf("enumerator%s=%lu\n", c, test(set, &dev, SPDRP_ENUMERATOR_NAME, w));
        /* unused or unknown numbers: ERROR_INVALID_REG_PROPERTY */
        printf("unused0%s=%lu\n", c, test(set, &dev, SPDRP_UNUSED0, w));
        printf("unused2%s=%lu\n", c, test(set, &dev, SPDRP_UNUSED2, w));
        printf("max%s=%lu\n", c, test(set, &dev, 0x25, w));
        printf("minus1%s=%lu\n", c, test(set, &dev, -1, w));
    }
    SetupDiDestroyDeviceInfoList(set);
    return 0;
}
