/* dcomp (patches/sg/7828): WaitForCommitCompletion returns at the next vertical blank, as on Windows,
 * not at once -- a render loop paced by it (Avalonia's DwmRenderTimerLoop: Commit, Wait, draw) is
 * paced at the display's rate, not a busy loop. Both the version 1 device and the desktop device
 * (DCompositionCreateDevice2, which Avalonia uses): 60 Commit + Wait rounds take about a second at
 * 60 Hz (between 0.5 and 3 s here: the display's rate is 60 or more); 60 rounds returned at once
 * took a millisecond. Vtable slots by index: 3 Commit, 4 WaitForCommitCompletion. */
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *PFN_CREATE)(void *, REFIID, void **);
typedef HRESULT (STDMETHODCALLTYPE *f_void)(void *);
typedef ULONG (STDMETHODCALLTYPE *f_ref)(void *);
#define SLOT(obj, i) ((*(void ***)(obj))[i])

static const GUID IID_Device = { 0xc37ea93a, 0xe7aa, 0x450d, { 0xb1, 0x6f, 0x97, 0x46, 0xcb, 0x04, 0x07, 0xf3 } };
static const GUID IID_DesktopDevice = { 0x5f4633fe, 0x1e08, 0x4cb8, { 0x8c, 0x75, 0xce, 0x24, 0x33, 0x3f, 0x56, 0x02 } };
static int failures;

static void rounds(const char *what, void *dev)
{
    LARGE_INTEGER f, a, b;
    double s;
    int i;
    DEVMODEW mode = { .dmSize = sizeof(mode) };

    EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &mode);
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    for (i = 0; i < 60; i++)
    {
        ((f_void)SLOT(dev, 3))(dev);
        ((f_void)SLOT(dev, 4))(dev);
    }
    QueryPerformanceCounter(&b);
    s = (double)(b.QuadPart - a.QuadPart) / f.QuadPart;
    if (s >= 0.5 && s <= 3.0) printf("PASS  %s: 60 Commit + WaitForCommitCompletion rounds took %.2f s (display %lu Hz)\n", what, s, mode.dmDisplayFrequency);
    else { printf("FAIL  %s: 60 rounds took %.3f s (want about 1 s: once per vertical blank; display %lu Hz)\n", what, s, mode.dmDisplayFrequency); failures++; }
}

int main(void)
{
    HMODULE dcomp = LoadLibraryA("dcomp.dll");
    PFN_CREATE create = dcomp ? (PFN_CREATE)GetProcAddress(dcomp, "DCompositionCreateDevice") : NULL;
    PFN_CREATE create2 = dcomp ? (PFN_CREATE)GetProcAddress(dcomp, "DCompositionCreateDevice2") : NULL;
    void *dev = NULL, *desk = NULL;

    if (!create || FAILED(create(NULL, &IID_Device, &dev))) { printf("FAIL  no device\nRESULT: FAIL\n"); return 1; }
    rounds("the device", dev);
    ((f_ref)SLOT(dev, 2))(dev);
    if (!create2 || FAILED(create2(NULL, &IID_DesktopDevice, &desk))) { printf("FAIL  no desktop device\n"); failures++; }
    else { rounds("the desktop device", desk); ((f_ref)SLOT(desk, 2))(desk); }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
