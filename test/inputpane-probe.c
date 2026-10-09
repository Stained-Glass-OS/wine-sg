/* The input pane and view settings (patches/sg/1663), run by
 * test/inputpane-gate.sh in the shell's desktop:
 *  - UIViewSettings.UserInteractionMode: mouse, touch while tablet mode is
 *    on; the object's runtime class name and interfaces;
 *  - InputPane (IInputPaneInterop::GetForWindow, asked for IInputPane2):
 *    nothing occluded and TryHide false with no keyboard; TryShow starts
 *    the on-screen keyboard (osk.exe through App Paths -- the gate points
 *    it at this probe, which then plays the keyboard: an OSKMainClass
 *    window); Showing is raised with the keyboard's rectangle, which
 *    OccludedRect then gives; TryHide hides it and Hiding is raised;
 *    a removed handler is no longer called.
 * These were E_NOTIMPL / FIXME stubs (TryShow always false). */
#include <windows.h>
#include <stdio.h>

typedef struct HSTRING__ *HSTRING;
typedef struct { float X, Y, Width, Height; } RECTF;
typedef struct { INT64 value; } TOKEN;

static const GUID IID_IUIViewSettingsInterop = {0x3694dbf9,0x8f68,0x44be,{0x8f,0xf5,0x19,0x5c,0x98,0xed,0xe8,0xa6}};
static const GUID IID_IUIViewSettings = {0xc63657f6,0x8850,0x470d,{0x88,0xf8,0x45,0x5e,0x16,0xea,0x2c,0x26}};
static const GUID IID_IInputPaneInterop = {0x75cf2c57,0x9195,0x4931,{0x83,0x32,0xf0,0xb4,0x09,0xe9,0x16,0xaf}};
static const GUID IID_IInputPane = {0x640ada70,0x06f3,0x4c87,{0xa6,0x78,0x98,0x29,0xc9,0x12,0x7c,0x28}};
static const GUID IID_IInputPane2 = {0x8a6b3f26,0x7090,0x4793,{0x94,0x4c,0xc3,0xf2,0xcd,0xe2,0x62,0x76}};
static const GUID IID_IVisibilityArgs = {0xd243e016,0xd907,0x4fcc,{0xbb,0x8d,0xf7,0x7b,0xaa,0x50,0x28,0xf1}};
static const GUID IID_Handler = {0xb813d684,0xd953,0x5a8a,{0x9b,0x30,0x78,0xb7,0x9f,0xb9,0x14,0x7b}};
static const GUID IID_IUnknown_ = {0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IAgile = {0x94ea2b94,0xe9cc,0x49e0,{0xc0,0xff,0xee,0x64,0xca,0x8f,0x5b,0x90}};

typedef struct { void **vtbl; } obj;
#define VT(o, i) (((obj *)(o))->vtbl[i])
#define METHOD(o, i) VT(o, 6 + (i))
#define QI(o, iid, out) ((HRESULT (WINAPI *)(void *, const GUID *, void **))VT(o, 0))(o, iid, out)
#define RELEASE(o) ((ULONG (WINAPI *)(void *))VT(o, 2))(o)

static HRESULT (WINAPI *get_factory)(HSTRING, const GUID *, void **);
static HRESULT (WINAPI *make_string)(const WCHAR *, UINT32, HSTRING *);
static const WCHAR *(WINAPI *string_buffer)(HSTRING, UINT32 *);
static HRESULT (WINAPI *delete_string)(HSTRING);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void *factory(const WCHAR *name, const GUID *iid)
{
    HSTRING s;
    void *f = NULL;
    make_string(name, lstrlenW(name), &s);
    get_factory(s, iid, &f);
    delete_string(s);
    return f;
}

static int class_name_is(void *o, const WCHAR *want)
{
    HSTRING s = NULL;
    int ok;

    if (FAILED(((HRESULT (WINAPI *)(void *, HSTRING *))VT(o, 4))(o, &s))) return 0;
    ok = !lstrcmpW(string_buffer(s, NULL), want);
    delete_string(s);
    return ok;
}

static int iids_has(void *o, const GUID *want, ULONG count_want)
{
    ULONG count = 0, i;
    GUID *iids = NULL;
    int found = 0;

    if (FAILED(((HRESULT (WINAPI *)(void *, ULONG *, GUID **))VT(o, 3))(o, &count, &iids))) return 0;
    for (i = 0; i < count; i++) if (IsEqualGUID(&iids[i], want)) found = 1;
    CoTaskMemFree(iids);
    return found && count == count_want;
}

/* a TypedEventHandler<InputPane, InputPaneVisibilityEventArgs> */
struct handler
{
    void **vtbl;
    LONG ref;
    HANDLE event;
    RECTF rect;
    LONG calls;
};

static HRESULT WINAPI h_qi(struct handler *h, const GUID *iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown_) || IsEqualGUID(iid, &IID_Handler) || IsEqualGUID(iid, &IID_IAgile))
    {
        *out = h;
        InterlockedIncrement(&h->ref);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI h_addref(struct handler *h) { return InterlockedIncrement(&h->ref); }
static ULONG WINAPI h_release(struct handler *h) { return InterlockedDecrement(&h->ref); }
static HRESULT WINAPI h_invoke(struct handler *h, void *sender, void *args)
{
    void *a = NULL;
    if (args && SUCCEEDED(QI(args, &IID_IVisibilityArgs, &a)))
    {
        ((HRESULT (WINAPI *)(void *, RECTF *))METHOD(a, 0))(a, &h->rect);
        RELEASE(a);
    }
    InterlockedIncrement(&h->calls);
    SetEvent(h->event);
    return S_OK;
}
static void *handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

/* the fake on-screen keyboard */
static LRESULT CALLBACK kbd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int run_keyboard(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;

    wc.lpfnWndProc = kbd_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"OSKMainClass";
    wc.hbrBackground = GetStockObject(GRAY_BRUSH);
    RegisterClassW(&wc);
    hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"OSKMainClass", L"On-Screen Keyboard",
                           WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, 100, 450, 600, 200, NULL, NULL, wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetTimer(hwnd, 1, 120000, NULL);
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_TIMER) break;
        DispatchMessageW(&msg);
    }
    return 0;
}

static int rect_is(const RECTF *r, float x, float y, float w, float h)
{
    return r->X == x && r->Y == y && r->Width == w && r->Height == h;
}

static int wait(HANDLE event, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;

    for (;;)
    {
        DWORD now = GetTickCount(), r;
        if ((LONG)(end - now) <= 0) return 0;
        r = MsgWaitForMultipleObjects(1, &event, FALSE, end - now, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return 1;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    }
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    WCHAR self[MAX_PATH];
    void *f, *settings = NULL, *pane2 = NULL, *pane = NULL;
    struct handler show = { handler_vtbl, 1 }, hide = { handler_vtbl, 1 };
    TOKEN show_token = {0}, hide_token = {0};
    int mode = -1;
    unsigned char ok = 9;
    RECTF rect;
    HWND hwnd, kbd;
    HKEY key;
    HRESULT hr;

    GetModuleFileNameW(NULL, self, MAX_PATH);
    if (wcsstr(self, L"fakeosk")) return run_keyboard();

    get_factory = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    make_string = (void *)GetProcAddress(combase, "WindowsCreateString");
    string_buffer = (void *)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    delete_string = (void *)GetProcAddress(combase, "WindowsDeleteString");
    ((HRESULT (WINAPI *)(int))GetProcAddress(combase, "RoInitialize"))(1);

    hwnd = CreateWindowExW(0, L"static", L"input pane probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           50, 50, 400, 300, NULL, NULL, NULL, NULL);

    /* UIViewSettings */
    f = factory(L"Windows.UI.ViewManagement.UIViewSettings", &IID_IUIViewSettingsInterop);
    check(f != NULL, "the UIViewSettings interop factory");
    if (f)
    {
        hr = ((HRESULT (WINAPI *)(void *, HWND, const GUID *, void **))METHOD(f, 0))(f, hwnd, &IID_IUIViewSettings,
                                                                                    &settings);
        check(SUCCEEDED(hr) && settings, "UIViewSettings for the window");
    }
    if (settings)
    {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\ImmersiveShell",
                           L"TabletMode");
        hr = ((HRESULT (WINAPI *)(void *, int *))METHOD(settings, 0))(settings, &mode);
        check(hr == S_OK && mode == 0, "UserInteractionMode is Mouse");
        RegCreateKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\ImmersiveShell", &key);
        RegSetValueExW(key, L"TabletMode", 0, REG_DWORD, (BYTE *)&(DWORD){1}, sizeof(DWORD));
        mode = -1;
        hr = ((HRESULT (WINAPI *)(void *, int *))METHOD(settings, 0))(settings, &mode);
        check(hr == S_OK && mode == 1, "UserInteractionMode is Touch in tablet mode");
        RegDeleteValueW(key, L"TabletMode");
        RegCloseKey(key);
        check(class_name_is(settings, L"Windows.UI.ViewManagement.UIViewSettings"), "UIViewSettings' class name");
        check(iids_has(settings, &IID_IUIViewSettings, 1), "UIViewSettings' interfaces");
        RELEASE(settings);
    }

    /* InputPane */
    f = factory(L"Windows.UI.ViewManagement.InputPane", &IID_IInputPaneInterop);
    check(f != NULL, "the InputPane interop factory");
    if (!f) goto done;
    hr = ((HRESULT (WINAPI *)(void *, HWND, const GUID *, void **))METHOD(f, 0))(f, hwnd, &IID_IInputPane2, &pane2);
    check(SUCCEEDED(hr) && pane2, "InputPane for the window, as IInputPane2");
    if (!pane2) goto done;
    QI(pane2, &IID_IInputPane, &pane);
    check(pane != NULL, "and as IInputPane");
    if (!pane) goto done;
    check(class_name_is(pane, L"Windows.UI.ViewManagement.InputPane"), "InputPane's class name");
    check(iids_has(pane, &IID_IInputPane2, 2), "InputPane's interfaces");

    show.event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hide.event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hr = ((HRESULT (WINAPI *)(void *, void *, TOKEN *))METHOD(pane, 0))(pane, &show, &show_token);
    check(hr == S_OK && show_token.value, "a Showing handler is added");
    hr = ((HRESULT (WINAPI *)(void *, void *, TOKEN *))METHOD(pane, 2))(pane, &hide, &hide_token);
    check(hr == S_OK && hide_token.value && hide_token.value != show_token.value, "a Hiding handler is added");

    memset(&rect, 0xcc, sizeof(rect));
    hr = ((HRESULT (WINAPI *)(void *, RECTF *))METHOD(pane, 4))(pane, &rect);
    check(hr == S_OK && rect_is(&rect, 0, 0, 0, 0), "nothing is occluded without a keyboard");
    hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(pane2, 1))(pane2, &ok);
    check(hr == S_OK && ok == 0, "TryHide without a keyboard is false");

    ok = 9;
    hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(pane2, 0))(pane2, &ok);
    check(hr == S_OK && ok == 1, "TryShow starts the on-screen keyboard");
    check(wait(show.event, 15000), "Showing is raised");
    check(rect_is(&show.rect, 100, 450, 600, 200), "with the keyboard's rectangle");
    hr = ((HRESULT (WINAPI *)(void *, RECTF *))METHOD(pane, 4))(pane, &rect);
    check(hr == S_OK && rect_is(&rect, 100, 450, 600, 200), "OccludedRect is the keyboard");

    ok = 9;
    hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(pane2, 1))(pane2, &ok);
    check(hr == S_OK && ok == 1, "TryHide hides it");
    check(wait(hide.event, 10000), "Hiding is raised");
    check(rect_is(&hide.rect, 0, 0, 0, 0), "with nothing occluded");

    ok = 9;
    hr = ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(pane2, 0))(pane2, &ok);
    check(hr == S_OK && ok == 1 && wait(show.event, 10000), "TryShow brings it back");

    hr = ((HRESULT (WINAPI *)(void *, TOKEN))METHOD(pane, 1))(pane, show_token);
    check(hr == S_OK, "the Showing handler is removed");
    ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(pane2, 1))(pane2, &ok);
    check(wait(hide.event, 10000), "Hiding is raised again");
    ((HRESULT (WINAPI *)(void *, unsigned char *))METHOD(pane2, 0))(pane2, &ok);
    check(!wait(show.event, 2000) && show.calls == 2, "the removed handler is not called");
    ((HRESULT (WINAPI *)(void *, TOKEN))METHOD(pane, 3))(pane, hide_token);

done:
    if ((kbd = FindWindowW(L"OSKMainClass", NULL))) PostMessageW(kbd, WM_CLOSE, 0, 0);
    if (pane) RELEASE(pane);
    if (pane2) RELEASE(pane2);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
