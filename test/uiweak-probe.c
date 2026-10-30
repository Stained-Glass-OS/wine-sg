/* uiweak-probe: a thread's CoreDispatcher and a UISettings can be held weakly (patches/sg/6203).
 *
 * Programs built with C++/WinRT keep them as weak references (React Native for Windows does, in
 * OneDrive): both answered E_NOINTERFACE for IWeakReferenceSource and the program used the null.
 * The dispatcher's weak reference resolves to it (it lasts as long as its thread); a UISettings'
 * resolves while it lives and to nothing once it is gone.
 * Interface identity per windows-rs 0.30.0 (MIT).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_CoreWindowStatic, 0x4d239005, 0x3c2a, 0x41b1, 0x90, 0x22, 0x53, 0x6b, 0xb9, 0xcf, 0x93, 0xb1);
DEFINE_GUID(IID_CoreDispatcher, 0x60db2fa8, 0xb705, 0x4fde, 0xa7, 0xd6, 0xeb, 0xbb, 0x18, 0x91, 0xd3, 0x9e);
DEFINE_GUID(IID_UISettings, 0x85361600, 0x1c63, 0x4627, 0xbc, 0xb1, 0x3a, 0x89, 0xe0, 0xbc, 0x9c, 0x55);
DEFINE_GUID(IID_WeakSource, 0x00000038, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *resolve_fn)(obj *, REFIID, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static void release(obj *o) { if (o) ((rel_fn)SLOT(o, 2))(o); }
static obj *identity(obj *o) { obj *u = as(o, &IID_IUnknown); release(u); return u; }

/* its weak reference (IWeakReferenceSource.GetWeakReference), or NULL */
static obj *weak_of(obj *o)
{
    obj *source = as(o, &IID_WeakSource), *weak = NULL;
    if (!source) return NULL;
    ((get_fn)SLOT(source, 3))(source, &weak);
    release(source);
    return weak;
}

/* IWeakReference.Resolve; 0xdead when it failed */
static obj *resolve(obj *weak, REFIID iid)
{
    obj *out = (obj *)0xdead;
    if (FAILED(((resolve_fn)SLOT(weak, 3))(weak, iid, &out))) return (obj *)0xdead;
    return out;
}

int main(void)
{
    obj *ms, *mgr = NULL, *cws, *cw = NULL, *dispatcher = NULL, *weak, *back, *settings = NULL;
    HRESULT hr;

    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    if (ms) ((get_fn)SLOT(ms, 6))(ms, &mgr);
    cws = factory(L"Windows.UI.Core.CoreWindow", &IID_CoreWindowStatic);
    if (cws) ((get_fn)SLOT(cws, 6))(cws, &cw);
    if (cw) ((get_fn)SLOT(cw, 9))(cw, &dispatcher);   /* ICoreWindow.Dispatcher */
    if (!dispatcher) { printf("no dispatcher\n"); return 1; }

    /* the dispatcher */
    weak = weak_of(dispatcher);
    printf("dispatcher_weak=%d\n", weak != NULL);
    if (weak)
    {
        back = resolve(weak, &IID_CoreDispatcher);
        printf("dispatcher_resolves=%d\n", back != (obj *)0xdead && back && identity(back) == identity(dispatcher));
        if (back && back != (obj *)0xdead) release(back);
        release(weak);
    }

    /* a UISettings, while it lives and once it is gone */
    hr = RoActivateInstance(hs(L"Windows.UI.ViewManagement.UISettings"), (IInspectable **)&settings);
    printf("settings=%08lx\n", hr);
    if (!settings) return 1;
    weak = weak_of(settings);
    printf("settings_weak=%d\n", weak != NULL);
    if (!weak) return 1;
    back = resolve(weak, &IID_UISettings);
    printf("settings_resolves=%d\n", back != (obj *)0xdead && back && identity(back) == identity(settings));
    if (back && back != (obj *)0xdead) release(back);
    release(settings);
    back = resolve(weak, &IID_UISettings);
    printf("settings_gone=%d\n", back == NULL);
    release(weak);
    printf("done=1\n");
    return 0;
}
