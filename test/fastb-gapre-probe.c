/* riched20 smoke: IRichEditOle link count / context help, client site notifications */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <richole.h>
#include <ole2.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HWND hwnd; IRichEditOle *ole = NULL; IOleClientSite *site = NULL; HRESULT hr; LONG n;
    OleInitialize(NULL);
    LoadLibraryA("riched20.dll");
    hwnd = CreateWindowExA(0, "RichEdit20W", NULL, WS_POPUP | ES_MULTILINE, 0, 0, 200, 200, NULL, NULL, NULL, NULL);
    if (!hwnd) { printf("FAIL no window\n"); return 1; }
    SendMessageA(hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    CHECK(ole != NULL, "no IRichEditOle");
    if (ole)
    {
        n = ole->lpVtbl->GetLinkCount(ole);
        CHECK(n == 0, "GetLinkCount %ld", n);
        n = ole->lpVtbl->GetObjectCount(ole);
        CHECK(n == 0, "GetObjectCount %ld", n);
        hr = ole->lpVtbl->ContextSensitiveHelp(ole, TRUE);
        CHECK(hr == S_OK, "ContextSensitiveHelp %#lx", hr);
        hr = ole->lpVtbl->ContextSensitiveHelp(ole, FALSE);
        CHECK(hr == S_OK, "ContextSensitiveHelp off %#lx", hr);
        hr = ole->lpVtbl->GetClientSite(ole, &site);
        CHECK(hr == S_OK && site, "GetClientSite %#lx", hr);
        if (site)
        {
            IOleInPlaceSite *ips = NULL;
            hr = IOleClientSite_ShowObject(site);
            CHECK(hr == S_OK, "ShowObject %#lx", hr);
            hr = IOleClientSite_OnShowWindow(site, TRUE);
            CHECK(hr == S_OK, "OnShowWindow %#lx", hr);
            hr = IOleClientSite_QueryInterface(site, &IID_IOleInPlaceSite, (void **)&ips);
            CHECK(hr == S_OK && ips, "QI InPlaceSite %#lx", hr);
            if (ips)
            {
                hr = IOleInPlaceSite_ContextSensitiveHelp(ips, TRUE);
                CHECK(hr == S_OK, "InPlaceSite ContextSensitiveHelp %#lx", hr);
                IOleInPlaceSite_Release(ips);
            }
            IOleClientSite_Release(site);
        }
        ole->lpVtbl->Release(ole);
    }
    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failed ? "FAIL" : "PASS");
    return failed != 0;
}
