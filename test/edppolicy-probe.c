/* edppolicy-probe: Windows.Security.EnterpriseData.ProtectionPolicyManager
 * (patches/sg/1526) answers as Windows does where Windows Information
 * Protection manages no identity. Office (Excel, OneNote) looks it up as it
 * starts and opens files; the class was missing (0x80040154).
 * Calls by vtable slot (the SDK headers are not needed). Prints one line:
 * "factory=<hr> managed=<0|1> enabled=<0|1> level=<n> check=<n> decrypt=<0|1>
 *  view=<hr> identity=<text> context=<hr> primary=<null|text>" */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef void *HSTRING;
typedef struct { INT64 value; } EventRegistrationToken;
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

static const GUID IID_Statics  = {0xc0bffc66,0x8c3d,0x4d56,{0x88,0x04,0xc6,0x8f,0x0a,0xd3,0x2e,0xc5}};
static const GUID IID_Statics2 = {0xb68f9a8c,0x39e0,0x4649,{0xb2,0xe4,0x07,0x0a,0xb8,0xa5,0x79,0xb3}};
static const GUID IID_Statics4 = {0x20b794db,0xccbd,0x490f,{0x8c,0x83,0x49,0xcc,0xb7,0x7a,0xea,0x6c}};

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *pRoInitialize)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    HRESULT (WINAPI *pWindowsCreateString)(const WCHAR *, UINT32, HSTRING *) = (void *)GetProcAddress(combase, "WindowsCreateString");
    const WCHAR *(WINAPI *pWindowsGetStringRawBuffer)(HSTRING, UINT32 *) = (void *)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    HRESULT (WINAPI *pRoGetActivationFactory)(HSTRING, REFIID, void **) = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    static const WCHAR name[] = L"Windows.Security.EnterpriseData.ProtectionPolicyManager";
    void *statics = NULL, *statics2 = NULL, *statics4 = NULL, *manager = NULL, *context = NULL;
    HSTRING class_name, identity, got = NULL, source, target, primary = (HSTRING)1;
    unsigned char managed = 7, enabled = 7, decrypt = 7;
    int level = -1, check = -1;
    HRESULT hr, hv = E_FAIL, hc = E_FAIL;

    pRoInitialize(1);
    pWindowsCreateString(name, wcslen(name), &class_name);
    pWindowsCreateString(L"user@contoso.com", 16, &identity);
    pWindowsCreateString(L"user@contoso.com", 16, &source);
    pWindowsCreateString(L"", 0, &target);
    hr = pRoGetActivationFactory(class_name, &IID_Statics, &statics);
    if (SUCCEEDED(hr))
    {
        SLOT(statics, 6, HRESULT (WINAPI *)(void *, HSTRING, unsigned char *))(statics, identity, &managed);
        SLOT(statics, 19, HRESULT (WINAPI *)(void *, HSTRING, HSTRING, int *))(statics, source, target, &check);
        hv = SLOT(statics, 12, HRESULT (WINAPI *)(void *, void **))(statics, &manager);
        if (SUCCEEDED(hv))
        {
            SLOT(manager, 6, HRESULT (WINAPI *)(void *, HSTRING))(manager, identity);
            SLOT(manager, 7, HRESULT (WINAPI *)(void *, HSTRING *))(manager, &got);
        }
        hc = SLOT(statics, 9, HRESULT (WINAPI *)(void *, HSTRING, void **))(statics, identity, &context);
        if (SUCCEEDED(hc) && context) SLOT(context, 6, HRESULT (WINAPI *)(void *))(context); /* IClosable::Close */
        if (SUCCEEDED(pRoGetActivationFactory(class_name, &IID_Statics2, &statics2)))
        {
            SLOT(statics2, 14, HRESULT (WINAPI *)(void *, unsigned char *))(statics2, &enabled);
            SLOT(statics2, 9, HRESULT (WINAPI *)(void *, HSTRING, int *))(statics2, identity, &level);
            SLOT(statics2, 10, HRESULT (WINAPI *)(void *, HSTRING, unsigned char *))(statics2, identity, &decrypt);
        }
        if (SUCCEEDED(pRoGetActivationFactory(class_name, &IID_Statics4, &statics4)))
            SLOT(statics4, 15, HRESULT (WINAPI *)(void *, HSTRING *))(statics4, &primary);
    }
    printf("factory=%#lx managed=%d enabled=%d level=%d check=%d decrypt=%d view=%#lx identity=%ls context=%#lx primary=%ls\n",
           hr, managed, enabled, level, check, decrypt, hv, got ? pWindowsGetStringRawBuffer(got, NULL) : L"(null)", hc,
           primary == (HSTRING)1 ? L"(unset)" : primary ? pWindowsGetStringRawBuffer(primary, NULL) : L"null");
    return 0;
}
