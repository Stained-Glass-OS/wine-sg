/* dmscript objects (patches/sg/2805, 2806), run by test/dmscript-gate.sh:
 * the script object (IPersistStream::Load of a DMSC form, Init, CallRoutine, Set/GetVariable*,
 * EnumRoutine / EnumVariable, error information) with VBScript and JScript sources, and the script
 * track (IPersistStream::Load through a segment file whose events reference a script file, InitPlay /
 * Play / PlayEx / Clone / EndPlay driving the routines of that script).
 *
 *   dmscript-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dmusici.h>
#include <dmusicf.h>
#include <dmerror.h>
#include <oleauto.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define EXPECT_HR(expr, want) do { HRESULT _hr = (expr); CHECKF(_hr == (HRESULT)(want), "%s = %#lx, want %#lx", #expr, _hr, (unsigned long)(HRESULT)(want)); } while (0)

/* the header declares the IDirectMusicScript call macros under the wrong interface name */
#define SC_Init(p,a,b)              (p)->lpVtbl->Init(p,a,b)
#define SC_CallRoutine(p,a,b)       (p)->lpVtbl->CallRoutine(p,a,b)
#define SC_SetVariableVariant(p,a,b,c,d) (p)->lpVtbl->SetVariableVariant(p,a,b,c,d)
#define SC_GetVariableVariant(p,a,b,c)   (p)->lpVtbl->GetVariableVariant(p,a,b,c)
#define SC_SetVariableNumber(p,a,b,c)    (p)->lpVtbl->SetVariableNumber(p,a,b,c)
#define SC_GetVariableNumber(p,a,b,c)    (p)->lpVtbl->GetVariableNumber(p,a,b,c)
#define SC_SetVariableObject(p,a,b,c)    (p)->lpVtbl->SetVariableObject(p,a,b,c)
#define SC_GetVariableObject(p,a,b,c,d)  (p)->lpVtbl->GetVariableObject(p,a,b,c,d)
#define SC_EnumRoutine(p,a,b)            (p)->lpVtbl->EnumRoutine(p,a,b)
#define SC_EnumVariable(p,a,b)           (p)->lpVtbl->EnumVariable(p,a,b)

/* ---- RIFF building ---- */
static char riff[8192];
static int riff_len;
static void put(const void *data, int size) { memcpy(riff + riff_len, data, size); riff_len += size; }
static int begin(const char *id, const char *type)
{
    DWORD zero = 0;
    int at;
    put(id, 4);
    at = riff_len;
    put(&zero, 4);
    if (type) put(type, 4);
    return at;
}
static void end(int at)
{
    DWORD size = riff_len - at - 4;
    memcpy(riff + at, &size, 4);
    if (riff_len & 1) riff[riff_len++] = 0;
}
static void chunk(const char *id, const void *data, int size)
{
    int at = begin(id, NULL);
    put(data, size);
    end(at);
}
static void wchunk(const char *id, const WCHAR *str)
{
    chunk(id, str, (lstrlenW(str) + 1) * sizeof(WCHAR));
}
static IStream *make_stream(void)
{
    static const LARGE_INTEGER zero;
    IStream *stream;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IStream_Write(stream, riff, riff_len, NULL);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    riff_len = 0;
    return stream;
}
DEFINE_GUID(GUID_Script1, 0x11223344, 0x5566, 0x7788, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01);

/* a DMSC form; 'name' is the UNFO name, NULL for none */
static void build_script(const WCHAR *language, const WCHAR *source, const WCHAR *name, BOOL header)
{
    DMUS_IO_VERSION version = {0x00010002, 0x00030004};
    DWORD flags = 0;
    int at = begin("RIFF", "DMSC"), un;

    if (header) chunk("schd", &flags, sizeof(flags));
    chunk("guid", &GUID_Script1, sizeof(GUID));
    chunk("vers", &version, sizeof(version));
    if (name)
    {
        un = begin("LIST", "UNFO");
        wchunk("UNAM", name);
        end(un);
    }
    if (language) wchunk("scla", language);
    if (source) wchunk("scsr", source);
    end(at);
}

static IDirectMusicScript *load_script(const WCHAR *language, const WCHAR *source, HRESULT *load_hr)
{
    IDirectMusicScript *script = NULL;
    IPersistStream *ps;
    IStream *stream;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicScript, (void **)&script);
    if (hr != S_OK) { check(0, "create script"); return NULL; }
    build_script(language, source, L"probe script", TRUE);
    stream = make_stream();
    IDirectMusicScript_QueryInterface(script, &IID_IPersistStream, (void **)&ps);
    *load_hr = IPersistStream_Load(ps, stream);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return script;
}

static const WCHAR vb_source[] =
    L"Dim counter\r\n"
    L"Dim label, other\r\n"
    L"Dim obj\r\n"
    L"counter = 5\r\n"
    L"label = \"hi\"\r\n"
    L"Sub Bump\r\n"
    L"  counter = counter + 1\r\n"
    L"End Sub\r\n"
    L"Function Twice\r\n"
    L"  Dim scratch\r\n"
    L"  counter = counter * 2\r\n"
    L"End Function\r\n"
    L"Sub Boom\r\n"
    L"  Dim x\r\n"
    L"  x = 1 / 0\r\n"
    L"End Sub\r\n";

static const WCHAR js_source[] =
    L"var total = 10, name = 'js';\r\n"
    L"var spare;\r\n"
    L"function Add() { total = total + 3; }\r\n"
    L"function Fail() { throw new Error('boom'); }\r\n"
    L"function Other() { var inner = 1; }\r\n";

static LONG get_number(IDirectMusicScript *script, const WCHAR *name, HRESULT *hr)
{
    LONG v = -999;
    *hr = SC_GetVariableNumber(script, (WCHAR *)name, &v, NULL);
    return v;
}

/* a minimal automation object */
static HRESULT WINAPI disp_QueryInterface(IDispatch *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI disp_AddRef(IDispatch *iface) { return 2; }
static ULONG WINAPI disp_Release(IDispatch *iface) { return 1; }
static HRESULT WINAPI disp_GetTypeInfoCount(IDispatch *iface, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI disp_GetTypeInfo(IDispatch *iface, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI disp_GetIDsOfNames(IDispatch *iface, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *id) { return DISP_E_UNKNOWNNAME; }
static HRESULT WINAPI disp_Invoke(IDispatch *iface, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a) { return E_NOTIMPL; }
static const IDispatchVtbl disp_vtbl = {disp_QueryInterface, disp_AddRef, disp_Release, disp_GetTypeInfoCount,
        disp_GetTypeInfo, disp_GetIDsOfNames, disp_Invoke};
static IDispatch disp_obj = {(IDispatchVtbl *)&disp_vtbl};

static void test_script_object(void)
{
    IDirectMusicPerformance *perf;
    IDirectMusicScript *script;
    IDirectMusicObject *dmo;
    DMUS_SCRIPT_ERRORINFO err;
    DMUS_OBJECTDESC desc;
    WCHAR name[MAX_PATH];
    HRESULT hr, load;
    VARIANT var;
    IUnknown *unk;
    LONG n;
    int i;

    CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance, (void **)&perf);
    IDirectMusicPerformance_Init(perf, NULL, NULL, NULL);

    {
        IDirectMusicObject *fresh;
        CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicObject, (void **)&fresh);
        memset(&desc, 0, sizeof(desc));
        IDirectMusicObject_GetDescriptor(fresh, &desc);
        CHECKF(desc.dwValidData == (DMUS_OBJ_CLASS | DMUS_OBJ_VERSION), "fresh script descriptor valid data %#lx", desc.dwValidData);
        IDirectMusicObject_Release(fresh);
    }

    /* load failures */
    {
        IPersistStream *ps;
        IStream *stream;
        CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicScript, (void **)&script);
        IDirectMusicScript_QueryInterface(script, &IID_IPersistStream, (void **)&ps);
        EXPECT_HR(IPersistStream_Load(ps, NULL), E_POINTER);
        begin("RIFF", "DMSG");
        stream = make_stream();
        EXPECT_HR(IPersistStream_Load(ps, stream), DMUS_E_SCRIPT_INVALID_FILE);
        IStream_Release(stream);
        build_script(L"AudioVBScript", L"x = 1", NULL, FALSE);
        stream = make_stream();
        EXPECT_HR(IPersistStream_Load(ps, stream), DMUS_E_SCRIPT_INVALID_FILE);
        IStream_Release(stream);
        build_script(NULL, L"x = 1", NULL, TRUE);
        stream = make_stream();
        EXPECT_HR(IPersistStream_Load(ps, stream), DMUS_E_SCRIPT_INVALID_FILE);
        IStream_Release(stream);
        build_script(L"NoSuchLanguage", L"x = 1", NULL, TRUE);
        stream = make_stream();
        EXPECT_HR(IPersistStream_Load(ps, stream), DMUS_E_SCRIPT_LANGUAGE_INCOMPATIBLE);
        IStream_Release(stream);
        IPersistStream_Release(ps);
        IDirectMusicScript_Release(script);
    }

    /* the VBScript flavour */
    script = load_script(L"AudioVBScript", vb_source, &load);
    EXPECT_HR(load, S_OK);
    memset(&desc, 0, sizeof(desc));
    IDirectMusicScript_QueryInterface(script, &IID_IDirectMusicObject, (void **)&dmo);
    desc.dwSize = sizeof(desc);
    EXPECT_HR(IDirectMusicObject_GetDescriptor(dmo, &desc), S_OK);
    CHECKF(desc.dwValidData == (DMUS_OBJ_CLASS | DMUS_OBJ_OBJECT | DMUS_OBJ_NAME | DMUS_OBJ_VERSION),
            "descriptor valid data %#lx", desc.dwValidData);
    CHECKF(IsEqualGUID(&desc.guidObject, &GUID_Script1), "descriptor object guid");
    CHECKF(!lstrcmpW(desc.wszName, L"probe script"), "descriptor name %ls", desc.wszName);
    CHECKF(desc.vVersion.dwVersionMS == 0x00010002 && desc.vVersion.dwVersionLS == 0x00030004,
            "descriptor version %#lx.%#lx", desc.vVersion.dwVersionMS, desc.vVersion.dwVersionLS);
    IDirectMusicObject_Release(dmo);

    /* the script is not running before Init */
    EXPECT_HR(SC_CallRoutine(script, (WCHAR *)L"Bump", NULL), DMUS_E_NOT_INIT);
    EXPECT_HR(SC_GetVariableNumber(script, (WCHAR *)L"counter", &n, NULL), DMUS_E_NOT_INIT);
    EXPECT_HR(SC_Init(script, NULL, NULL), E_POINTER);
    EXPECT_HR(SC_Init(script, perf, NULL), S_OK);
    EXPECT_HR(SC_Init(script, perf, NULL), S_OK);

    n = get_number(script, L"counter", &hr);
    CHECKF(hr == S_OK && n == 5, "counter after Init: %#lx %ld (global code ran)", hr, n);
    EXPECT_HR(SC_CallRoutine(script, (WCHAR *)L"Bump", NULL), S_OK);
    n = get_number(script, L"counter", &hr);
    CHECKF(hr == S_OK && n == 6, "counter after Bump: %ld", n);
    EXPECT_HR(SC_SetVariableNumber(script, (WCHAR *)L"counter", 41, NULL), S_OK);
    EXPECT_HR(SC_CallRoutine(script, (WCHAR *)L"Twice", NULL), S_OK);
    n = get_number(script, L"counter", &hr);
    CHECKF(hr == S_OK && n == 82, "counter after Twice: %ld", n);
    EXPECT_HR(SC_CallRoutine(script, (WCHAR *)L"NoSuchRoutine", NULL), DMUS_E_SCRIPT_ROUTINE_NOT_FOUND);
    EXPECT_HR(SC_CallRoutine(script, NULL, NULL), E_POINTER);
    EXPECT_HR(SC_GetVariableNumber(script, (WCHAR *)L"nothing", &n, NULL), DMUS_E_SCRIPT_VARIABLE_NOT_FOUND);
    EXPECT_HR(SC_SetVariableNumber(script, (WCHAR *)L"nothing", 1, NULL), DMUS_E_SCRIPT_VARIABLE_NOT_FOUND);
    EXPECT_HR(SC_GetVariableNumber(script, (WCHAR *)L"counter", NULL, NULL), E_POINTER);

    /* an error in a routine reports the failure and fills the error information */
    memset(&err, 0xcc, sizeof(err));
    err.dwSize = sizeof(err);
    EXPECT_HR(SC_CallRoutine(script, (WCHAR *)L"Boom", &err), DMUS_E_SCRIPT_ERROR_IN_SCRIPT);
    CHECKF(FAILED(err.hr) && err.hr != (HRESULT)0xcccccccc, "error info hr %#lx", err.hr);
    CHECKF(err.dwSize == sizeof(err), "error info dwSize %lu", err.dwSize);

    /* variants */
    VariantInit(&var);
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString(L"sung");
    EXPECT_HR(SC_SetVariableVariant(script, (WCHAR *)L"label", var, FALSE, NULL), S_OK);
    VariantClear(&var);
    EXPECT_HR(SC_GetVariableVariant(script, (WCHAR *)L"label", &var, NULL), S_OK);
    CHECKF(V_VT(&var) == VT_BSTR && !lstrcmpW(V_BSTR(&var), L"sung"), "string variable: vt %d", V_VT(&var));
    VariantClear(&var);
    V_VT(&var) = VT_R8;
    V_R8(&var) = 2.5;
    EXPECT_HR(SC_SetVariableVariant(script, (WCHAR *)L"other", var, FALSE, NULL), S_OK);
    EXPECT_HR(SC_GetVariableVariant(script, (WCHAR *)L"other", &var, NULL), S_OK);
    CHECKF(V_VT(&var) == VT_R8 && V_R8(&var) == 2.5, "double variable: vt %d", V_VT(&var));
    V_VT(&var) = VT_I4;
    V_I4(&var) = 9;
    EXPECT_HR(SC_SetVariableVariant(script, (WCHAR *)L"other", var, TRUE, NULL), DMUS_E_SCRIPT_NOT_A_REFERENCE);
    V_VT(&var) = VT_ARRAY | VT_I4;
    V_ARRAY(&var) = NULL;
    EXPECT_HR(SC_SetVariableVariant(script, (WCHAR *)L"other", var, FALSE, NULL), DMUS_E_SCRIPT_UNSUPPORTED_VARTYPE);
    EXPECT_HR(SC_GetVariableVariant(script, (WCHAR *)L"nothing", &var, NULL), DMUS_E_SCRIPT_VARIABLE_NOT_FOUND);
    /* a string is no number */
    EXPECT_HR(SC_GetVariableNumber(script, (WCHAR *)L"label", &n, NULL), DMUS_E_SCRIPT_UNSUPPORTED_VARTYPE);
    /* an uninitialised variable is empty, so zero */
    n = get_number(script, L"obj", &hr);
    CHECKF(hr == S_OK && n == 0, "empty variable as number: %#lx %ld", hr, n);

    /* objects */
    EXPECT_HR(SC_GetVariableObject(script, (WCHAR *)L"counter", &IID_IUnknown, (void **)&unk, NULL), DMUS_E_SCRIPT_NOT_A_REFERENCE);
    EXPECT_HR(SC_SetVariableObject(script, (WCHAR *)L"obj", (IUnknown *)&disp_obj, NULL), S_OK);
    unk = NULL;
    hr = SC_GetVariableObject(script, (WCHAR *)L"obj", &IID_IDispatch, (void **)&unk, NULL);
    CHECKF(hr == S_OK && unk == (IUnknown *)&disp_obj, "object variable: %#lx %p vs %p", hr, unk, &disp_obj);
    if (unk) IUnknown_Release(unk);
    EXPECT_HR(SC_GetVariableObject(script, (WCHAR *)L"obj", &IID_IStream, (void **)&unk, NULL), E_NOINTERFACE);
    EXPECT_HR(SC_GetVariableObject(script, (WCHAR *)L"nothing", &IID_IUnknown, (void **)&unk, NULL), DMUS_E_SCRIPT_VARIABLE_NOT_FOUND);

    /* enumeration: declaration order, only the top level */
    {
        static const WCHAR *routines[] = {L"Bump", L"Twice", L"Boom"};
        static const WCHAR *variables[] = {L"counter", L"label", L"other", L"obj"};
        for (i = 0; i < 3; i++)
        {
            name[0] = 0;
            hr = SC_EnumRoutine(script, i, name);
            CHECKF(hr == S_OK && !lstrcmpW(name, routines[i]), "EnumRoutine(%d) = %#lx %ls", i, hr, name);
        }
        EXPECT_HR(SC_EnumRoutine(script, 3, name), S_FALSE);
        for (i = 0; i < 4; i++)
        {
            name[0] = 0;
            hr = SC_EnumVariable(script, i, name);
            CHECKF(hr == S_OK && !lstrcmpW(name, variables[i]), "EnumVariable(%d) = %#lx %ls", i, hr, name);
        }
        EXPECT_HR(SC_EnumVariable(script, 4, name), S_FALSE);
    }
    IDirectMusicScript_Release(script);

    /* an error in the global code is reported by Init */
    script = load_script(L"AudioVBScript", L"Dim x\r\nx = 1 / 0\r\n", &load);
    EXPECT_HR(load, S_OK);
    memset(&err, 0, sizeof(err));
    err.dwSize = sizeof(err);
    EXPECT_HR(SC_Init(script, perf, &err), DMUS_E_SCRIPT_ERROR_IN_SCRIPT);
    CHECKF(FAILED(err.hr), "global code error hr %#lx", err.hr);
    IDirectMusicScript_Release(script);

    /* a syntax error as well */
    script = load_script(L"AudioVBScript", L"Sub Broken\r\n  If\r\nEnd Sub\r\n", &load);
    memset(&err, 0, sizeof(err));
    err.dwSize = sizeof(err);
    EXPECT_HR(SC_Init(script, perf, &err), DMUS_E_SCRIPT_ERROR_IN_SCRIPT);
    CHECKF(FAILED(err.hr), "syntax error hr %#lx", err.hr);
    IDirectMusicScript_Release(script);

    /* the JScript flavour */
    script = load_script(L"JScript", js_source, &load);
    EXPECT_HR(load, S_OK);
    EXPECT_HR(SC_Init(script, perf, NULL), S_OK);
    n = get_number(script, L"total", &hr);
    CHECKF(hr == S_OK && n == 10, "total after Init: %#lx %ld", hr, n);
    EXPECT_HR(SC_CallRoutine(script, (WCHAR *)L"Add", NULL), S_OK);
    n = get_number(script, L"total", &hr);
    CHECKF(hr == S_OK && n == 13, "total after Add: %ld", n);
    memset(&err, 0, sizeof(err));
    err.dwSize = sizeof(err);
    EXPECT_HR(SC_CallRoutine(script, (WCHAR *)L"Fail", &err), DMUS_E_SCRIPT_ERROR_IN_SCRIPT);
    CHECKF(FAILED(err.hr), "js error hr %#lx", err.hr);
    {
        static const WCHAR *routines[] = {L"Add", L"Fail", L"Other"};
        static const WCHAR *variables[] = {L"total", L"name", L"spare"};
        for (i = 0; i < 3; i++)
        {
            name[0] = 0;
            hr = SC_EnumRoutine(script, i, name);
            CHECKF(hr == S_OK && !lstrcmpW(name, routines[i]), "js EnumRoutine(%d) = %#lx %ls", i, hr, name);
            name[0] = 0;
            hr = SC_EnumVariable(script, i, name);
            CHECKF(hr == S_OK && !lstrcmpW(name, variables[i]), "js EnumVariable(%d) = %#lx %ls", i, hr, name);
        }
        EXPECT_HR(SC_EnumRoutine(script, 3, name), S_FALSE);
        EXPECT_HR(SC_EnumVariable(script, 3, name), S_FALSE);
    }
    IDirectMusicScript_Release(script);
    IDirectMusicPerformance_Release(perf);
}

/* ---- the script track ---- */
/* a loader that serves the one script object, and a stream that hands it out, like the loader's streams */
static IDirectMusicScript *shared_script;
static int loader_requests;
static BOOL loader_desc_ok = TRUE;

static HRESULT WINAPI ldr_QueryInterface(IDirectMusicLoader *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDirectMusicLoader)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI ldr_AddRef(IDirectMusicLoader *iface) { return 2; }
static ULONG WINAPI ldr_Release(IDirectMusicLoader *iface) { return 1; }
static HRESULT WINAPI ldr_GetObject(IDirectMusicLoader *iface, DMUS_OBJECTDESC *desc, REFIID riid, void **out)
{
    loader_requests++;
    if (!(desc->dwValidData & DMUS_OBJ_CLASS) || !IsEqualGUID(&desc->guidClass, &CLSID_DirectMusicScript)
            || !(desc->dwValidData & DMUS_OBJ_FILENAME) || lstrcmpW(desc->wszFileName, L"sgprobe.spt"))
        loader_desc_ok = FALSE;
    return IDirectMusicScript_QueryInterface(shared_script, riid, out);
}
static HRESULT WINAPI ldr_SetObject(IDirectMusicLoader *i, DMUS_OBJECTDESC *d) { return E_NOTIMPL; }
static HRESULT WINAPI ldr_SetSearchDirectory(IDirectMusicLoader *i, REFGUID g, WCHAR *p, BOOL c) { return E_NOTIMPL; }
static HRESULT WINAPI ldr_ScanDirectory(IDirectMusicLoader *i, REFGUID g, WCHAR *e, WCHAR *s) { return E_NOTIMPL; }
static HRESULT WINAPI ldr_CacheObject(IDirectMusicLoader *i, IDirectMusicObject *o) { return E_NOTIMPL; }
static HRESULT WINAPI ldr_ReleaseObject(IDirectMusicLoader *i, IDirectMusicObject *o) { return E_NOTIMPL; }
static HRESULT WINAPI ldr_ClearCache(IDirectMusicLoader *i, REFGUID g) { return E_NOTIMPL; }
static HRESULT WINAPI ldr_EnableCache(IDirectMusicLoader *i, REFGUID g, BOOL e) { return E_NOTIMPL; }
static HRESULT WINAPI ldr_EnumObject(IDirectMusicLoader *i, REFGUID g, DWORD x, DMUS_OBJECTDESC *d) { return E_NOTIMPL; }
static const IDirectMusicLoaderVtbl ldr_vtbl = {ldr_QueryInterface, ldr_AddRef, ldr_Release, ldr_GetObject,
        ldr_SetObject, ldr_SetSearchDirectory, ldr_ScanDirectory, ldr_CacheObject, ldr_ReleaseObject,
        ldr_ClearCache, ldr_EnableCache, ldr_EnumObject};
static IDirectMusicLoader probe_loader = {(IDirectMusicLoaderVtbl *)&ldr_vtbl};

struct lstream
{
    IStream IStream_iface;
    IDirectMusicGetLoader getter;
    IStream *inner;
};
static struct lstream *ls_from_stream(IStream *iface) { return (struct lstream *)iface; }
static HRESULT WINAPI ls_QueryInterface(IStream *iface, REFIID riid, void **out)
{
    struct lstream *s = ls_from_stream(iface);
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IStream) || IsEqualGUID(riid, &IID_ISequentialStream))
        *out = iface;
    else if (IsEqualGUID(riid, &IID_IDirectMusicGetLoader))
        *out = &s->getter;
    else { *out = NULL; return E_NOINTERFACE; }
    return S_OK;
}
static ULONG WINAPI ls_AddRef(IStream *iface) { return 2; }
static ULONG WINAPI ls_Release(IStream *iface) { return 1; }
static HRESULT WINAPI ls_Read(IStream *iface, void *p, ULONG n, ULONG *r) { return IStream_Read(ls_from_stream(iface)->inner, p, n, r); }
static HRESULT WINAPI ls_Write(IStream *iface, const void *p, ULONG n, ULONG *r) { return E_NOTIMPL; }
static HRESULT WINAPI ls_Seek(IStream *iface, LARGE_INTEGER m, DWORD o, ULARGE_INTEGER *np) { return IStream_Seek(ls_from_stream(iface)->inner, m, o, np); }
static HRESULT WINAPI ls_SetSize(IStream *iface, ULARGE_INTEGER s) { return E_NOTIMPL; }
static HRESULT WINAPI ls_CopyTo(IStream *iface, IStream *d, ULARGE_INTEGER c, ULARGE_INTEGER *r, ULARGE_INTEGER *w) { return E_NOTIMPL; }
static HRESULT WINAPI ls_Commit(IStream *iface, DWORD f) { return E_NOTIMPL; }
static HRESULT WINAPI ls_Revert(IStream *iface) { return E_NOTIMPL; }
static HRESULT WINAPI ls_LockRegion(IStream *iface, ULARGE_INTEGER o, ULARGE_INTEGER c, DWORD t) { return E_NOTIMPL; }
static HRESULT WINAPI ls_UnlockRegion(IStream *iface, ULARGE_INTEGER o, ULARGE_INTEGER c, DWORD t) { return E_NOTIMPL; }
static HRESULT WINAPI ls_Stat(IStream *iface, STATSTG *s, DWORD f) { return IStream_Stat(ls_from_stream(iface)->inner, s, f); }
static HRESULT WINAPI ls_Clone(IStream *iface, IStream **c) { return E_NOTIMPL; }
static const IStreamVtbl ls_vtbl = {ls_QueryInterface, ls_AddRef, ls_Release, ls_Read, ls_Write, ls_Seek, ls_SetSize,
        ls_CopyTo, ls_Commit, ls_Revert, ls_LockRegion, ls_UnlockRegion, ls_Stat, ls_Clone};
static HRESULT WINAPI lg_QueryInterface(IDirectMusicGetLoader *iface, REFIID riid, void **out)
{
    struct lstream *s = (struct lstream *)((char *)iface - offsetof(struct lstream, getter));
    return ls_QueryInterface(&s->IStream_iface, riid, out);
}
static ULONG WINAPI lg_AddRef(IDirectMusicGetLoader *iface) { return 2; }
static ULONG WINAPI lg_Release(IDirectMusicGetLoader *iface) { return 1; }
static HRESULT WINAPI lg_GetLoader(IDirectMusicGetLoader *iface, IDirectMusicLoader **ldr)
{
    *ldr = &probe_loader;
    return S_OK;
}
static const IDirectMusicGetLoaderVtbl lg_vtbl = {lg_QueryInterface, lg_AddRef, lg_Release, lg_GetLoader};
static void lstream_init(struct lstream *s, IStream *inner)
{
    s->IStream_iface.lpVtbl = (IStreamVtbl *)&ls_vtbl;
    s->getter.lpVtbl = (IDirectMusicGetLoaderVtbl *)&lg_vtbl;
    s->inner = inner;
}

static void add_event(DWORD flags, MUSIC_TIME logical, MUSIC_TIME physical, const WCHAR *routine)
{
    DMUS_IO_SCRIPTTRACK_EVENTHEADER header = {flags, logical, physical};
    DMUS_IO_REFERENCE ref = {{0}, DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME};
    int e = begin("LIST", "scre"), r;

    chunk("scrh", &header, sizeof(header));
    ref.guidClassID = CLSID_DirectMusicScript;
    r = begin("LIST", "DMRF");
    chunk("refh", &ref, sizeof(ref));
    wchunk("file", L"sgprobe.spt");
    end(r);
    wchunk("scrn", routine);
    end(e);
}

static void build_track_list(void)
{
    int scrt = begin("LIST", "scrt"), scrl = begin("LIST", "scrl");

    add_event(DMUS_IO_SCRIPTTRACKF_ATTIME, 10, 10, L"Bump");
    add_event(DMUS_IO_SCRIPTTRACKF_QUEUE, 20, 20, L"Twice");
    add_event(DMUS_IO_SCRIPTTRACKF_PREPARE, 50, 50, L"Boom");
    end(scrl);
    end(scrt);
}

static void test_script_track(void)
{
    IDirectMusicTrack8 *track, *clone;
    IDirectMusicPerformance *perf;
    IDirectMusicSegmentState *state = (IDirectMusicSegmentState *)0x1000;
    IDirectMusicScript *script;
    struct lstream ls;
    void *data = NULL;
    HRESULT hr, load;
    IPersistStream *ps;
    IStream *stream;
    LONG n;

    script = load_script(L"AudioVBScript", vb_source, &load);
    shared_script = script;

    CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance, (void **)&perf);
    IDirectMusicPerformance_Init(perf, NULL, NULL, NULL);

    hr = CoCreateInstance(&CLSID_DirectMusicScriptTrack, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack8, (void **)&track);
    CHECKF(hr == S_OK, "create the script track: %#lx", hr);
    IDirectMusicTrack8_QueryInterface(track, &IID_IPersistStream, (void **)&ps);

    /* load errors */
    EXPECT_HR(IPersistStream_Load(ps, NULL), E_POINTER);
    begin("LIST", "lyrc");
    stream = make_stream();
    lstream_init(&ls, stream);
    EXPECT_HR(IPersistStream_Load(ps, &ls.IStream_iface), DMUS_E_UNSUPPORTED_STREAM);
    IStream_Release(stream);
    {
        int scrt = begin("LIST", "scrt");
        begin("LIST", "xxxx");
        riff_len = scrt + 8 + 4 + 8 + 4;
        end(scrt);
    }
    stream = make_stream();
    lstream_init(&ls, stream);
    EXPECT_HR(IPersistStream_Load(ps, &ls.IStream_iface), DMUS_E_UNSUPPORTED_STREAM);
    IStream_Release(stream);

    /* the real thing */
    build_track_list();
    stream = make_stream();
    lstream_init(&ls, stream);
    EXPECT_HR(IPersistStream_Load(ps, &ls.IStream_iface), S_OK);
    IStream_Release(stream);
    CHECKF(loader_requests == 3 && loader_desc_ok, "the loader was asked for the script %d times, description ok %d",
            loader_requests, loader_desc_ok);
    IPersistStream_Release(ps);

    /* argument checks */
    EXPECT_HR(IDirectMusicTrack8_Init(track, NULL), E_POINTER);
    EXPECT_HR(IDirectMusicTrack8_InitPlay(track, NULL, NULL, NULL, 0, 0), E_POINTER);
    EXPECT_HR(IDirectMusicTrack8_InitPlay(track, state, perf, NULL, 0, 0), E_POINTER);
    EXPECT_HR(IDirectMusicTrack8_EndPlay(track, NULL), E_POINTER);
    EXPECT_HR(IDirectMusicTrack8_Play(track, NULL, 0, 0, 0, 0, NULL, NULL, 0), E_POINTER);
    EXPECT_HR(IDirectMusicTrack8_PlayEx(track, NULL, 0, 0, 0, 0, NULL, NULL, 0), E_POINTER);
    EXPECT_HR(IDirectMusicTrack8_Clone(track, 0, 10, NULL), E_POINTER);
    EXPECT_HR(IDirectMusicTrack8_Clone(track, -1, 10, (IDirectMusicTrack **)&clone), E_INVALIDARG);
    EXPECT_HR(IDirectMusicTrack8_Clone(track, 11, 10, (IDirectMusicTrack **)&clone), E_INVALIDARG);
    EXPECT_HR(IDirectMusicTrack8_GetParamEx(track, &GUID_TempoParam, 0, NULL, NULL, NULL, 0), DMUS_E_GET_UNSUPPORTED);
    EXPECT_HR(IDirectMusicTrack8_SetParamEx(track, &GUID_TempoParam, 0, NULL, NULL, 0), DMUS_E_SET_UNSUPPORTED);

    /* InitPlay starts the scripts the events call: the counter is 5 afterwards */
    EXPECT_HR(IDirectMusicTrack8_InitPlay(track, state, perf, &data, 0, 0), S_OK);
    CHECKF(data != NULL, "InitPlay returns the state data");
    n = get_number(script, L"counter", &hr);
    CHECKF(hr == S_OK && n == 5, "script started by InitPlay: %#lx %ld", hr, n);

    /* Play runs the routines of the events in the time range */
    EXPECT_HR(IDirectMusicTrack8_Play(track, data, 0, 10, 0, 0, perf, state, 0), S_OK);
    n = get_number(script, L"counter", &hr);
    CHECKF(n == 5, "nothing before the first event: %ld", n);
    EXPECT_HR(IDirectMusicTrack8_Play(track, data, 10, 11, 0, 0, perf, state, 0), S_OK);
    n = get_number(script, L"counter", &hr);
    CHECKF(n == 6, "Bump at 10: %ld", n);
    EXPECT_HR(IDirectMusicTrack8_Play(track, data, 0, 30, 0, 0, perf, state, 0), S_OK);
    n = get_number(script, L"counter", &hr);
    CHECKF(n == 14, "Bump and Twice over 0-30 ((6+1)*2): %ld", n);
    EXPECT_HR(IDirectMusicTrack8_Play(track, data, 30, 40, 0, 0, perf, state, 0), S_OK);
    n = get_number(script, L"counter", &hr);
    CHECKF(n == 14, "nothing in 30-40: %ld", n);

    /* a routine that fails makes Play fail */
    EXPECT_HR(IDirectMusicTrack8_Play(track, data, 40, 60, 0, 0, perf, state, 0), DMUS_E_SCRIPT_ERROR_IN_SCRIPT);

    /* PlayEx is Play in reference time */
    {
        REFERENCE_TIME now = 0;
        EXPECT_HR(IDirectMusicPerformance_GetLatencyTime(perf, &now), S_OK);
        EXPECT_HR(SC_SetVariableNumber(script, (WCHAR *)L"counter", 1000, NULL), S_OK);
        EXPECT_HR(IDirectMusicTrack8_PlayEx(track, data, 60000, 120000, now, 0, perf, state, 0), S_OK);
        n = get_number(script, L"counter", &hr);
        CHECKF(n == 1001, "PlayEx 6ms-12ms plays only Bump (music time 10): %ld", n);
    }

    /* Clone shifts the events to the start of the range */
    EXPECT_HR(IDirectMusicTrack8_Clone(track, 15, 35, (IDirectMusicTrack **)&clone), S_OK);
    {
        void *cdata = NULL;
        EXPECT_HR(SC_SetVariableNumber(script, (WCHAR *)L"counter", 100, NULL), S_OK);
        EXPECT_HR(IDirectMusicTrack8_InitPlay(clone, state, perf, &cdata, 0, 0), S_OK);
        EXPECT_HR(IDirectMusicTrack8_Play(clone, cdata, 0, 4, 0, 0, perf, state, 0), S_OK);
        n = get_number(script, L"counter", &hr);
        CHECKF(n == 100, "the clone calls nothing in 0-4: %ld", n);
        EXPECT_HR(IDirectMusicTrack8_Play(clone, cdata, 5, 6, 0, 0, perf, state, 0), S_OK);
        n = get_number(script, L"counter", &hr);
        CHECKF(n == 200, "Twice at 5 after the shift: %ld", n);
        EXPECT_HR(IDirectMusicTrack8_EndPlay(clone, cdata), S_OK);
    }
    IDirectMusicTrack8_Release(clone);
    EXPECT_HR(IDirectMusicTrack8_EndPlay(track, data), S_OK);

    IDirectMusicTrack8_Release(track);
    IDirectMusicScript_Release(script);
    IDirectMusicPerformance_Release(perf);
}

static void test_audiovbscript_class(void)
{
    IUnknown *engine = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_AudioVBScript, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&engine);
    CHECKF(hr == S_OK && engine, "AudioVBScript class creates an engine: %#lx", hr);
    if (engine) IUnknown_Release(engine);
}

int main(void)
{
    CoInitialize(NULL);
    test_audiovbscript_class();
    test_script_object();
    test_script_track();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
