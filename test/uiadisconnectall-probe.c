/* uiadisconnectall-probe: UiaDisconnectAllProviders (patches/sg/7000).
 *
 *   uiadisconnectall-probe.exe            the provider side: a window whose
 *                                         WM_GETOBJECT returns a UIA provider;
 *                                         starts the client, then disconnects
 *                                         every provider when it asks
 *   uiadisconnectall-probe.exe client HWND  the client side (another process)
 *
 * Electron and Chromium import UiaDisconnectAllProviders (delay-loaded); it
 * was not exported at all, so the delay-load failed where they call it.
 * Prints key=1|0 lines.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <stdio.h>

static HRESULT (WINAPI *pUiaDisconnectAllProviders)(void);

/* ---- a minimal provider ---- */
struct prov { IRawElementProviderSimple iface; LONG ref; HWND hwnd; };
static struct prov provider;

static HRESULT WINAPI p_QI(IRawElementProviderSimple *i, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRawElementProviderSimple))
    { *out = i; IRawElementProviderSimple_AddRef(i); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI p_AddRef(IRawElementProviderSimple *i) { return InterlockedIncrement(&provider.ref); }
static ULONG WINAPI p_Release(IRawElementProviderSimple *i) { return InterlockedDecrement(&provider.ref); }
static HRESULT WINAPI p_opts(IRawElementProviderSimple *i, enum ProviderOptions *o)
{ *o = ProviderOptions_ServerSideProvider; return S_OK; }
static HRESULT WINAPI p_pattern(IRawElementProviderSimple *i, PATTERNID id, IUnknown **o) { *o = NULL; return S_OK; }
static HRESULT WINAPI p_prop(IRawElementProviderSimple *i, PROPERTYID id, VARIANT *v)
{
    VariantInit(v);
    if (id == UIA_NamePropertyId) { V_VT(v) = VT_BSTR; V_BSTR(v) = SysAllocString(L"sg-provider"); }
    else if (id == UIA_ControlTypePropertyId) { V_VT(v) = VT_I4; V_I4(v) = UIA_ButtonControlTypeId; }
    return S_OK;
}
static HRESULT WINAPI p_host(IRawElementProviderSimple *i, IRawElementProviderSimple **o)
{
    return UiaHostProviderFromHwnd(provider.hwnd, o);
}
static IRawElementProviderSimpleVtbl pvtbl = { p_QI, p_AddRef, p_Release, p_opts, p_pattern, p_prop, p_host };

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_GETOBJECT && (LONG)lp == UiaRootObjectId)
        return UiaReturnRawElementProvider(hwnd, wp, lp, &provider.iface);
    if (msg == WM_APP)
    {
        LONG before = provider.ref;
        HRESULT hr = pUiaDisconnectAllProviders();
        printf("provider_ref_before=%ld provider_ref_after=%ld\n", before, provider.ref);
        printf("disconnect_ok=%d\n", hr == S_OK);
        printf("provider_released=%d\n", before > 1 && provider.ref == 1);
        fflush(stdout);
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int client(HWND hwnd)
{
    HUIANODE node;
    VARIANT v;
    HRESULT hr;
    int ok;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = UiaNodeFromHandle(hwnd, &node);
    if (FAILED(hr)) { printf("client_node=0 (hr %#lx)\n", hr); return 1; }
    hr = UiaGetPropertyValue(node, UIA_NamePropertyId, &v);
    ok = hr == S_OK && V_VT(&v) == VT_BSTR && !lstrcmpW(V_BSTR(&v), L"sg-provider");
    printf("client_name_before=%d\n", ok);
    VariantClear(&v);
    fflush(stdout);

    /* the provider's process disconnects everything */
    SendMessageW(hwnd, WM_APP, 0, 0);

    hr = UiaGetPropertyValue(node, UIA_NamePropertyId, &v);
    printf("client_after_hr=%#lx\n", hr);
    printf("client_unavailable_after=%d\n", hr == UIA_E_ELEMENTNOTAVAILABLE);
    if (SUCCEEDED(hr)) VariantClear(&v);
    printf("client_release=%d\n", UiaNodeRelease(node));
    fflush(stdout);
    CoUninitialize();
    return 0;
}

int main(int argc, char **argv)
{
    HMODULE uia = LoadLibraryA("uiautomationcore.dll");
    WNDCLASSW wc = { 0 };
    char cmd[MAX_PATH + 64], self[MAX_PATH];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HWND hwnd;
    MSG msg;
    DWORD rc;

    pUiaDisconnectAllProviders = (void *)GetProcAddress(uia, "UiaDisconnectAllProviders");
    if (argc > 2 && !strcmp(argv[1], "client")) return client((HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16));

    printf("exported=%d\n", pUiaDisconnectAllProviders != NULL);
    if (!pUiaDisconnectAllProviders) { printf("done=1\n"); return 0; }

    /* with nothing handed out it is still S_OK */
    printf("empty_ok=%d\n", pUiaDisconnectAllProviders() == S_OK);

    provider.iface.lpVtbl = &pvtbl;
    provider.ref = 1;
    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = L"sg_uia_probe";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"sg_uia_probe", L"uia probe", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    provider.hwnd = hwnd;
    fflush(stdout);

    GetModuleFileNameA(NULL, self, sizeof(self));
    sprintf(cmd, "\"%s\" client %Ix", self, (ULONG_PTR)hwnd);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { printf("spawn=0\n"); return 1; }
    while (MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 30000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    GetExitCodeProcess(pi.hProcess, &rc);
    printf("client_exit=%lu\n", rc);
    DestroyWindow(hwnd);
    printf("done=1\n");
    return 0;
}
