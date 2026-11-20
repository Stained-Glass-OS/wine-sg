/* ICorDebug object values over Wine Mono's soft debugger agent (wine-sg 8200), for test/cordbgobj-gate.sh:
 * the debugger. Raw vtable slots (cordebug.h is not in mingw); 64-bit.
 *
 *   cordbgobj-probe.exe C:\App.exe   prints KEY=VALUE lines
 *
 * Domains, assemblies and modules; the module's metadata finds App.Hold, a breakpoint there stops the
 * program; Hold's arguments: an object (fields, an inherited one, an object in a field, a struct in a field,
 * a static), arrays (one- and two-dimensional, of ints and strings), a struct, an enum; values set (a field,
 * an element, an argument) are seen by the program.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { void **vtbl; } Obj;
#define CALL(o, i, ret, ...) ((ret (__stdcall *)(Obj*, ##__VA_ARGS__))((o)->vtbl[i]))
#define REL(o) do { if (o) CALL((o), 2, ULONG)(o); } while (0)
#define HR(x) ((unsigned)(x))

static const GUID IID_ICorDebug_ = {0x3d6f5f61, 0x7538, 0x11d3, {0x8d, 0x5b, 0x00, 0x10, 0x4b, 0x35, 0xe7, 0xef}};
static const GUID IID_ICorDebugILFrame_ = {0x03e26311, 0x4f76, 0x11d3, {0x88, 0xc6, 0x00, 0x60, 0x97, 0x94, 0x54, 0x18}};
static const GUID IID_ICorDebugObjectValue_ = {0x18ad3d6e, 0xb7d2, 0x11d2, {0xbd, 0x04, 0x00, 0x00, 0xf8, 0x08, 0x49, 0xbd}};
static const GUID IID_ICorDebugArrayValue_ = {0x0405b0df, 0xa660, 0x11d2, {0xbd, 0x02, 0x00, 0x00, 0xf8, 0x08, 0x49, 0xbd}};
static const GUID IID_ICorDebugGenericValue_ = {0xcc7bcaf8, 0x8a68, 0x11d2, {0x98, 0x3c, 0x00, 0x00, 0xf8, 0x08, 0x34, 0x2d}};
static const GUID IID_ICorDebugReferenceValue_ = {0xcc7bcaf9, 0x8a68, 0x11d2, {0x98, 0x3c, 0x00, 0x00, 0xf8, 0x08, 0x34, 0x2d}};
static const GUID IID_ICorDebugStringValue_ = {0xcc7bcafd, 0x8a68, 0x11d2, {0x98, 0x3c, 0x00, 0x00, 0xf8, 0x08, 0x34, 0x2d}};
static const GUID IID_IMetaDataImport_ = {0x7dac8207, 0xd3ae, 0x4c75, {0x9b, 0x67, 0x92, 0x80, 0x1a, 0x49, 0x7d, 0x44}};

static LONG bp_hits, evals_done, evals_threw;
static Obj *bp_thread;
static void *cb_vtbl[32];
static Obj cb = { cb_vtbl };
static HRESULT __stdcall cb_qi(Obj *o, const GUID *iid, void **out) { *out = o; return S_OK; }
static ULONG __stdcall cb_ref(Obj *o) { return 1; }
static HRESULT __stdcall cb_any(Obj *o, void *a, void *b, void *c, void *d, void *e) { return S_OK; }
static HRESULT __stdcall cb_evalcomplete(Obj *o, Obj *domain, Obj *thread, Obj *eval)
{ InterlockedIncrement(&evals_done); return S_OK; }
static HRESULT __stdcall cb_evalexception(Obj *o, Obj *domain, Obj *thread, Obj *eval)
{ InterlockedIncrement(&evals_threw); return S_OK; }
static HRESULT __stdcall cb_breakpoint(Obj *o, Obj *domain, Obj *thread, Obj *bp)
{
    if (!bp_thread && thread) { bp_thread = thread; CALL(thread, 1, ULONG)(thread); }
    InterlockedIncrement(&bp_hits);
    return S_OK;
}

static HANDLE debuggee;
/* the load callbacks (wine-sg 8209): each stops the process; a debugger continues it */
static LONG domains_created, assemblies_loaded, modules_loaded, xml_loaded;
static HRESULT __stdcall cb_createappdomain(Obj *o, Obj *proc, Obj *domain)
{
    InterlockedIncrement(&domains_created);
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    return S_OK;
}
static HRESULT __stdcall cb_loadassembly(Obj *o, Obj *domain, Obj *assembly)
{
    InterlockedIncrement(&assemblies_loaded);
    CALL(domain, 4, HRESULT, BOOL)(domain, FALSE);
    return S_OK;
}
static HRESULT __stdcall cb_loadmodule(Obj *o, Obj *domain, Obj *module)
{
    WCHAR name[MAX_PATH];
    ULONG32 len = 0;
    InterlockedIncrement(&modules_loaded);
    if (CALL(module, 6, HRESULT, ULONG32, ULONG32 *, WCHAR *)(module, MAX_PATH, &len, name) == S_OK && len > 14 &&
        !_wcsicmp(name + wcslen(name) - 14, L"System.Xml.dll"))
        InterlockedIncrement(&xml_loaded);
    CALL(domain, 4, HRESULT, BOOL)(domain, FALSE);
    return S_OK;
}

static int bail(void)
{
    if (debuggee) TerminateProcess(debuggee, 1);   /* (it holds the gate's output pipe while it lives) */
    fflush(stdout);
    return 1;
}

static Obj *qi(Obj *o, const GUID *iid)
{
    Obj *r = NULL;
    if (!o || CALL(o, 0, HRESULT, const GUID *, void **)(o, iid, (void **)&r) != S_OK) return NULL;
    return r;
}

/* a generic value's bytes (8 at most) */
static long long gen(Obj *v)
{
    Obj *g = qi(v, &IID_ICorDebugGenericValue_);
    long long r = -9999;
    unsigned char raw[8] = {0};
    ULONG32 size = 0;
    if (!g) return r;
    CALL(g, 4, HRESULT, ULONG32 *)(g, &size);
    if (CALL(g, 7, HRESULT, void *)(g, raw) == S_OK)
    {
        if (size == 1) r = (signed char)raw[0];
        else if (size == 2) r = *(short *)raw;
        else if (size == 4) r = *(int *)raw;
        else memcpy(&r, raw, 8);
    }
    REL(g);
    return r;
}

static double gen_double(Obj *v)
{
    double d = -1;
    Obj *g = qi(v, &IID_ICorDebugGenericValue_);
    if (g) { CALL(g, 7, HRESULT, void *)(g, &d); REL(g); }
    return d;
}

/* what a reference refers to */
static Obj *deref(Obj *v)
{
    Obj *r = qi(v, &IID_ICorDebugReferenceValue_), *out = NULL;
    if (!r) return NULL;
    CALL(r, 10, HRESULT, Obj **)(r, &out);
    REL(r);
    return out;
}

static int is_null(Obj *v)
{
    Obj *r = qi(v, &IID_ICorDebugReferenceValue_);
    BOOL n = FALSE;
    if (!r) return -1;
    CALL(r, 7, HRESULT, BOOL *)(r, &n);
    REL(r);
    return n;
}

/* a string reference's text (UTF-8) */
static const char *str(Obj *v)
{
    static char u[256];
    Obj *o = deref(v), *s = qi(o, &IID_ICorDebugStringValue_);
    WCHAR w[128];
    ULONG32 n = 0;
    strcpy(u, "(none)");
    if (s && CALL(s, 10, HRESULT, ULONG32, ULONG32 *, WCHAR *)(s, 127, &n, w) == S_OK)
    {
        w[n] = 0;
        WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL);
    }
    REL(s); REL(o);
    return u;
}

static int type_of(Obj *v)
{
    int t = -1;
    if (v) CALL(v, 3, HRESULT, int *)(v, &t);
    return t;
}

static Obj *field(Obj *obj, Obj *cls, unsigned int token)
{
    Obj *o = qi(obj, &IID_ICorDebugObjectValue_), *v = NULL;
    HRESULT hr;
    if (!o) return NULL;
    hr = CALL(o, 8, HRESULT, Obj *, unsigned int, Obj **)(o, cls, token, &v);
    REL(o);
    return hr == S_OK ? v : NULL;
}

static Obj *element(Obj *arr, ULONG32 n, ULONG32 *idx)
{
    Obj *a = qi(arr, &IID_ICorDebugArrayValue_), *v = NULL;
    if (!a) return NULL;
    CALL(a, 15, HRESULT, ULONG32, ULONG32 *, Obj **)(a, n, idx, &v);
    REL(a);
    return v;
}

static int ends_with(const WCHAR *s, const WCHAR *tail)
{
    size_t a = wcslen(s), b = wcslen(tail);
    return a >= b && !_wcsicmp(s + a - b, tail);
}

static unsigned int find_type(Obj *md, const WCHAR *name)
{
    unsigned int td = 0;
    CALL(md, 9, HRESULT, const WCHAR *, unsigned int, unsigned int *)(md, name, 0, &td);
    return td;
}

static unsigned int find_field(Obj *md, unsigned int td, const WCHAR *name)
{
    unsigned int fd = 0;
    CALL(md, 28, HRESULT, unsigned int, const WCHAR *, const BYTE *, ULONG, unsigned int *)(md, td, name, NULL, 0, &fd);
    return fd;
}

static Obj *class_of(Obj *module, unsigned int td)
{
    Obj *c = NULL;
    CALL(module, 11, HRESULT, unsigned int, Obj **)(module, td, &c);
    return c;
}

static void set_int(Obj *v, int value, const char *tag)
{
    Obj *g = qi(v, &IID_ICorDebugGenericValue_);
    HRESULT hr = g ? CALL(g, 8, HRESULT, void *)(g, &value) : E_NOINTERFACE;
    printf("Set.%s=%#x\n", tag, HR(hr));
    REL(g);
}

int main(int argc, char **argv)
{
    typedef HRESULT (WINAPI *PFN_CDI)(int, LPCWSTR, IUnknown **);
    WCHAR app[MAX_PATH], cmd[MAX_PATH + 8], name[MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    IUnknown *unk = NULL;
    Obj *dbg = NULL, *proc = NULL, *en = NULL, *domain = NULL, *asm_ = NULL, *appasm = NULL, *module = NULL, *md = NULL;
    Obj *mod2 = NULL, *fn = NULL, *fcls = NULL, *fmod = NULL, *bp = NULL, *fr = NULL, *il = NULL, *tmp = NULL;
    Obj *a[7] = {0}, *d = NULL, *cNode, *cDerived, *cPoint, *v, *v2, *arr;
    unsigned int tdApp, tdNode, tdDerived, tdPoint, mdHold = 0, tok = 0;
    unsigned int fId, fName, fNext, fWeight, fAt, fKind, fExtra, fX, fY, fMade;
    ULONG n = 0, fetched, assemblies = 0;
    ULONG32 len = 0, idx[2], dims[2] = {0, 0}, rank = 0, count = 0;
    int i, apps = 0;
    char seenpath[64], seen[64] = "";
    FILE *f;
    HRESULT hr;

    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, app, MAX_PATH);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", app);
    for (i = 0; i < 32; i++) cb_vtbl[i] = cb_any;
    cb_vtbl[0] = cb_qi; cb_vtbl[1] = cb_ref; cb_vtbl[2] = cb_ref; cb_vtbl[3] = cb_breakpoint;
    cb_vtbl[7] = cb_evalcomplete; cb_vtbl[8] = cb_evalexception;
    cb_vtbl[13] = cb_loadmodule; cb_vtbl[20] = cb_createappdomain; cb_vtbl[22] = cb_loadassembly;

    CoInitialize(NULL);
    {
        PFN_CDI create = (PFN_CDI)GetProcAddress(LoadLibraryW(L"mscoree.dll"), "CreateDebuggingInterfaceFromVersion");
        if (!create || (create(3, L"v4.0.30319", &unk) != S_OK && create(3, L"v2.0.50727", &unk) != S_OK)) { printf("Debugger=0\n"); return 1; }
    }
    CALL((Obj *)unk, 0, HRESULT, const GUID *, void **)((Obj *)unk, &IID_ICorDebug_, (void **)&dbg);
    CALL(dbg, 3, HRESULT)(dbg);
    CALL(dbg, 5, HRESULT, Obj *)(dbg, &cb);
    memset(&pi, 0, sizeof(pi));
    hr = CALL(dbg, 7, HRESULT, LPCWSTR, LPWSTR, void *, void *, BOOL, DWORD, void *, LPCWSTR, LPSTARTUPINFOW,
              LPPROCESS_INFORMATION, DWORD, Obj **)(dbg, app, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi, 0, &proc);
    printf("Launch=%#x\n", HR(hr));
    if (FAILED(hr)) return 1;
    debuggee = pi.hProcess;
    sprintf(seenpath, "C:\\seen-%lu.txt", pi.dwProcessId);
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    Sleep(3000);
    CALL(proc, 3, HRESULT, DWORD)(proc, 0);
    fflush(stdout);

    /* domains, assemblies, modules */
    hr = CALL(proc, 26, HRESULT, Obj **)(proc, &en);
    if (SUCCEEDED(hr)) { CALL(en, 6, HRESULT, ULONG *)(en, &n); CALL(en, 7, HRESULT, ULONG, Obj **, ULONG *)(en, 1, &domain, &fetched); REL(en); }
    printf("Domains=%#x %lu %d\n", HR(hr), n, domain != NULL);
    if (!domain) return bail();
    en = NULL;
    hr = CALL(domain, 14, HRESULT, Obj **)(domain, &en);
    while (SUCCEEDED(hr) && CALL(en, 7, HRESULT, ULONG, Obj **, ULONG *)(en, 1, &asm_, &fetched) == S_OK && fetched == 1)
    {
        assemblies++;
        len = 0;
        if (CALL(asm_, 7, HRESULT, ULONG32, ULONG32 *, WCHAR *)(asm_, MAX_PATH, &len, name) == S_OK && ends_with(name, L"App.exe"))
        {
            apps++;
            appasm = asm_;
        }
        else REL(asm_);
    }
    if (en) REL(en);
    printf("Assemblies=%#x %d %d\n", HR(hr), assemblies >= 2, apps == 1);
    if (!appasm) return bail();
    en = NULL;
    n = 0;
    hr = CALL(appasm, 5, HRESULT, Obj **)(appasm, &en);
    if (SUCCEEDED(hr)) { CALL(en, 6, HRESULT, ULONG *)(en, &n); CALL(en, 7, HRESULT, ULONG, Obj **, ULONG *)(en, 1, &module, &fetched); REL(en); }
    printf("Modules=%#x %lu\n", HR(hr), n);
    if (!module) return bail();
    len = 0;
    CALL(module, 6, HRESULT, ULONG32, ULONG32 *, WCHAR *)(module, MAX_PATH, &len, name);
    printf("Module.Name=%d\n", len == wcslen(name) + 1 && ends_with(name, L"App.exe"));
    CALL(module, 5, HRESULT, Obj **)(module, &tmp);
    printf("Module.Assembly=%d\n", tmp == appasm);
    REL(tmp); tmp = NULL;
    hr = CALL(module, 14, HRESULT, const GUID *, IUnknown **)(module, &IID_IMetaDataImport_, (IUnknown **)&md);
    printf("Module.MetaData=%#x\n", HR(hr));
    if (!md) return bail();
    hr = CALL(domain, 15, HRESULT, Obj *, Obj **)(domain, md, &mod2);
    printf("Module.FromMetaData=%#x %d\n", HR(hr), mod2 == module);
    REL(mod2);

    tdApp = find_type(md, L"App"); tdNode = find_type(md, L"Node"); tdDerived = find_type(md, L"Derived");
    tdPoint = find_type(md, L"Point");
    CALL(md, 27, HRESULT, unsigned int, const WCHAR *, const BYTE *, ULONG, unsigned int *)(md, tdApp, L"Hold", NULL, 0, &mdHold);
    fId = find_field(md, tdNode, L"Id"); fName = find_field(md, tdNode, L"Name"); fNext = find_field(md, tdNode, L"Next");
    fWeight = find_field(md, tdNode, L"Weight"); fAt = find_field(md, tdNode, L"At"); fKind = find_field(md, tdNode, L"Kind");
    fExtra = find_field(md, tdDerived, L"Extra");
    fX = find_field(md, tdPoint, L"X"); fY = find_field(md, tdPoint, L"Y"); fMade = find_field(md, tdPoint, L"Made");
    printf("MetaData.Tokens=%d\n", tdApp && tdNode && tdDerived && tdPoint && mdHold && fId && fExtra && fX && fMade);

    hr = CALL(module, 9, HRESULT, unsigned int, Obj **)(module, mdHold, &fn);
    if (fn) CALL(fn, 5, HRESULT, unsigned int *)(fn, &tok);
    printf("Function=%#x %d\n", HR(hr), tok == mdHold && tok);
    if (!fn) return bail();
    CALL(fn, 3, HRESULT, Obj **)(fn, &fmod);
    printf("Function.Module=%d\n", fmod == module);
    REL(fmod);
    tok = 0;
    if (CALL(fn, 4, HRESULT, Obj **)(fn, &fcls) == S_OK) CALL(fcls, 4, HRESULT, unsigned int *)(fcls, &tok);
    printf("Function.Class=%d\n", tok == tdApp);
    REL(fcls);
    cNode = class_of(module, tdNode); cDerived = class_of(module, tdDerived); cPoint = class_of(module, tdPoint);
    printf("Classes=%d\n", cNode && cDerived && cPoint);
    fflush(stdout);

    /* stopped in Hold */
    hr = CALL(fn, 8, HRESULT, Obj **)(fn, &bp);
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    for (i = 0; i < 200 && !bp_hits; i++) Sleep(50);
    printf("Hold=%#x %d\n", HR(hr), bp_hits >= 1);
    if (!bp_thread) return bail();
    CALL(bp_thread, 15, HRESULT, Obj **)(bp_thread, &fr);
    il = qi(fr, &IID_ICorDebugILFrame_);
    if (!il) { printf("Frame=0\n"); return bail(); }
    for (i = 0; i < 7; i++) CALL(il, 16, HRESULT, DWORD, Obj **)(il, i, &a[i]);

    /* an object */
    d = deref(a[0]);
    printf("Object=%#x %d\n", type_of(d), qi(d, &IID_ICorDebugObjectValue_) != NULL);
    tok = 0;
    {
        Obj *o = qi(d, &IID_ICorDebugObjectValue_), *c = NULL;
        BOOL vc = TRUE;
        if (o && CALL(o, 7, HRESULT, Obj **)(o, &c) == S_OK) CALL(c, 4, HRESULT, unsigned int *)(c, &tok);
        if (o) CALL(o, 11, HRESULT, BOOL *)(o, &vc);
        printf("Object.Class=%d %d\n", tok == tdDerived, !vc);
        REL(c); REL(o);
    }
    v = field(d, cNode, fId); printf("Field.Id=%lld\n", gen(v));
    set_int(v, 500, "Field"); REL(v);
    v = field(d, cNode, fId); printf("Field.IdAgain=%lld\n", gen(v)); REL(v);
    v = field(d, cNode, fName); printf("Field.Name=%s\n", str(v)); REL(v);
    v = field(d, cNode, fWeight); printf("Field.Weight=%.2f\n", gen_double(v)); REL(v);
    v = field(d, cDerived, fExtra); printf("Field.Extra=%lld\n", gen(v)); REL(v);
    v = field(d, NULL, fExtra); printf("Field.ExtraNoClass=%lld\n", gen(v)); REL(v);
    v = field(d, cNode, fNext); v2 = deref(v); REL(v);
    v = field(v2, cNode, fId); printf("Field.NextId=%lld\n", gen(v)); REL(v); REL(v2);
    v = field(d, cNode, fAt);
    {
        Obj *o = qi(v, &IID_ICorDebugObjectValue_);
        BOOL vc = FALSE;
        if (o) CALL(o, 11, HRESULT, BOOL *)(o, &vc);
        REL(o);
        v2 = field(v, cPoint, fX);
        printf("Field.At=%#x %d %lld", type_of(v), vc, gen(v2)); REL(v2);
        v2 = field(v, cPoint, fY); printf(" %lld\n", gen(v2)); REL(v2);
    }
    REL(v);
    v = NULL;
    hr = cNode ? CALL(cNode, 5, HRESULT, unsigned int, Obj *, Obj **)(cNode, fKind, fr, &v) : E_FAIL;
    printf("Static.Kind=%#x %s\n", HR(hr), str(v)); REL(v);
    v = field(d, cNode, 0x04ffffff);
    printf("Field.Bad=%d\n", v == NULL); REL(v);

    /* arrays */
    arr = deref(a[1]);
    {
        Obj *x = qi(arr, &IID_ICorDebugArrayValue_);
        int et = 0;
        if (x)
        {
            CALL(x, 9, HRESULT, int *)(x, &et); CALL(x, 10, HRESULT, ULONG32 *)(x, &rank); CALL(x, 11, HRESULT, ULONG32 *)(x, &count);
        }
        printf("Array=%#x %#x %u %u\n", type_of(arr), et, rank, count);
        idx[0] = 2; v = element(arr, 1, idx); printf("Array.2=%lld\n", gen(v)); REL(v);
        v = NULL; if (x) CALL(x, 16, HRESULT, ULONG32, Obj **)(x, 3, &v); printf("Array.At3=%lld\n", gen(v)); REL(v);
        v = NULL; hr = x ? CALL(x, 16, HRESULT, ULONG32, Obj **)(x, 4, &v) : E_FAIL; printf("Array.Out=%d\n", hr == E_INVALIDARG); REL(v);
        idx[0] = 0; v = element(arr, 1, idx); set_int(v, 111, "Element"); REL(v);
        REL(x);
    }
    REL(arr);
    arr = deref(a[2]);
    idx[0] = 1; v = element(arr, 1, idx); printf("Words.1=%d\n", is_null(v)); REL(v);
    idx[0] = 2; v = element(arr, 1, idx); printf("Words.2=%s\n", str(v)); REL(v);
    REL(arr);
    arr = deref(a[3]);
    {
        Obj *x = qi(arr, &IID_ICorDebugArrayValue_);
        rank = 0;
        if (x) { CALL(x, 10, HRESULT, ULONG32 *)(x, &rank); CALL(x, 12, HRESULT, ULONG32, ULONG32 *)(x, 2, dims); }
        idx[0] = 1; idx[1] = 2; v = element(arr, 2, idx);
        printf("Grid=%#x %u %u %u %lld\n", type_of(arr), rank, dims[0], dims[1], gen(v)); REL(v);
        REL(x);
    }
    REL(arr);

    /* evaluation (wine-sg 8201): continued, each runs; the process stops again when it is done */
    {
        Obj *ev = NULL, *arg = NULL, *res = NULL, *fTwice = NULL, *fFail = NULL, *fCtor = NULL;
        unsigned int mdTwice = 0, mdFail = 0, mdCtor = 0;
        int x = 21;
        BOOL running = TRUE;
        LONG before;

        CALL(md, 27, HRESULT, unsigned int, const WCHAR *, const BYTE *, ULONG, unsigned int *)(md, tdApp, L"Twice", NULL, 0, &mdTwice);
        CALL(md, 27, HRESULT, unsigned int, const WCHAR *, const BYTE *, ULONG, unsigned int *)(md, tdApp, L"Fail", NULL, 0, &mdFail);
        CALL(md, 27, HRESULT, unsigned int, const WCHAR *, const BYTE *, ULONG, unsigned int *)(md, tdNode, L".ctor", NULL, 0, &mdCtor);
        CALL(module, 9, HRESULT, unsigned int, Obj **)(module, mdTwice, &fTwice);
        CALL(module, 9, HRESULT, unsigned int, Obj **)(module, mdFail, &fFail);
        CALL(module, 9, HRESULT, unsigned int, Obj **)(module, mdCtor, &fCtor);
        hr = CALL(bp_thread, 17, HRESULT, Obj **)(bp_thread, &ev);
        printf("Eval.Create=%#x\n", HR(hr));
        if (ev && fTwice && fFail && fCtor)
        {
            hr = CALL(ev, 12, HRESULT, int, Obj *, Obj **)(ev, 8, NULL, &arg);
            if (arg) { Obj *g = qi(arg, &IID_ICorDebugGenericValue_); if (g) { CALL(g, 8, HRESULT, void *)(g, &x); REL(g); } }
            printf("Eval.Value=%#x %lld\n", HR(hr), gen(arg));
            hr = CALL(ev, 10, HRESULT, Obj **)(ev, &res);
            printf("Eval.NotYet=%d\n", hr == (HRESULT)0x80131315);
            hr = CALL(ev, 3, HRESULT, Obj *, ULONG32, Obj **)(ev, fTwice, 1, &arg);
            before = evals_done;
            CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
            for (i = 0; i < 200 && evals_done == before; i++) Sleep(50);
            CALL(proc, 5, HRESULT, BOOL *)(proc, &running);
            res = NULL; CALL(ev, 10, HRESULT, Obj **)(ev, &res);
            printf("Eval.Call=%#x %d %d %lld\n", HR(hr), evals_done > before, !running, gen(res));
            REL(res); res = NULL;
            hr = CALL(ev, 6, HRESULT, const WCHAR *)(ev, L"sg-eval");
            before = evals_done;
            CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
            for (i = 0; i < 200 && evals_done == before; i++) Sleep(50);
            CALL(ev, 10, HRESULT, Obj **)(ev, &res);
            printf("Eval.String=%#x %d %s\n", HR(hr), evals_done > before, str(res));
            REL(res); res = NULL;
            hr = CALL(ev, 4, HRESULT, Obj *, ULONG32, Obj **)(ev, fCtor, 0, NULL);
            before = evals_done;
            CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
            for (i = 0; i < 200 && evals_done == before; i++) Sleep(50);
            CALL(ev, 10, HRESULT, Obj **)(ev, &res);
            v2 = deref(res);
            v = field(v2, cNode, fWeight);
            printf("Eval.NewObject=%#x %d %#x %.2f\n", HR(hr), evals_done > before, type_of(v2), gen_double(v));
            REL(v); REL(v2); REL(res); res = NULL;
            hr = CALL(ev, 3, HRESULT, Obj *, ULONG32, Obj **)(ev, fFail, 0, NULL);
            before = evals_threw;
            CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
            for (i = 0; i < 200 && evals_threw == before; i++) Sleep(50);
            CALL(ev, 10, HRESULT, Obj **)(ev, &res);
            v2 = deref(res);
            printf("Eval.Throws=%#x %d %#x\n", HR(hr), evals_threw > before, type_of(v2));
            REL(v2); REL(res);
        }
        REL(arg); REL(ev); REL(fTwice); REL(fFail); REL(fCtor);
    }
    fflush(stdout);

    /* a struct, an enum, an int */
    v = field(a[4], cPoint, fX); v2 = field(a[4], cPoint, fY);
    printf("Struct=%#x %lld %lld\n", type_of(a[4]), gen(v), gen(v2)); REL(v); REL(v2);
    v = NULL;
    hr = cPoint ? CALL(cPoint, 5, HRESULT, unsigned int, Obj *, Obj **)(cPoint, fMade, fr, &v) : E_FAIL;
    printf("Static.Made=%#x %lld\n", HR(hr), gen(v)); REL(v);
    printf("Enum=%#x %lld\n", type_of(a[5]), gen(a[5]));
    printf("Round=%#x\n", type_of(a[6]));
    set_int(a[6], 1234, "Argument");
    fflush(stdout);

    /* what was set, the program sees */
    CALL(bp, 3, HRESULT, BOOL)(bp, FALSE);
    CALL(proc, 4, HRESULT, BOOL)(proc, FALSE);
    for (i = 0; i < 100; i++)
    {
        Sleep(100);
        if ((f = fopen(seenpath, "r"))) { fgets(seen, sizeof(seen), f); fclose(f); if (seen[0]) break; }
    }
    printf("Seen=%s\n", seen[0] ? seen : "nothing");
    printf("Loads=%ld %d %d %ld\n", domains_created, assemblies_loaded >= 2, modules_loaded >= 2, xml_loaded);
    CALL(proc, 3, HRESULT, DWORD)(proc, 0);
    for (i = 0; i < 7; i++) REL(a[i]);
    REL(d); REL(il); REL(fr); REL(bp); REL(fn); REL(cNode); REL(cDerived); REL(cPoint);
    REL(md); REL(module); REL(appasm); REL(domain);
    CALL(proc, 10, HRESULT, UINT)(proc, 0);
    REL(proc);
    TerminateProcess(debuggee, 0);
    printf("done=1\n");
    return 0;
}
