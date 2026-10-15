/* Probe for patches/sg/4579: the DirectSound renderer's
 * IAMDirectSound::SetFocusWindow and GetFocusWindow keep the window and
 * whether the sound stays audible without the focus. Table driven; lines
 * "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <amaudio.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef IAMDirectSound_SetFocusWindow
#define IAMDirectSound_SetFocusWindow(p, a, b) (p)->lpVtbl->SetFocusWindow(p, a, b)
#define IAMDirectSound_GetFocusWindow(p, a, b) (p)->lpVtbl->GetFocusWindow(p, a, b)
#endif

static const GUID iid_amdirectsound = {0x546f4260,0xd53e,0x11cf,{0xb3,0xf0,0x00,0xaa,0x00,0x37,0x61,0xc5}};

int main(int argc, char **argv)
{
    IBaseFilter *filter;
    IAMDirectSound *ds;
    HWND hwnd, desktop = GetDesktopWindow();
    BOOL audible;
    HRESULT hr;
    unsigned int i;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
    if (hr == VFW_E_NO_AUDIO_HARDWARE) { printf("ok   no audio device: skipped\nRESULT: PASS\n"); return 0; }
    check(hr == S_OK, "DirectSound renderer %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }
    hr = IBaseFilter_QueryInterface(filter, &iid_amdirectsound, (void **)&ds);
    check(hr == S_OK, "IAMDirectSound %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }

    check(IAMDirectSound_GetFocusWindow(ds, NULL, &audible) == E_POINTER, "GetFocusWindow(NULL, p)");
    check(IAMDirectSound_GetFocusWindow(ds, &hwnd, NULL) == E_POINTER, "GetFocusWindow(p, NULL)");
    hwnd = (HWND)0xdead; audible = 0xdead;
    hr = IAMDirectSound_GetFocusWindow(ds, &hwnd, &audible);
    check(hr == S_OK && !hwnd && audible == TRUE, "no window at first, audible: %#lx %p %d", hr, hwnd, audible);

    {
        static const struct { int window; BOOL audible; } cases[] = {{1, FALSE}, {1, TRUE}, {0, FALSE}, {0, TRUE}, {1, FALSE}};
        for (i = 0; i < ARRAYSIZE(cases); i++)
        {
            HWND want = cases[i].window ? desktop : NULL;

            hr = IAMDirectSound_SetFocusWindow(ds, want, cases[i].audible);
            check(hr == S_OK, "SetFocusWindow(%p, %d): %#lx", want, cases[i].audible, hr);
            hwnd = (HWND)0xdead; audible = 0xdead;
            hr = IAMDirectSound_GetFocusWindow(ds, &hwnd, &audible);
            check(hr == S_OK && hwnd == want && audible == cases[i].audible, "GetFocusWindow: %#lx %p %d", hr, hwnd, audible);
        }
    }

    IUnknown_Release((IUnknown *)ds);
    IBaseFilter_Release(filter);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
