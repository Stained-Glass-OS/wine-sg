/* fileprotect-probe: Windows.Security.EnterpriseData.FileProtectionManager (6260) answers as Windows
 * does where Windows Information Protection manages no identity: files are unprotected, protecting
 * or unprotecting leaves them so, nothing is a protected container.  OneDrive looks it up (the
 * class was missing). Calls by vtable slot. */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef void *HSTRING;
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

static const GUID IID_Statics  = {0x5846fc9b,0xe613,0x426b,{0xbb,0x38,0x88,0xcb,0xa1,0xdc,0x9a,0xdb}};
static const GUID IID_Statics2 = {0x83d2a745,0x0483,0x41ab,{0xb2,0xd5,0xbc,0x7f,0x23,0xd7,0x4e,0xbb}};
static const GUID IID_Statics3 = {0x6918849a,0x624f,0x46d6,{0xb2,0x41,0xe9,0xcd,0x5f,0xdf,0x3e,0x3f}};
static const GUID IID_Info2    = {0x82123a4c,0x557a,0x498d,{0x8e,0x94,0x94,0x4c,0xd5,0x83,0x64,0x32}};
static const GUID IID_AsyncInfo = {0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } else printf("ok   %s\n", #c); } while (0)

/* an operation's status (IAsyncInfo) and result */
static int op_status(void *op)
{
    void *info = NULL;
    int status = -1;
    if (SUCCEEDED(SLOT(op, 0, HRESULT (WINAPI *)(void *, REFIID, void **))(op, &IID_AsyncInfo, &info)) && info)
    {
        SLOT(info, 7, HRESULT (WINAPI *)(void *, int *))(info, &status);
        SLOT(info, 2, ULONG (WINAPI *)(void *))(info);
    }
    return status;
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *pRoInitialize)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    HRESULT (WINAPI *pWindowsCreateString)(const WCHAR *, UINT32, HSTRING *) = (void *)GetProcAddress(combase, "WindowsCreateString");
    const WCHAR *(WINAPI *pWindowsGetStringRawBuffer)(HSTRING, UINT32 *) = (void *)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    HRESULT (WINAPI *pRoGetActivationFactory)(HSTRING, REFIID, void **) = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    static const WCHAR name[] = L"Windows.Security.EnterpriseData.FileProtectionManager";
    void *statics = NULL, *statics2 = NULL, *statics3 = NULL, *op = NULL, *info = NULL, *info2 = NULL;
    HSTRING class_name, identity, got = (HSTRING)1, cls = NULL;
    int status = -1;
    unsigned char roam = 7, flag = 7;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    pRoInitialize(1);
    pWindowsCreateString(name, wcslen(name), &class_name);
    pWindowsCreateString(L"user@contoso.com", 16, &identity);
    hr = pRoGetActivationFactory(class_name, &IID_Statics, &statics);
    CHECK(hr == S_OK && statics, "the class is there: %#lx", hr);
    if (!statics) { printf("RESULT: FAIL\n"); return 1; }

    /* GetProtectionInfoAsync of an item (any object will do here) */
    hr = SLOT(statics, 8, HRESULT (WINAPI *)(void *, void *, void **))(statics, statics, &op);
    CHECK(hr == S_OK && op, "GetProtectionInfoAsync: %#lx", hr);
    if (op)
    {
        CHECK(op_status(op) == 1, "completed: %d", op_status(op));
        hr = SLOT(op, 8, HRESULT (WINAPI *)(void *, void **))(op, &info);
        CHECK(hr == S_OK && info, "a FileProtectionInfo: %#lx", hr);
    }
    if (info)
    {
        SLOT(info, 6, HRESULT (WINAPI *)(void *, int *))(info, &status);
        SLOT(info, 7, HRESULT (WINAPI *)(void *, unsigned char *))(info, &roam);
        SLOT(info, 8, HRESULT (WINAPI *)(void *, HSTRING *))(info, &got);
        CHECK(status == 1, "Unprotected: %d", status);
        CHECK(roam == 0 && got == NULL, "not roamable, no identity: %d %p", roam, got);
        SLOT(info, 4, HRESULT (WINAPI *)(void *, HSTRING *))(info, &cls);
        CHECK(cls && !wcscmp(pWindowsGetStringRawBuffer(cls, NULL), L"Windows.Security.EnterpriseData.FileProtectionInfo"),
              "its class: %ls", cls ? pWindowsGetStringRawBuffer(cls, NULL) : L"(none)");
        CHECK(SUCCEEDED(SLOT(info, 0, HRESULT (WINAPI *)(void *, REFIID, void **))(info, &IID_Info2, &info2)) && info2,
              "it has IFileProtectionInfo2");
        if (info2) SLOT(info2, 6, HRESULT (WINAPI *)(void *, unsigned char *))(info2, &flag);
        CHECK(flag == 0, "protect while open not supported: %d", flag);
    }
    op = NULL;
    hr = SLOT(statics, 8, HRESULT (WINAPI *)(void *, void *, void **))(statics, NULL, &op);
    CHECK(hr == E_INVALIDARG && !op, "no item: %#lx", hr);

    /* ProtectAsync leaves it unprotected (no managed identity); CopyProtectionAsync is done */
    op = NULL; info = NULL; status = -1;
    hr = SLOT(statics, 6, HRESULT (WINAPI *)(void *, void *, HSTRING, void **))(statics, statics, identity, &op);
    if (op && SUCCEEDED(SLOT(op, 8, HRESULT (WINAPI *)(void *, void **))(op, &info)) && info)
        SLOT(info, 6, HRESULT (WINAPI *)(void *, int *))(info, &status);
    CHECK(hr == S_OK && status == 1, "ProtectAsync: %#lx, status %d", hr, status);
    op = NULL; flag = 7;
    hr = SLOT(statics, 7, HRESULT (WINAPI *)(void *, void *, void *, void **))(statics, statics, statics, &op);
    if (op) SLOT(op, 8, HRESULT (WINAPI *)(void *, unsigned char *))(op, &flag);
    CHECK(hr == S_OK && flag == 1, "CopyProtectionAsync: %#lx, %d", hr, flag);

    /* IsContainerAsync: no; UnprotectAsync: unprotected */
    CHECK(SUCCEEDED(pRoGetActivationFactory(class_name, &IID_Statics2, &statics2)) && statics2, "statics 2");
    if (statics2)
    {
        op = NULL; flag = 7;
        hr = SLOT(statics2, 6, HRESULT (WINAPI *)(void *, void *, void **))(statics2, statics, &op);
        if (op) SLOT(op, 8, HRESULT (WINAPI *)(void *, unsigned char *))(op, &flag);
        CHECK(hr == S_OK && flag == 0, "IsContainerAsync: %#lx, %d", hr, flag);
    }
    CHECK(SUCCEEDED(pRoGetActivationFactory(class_name, &IID_Statics3, &statics3)) && statics3, "statics 3");
    if (statics3)
    {
        op = NULL; info = NULL; status = -1;
        hr = SLOT(statics3, 6, HRESULT (WINAPI *)(void *, void *, void **))(statics3, statics, &op);
        if (op && SUCCEEDED(SLOT(op, 8, HRESULT (WINAPI *)(void *, void **))(op, &info)) && info)
            SLOT(info, 6, HRESULT (WINAPI *)(void *, int *))(info, &status);
        CHECK(hr == S_OK && status == 1, "UnprotectAsync: %#lx, status %d", hr, status);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
