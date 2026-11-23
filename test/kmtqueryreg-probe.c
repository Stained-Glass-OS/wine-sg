/* kmtqueryreg-probe (wine-sg 9802): D3DKMTQueryAdapterInfo(KMTQAITYPE_QUERYREGISTRY), the adapter's
 * registry values as the Khronos Vulkan loader Chromium bundles asks for them (VulkanDriverName in the
 * adapter's driver key, REG_MULTI_SZ first, then REG_SZ; a first call with just the header to learn the
 * size). The probe puts its own values into the adapter's driver key (Control\Class\{4d36e968-...}\NNNN,
 * found from the device's Enum key "Driver" value) and into the key of a service it gives the device.
 * Layout: D3DDDI_QUERYREGISTRY_INFO (d3dukmdt.h), 552 bytes, the output union at 544. Prints ok/FAIL. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

typedef UINT D3DKMT_HANDLE;
typedef struct { WCHAR DeviceName[32]; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; UINT VidPnSourceId; } OPENBYNAME;
typedef struct { D3DKMT_HANDLE hAdapter; UINT Type; void *pPrivateDriverData; UINT PrivateDriverDataSize; } QAI;
typedef struct
{
    UINT QueryType;   /* 0 SERVICEKEY, 1 ADAPTERKEY, 2 DRIVERSTOREPATH, 3 DRIVERIMAGEPATH */
    UINT QueryFlags;  /* TranslatePath:1, MutableValue:1 */
    WCHAR ValueName[MAX_PATH];
    ULONG ValueType;
    ULONG PhysicalAdapterIndex;
    ULONG OutputValueSize;
    UINT Status;      /* 0 SUCCESS, 1 BUFFER_OVERFLOW, 2 FAIL */
    union { DWORD OutputDword; UINT64 OutputQword; WCHAR OutputString[1]; BYTE OutputBinary[1]; };
} QRI;
#define KMTQAITYPE_QUERYREGISTRY 48
#define STATUS_INVALID_PARAMETER_ ((NTSTATUS)0xc000000d)

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static NTSTATUS (WINAPI *pQuery)(QAI *);
static D3DKMT_HANDLE adapter;

static NTSTATUS query(QRI *info, UINT size, UINT type, UINT flags, const WCHAR *name, ULONG value_type)
{
    QAI qai;
    memset(info, 0xcc, size);
    info->QueryType = type;
    info->QueryFlags = flags;
    memset(info->ValueName, 0, sizeof(info->ValueName));
    lstrcpyW(info->ValueName, name);
    info->ValueType = value_type;
    info->PhysicalAdapterIndex = 0;
    qai.hAdapter = adapter;
    qai.Type = KMTQAITYPE_QUERYREGISTRY;
    qai.pPrivateDriverData = info;
    qai.PrivateDriverDataSize = size;
    return pQuery(&qai);
}

static const WCHAR class_key[] = L"System\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";

/* the Enum key of the first display device whose driver key is NNNN: its path, and NNNN */
static BOOL find_device(WCHAR *path, WCHAR *index)
{
    WCHAR dev[MAX_PATH], inst[MAX_PATH], full[MAX_PATH * 2], value[MAX_PATH];
    HKEY pci, d;
    DWORD i, j, n, size;
    BOOL found = FALSE;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Enum\\PCI", 0, KEY_READ, &pci)) return FALSE;
    for (i = 0; !found && (n = ARRAYSIZE(dev), !RegEnumKeyExW(pci, i, dev, &n, NULL, NULL, NULL, NULL)); i++)
    {
        if (RegOpenKeyExW(pci, dev, 0, KEY_READ, &d)) continue;
        for (j = 0; !found && (n = ARRAYSIZE(inst), !RegEnumKeyExW(d, j, inst, &n, NULL, NULL, NULL, NULL)); j++)
        {
            swprintf(full, ARRAYSIZE(full), L"System\\CurrentControlSet\\Enum\\PCI\\%ls\\%ls", dev, inst);
            size = sizeof(value);
            if (!RegGetValueW(HKEY_LOCAL_MACHINE, full, L"Driver", RRF_RT_REG_SZ, NULL, value, &size) &&
                !_wcsnicmp(value, L"{4d36e968-e325-11ce-bfc1-08002be10318}\\", 39))
            {
                lstrcpyW(path, full);
                lstrcpyW(index, value + 39);
                found = TRUE;
            }
        }
        RegCloseKey(d);
    }
    RegCloseKey(pci);
    return found;
}

int main(void)
{
    static const WCHAR multi[] = L"\\SystemRoot\\System32\\sg-a.json\0sg-b.json\0";   /* + the final NUL */
    static const WCHAR multi_t[] = L"C:\\windows\\System32\\sg-a.json\0sg-b.json\0";
    static const WCHAR sz[] = L"\\SystemRoot\\sg-c.json";
    HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
    NTSTATUS (WINAPI *pOpen)(OPENBYNAME *) = (void *)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
    DISPLAY_DEVICEW dd = {sizeof(dd)};
    WCHAR device[MAX_PATH * 2], index[16], key[MAX_PATH];
    union { QRI info; BYTE bytes[4096]; } u;
    OPENBYNAME byname;
    DWORD i, dword = 0x1234;
    NTSTATUS st;
    HKEY hkey;

    printf("layout %u %u %u\n", (UINT)sizeof(QRI), (UINT)offsetof(QRI, OutputDword), (UINT)offsetof(QRI, Status));
    check(sizeof(QRI) == 552 && offsetof(QRI, OutputDword) == 544, "the probe's D3DDDI_QUERYREGISTRY_INFO is Windows'");
    pQuery = (void *)GetProcAddress(gdi32, "D3DKMTQueryAdapterInfo");
    if (!pQuery || !pOpen) { printf("FAIL no D3DKMT exports\nRESULT: FAIL\n"); return 1; }
    for (i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); i++) if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) break;
    memset(&byname, 0, sizeof(byname));
    lstrcpyW(byname.DeviceName, dd.DeviceName);
    st = pOpen(&byname);
    check(!st, "the primary display's adapter: %#lx", st);
    adapter = byname.hAdapter;
    check(find_device(device, index), "the display device's Enum key %ls, driver key %ls", device, index);

    swprintf(key, ARRAYSIZE(key), L"%ls\\%ls", class_key, index);
    check(!RegCreateKeyExW(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL), "open %ls", key);
    RegSetValueExW(hkey, L"SgTestDword", 0, REG_DWORD, (BYTE *)&dword, sizeof(dword));
    RegSetValueExW(hkey, L"SgTestMulti", 0, REG_MULTI_SZ, (BYTE *)multi, sizeof(multi));
    RegSetValueExW(hkey, L"SgTestSz", 0, REG_SZ, (BYTE *)sz, sizeof(sz));
    RegCloseKey(hkey);
    RegSetKeyValueW(HKEY_LOCAL_MACHINE, device, L"Service", REG_SZ, L"SgTestSvc", sizeof(L"SgTestSvc"));
    dword = 7;
    RegSetKeyValueW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\SgTestSvc", L"SgSvcValue", REG_DWORD, &dword, sizeof(dword));

    /* a DWORD fits in the header's union */
    st = query(&u.info, sizeof(QRI), 1, 0, L"SgTestDword", REG_DWORD);
    check(!st && u.info.Status == 0 && u.info.OutputValueSize == 4 && u.info.OutputDword == 0x1234,
          "ADAPTERKEY REG_DWORD: %#lx status %u size %lu value %#lx", st, u.info.Status, u.info.OutputValueSize, u.info.OutputDword);
    check(u.info.ValueType == REG_DWORD && u.info.QueryType == 1, "  the request's fields are left as they were");

    /* the loader's way: the header alone first, then room for OutputValueSize */
    st = query(&u.info, sizeof(QRI), 1, 0, L"SgTestMulti", REG_MULTI_SZ);
    check(!st && u.info.Status == 1 && u.info.OutputValueSize == sizeof(multi),
          "REG_MULTI_SZ into the header alone: BUFFER_OVERFLOW, size %lu (%#lx status %u)", u.info.OutputValueSize, st, u.info.Status);
    st = query(&u.info, sizeof(QRI) + sizeof(multi), 1, 0, L"SgTestMulti", REG_MULTI_SZ);
    check(!st && u.info.Status == 0 && u.info.OutputValueSize == sizeof(multi) && !memcmp(u.info.OutputString, multi, sizeof(multi)),
          "REG_MULTI_SZ with room: the value as stored (%#lx status %u size %lu: %ls)", st, u.info.Status, u.info.OutputValueSize, u.info.OutputString);
    st = query(&u.info, sizeof(QRI) + 512, 1, 1, L"SgTestMulti", REG_MULTI_SZ);
    check(!st && u.info.Status == 0 && u.info.OutputValueSize == sizeof(multi_t) && !memcmp(u.info.OutputString, multi_t, sizeof(multi_t)),
          "TranslatePath: \\SystemRoot\\ becomes the Windows directory (%#lx status %u size %lu: %ls)", st, u.info.Status, u.info.OutputValueSize, u.info.OutputString);
    st = query(&u.info, sizeof(QRI) + 512, 1, 1, L"SgTestSz", REG_SZ);
    check(!st && u.info.Status == 0 && !lstrcmpW(u.info.OutputString, L"C:\\windows\\sg-c.json") &&
          u.info.OutputValueSize == sizeof(L"C:\\windows\\sg-c.json"),
          "TranslatePath of a REG_SZ: %ls (%lu bytes)", u.info.OutputString, u.info.OutputValueSize);

    /* the loader asks for REG_MULTI_SZ, then REG_SZ: another type fails the call */
    st = query(&u.info, sizeof(QRI) + 512, 1, 0, L"SgTestSz", REG_MULTI_SZ);
    check(st == STATUS_INVALID_PARAMETER_, "a REG_SZ asked as REG_MULTI_SZ: STATUS_INVALID_PARAMETER (%#lx)", st);
    st = query(&u.info, sizeof(QRI), 1, 0, L"SgNoSuchValue", REG_SZ);
    check(!st && u.info.Status == 2, "a missing value: STATUS_FAIL (%#lx status %u)", st, u.info.Status);
    st = query(&u.info, sizeof(QRI), 1, 1, L"VulkanDriverName", REG_MULTI_SZ);
    check(!st && u.info.Status == 2, "VulkanDriverName (winevulkan is registered under Khronos\\Vulkan\\Drivers): STATUS_FAIL (%#lx status %u)", st, u.info.Status);
    st = query(&u.info, sizeof(QRI) - 8, 1, 0, L"SgTestDword", REG_DWORD);
    check(st == STATUS_INVALID_PARAMETER_, "less room than the header: STATUS_INVALID_PARAMETER (%#lx)", st);
    st = query(&u.info, sizeof(QRI), 4, 0, L"SgTestDword", REG_DWORD);
    check(st == STATUS_INVALID_PARAMETER_, "an unknown query type: STATUS_INVALID_PARAMETER (%#lx)", st);

    /* SERVICEKEY: the key of the service the device names */
    st = query(&u.info, sizeof(QRI), 0, 0, L"SgSvcValue", REG_DWORD);
    check(!st && u.info.Status == 0 && u.info.OutputDword == 7, "SERVICEKEY: Services\\SgTestSvc's value (%#lx status %u value %lu)",
          st, u.info.Status, u.info.OutputDword);
    st = query(&u.info, sizeof(QRI), 2, 0, L"", REG_SZ);
    check(!st && u.info.Status == 2, "DRIVERSTOREPATH: STATUS_FAIL, Wine's drivers are in no driver store (%#lx status %u)", st, u.info.Status);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
