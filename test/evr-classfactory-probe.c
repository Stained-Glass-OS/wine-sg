/* evr class factories (patch 2996), run by test/evr-classfactory-gate.sh: DllGetClassObject for each
 * evr class, IClassFactory::LockServer(TRUE/FALSE) returns S_OK repeatedly (also unbalanced) without a
 * FIXME, the factory still creates objects afterwards; an unknown CLSID gives CLASS_E_CLASSNOTAVAILABLE
 * and a non-factory IID E_NOINTERFACE. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfidl.h>
#include <dshow.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

DEFINE_GUID(CLSID_EVR_, 0xfa10746c, 0x9b63, 0x4b6c, 0xbc, 0x49, 0xfc, 0x30, 0x0e, 0xa5, 0xf2, 0x56);
DEFINE_GUID(CLSID_Mixer9_, 0xe474e05a, 0xab65, 0x4f6a, 0x82, 0x7c, 0x21, 0x8b, 0x1b, 0xaa, 0xf3, 0x1f);
DEFINE_GUID(CLSID_Presenter9_, 0x98455561, 0x5136, 0x4d28, 0xab, 0x08, 0x4c, 0xee, 0x40, 0xea, 0x27, 0x81);
DEFINE_GUID(IID_D3D9_, 0x81bdcbca, 0x64d4, 0x426d, 0xae, 0x8d, 0xad, 0x01, 0x47, 0xf4, 0x27, 0x5c);
DEFINE_GUID(CLSID_Unknown, 0x7f3a1c00, 0x1111, 0x2222, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa);

static const struct { const GUID *clsid; const char *name; const GUID *riid; } classes[] =
{
    { &CLSID_EVR_, "EVR", &IID_IBaseFilter },
    { &CLSID_Mixer9_, "Mixer9", &IID_IMFTransform },
    { &CLSID_Presenter9_, "Presenter9", &IID_IUnknown },
};

int main(void)
{
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **);
    HMODULE evr;
    unsigned int i, j;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    evr = LoadLibraryA("evr.dll");
    check(!!evr, "LoadLibrary evr");
    if (!evr) { puts("RESULT: FAIL"); return 1; }
    get_class = (void *)GetProcAddress(evr, "DllGetClassObject");
    check(!!get_class, "DllGetClassObject exported");

    for (i = 0; i < 3; i++)
    {
        static const BOOL seq[] = { TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE };
        IClassFactory *cf = NULL;
        IUnknown *unk;
        ULONG ref;

        hr = get_class(classes[i].clsid, &IID_IClassFactory, (void **)&cf);
        CHECKF(hr == S_OK && cf, "%s: DllGetClassObject hr %#lx", classes[i].name, hr);
        if (!cf) continue;
        for (j = 0; j < sizeof(seq) / sizeof(*seq); j++)
        {
            hr = IClassFactory_LockServer(cf, seq[j]);
            CHECKF(hr == S_OK, "%s: LockServer(%d) #%u hr %#lx", classes[i].name, seq[j], j, hr);
        }
        unk = NULL;
        hr = IClassFactory_CreateInstance(cf, NULL, classes[i].riid, (void **)&unk);
        CHECKF(hr == S_OK && unk, "%s: CreateInstance after LockServer hr %#lx", classes[i].name, hr);
        if (unk) IUnknown_Release(unk);
        IClassFactory_AddRef(cf);
        ref = IClassFactory_Release(cf);
        CHECKF(ref == 1, "%s: refcount %lu", classes[i].name, ref);
        ref = IClassFactory_Release(cf);
        CHECKF(ref == 0, "%s: final refcount %lu", classes[i].name, ref);

        cf = (IClassFactory *)0x1;
        hr = get_class(classes[i].clsid, &IID_D3D9_, (void **)&cf);
        CHECKF(hr == E_NOINTERFACE, "%s: bad IID hr %#lx", classes[i].name, hr);
    }

    {
        IClassFactory *cf = NULL;
        hr = get_class(&CLSID_Unknown, &IID_IClassFactory, (void **)&cf);
        CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE && !cf, "unknown clsid hr %#lx", hr);
    }

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
