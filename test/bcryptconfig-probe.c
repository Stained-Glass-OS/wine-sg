/* The CNG configuration API (patches/sg/2402), run by test/bcryptconfig-gate.sh:
 * contexts, per-interface function lists with priorities, per-function
 * provider lists, function configuration and properties, and provider
 * registration. These used to be stubs that answered success or
 * STATUS_NOT_IMPLEMENTED and returned nothing. Everything is reached through
 * GetProcAddress so the probe builds with any mingw header level. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef LONG NTS;
#define S_OK_      ((NTS)0)
#define S_INVALID  ((NTS)0xC000000D)
#define S_SMALL    ((NTS)0xC0000023)
#define S_COLLIDE  ((NTS)0xC0000035)
#define S_NOTFOUND ((NTS)0xC0000225)

#define T_LOCAL  1
#define T_DOMAIN 2
#define I_CIPHER 1
#define I_HASH   2
#define I_ASYM   3
#define I_SIGN   5
#define I_RNG    6
#define I_KDF    7
#define BOTTOM   0xFFFFFFFF
#define M_UM 1
#define M_KM 2
#define M_MM 3
#define M_ANY 4

typedef struct { ULONG count; WCHAR **list; } strlist;
typedef struct { ULONG flags, reserved; } cfg;
typedef struct { ULONG iface, flags, nfunc; WCHAR **funcs; } p_iface;
typedef struct { WCHAR *image; ULONG nif; p_iface **ifs; } p_image;
typedef struct { ULONG nalias; WCHAR **aliases; p_image *um, *km; } p_reg;
typedef struct { WCHAR *name; } p_name;

static HMODULE bc;
#define F(name) static NTS (WINAPI *p##name)()
F(BCryptCreateContext); F(BCryptDeleteContext); F(BCryptConfigureContext); F(BCryptQueryContextConfiguration);
F(BCryptEnumContexts); F(BCryptAddContextFunction); F(BCryptRemoveContextFunction); F(BCryptEnumContextFunctions);
F(BCryptConfigureContextFunction); F(BCryptQueryContextFunctionConfiguration);
F(BCryptAddContextFunctionProvider); F(BCryptRemoveContextFunctionProvider); F(BCryptEnumContextFunctionProviders);
F(BCryptSetContextFunctionProperty); F(BCryptQueryContextFunctionProperty);
F(BCryptRegisterProvider); F(BCryptUnregisterProvider); F(BCryptQueryProviderRegistration);
F(BCryptEnumRegisteredProviders); F(BCryptEnumProviders);
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
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (got %08lx, want %08lx)", what, (unsigned long)got, (unsigned long)want);
    check(got == want, buf);
}

/* does the list hold exactly these names, in this order (NULL-terminated) */
static int list_is(const strlist *l, const WCHAR **want)
{
    ULONG i;
    for (i = 0; want[i]; i++)
        if (i >= l->count || lstrcmpiW(l->list[i], want[i])) return 0;
    return i == l->count;
}
static int list_has(const strlist *l, const WCHAR *name)
{
    ULONG i;
    for (i = 0; i < l->count; i++) if (!lstrcmpiW(l->list[i], name)) return 1;
    return 0;
}

static NTS funcs(ULONG table, const WCHAR *ctx, ULONG iface, strlist **out)
{
    ULONG size = 0;
    *out = NULL;
    return pBCryptEnumContextFunctions(table, ctx, iface, &size, out);
}
static NTS provs(const WCHAR *ctx, ULONG iface, const WCHAR *fn, strlist **out)
{
    ULONG size = 0;
    *out = NULL;
    return pBCryptEnumContextFunctionProviders(T_LOCAL, ctx, iface, fn, &size, out);
}

static const WCHAR *ord_cab[]  = { L"C", L"A", L"B", NULL };
static const WCHAR *ord_ca[]   = { L"C", L"A", NULL };
static const WCHAR *ord_a[]    = { L"A", NULL };
static const WCHAR *ord_none[] = { NULL };
static const WCHAR *ord_p[]    = { L"P0", L"P1", L"P2", NULL };
static const WCHAR *ord_p02[]  = { L"P0", L"P2", NULL };

static void test_contexts(void)
{
    static const struct { const char *what; ULONG table; const WCHAR *name; NTS want; } bad[] =
    {
        { "table 0", 0, L"X", S_INVALID }, { "table 3", 3, L"X", S_INVALID },
        { "NULL name", T_LOCAL, NULL, S_INVALID }, { "empty name", T_LOCAL, L"", S_INVALID },
    };
    cfg c = { 0x10000, 0 }, *q;
    strlist *ctxs;
    ULONG size, i;
    NTS st;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        char what[80];
        snprintf(what, sizeof(what), "CreateContext rejects %s", bad[i].what);
        checkst(pBCryptCreateContext(bad[i].table, bad[i].name, NULL), bad[i].want, what);
    }

    size = 0; ctxs = NULL;
    checkst(pBCryptEnumContexts(T_LOCAL, &size, &ctxs), S_OK_, "EnumContexts local");
    check(ctxs && size >= sizeof(*ctxs) && list_has(ctxs, L"Default"), "the Default context exists");
    check(ctxs && ctxs->count && ctxs->list[0] > (WCHAR *)ctxs && (BYTE *)ctxs->list[0] < (BYTE *)ctxs + size,
          "enumerated names live inside the one block");
    if (ctxs) pBCryptFreeBuffer(ctxs);

    checkst(pBCryptCreateContext(T_LOCAL, L"SgProbe", &c), S_OK_, "CreateContext");
    checkst(pBCryptCreateContext(T_LOCAL, L"sgprobe", NULL), S_COLLIDE, "CreateContext again (name is caseless)");
    size = 0; ctxs = NULL;
    pBCryptEnumContexts(T_LOCAL, &size, &ctxs);
    check(ctxs && list_has(ctxs, L"SgProbe"), "the new context is listed");
    if (ctxs) pBCryptFreeBuffer(ctxs);
    size = 0; ctxs = NULL;
    pBCryptEnumContexts(T_DOMAIN, &size, &ctxs);
    check(ctxs && !list_has(ctxs, L"SgProbe"), "the domain table does not see a local context");
    if (ctxs) pBCryptFreeBuffer(ctxs);

    size = 0; q = NULL;
    checkst(pBCryptQueryContextConfiguration(T_LOCAL, L"SgProbe", &size, &q), S_OK_, "QueryContextConfiguration");
    check(q && size == sizeof(*q) && q->flags == 0x10000, "the creation flags come back");
    if (q) pBCryptFreeBuffer(q);
    c.flags = 1;
    checkst(pBCryptConfigureContext(T_LOCAL, L"SgProbe", &c), S_OK_, "ConfigureContext");
    size = 0; q = NULL;
    pBCryptQueryContextConfiguration(T_LOCAL, L"SgProbe", &size, &q);
    check(q && q->flags == 1, "the new flags come back");
    if (q) pBCryptFreeBuffer(q);
    {
        cfg mine[4]; cfg *p = mine;
        size = 1;
        st = pBCryptQueryContextConfiguration(T_LOCAL, L"SgProbe", &size, &p);
        check(st == S_OK_ || st == S_SMALL, "caller buffer path answers");
        size = sizeof(mine); p = mine;
        checkst(pBCryptQueryContextConfiguration(T_LOCAL, L"SgProbe", &size, &p), S_OK_, "query into a caller buffer");
        check(p == mine && size == sizeof(cfg) && mine[0].flags == 1, "the caller buffer is filled, size reported");
        size = 2; p = mine;
        st = pBCryptQueryContextConfiguration(T_LOCAL, L"SgProbe", &size, &p);
        check(st == S_SMALL && size == sizeof(cfg), "too small a caller buffer reports the size needed");
    }
    checkst(pBCryptConfigureContext(T_LOCAL, L"NoSuchCtx", &c), S_NOTFOUND, "ConfigureContext on a missing context");
    checkst(pBCryptConfigureContext(T_LOCAL, L"SgProbe", NULL), S_INVALID, "ConfigureContext with no config");
    checkst(pBCryptQueryContextConfiguration(T_LOCAL, L"NoSuchCtx", &size, &q), S_NOTFOUND, "query a missing context");

    checkst(pBCryptDeleteContext(T_LOCAL, L"SgProbe"), S_OK_, "DeleteContext");
    checkst(pBCryptDeleteContext(T_LOCAL, L"SgProbe"), S_NOTFOUND, "DeleteContext again");
    checkst(pBCryptCreateContext(T_LOCAL, L"SgProbe", NULL), S_OK_, "the name can be created again");
    pBCryptDeleteContext(T_LOCAL, L"SgProbe");
}

static void test_functions(void)
{
    static const struct { const char *what; ULONG table; const WCHAR *ctx; ULONG iface; const WCHAR *fn; } bad[] =
    {
        { "iface 0", T_LOCAL, L"Default", 0, L"A" }, { "iface 8", T_LOCAL, L"Default", 8, L"A" },
        { "table 0", 0, L"Default", I_HASH, L"A" }, { "NULL function", T_LOCAL, L"Default", I_HASH, NULL },
    };
    strlist *l;
    ULONG i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        char what[80];
        snprintf(what, sizeof(what), "AddContextFunction rejects %s", bad[i].what);
        checkst(pBCryptAddContextFunction(bad[i].table, bad[i].ctx, bad[i].iface, bad[i].fn, BOTTOM), S_INVALID, what);
    }
    checkst(pBCryptAddContextFunction(T_LOCAL, L"NoSuchCtx", I_HASH, L"A", BOTTOM), S_NOTFOUND,
            "AddContextFunction to a missing context");

    /* the built-in Default context lists what this bcrypt implements */
    checkst(funcs(T_LOCAL, L"Default", I_HASH, &l), S_OK_, "EnumContextFunctions Default/hash");
    check(l && list_has(l, L"SHA256") && list_has(l, L"MD5") && list_has(l, L"SHA512"), "hash functions are listed");
    if (l) pBCryptFreeBuffer(l);
    checkst(funcs(T_LOCAL, L"Default", I_CIPHER, &l), S_OK_, "EnumContextFunctions Default/cipher");
    check(l && list_has(l, L"AES"), "AES is a cipher function");
    check(l && !list_has(l, L"SHA256"), "and the hash functions are not in that list");
    if (l) pBCryptFreeBuffer(l);
    checkst(funcs(T_LOCAL, L"Default", I_RNG, &l), S_OK_, "EnumContextFunctions Default/rng");
    check(l && list_has(l, L"RNG"), "RNG is listed");
    if (l) pBCryptFreeBuffer(l);
    checkst(funcs(T_LOCAL, L"Default", I_KDF, &l), S_OK_, "an interface with nothing registered lists empty");
    check(l && l->count == 0, "and says so");
    if (l) pBCryptFreeBuffer(l);
    checkst(funcs(T_LOCAL, L"NoSuchCtx", I_HASH, &l), S_NOTFOUND, "enum functions of a missing context");

    pBCryptCreateContext(T_LOCAL, L"SgFn", NULL);
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgFn", I_HASH, L"A", BOTTOM), S_OK_, "add A at the bottom");
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgFn", I_HASH, L"B", BOTTOM), S_OK_, "add B at the bottom");
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgFn", I_HASH, L"C", 0), S_OK_, "add C at the top");
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgFn", I_HASH, L"a", BOTTOM), S_COLLIDE, "add A again (caseless)");
    checkst(pBCryptAddContextFunction(T_LOCAL, L"SgFn", I_CIPHER, L"A", BOTTOM), S_OK_, "the same name on another interface");
    checkst(funcs(T_LOCAL, L"SgFn", I_HASH, &l), S_OK_, "enum hash functions");
    check(l && list_is(l, ord_cab), "the order is C, A, B (priority, not name)");
    if (l) pBCryptFreeBuffer(l);
    checkst(funcs(T_LOCAL, L"SgFn", I_CIPHER, &l), S_OK_, "enum cipher functions");
    check(l && list_is(l, ord_a), "interfaces keep separate lists");
    if (l) pBCryptFreeBuffer(l);
    checkst(pBCryptRemoveContextFunction(T_LOCAL, L"SgFn", I_HASH, L"B"), S_OK_, "remove B");
    checkst(pBCryptRemoveContextFunction(T_LOCAL, L"SgFn", I_HASH, L"B"), S_NOTFOUND, "remove B again");
    checkst(pBCryptRemoveContextFunction(T_LOCAL, L"NoSuchCtx", I_HASH, L"B"), S_NOTFOUND, "remove from a missing context");
    funcs(T_LOCAL, L"SgFn", I_HASH, &l);
    check(l && list_is(l, ord_ca), "C, A remain");
    if (l) pBCryptFreeBuffer(l);
    {
        BYTE small[8]; strlist *p = (strlist *)small; ULONG size = sizeof(small);
        NTS st = pBCryptEnumContextFunctions(T_LOCAL, L"SgFn", I_HASH, &size, &p);
        check(st == S_SMALL && size > sizeof(small), "a caller buffer that is too small reports the size needed");
    }
    pBCryptRemoveContextFunction(T_LOCAL, L"SgFn", I_HASH, L"A");
    pBCryptRemoveContextFunction(T_LOCAL, L"SgFn", I_HASH, L"C");
    funcs(T_LOCAL, L"SgFn", I_HASH, &l);
    check(l && list_is(l, ord_none), "all removed leaves an empty list");
    if (l) pBCryptFreeBuffer(l);
    pBCryptDeleteContext(T_LOCAL, L"SgFn");
}

static void test_function_providers_and_properties(void)
{
    cfg fc = { 5, 0 }, *q;
    strlist *l;
    BYTE v[4] = { 1, 2, 3, 4 }, *got;
    ULONG size;
    NTS st;

    pBCryptCreateContext(T_LOCAL, L"SgProv", NULL);
    pBCryptAddContextFunction(T_LOCAL, L"SgProv", I_HASH, L"F", BOTTOM);

    checkst(pBCryptAddContextFunctionProvider(T_LOCAL, L"SgProv", I_HASH, L"F", L"P1", BOTTOM), S_OK_, "add provider P1");
    checkst(pBCryptAddContextFunctionProvider(T_LOCAL, L"SgProv", I_HASH, L"F", L"P2", BOTTOM), S_OK_, "add provider P2");
    checkst(pBCryptAddContextFunctionProvider(T_LOCAL, L"SgProv", I_HASH, L"F", L"P0", 0), S_OK_, "add provider P0 first");
    checkst(pBCryptAddContextFunctionProvider(T_LOCAL, L"SgProv", I_HASH, L"F", L"p1", BOTTOM), S_COLLIDE, "add P1 again");
    checkst(pBCryptAddContextFunctionProvider(T_LOCAL, L"SgProv", I_HASH, L"NoFn", L"P1", BOTTOM), S_NOTFOUND,
            "add a provider to a missing function");
    checkst(provs(L"SgProv", I_HASH, L"F", &l), S_OK_, "EnumContextFunctionProviders");
    check(l && list_is(l, ord_p), "providers come back in priority order");
    if (l) pBCryptFreeBuffer(l);
    checkst(pBCryptRemoveContextFunctionProvider(T_LOCAL, L"SgProv", I_HASH, L"F", L"P1"), S_OK_, "remove P1");
    checkst(pBCryptRemoveContextFunctionProvider(T_LOCAL, L"SgProv", I_HASH, L"F", L"P1"), S_NOTFOUND, "remove P1 again");
    provs(L"SgProv", I_HASH, L"F", &l);
    check(l && list_is(l, ord_p02), "P0, P2 remain");
    if (l) pBCryptFreeBuffer(l);
    checkst(provs(L"SgProv", I_HASH, L"NoFn", &l), S_NOTFOUND, "providers of a missing function");

    /* the built-in functions name the built-in provider */
    checkst(provs(L"Default", I_HASH, L"SHA256", &l), S_OK_, "providers of Default/SHA256");
    check(l && l->count == 1 && !lstrcmpiW(l->list[0], L"Microsoft Primitive Provider"), "the primitive provider");
    if (l) pBCryptFreeBuffer(l);

    size = 0; q = NULL;
    checkst(pBCryptQueryContextFunctionConfiguration(T_LOCAL, L"SgProv", I_HASH, L"F", &size, &q), S_OK_,
            "QueryContextFunctionConfiguration");
    check(q && q->flags == 0, "no flags until configured");
    if (q) pBCryptFreeBuffer(q);
    checkst(pBCryptConfigureContextFunction(T_LOCAL, L"SgProv", I_HASH, L"F", &fc), S_OK_, "ConfigureContextFunction");
    size = 0; q = NULL;
    pBCryptQueryContextFunctionConfiguration(T_LOCAL, L"SgProv", I_HASH, L"F", &size, &q);
    check(q && q->flags == 5, "the function flags come back");
    if (q) pBCryptFreeBuffer(q);
    checkst(pBCryptConfigureContextFunction(T_LOCAL, L"SgProv", I_HASH, L"NoFn", &fc), S_NOTFOUND,
            "configure a missing function");

    size = 0; got = NULL;
    checkst(pBCryptQueryContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", L"Image", &size, &got), S_NOTFOUND,
            "a property that was never set");
    checkst(pBCryptSetContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", L"Image", sizeof(v), v), S_OK_,
            "SetContextFunctionProperty");
    size = 0; got = NULL;
    checkst(pBCryptQueryContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", L"image", &size, &got), S_OK_,
            "QueryContextFunctionProperty (caseless name)");
    check(got && size == 4 && !memcmp(got, v, 4), "the property value and size come back");
    if (got) pBCryptFreeBuffer(got);
    v[0] = 9;
    pBCryptSetContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", L"Image", 2, v);
    size = 0; got = NULL;
    pBCryptQueryContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", L"Image", &size, &got);
    check(got && size == 2 && got[0] == 9, "setting again replaces the value");
    if (got) pBCryptFreeBuffer(got);
    {
        BYTE buf[1]; BYTE *p = buf;
        size = 1;
        st = pBCryptQueryContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", L"Image", &size, &p);
        check(st == S_SMALL && size == 2, "a short caller buffer reports the property size");
    }
    checkst(pBCryptSetContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", NULL, 4, v), S_INVALID,
            "SetContextFunctionProperty needs a name");
    checkst(pBCryptSetContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"NoFn", L"X", 4, v), S_NOTFOUND,
            "set a property of a missing function");

    /* functions go away with their properties and providers */
    pBCryptRemoveContextFunction(T_LOCAL, L"SgProv", I_HASH, L"F");
    pBCryptAddContextFunction(T_LOCAL, L"SgProv", I_HASH, L"F", BOTTOM);
    size = 0; got = NULL;
    checkst(pBCryptQueryContextFunctionProperty(T_LOCAL, L"SgProv", I_HASH, L"F", L"Image", &size, &got), S_NOTFOUND,
            "a recreated function has no old property");
    provs(L"SgProv", I_HASH, L"F", &l);
    check(l && l->count == 0, "nor old providers");
    if (l) pBCryptFreeBuffer(l);
    pBCryptDeleteContext(T_LOCAL, L"SgProv");
}

static p_iface mk_iface(ULONG id, ULONG n, WCHAR **names)
{
    p_iface i = { id, 0, n, names };
    return i;
}

static void test_providers(void)
{
    WCHAR *umhash[] = { L"SGHASH1", L"SGHASH2" }, *umcipher[] = { L"SGCIPHER" }, *kmhash[] = { L"SGHASH1" };
    WCHAR *aliases[] = { L"SgAlias" };
    p_iface ih = mk_iface(I_HASH, 2, umhash), ic = mk_iface(I_CIPHER, 1, umcipher), ik = mk_iface(I_HASH, 1, kmhash);
    p_iface *umifs[] = { &ih, &ic }, *kmifs[] = { &ik };
    p_image um = { L"sgprobe_um.dll", 2, umifs }, km = { L"sgprobe_km.sys", 1, kmifs };
    p_reg reg = { 1, aliases, &um, &km }, *q;
    strlist *l;
    p_name *names;
    ULONG size, count;
    NTS st;

    /* the built-in provider */
    size = 0; l = NULL;
    checkst(pBCryptEnumRegisteredProviders(&size, (void *)&l), S_OK_, "EnumRegisteredProviders");
    check(l && list_has(l, L"Microsoft Primitive Provider"), "the primitive provider is registered");
    if (l) pBCryptFreeBuffer(l);
    count = 0; names = NULL;
    checkst(pBCryptEnumProviders(L"AES", &count, (void *)&names, 0), S_OK_, "EnumProviders for AES");
    check(count >= 1 && names && !lstrcmpiW(names[0].name, L"Microsoft Primitive Provider"), "the primitive provider has AES");
    if (names) pBCryptFreeBuffer(names);
    count = 0; names = NULL;
    checkst(pBCryptEnumProviders(L"NoSuchAlgorithm", &count, (void *)&names, 0), S_NOTFOUND, "EnumProviders for a made-up algorithm");
    size = 0; q = NULL;
    checkst(pBCryptQueryProviderRegistration(L"Microsoft Primitive Provider", M_UM, 0, &size, (void *)&q), S_OK_,
            "QueryProviderRegistration of the primitive provider");
    check(q && q->um && !q->km && q->um->nif >= 2 && !lstrcmpiW(q->um->image, L"bcryptprimitives.dll"),
          "user mode image only, bcryptprimitives.dll");
    if (q) pBCryptFreeBuffer(q);
    size = 0; q = NULL;
    checkst(pBCryptQueryProviderRegistration(L"Microsoft Primitive Provider", M_KM, 0, &size, (void *)&q), S_NOTFOUND,
            "it has no kernel mode image");

    checkst(pBCryptRegisterProvider(NULL, 0, &reg), S_INVALID, "RegisterProvider needs a name");
    checkst(pBCryptRegisterProvider(L"SgProbeProv", 0, NULL), S_INVALID, "RegisterProvider needs a registration");
    checkst(pBCryptRegisterProvider(L"SgProbeProv", 0x80, &reg), S_INVALID, "RegisterProvider rejects unknown flags");
    checkst(pBCryptRegisterProvider(L"SgProbeProv", 0, &reg), S_OK_, "RegisterProvider");
    checkst(pBCryptRegisterProvider(L"sgprobeprov", 0, &reg), S_COLLIDE, "RegisterProvider again");
    checkst(pBCryptRegisterProvider(L"SgProbeProv", 1, &reg), S_OK_, "RegisterProvider with CRYPT_OVERWRITE");

    size = 0; l = NULL;
    pBCryptEnumRegisteredProviders(&size, (void *)&l);
    check(l && list_has(l, L"SgProbeProv") && list_has(l, L"Microsoft Primitive Provider"), "both providers are listed");
    if (l) pBCryptFreeBuffer(l);

    /* the registration is copied, not borrowed */
    umhash[0] = L"CHANGED";
    size = 0; q = NULL;
    checkst(pBCryptQueryProviderRegistration(L"SgProbeProv", M_MM, 0, &size, (void *)&q), S_OK_, "query it back (both modes)");
    check(q && q->nalias == 1 && !lstrcmpiW(q->aliases[0], L"SgAlias"), "the alias round-trips");
    check(q && q->um && q->km && !lstrcmpiW(q->um->image, L"sgprobe_um.dll") && !lstrcmpiW(q->km->image, L"sgprobe_km.sys"),
          "both images round-trip");
    /* the store is the registry: interfaces come back in key (id) order, not registration order */
    {
        p_iface *hi = NULL, *ci = NULL;
        ULONG k;
        for (k = 0; q && q->um && k < q->um->nif; k++)
        {
            if (q->um->ifs[k]->iface == I_HASH) hi = q->um->ifs[k];
            if (q->um->ifs[k]->iface == I_CIPHER) ci = q->um->ifs[k];
        }
        check(q && q->um && q->um->nif == 2 && hi && hi->nfunc == 2 &&
              !lstrcmpiW(hi->funcs[0], L"SGHASH1") && !lstrcmpiW(hi->funcs[1], L"SGHASH2"),
              "the user mode hash interface is a copy made at registration time");
        check(ci && ci->nfunc == 1 && !lstrcmpiW(ci->funcs[0], L"SGCIPHER"), "the user mode cipher interface");
    }
    if (q) pBCryptFreeBuffer(q);
    umhash[0] = L"SGHASH1";

    size = 0; q = NULL;
    checkst(pBCryptQueryProviderRegistration(L"SgProbeProv", M_UM, I_CIPHER, &size, (void *)&q), S_OK_,
            "query one interface only");
    check(q && !q->km && q->um && q->um->nif == 1 && q->um->ifs[0]->iface == I_CIPHER, "only that interface is returned");
    if (q) pBCryptFreeBuffer(q);
    size = 0; q = NULL;
    checkst(pBCryptQueryProviderRegistration(L"SgProbeProv", M_KM, 0, &size, (void *)&q), S_OK_, "query kernel mode");
    check(q && !q->um && q->km && q->km->nif == 1, "kernel mode image only");
    if (q) pBCryptFreeBuffer(q);
    size = 0; q = NULL;
    checkst(pBCryptQueryProviderRegistration(L"NoSuchProv", M_UM, 0, &size, (void *)&q), S_NOTFOUND, "query a missing provider");
    checkst(pBCryptQueryProviderRegistration(L"SgProbeProv", 9, 0, &size, (void *)&q), S_INVALID, "query with a bad mode");
    {
        BYTE buf[16]; p_reg *p = (p_reg *)buf;
        size = sizeof(buf);
        st = pBCryptQueryProviderRegistration(L"SgProbeProv", M_MM, 0, &size, (void *)&p);
        check(st == S_SMALL && size > sizeof(buf), "a small caller buffer reports the size needed");
    }

    count = 0; names = NULL;
    checkst(pBCryptEnumProviders(L"sghash2", &count, (void *)&names, 0), S_OK_, "EnumProviders finds the registered one");
    check(count == 1 && names && !lstrcmpiW(names[0].name, L"SgProbeProv"), "it is the only provider of SGHASH2");
    if (names) pBCryptFreeBuffer(names);

    checkst(pBCryptUnregisterProvider(L"SgProbeProv"), S_OK_, "UnregisterProvider");
    checkst(pBCryptUnregisterProvider(L"SgProbeProv"), S_NOTFOUND, "UnregisterProvider again");
    checkst(pBCryptUnregisterProvider(NULL), S_INVALID, "UnregisterProvider needs a name");
    size = 0; l = NULL;
    pBCryptEnumRegisteredProviders(&size, (void *)&l);
    check(l && !list_has(l, L"SgProbeProv"), "it is gone from the list");
    if (l) pBCryptFreeBuffer(l);
}

int main(void)
{
    bc = LoadLibraryA("bcrypt.dll");
    if (!bc) { printf("FAIL  no bcrypt.dll\n"); return 1; }
#define L_(name) do { *(FARPROC *)&p##name = GetProcAddress(bc, #name); \
    if (!p##name) { printf("FAIL  %s is not exported\n", #name); return 1; } } while (0)
    L_(BCryptCreateContext); L_(BCryptDeleteContext); L_(BCryptConfigureContext); L_(BCryptQueryContextConfiguration);
    L_(BCryptEnumContexts); L_(BCryptAddContextFunction); L_(BCryptRemoveContextFunction); L_(BCryptEnumContextFunctions);
    L_(BCryptConfigureContextFunction); L_(BCryptQueryContextFunctionConfiguration);
    L_(BCryptAddContextFunctionProvider); L_(BCryptRemoveContextFunctionProvider); L_(BCryptEnumContextFunctionProviders);
    L_(BCryptSetContextFunctionProperty); L_(BCryptQueryContextFunctionProperty);
    L_(BCryptRegisterProvider); L_(BCryptUnregisterProvider); L_(BCryptQueryProviderRegistration);
    L_(BCryptEnumRegisteredProviders); L_(BCryptEnumProviders); L_(BCryptFreeBuffer);

    test_contexts();
    test_functions();
    test_function_providers_and_properties();
    test_providers();

    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
