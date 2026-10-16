/* Shared by the WinRT IInspectable probes (5057-5060): checks one object's
 * GetIids / GetRuntimeClassName / GetTrustLevel against a table row.
 * IInspectable vtable: 0 QI, 1 AddRef, 2 Release, 3 GetIids, 4 GetRuntimeClassName, 5 GetTrustLevel. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#ifndef E_ILLEGAL_METHOD_CALL
#define E_ILLEGAL_METHOD_CALL ((HRESULT)0x8000000e)
#endif

#ifndef E_BOUNDS
#define E_BOUNDS ((HRESULT)0x8000000b)
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { void **lpVtbl; } sg_obj;
#define CALL(o, idx, type, ...) (((type)((sg_obj *)(o))->lpVtbl[idx])((void *)(o), ##__VA_ARGS__))
typedef HRESULT (WINAPI *sg_qi_t)(void *, REFIID, void **);
typedef ULONG (WINAPI *sg_rel_t)(void *);
typedef HRESULT (WINAPI *sg_iids_t)(void *, ULONG *, IID **);
typedef HRESULT (WINAPI *sg_str_t)(void *, HSTRING *);
typedef HRESULT (WINAPI *sg_trust_t)(void *, int *);
typedef HRESULT (WINAPI *sg_out_t)(void *, void **);

DEFINE_GUID(SG_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(SG_IID_IInspectable, 0xaf86e2e0, 0xb12d, 0x4c6a, 0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90);
DEFINE_GUID(SG_IID_IActivationFactory, 0x00000035, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(SG_IID_IAsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(SG_IID_IAgileObject, 0x94ea2b94, 0xe9cc, 0x49e0, 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90);

/* what one object must answer: the IIDs GetIids must list (NULL-terminated),
 * the class name (NULL: a factory or statics object, E_ILLEGAL_METHOD_CALL) */
struct sg_expect
{
    const char *what;
    const GUID *iids[6];
    const WCHAR *class_name;
};

static void sg_check_inspectable(void *o, const struct sg_expect *e)
{
    ULONG count = 0xdeadbeef, i, j;
    IID *iids = NULL;
    HSTRING name = NULL;
    int level = -1;
    HRESULT hr;

    hr = CALL(o, 3, sg_iids_t, &count, &iids);
    check(hr == S_OK && iids && count != 0xdeadbeef, "%s: GetIids %#lx count %lu", e->what, hr, count);
    if (hr == S_OK && iids)
    {
        for (j = 0; e->iids[j]; j++)
        {
            for (i = 0; i < count; i++) if (IsEqualGUID(&iids[i], e->iids[j])) break;
            check(i < count, "%s: GetIids lists expected IID %lu", e->what, j);
        }
        for (i = 0; i < count; i++)
        {
            void *unk = NULL;
            check(!IsEqualGUID(&iids[i], &SG_IID_IUnknown) && !IsEqualGUID(&iids[i], &SG_IID_IInspectable),
                  "%s: GetIids entry %lu is not IUnknown/IInspectable", e->what, i);
            hr = CALL(o, 0, sg_qi_t, &iids[i], &unk);
            check(hr == S_OK && unk, "%s: GetIids entry %lu answers QueryInterface: %#lx", e->what, i, hr);
            if (unk) CALL(unk, 2, sg_rel_t);
        }
        CoTaskMemFree(iids);
    }

    hr = CALL(o, 4, sg_str_t, &name);
    if (!e->class_name)
        check(hr == E_ILLEGAL_METHOD_CALL, "%s: GetRuntimeClassName of a factory/statics object %#lx", e->what, hr);
    else
    {
        UINT32 len = 0;
        const WCHAR *buf = hr == S_OK ? WindowsGetStringRawBuffer(name, &len) : NULL;
        check(hr == S_OK && buf && !lstrcmpW(buf, e->class_name) && len == lstrlenW(e->class_name),
              "%s: GetRuntimeClassName %#lx \"%ls\" length %u", e->what, hr, buf ? buf : L"", len);
        WindowsDeleteString(name);
    }

    hr = CALL(o, 5, sg_trust_t, &level);
    check(hr == S_OK && level == 0 /* BaseTrust */, "%s: GetTrustLevel %#lx level %d", e->what, hr, level);
}

static void *sg_factory(const WCHAR *class_name, const GUID *iid)
{
    HSTRING s;
    void *f = NULL;
    HRESULT hr;

    WindowsCreateString(class_name, lstrlenW(class_name), &s);
    hr = RoGetActivationFactory(s, iid, &f);
    WindowsDeleteString(s);
    check(hr == S_OK && f, "factory %ls: %#lx", class_name, hr);
    return hr == S_OK ? f : NULL;
}
