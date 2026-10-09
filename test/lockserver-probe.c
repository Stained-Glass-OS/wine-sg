/* The class factories' LockServer of qdvd, xaudio2_7 (the engine's and the
 * XAPO one) and evr (patches/sg/2837), run by test/lockserver-gate.sh. They
 * returned S_OK already and logged a FIXME: the gate fails when one is logged.
 *
 *   lockserver-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

DEFINE_GUID(CLSID_DVDNavigator_sg, 0x9b8c4620, 0x2c1a, 0x11d0, 0x84, 0x93, 0x00, 0xa0, 0x24, 0x38, 0xad, 0x48);
DEFINE_GUID(CLSID_EVR_sg, 0xfa10746c, 0x9b63, 0x4b6c, 0xbc, 0x49, 0xfc, 0x30, 0x0e, 0xa5, 0xf2, 0x56);
DEFINE_GUID(CLSID_XAudio2_sg, 0x5a508685, 0xa254, 0x4fba, 0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf);
DEFINE_GUID(CLSID_AudioVolumeMeter_sg, 0xcac1105f, 0x619b, 0x4d04, 0x83, 0x1a, 0x44, 0xe1, 0xcb, 0xf1, 0x2d, 0x57);
DEFINE_GUID(CLSID_AudioReverb_sg, 0x6a93130e, 0x1d53, 0x41d1, 0xa9, 0xcf, 0xe7, 0x58, 0x80, 0x0b, 0xb1, 0x79);

static const struct { const char *dll; const GUID *clsid; const char *name; } cases[] =
{
    {"qdvd.dll", &CLSID_DVDNavigator_sg, "qdvd DVD navigator"},
    {"evr.dll", &CLSID_EVR_sg, "evr video renderer"},
    {"xaudio2_7.dll", &CLSID_XAudio2_sg, "xaudio2_7 engine"},
    {"xaudio2_7.dll", &CLSID_AudioVolumeMeter_sg, "xaudio2_7 volume meter APO"},
    {"xaudio2_7.dll", &CLSID_AudioReverb_sg, "xaudio2_7 reverb APO"},
};

int main(void)
{
    size_t i;

    CoInitialize(NULL);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        HMODULE dll = LoadLibraryA(cases[i].dll);
        HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **) =
            dll ? (void *)GetProcAddress(dll, "DllGetClassObject") : NULL;
        IClassFactory *factory = NULL;
        HRESULT hr;

        if (!get_class_object || FAILED(hr = get_class_object(cases[i].clsid, &IID_IClassFactory, (void **)&factory)))
        {
            CHECKF(0, "%s: class factory", cases[i].name);
            continue;
        }
        hr = IClassFactory_LockServer(factory, TRUE);
        CHECKF(hr == S_OK, "%s: LockServer(TRUE) is S_OK (%#lx)", cases[i].name, hr);
        hr = IClassFactory_LockServer(factory, FALSE);
        CHECKF(hr == S_OK, "%s: LockServer(FALSE) is S_OK (%#lx)", cases[i].name, hr);
        IClassFactory_Release(factory);
    }
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
