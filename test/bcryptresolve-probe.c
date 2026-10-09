/* The CNG configuration store in the registry, BCryptResolveProviders and the
 * configuration change notification (patches/sg/2410), run by
 * test/bcryptresolve-gate.sh. The store used to be per-process memory; now
 * a second process sees what the first configured, a hand-made registry entry
 * is honoured, providers resolve by mode / flags / function, and a registered
 * event is signalled on a change. Reached through GetProcAddress so it builds
 * with any mingw header level. Run with an argument ("child-check",
 * "child-mutate") it is the other process. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef LONG NTS;
#define S_OK_      ((NTS)0)
#define S_INVALID  ((NTS)0xC000000D)
#define S_SMALL    ((NTS)0xC0000023)
#define S_NOTFOUND ((NTS)0xC0000225)

#define T_LOCAL  1
#define T_DOMAIN 2
#define I_CIPHER 1
#define I_HASH   2
#define BOTTOM   0xFFFFFFFF
#define M_UM 1
#define M_KM 2
#define M_MM 3
#define M_ANY 4
#define F_ALLFUNC 1
#define F_ALLPROV 2

typedef struct { ULONG count; WCHAR **list; } strlist;
typedef struct { ULONG flags, reserved; } cfg;
typedef struct { ULONG iface, flags, nfunc; WCHAR **funcs; } p_iface;
typedef struct { WCHAR *image; ULONG nif; p_iface **ifs; } p_image;
typedef struct { ULONG nalias; WCHAR **aliases; p_image *um, *km; } p_reg;
typedef struct { WCHAR *name; ULONG size; BYTE *value; } r_prop;
typedef struct { WCHAR *image; ULONG flags; } r_image;
typedef struct { ULONG iface; WCHAR *func, *prov; ULONG nprops; r_prop **props; r_image *um, *km; } r_ref;
typedef struct { ULONG n; r_ref **refs; } r_refs;

static HMODULE bc;
#define F(name) static NTS (WINAPI *p##name)()
F(BCryptCreateContext); F(BCryptDeleteContext); F(BCryptEnumContexts); F(BCryptAddContextFunction);
F(BCryptRemoveContextFunction); F(BCryptEnumContextFunctions); F(BCryptConfigureContext);
F(BCryptConfigureContextFunction); F(BCryptQueryContextConfiguration); F(BCryptQueryContextFunctionConfiguration);
F(BCryptAddContextFunctionProvider); F(BCryptEnumContextFunctionProviders);
F(BCryptSetContextFunctionProperty); F(BCryptQueryContextFunctionProperty);
F(BCryptRegisterProvider); F(BCryptUnregisterProvider); F(BCryptQueryProviderRegistration);
F(BCryptResolveProviders); F(BCryptRegisterConfigChangeNotify); F(BCryptUnregisterConfigChangeNotify);
static void (WINAPI *pBCryptFreeBuffer)(void *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkst(NTS got, NTS want, const char *what)
{
    char buf[220];
    snprintf(buf, sizeof(buf), "%s (got %08lx, want %08lx)", what, (unsigned long)got, (unsigned long)want);
    check(got == want, buf);
}

#define CFG L"System\\CurrentControlSet\\Control\\Cryptography\\Configuration\\Local"
#define PROVS L"System\\CurrentControlSet\\Control\\Cryptography\\Providers"

static int list_is(const strlist *l, const WCHAR **want)
{
    ULONG i;
    if (!l) return 0;
    for (i = 0; want[i]; i++)
        if (i >= l->count || lstrcmpiW(l->list[i], want[i])) return 0;
    return i == l->count;
}
static int list_has(const strlist *l, const WCHAR *name)
{
    ULONG i;
    for (i = 0; l && i < l->count; i++) if (!lstrcmpiW(l->list[i], name)) return 1;
    return 0;
}

static strlist *funcs(ULONG table, const WCHAR *ctx, ULONG iface)
{
    ULONG size = 0;
    strlist *l = NULL;
    if (pBCryptEnumContextFunctions(table, ctx, iface, &size, &l)) return NULL;
    return l;
}
static strlist *provs(const WCHAR *ctx, ULONG iface, const WCHAR *fn)
{
    ULONG size = 0;
    strlist *l = NULL;
    if (pBCryptEnumContextFunctionProviders(T_LOCAL, ctx, iface, fn, &size, &l)) return NULL;
    return l;
}

/* the REG_MULTI_SZ value `name` of HKLM\<path> equals the given strings */
static int multi_is(const WCHAR *path, const WCHAR *name, const WCHAR **want)
{
    HKEY k;
    WCHAR buf[512], *p;
    DWORD size = sizeof(buf), type;
    ULONG i = 0;
    LONG ret;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &k)) return 0;
    ret = RegQueryValueExW(k, name, NULL, &type, (BYTE *)buf, &size);
    RegCloseKey(k);
    if (ret || type != REG_MULTI_SZ) return 0;
    buf[size / sizeof(WCHAR)] = 0;
    for (p = buf; *p; p += lstrlenW(p) + 1, i++)
        if (!want[i] || lstrcmpW(p, want[i])) return 0;
    return !want[i];
}
static int key_exists(const WCHAR *path)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &k)) return 0;
    RegCloseKey(k);
    return 1;
}
static int dword_is(const WCHAR *path, const WCHAR *name, DWORD want)
{
    HKEY k;
    DWORD v = 0, size = sizeof(v), type;
    LONG ret;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &k)) return 0;
    ret = RegQueryValueExW(k, name, NULL, &type, (BYTE *)&v, &size);
    RegCloseKey(k);
    return !ret && type == REG_DWORD && v == want;
}

static const WCHAR *w_sga_sgb[] = { L"SGA", L"SGB", NULL };
static const WCHAR *w_sgb_sga[] = { L"SGB", L"SGA", NULL };
static const WCHAR *w_sga[] = { L"SGA", NULL };
static const WCHAR *w_pa[] = { L"SgPersistProvA", NULL };

static void cleanup(void)
{
    pBCryptDeleteContext(T_LOCAL, L"SgPersist");
    pBCryptDeleteContext(T_DOMAIN, L"SgPersist");
    pBCryptDeleteContext(T_LOCAL, L"SgResolve");
    pBCryptDeleteContext(T_DOMAIN, L"SgDomRes");
    pBCryptDeleteContext(T_LOCAL, L"SgNotify");
    pBCryptDeleteContext(T_LOCAL, L"SgNotifyChild");
    pBCryptUnregisterProvider(L"SgResProvA");
    pBCryptUnregisterProvider(L"SgResProvB");
    pBCryptUnregisterProvider(L"SgResProvC");
    pBCryptUnregisterProvider(L"SgPersistProvA");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, CFG L"\\SgHand");
}

/* ---------------- the other process ---------------- */

static int child_check(void)
{
    cfg *q = NULL;
    ULONG size = 0;
    strlist *l;
    UCHAR *val = NULL;
    r_refs *refs = NULL;

    checkst(pBCryptQueryContextConfiguration(T_LOCAL, L"SgPersist", &size, &q), S_OK_, "child: the context is there");
    check(q && q->flags == 0x1234, "child: its flags are the parent's");
    if (q) pBCryptFreeBuffer(q);
    l = funcs(T_LOCAL, L"SgPersist", I_HASH);
    check(list_is(l, w_sgb_sga), "child: function order is the parent's");
    if (l) pBCryptFreeBuffer(l);
    l = provs(L"SgPersist", I_HASH, L"SGA");
    check(list_is(l, w_pa), "child: provider list is the parent's");
    if (l) pBCryptFreeBuffer(l);
    size = 0;
    checkst(pBCryptQueryContextFunctionProperty(T_LOCAL, L"SgPersist", I_HASH, L"SGA", L"Weight", &size, &val), S_OK_,
            "child: the property is there");
    check(val && size == 4 && *(DWORD *)val == 77, "child: with the parent's value");
    if (val) pBCryptFreeBuffer(val);
    size = 0;
    checkst(pBCryptResolveProviders(L"SgPersist", I_HASH, L"SGA", NULL, M_ANY, 0, &size, (void *)&refs), S_OK_,
            "child: resolve sees the parent's provider");
    check(refs && refs->n == 1, "child: one provider");
    if (refs) pBCryptFreeBuffer(refs);
    return failures;
}

static int child_mutate(void)
{
    checkst(pBCryptCreateContext(T_LOCAL, L"SgNotifyChild", NULL), S_OK_, "child: create a context");
    return failures;
}

static int run_child(const char *mode)
{
    char path[MAX_PATH], cmd[MAX_PATH + 32];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 99;

    GetModuleFileNameA(NULL, path, sizeof(path));
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, mode);
    fflush(stdout);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return 98;
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

/* ---------------- the store ---------------- */

static void test_store(void)
{
    cfg c = { 0x1234, 0 };
    BYTE prop[4] = { 77, 0, 0, 0 };
    strlist *l;
    ULONG size = 0;
    static const WCHAR *hand_funcs[] = { L"HF2", L"HF1", NULL };
    static const WCHAR *hand_provs[] = { L"SgResProvZ", NULL };
    HKEY k, k2;
    DWORD v;

    checkst(pBCryptCreateContext(T_LOCAL, L"SgPersist", &c), S_OK_, "CreateContext");
    check(key_exists(CFG L"\\SgPersist"), "the context is a key under Configuration\\Local");
    check(dword_is(CFG L"\\SgPersist", L"Flags", 0x1234), "its Flags value is stored");
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgPersist", I_HASH, L"SGA", BOTTOM), S_OK_, "AddContextFunction SGA");
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgPersist", I_HASH, L"SGB", 0), S_OK_, "AddContextFunction SGB on top");
    check(multi_is(CFG L"\\SgPersist\\00000002", L"Functions", w_sgb_sga), "the interface key lists the functions in order");
    checkst(pBCryptAddContextFunctionProvider(T_LOCAL, L"SgPersist", I_HASH, L"SGA", L"SgPersistProvA", BOTTOM), S_OK_,
            "AddContextFunctionProvider");
    check(multi_is(CFG L"\\SgPersist\\00000002\\SGA", L"Providers", w_pa), "the function key lists its providers");
    checkst(pBCryptSetContextFunctionProperty(T_LOCAL, L"SgPersist", I_HASH, L"SGA", L"Weight", 4, prop), S_OK_,
            "SetContextFunctionProperty");
    check(key_exists(CFG L"\\SgPersist\\00000002\\SGA\\Properties"), "the property is stored");
    checkst(pBCryptRemoveContextFunction(T_LOCAL, L"SgPersist", I_HASH, L"SGB"), S_OK_, "RemoveContextFunction SGB");
    check(!key_exists(CFG L"\\SgPersist\\00000002\\SGB"), "the removed function's key is gone");
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgPersist", I_HASH, L"SGB", 0), S_OK_, "SGB again, on top");

    {
        p_iface hi = { I_HASH, 0, 1, (WCHAR *[]){ L"SGA" } }, *ifl[] = { &hi };
        p_image um = { L"sgpersist.dll", 1, ifl };
        p_reg reg = { 1, (WCHAR *[]){ L"SgAlias" }, &um, NULL };
        checkst(pBCryptRegisterProvider(L"SgPersistProvA", 0, &reg), S_OK_, "RegisterProvider");
    }
    check(key_exists(PROVS L"\\SgPersistProvA\\UM") && !key_exists(PROVS L"\\SgPersistProvA\\KM"),
          "the provider has a UM key and no KM key");
    check(multi_is(PROVS L"\\SgPersistProvA\\UM\\00000002", L"Functions", w_sga), "its interface key lists the functions");

    check(run_child("child-check") == 0, "a second process sees the same store");

    /* removed behind our back through the registry: the next call sees it */
    check(!RegCreateKeyExW(HKEY_LOCAL_MACHINE, CFG L"\\SgHand\\00000002", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL),
          "make an entry by hand");
    RegSetValueExW(k, L"Functions", 0, REG_MULTI_SZ, (const BYTE *)L"HF2\0HF1\0", 9 * sizeof(WCHAR));
    RegCreateKeyExW(k, L"HF1", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k2, NULL);
    RegSetValueExW(k2, L"Providers", 0, REG_MULTI_SZ, (const BYTE *)L"SgResProvZ\0", 12 * sizeof(WCHAR));
    v = 5;
    RegSetValueExW(k2, L"Flags", 0, REG_DWORD, (const BYTE *)&v, 4);
    RegCloseKey(k2);
    RegCloseKey(k);
    size = 0; l = NULL;
    pBCryptEnumContexts(T_LOCAL, &size, &l);
    check(list_has(l, L"SgHand"), "a context made in the registry is listed");
    if (l) pBCryptFreeBuffer(l);
    l = funcs(T_LOCAL, L"SgHand", I_HASH);
    check(list_is(l, hand_funcs), "its function order comes from the registry value");
    if (l) pBCryptFreeBuffer(l);
    l = provs(L"SgHand", I_HASH, L"HF1");
    check(list_is(l, hand_provs), "its provider list comes from the registry");
    if (l) pBCryptFreeBuffer(l);
    {
        cfg *q = NULL;
        size = 0;
        pBCryptQueryContextFunctionConfiguration(T_LOCAL, L"SgHand", I_HASH, L"HF1", &size, &q);
        check(q && q->flags == 5, "and its Flags");
        if (q) pBCryptFreeBuffer(q);
    }
    checkst(pBCryptDeleteContext(T_LOCAL, L"SgHand"), S_OK_, "DeleteContext of a hand-made entry");
    check(!key_exists(CFG L"\\SgHand"), "its key is gone");

    checkst(pBCryptUnregisterProvider(L"SgPersistProvA"), S_OK_, "UnregisterProvider");
    check(!key_exists(PROVS L"\\SgPersistProvA"), "the provider key is gone");
    checkst(pBCryptCreateContext(T_LOCAL, L"Bad\\Name", NULL), S_INVALID, "a name with a backslash is rejected");
}

/* ---------------- resolving ---------------- */

static r_refs *resolve(const WCHAR *ctx, ULONG iface, const WCHAR *fn, const WCHAR *prov, ULONG mode, ULONG flags, NTS *st)
{
    ULONG size = 0;
    r_refs *refs = NULL;
    *st = pBCryptResolveProviders(ctx, iface, fn, prov, mode, flags, &size, (void *)&refs);
    return *st ? NULL : refs;
}

static int refs_are(const r_refs *r, const WCHAR **provs_)
{
    ULONG i;
    if (!r) return 0;
    for (i = 0; provs_[i]; i++)
        if (i >= r->n || lstrcmpiW(r->refs[i]->prov, provs_[i])) return 0;
    return i == r->n;
}

static void reg_prov(const WCHAR *name, int um, int km, const WCHAR **hash_funcs, ULONG nhash)
{
    p_iface hi = { I_HASH, 7, nhash, (WCHAR **)hash_funcs }, *ifl[] = { &hi };
    p_image uim = { L"sgres_um.dll", 1, ifl }, kim = { L"sgres_km.sys", 1, ifl };
    p_reg reg = { 0, NULL, um ? &uim : NULL, km ? &kim : NULL };
    NTS st = pBCryptRegisterProvider(name, 0, &reg);
    check(st == S_OK_, "register a test provider");
}

static void test_resolve(void)
{
    static const WCHAR *fa[] = { L"SGRF", L"SGUNCONF" };
    static const WCHAR *fb[] = { L"SGRF", L"SGRG" };
    static const WCHAR *fc[] = { L"OTHER" };
    static const WCHAR *r_ba[] = { L"SgResProvB", L"SgResProvA", NULL };
    static const WCHAR *r_a[] = { L"SgResProvA", NULL };
    static const WCHAR *r_ab[] = { L"SgResProvA", L"SgResProvB", NULL };
    static const WCHAR *r_prim[] = { L"Microsoft Primitive Provider", NULL };
    static const WCHAR *r_all[] = { L"SgResProvB", L"SgResProvA", L"SgResProvB", NULL };
    BYTE pv[3] = { 1, 2, 3 };
    r_refs *r;
    NTS st;
    cfg dc = { 0, 0 };

    reg_prov(L"SgResProvA", 1, 1, fa, 2);
    reg_prov(L"SgResProvB", 1, 0, fb, 2);
    reg_prov(L"SgResProvC", 1, 0, fc, 1);
    pBCryptCreateContext(T_LOCAL, L"SgResolve", NULL);
    pBCryptAddContextFunction(T_LOCAL, L"SgResolve", I_HASH, L"SGRF", BOTTOM);
    pBCryptAddContextFunction(T_LOCAL, L"SgResolve", I_HASH, L"SGRG", BOTTOM);
    pBCryptAddContextFunctionProvider(T_LOCAL, L"SgResolve", I_HASH, L"SGRF", L"SgResProvB", BOTTOM);
    pBCryptAddContextFunctionProvider(T_LOCAL, L"SgResolve", I_HASH, L"SGRF", L"SgResProvA", BOTTOM);
    pBCryptAddContextFunctionProvider(T_LOCAL, L"SgResolve", I_HASH, L"SGRF", L"SgResProvNone", BOTTOM);
    pBCryptAddContextFunctionProvider(T_LOCAL, L"SgResolve", I_HASH, L"SGRF", L"SgResProvC", BOTTOM);
    pBCryptAddContextFunctionProvider(T_LOCAL, L"SgResolve", I_HASH, L"SGRG", L"SgResProvB", BOTTOM);
    pBCryptSetContextFunctionProperty(T_LOCAL, L"SgResolve", I_HASH, L"SGRF", L"Prop1", 3, pv);

    /* the built-in context and provider */
    r = resolve(NULL, I_HASH, L"SHA256", NULL, M_ANY, 0, &st);
    checkst(st, S_OK_, "resolve SHA256 in the default context");
    check(refs_are(r, r_prim) && r->refs[0]->um && !r->refs[0]->km && r->refs[0]->um->image &&
          !lstrcmpiW(r->refs[0]->um->image, L"bcryptprimitives.dll") && r->refs[0]->iface == I_HASH &&
          !lstrcmpiW(r->refs[0]->func, L"SHA256") && !r->refs[0]->nprops, "the built-in provider, user mode image only");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"Default", I_HASH, L"SHA256", NULL, M_UM, 0, &st);
    checkst(st, S_OK_, "the Default context by name");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(NULL, I_HASH, L"SHA256", NULL, M_KM, 0, &st);
    checkst(st, S_NOTFOUND, "no kernel mode image for it");
    r = resolve(NULL, I_HASH, L"SHA256", NULL, M_MM, 0, &st);
    checkst(st, S_NOTFOUND, "and not for both modes either");

    /* the configured list: order, registered only, mode filter, properties */
    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, M_ANY, 0, &st);
    checkst(st, S_OK_, "resolve SGRF");
    check(refs_are(r, r_ba), "context order, unregistered and non-offering providers left out");
    check(r && r->refs[0]->um && !r->refs[0]->km && r->refs[1]->um && r->refs[1]->km, "images follow what each provider has");
    check(r && r->refs[1]->um->flags == 7 && !lstrcmpiW(r->refs[1]->km->image, L"sgres_km.sys"), "image flags and names");
    check(r && r->refs[0]->nprops == 1 && !lstrcmpiW(r->refs[0]->props[0]->name, L"Prop1") &&
          r->refs[0]->props[0]->size == 3 && !memcmp(r->refs[0]->props[0]->value, pv, 3),
          "the function's properties ride along");
    check(r && (BYTE *)r->refs[0]->func > (BYTE *)r && (BYTE *)r->refs[1]->km->image > (BYTE *)r, "all in one block");
    if (r) pBCryptFreeBuffer(r);

    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, M_UM, 0, &st);
    check(refs_are(r, r_ba) && r->refs[1]->um && !r->refs[1]->km, "user mode asked: no kernel image reported");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, M_KM, 0, &st);
    check(refs_are(r, r_a) && !r->refs[0]->um && r->refs[0]->km, "kernel mode asked: only the provider that has one");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, M_MM, 0, &st);
    check(refs_are(r, r_a) && r->refs[0]->um && r->refs[0]->km, "both modes asked: only the provider that has both");
    if (r) pBCryptFreeBuffer(r);

    r = resolve(L"SgResolve", I_HASH, L"SGRF", L"SgResProvA", M_ANY, 0, &st);
    check(refs_are(r, r_a), "a provider name narrows the result");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"SgResolve", I_HASH, L"SGRF", L"SgResProvC", M_ANY, 0, &st);
    checkst(st, S_NOTFOUND, "a provider that does not offer the function");

    /* all providers / all functions */
    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, M_ANY, F_ALLPROV, &st);
    check(refs_are(r, r_ab), "ALL_PROVIDERS: every registered provider offering it, not the configured list");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"SgResolve", I_HASH, L"SGUNCONF", NULL, M_ANY, F_ALLPROV, &st);
    check(refs_are(r, r_a), "ALL_PROVIDERS also works for a function the context does not list");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"SgResolve", I_HASH, L"SGUNCONF", NULL, M_ANY, 0, &st);
    checkst(st, S_NOTFOUND, "without it that function is not found");
    r = resolve(L"SgResolve", I_HASH, NULL, NULL, M_ANY, 0, &st);
    check(refs_are(r, r_all) && !lstrcmpiW(r->refs[0]->func, L"SGRF") && !lstrcmpiW(r->refs[2]->func, L"SGRG"),
          "a NULL function walks every function of the interface");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"SgResolve", I_HASH, L"SGRG", NULL, M_ANY, F_ALLFUNC, &st);
    check(refs_are(r, r_all), "CRYPT_ALL_FUNCTIONS ignores the function name");
    if (r) pBCryptFreeBuffer(r);
    r = resolve(L"SgResolve", 0, NULL, NULL, M_ANY, 0, &st);
    check(st == S_OK_ && r && r->n == 3, "interface 0 covers every interface");
    if (r) pBCryptFreeBuffer(r);

    /* the domain table */
    pBCryptCreateContext(T_DOMAIN, L"SgDomRes", &dc);
    pBCryptAddContextFunction(T_DOMAIN, L"SgDomRes", I_HASH, L"SGRF", BOTTOM);
    pBCryptAddContextFunctionProvider(T_DOMAIN, L"SgDomRes", I_HASH, L"SGRF", L"SgResProvA", BOTTOM);
    r = resolve(L"SgDomRes", I_HASH, L"SGRF", NULL, M_ANY, 0, &st);
    check(st == S_OK_ && refs_are(r, r_a), "a context in the domain table resolves");
    if (r) pBCryptFreeBuffer(r);

    /* errors and the buffer convention */
    r = resolve(L"NoSuchCtx", I_HASH, L"SGRF", NULL, M_ANY, 0, &st);
    checkst(st, S_NOTFOUND, "unknown context");
    r = resolve(L"SgResolve", I_HASH, L"NoSuchFunc", NULL, M_ANY, 0, &st);
    checkst(st, S_NOTFOUND, "unknown function");
    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, 0, 0, &st);
    checkst(st, S_INVALID, "mode 0");
    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, 5, 0, &st);
    checkst(st, S_INVALID, "mode 5");
    r = resolve(L"SgResolve", I_HASH, L"SGRF", NULL, M_ANY, 4, &st);
    checkst(st, S_INVALID, "unknown flag");
    r = resolve(L"SgResolve", 9, L"SGRF", NULL, M_ANY, 0, &st);
    checkst(st, S_INVALID, "unknown interface");
    checkst(pBCryptResolveProviders(L"SgResolve", I_HASH, L"SGRF", NULL, M_ANY, 0, NULL, (void *)&r), S_INVALID, "no size");
    checkst(pBCryptResolveProviders(L"SgResolve", I_HASH, L"SGRF", NULL, M_ANY, 0, &(ULONG){0}, NULL), S_INVALID, "no buffer");
    {
        BYTE mine[4096];
        r_refs *p = (r_refs *)mine;
        ULONG size = 8;
        st = pBCryptResolveProviders(L"SgResolve", I_HASH, L"SGRF", NULL, M_ANY, 0, &size, (void *)&p);
        check(st == S_SMALL && size > 8, "a small caller buffer reports the size needed");
        size = sizeof(mine);
        p = (r_refs *)mine;
        st = pBCryptResolveProviders(L"SgResolve", I_HASH, L"SGRF", NULL, M_ANY, 0, &size, (void *)&p);
        check(st == S_OK_ && p == (r_refs *)mine && p->n == 2 && size <= sizeof(mine), "into a caller buffer");
    }
}

/* ---------------- notification ---------------- */

static void test_notify(void)
{
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), ev2 = NULL, junk = (HANDLE)(ULONG_PTR)0x7ff0;
    NTS st;

    checkst(pBCryptRegisterConfigChangeNotify(NULL), S_INVALID, "Register needs a place for the event");
    checkst(pBCryptUnregisterConfigChangeNotify(NULL), S_INVALID, "Unregister needs an event");
    checkst(pBCryptUnregisterConfigChangeNotify(ev), S_NOTFOUND, "Unregister of an event never registered");
    checkst(pBCryptRegisterConfigChangeNotify(&ev), S_OK_, "Register an event");
    check(WaitForSingleObject(ev, 300) == WAIT_TIMEOUT, "nothing changed, not signalled");
    pBCryptCreateContext(T_LOCAL, L"SgNotify", NULL);
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "a change in this process signals it");
    ResetEvent(ev);
    check(run_child("child-mutate") == 0, "another process changes the store");
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "a change in another process signals it");
    pBCryptDeleteContext(T_LOCAL, L"SgNotify");
    pBCryptDeleteContext(T_LOCAL, L"SgNotifyChild");
    Sleep(500);
    ResetEvent(ev);
    st = pBCryptRegisterConfigChangeNotify(&ev2);
    checkst(st, S_OK_, "Register without an event of our own");
    check(ev2 != NULL && ev2 != ev, "an event is handed back");
    checkst(pBCryptUnregisterConfigChangeNotify(ev), S_OK_, "Unregister the first");
    pBCryptCreateContext(T_LOCAL, L"SgNotify", NULL);
    check(ev2 && WaitForSingleObject(ev2, 10000) == WAIT_OBJECT_0, "the other registration is still served");
    check(WaitForSingleObject(ev, 1000) == WAIT_TIMEOUT, "the unregistered event is not signalled again");
    checkst(pBCryptUnregisterConfigChangeNotify(ev), S_NOTFOUND, "Unregister twice");
    checkst(pBCryptUnregisterConfigChangeNotify(ev2), S_OK_, "Unregister the second");
    ResetEvent(ev2);
    pBCryptDeleteContext(T_LOCAL, L"SgNotify");
    check(WaitForSingleObject(ev2, 1000) == WAIT_TIMEOUT, "nothing is signalled with nobody registered");
    /* and it can be started again */
    ResetEvent(ev);
    checkst(pBCryptRegisterConfigChangeNotify(&ev), S_OK_, "Register again");
    pBCryptCreateContext(T_LOCAL, L"SgNotify", NULL);
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "and it signals again");
    pBCryptUnregisterConfigChangeNotify(ev);
    (void)junk;
}

int main(int argc, char **argv)
{
    bc = LoadLibraryA("bcrypt.dll");
    if (!bc) { printf("FAIL  no bcrypt.dll\n"); return 1; }
#define L_(name) do { *(FARPROC *)&p##name = GetProcAddress(bc, #name); \
    if (!p##name) { printf("FAIL  %s is not exported\n", #name); return 1; } } while (0)
    L_(BCryptCreateContext); L_(BCryptDeleteContext); L_(BCryptEnumContexts); L_(BCryptAddContextFunction);
    L_(BCryptRemoveContextFunction); L_(BCryptEnumContextFunctions); L_(BCryptConfigureContext);
    L_(BCryptConfigureContextFunction); L_(BCryptQueryContextConfiguration); L_(BCryptQueryContextFunctionConfiguration);
    L_(BCryptAddContextFunctionProvider); L_(BCryptEnumContextFunctionProviders);
    L_(BCryptSetContextFunctionProperty); L_(BCryptQueryContextFunctionProperty);
    L_(BCryptRegisterProvider); L_(BCryptUnregisterProvider); L_(BCryptQueryProviderRegistration);
    L_(BCryptResolveProviders); L_(BCryptRegisterConfigChangeNotify); L_(BCryptUnregisterConfigChangeNotify);
    L_(BCryptFreeBuffer);

    if (argc > 1)
    {
        if (!strcmp(argv[1], "child-check")) return child_check();
        if (!strcmp(argv[1], "child-mutate")) return child_mutate();
        return 2;
    }
    cleanup();
    test_store();
    test_resolve();
    test_notify();
    cleanup();

    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
