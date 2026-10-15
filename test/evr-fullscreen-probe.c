/* Probe for patches/sg/4584: the EVR presenter's
 * IMFVideoDisplayControl::GetFullscreen says it is not in full-screen mode
 * (it never enters it), E_POINTER for NULL. Lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const GUID iid_d3ddevice9 = {0xd0223b96,0xbf7a,0x43fd,{0x92,0xbd,0xa4,0x3b,0x0d,0x82,0xb9,0xeb}};
static const GUID iid_displaycontrol = {0xa490b1e4,0xab84,0x4d31,{0xa1,0xb2,0x18,0x1e,0x03,0xb1,0x07,0x7a}};

/* IMFVideoDisplayControl: IUnknown(3), GetNativeVideoSize, GetIdealVideoSize, SetVideoPosition, GetVideoPosition,
 * SetAspectRatioMode, GetAspectRatioMode, SetVideoWindow, GetVideoWindow, RepaintVideo, GetCurrentImage,
 * SetBorderColor, GetBorderColor, SetRenderingPrefs, GetRenderingPrefs, SetFullscreen, GetFullscreen */
typedef HRESULT (WINAPI *fn_getfullscreen)(void *, BOOL *);
typedef ULONG (WINAPI *fn_release)(void *);

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA("evr.dll");
    HRESULT (WINAPI *p_create)(IUnknown *, REFIID, REFIID, void **);
    void *control = NULL, **vtbl;
    BOOL fs;
    HRESULT hr;
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    CoInitialize(NULL);
    p_create = (void *)GetProcAddress(mod, "MFCreateVideoPresenter");
    hr = p_create ? p_create(NULL, &iid_d3ddevice9, &iid_displaycontrol, &control) : E_FAIL;
    check(hr == S_OK && control, "MFCreateVideoPresenter %#lx", hr);
    if (!control) { printf("RESULT: FAIL\n"); return 1; }
    vtbl = *(void ***)control;

    check(((fn_getfullscreen)vtbl[18])(control, NULL) == E_POINTER, "GetFullscreen(NULL)");
    for (i = 0; i < 2; i++)
    {
        fs = 0xdead;
        hr = ((fn_getfullscreen)vtbl[18])(control, &fs);
        check(hr == S_OK && fs == FALSE, "GetFullscreen %u: %#lx %d", i, hr, fs);
    }
    ((fn_release)vtbl[2])(control);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
