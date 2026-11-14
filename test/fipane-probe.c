/* test/fipane-gate.sh's probe (7782): FrameworkInputPane, the COM face of the input pane.
 * Avalonia (ILSpy 11) creates it for every window and stopped with REGDB_E_CLASSNOTREG.
 * The probe registers a handler for its window, then plays the on-screen keyboard (an
 * OSKMainClass window, as sg-osk's): the handler is told it shows, where, and that it hides.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shobjidl.h>
#include <stdio.h>

DEFINE_GUID(p_CLSID_FrameworkInputPane, 0xd5120aa3, 0x46ba, 0x44c5, 0x82, 0x2d, 0xca, 0x80, 0x92, 0xc1, 0xfc, 0x72);
DEFINE_GUID(p_IID_IFrameworkInputPane, 0x5752238b, 0x24f0, 0x495a, 0x82, 0xf1, 0x2f, 0xd5, 0x93, 0x05, 0x67, 0x96);
DEFINE_GUID(p_IID_IFrameworkInputPaneHandler, 0x226c537b, 0x1e76, 0x4d9e, 0xa7, 0x60, 0x33, 0xdb, 0x29, 0x92, 0x2f, 0x18);

static HANDLE shown_event, hidden_event;
static RECT shown_rect;
static LONG showings, hidings;

static HRESULT WINAPI h_QueryInterface(IFrameworkInputPaneHandler *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &p_IID_IFrameworkInputPaneHandler)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI h_AddRef(IFrameworkInputPaneHandler *iface) { return 2; }
static ULONG WINAPI h_Release(IFrameworkInputPaneHandler *iface) { return 1; }
static HRESULT WINAPI h_Showing(IFrameworkInputPaneHandler *iface, RECT *rect, BOOL ensure)
{
    shown_rect = *rect;
    InterlockedIncrement(&showings);
    SetEvent(shown_event);
    return S_OK;
}
static HRESULT WINAPI h_Hiding(IFrameworkInputPaneHandler *iface, BOOL ensure)
{
    InterlockedIncrement(&hidings);
    SetEvent(hidden_event);
    return S_OK;
}
static IFrameworkInputPaneHandlerVtbl h_vtbl = { h_QueryInterface, h_AddRef, h_Release, h_Showing, h_Hiding };
static IFrameworkInputPaneHandler handler = { &h_vtbl };

static HWND kbd;
static DWORD WINAPI keyboard_thread(void *arg)
{
    WNDCLASSW wc = {0};
    MSG msg;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"OSKMainClass";
    wc.hbrBackground = GetStockObject(GRAY_BRUSH);
    RegisterClassW(&wc);
    kbd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"OSKMainClass", L"On-Screen Keyboard", WS_POPUP,
                          100, 450, 600, 200, NULL, NULL, wc.hInstance, NULL);
    SetEvent(arg);
    while (GetMessageW(&msg, NULL, 0, 0)) DispatchMessageW(&msg);
    return 0;
}

static int wait(HANDLE event, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    for (;;)
    {
        DWORD now = GetTickCount();
        if ((LONG)(end - now) <= 0) return 0;
        if (MsgWaitForMultipleObjects(1, &event, FALSE, end - now, QS_ALLINPUT) == WAIT_OBJECT_0) return 1;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    }
}

int main(void)
{
    IFrameworkInputPane *pane = NULL;
    DWORD cookie = 0;
    HANDLE ready;
    HWND hwnd;
    RECT loc;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    shown_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hidden_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hwnd = CreateWindowExW(0, L"static", L"fipane probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 400, 300,
                           NULL, NULL, NULL, NULL);
    hr = CoCreateInstance(&p_CLSID_FrameworkInputPane, NULL, CLSCTX_INPROC_SERVER, &p_IID_IFrameworkInputPane, (void **)&pane);
    printf("Create=0x%08lx\n", hr);
    if (FAILED(hr)) return 0;
    hr = IFrameworkInputPane_AdviseWithHWND(pane, hwnd, &handler, &cookie);
    printf("Advise=0x%08lx %d\n", hr, cookie != 0);
    loc.left = 1; loc.top = 1; loc.right = 2; loc.bottom = 2;
    hr = IFrameworkInputPane_Location(pane, &loc);
    printf("LocationNone=0x%08lx %ld,%ld,%ld,%ld\n", hr, loc.left, loc.top, loc.right, loc.bottom);

    ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    CloseHandle(CreateThread(NULL, 0, keyboard_thread, ready, 0, NULL));
    WaitForSingleObject(ready, 5000);
    ShowWindow(kbd, SW_SHOWNOACTIVATE);
    printf("Showing=%d\n", wait(shown_event, 5000));
    printf("ShownRect=%ld,%ld,%ld,%ld\n", shown_rect.left, shown_rect.top, shown_rect.right, shown_rect.bottom);
    SetForegroundWindow(hwnd);
    hr = IFrameworkInputPane_Location(pane, &loc);
    printf("LocationShown=0x%08lx %ld,%ld,%ld,%ld\n", hr, loc.left, loc.top, loc.right, loc.bottom);
    ShowWindow(kbd, SW_HIDE);
    printf("Hiding=%d\n", wait(hidden_event, 5000));

    printf("Unadvise=0x%08lx\n", IFrameworkInputPane_Unadvise(pane, cookie));
    printf("UnadviseAgain=0x%08lx\n", IFrameworkInputPane_Unadvise(pane, cookie));
    ShowWindow(kbd, SW_SHOWNOACTIVATE);
    printf("AfterUnadvise=%d\n", wait(shown_event, 1500));
    IFrameworkInputPane_Release(pane);
    return 0;
}
