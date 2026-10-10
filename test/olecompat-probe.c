/* ole32/combase edge cases that Wine's conformance test compobj.c records from
 * Windows (patches/sg/2220): OleSetMenuDescriptor(NULL, ...) with nothing
 * installed succeeds; CoUnmarshalInterface without an apartment is
 * CO_E_NOTINITIALIZED; GetClassFile of a missing file is MK_E_CANTOPENFILE;
 * CoGetInstanceFromFile refuses a MULTI_QI that already holds an interface. */
#include <windows.h>
#include <objbase.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void hrck(HRESULT got, HRESULT want, const char *what)
{
    char b[200];
    snprintf(b, sizeof(b), "%s (hr %08lx, want %08lx)", what, (unsigned long)got, (unsigned long)want);
    check(got == want, b);
}

int main(void)
{
    HWND frame;
    HRESULT hr;
    IStream *stream;
    void *proxy = NULL;
    CLSID clsid;
    MULTI_QI mqi[1];
    HANDLE f;
    static const CLSID unregistered = { 0x7e57c0de, 0x1234, 0x4321, { 1, 2, 3, 4, 5, 6, 7, 8 } };

    /* --- OleSetMenuDescriptor --- */
    frame = CreateWindowA("STATIC", "frame", 0, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    check(frame != NULL, "a frame window");
    hrck(OleSetMenuDescriptor(NULL, frame, NULL, NULL, NULL), S_OK, "removing a descriptor that was never set is S_OK");
    hrck(OleSetMenuDescriptor(NULL, NULL, NULL, NULL, NULL), E_INVALIDARG, "a NULL frame window is E_INVALIDARG");
    DestroyWindow(frame);

    /* --- CoUnmarshalInterface without an apartment --- */
    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    check(hr == S_OK, "a stream");
    hrck(CoUnmarshalInterface(stream, &IID_IUnknown, &proxy), CO_E_NOTINITIALIZED, "CoUnmarshalInterface without an apartment");
    hrck(CoUnmarshalInterface(NULL, &IID_IUnknown, &proxy), E_INVALIDARG, "...a NULL stream is still E_INVALIDARG");
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hrck(CoUnmarshalInterface(stream, &IID_IUnknown, &proxy), STG_E_READFAULT, "with an apartment an empty stream is STG_E_READFAULT");

    /* --- GetClassFile --- */
    hrck(GetClassFile(L"sg_missing_file_without_extension", &clsid), MK_E_CANTOPENFILE, "GetClassFile of a missing file");
    hrck(GetClassFile(L"C:\\sg_no_such_dir\\file.txt", &clsid), MK_E_CANTOPENFILE, "...also with an extension and a missing directory");
    f = CreateFileA("sgolecompat.sgprobe", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(f);
    hrck(GetClassFile(L"sgolecompat.sgprobe", &clsid), MK_E_INVALIDEXTENSION, "an existing file with an extension nobody registered is MK_E_INVALIDEXTENSION");
    DeleteFileA("sgolecompat.sgprobe");

    /* --- CoGetInstanceFromFile --- */
    mqi[0].pIID = &IID_IUnknown;
    mqi[0].pItf = (IUnknown *)(ULONG_PTR)0xdeadbeef;
    mqi[0].hr = S_OK;
    hrck(CoGetInstanceFromFile(NULL, (CLSID *)&unregistered, NULL, CLSCTX_INPROC_SERVER, STGM_READ, L"dummypath", 1, mqi), E_INVALIDARG,
         "an interface pointer already in the MULTI_QI is E_INVALIDARG");
    check(mqi[0].pItf == (IUnknown *)(ULONG_PTR)0xdeadbeef && mqi[0].hr == S_OK, "...and the entry is left alone");
    mqi[0].pItf = NULL;
    mqi[0].hr = E_NOTIMPL;
    hrck(CoGetInstanceFromFile(NULL, (CLSID *)&unregistered, NULL, CLSCTX_INPROC_SERVER, STGM_READ, L"dummypath", 1, mqi), REGDB_E_CLASSNOTREG,
         "with a NULL interface pointer an unregistered class is REGDB_E_CLASSNOTREG");
    hrck(mqi[0].hr, REGDB_E_CLASSNOTREG, "...and the entry holds it");
    mqi[0].pItf = NULL;
    mqi[0].hr = E_NOTIMPL;
    hr = CoGetInstanceFromFile(NULL, NULL, NULL, CLSCTX_INPROC_SERVER, STGM_READ, L"dummypath", 1, mqi);
    hrck(hr, MK_E_CANTOPENFILE, "no class and a missing file: MK_E_CANTOPENFILE");
    hrck(mqi[0].hr, E_NOINTERFACE, "...the entry is E_NOINTERFACE");

    if (stream) stream->lpVtbl->Release(stream);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
