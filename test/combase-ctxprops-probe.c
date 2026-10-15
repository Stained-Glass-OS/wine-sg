/* Probe for patches/sg/5063: IContext (IObjContext) properties on the thread's object context.
 * - SetProperty stores the property with its flags and takes a reference; a NULL
 *   property is E_INVALIDARG; setting a GUID that is already there is E_FAIL;
 * - GetProperty hands back the same pointer (a new reference) and flags; an unknown GUID
 *   is E_FAIL with a NULL pointer;
 * - EnumContextProps lists the properties (Count, Next, Skip, Reset, Clone);
 * - RemoveProperty drops the reference; removing it again is E_FAIL.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#define USE_COM_CONTEXT_DEF
#include <windows.h>
#include <initguid.h>
#include <objidl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

DEFINE_GUID(IID_IObjContext_, 0x000001c6, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

struct prop { IUnknown IUnknown_iface; LONG ref; };
static HRESULT WINAPI p_qi(IUnknown *i, REFIID iid, void **out) { *out = NULL; return E_NOINTERFACE; }
static ULONG WINAPI p_addref(IUnknown *i) { return InterlockedIncrement(&((struct prop *)i)->ref); }
static ULONG WINAPI p_release(IUnknown *i) { return InterlockedDecrement(&((struct prop *)i)->ref); }
static IUnknownVtbl prop_vtbl = { p_qi, p_addref, p_release };

int main(void)
{
    static const GUID ids[] =
    {
        {0x1a1a1a1a, 1, 1, {1, 1, 1, 1, 1, 1, 1, 1}},
        {0x2b2b2b2b, 2, 2, {2, 2, 2, 2, 2, 2, 2, 2}},
        {0x3c3c3c3c, 3, 3, {3, 3, 3, 3, 3, 3, 3, 3}},
    };
    static const CPFLAGS flags[] = {0, 2, 4};
    struct prop props[3] = {{{&prop_vtbl}, 1}, {{&prop_vtbl}, 1}, {{&prop_vtbl}, 1}};
    ContextProperty got[4];
    IEnumContextProps *e = NULL, *clone = NULL;
    IContext *ctx = NULL;   /* IObjContext starts with IContext's methods */
    IUnknown *unk;
    ULONG n, count;
    CPFLAGS f;
    unsigned int i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoGetObjectContext(&IID_IObjContext_, (void **)&ctx);
    check(hr == S_OK && ctx, "CoGetObjectContext %#lx", hr);
    if (!ctx) goto done;

    for (i = 0; i < 3; i++)
    {
        hr = IContext_SetProperty(ctx, &ids[i], flags[i], &props[i].IUnknown_iface);
        check(hr == S_OK && props[i].ref == 2, "SetProperty %u %#lx ref %ld", i, hr, props[i].ref);
    }
    hr = IContext_SetProperty(ctx, &ids[0], 0, &props[1].IUnknown_iface);
    check(hr == E_FAIL && props[1].ref == 2, "SetProperty on a GUID already there %#lx", hr);
    hr = IContext_SetProperty(ctx, &IID_IUnknown, 0, NULL);
    check(hr == E_INVALIDARG, "SetProperty NULL %#lx", hr);

    for (i = 0; i < 3; i++)
    {
        unk = NULL; f = 0xdead;
        hr = IContext_GetProperty(ctx, &ids[i], &f, &unk);
        check(hr == S_OK && unk == &props[i].IUnknown_iface && f == flags[i] && props[i].ref == 3,
              "GetProperty %u %#lx flags %lu ref %ld", i, hr, f, props[i].ref);
        if (unk) IUnknown_Release(unk);
    }
    unk = (IUnknown *)0xdeadbeef;
    hr = IContext_GetProperty(ctx, &IID_IUnknown, &f, &unk);
    check(hr == E_FAIL && !unk, "GetProperty unknown GUID %#lx %p", hr, unk);

    hr = IContext_EnumContextProps(ctx, &e);
    check(hr == S_OK && e, "EnumContextProps %#lx", hr);
    if (e)
    {
        count = 0;
        hr = IEnumContextProps_Count(e, &count);
        check(hr == S_OK && count == 3, "Count %#lx %lu", hr, count);
        n = 0;
        hr = IEnumContextProps_Next(e, 4, got, &n);
        check(hr == S_FALSE && n == 3, "Next 4 %#lx fetched %lu", hr, n);
        for (i = 0; i < n && i < 3; i++)
        {
            check(IsEqualGUID(&got[i].policyId, &ids[i]) && got[i].flags == flags[i] && got[i].pUnk == &props[i].IUnknown_iface,
                  "enumerated property %u", i);
            IUnknown_Release(got[i].pUnk);
        }
        IEnumContextProps_Reset(e);
        hr = IEnumContextProps_Skip(e, 2);
        check(hr == S_OK, "Skip 2 %#lx", hr);
        hr = IEnumContextProps_Clone(e, &clone);
        check(hr == S_OK && clone, "Clone %#lx", hr);
        if (clone)
        {
            n = 0;
            hr = IEnumContextProps_Next(clone, 1, got, &n);
            check(hr == S_OK && n == 1 && IsEqualGUID(&got[0].policyId, &ids[2]), "the clone goes on from its position %#lx", hr);
            if (n) IUnknown_Release(got[0].pUnk);
            IEnumContextProps_Release(clone);
        }
        hr = IEnumContextProps_Skip(e, 2);
        check(hr == S_FALSE, "Skip past the end %#lx", hr);
        IEnumContextProps_Release(e);
    }

    hr = IContext_RemoveProperty(ctx, &ids[1]);
    check(hr == S_OK && props[1].ref == 1, "RemoveProperty %#lx ref %ld", hr, props[1].ref);
    hr = IContext_RemoveProperty(ctx, &ids[1]);
    check(hr == E_FAIL, "RemoveProperty again %#lx", hr);
    unk = NULL;
    hr = IContext_GetProperty(ctx, &ids[1], &f, &unk);
    check(hr == E_FAIL && !unk, "GetProperty after RemoveProperty %#lx", hr);
    for (i = 0; i < 3; i += 2) IContext_RemoveProperty(ctx, &ids[i]);
    check(props[0].ref == 1 && props[2].ref == 1, "every reference handed back");
    IContext_Release(ctx);
done:
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
