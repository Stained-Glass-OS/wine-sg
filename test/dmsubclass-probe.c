/* dmsubclass-probe: Direct Manipulation on a window the program subclasses
 * after it (wine-sg 9557), as Steam's web helper (Chromium) does: activate,
 * the program's own subclass on top, deactivate, activate again (a second
 * manager), then one message. It must reach the program's procedures once
 * each; before, it went round between the two subclass procedures until the
 * stack ran out and the web helper died, every few seconds, at its sign-in
 * window. Run by test/dmsubclass-gate.sh; prints RESULT lines. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <directmanipulation.h>
#include <stdio.h>
static WNDPROC prev;
static int hits_class, hits_sub;
static LRESULT CALLBACK class_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_USER + 1) { hits_class++; return 7; }
    return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK sub_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_USER + 1) hits_sub++;
    return CallWindowProcW(prev, h, m, w, l);
}
static IDirectManipulationManager *manager(void)
{
    IDirectManipulationManager *mgr = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_DirectManipulationManager, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IDirectManipulationManager, (void **)&mgr);
    if (FAILED(hr)) { printf("NOMANAGER %#lx\n", hr); fflush(stdout); exit(2); }
    return mgr;
}
int main(void)
{
    WNDCLASSW wc = {0}; HWND hwnd; IDirectManipulationManager *m1, *m2; LRESULT r;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    wc.lpfnWndProc = class_proc; wc.lpszClassName = L"dmsub"; RegisterClassW(&wc);
    hwnd = CreateWindowW(L"dmsub", L"dmsub", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, 0, 0, 0, 0);
    m1 = manager();
    printf("activate1 %#lx\n", IDirectManipulationManager_Activate(m1, hwnd));
    prev = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)sub_proc);
    printf("deactivate1 %#lx\n", IDirectManipulationManager_Deactivate(m1, hwnd));
    IDirectManipulationManager_Release(m1);
    m2 = manager();
    printf("activate2 %#lx\n", IDirectManipulationManager_Activate(m2, hwnd));
    fflush(stdout);
    r = SendMessageW(hwnd, WM_USER + 1, 0, 0);
    printf("RESULT ret %Id class %d sub %d\n", r, hits_class, hits_sub);
    IDirectManipulationManager_Deactivate(m2, hwnd);
    r = SendMessageW(hwnd, WM_USER + 1, 0, 0);
    printf("RESULT2 ret %Id class %d sub %d\n", r, hits_class, hits_sub);
    IDirectManipulationManager_Release(m2);
    DestroyWindow(hwnd);
    printf("DONE\n");
    return 0;
}
