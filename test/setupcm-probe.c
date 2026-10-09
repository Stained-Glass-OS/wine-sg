/* setupapi/cfgmgr32 stubs batch (patches/sg/2002), run by test/setupcm-gate.sh.
 * One table of checks per behaviour family: machine handles, class
 * enumeration, class registry properties (CM_ and SetupDi, A and W),
 * the selected device, destroying a driver list, ANSI device-node
 * properties, custom device properties and INF [Version] queries.
 * Everything is looked up with GetProcAddress, as the exports are what
 * is under test.
 *
 *   setupcm-probe.exe */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
static const GUID null_guid;
#define GUID_NULL null_guid
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

#define SPCRP_UPPERFILTERS_ 0x11
#define SPCRP_LOWERFILTERS_ 0x12
#define SPCRP_SECURITY_     0x17
#define SPCRP_SDS_          0x18
#define SPCRP_DEVTYPE_      0x19
#define SPCRP_EXCLUSIVE_    0x1a
#define SPCRP_CHARS_        0x1b

typedef CONFIGRET (WINAPI *connect_t)(const void *, HMACHINE *);
typedef CONFIGRET (WINAPI *disconnect_t)(HMACHINE);
typedef CONFIGRET (WINAPI *enum_t)(ULONG, GUID *, ULONG);
typedef CONFIGRET (WINAPI *enum_ex_t)(ULONG, GUID *, ULONG, HMACHINE);
typedef CONFIGRET (WINAPI *cmget_t)(GUID *, ULONG, ULONG *, void *, ULONG *, ULONG, HMACHINE);
typedef CONFIGRET (WINAPI *cmset_t)(GUID *, ULONG, const void *, ULONG, ULONG, HMACHINE);
typedef CONFIGRET (WINAPI *devprop_t)(DEVINST, ULONG, ULONG *, void *, ULONG *, ULONG, HMACHINE);
typedef BOOL (WINAPI *sdget_t)(const GUID *, DWORD, DWORD *, BYTE *, DWORD, DWORD *, const void *, void *);
typedef BOOL (WINAPI *sdset_t)(const GUID *, DWORD, const BYTE *, DWORD, const void *, void *);

static HMODULE sapi;
static void *fn(const char *name)
{
    void *p = GetProcAddress(sapi, name);
    if (!p) printf("FAIL  export %s missing\n", name), failures++;
    return p;
}

static const GUID guid_a = {0x5e600001, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};
static const GUID guid_b = {0x5e600002, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};
static const GUID guid_c = {0x5e600003, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};
static const GUID guid_missing = {0x5e6000ff, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};

static void guid_str(const GUID *g, WCHAR *out)
{
    wsprintfW(out, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g->Data1, g->Data2, g->Data3,
            g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6],
            g->Data4[7]);
}

static void make_key(const WCHAR *root, const WCHAR *name)
{
    WCHAR path[256];
    HKEY key;

    wsprintfW(path, L"%s\\%s", root, name);
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL)) RegCloseKey(key);
}

#define CLASSROOT L"System\\CurrentControlSet\\Control\\Class"
#define IFACEROOT L"System\\CurrentControlSet\\Control\\DeviceClasses"

static void test_machine(void)
{
    connect_t connw = fn("CM_Connect_MachineW"), conna = fn("CM_Connect_MachineA");
    disconnect_t disc = fn("CM_Disconnect_Machine");
    WCHAR host[MAX_COMPUTERNAME_LENGTH + 1], name[MAX_COMPUTERNAME_LENGTH + 8];
    char namea[MAX_COMPUTERNAME_LENGTH + 8];
    DWORD size = ARRAY_SIZE(host);
    HMACHINE m = (HMACHINE)0x1234;

    GetComputerNameW(host, &size);
    wsprintfW(name, L"\\\\%s", host);
    wsprintfA(namea, "\\\\%ls", host);

    CHECK(connw(NULL, &m) == CR_SUCCESS && m != NULL && m != (HMACHINE)0x1234);
    CHECK(disc(m) == CR_SUCCESS);
    CHECK(disc(m) == CR_INVALID_MACHINENAME);
    CHECK(connw(L"", &m) == CR_SUCCESS && m);
    CHECK(disc(m) == CR_SUCCESS);
    CHECK(connw(name, &m) == CR_SUCCESS && m);
    CHECK(disc(m) == CR_SUCCESS);
    CHECK(conna(namea, &m) == CR_SUCCESS && m);
    CHECK(disc(m) == CR_SUCCESS);
    CHECK(conna(NULL, &m) == CR_SUCCESS && m);
    CHECK(disc(m) == CR_SUCCESS);
    CHECK(connw(L"NOSLASHES", &m) == CR_INVALID_MACHINENAME && !m);
    CHECK(connw(L"\\\\", &m) == CR_INVALID_MACHINENAME);
    CHECK(connw(L"\\\\SG-NO-SUCH-HOST", &m) == CR_REMOTE_COMM_FAILURE && !m);
    CHECK(conna("\\\\SG-NO-SUCH-HOST", &m) == CR_REMOTE_COMM_FAILURE);
    CHECK(connw(NULL, NULL) == CR_INVALID_POINTER);
    CHECK(disc(NULL) == CR_SUCCESS);
}

static void test_enum_classes(void)
{
    enum_t en = fn("CM_Enumerate_Classes");
    enum_ex_t enex = fn("CM_Enumerate_Classes_Ex");
    WCHAR str[64];
    int found_a = 0, found_if = 0, count = 0, expected = 0;
    DWORD i;
    GUID g;
    HKEY root;
    CONFIGRET cr;

    guid_str(&guid_a, str);
    make_key(CLASSROOT, str);
    make_key(CLASSROOT, L"not-a-guid");
    guid_str(&guid_b, str);
    make_key(IFACEROOT, str);

    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, CLASSROOT, 0, KEY_ENUMERATE_SUB_KEYS, &root))
    {
        for (i = 0; ; ++i)
        {
            DWORD len = ARRAY_SIZE(str);
            if (RegEnumKeyExW(root, i, str, &len, NULL, NULL, NULL, NULL)) break;
            if (len == 38 && str[0] == '{') expected++;
        }
        RegCloseKey(root);
    }

    for (i = 0; i < 1000; ++i)
    {
        cr = en(i, &g, CM_ENUMERATE_CLASSES_INSTALLER);
        if (cr != CR_SUCCESS) break;
        count++;
        if (IsEqualGUID(&g, &guid_a)) found_a = 1;
    }
    CHECK(cr == CR_NO_SUCH_VALUE);
    CHECK(found_a);
    CHECK(count == expected && count > 0);

    for (i = 0; i < 1000; ++i)
    {
        if (enex(i, &g, CM_ENUMERATE_CLASSES_INTERFACE, NULL)) break;
        if (IsEqualGUID(&g, &guid_b)) found_if = 1;
    }
    CHECK(found_if);
    CHECK(en(0, NULL, 0) == CR_INVALID_POINTER);
    CHECK(en(0, &g, 2) == CR_INVALID_FLAG);
    CHECK(en(100000, &g, 0) == CR_NO_SUCH_VALUE);
    CHECK(enex(0, &g, 0, NULL) == CR_SUCCESS);
}

static void test_class_props(void)
{
    cmget_t getw = fn("CM_Get_Class_Registry_PropertyW"), geta = fn("CM_Get_Class_Registry_PropertyA");
    cmset_t setw = fn("CM_Set_Class_Registry_PropertyW"), seta = fn("CM_Set_Class_Registry_PropertyA");
    sdget_t sdgetw = fn("SetupDiGetClassRegistryPropertyW"), sdgeta = fn("SetupDiGetClassRegistryPropertyA");
    sdset_t sdsetw = fn("SetupDiSetClassRegistryPropertyW"), sdseta = fn("SetupDiSetClassRegistryPropertyA");
    static const WCHAR filtersw[] = L"filtera\0filterb\0";
    static const char filtersa[] = "filtera\0filterb\0";
    GUID g = guid_a, missing = guid_missing;
    WCHAR bufw[64];
    char bufa[64];
    ULONG len, type;
    DWORD dw, req, d;
    BYTE sdbuf[256];

    /* wide round trip, CM and SetupDi */
    CHECK(setw(&g, CM_CRP_UPPERFILTERS, filtersw, sizeof(filtersw), 0, NULL) == CR_SUCCESS);
    len = 0;
    CHECK(getw(&g, CM_CRP_UPPERFILTERS, &type, NULL, &len, 0, NULL) == CR_BUFFER_SMALL && len == sizeof(filtersw));
    len = 4;
    CHECK(getw(&g, CM_CRP_UPPERFILTERS, &type, bufw, &len, 0, NULL) == CR_BUFFER_SMALL && len == sizeof(filtersw));
    len = sizeof(bufw);
    memset(bufw, 0, sizeof(bufw));
    CHECK(getw(&g, CM_CRP_UPPERFILTERS, &type, bufw, &len, 0, NULL) == CR_SUCCESS);
    CHECK(type == REG_MULTI_SZ && len == sizeof(filtersw) && !memcmp(bufw, filtersw, sizeof(filtersw)));
    /* ANSI */
    len = sizeof(bufa);
    memset(bufa, 0, sizeof(bufa));
    CHECK(geta(&g, CM_CRP_UPPERFILTERS, &type, bufa, &len, 0, NULL) == CR_SUCCESS);
    CHECK(type == REG_MULTI_SZ && len == sizeof(filtersa) && !memcmp(bufa, filtersa, sizeof(filtersa)));
    len = 3;
    CHECK(geta(&g, CM_CRP_UPPERFILTERS, NULL, bufa, &len, 0, NULL) == CR_BUFFER_SMALL && len == sizeof(filtersa));
    CHECK(seta(&g, CM_CRP_LOWERFILTERS, filtersa, sizeof(filtersa), 0, NULL) == CR_SUCCESS);
    len = sizeof(bufw);
    memset(bufw, 0, sizeof(bufw));
    CHECK(getw(&g, CM_CRP_LOWERFILTERS, NULL, bufw, &len, 0, NULL) == CR_SUCCESS
            && len == sizeof(filtersw) && !memcmp(bufw, filtersw, sizeof(filtersw)));

    /* DWORD properties */
    dw = 0x20;
    CHECK(setw(&g, CM_CRP_DEVTYPE, &dw, sizeof(dw), 0, NULL) == CR_SUCCESS);
    dw = 0;
    len = sizeof(dw);
    CHECK(getw(&g, CM_CRP_DEVTYPE, &type, &dw, &len, 0, NULL) == CR_SUCCESS && type == REG_DWORD && dw == 0x20);
    dw = 1;
    CHECK(sdsetw(&g, SPCRP_EXCLUSIVE_, (BYTE *)&dw, sizeof(dw), NULL, NULL));
    dw = 0;
    CHECK(sdgetw(&g, SPCRP_EXCLUSIVE_, &d, (BYTE *)&dw, sizeof(dw), &req, NULL, NULL) && d == REG_DWORD && dw == 1
            && req == sizeof(dw));
    dw = 0x100;
    CHECK(sdseta(&g, SPCRP_CHARS_, (BYTE *)&dw, sizeof(dw), NULL, NULL));
    dw = 0;
    CHECK(sdgeta(&g, SPCRP_CHARS_, &d, (BYTE *)&dw, sizeof(dw), &req, NULL, NULL) && dw == 0x100);
    CHECK(setw(&g, CM_CRP_DEVTYPE, &dw, 2, 0, NULL) == CR_INVALID_DATA);

    /* SetupDi errors */
    SetLastError(0);
    CHECK(!sdgetw(&g, SPCRP_UPPERFILTERS_, &d, NULL, 0, &req, NULL, NULL)
            && GetLastError() == ERROR_INSUFFICIENT_BUFFER && req == sizeof(filtersw));
    SetLastError(0);
    CHECK(!sdgetw(&g, SPCRP_UPPERFILTERS_, &d, (BYTE *)bufw, 4, &req, NULL, NULL)
            && GetLastError() == ERROR_INSUFFICIENT_BUFFER && req == sizeof(filtersw));
    SetLastError(0);
    CHECK(!sdgetw(&missing, SPCRP_UPPERFILTERS_, &d, (BYTE *)bufw, sizeof(bufw), &req, NULL, NULL)
            && GetLastError() == ERROR_INVALID_CLASS);
    SetLastError(0);
    CHECK(!sdgetw(&g, 0x05, &d, (BYTE *)bufw, sizeof(bufw), &req, NULL, NULL)
            && GetLastError() == ERROR_INVALID_REG_PROPERTY);
    SetLastError(0);
    CHECK(!sdgetw(&g, SPCRP_SDS_, &d, (BYTE *)bufw, sizeof(bufw), &req, NULL, NULL)
            && GetLastError() == ERROR_INVALID_REG_PROPERTY);
    SetLastError(0);
    CHECK(!sdgetw(&g, SPCRP_UPPERFILTERS_, &d, (BYTE *)bufw, sizeof(bufw), &req, L"\\\\other", NULL)
            && GetLastError() == ERROR_INVALID_MACHINENAME);
    SetLastError(0);
    CHECK(!sdgetw(NULL, SPCRP_UPPERFILTERS_, &d, (BYTE *)bufw, sizeof(bufw), &req, NULL, NULL)
            && GetLastError() == ERROR_INVALID_PARAMETER);

    /* CM errors */
    len = sizeof(bufw);
    CHECK(getw(&g, 0x01, &type, bufw, &len, 0, NULL) == CR_INVALID_PROPERTY);
    CHECK(getw(&g, CM_CRP_UPPERFILTERS, &type, bufw, &len, 1, NULL) == CR_INVALID_FLAG);
    CHECK(getw(&g, CM_CRP_UPPERFILTERS, &type, bufw, NULL, 0, NULL) == CR_INVALID_POINTER);
    CHECK(getw(NULL, CM_CRP_UPPERFILTERS, &type, bufw, &len, 0, NULL) == CR_INVALID_POINTER);
    CHECK(getw(&missing, CM_CRP_UPPERFILTERS, &type, bufw, &len, 0, NULL) == CR_NO_SUCH_REGISTRY_KEY);
    CHECK(getw(&g, CM_CRP_SECURITY, &type, bufw, &len, 0, NULL) == CR_NO_SUCH_VALUE);
    CHECK(setw(&missing, CM_CRP_UPPERFILTERS, filtersw, sizeof(filtersw), 0, NULL) == CR_NO_SUCH_REGISTRY_KEY);
    CHECK(setw(&g, CM_CRP_UPPERFILTERS, filtersw, sizeof(filtersw), 4, NULL) == CR_INVALID_FLAG);
    CHECK(setw(&g, 0x01, filtersw, sizeof(filtersw), 0, NULL) == CR_INVALID_PROPERTY);

    /* security: SDDL in, binary SD out */
    CHECK(sdsetw(&g, SPCRP_SDS_, (const BYTE *)L"D:(A;;GA;;;WD)", sizeof(L"D:(A;;GA;;;WD)"), NULL, NULL));
    memset(sdbuf, 0, sizeof(sdbuf));
    req = 0;
    CHECK(sdgetw(&g, SPCRP_SECURITY_, &d, sdbuf, sizeof(sdbuf), &req, NULL, NULL) && d == REG_BINARY
            && req > 0 && IsValidSecurityDescriptor(sdbuf));
    SetLastError(0);
    CHECK(!sdsetw(&g, SPCRP_SDS_, (const BYTE *)L"not sddl", sizeof(L"not sddl"), NULL, NULL)
            && GetLastError() == ERROR_INVALID_DATA);

    /* NULL buffer deletes */
    CHECK(setw(&g, CM_CRP_UPPERFILTERS, NULL, 0, 0, NULL) == CR_SUCCESS);
    len = sizeof(bufw);
    CHECK(getw(&g, CM_CRP_UPPERFILTERS, &type, bufw, &len, 0, NULL) == CR_NO_SUCH_VALUE);
    SetLastError(0);
    CHECK(!sdgetw(&g, SPCRP_UPPERFILTERS_, &d, (BYTE *)bufw, sizeof(bufw), &req, NULL, NULL)
            && GetLastError() == ERROR_INVALID_DATA);
}

static const char probe_inf[] =
    "[Version]\nSignature=\"$Chicago$\"\nClass=SGProbeClass\nProvider=SGProbeInc\n"
    "[Manufacturer]\nmfg1=mfg1_key,NTamd64\n[mfg1_key.NTamd64]\ndesc0=,sgprobe_hwid\n";

static void write_file(const char *path, const char *data)
{
    DWORD n;
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, data, strlen(data), &n, NULL);
    CloseHandle(h);
}

static const WCHAR multi_ab[] = {'a', 0, 'b', 0, 0};
static const WCHAR multi_cd[] = {'c', 0, 'd', 0, 0};

static void test_devices(void)
{
    typedef BOOL (WINAPI *selget_t)(HDEVINFO, SP_DEVINFO_DATA *);
    typedef BOOL (WINAPI *destroy_t)(HDEVINFO, SP_DEVINFO_DATA *, DWORD);
    typedef BOOL (WINAPI *custom_t)(HDEVINFO, SP_DEVINFO_DATA *, const void *, DWORD, DWORD *, BYTE *, DWORD, DWORD *);
    selget_t selget = fn("SetupDiGetSelectedDevice"), selset = fn("SetupDiSetSelectedDevice");
    destroy_t destroy = fn("SetupDiDestroyDriverInfoList");
    custom_t customw = fn("SetupDiGetCustomDevicePropertyW"), customa = fn("SetupDiGetCustomDevicePropertyA");
    devprop_t propa = fn("CM_Get_DevNode_Registry_Property_ExA"), propw = fn("CM_Get_DevNode_Registry_Property_ExW");
    static const char hwids[] = "sgprobe_hwid\0other_id\0";
    SP_DEVINSTALL_PARAMS_A params = {sizeof(params)};
    SP_DRVINFO_DATA_A drv = {sizeof(drv)};
    SP_DEVINFO_DATA d1 = {sizeof(d1)}, d2 = {sizeof(d2)}, got = {sizeof(got)};
    char inf_path[MAX_PATH], buf[128];
    WCHAR bufw[128];
    HDEVINFO set;
    ULONG len, type;
    DWORD t, req;
    HKEY key;

    set = SetupDiCreateDeviceInfoList(NULL, NULL);
    CHECK(SetupDiCreateDeviceInfoA(set, "Root\\SGPROBE\\0000", &GUID_NULL, NULL, NULL, 0, &d1));
    CHECK(SetupDiCreateDeviceInfoA(set, "Root\\SGPROBE\\0001", &GUID_NULL, NULL, NULL, 0, &d2));

    /* selected device */
    SetLastError(0);
    CHECK(!selget(set, &got) && GetLastError() == ERROR_NO_DEVICE_SELECTED);
    CHECK(selset(set, &d2));
    memset(&got, 0, sizeof(got)); got.cbSize = sizeof(got);
    CHECK(selget(set, &got) && got.DevInst == d2.DevInst && IsEqualGUID(&got.ClassGuid, &d2.ClassGuid));
    CHECK(selset(set, &d1));
    CHECK(selget(set, &got) && got.DevInst == d1.DevInst);
    got.cbSize = 4;
    SetLastError(0);
    CHECK(!selget(set, &got) && GetLastError() == ERROR_INVALID_USER_BUFFER);
    got.cbSize = sizeof(got);
    SetLastError(0);
    CHECK(!selget(INVALID_HANDLE_VALUE, &got) && GetLastError() == ERROR_INVALID_HANDLE);
    CHECK(selset(set, NULL));
    SetLastError(0);
    CHECK(!selget(set, &got) && GetLastError() == ERROR_NO_DEVICE_SELECTED);
    CHECK(selset(set, &d1));
    CHECK(SetupDiDeleteDeviceInfo(set, &d1));
    SetLastError(0);
    CHECK(!selget(set, &got) && GetLastError() == ERROR_NO_DEVICE_SELECTED);

    /* ANSI node properties on d2 */
    CHECK(SetupDiSetDeviceRegistryPropertyA(set, &d2, SPDRP_HARDWAREID, (const BYTE *)hwids, sizeof(hwids)));
    CHECK(SetupDiSetDeviceRegistryPropertyA(set, &d2, SPDRP_DEVICEDESC, (const BYTE *)"Probe Dev", 10));
    len = 0;
    CHECK(propa(d2.DevInst, CM_DRP_HARDWAREID, &type, NULL, &len, 0, NULL) == CR_BUFFER_SMALL
            && len == sizeof(hwids) && type == REG_MULTI_SZ);
    len = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    CHECK(propa(d2.DevInst, CM_DRP_HARDWAREID, &type, buf, &len, 0, NULL) == CR_SUCCESS
            && len == sizeof(hwids) && !memcmp(buf, hwids, sizeof(hwids)));
    len = 3;
    CHECK(propa(d2.DevInst, CM_DRP_DEVICEDESC, &type, buf, &len, 0, NULL) == CR_BUFFER_SMALL && len == 10);
    len = sizeof(buf);
    CHECK(propa(d2.DevInst, CM_DRP_DEVICEDESC, &type, buf, &len, 0, NULL) == CR_SUCCESS
            && type == REG_SZ && !strcmp(buf, "Probe Dev") && len == 10);
    len = sizeof(bufw);
    CHECK(propw(d2.DevInst, CM_DRP_DEVICEDESC, &type, bufw, &len, 0, NULL) == CR_SUCCESS && !lstrcmpW(bufw, L"Probe Dev"));
    CHECK(propa(d2.DevInst, CM_DRP_DEVICEDESC, &type, buf, NULL, 0, NULL) == CR_INVALID_POINTER);
    len = sizeof(buf);
    CHECK(propa(d2.DevInst, CM_DRP_SERVICE, &type, buf, &len, 0, NULL) == CR_NO_SUCH_VALUE);
    CHECK(propa(d2.DevInst, 0, &type, buf, &len, 0, NULL) == CR_INVALID_PROPERTY);
    CHECK(propa(0x7ffffff0, CM_DRP_DEVICEDESC, &type, buf, &len, 0, NULL) == CR_NO_SUCH_DEVNODE);

    /* custom properties: hardware key first, then the driver key */
    SetLastError(0);
    CHECK(!customw(set, &d2, L"SGCustom", 0, &t, (BYTE *)bufw, sizeof(bufw), &req) && GetLastError() == ERROR_INVALID_DATA);
    CHECK(SetupDiRegisterDeviceInfo(set, &d2, 0, NULL, NULL, NULL));
    key = SetupDiCreateDevRegKeyW(set, &d2, DICS_FLAG_GLOBAL, 0, DIREG_DEV, NULL, NULL);
    CHECK(key != INVALID_HANDLE_VALUE);
    if (key != INVALID_HANDLE_VALUE)
    {
        RegSetValueExW(key, L"SGCustom", 0, REG_SZ, (const BYTE *)L"hello", sizeof(L"hello"));
        RegSetValueExW(key, L"SGMulti", 0, REG_MULTI_SZ, (const BYTE *)multi_ab, sizeof(multi_ab));
        RegCloseKey(key);
    }
    key = SetupDiCreateDevRegKeyW(set, &d2, DICS_FLAG_GLOBAL, 0, DIREG_DRV, NULL, NULL);
    if (key != INVALID_HANDLE_VALUE)
    {
        RegSetValueExW(key, L"SGMulti", 0, REG_MULTI_SZ, (const BYTE *)multi_cd, sizeof(multi_cd));
        RegSetValueExW(key, L"SGDrvOnly", 0, REG_DWORD, (const BYTE *)&len, sizeof(len));
        RegCloseKey(key);
    }
    memset(bufw, 0, sizeof(bufw));
    CHECK(customw(set, &d2, L"SGCustom", 0, &t, (BYTE *)bufw, sizeof(bufw), &req) && t == REG_SZ
            && !lstrcmpW(bufw, L"hello") && req == sizeof(L"hello"));
    SetLastError(0);
    CHECK(!customw(set, &d2, L"SGCustom", 0, &t, (BYTE *)bufw, 4, &req) && GetLastError() == ERROR_INSUFFICIENT_BUFFER
            && req == sizeof(L"hello"));
    CHECK(!customw(set, &d2, L"SGCustom", 0, &t, NULL, 0, &req) && req == sizeof(L"hello"));
    memset(buf, 0, sizeof(buf));
    CHECK(customa(set, &d2, "SGCustom", 0, &t, (BYTE *)buf, sizeof(buf), &req) && !strcmp(buf, "hello") && req == 6);
    CHECK(customw(set, &d2, L"SGDrvOnly", 0, &t, (BYTE *)bufw, sizeof(bufw), &req) && t == REG_DWORD && req == 4);
    /* hardware-key value alone unless asked to merge */
    memset(bufw, 0, sizeof(bufw));
    CHECK(customw(set, &d2, L"SGMulti", 0, &t, (BYTE *)bufw, sizeof(bufw), &req) && t == REG_MULTI_SZ
            && req == sizeof(multi_ab) && !memcmp(bufw, multi_ab, sizeof(multi_ab)));
    memset(bufw, 0, sizeof(bufw));
    CHECK(customw(set, &d2, L"SGMulti", DICUSTOMDEVPROP_MERGE_MULTISZ, &t, (BYTE *)bufw, sizeof(bufw), &req)
            && req == 9 * sizeof(WCHAR) && !memcmp(bufw, L"a\0b\0c\0d\0", 9 * sizeof(WCHAR)));
    SetLastError(0);
    CHECK(!customw(set, &d2, L"SGCustom", 0x10, &t, (BYTE *)bufw, sizeof(bufw), &req) && GetLastError() == ERROR_INVALID_FLAGS);

    /* driver list: build from a one-INF dir, destroy */
    GetTempPathA(sizeof(inf_path), inf_path);
    strcat(inf_path, "sgprobe_drv.inf");
    write_file(inf_path, probe_inf);
    CHECK(SetupDiGetDeviceInstallParamsA(set, &d2, &params));
    strcpy(params.DriverPath, inf_path);
    params.Flags = DI_ENUMSINGLEINF;
    CHECK(SetupDiSetDeviceInstallParamsA(set, &d2, &params));
    CHECK(SetupDiBuildDriverInfoList(set, &d2, SPDIT_COMPATDRIVER));
    CHECK(SetupDiEnumDriverInfoA(set, &d2, SPDIT_COMPATDRIVER, 0, &drv) && !strcmp(drv.Description, "desc0"));
    CHECK(SetupDiSelectBestCompatDrv(set, &d2));
    CHECK(SetupDiGetSelectedDriverA(set, &d2, &drv));
    CHECK(destroy(set, &d2, SPDIT_COMPATDRIVER));
    SetLastError(0);
    CHECK(!SetupDiEnumDriverInfoA(set, &d2, SPDIT_COMPATDRIVER, 0, &drv) && GetLastError() == ERROR_NO_MORE_ITEMS);
    SetLastError(0);
    CHECK(!SetupDiGetSelectedDriverA(set, &d2, &drv) && GetLastError() == ERROR_NO_DRIVER_SELECTED);
    CHECK(destroy(set, &d2, SPDIT_COMPATDRIVER));   /* idempotent */
    CHECK(destroy(set, &d2, SPDIT_CLASSDRIVER));
    SetLastError(0);
    CHECK(!destroy(set, &d2, 0) && GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!destroy(INVALID_HANDLE_VALUE, &d2, SPDIT_COMPATDRIVER) && GetLastError() == ERROR_INVALID_HANDLE);
    CHECK(SetupDiBuildDriverInfoList(set, &d2, SPDIT_COMPATDRIVER));   /* rebuild works */
    CHECK(SetupDiEnumDriverInfoA(set, &d2, SPDIT_COMPATDRIVER, 0, &drv));
    DeleteFileA(inf_path);
    SetupDiDestroyDeviceInfoList(set);
}

static void test_inf_version(void)
{
    typedef BOOL (WINAPI *qa_t)(SP_INF_INFORMATION *, UINT, const char *, char *, DWORD, DWORD *);
    typedef BOOL (WINAPI *qw_t)(SP_INF_INFORMATION *, UINT, const WCHAR *, WCHAR *, DWORD, DWORD *);
    qa_t qa = fn("SetupQueryInfVersionInformationA");
    qw_t qw = fn("SetupQueryInfVersionInformationW");
    char path[MAX_PATH], buf[128];
    WCHAR bufw[128];
    SP_INF_INFORMATION *info = HeapAlloc(GetProcessHeap(), 0, 4096);
    DWORD req, i;
    int has_sig = 0, has_class = 0, has_prov = 0;
    const char *p;

    GetTempPathA(sizeof(path), path);
    strcat(path, "sgprobe_ver.inf");
    write_file(path, probe_inf);
    CHECK(SetupGetInfInformationA(path, INFINFO_INF_NAME_IS_ABSOLUTE, info, 4096, &req));

    memset(buf, 0, sizeof(buf));
    CHECK(qa(info, 0, "Class", buf, sizeof(buf), &req) && !strcmp(buf, "SGProbeClass") && req == 13);
    CHECK(qa(info, 0, "Provider", buf, sizeof(buf), &req) && !strcmp(buf, "SGProbeInc"));
    CHECK(qa(info, 0, "Signature", buf, sizeof(buf), &req) && !strcmp(buf, "$Chicago$"));
    CHECK(qw(info, 0, L"Class", bufw, ARRAY_SIZE(bufw), &req) && !lstrcmpW(bufw, L"SGProbeClass") && req == 13);
    req = 0;
    CHECK(qa(info, 0, "Class", NULL, 0, &req) && req == 13);
    SetLastError(0);
    CHECK(!qa(info, 0, "Class", buf, 4, &req) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && req == 13);
    SetLastError(0);
    CHECK(!qa(info, 0, "NoSuchKey", buf, sizeof(buf), &req));
    SetLastError(0);
    CHECK(!qa(info, 5, "Class", buf, sizeof(buf), &req) && GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!qa(NULL, 0, "Class", buf, sizeof(buf), &req) && GetLastError() == ERROR_INVALID_PARAMETER);

    /* no key: the section's key names, as a multi-string */
    memset(buf, 0, sizeof(buf));
    CHECK(qa(info, 0, NULL, buf, sizeof(buf), &req) && req > 1);
    for (p = buf; *p; p += strlen(p) + 1)
    {
        if (!strcmp(p, "Signature")) has_sig = 1;
        if (!strcmp(p, "Class")) has_class = 1;
        if (!strcmp(p, "Provider")) has_prov = 1;
    }
    CHECK(has_sig && has_class && has_prov);
    (void)i;
    DeleteFileA(path);
    HeapFree(GetProcessHeap(), 0, info);
}

int main(void)
{
    sapi = LoadLibraryA("setupapi.dll");
    if (!sapi) { puts("FAIL  setupapi.dll did not load"); return 1; }
    test_machine();
    test_enum_classes();
    test_class_props();
    test_devices();
    test_inf_version();
    printf("RESULT: %s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
