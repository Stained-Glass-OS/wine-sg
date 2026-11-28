/* defdev-probe: programs hear the default output change (patches/sg/9419).
 *
 * Windows tells programs when the person picks another default output or input
 * (IMMNotificationClient::OnDefaultDeviceChanged, for each role, with the default device's ID);
 * players, games and voice chat follow it there. Wine told them only when its own registry
 * default changed (winecfg), never when the sound server's did (the volume flyout, Settings).
 * The probe registers a notification client, with no stream of its own and then with one
 * playing, and has the sound server's default output changed (pactl through start /unix).
 *
 * Prints name=value lines; see test/defdev-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>

static void say(const char *name, int ok, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s=%d %s\n", name, ok ? 1 : 0, buf);
    fflush(stdout);
}

static void unix_cmd(const char *cmd)
{
    char line[2048];
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    snprintf(line, sizeof(line), "C:\\windows\\system32\\start.exe /wait /unix /bin/sh -c \"%s\"", cmd);
    if (CreateProcessA(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}

static volatile LONG console, multimedia, communications, capture;
static WCHAR last_id[256];
static HRESULT WINAPI nc_QueryInterface(IMMNotificationClient *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI nc_AddRef(IMMNotificationClient *iface) { return 2; }
static ULONG WINAPI nc_Release(IMMNotificationClient *iface) { return 1; }
static HRESULT WINAPI nc_OnDeviceStateChanged(IMMNotificationClient *iface, LPCWSTR id, DWORD state) { return S_OK; }
static HRESULT WINAPI nc_OnDeviceAdded(IMMNotificationClient *iface, LPCWSTR id) { return S_OK; }
static HRESULT WINAPI nc_OnDeviceRemoved(IMMNotificationClient *iface, LPCWSTR id) { return S_OK; }
static HRESULT WINAPI nc_OnDefaultDeviceChanged(IMMNotificationClient *iface, EDataFlow flow, ERole role, LPCWSTR id)
{
    if (flow == eRender)
    {
        if (role == eConsole) { if (id) lstrcpynW(last_id, id, 256); InterlockedIncrement(&console); }
        if (role == eMultimedia) InterlockedIncrement(&multimedia);
        if (role == eCommunications) InterlockedIncrement(&communications);
    }
    else InterlockedIncrement(&capture);
    return S_OK;
}
static HRESULT WINAPI nc_OnPropertyValueChanged(IMMNotificationClient *iface, LPCWSTR id, const PROPERTYKEY key) { return S_OK; }
static const IMMNotificationClientVtbl nc_vtbl = { nc_QueryInterface, nc_AddRef, nc_Release, nc_OnDeviceStateChanged,
    nc_OnDeviceAdded, nc_OnDeviceRemoved, nc_OnDefaultDeviceChanged, nc_OnPropertyValueChanged };
static IMMNotificationClient client = { &nc_vtbl };

static int wait_for(volatile LONG *counter, LONG want, DWORD ms)
{
    DWORD t0 = GetTickCount();
    while (*counter < want && GetTickCount() - t0 < ms) Sleep(20);
    return *counter >= want;
}

int main(void)
{
    IMMDeviceEnumerator *mmde;
    IMMDevice *dev;
    IAudioClient *ac = NULL;
    WAVEFORMATEX *fmt;
    WCHAR *def_id = NULL;
    LONG before;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&mmde);
    hr = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(mmde, &client);
    IMMDeviceEnumerator_GetDefaultAudioEndpoint(mmde, eRender, eConsole, &dev);
    IMMDevice_GetId(dev, &def_id);
    say("registered", hr == S_OK, "RegisterEndpointNotificationCallback %#lx, default output %ls", hr, def_id);
    Sleep(1000);

    /* no stream of its own: the default output changes */
    unix_cmd("pactl set-default-sink sgnull2");
    wait_for(&console, 1, 4000);
    say("told", console >= 1, "OnDefaultDeviceChanged(eRender, eConsole) %ld time(s)", console);
    say("roles", multimedia >= 1 && communications >= 1, "eMultimedia %ld, eCommunications %ld", multimedia, communications);
    say("id", console >= 1 && !lstrcmpW(last_id, def_id), "with the default device's ID (%ls)", last_id);

    /* playing: and back */
    IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&ac);
    IAudioClient_GetMixFormat(ac, &fmt);
    IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, 0, 2000000, 0, fmt, NULL);
    IAudioClient_Start(ac);
    Sleep(500);
    before = console;
    unix_cmd("pactl set-default-sink sgnull");
    wait_for(&console, before + 1, 4000);
    say("told_playing", console > before, "with a stream playing: told %ld time(s)", console - before);

    /* the default input */
    before = capture;
    unix_cmd("pactl set-default-source sgnull2.monitor");
    wait_for(&capture, before + 1, 4000);
    say("capture", capture > before, "the default input changed: told %ld time(s)", capture - before);

    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
