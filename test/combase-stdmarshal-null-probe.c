/* Probe for patches/sg/5120: CoGetStandardMarshal(riid, NULL, ...) (the call COM
 * makes for a marshaled packet without a proxy CLSID) returns the standard
 * marshaler instead of E_NOTIMPL, and that marshaler unmarshals a standard
 * OBJREF written by CoMarshalInterface. Table driven over contexts and flags. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG refs = 1;
static HRESULT WINAPI qi(IUnknown *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown)) { *o = i; IUnknown_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI addref(IUnknown *i) { return InterlockedIncrement(&refs); }
static ULONG WINAPI release(IUnknown *i) { return InterlockedDecrement(&refs); }
static const IUnknownVtbl vtbl = { qi, addref, release };
static IUnknown obj = { (IUnknownVtbl *)&vtbl };

int main(void)
{
    static const struct { DWORD ctx; DWORD flags; const char *name; } tests[] = {
        { MSHCTX_INPROC, MSHLFLAGS_NORMAL, "inproc, normal" },
        { MSHCTX_LOCAL, MSHLFLAGS_NORMAL, "local, normal" },
        { MSHCTX_INPROC, MSHLFLAGS_TABLESTRONG, "inproc, table strong" },
    };
    unsigned i;
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    check(hr == S_OK, "CoInitializeEx %#lx", hr);
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        IMarshal *m = NULL;
        IStream *stream;
        IUnknown *out = NULL;
        CLSID clsid;

        hr = CoGetStandardMarshal(&IID_IUnknown, NULL, tests[i].ctx, NULL, tests[i].flags, &m);
        check(hr == S_OK && m, "%s: CoGetStandardMarshal(NULL) %#lx", tests[i].name, hr);
        if (!m) continue;

        hr = IMarshal_GetUnmarshalClass(m, &IID_IUnknown, NULL, tests[i].ctx, NULL, tests[i].flags, &clsid);
        check(hr == S_OK && IsEqualCLSID(&clsid, &CLSID_StdMarshal), "%s: unmarshal class %#lx", tests[i].name, hr);

        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        hr = CoMarshalInterface(stream, &IID_IUnknown, &obj, tests[i].ctx, NULL, tests[i].flags);
        check(hr == S_OK, "%s: CoMarshalInterface %#lx", tests[i].name, hr);
        IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
        hr = IMarshal_UnmarshalInterface(m, stream, &IID_IUnknown, (void **)&out);
        check(hr == S_OK && out == &obj, "%s: unmarshal through the NULL-object marshaler %#lx, same object %d",
              tests[i].name, hr, out == &obj);
        if (out) IUnknown_Release(out);
        IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
        CoReleaseMarshalData(stream);
        IStream_Release(stream);
        CoDisconnectObject(&obj, 0);
        IMarshal_Release(m);
    }
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
