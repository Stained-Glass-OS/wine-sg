/* Windows.Foundation.Collections.ValueSet and PropertySet made (patches/sg/6176: wintypes, where they are
 * registered, hands them to windows.applicationmodel's property set), and
 * Windows.ApplicationModel.Resources.Core.ResourceContext as a class with its statics -- Windows
 * Terminal passes its connections' settings in ValueSets and asks for a view-independent context.
 * Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_MapStringObject, 0x1b0d3570, 0x0877, 0x5ec2, 0x8a, 0x2c, 0x3b, 0x95, 0x39, 0x50, 0x6a, 0xca);
DEFINE_GUID(IID_PVStatics, 0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_ResourceContext, 0x2fa22f4b, 0x707e, 0x4b27, 0xad, 0x0d, 0xd0, 0xd8, 0xcd, 0x46, 0x8f, 0xd2);
DEFINE_GUID(IID_ResourceContextStatics2, 0x41f752ef, 0x12af, 0x41b9, 0xab, 0x36, 0xb1, 0xeb, 0x4b, 0x51, 0x24, 0x60);
DEFINE_GUID(IID_ResourceContextStatics3, 0x20cf492c, 0xaf0f, 0x450b, 0x9d, 0xa6, 0x10, 0x6d, 0xd0, 0xc2, 0x9a, 0x39);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }

static void set_test(const WCHAR *class)
{
    obj *set = NULL, *map, *pvs = NULL, *box = NULL, *found = NULL;
    HRESULT hr = RoActivateInstance(hs(class), (IInspectable **)&set);
    boolean replaced = TRUE, has = FALSE;
    UINT32 size = 0;
    HRESULT lookup = E_FAIL, missing = S_OK;

    map = as(set, &IID_MapStringObject);
    RoGetActivationFactory(hs(L"Windows.Foundation.PropertyValue"), &IID_PVStatics, (void **)&pvs);
    if (pvs) ((HRESULT (WINAPI *)(obj *, INT32, obj **))SLOT(pvs, 10))(pvs, 42, &box);   /* CreateInt32 */
    if (map)
    {
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj *, boolean *))SLOT(map, 10))(map, hs(L"initialRows"), box, &replaced);
        ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(map, 7))(map, &size);
        ((HRESULT (WINAPI *)(obj *, HSTRING, boolean *))SLOT(map, 8))(map, hs(L"initialRows"), &has);
        lookup = ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(map, 6))(map, hs(L"initialRows"), &found);
        missing = ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(map, 6))(map, hs(L"nothing"), &found);
    }
    printf("%ls=%08lx,%d,%d,%u,%d,%08lx,%08lx\n", class, hr, map != NULL, replaced, size, has, lookup, missing);
}

int main(void)
{
    obj *ctx = NULL, *rc, *s2 = NULL, *s3 = NULL, *view = NULL, *clone = NULL;
    HRESULT hr, hr2 = E_FAIL, hr3 = E_FAIL;

    RoInitialize(RO_INIT_SINGLETHREADED);
    set_test(L"Windows.Foundation.Collections.ValueSet");
    set_test(L"Windows.Foundation.Collections.PropertySet");

    hr = RoActivateInstance(hs(L"Windows.ApplicationModel.Resources.Core.ResourceContext"), (IInspectable **)&ctx);
    rc = as(ctx, &IID_ResourceContext);
    if (rc) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(rc, 10))(rc, &clone);   /* Clone */
    RoGetActivationFactory(hs(L"Windows.ApplicationModel.Resources.Core.ResourceContext"), &IID_ResourceContextStatics2, (void **)&s2);
    if (s2)
    {
        ((HRESULT (WINAPI *)(obj *, obj **))SLOT(s2, 10))(s2, &view);   /* GetForViewIndependentUse */
        hr2 = ((HRESULT (WINAPI *)(obj *, HSTRING, HSTRING))SLOT(s2, 7))(s2, hs(L"Language"), hs(L"en-US"));
    }
    RoGetActivationFactory(hs(L"Windows.ApplicationModel.Resources.Core.ResourceContext"), &IID_ResourceContextStatics3, (void **)&s3);
    if (s3) hr3 = ((HRESULT (WINAPI *)(obj *, HSTRING, HSTRING, INT32))SLOT(s3, 6))(s3, hs(L"Contrast"), hs(L"standard"), 0);
    printf("context=%08lx,%d,%d,%d,%08lx,%08lx\n", hr, rc != NULL, clone != NULL, view != NULL && as(view, &IID_ResourceContext) != NULL,
           hr2, hr3);
    return 0;
}
