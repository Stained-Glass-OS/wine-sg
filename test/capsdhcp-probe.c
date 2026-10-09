/* Caption settings and DHCP client notifications (patches/sg/1700), run by
 * test/capsdhcp-gate.sh in a network namespace with a dummy interface d0:
 * Windows.Media.ClosedCaptioning.ClosedCaptionProperties' getters were
 * "semi-stubs" that always said Default and the computed colours were
 * E_NOTIMPL (E_INVALIDARG with no colour chosen, as on Windows); DhcpCApiInitialize/DhcpCApiCleanup were FIXMEs and
 * DhcpRegisterParamChange/DhcpDeRegisterParamChange "@ stub"s. The gate adds
 * an address to d0 once the probe has written the ready file.
 *
 *   capsdhcp-probe.exe READYFILE */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winstring.h>
#include <iphlpapi.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef struct { BYTE A, R, G, B; } color;
typedef struct { ULONG Flags, OptionId; BOOL IsVendor; BYTE *Data; DWORD nBytesData; } params;
typedef struct { ULONG nParams; params *Params; } params_array;

static const IID iid_statics = { 0x10aa1f84, 0xcc30, 0x4141, { 0xb5, 0x03, 0x52, 0x72, 0x28, 0x9e, 0x0c, 0x20 } };
static const IID iid_factory = { 0x00000035, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const WCHAR class_name[] = L"Windows.Media.ClosedCaptioning.ClosedCaptionProperties";
#define CAPTIONS_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\ClosedCaptioning"

typedef HRESULT (WINAPI *get_enum)(void *, int *);
typedef HRESULT (WINAPI *get_color)(void *, color *);
typedef HRESULT (WINAPI *get_name)(void *, HSTRING *);
typedef HRESULT (WINAPI *get_trust)(void *, int *);

static int enum_at(void *iface, int slot)
{
    int v = -1;
    ((get_enum)(*(void ***)iface)[slot])(iface, &v);
    return v;
}

static color color_at(void *iface, int slot)
{
    color c = { 0 };
    ((get_color)(*(void ***)iface)[slot])(iface, &c);
    return c;
}

static void set_value(const WCHAR *name, DWORD value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, CAPTIONS_KEY, name, REG_DWORD, &value, sizeof(value));
}

static void test_captions(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *pRoInitialize)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    HRESULT (WINAPI *pRoGetActivationFactory)(HSTRING, REFIID, void **) = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *pWindowsCreateString)(const WCHAR *, UINT32, HSTRING *) = (void *)GetProcAddress(combase, "WindowsCreateString");
    const WCHAR *(WINAPI *pWindowsGetStringRawBuffer)(HSTRING, UINT32 *) = (void *)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    void *statics = NULL, *factory = NULL;
    HSTRING name = NULL, got = NULL;
    color c;
    int trust = -1;
    HRESULT hr;

    pRoInitialize(1);
    pWindowsCreateString(class_name, lstrlenW(class_name), &name);
    hr = pRoGetActivationFactory(name, &iid_statics, &statics);
    check(hr == S_OK && statics, "ClosedCaptionProperties");
    if (!statics) return;
    pRoGetActivationFactory(name, &iid_factory, &factory);

    RegDeleteTreeW(HKEY_CURRENT_USER, CAPTIONS_KEY);
    check(enum_at(statics, 6) == 0 && enum_at(statics, 9) == 0, "nothing chosen: Default");
    check(((get_color)(*(void ***)statics)[7])(statics, &c) == E_INVALIDARG, "ComputedFontColor with none chosen: E_INVALIDARG");
    set_value(L"CaptionColor", 6);       /* yellow */
    set_value(L"CaptionSize", 4);        /* 200% */
    set_value(L"CaptionFontStyle", 7);   /* small capitals */
    set_value(L"CaptionEffects", 5);     /* drop shadow */
    set_value(L"BackgroundOpacity", 3);  /* 25% */
    set_value(L"RegionColor", 5);        /* blue */
    set_value(L"RegionOpacity", 99);     /* not valid */
    check(enum_at(statics, 6) == 6, "FontColor: the user's (was always Default)");
    c = color_at(statics, 7);
    check(c.A == 255 && c.R == 255 && c.G == 255 && !c.B, "ComputedFontColor: yellow (was E_NOTIMPL)");
    check(enum_at(statics, 9) == 4 && enum_at(statics, 10) == 7 && enum_at(statics, 11) == 5, "size, style and effect");
    check(enum_at(statics, 14) == 3, "BackgroundOpacity");
    c = color_at(statics, 16);
    check(!c.R && !c.G && c.B == 255, "ComputedRegionColor: blue");
    check(enum_at(statics, 17) == 0, "a value not valid: Default");
    RegDeleteTreeW(HKEY_CURRENT_USER, CAPTIONS_KEY);

    if (factory)
    {
        hr = ((get_name)(*(void ***)factory)[4])(factory, &got);
        check(hr == S_OK && !lstrcmpW(pWindowsGetStringRawBuffer(got, NULL), class_name),
              "the factory's GetRuntimeClassName (was E_NOTIMPL)");
        hr = ((get_trust)(*(void ***)factory)[5])(factory, &trust);
        check(hr == S_OK && trust == 0, "GetTrustLevel: BaseTrust");
    }
}

static void test_dhcp(const char *ready)
{
    HMODULE dhcp = LoadLibraryA("dhcpcsvc.dll");
    DWORD (WINAPI *pInit)(DWORD *) = (void *)GetProcAddress(dhcp, "DhcpCApiInitialize");
    void (WINAPI *pCleanup)(void) = (void *)GetProcAddress(dhcp, "DhcpCApiCleanup");
    DWORD (WINAPI *pRegister)(DWORD, void *, WCHAR *, void *, params_array, void *) = (void *)GetProcAddress(dhcp, "DhcpRegisterParamChange");
    DWORD (WINAPI *pDeregister)(DWORD, void *, void *) = (void *)GetProcAddress(dhcp, "DhcpDeRegisterParamChange");
    params_array none = { 0, NULL };
    HANDLE any = NULL, d0 = NULL, lo = NULL, bad = NULL;
    WCHAR d0_name[64] = L"", lo_name[64] = L"";
    IP_ADAPTER_ADDRESSES *addrs, *a;
    ULONG size = 65536;
    DWORD version = 0;
    FILE *f;

    check(pRegister && pDeregister, "DhcpRegisterParamChange and DhcpDeRegisterParamChange are there");
    if (!pRegister || !pDeregister) return;
    check(pInit(&version) == ERROR_SUCCESS && version == 2, "DhcpCApiInitialize");
    check(pInit(NULL) == ERROR_INVALID_PARAMETER, "no version: ERROR_INVALID_PARAMETER");

    addrs = malloc(size);
    if (!GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addrs, &size))
        for (a = addrs; a; a = a->Next)
        {
            if (!lstrcmpW(a->FriendlyName, L"d0")) MultiByteToWideChar(CP_ACP, 0, a->AdapterName, -1, d0_name, 64);
            if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) MultiByteToWideChar(CP_ACP, 0, a->AdapterName, -1, lo_name, 64);
        }
    free(addrs);
    printf("      d0 %ls, loopback %ls\n", d0_name, lo_name);

    check(pRegister(0, NULL, NULL, NULL, none, &any) == ERROR_INVALID_PARAMETER, "flags 0: refused");
    check(pRegister(1, NULL, NULL, NULL, none, &any) == ERROR_SUCCESS && any, "any adapter: an event");
    check(d0_name[0] && pRegister(1, NULL, d0_name, NULL, none, &d0) == ERROR_SUCCESS && d0, "d0: an event");
    if (lo_name[0]) pRegister(1, NULL, lo_name, NULL, none, &lo);
    check(pRegister(1, NULL, (WCHAR *)L"{00000000-0000-0000-0000-000000000000}", NULL, none, &bad) != ERROR_SUCCESS,
          "an unknown adapter: refused");

    if ((f = fopen(ready, "w"))) fclose(f);
    check(any && WaitForSingleObject(any, 15000) == WAIT_OBJECT_0, "an address added to d0: the any-adapter event");
    check(d0 && WaitForSingleObject(d0, 5000) == WAIT_OBJECT_0, "and d0's");
    if (lo) check(WaitForSingleObject(lo, 1000) == WAIT_TIMEOUT, "not the loopback's");

    check(pDeregister(1, NULL, any) == ERROR_SUCCESS && pDeregister(1, NULL, any) == ERROR_INVALID_PARAMETER,
          "DhcpDeRegisterParamChange, once");
    if (d0) pDeregister(1, NULL, d0);
    if (lo) pDeregister(1, NULL, lo);
    pCleanup();
}

int main(int argc, char **argv)
{
    test_captions();
    test_dhcp(argc > 1 ? argv[1] : "ready");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
