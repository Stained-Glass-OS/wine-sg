/* Device interfaces through the configuration manager (patches/sg/1697),
 * run by test/cmifaces-gate.sh: CM_Get_Device_Interface_List (and _Size)
 * were stubs that failed, CM_Get_Device_Interface_Property returned
 * CR_CALL_NOT_IMPLEMENTED, SetupDiOpenDeviceInterface was a FIXME,
 * CM_Get_Device_Interface_Alias and SetupDiGetDeviceInterfaceAlias,
 * CM_Register_Device_Interface and CM_Unregister_Device_Interface were
 * stubs, and the interface property calls (SetupDi and CM) were missing.
 * The probe makes a root device with interfaces of three classes. */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID class_a = { 0x5a9e0001, 0x5347, 0x4f53, { 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x97 } };
static const GUID class_b = { 0x5a9e0002, 0x5347, 0x4f53, { 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x97 } };
static const GUID class_c = { 0x5a9e0003, 0x5347, 0x4f53, { 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x97 } };
static const GUID setup_class = { 0x4d36e97d, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
static const DEVPROPKEY key_instance = { { 0x78c34fc8, 0x104a, 0x4aca, { 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57 } }, 256 };
static const DEVPROPKEY key_friendly = { { 0x026e516e, 0xb814, 0x414b, { 0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22 } }, 2 };
static const DEVPROPKEY key_enabled = { { 0x026e516e, 0xb814, 0x414b, { 0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22 } }, 3 };
static const DEVPROPKEY key_class = { { 0x026e516e, 0xb814, 0x414b, { 0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22 } }, 4 };
static const DEVPROPKEY key_other = { { 0x5a9e00ff, 0x5347, 0x4f53, { 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x97 } }, 7 };

typedef CONFIGRET (WINAPI *list_size_w)(ULONG *, GUID *, WCHAR *, ULONG);
typedef CONFIGRET (WINAPI *list_w)(GUID *, WCHAR *, WCHAR *, ULONG, ULONG);
typedef CONFIGRET (WINAPI *list_a)(GUID *, char *, char *, ULONG, ULONG);
typedef CONFIGRET (WINAPI *alias_w)(const WCHAR *, GUID *, WCHAR *, ULONG *, ULONG);
typedef CONFIGRET (WINAPI *get_prop)(const WCHAR *, const DEVPROPKEY *, DEVPROPTYPE *, BYTE *, ULONG *, ULONG);
typedef CONFIGRET (WINAPI *set_prop)(const WCHAR *, const DEVPROPKEY *, DEVPROPTYPE, const BYTE *, ULONG, ULONG);
typedef CONFIGRET (WINAPI *prop_keys)(const WCHAR *, DEVPROPKEY *, ULONG *, ULONG);
typedef CONFIGRET (WINAPI *reg_iface)(DEVINST, GUID *, const WCHAR *, WCHAR *, ULONG *, ULONG);
typedef CONFIGRET (WINAPI *unreg_iface)(const WCHAR *, ULONG);
typedef BOOL (WINAPI *open_iface)(HDEVINFO, const WCHAR *, DWORD, SP_DEVICE_INTERFACE_DATA *);
typedef BOOL (WINAPI *iface_prop)(HDEVINFO, SP_DEVICE_INTERFACE_DATA *, const DEVPROPKEY *, DEVPROPTYPE *, BYTE *, DWORD, DWORD *, DWORD);
typedef BOOL (WINAPI *iface_alias)(HDEVINFO, SP_DEVICE_INTERFACE_DATA *, const GUID *, SP_DEVICE_INTERFACE_DATA *);

static WCHAR list[16384];

static int in_list(const WCHAR *l, const WCHAR *path)
{
    for (; *l; l += wcslen(l) + 1) if (!_wcsicmp(l, path)) return 1;
    return 0;
}

static int list_count(const WCHAR *l)
{
    int n = 0;
    for (; *l; l += wcslen(l) + 1) n++;
    return n;
}

static void iface_path(HDEVINFO set, SP_DEVICE_INTERFACE_DATA *iface, WCHAR *path)
{
    BYTE buf[2048];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)buf;

    path[0] = 0;
    detail->cbSize = sizeof(*detail);
    if (SetupDiGetDeviceInterfaceDetailW(set, iface, detail, sizeof(buf), NULL, NULL))
        lstrcpyW(path, detail->DevicePath);
}

/* enables an interface as a driver would (IoSetDeviceInterfaceState) */
static void enable(const GUID *class, const WCHAR *path)
{
    WCHAR key_path[1024], *p;
    HKEY key;
    DWORD one = 1;
    const WCHAR *slash = wcschr(path + 4, '\\');
    int len = slash ? (int)(slash - path) : (int)wcslen(path);

    swprintf(key_path, ARRAY_SIZE(key_path),
             L"System\\CurrentControlSet\\Control\\DeviceClasses\\{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\\",
             class->Data1, class->Data2, class->Data3, class->Data4[0], class->Data4[1], class->Data4[2],
             class->Data4[3], class->Data4[4], class->Data4[5], class->Data4[6], class->Data4[7]);
    p = key_path + wcslen(key_path);
    memcpy(p, path, len * sizeof(WCHAR));
    p[len] = 0;
    p[0] = p[1] = p[3] = '#';
    lstrcatW(key_path, slash ? L"\\#" : L"\\#");
    if (slash) lstrcatW(key_path, slash + 1);
    lstrcatW(key_path, L"\\Control");
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, key_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
    {
        RegSetValueExW(key, L"Linked", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
        RegCloseKey(key);
    }
}

int main(void)
{
    HMODULE cfg = LoadLibraryA("cfgmgr32.dll"), sapi = LoadLibraryA("setupapi.dll");
    list_size_w pListSizeW = (void *)GetProcAddress(cfg, "CM_Get_Device_Interface_List_SizeW");
    list_w pListW = (void *)GetProcAddress(cfg, "CM_Get_Device_Interface_ListW");
    list_a pListA = (void *)GetProcAddress(cfg, "CM_Get_Device_Interface_ListA");
    alias_w pAliasW = (void *)GetProcAddress(cfg, "CM_Get_Device_Interface_AliasW");
    get_prop pGetProp = (void *)GetProcAddress(cfg, "CM_Get_Device_Interface_PropertyW");
    set_prop pSetProp = (void *)GetProcAddress(cfg, "CM_Set_Device_Interface_PropertyW");
    prop_keys pKeys = (void *)GetProcAddress(cfg, "CM_Get_Device_Interface_Property_KeysW");
    reg_iface pRegister = (void *)GetProcAddress(cfg, "CM_Register_Device_InterfaceW");
    unreg_iface pUnregister = (void *)GetProcAddress(cfg, "CM_Unregister_Device_InterfaceW");
    open_iface pOpen = (void *)GetProcAddress(sapi, "SetupDiOpenDeviceInterfaceW");
    iface_alias pAlias = (void *)GetProcAddress(sapi, "SetupDiGetDeviceInterfaceAlias");
    iface_prop pIfaceProp = (void *)GetProcAddress(sapi, "SetupDiGetDeviceInterfacePropertyW");
    SP_DEVINFO_DATA dev = { sizeof(dev) }, dev2 = { sizeof(dev2) };
    SP_DEVICE_INTERFACE_DATA ia = { sizeof(ia) }, ib = { sizeof(ib) }, ia2 = { sizeof(ia2) }, opened = { sizeof(opened) };
    WCHAR id[MAX_DEVICE_ID_LEN], path_a[512], path_b[512], path_a2[512], path_c[512], other[512];
    char lista[16384];
    HDEVINFO set, set2;
    ULONG len, size;
    DEVPROPTYPE type;
    DEVPROPKEY keys[16];
    BYTE buf[512];
    CONFIGRET cr;
    GUID g;
    int i, found;

    check(pListSizeW && pListW && pListA && pAliasW && pGetProp && pSetProp && pKeys && pRegister && pUnregister
          && pOpen && pAlias && pIfaceProp, "the calls are there");
    if (failures) goto done;

    set = SetupDiCreateDeviceInfoList(&setup_class, NULL);
    if (!SetupDiCreateDeviceInfoW(set, L"SGIFACEPROBE", &setup_class, NULL, NULL, DICD_GENERATE_ID, &dev)
            || !SetupDiRegisterDeviceInfo(set, &dev, 0, NULL, NULL, NULL))
    {
        check(0, "a root device for the probe");
        goto done;
    }
    SetupDiGetDeviceInstanceIdW(set, &dev, id, ARRAY_SIZE(id), NULL);
    printf("  device %ls\n", id);
    SetupDiCreateDeviceInterfaceW(set, &dev, &class_a, NULL, 0, &ia);
    SetupDiCreateDeviceInterfaceW(set, &dev, &class_b, NULL, 0, &ib);
    SetupDiCreateDeviceInterfaceW(set, &dev, &class_a, L"second", 0, &ia2);
    iface_path(set, &ia, path_a);
    iface_path(set, &ib, path_b);
    iface_path(set, &ia2, path_a2);
    printf("  %ls\n  %ls\n", path_a, path_a2);

    /* lists */
    len = 0;
    cr = pListSizeW(&len, (GUID *)&class_a, NULL, CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    check(cr == CR_SUCCESS && len > wcslen(path_a) + wcslen(path_a2) + 2, "List_Size: room for both interfaces of the class (was a stub)");
    cr = pListW((GUID *)&class_a, NULL, list, ARRAY_SIZE(list), CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    check(cr == CR_SUCCESS && in_list(list, path_a) && in_list(list, path_a2) && !in_list(list, path_b),
          "List: the class's interfaces, all of them");
    cr = pListW((GUID *)&class_a, id, list, ARRAY_SIZE(list), CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    check(cr == CR_SUCCESS && list_count(list) == 2, "List for the device: its two");
    cr = pListW((GUID *)&class_a, NULL, list, 3, CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    check(cr == CR_BUFFER_SMALL, "a small buffer: CR_BUFFER_SMALL");
    cr = pListW((GUID *)&class_a, id, list, ARRAY_SIZE(list), CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    check(cr == CR_SUCCESS && !list[0], "none enabled: an empty list");
    enable(&class_a, path_a);
    cr = pListW((GUID *)&class_a, id, list, ARRAY_SIZE(list), CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    check(cr == CR_SUCCESS && list_count(list) == 1 && in_list(list, path_a), "one enabled: it alone");
    cr = pListA((GUID *)&class_a, NULL, lista, sizeof(lista), CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    check(cr == CR_SUCCESS && lista[0] == '\\', "ListA");
    cr = pListW((GUID *)&class_a, (WCHAR *)L"ROOT\\NOSUCHDEVICE\\0000", list, ARRAY_SIZE(list), 0);
    check(cr != CR_SUCCESS, "no such device: an error");

    /* opening by path */
    set2 = SetupDiCreateDeviceInfoList(NULL, NULL);
    check(pOpen(set2, path_a, 0, &opened) && IsEqualGUID(&opened.InterfaceClassGuid, &class_a)
          && (opened.Flags & SPINT_ACTIVE), "SetupDiOpenDeviceInterface by path (was a FIXME)");
    iface_path(set2, &opened, other);
    check(!_wcsicmp(other, path_a), "its path");
    {
        BYTE dbuf[2048];
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)dbuf;
        WCHAR id2[MAX_DEVICE_ID_LEN] = { 0 };
        detail->cbSize = sizeof(*detail);
        SetupDiGetDeviceInterfaceDetailW(set2, &opened, detail, sizeof(dbuf), NULL, &dev2);
        SetupDiGetDeviceInstanceIdW(set2, &dev2, id2, ARRAY_SIZE(id2), NULL);
        check(!_wcsicmp(id2, id), "its device");
    }
    SetLastError(0xdeadbeef);
    check(!pOpen(set2, L"\\\\?\\ROOT#NOSUCH#0000#{5a9e0001-5347-4f53-9100-000000001697}", 0, &opened)
          && GetLastError() == ERROR_NO_SUCH_DEVICE_INTERFACE, "an unknown path: ERROR_NO_SUCH_DEVICE_INTERFACE");

    /* aliases */
    len = ARRAY_SIZE(other);
    cr = pAliasW(path_a, (GUID *)&class_b, other, &len, 0);
    check(cr == CR_SUCCESS && !_wcsicmp(other, path_b), "CM_Get_Device_Interface_Alias: the device's interface of the other class (was a stub)");
    len = 2;
    cr = pAliasW(path_a, (GUID *)&class_b, other, &len, 0);
    check(cr == CR_BUFFER_SMALL && len == wcslen(path_b) + 1, "a small buffer: CR_BUFFER_SMALL and the size");
    len = ARRAY_SIZE(other);
    cr = pAliasW(path_a2, (GUID *)&class_b, other, &len, 0);
    check(cr == CR_NO_SUCH_DEVICE_INTERFACE, "no alias with that reference string");
    pOpen(set2, path_a, 0, &opened);
    {
        SP_DEVICE_INTERFACE_DATA alias = { sizeof(alias) };
        check(pAlias(set2, &opened, &class_b, &alias) && IsEqualGUID(&alias.InterfaceClassGuid, &class_b),
              "SetupDiGetDeviceInterfaceAlias (was a stub)");
    }

    /* properties */
    size = sizeof(buf);
    cr = pGetProp(path_a, &key_instance, &type, buf, &size, 0);
    check(cr == CR_SUCCESS && type == DEVPROP_TYPE_STRING && !_wcsicmp((WCHAR *)buf, id),
          "CM_Get_Device_Interface_Property: the device instance (was not implemented)");
    size = sizeof(buf);
    cr = pGetProp(path_a, &key_enabled, &type, buf, &size, 0);
    check(cr == CR_SUCCESS && type == DEVPROP_TYPE_BOOLEAN && *(DEVPROP_BOOLEAN *)buf == DEVPROP_TRUE, "enabled");
    size = sizeof(buf);
    cr = pGetProp(path_a2, &key_enabled, &type, buf, &size, 0);
    check(cr == CR_SUCCESS && *(DEVPROP_BOOLEAN *)buf == DEVPROP_FALSE, "the other one is not");
    size = sizeof(buf);
    cr = pGetProp(path_a, &key_class, &type, buf, &size, 0);
    memcpy(&g, buf, sizeof(g));
    check(cr == CR_SUCCESS && type == DEVPROP_TYPE_GUID && IsEqualGUID(&g, &class_a), "its class");
    size = 4;
    cr = pGetProp(path_a, &key_instance, &type, buf, &size, 0);
    check(cr == CR_BUFFER_SMALL && size == (wcslen(id) + 1) * sizeof(WCHAR), "a small buffer: CR_BUFFER_SMALL and the size");
    size = sizeof(buf);
    cr = pGetProp(path_a, &key_other, &type, buf, &size, 0);
    check(cr == CR_NO_SUCH_VALUE, "not set: CR_NO_SUCH_VALUE");
    cr = pSetProp(path_a, &key_friendly, DEVPROP_TYPE_STRING, (BYTE *)L"SG probe port", sizeof(L"SG probe port"), 0);
    size = sizeof(buf);
    check(cr == CR_SUCCESS && pGetProp(path_a, &key_friendly, &type, buf, &size, 0) == CR_SUCCESS
          && !lstrcmpW((WCHAR *)buf, L"SG probe port"), "CM_Set_Device_Interface_Property: a friendly name, read back");
    cr = pSetProp(path_a, &key_instance, DEVPROP_TYPE_STRING, (BYTE *)L"X", 4, 0);
    check(cr == CR_ACCESS_DENIED, "its device instance cannot be set");
    size = ARRAY_SIZE(keys);
    cr = pKeys(path_a, keys, &size, 0);
    for (i = 0, found = 0; cr == CR_SUCCESS && i < (int)size; i++)
        if (IsEqualDevPropKey(keys[i], key_friendly) || IsEqualDevPropKey(keys[i], key_instance)) found++;
    check(cr == CR_SUCCESS && found == 2, "Property_Keys: the instance and the friendly name among them");
    {
        DWORD req = 0;
        size = sizeof(buf);
        check(pIfaceProp(set2, &opened, &key_friendly, &type, buf, sizeof(buf), &req, 0)
              && req == sizeof(L"SG probe port"), "SetupDiGetDeviceInterfacePropertyW");
    }

    /* registering and unregistering */
    len = ARRAY_SIZE(path_c);
    cr = pRegister(dev.DevInst, (GUID *)&class_c, L"reg", path_c, &len, 0);
    check(cr == CR_SUCCESS && wcsstr(path_c, L"5a9e0003") && len == wcslen(path_c) + 1,
          "CM_Register_Device_Interface: its path (was a stub)");
    cr = pListW((GUID *)&class_c, id, list, ARRAY_SIZE(list), CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    check(cr == CR_SUCCESS && in_list(list, path_c), "listed");
    cr = pUnregister(path_c, 0);
    check(cr == CR_SUCCESS, "CM_Unregister_Device_Interface (was a stub)");
    if (cr) printf("      cr %#lx\n", cr);
    cr = pListW((GUID *)&class_c, id, list, ARRAY_SIZE(list), CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    check(cr == CR_SUCCESS && !in_list(list, path_c), "not listed any more");

    SetupDiDestroyDeviceInfoList(set2);
    SetupDiRemoveDevice(set, &dev);
    SetupDiDestroyDeviceInfoList(set);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
