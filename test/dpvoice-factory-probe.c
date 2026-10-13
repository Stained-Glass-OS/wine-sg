/* Probe for patches/sg/4581: dpvoice's class factories answer
 * QueryInterface for IUnknown and IClassFactory (and clear the pointer for
 * anything else), LockServer succeeds, and DirectPlayVoiceCreate takes three
 * arguments (32-bit: the stack is balanced). Table driven; lines "ok"/"FAIL",
 * last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

#pragma GCC optimize("no-omit-frame-pointer")

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const GUID clsids[] =
{
    {0xb9f3eb85,0xb781,0x4ac1,{0x8d,0x90,0x93,0xa0,0x5e,0xe3,0x7d,0x7d}}, /* client */
    {0xd3f5b8e6,0x9b78,0x4a4c,{0x94,0xea,0xca,0x23,0x97,0xb6,0x63,0xd3}}, /* server */
    {0x0f0f094b,0xb01c,0x4091,{0xa1,0x4d,0xdd,0x0c,0xd8,0x07,0x71,0x1a}}, /* test */
};
static const GUID iid_client = {0x1dfdc8ea,0xbcf7,0x41d6,{0xb2,0x95,0xab,0x64,0xb3,0xb2,0x33,0x06}};

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA("dpvoice.dll");
    HRESULT (WINAPI *p_get)(REFCLSID, REFIID, void **);
    HRESULT (WINAPI *p_create)(const GUID *, void **, void *);
    unsigned int i;
    HRESULT hr;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_get = (void *)GetProcAddress(mod, "DllGetClassObject");
    p_create = (void *)GetProcAddress(mod, "DirectPlayVoiceCreate");
    check(p_get && p_create, "exports");
    if (!p_get || !p_create) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(clsids); i++)
    {
        IClassFactory *cf;
        IUnknown *unk;
        void *out;

        hr = p_get(&clsids[i], &IID_IClassFactory, (void **)&cf);
        check(hr == S_OK, "class %u: DllGetClassObject %#lx", i, hr);
        if (FAILED(hr)) continue;
        hr = IClassFactory_QueryInterface(cf, &IID_IUnknown, (void **)&unk);
        check(hr == S_OK && unk == (IUnknown *)cf, "class %u: QueryInterface(IUnknown) %#lx", i, hr);
        if (hr == S_OK) IUnknown_Release(unk);
        out = NULL;
        hr = IClassFactory_QueryInterface(cf, &IID_IClassFactory, &out);
        check(hr == S_OK && out == cf, "class %u: QueryInterface(IClassFactory) %#lx", i, hr);
        if (hr == S_OK) IClassFactory_Release(cf);
        out = (void *)0xdead;
        hr = IClassFactory_QueryInterface(cf, &iid_client, &out);
        check(hr == E_NOINTERFACE && !out, "class %u: QueryInterface(other) %#lx %p", i, hr, out);
        check(IClassFactory_LockServer(cf, TRUE) == S_OK && IClassFactory_LockServer(cf, FALSE) == S_OK, "class %u: LockServer", i);
        IClassFactory_Release(cf);
    }

    {
        void *out = NULL;
#ifdef __i386__
        DWORD before, after;
        __asm__ __volatile__("movl %%esp, %0" : "=r"(before));
        hr = p_create(&iid_client, &out, NULL);
        __asm__ __volatile__("movl %%esp, %0" : "=r"(after));
        check(before == after, "DirectPlayVoiceCreate leaves the stack balanced (%#lx %#lx)", before, after);
#else
        hr = p_create(&iid_client, &out, NULL);
#endif
        check(FAILED(hr) || out, "DirectPlayVoiceCreate %#lx", hr);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
