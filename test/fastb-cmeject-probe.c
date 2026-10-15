/* setupapi: CM_Request_Device_EjectA/W (patch 3762) */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

DEFINE_GUID(CDROM_CLASS, 0x4d36e965, 0xe325, 0x11ce, 0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18);
DEFINE_GUID(CDROM_IFACE, 0x53f56308, 0xb6bf, 0x11d0, 0x94,0xf2,0x00,0xa0,0xc9,0x1e,0xfb,0x8b);
DEFINE_GUID(SYSTEM_CLASS, 0x4d36e97d, 0xe325, 0x11ce, 0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18);

static DEVINST make_device(const WCHAR *id, const GUID *class, DWORD caps, BOOL with_iface)
{
    HDEVINFO set = SetupDiCreateDeviceInfoList(class, NULL);
    SP_DEVINFO_DATA data = { sizeof(data) };
    SP_DEVICE_INTERFACE_DATA iface = { sizeof(iface) };
    DEVINST dn = (DEVINST)-1;

    if (set == INVALID_HANDLE_VALUE) return (DEVINST)-1;
    if (!SetupDiCreateDeviceInfoW(set, id, class, NULL, NULL, DICD_GENERATE_ID, &data)) { printf("note: create %lu\n", GetLastError()); return (DEVINST)-1; }
    SetupDiRegisterDeviceInfo(set, &data, 0, NULL, NULL, NULL);
    SetupDiSetDeviceRegistryPropertyW(set, &data, SPDRP_CAPABILITIES, (BYTE *)&caps, sizeof(caps));
    if (with_iface)
    {
        SetupDiCreateDeviceInterfaceW(set, &data, &CDROM_IFACE, NULL, 0, &iface);
    }
    dn = data.DevInst;
    return dn;
}

int main(void)
{
    DEVINST plain, removable, ejectable, cd_noiface, cd_iface, root;
    PNP_VETO_TYPE veto;
    WCHAR name[MAX_PATH];
    char nameA[MAX_PATH];
    CONFIGRET cr;

    plain = make_device(L"FASTBPLAIN", &SYSTEM_CLASS, 0, FALSE);
    removable = make_device(L"FASTBREMOVABLE", &SYSTEM_CLASS, CM_DEVCAP_REMOVABLE, FALSE);
    ejectable = make_device(L"FASTBEJECTABLE", &SYSTEM_CLASS, CM_DEVCAP_EJECTSUPPORTED | CM_DEVCAP_REMOVABLE, FALSE);
    cd_noiface = make_device(L"FASTBCDNOIFACE", &CDROM_CLASS, CM_DEVCAP_EJECTSUPPORTED | CM_DEVCAP_REMOVABLE, FALSE);
    cd_iface = make_device(L"FASTBCDIFACE", &CDROM_CLASS, CM_DEVCAP_EJECTSUPPORTED | CM_DEVCAP_REMOVABLE, TRUE);
    CHECK(plain != (DEVINST)-1 && removable != (DEVINST)-1 && ejectable != (DEVINST)-1 && cd_noiface != (DEVINST)-1 && cd_iface != (DEVINST)-1, "devices: %lx %lx %lx %lx %lx", plain, removable, ejectable, cd_noiface, cd_iface);
    if (plain == (DEVINST)-1 || removable == (DEVINST)-1 || ejectable == (DEVINST)-1 || cd_noiface == (DEVINST)-1 || cd_iface == (DEVINST)-1) { printf("RESULT: FAIL\n"); return 1; }

    /* arguments */
    veto = (PNP_VETO_TYPE)99; name[0] = 'x';
    CHECK(CM_Request_Device_EjectW(0x7f7f7f7f, &veto, name, MAX_PATH, 0) == CR_INVALID_DEVNODE, "unknown device node");
    CHECK(CM_Request_Device_EjectW(0x7fff0000, &veto, name, MAX_PATH, 0) == CR_INVALID_DEVNODE, "the root of the tree can not be ejected");
    CHECK(CM_Request_Device_EjectW(removable, &veto, name, MAX_PATH, 1) == CR_INVALID_FLAG, "flags must be 0");
    CHECK(CM_Request_Device_EjectW(removable, &veto, name, 0, 0) == CR_INVALID_POINTER, "a name buffer of 0 characters");
    CHECK(CM_Request_Device_EjectA(removable, &veto, nameA, 0, 0) == CR_INVALID_POINTER, "a name buffer of 0 characters (A)");

    /* a device that can neither be ejected nor removed */
    veto = (PNP_VETO_TYPE)99; name[0] = 'x';
    cr = CM_Request_Device_EjectW(plain, &veto, name, MAX_PATH, 0);
    CHECK(cr == CR_REMOVE_VETOED, "non-removable: %lx", cr);
    CHECK(veto == PNP_VetoNonDisableable, "veto type %d", veto);
    CHECK(!wcsicmp(name, L"ROOT\\FASTBPLAIN\\0000"), "veto name %ls", name);
    cr = CM_Request_Device_EjectW(plain, NULL, NULL, 0, 0);
    CHECK(cr == CR_REMOVE_VETOED, "no output arguments: %lx", cr);
    memset(nameA, 'x', sizeof(nameA));
    veto = (PNP_VETO_TYPE)99;
    cr = CM_Request_Device_EjectA(plain, &veto, nameA, sizeof(nameA), 0);
    CHECK(cr == CR_REMOVE_VETOED && veto == PNP_VetoNonDisableable && strstr(nameA, "FASTBPLAIN"), "A: %lx %d '%s'", cr, veto, nameA);
    nameA[0] = 'x'; nameA[1] = 'y'; nameA[2] = 0;
    cr = CM_Request_Device_EjectA(plain, &veto, nameA, 4, 0);
    CHECK(cr == CR_REMOVE_VETOED && strlen(nameA) <= 3, "A with a small buffer: '%s'", nameA);

    /* removable devices: success, the veto type reset */
    veto = (PNP_VETO_TYPE)99; wcscpy(name, L"stale");
    cr = CM_Request_Device_EjectW(removable, &veto, name, MAX_PATH, 0);
    CHECK(cr == CR_SUCCESS && veto == PNP_VetoTypeUnknown && !name[0], "removable: %lx veto %d name '%ls'", cr, veto, name);
    cr = CM_Request_Device_EjectW(ejectable, &veto, name, MAX_PATH, 0);
    CHECK(cr == CR_SUCCESS, "ejectable: %lx", cr);
    cr = CM_Request_Device_EjectA(ejectable, NULL, NULL, 0, 0);
    CHECK(cr == CR_SUCCESS, "ejectable, no outputs: %lx", cr);

    /* a CD-ROM drive: there is no drive behind it here, so the request is vetoed with the device */
    veto = (PNP_VETO_TYPE)99; name[0] = 0;
    cr = CM_Request_Device_EjectW(cd_noiface, &veto, name, MAX_PATH, 0);
    CHECK(cr == CR_REMOVE_VETOED && veto == PNP_VetoDevice && wcsstr(name, L"CDNOIFACE"), "cd, no interface: %lx %d '%ls'", cr, veto, name);
    veto = (PNP_VETO_TYPE)99; name[0] = 0;
    cr = CM_Request_Device_EjectW(cd_iface, &veto, name, MAX_PATH, 0);
    CHECK(cr == CR_REMOVE_VETOED && (veto == PNP_VetoDevice || veto == PNP_VetoOutstandingOpen), "cd, interface that does not open: %lx %d", cr, veto);

    root = 0;
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
