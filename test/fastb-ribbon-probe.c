/* UI ribbon framework object model and the image factory (patch 3579). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <stdio.h>
#include <uiribbon.h>

#define UI_E_VALUE_NOT_SET ((HRESULT)0x802A0005)
static void InitI4(LONG val, PROPVARIANT *v) { PropVariantInit(v); v->vt = VT_I4; v->lVal = val; }
#define InitPropVariantFromInt32(val, v) InitI4(val, v)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int view_create, view_destroy, update_calls, create_cmd;
static UINT32 last_update_cmd;

typedef struct { IUICommandHandler iface; LONG ref; } Handler;
typedef struct { IUIApplication iface; LONG ref; Handler handler; } App;

static HRESULT WINAPI h_QI(IUICommandHandler *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IUICommandHandler)) { *ppv = i; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI h_AddRef(IUICommandHandler *i) { return 2; }
static ULONG WINAPI h_Release(IUICommandHandler *i) { return 1; }
static HRESULT WINAPI h_Execute(IUICommandHandler *i, UINT32 id, UI_EXECUTIONVERB v, const PROPERTYKEY *k, const PROPVARIANT *c, IUISimplePropertySet *p) { return S_OK; }
static HRESULT WINAPI h_Update(IUICommandHandler *i, UINT32 id, REFPROPERTYKEY key, const PROPVARIANT *cur, PROPVARIANT *nv)
{
    update_calls++;
    last_update_cmd = id;
    InitPropVariantFromInt32(1000 + update_calls, nv);
    return S_OK;
}
static IUICommandHandlerVtbl h_vtbl = { h_QI, h_AddRef, h_Release, h_Execute, h_Update };

static HRESULT WINAPI a_QI(IUIApplication *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IUIApplication)) { *ppv = i; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI a_AddRef(IUIApplication *i) { return 2; }
static ULONG WINAPI a_Release(IUIApplication *i) { return 1; }
static HRESULT WINAPI a_View(IUIApplication *i, UINT32 id, UI_VIEWTYPE t, IUnknown *view, UI_VIEWVERB verb, INT32 reason)
{
    IUIRibbon *rib = NULL;
    if (verb == UI_VIEWVERB_CREATE)
    {
        view_create++;
        if (t != UI_VIEWTYPE_RIBBON || id != 0 || FAILED(IUnknown_QueryInterface(view, &IID_IUIRibbon, (void **)&rib))) view_create += 100;
        if (rib) IUIRibbon_Release(rib);
    }
    if (verb == UI_VIEWVERB_DESTROY) view_destroy++;
    return S_OK;
}
static App app;
static HRESULT WINAPI a_Create(IUIApplication *i, UINT32 id, UI_COMMANDTYPE t, IUICommandHandler **h)
{
    create_cmd++;
    *h = &app.handler.iface;
    return S_OK;
}
static HRESULT WINAPI a_Destroy(IUIApplication *i, UINT32 id, UI_COMMANDTYPE t, IUICommandHandler *h) { return S_OK; }
static IUIApplicationVtbl a_vtbl = { a_QI, a_AddRef, a_Release, a_View, a_Create, a_Destroy };

static void pump(DWORD ms)
{
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < ms)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

int main(void)
{
    static const PROPERTYKEY key = { { 0x12345678, 0x1234, 0x1234, { 1, 2, 3, 4, 5, 6, 7, 8 } }, 7 };
    IUIFramework *fw;
    IUIRibbon *ribbon;
    IUIImageFromBitmap *factory;
    IUIImage *image;
    IStream *stream;
    PROPVARIANT v;
    HBITMAP bmp, got;
    HWND hwnd;
    UINT32 h;
    HRESULT hr;

    app.iface.lpVtbl = &a_vtbl;
    app.handler.iface.lpVtbl = &h_vtbl;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    hr = CoCreateInstance(&CLSID_UIRibbonFramework, NULL, CLSCTX_INPROC_SERVER, &IID_IUIFramework, (void **)&fw);
    if (FAILED(hr)) { printf("FAIL  no UIRibbonFramework (%#lx)\nRESULT: FAIL\n", hr); return 1; }
    hwnd = CreateWindowW(L"static", L"ribbon host", WS_OVERLAPPEDWINDOW, 0, 0, 400, 300, NULL, NULL, NULL, NULL);

    check(IUIFramework_Initialize(fw, NULL, &app.iface) == E_INVALIDARG, "Initialize without window");
    check(IUIFramework_Initialize(fw, hwnd, NULL) == E_INVALIDARG, "Initialize without application");
    check(IUIFramework_LoadUI(fw, GetModuleHandleW(NULL), L"APPLICATION_RIBBON") == E_UNEXPECTED, "LoadUI before Initialize");
    check(IUIFramework_Initialize(fw, hwnd, &app.iface) == S_OK, "Initialize");
    check(IUIFramework_Initialize(fw, hwnd, &app.iface) == E_UNEXPECTED, "Initialize twice");
    hr = IUIFramework_LoadUI(fw, GetModuleHandleW(NULL), L"NO_SUCH_RIBBON");
    check(hr == HRESULT_FROM_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND), "LoadUI of a missing resource");
    check(view_create == 0, "... no view was created");
    hr = IUIFramework_LoadUI(fw, GetModuleHandleW(NULL), L"APPLICATION_RIBBON");
    check(hr == S_OK, "LoadUI");
    check(view_create == 1, "OnViewChanged(CREATE) with the IUIRibbon view");
    check(IUIFramework_LoadUI(fw, GetModuleHandleW(NULL), L"APPLICATION_RIBBON") == E_UNEXPECTED, "LoadUI twice");

    ribbon = NULL;
    check(IUIFramework_GetView(fw, 0, &IID_IUIRibbon, (void **)&ribbon) == S_OK && ribbon, "GetView(0) is the ribbon");
    h = 99;
    check(ribbon && IUIRibbon_GetHeight(ribbon, &h) == S_OK && h == 0, "GetHeight");
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    check(ribbon && IUIRibbon_SaveSettingsToStream(ribbon, stream) == S_OK, "SaveSettingsToStream");
    {
        LARGE_INTEGER zero = { 0 };
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    }
    check(ribbon && IUIRibbon_LoadSettingsFromStream(ribbon, stream) == S_OK, "settings round trip");
    IStream_Release(stream);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    check(ribbon && IUIRibbon_LoadSettingsFromStream(ribbon, stream) != S_OK, "empty settings refused");
    IStream_Release(stream);
    if (ribbon) IUIRibbon_Release(ribbon);
    ribbon = NULL;
    check(IUIFramework_GetView(fw, 5, &IID_IUIRibbon, (void **)&ribbon) == E_INVALIDARG && !ribbon, "GetView of an unknown view");

    /* command properties */
    InitPropVariantFromInt32(42, &v);
    check(IUIFramework_SetUICommandProperty(fw, 7, &key, &v) == S_OK, "SetUICommandProperty");
    PropVariantClear(&v);
    PropVariantInit(&v);
    check(IUIFramework_GetUICommandProperty(fw, 7, &key, &v) == S_OK && v.vt == VT_I4 && v.lVal == 42, "GetUICommandProperty");
    PropVariantClear(&v);
    PropVariantInit(&v);
    check(IUIFramework_GetUICommandProperty(fw, 8, &key, &v) == UI_E_VALUE_NOT_SET, "a property never set");

    /* invalidation */
    check(IUIFramework_InvalidateUICommand(fw, 7, UI_INVALIDATIONS_PROPERTY, NULL) == E_INVALIDARG, "PROPERTY invalidation needs a key");
    check(IUIFramework_InvalidateUICommand(fw, 7, 0, NULL) == E_INVALIDARG, "no flags");
    check(IUIFramework_InvalidateUICommand(fw, 7, UI_INVALIDATIONS_PROPERTY, &key) == S_OK, "InvalidateUICommand");
    check(update_calls == 0, "... not run at once");
    check(IUIFramework_FlushPendingInvalidations(fw) == S_OK, "FlushPendingInvalidations");
    check(update_calls == 1 && last_update_cmd == 7 && create_cmd == 1, "the handler was asked to update the property");
    PropVariantInit(&v);
    check(IUIFramework_GetUICommandProperty(fw, 7, &key, &v) == S_OK && v.vt == VT_I4 && v.lVal == 1001, "the new value is kept");
    PropVariantClear(&v);
    IUIFramework_InvalidateUICommand(fw, 7, UI_INVALIDATIONS_ALLPROPERTIES, NULL);
    pump(500);
    check(update_calls == 2, "an invalidation is also run from the message loop");
    check(IUIFramework_SetModes(fw, 0x5) == S_OK, "SetModes");

    check(IUIFramework_Destroy(fw) == S_OK, "Destroy");
    check(view_destroy == 1, "OnViewChanged(DESTROY)");
    check(IUIFramework_Destroy(fw) == E_UNEXPECTED, "Destroy twice");
    IUIFramework_Release(fw);

    /* images */
    hr = CoCreateInstance(&CLSID_UIRibbonImageFromBitmapFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IUIImageFromBitmap, (void **)&factory);
    check(hr == S_OK, "UIRibbonImageFromBitmapFactory");
    if (SUCCEEDED(hr))
    {
        bmp = CreateBitmap(4, 4, 1, 32, NULL);
        image = NULL;
        check(IUIImageFromBitmap_CreateImage(factory, NULL, UI_OWNERSHIP_COPY, &image) == E_INVALIDARG, "no bitmap");
        check(IUIImageFromBitmap_CreateImage(factory, bmp, 7, &image) == E_INVALIDARG, "bad ownership");
        hr = IUIImageFromBitmap_CreateImage(factory, bmp, UI_OWNERSHIP_COPY, &image);
        got = NULL;
        check(hr == S_OK && image && IUIImage_GetBitmap(image, &got) == S_OK && got && got != bmp, "COPY gives another bitmap");
        if (image) IUIImage_Release(image);
        check(GetObjectType(bmp) == OBJ_BITMAP, "... the original is still ours");
        hr = IUIImageFromBitmap_CreateImage(factory, bmp, UI_OWNERSHIP_TRANSFER, &image);
        got = NULL;
        check(hr == S_OK && image && IUIImage_GetBitmap(image, &got) == S_OK && got == bmp, "TRANSFER keeps the bitmap");
        if (image) IUIImage_Release(image);
        check(GetObjectType(bmp) == 0, "... and the image deletes it");
        IUIImageFromBitmap_Release(factory);
    }
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
