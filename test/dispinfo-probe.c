/* dispinfo-probe: Windows.Graphics.Display.DisplayInformation (patches/sg/6204).
 *
 * The class did not exist: React Native for Windows (OneDrive) asks GetForCurrentView for the scale
 * it lays out with and stopped with an unhandled exception. Checks the view's DPI, scale, orientation
 * and size on a 1024x768 96 DPI display, one object per thread, AutoRotationPreferences.
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

DEFINE_GUID(IID_DisplayInformationStatics, 0xc6a02a6c, 0xd452, 0x44dc, 0xba, 0x07, 0x96, 0xf3, 0xc6, 0xad, 0xf9, 0xd1);
DEFINE_GUID(IID_DisplayInformation2, 0x4dcd0021, 0xfad1, 0x4b8e, 0x8e, 0xdf, 0x77, 0x58, 0x87, 0xb8, 0xbf, 0x19);
DEFINE_GUID(IID_DisplayInformation4, 0xc972ce2f, 0x1242, 0x46be, 0xb5, 0x36, 0xe1, 0xaa, 0xfe, 0x9e, 0x7a, 0xcf);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static void release(obj *o) { if (o) ((rel_fn)SLOT(o, 2))(o); }

static obj *statics, *other_view;

static DWORD WINAPI other_thread(void *arg)
{
    ((get_fn)SLOT(statics, 6))(statics, &other_view);
    return 0;
}

int main(void)
{
    obj *view = NULL, *again = NULL, *v2, *v4;
    INT32 orientation = -1, native = -1, scale = -1, rotation = -1;
    FLOAT dpi = 0, rawx = 0;
    DOUBLE per_view = 0;
    UINT32 width = 0, height = 0;
    HANDLE thread;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(hs(L"Windows.Graphics.Display.DisplayInformation"), &IID_DisplayInformationStatics,
                                (void **)&statics);
    printf("statics=%08lx\n", hr);
    if (!statics) return 1;
    hr = ((get_fn)SLOT(statics, 6))(statics, &view);   /* GetForCurrentView */
    printf("view=%08lx\n", hr);
    if (!view) return 1;

    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(view, 6))(view, &orientation);
    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(view, 7))(view, &native);
    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(view, 10))(view, &scale);
    ((HRESULT (WINAPI *)(obj *, FLOAT *))SLOT(view, 11))(view, &dpi);
    ((HRESULT (WINAPI *)(obj *, FLOAT *))SLOT(view, 12))(view, &rawx);
    printf("orientation=%d native=%d\n", orientation, native);
    printf("scale=%d dpi=%d raw=%d\n", scale, (int)(dpi + 0.5f), rawx > 0);
    if ((v2 = as(view, &IID_DisplayInformation2)))
    {
        ((HRESULT (WINAPI *)(obj *, DOUBLE *))SLOT(v2, 6))(v2, &per_view);
        release(v2);
    }
    printf("per_view=%d\n", (int)(per_view * 100 + 0.5));
    if ((v4 = as(view, &IID_DisplayInformation4)))
    {
        ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(v4, 6))(v4, &width);
        ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(v4, 7))(v4, &height);
        release(v4);
    }
    printf("size=%ux%u\n", width, height);

    /* the same view's object again; another thread's is its own */
    ((get_fn)SLOT(statics, 6))(statics, &again);
    thread = CreateThread(NULL, 0, other_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, 5000);
    printf("same=%d other=%d\n", again == view, other_view && other_view != view);

    /* AutoRotationPreferences: Portrait (2) kept */
    ((HRESULT (WINAPI *)(obj *, INT32))SLOT(statics, 8))(statics, 2);
    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(statics, 7))(statics, &rotation);
    printf("rotation=%d\n", rotation);
    printf("done=1\n");
    return 0;
}
