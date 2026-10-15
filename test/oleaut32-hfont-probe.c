/* Probe for patches/sg/5033: an HFONT crosses an apartment as a local handle
 * (RemotableHandle, WDT_INPROC_CALL): IFont::get_hFont through a proxy gives
 * the font's handle. The HFONT marshallers were stubs returning NULL.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <olectl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static IStream *stream;
static HANDLE ready, quit;
static HFONT local_font;
static IFont *font;

static DWORD WINAPI sta_thread(void *arg)
{
    FONTDESC desc = {sizeof(desc), (LPOLESTR)L"Arial", {{0}}, FW_BOLD, ANSI_CHARSET, FALSE, FALSE, FALSE};
    MSG msg;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    desc.cySize.int64 = 120000; /* 12 points */
    OleCreateFontIndirect(&desc, &IID_IFont, (void **)&font);
    if (font) IFont_get_hFont(font, &local_font);
    if (font) CoMarshalInterThreadInterfaceInStream(&IID_IFont, (IUnknown *)font, &stream);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &quit, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    if (font) IFont_Release(font);
    CoUninitialize();
    return 0;
}

int main(void)
{
    IFont *proxy = NULL;
    HFONT hfont = (HFONT)0xdeadbeef;
    LOGFONTW lf;
    HANDLE thread;
    BSTR name = NULL;
    HRESULT hr;
    unsigned int i;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    quit = CreateEventA(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    check(font && local_font, "font made in the STA (%p %p)", font, local_font);
    if (stream) CoGetInterfaceAndReleaseStream(stream, &IID_IFont, (void **)&proxy);
    check(proxy && proxy != font, "an IFont proxy (%p)", proxy);
    if (proxy)
    {
        hr = IFont_get_Name(proxy, &name);
        check(hr == S_OK && name && name[0], "get_Name through the proxy: %#lx %ls", hr, name ? name : L"");
        SysFreeString(name);
        for (i = 0; i < 2; i++)
        {
            hfont = (HFONT)0xdeadbeef;
            hr = IFont_get_hFont(proxy, &hfont);
            check(hr == S_OK && hfont == local_font, "get_hFont through the proxy (%u): %#lx %p (%p)", i, hr, hfont, local_font);
        }
        check(GetObjectW(hfont, sizeof(lf), &lf) && lf.lfWeight == FW_BOLD, "the handle is the bold font (%ld)", lf.lfWeight);
        IFont_Release(proxy);
    }
    SetEvent(quit);
    WaitForSingleObject(thread, 10000);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
