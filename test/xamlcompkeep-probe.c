/* Windows.UI.Xaml's compositor outlives a program's stray release (patch 6244): Window.Compositor
 * released once more than it was got (as OneDrive does) leaves it working -- the same compositor,
 * its animations still run.  Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_WindowStatics,  0x93328409, 0x4ea1, 0x4afa, 0x83, 0xdc, 0x0c, 0x4e, 0x73, 0xe8, 0x8b, 0xb1);
DEFINE_GUID(IID_Window3,        0xb70bdc9d, 0x1c35, 0x462a, 0x9b, 0x97, 0x80, 0x8d, 0x5a, 0xf9, 0xf2, 0x8e);
DEFINE_GUID(IID_Compositor,     0xb403ca50, 0x7f8c, 0x4e83, 0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8);
DEFINE_GUID(IID_CompObject,     0xbcb4ad45, 0x7609, 0x4550, 0x93, 0x4f, 0x16, 0x00, 0x2a, 0x68, 0xfd, 0xed);
DEFINE_GUID(IID_KeyFrame,       0x126e7f22, 0x3ae9, 0x4540, 0x9a, 0x8a, 0xde, 0xae, 0x8a, 0x4a, 0x4a, 0x84);
DEFINE_GUID(IID_CompAnimation,  0x464c4c2c, 0x1caa, 0x4061, 0x9b, 0x40, 0xe1, 0x3f, 0xde, 0x15, 0x03, 0xca);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) (((obj *)(o))->vtbl[n])
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
struct timespan { INT64 duration; };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) IUnknown_QueryInterface((IUnknown *)o, iid, (void **)&r); return r; }
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static obj *window_compositor(void)
{
    obj *ws = NULL, *window = NULL, *comp = NULL;
    HRESULT fhr = RoGetActivationFactory(hs(L"Windows.UI.Xaml.Window"), &IID_WindowStatics, (void **)&ws);
    if (!ws) printf("window statics %#lx\n", fhr);
    if (ws) ((get_fn)SLOT(ws, 6))(ws, &window);
    if (window) ((get_fn)SLOT(as(window, &IID_Window3), 6))(as(window, &IID_Window3), &comp);
    if (!comp) printf("statics %p window %p window3 %p\n", ws, window, window ? as(window, &IID_Window3) : NULL);
    return comp;
}

int main(void)
{
    obj *ms = NULL, *mgr = NULL, *comp, *again, *set = NULL, *anim = NULL, *kf;
    IUnknown *id1, *id2;
    struct timespan t = { 3000000 };    /* 0.3 s */
    float value = -1;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    RoGetActivationFactory(hs(L"Windows.UI.Xaml.Hosting.WindowsXamlManager"), &IID_ManagerStatics, (void **)&ms);
    if (ms) ((get_fn)SLOT(ms, 6))(ms, &mgr);    /* XAML on this thread */
    comp = window_compositor();
    printf("compositor=%d\n", comp != NULL);
    if (!comp) return 1;
    IUnknown_QueryInterface((IUnknown *)comp, &IID_IUnknown, (void **)&id1);
    IUnknown_Release(id1);
    /* the program lets go of it twice */
    IUnknown_Release((IUnknown *)comp);
    IUnknown_Release((IUnknown *)comp);

    again = window_compositor();
    IUnknown_QueryInterface((IUnknown *)again, &IID_IUnknown, (void **)&id2);
    IUnknown_Release(id2);
    printf("same=%d\n", again && id1 == id2);

    /* a property set's entry animated 0 -> 1: it runs only on a compositor that has not ended */
    ((get_fn)SLOT(again, 18))(again, &set);
    ((get_fn)SLOT(again, 20))(again, &anim);
    if (set && anim)
    {
        HSTRING name = hs(L"x");
        ((HRESULT (WINAPI *)(obj *, HSTRING, float))SLOT(set, 10))(set, name, 0.0f);
        ((HRESULT (WINAPI *)(obj *, float, float))SLOT(anim, 6))(anim, 1.0f, 1.0f);
        kf = as(anim, &IID_KeyFrame);
        ((HRESULT (WINAPI *)(obj *, struct timespan))SLOT(kf, 9))(kf, t);
        hr = ((HRESULT (WINAPI *)(obj *, HSTRING, obj *))SLOT(as(set, &IID_CompObject), 9))(as(set, &IID_CompObject), name,
                as(anim, &IID_CompAnimation));
        pump(800);
        {
            int status = -1;
            ((HRESULT (WINAPI *)(obj *, HSTRING, float *, int *))SLOT(set, 18))(set, name, &value, &status);
        }
        printf("animated=%d (start %#lx, value %.2f)\n", hr == S_OK && value > 0.99f, hr, value);
    }
    else printf("animated=0 (no set %p or animation %p)\n", set, anim);
    printf("done=1\n");
    return 0;
}
