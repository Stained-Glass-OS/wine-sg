/* privacysettings-gate.sh's probe (patches/sg/1303): the WinRT settings Edge
 * asks for at start. Prints "NAME VALUE" lines. */
#include <windows.h>
#include <stdio.h>

typedef struct HSTRING__ *HSTRING;
typedef HRESULT (WINAPI *ro_get_factory)(HSTRING, REFIID, void **);
typedef HRESULT (WINAPI *create_string)(const WCHAR *, UINT32, HSTRING *);
typedef HRESULT (WINAPI *ro_init)(int);

/* the methods past IInspectable's six */
typedef struct { void **vtbl; } obj;
#define METHOD(o, i) ((void **)((obj *)(o))->vtbl)[6 + (i)]

static const GUID IID_PlatformDiagStatics = {0xb6e24c1b,0x7b1c,0x4b32,{0x8c,0x62,0xa6,0x65,0x97,0xce,0x72,0x3a}};
static const GUID IID_EducationStatics = {0xfc53f0ef,0x4d3e,0x4e13,{0x9b,0x23,0x50,0x5f,0x4d,0x09,0x1e,0x92}};
static const GUID IID_AssignedAccessStatics = {0x34a81d0d,0x8a29,0x5ef3,{0xa7,0xbe,0x61,0x8e,0x6a,0xc3,0xbd,0x01}};
static const GUID IID_DiagnosticsSettingsStatics = {0x72d2e80f,0x5390,0x4793,{0x99,0x0b,0x3c,0xcc,0x7d,0x6a,0xc9,0xc8}};

static ro_get_factory get_factory;
static create_string make_string;

static void *factory(const WCHAR *name, const GUID *iid, const char *label)
{
    HSTRING s;
    void *f = NULL;
    HRESULT hr;
    make_string(name, lstrlenW(name), &s);
    hr = get_factory(s, iid, &f);
    printf("%s-factory %08lx\n", label, hr);
    return SUCCEEDED(hr) ? f : NULL;
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    void *f, *o;
    int level = -1;
    unsigned char b;
    HRESULT hr;

    get_factory = (ro_get_factory)GetProcAddress(combase, "RoGetActivationFactory");
    make_string = (create_string)GetProcAddress(combase, "WindowsCreateString");
    ((ro_init)GetProcAddress(combase, "RoInitialize"))(1);

    if ((f = factory(L"Windows.System.Profile.PlatformDiagnosticsAndUsageDataSettings", &IID_PlatformDiagStatics, "diag")))
    {
        hr = ((HRESULT (WINAPI *)(void *, int *))METHOD(f, 0))(f, &level);
        printf("diag-level %08lx %d\n", hr, level);
        b = 9; hr = ((HRESULT (WINAPI *)(void *, int, unsigned char *))METHOD(f, 3))(f, 1, &b);
        printf("diag-can-basic %08lx %d\n", hr, b);
        b = 9; hr = ((HRESULT (WINAPI *)(void *, int, unsigned char *))METHOD(f, 3))(f, 3, &b);
        printf("diag-can-full %08lx %d\n", hr, b);
    }
    if ((f = factory(L"Windows.System.Profile.EducationSettings", &IID_EducationStatics, "edu")))
    {
        b = 9; hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(f, 0))(f, &b);
        printf("edu-is %08lx %d\n", hr, b);
    }
    if ((f = factory(L"Windows.System.UserProfile.AssignedAccessSettings", &IID_AssignedAccessStatics, "kiosk")))
    {
        o = NULL; hr = ((HRESULT (WINAPI *)(void *, void **))METHOD(f, 0))(f, &o);
        printf("kiosk-default %08lx\n", hr);
        if (o)
        {
            b = 9; hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(o, 0))(o, &b);
            printf("kiosk-enabled %08lx %d\n", hr, b);
            b = 9; hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(o, 1))(o, &b);
            printf("kiosk-single %08lx %d\n", hr, b);
        }
    }
    if ((f = factory(L"Windows.System.UserProfile.DiagnosticsSettings", &IID_DiagnosticsSettingsStatics, "tailor")))
    {
        o = NULL; hr = ((HRESULT (WINAPI *)(void *, void **))METHOD(f, 0))(f, &o);
        printf("tailor-default %08lx\n", hr);
        if (o)
        {
            b = 9; hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(o, 0))(o, &b);
            printf("tailor-can %08lx %d\n", hr, b);
        }
    }
    fflush(stdout);
    return 0;
}
