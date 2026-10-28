/* Windows.UI and Windows.UI.Composition for WinUI 2 controls (patches/sg/6182): the thread's CoreWindow
 * gives weak references (C++/WinRT's make_weak, MUX's AnimatedVisualPlayer), and the compositor is an
 * ICompositor5: its Comment and GlobalPlaybackRate kept, RequestCommitAsync done, shapes not made
 * (E_NOTIMPL, so Lottie animations fall back). Interface identity per windows-rs (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_CoreWindowStatic, 0x4d239005, 0x3c2a, 0x41b1, 0x90, 0x22, 0x53, 0x6b, 0xb9, 0xcf, 0x93, 0xb1);
DEFINE_GUID(IID_CoreWindow, 0x79b9d5f2, 0x879e, 0x4b89, 0xb7, 0x98, 0x79, 0xe4, 0x75, 0x98, 0x03, 0x0c);
DEFINE_GUID(IID_WeakSource, 0x00000038, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_Compositor5, 0x48ea31ad, 0x7fcd, 0x4076, 0xa7, 0x9c, 0x90, 0xcc, 0x4b, 0x85, 0x2c, 0x9b);
DEFINE_GUID(IID_AsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }

int main(void)
{
    obj *ms, *mgr = NULL, *cws, *cw = NULL, *source, *weak = NULL, *resolved = NULL, *comp = NULL, *c5, *action = NULL, *info, *shape = NULL;
    HSTRING comment = NULL;
    FLOAT rate = 0, rate2 = 0;
    HRESULT hr, hr_shape = S_OK;
    int status = -1;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    if (ms) ((get_fn)SLOT(ms, 6))(ms, &mgr);
    cws = factory(L"Windows.UI.Core.CoreWindow", &IID_CoreWindowStatic);
    if (cws) ((get_fn)SLOT(cws, 6))(cws, &cw);

    /* the CoreWindow's weak reference resolves to it */
    source = as(cw, &IID_WeakSource);
    if (source) ((get_fn)SLOT(source, 3))(source, &weak);
    if (weak) ((HRESULT (WINAPI *)(obj *, REFIID, obj **))SLOT(weak, 3))(weak, &IID_CoreWindow, &resolved);
    printf("weak=%d,%d,%d\n", cw != NULL, weak != NULL, resolved && resolved == as(cw, &IID_CoreWindow));

    /* ICompositor5 */
    hr = RoActivateInstance(hs(L"Windows.UI.Composition.Compositor"), (IInspectable **)&comp);
    c5 = as(comp, &IID_Compositor5);
    printf("compositor5=%08lx,%d\n", hr, c5 != NULL);
    if (!c5) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING))SLOT(c5, 7))(c5, hs(L"sg note"));
    ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(c5, 6))(c5, &comment);
    ((HRESULT (WINAPI *)(obj *, FLOAT *))SLOT(c5, 8))(c5, &rate);
    ((HRESULT (WINAPI *)(obj *, FLOAT))SLOT(c5, 9))(c5, 2.5f);
    ((HRESULT (WINAPI *)(obj *, FLOAT *))SLOT(c5, 8))(c5, &rate2);
    printf("kept=%ls,%.1f,%.1f\n", comment ? WindowsGetStringRawBuffer(comment, NULL) : L"-", rate, rate2);
    hr = ((HRESULT (WINAPI *)(obj *, obj **))SLOT(c5, 25))(c5, &action);   /* RequestCommitAsync */
    info = as(action, &IID_AsyncInfo);
    if (info) ((HRESULT (WINAPI *)(obj *, int *))SLOT(info, 7))(info, &status);
    printf("commit=%08lx,%d\n", hr, status);
    hr_shape = ((HRESULT (WINAPI *)(obj *, obj **))SLOT(c5, 21))(c5, &shape);   /* CreateShapeVisual */
    printf("shapes=%08lx,%d\n", hr_shape, shape == NULL);
    return 0;
}
