/* nodriver-probe: a program that listens for audio device changes on a machine
 * whose sound server cannot be reached (no audio driver loads) -- wine-sg 9558.
 * Registers an IMMNotificationClient (as Steam, browsers and games do at start),
 * waits, asks for the default output, and ends. Prints its steps; DONE at the end. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <stdio.h>
static HRESULT WINAPI qi(IMMNotificationClient *t, REFIID r, void **p) { *p = t; return S_OK; }
static ULONG WINAPI ar(IMMNotificationClient *t) { return 2; }
static ULONG WINAPI rl(IMMNotificationClient *t) { return 1; }
static HRESULT WINAPI st(IMMNotificationClient *t, LPCWSTR id, DWORD s) { return S_OK; }
static HRESULT WINAPI ad(IMMNotificationClient *t, LPCWSTR id) { return S_OK; }
static HRESULT WINAPI dc(IMMNotificationClient *t, EDataFlow f, ERole r, LPCWSTR id) { return S_OK; }
static HRESULT WINAPI pv(IMMNotificationClient *t, LPCWSTR id, const PROPERTYKEY k) { return S_OK; }
static IMMNotificationClientVtbl vt = { qi, ar, rl, st, ad, ad, dc, pv };
static IMMNotificationClient client = { &vt };
static DWORD WINAPI watchdog(void *arg) { Sleep(15000); printf("HUNG\n"); fflush(stdout); ExitProcess(3); return 0; }
int main(void)
{
    IMMDeviceEnumerator *e; IMMDevice *dev = NULL; HRESULT hr;
    CloseHandle(CreateThread(NULL, 0, watchdog, NULL, 0, NULL));
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&e);
    printf("enumerator %#lx\n", hr); fflush(stdout);
    if (FAILED(hr)) return 1;
    hr = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(e, &client);
    printf("register %#lx\n", hr); fflush(stdout);
    Sleep(1500);
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(e, eRender, eConsole, &dev);
    printf("default %#lx\n", hr); fflush(stdout);
    if (dev) IMMDevice_Release(dev);
    hr = IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(e, &client);
    printf("unregister %#lx\n", hr);
    IMMDeviceEnumerator_Release(e);
    printf("DONE\n"); fflush(stdout);
    return 0;
}
