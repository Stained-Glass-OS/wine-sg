/* generated from dlls/rometadata/tests/rometadata.c: hand-built ECMA-335 image read through the metadata interfaces */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <objbase.h>
#include <cor.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
#ifndef CLDB_E_FILE_CORRUPT
#define CLDB_E_FILE_CORRUPT ((HRESULT)0x8013110e)
#endif
#ifndef CLDB_E_RECORD_NOTFOUND
#define CLDB_E_RECORD_NOTFOUND ((HRESULT)0x80131130)
#endif
#ifndef CLDB_S_TRUNCATION
#define CLDB_S_TRUNCATION ((HRESULT)0x00131106)
#endif
static int failures, checks;
#define ok(cond, ...) ok_(__FILE__, __LINE__)(cond, __VA_ARGS__)
static const char *cur_file; static int cur_line;
static void report(int c, const char *fmt, ...);
#define ok_(f, l) (cur_file = f, cur_line = l, report)
#include <stdarg.h>
static void report(int c, const char *fmt, ...)
{
    va_list ap;
    checks++;
    if (c) return;
    failures++;
    printf("FAIL line %d: ", cur_line);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}
#define skip printf
#define trace (void)
static HRESULT (WINAPI *pMetaDataGetDispenser)(REFCLSID, REFIID, void **);
static const IID IID_IMetaDataImport2_ = { 0xfce5efa0, 0x8bba, 0x4f8e, { 0xa0, 0x36, 0x8f, 0x20, 0x22, 0xb0, 0x84, 0x66 } };
#define IID_IMetaDataImport2 IID_IMetaDataImport2_
/* ---- a small hand-built ECMA-335 metadata image ---- */

struct bytes
{
    BYTE *p;
    size_t n, cap;
};

static void put(struct bytes *b, const void *data, size_t len)
{
    if (b->n + len > b->cap)
    {
        b->cap = (b->n + len) * 2 + 64;
        b->p = realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, data, len);
    b->n += len;
}

static void put8(struct bytes *b, unsigned int v) { BYTE c = v; put(b, &c, 1); }
static void put16(struct bytes *b, unsigned int v) { put8(b, v & 0xff); put8(b, v >> 8); }
static void put32(struct bytes *b, unsigned int v) { put16(b, v & 0xffff); put16(b, v >> 16); }
static void align4(struct bytes *b) { while (b->n & 3) put8(b, 0); }

static struct bytes strings, blobs;

static unsigned int add_str(const char *s)
{
    unsigned int off = strings.n;
    if (!*s) return 0;
    put(&strings, s, strlen(s) + 1);
    return off;
}

static unsigned int add_blob(const void *data, unsigned int len)
{
    unsigned int off = blobs.n;
    if (!len) return 0;
    put8(&blobs, len);
    put(&blobs, data, len);
    return off;
}

#define BLOB(...) add_blob((const BYTE[]){ __VA_ARGS__ }, sizeof((const BYTE[]){ __VA_ARGS__ }))

static void build_metadata(struct bytes *out)
{
    static const char version[] = "WindowsRuntime 1.2";
    static const BYTE guid[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    static const BYTE us[] = { 0, 5, 'h', 0, 'i', 0, 0, 0 };
    unsigned int s_module, s_test, s_base, s_derived, s_nested, s_f1, s_f2, s_m1, s_m2, s_m3, s_p1, s_system, s_object;
    unsigned int s_asm, s_corlib, s_ctor, s_prop, s_modname, s_t;
    unsigned int b_msig, b_msig2, b_fsig, b_const, b_ctor, b_ca, b_prop;
    struct bytes root = { 0 }, t = { 0 };
    static const char *names[] = { "#~", "#Strings", "#US", "#GUID", "#Blob" };
    unsigned int sizes[5], i, hdr;
    const BYTE *data[5];

    strings.n = blobs.n = 0;
    put8(&strings, 0);
    put8(&blobs, 0);
    s_module = add_str("<Module>"); s_modname = add_str("test.winmd");
    s_test = add_str("Test"); s_base = add_str("Base"); s_derived = add_str("Derived"); s_nested = add_str("Nested");
    s_f1 = add_str("Field1"); s_f2 = add_str("Field2"); s_m1 = add_str("Method1"); s_m2 = add_str("Method2");
    s_m3 = add_str("Method3"); s_p1 = add_str("p1"); s_system = add_str("System"); s_object = add_str("Object");
    s_asm = add_str("Asm"); s_corlib = add_str("mscorlib"); s_ctor = add_str(".ctor"); s_prop = add_str("Prop");
    s_t = add_str("T");
    b_msig = BLOB(0x20, 0x01, 0x01, 0x08);
    b_msig2 = BLOB(0x00, 0x00, 0x01);
    b_fsig = BLOB(0x06, 0x08);
    b_const = BLOB(0x05, 0, 0, 0);
    b_ctor = BLOB(0x20, 0x00, 0x01);
    b_ca = BLOB(0x01, 0x00, 0x00, 0x00);
    b_prop = BLOB(0x28, 0x00, 0x08);

    /* #~ stream */
    put32(&t, 0); put8(&t, 2); put8(&t, 0); put8(&t, 0); put8(&t, 1);
    {
        unsigned int valid[] = { 0, 1, 2, 4, 6, 8, 9, 0xa, 0xb, 0xc, 0xf, 0x10, 0x15, 0x17, 0x18, 0x20, 0x23, 0x29, 0x2a };
        unsigned int nrows[] = { 1, 1, 4, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        unsigned long long mask = 0;

        for (i = 0; i < ARRAY_SIZE(valid); i++) mask |= 1ull << valid[i];
        put32(&t, mask & 0xffffffff); put32(&t, mask >> 32);
        put32(&t, 0); put32(&t, 0);   /* sorted */
        for (i = 0; i < ARRAY_SIZE(nrows); i++) put32(&t, nrows[i]);
    }
    /* Module */ put16(&t, 0); put16(&t, s_modname); put16(&t, 1); put16(&t, 0); put16(&t, 0);
    /* TypeRef */ put16(&t, 6); put16(&t, s_object); put16(&t, s_system);
    /* TypeDef */
    put32(&t, 0); put16(&t, s_module); put16(&t, 0); put16(&t, 0); put16(&t, 1); put16(&t, 1);
    put32(&t, 0x100001); put16(&t, s_base); put16(&t, s_test); put16(&t, 5); put16(&t, 1); put16(&t, 1);
    put32(&t, 0x100001); put16(&t, s_derived); put16(&t, s_test); put16(&t, 8); put16(&t, 2); put16(&t, 3);
    put32(&t, 0x000002); put16(&t, s_nested); put16(&t, 0); put16(&t, 5); put16(&t, 3); put16(&t, 4);
    /* Field */
    put16(&t, 6); put16(&t, s_f1); put16(&t, b_fsig);
    put16(&t, 0x8056); put16(&t, s_f2); put16(&t, b_fsig);
    /* MethodDef */
    put32(&t, 0x2050); put16(&t, 0); put16(&t, 0x86); put16(&t, s_m1); put16(&t, b_msig); put16(&t, 1);
    put32(&t, 0); put16(&t, 0); put16(&t, 0x446); put16(&t, s_m2); put16(&t, b_msig2); put16(&t, 2);
    put32(&t, 0x2060); put16(&t, 0); put16(&t, 0x86); put16(&t, s_m3); put16(&t, b_msig2); put16(&t, 2);
    /* Param */ put16(&t, 0); put16(&t, 1); put16(&t, s_p1);
    /* InterfaceImpl */ put16(&t, 3); put16(&t, 5);
    /* MemberRef */ put16(&t, 9); put16(&t, s_ctor); put16(&t, b_ctor);
    /* Constant */ put8(&t, 8); put8(&t, 0); put16(&t, 8); put16(&t, b_const);
    /* CustomAttribute */ put16(&t, (2 << 5) | 3); put16(&t, (1 << 3) | 3); put16(&t, b_ca);
    /* ClassLayout */ put16(&t, 4); put32(&t, 16); put16(&t, 3);
    /* FieldLayout */ put32(&t, 8); put16(&t, 2);
    /* PropertyMap */ put16(&t, 2); put16(&t, 1);
    /* Property */ put16(&t, 0); put16(&t, s_prop); put16(&t, b_prop);
    /* MethodSemantics */ put16(&t, 2); put16(&t, 1); put16(&t, 3);
    /* Assembly */ put32(&t, 0x8004); put16(&t, 1); put16(&t, 2); put16(&t, 3); put16(&t, 4); put32(&t, 0);
                   put16(&t, 0); put16(&t, s_asm); put16(&t, 0);
    /* AssemblyRef */ put16(&t, 5); put16(&t, 6); put16(&t, 7); put16(&t, 8); put32(&t, 0); put16(&t, 0);
                      put16(&t, s_corlib); put16(&t, 0); put16(&t, 0);
    /* NestedClass */ put16(&t, 4); put16(&t, 2);
    /* GenericParam */ put16(&t, 0); put16(&t, 0); put16(&t, 6); put16(&t, s_t);
    align4(&t);

    align4(&strings);
    align4(&blobs);
    data[0] = t.p; sizes[0] = t.n;
    data[1] = strings.p; sizes[1] = strings.n;
    data[2] = us; sizes[2] = sizeof(us);
    data[3] = guid; sizes[3] = sizeof(guid);
    data[4] = blobs.p; sizes[4] = blobs.n;

    put32(&root, 0x424a5342); put16(&root, 1); put16(&root, 1); put32(&root, 0);
    put32(&root, 20); put(&root, version, sizeof(version)); put8(&root, 0);
    put16(&root, 0); put16(&root, 5);
    hdr = root.n;
    for (i = 0; i < 5; i++) hdr += 8 + ((strlen(names[i]) + 4) & ~3u);
    for (i = 0; i < 5; i++)
    {
        unsigned int len = (strlen(names[i]) + 4) & ~3u;
        char pad[16] = { 0 };

        put32(&root, hdr); put32(&root, sizes[i]);
        strcpy(pad, names[i]);
        put(&root, pad, len);
        hdr += sizes[i];
    }
    for (i = 0; i < 5; i++) put(&root, data[i], sizes[i]);
    *out = root;
    free(t.p);
}

static void check_str(const WCHAR *got, const char *expect, int line)
{
    WCHAR w[128];
    MultiByteToWideChar(CP_ACP, 0, expect, -1, w, ARRAY_SIZE(w));
    ok_(__FILE__, line)(!wcscmp(got, w), "Got %s, expected %s.\n", "?", expect);
}
#define CHECK_STR(g, e) check_str(g, e, __LINE__)

static void test_import(void)
{
    IMetaDataDispenser *dispenser;
    IMetaDataImport2 *import;
    IMetaDataImport *import1;
    IMetaDataAssemblyImport *asm_import;
    struct bytes md = { 0 };
    IUnknown *unk;
    HCORENUM e;
    mdTypeDef tds[8], td, td2;
    mdToken tokens[8], tk, tk2;
    mdMethodDef mds[8], md1;
    mdFieldDef fds[8];
    WCHAR name[64];
    ULONG count, len, size, i, rva, seq;
    DWORD flags, impl;
    const BYTE *sig;
    const void *blob;
    HRESULT hr;

    build_metadata(&md);

    hr = pMetaDataGetDispenser(&CLSID_CorMetaDataDispenser, &IID_IMetaDataDispenser, (void **)&dispenser);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = dispenser->lpVtbl->OpenScopeOnMemory(dispenser, md.p, md.n, 0, &IID_IMetaDataImport2, &unk);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    if (hr != S_OK) return;
    import = (IMetaDataImport2 *)unk;
    hr = import->lpVtbl->QueryInterface(import, &IID_IMetaDataImport, (void **)&import1);
    ok(hr == S_OK && (void *)import1 == (void *)import, "Got hr %#lx.\n", hr);
    import1->lpVtbl->Release(import1);
    hr = import->lpVtbl->QueryInterface(import, &IID_IMetaDataAssemblyImport, (void **)&asm_import);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = import->lpVtbl->QueryInterface(import, &IID_IDispatch, (void **)&unk);
    ok(hr == E_NOINTERFACE, "Got hr %#lx.\n", hr);

    /* bad data */
    hr = dispenser->lpVtbl->OpenScopeOnMemory(dispenser, "garbage garbage garbage", 24, 0, &IID_IMetaDataImport, &unk);
    ok(hr == CLDB_E_FILE_CORRUPT, "Got hr %#lx.\n", hr);
    hr = dispenser->lpVtbl->OpenScopeOnMemory(dispenser, md.p, md.n / 2, 0, &IID_IMetaDataImport, &unk);
    ok(FAILED(hr), "Got hr %#lx.\n", hr);
    hr = dispenser->lpVtbl->OpenScope(dispenser, L"C:\\does\\not\\exist.winmd", 0, &IID_IMetaDataImport, &unk);
    ok(hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
       "Got hr %#lx.\n", hr);

    /* scope and version */
    hr = import->lpVtbl->GetVersionString(import, name, ARRAY_SIZE(name), &len);
    ok(hr == S_OK && len == 19, "Got hr %#lx, len %lu.\n", hr, len);
    CHECK_STR(name, "WindowsRuntime 1.2");
    hr = import->lpVtbl->GetScopeProps(import, name, ARRAY_SIZE(name), &len, NULL);
    ok(hr == S_OK && len == 11, "Got hr %#lx, len %lu.\n", hr, len);
    CHECK_STR(name, "test.winmd");
    hr = import->lpVtbl->GetPEKind(import, &flags, &impl);
    ok(hr == S_OK && !flags, "Got hr %#lx, kind %#lx.\n", hr, flags);

    /* type definitions: the global <Module> type is not enumerated */
    e = NULL;
    hr = import->lpVtbl->EnumTypeDefs(import, &e, tds, ARRAY_SIZE(tds), &count);
    ok(hr == S_OK && count == 3 && tds[0] == 0x02000002 && tds[2] == 0x02000004, "Got hr %#lx, count %lu.\n", hr, count);
    hr = import->lpVtbl->CountEnum(import, e, &count);
    ok(hr == S_OK && count == 3, "Got hr %#lx, count %lu.\n", hr, count);
    hr = import->lpVtbl->EnumTypeDefs(import, &e, tds, ARRAY_SIZE(tds), &count);
    ok(hr == S_FALSE && !count, "Got hr %#lx, count %lu.\n", hr, count);
    hr = import->lpVtbl->ResetEnum(import, e, 1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = import->lpVtbl->EnumTypeDefs(import, &e, tds, 1, &count);
    ok(hr == S_OK && count == 1 && tds[0] == 0x02000003, "Got hr %#lx, count %lu, tk %#x.\n", hr, count, tds[0]);
    import->lpVtbl->CloseEnum(import, e);

    hr = import->lpVtbl->FindTypeDefByName(import, L"Test.Derived", 0, &td);
    ok(hr == S_OK && td == 0x02000003, "Got hr %#lx, td %#x.\n", hr, td);
    hr = import->lpVtbl->FindTypeDefByName(import, L"Test.Missing", 0, &td2);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);
    hr = import->lpVtbl->FindTypeDefByName(import, L"Nested", 0x02000002, &td2);
    ok(hr == S_OK && td2 == 0x02000004, "Got hr %#lx, td %#x.\n", hr, td2);
    hr = import->lpVtbl->FindTypeDefByName(import, L"Nested", 0, &td2);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);
    hr = import->lpVtbl->GetNestedClassProps(import, 0x02000004, &tk);
    ok(hr == S_OK && tk == 0x02000002, "Got hr %#lx, tk %#x.\n", hr, tk);
    hr = import->lpVtbl->GetNestedClassProps(import, 0x02000002, &tk);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);

    hr = import->lpVtbl->GetTypeDefProps(import, td, name, ARRAY_SIZE(name), &len, &flags, &tk);
    ok(hr == S_OK && len == 13 && flags == 0x100001 && tk == 0x02000002, "Got hr %#lx, len %lu, flags %#lx, ext %#x.\n",
       hr, len, flags, tk);
    CHECK_STR(name, "Test.Derived");
    hr = import->lpVtbl->GetTypeDefProps(import, 0x02000002, NULL, 0, &len, NULL, &tk);
    ok(hr == S_OK && len == 10 && tk == 0x01000001, "Got hr %#lx, len %lu, ext %#x.\n", hr, len, tk);
    /* truncation */
    memset(name, 0xff, sizeof(name));
    hr = import->lpVtbl->GetTypeDefProps(import, td, name, 5, &len, NULL, NULL);
    ok(hr == CLDB_S_TRUNCATION && len == 13 && !wcscmp(name, L"Test"), "Got hr %#lx, len %lu, %s.\n", hr, len,
       "?");
    hr = import->lpVtbl->GetTypeDefProps(import, 0x02000009, name, 5, &len, NULL, NULL);
    ok(FAILED(hr), "Got hr %#lx.\n", hr);
    hr = import->lpVtbl->GetTypeDefProps(import, 0x06000001, name, 5, &len, NULL, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = import->lpVtbl->GetTypeRefProps(import, 0x01000001, &tk, name, ARRAY_SIZE(name), &len);
    ok(hr == S_OK && tk == 0x23000001 && len == 14, "Got hr %#lx, scope %#x, len %lu.\n", hr, tk, len);
    CHECK_STR(name, "System.Object");
    hr = import->lpVtbl->FindTypeRef(import, 0x23000001, L"System.Object", &tk);
    ok(hr == S_OK && tk == 0x01000001, "Got hr %#lx, tk %#x.\n", hr, tk);
    hr = import->lpVtbl->ResolveTypeRef(import, 0x01000001, &IID_IMetaDataImport, &unk, &tk);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);

    /* interface impls */
    e = NULL;
    hr = import->lpVtbl->EnumInterfaceImpls(import, &e, td, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 1 && tokens[0] == 0x09000001, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    hr = import->lpVtbl->GetInterfaceImplProps(import, 0x09000001, &tk, &tk2);
    ok(hr == S_OK && tk == td && tk2 == 0x01000001, "Got hr %#lx, %#x %#x.\n", hr, tk, tk2);

    /* methods */
    e = NULL;
    hr = import->lpVtbl->EnumMethods(import, &e, 0x02000002, mds, ARRAY_SIZE(mds), &count);
    ok(hr == S_OK && count == 2 && mds[0] == 0x06000001 && mds[1] == 0x06000002, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    e = NULL;
    hr = import->lpVtbl->EnumMethods(import, &e, td, mds, ARRAY_SIZE(mds), &count);
    ok(hr == S_OK && count == 1 && mds[0] == 0x06000003, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    e = NULL;
    hr = import->lpVtbl->EnumMethods(import, &e, 0x02000004, mds, ARRAY_SIZE(mds), &count);
    ok(hr == S_FALSE && !count, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    e = NULL;
    hr = import->lpVtbl->EnumMethodsWithName(import, &e, 0x02000002, L"Method2", mds, ARRAY_SIZE(mds), &count);
    ok(hr == S_OK && count == 1 && mds[0] == 0x06000002, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);

    hr = import->lpVtbl->GetMethodProps(import, 0x06000001, &tk, name, ARRAY_SIZE(name), &len, &flags, &sig, &size,
                                         &rva, &impl);
    ok(hr == S_OK && tk == 0x02000002 && len == 8 && flags == 0x86 && size == 4 && rva == 0x2050 && !impl
       && sig[0] == 0x20 && sig[3] == 0x08, "Got hr %#lx, class %#x, len %lu, flags %#lx, size %lu, rva %#lx.\n",
       hr, tk, len, flags, size, rva);
    CHECK_STR(name, "Method1");
    hr = import->lpVtbl->GetMethodProps(import, 0x06000003, &tk, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    ok(hr == S_OK && tk == 0x02000003, "Got hr %#lx, class %#x.\n", hr, tk);

    hr = import->lpVtbl->FindMethod(import, 0x02000002, L"Method1", NULL, 0, &md1);
    ok(hr == S_OK && md1 == 0x06000001, "Got hr %#lx, %#x.\n", hr, md1);
    hr = import->lpVtbl->FindMethod(import, 0x02000002, L"Method1", (const BYTE[]){0x20, 0x01, 0x01, 0x08}, 4, &md1);
    ok(hr == S_OK && md1 == 0x06000001, "Got hr %#lx, %#x.\n", hr, md1);
    hr = import->lpVtbl->FindMethod(import, 0x02000002, L"Method1", (const BYTE[]){0x20, 0x01, 0x01, 0x09}, 4, &md1);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);
    hr = import->lpVtbl->FindMethod(import, td, L"Method1", NULL, 0, &md1);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);
    hr = import->lpVtbl->FindMember(import, 0x02000002, L"Field1", NULL, 0, &tk);
    ok(hr == S_OK && tk == 0x04000001, "Got hr %#lx, %#x.\n", hr, tk);

    /* params */
    e = NULL;
    hr = import->lpVtbl->EnumParams(import, &e, 0x06000001, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 1 && tokens[0] == 0x08000001, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    e = NULL;
    hr = import->lpVtbl->EnumParams(import, &e, 0x06000002, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_FALSE && !count, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    hr = import->lpVtbl->GetParamProps(import, 0x08000001, &tk, &seq, name, ARRAY_SIZE(name), &len, &flags, NULL, NULL, NULL);
    ok(hr == S_OK && tk == 0x06000001 && seq == 1 && len == 3, "Got hr %#lx, method %#x, seq %lu.\n", hr, tk, seq);
    CHECK_STR(name, "p1");
    hr = import->lpVtbl->GetParamForMethodIndex(import, 0x06000001, 1, &tk);
    ok(hr == S_OK && tk == 0x08000001, "Got hr %#lx, %#x.\n", hr, tk);
    hr = import->lpVtbl->GetParamForMethodIndex(import, 0x06000001, 2, &tk);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);

    /* fields and constants */
    e = NULL;
    hr = import->lpVtbl->EnumFields(import, &e, 0x02000002, fds, ARRAY_SIZE(fds), &count);
    ok(hr == S_OK && count == 1 && fds[0] == 0x04000001, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    e = NULL;
    hr = import->lpVtbl->EnumMembers(import, &e, 0x02000002, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 3, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    hr = import->lpVtbl->GetFieldProps(import, 0x04000002, &tk, name, ARRAY_SIZE(name), &len, &flags, &sig, &size,
                                        &impl, &blob, &i);
    ok(hr == S_OK && tk == td && flags == 0x8056 && size == 2 && impl == ELEMENT_TYPE_I4 && *(const int *)blob == 5,
       "Got hr %#lx, class %#x, flags %#lx, type %lu.\n", hr, tk, flags, impl);
    CHECK_STR(name, "Field2");
    hr = import->lpVtbl->GetFieldProps(import, 0x04000001, NULL, NULL, 0, NULL, NULL, NULL, NULL, &impl, &blob, NULL);
    ok(hr == S_OK && impl == ELEMENT_TYPE_END && !blob, "Got hr %#lx, type %lu.\n", hr, impl);

    /* class layout */
    {
        COR_FIELD_OFFSET offsets[4];
        DWORD pack;

        hr = import->lpVtbl->GetClassLayout(import, td, &pack, offsets, ARRAY_SIZE(offsets), &count, &size);
        ok(hr == S_OK && pack == 4 && size == 16 && count == 1 && offsets[0].ridOfField == 0x04000002
           && offsets[0].ulOffset == 8, "Got hr %#lx, pack %lu, size %lu, count %lu.\n", hr, pack, size, count);
        hr = import->lpVtbl->GetClassLayout(import, 0x02000002, &pack, offsets, ARRAY_SIZE(offsets), &count, &size);
        ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);
    }

    /* member refs and custom attributes */
    hr = import->lpVtbl->GetMemberRefProps(import, 0x0a000001, &tk, name, ARRAY_SIZE(name), &len, &sig, &size);
    ok(hr == S_OK && tk == 0x01000001 && size == 3, "Got hr %#lx, parent %#x.\n", hr, tk);
    CHECK_STR(name, ".ctor");
    hr = import->lpVtbl->FindMemberRef(import, 0x01000001, L".ctor", NULL, 0, &tk);
    ok(hr == S_OK && tk == 0x0a000001, "Got hr %#lx.\n", hr);
    e = NULL;
    hr = import->lpVtbl->EnumMemberRefs(import, &e, 0x01000001, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 1, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    e = NULL;
    hr = import->lpVtbl->EnumCustomAttributes(import, &e, 0x02000002, 0, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 1 && tokens[0] == 0x0c000001, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    e = NULL;
    hr = import->lpVtbl->EnumCustomAttributes(import, &e, td, 0, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_FALSE && !count, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    hr = import->lpVtbl->GetCustomAttributeProps(import, 0x0c000001, &tk, &tk2, &blob, &size);
    ok(hr == S_OK && tk == 0x02000002 && tk2 == 0x0a000001 && size == 4, "Got hr %#lx, %#x %#x %lu.\n", hr, tk, tk2, size);
    hr = import->lpVtbl->GetCustomAttributeByName(import, 0x02000002, L"System.Object", &blob, &size);
    ok(hr == S_OK && size == 4, "Got hr %#lx, size %lu.\n", hr, size);
    hr = import->lpVtbl->GetCustomAttributeByName(import, 0x02000002, L"System.Other", &blob, &size);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    /* properties and method semantics */
    e = NULL;
    hr = import->lpVtbl->EnumProperties(import, &e, 0x02000002, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 1 && tokens[0] == 0x17000001, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    {
        mdMethodDef setter, getter, others[2];

        hr = import->lpVtbl->GetPropertyProps(import, 0x17000001, &tk, name, ARRAY_SIZE(name), &len, &flags, &sig, &size,
                                               NULL, NULL, NULL, &setter, &getter, others, 2, &count);
        ok(hr == S_OK && tk == 0x02000002 && !setter && getter == 0x06000001 && !count && size == 3,
           "Got hr %#lx, class %#x, getter %#x.\n", hr, tk, getter);
        CHECK_STR(name, "Prop");
    }
    hr = import->lpVtbl->GetMethodSemantics(import, 0x06000001, 0x17000001, &flags);
    ok(hr == S_OK && flags == 2, "Got hr %#lx, flags %#lx.\n", hr, flags);
    hr = import->lpVtbl->GetMethodSemantics(import, 0x06000002, 0x17000001, &flags);
    ok(hr == CLDB_E_RECORD_NOTFOUND, "Got hr %#lx.\n", hr);

    /* generics, strings, tokens */
    e = NULL;
    hr = import->lpVtbl->EnumGenericParams(import, &e, td, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 1, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    hr = import->lpVtbl->GetGenericParamProps(import, 0x2a000001, &seq, &flags, &tk, NULL, name, ARRAY_SIZE(name), &len);
    ok(hr == S_OK && !seq && tk == td && len == 2, "Got hr %#lx, owner %#x.\n", hr, tk);
    hr = import->lpVtbl->GetUserString(import, 0x70000001, name, ARRAY_SIZE(name), &len);
    ok(hr == S_OK && len == 2 && name[0] == 'h' && name[1] == 'i', "Got hr %#lx, len %lu.\n", hr, len);
    e = NULL;
    hr = import->lpVtbl->EnumUserStrings(import, &e, tokens, ARRAY_SIZE(tokens), &count);
    ok(hr == S_OK && count == 1 && tokens[0] == 0x70000001, "Got hr %#lx, count %lu.\n", hr, count);
    import->lpVtbl->CloseEnum(import, e);
    ok(import->lpVtbl->IsValidToken(import, 0x02000004), "Expected valid.\n");
    ok(!import->lpVtbl->IsValidToken(import, 0x02000005), "Expected invalid.\n");
    ok(!import->lpVtbl->IsValidToken(import, 0x06000004), "Expected invalid.\n");
    ok(!import->lpVtbl->IsValidToken(import, 0x02000000), "Expected invalid.\n");
    ok(!import->lpVtbl->IsValidToken(import, 0x7f000001), "Expected invalid.\n");
    {
        const char *utf8;
        int global;

        hr = import->lpVtbl->GetNameFromToken(import, 0x02000003, &utf8);
        ok(hr == S_OK && !strcmp(utf8, "Derived"), "Got hr %#lx.\n", hr);
        hr = import->lpVtbl->IsGlobal(import, 0x02000001, &global);
        ok(hr == S_OK && global, "Got hr %#lx.\n", hr);
        hr = import->lpVtbl->IsGlobal(import, td, &global);
        ok(hr == S_OK && !global, "Got hr %#lx.\n", hr);
    }
    hr = import->lpVtbl->GetRVA(import, 0x06000003, &rva, &impl);
    ok(hr == S_OK && rva == 0x2060, "Got hr %#lx, rva %#lx.\n", hr, rva);

    /* assembly */
    {
        ASSEMBLYMETADATA amd = { 0 };
        WCHAR locale[8];
        const void *key;
        ULONG keylen, alg;

        hr = asm_import->lpVtbl->GetAssemblyFromScope(asm_import, &tk);
        ok(hr == S_OK && tk == 0x20000001, "Got hr %#lx.\n", hr);
        amd.szLocale = locale;
        amd.cbLocale = ARRAY_SIZE(locale);
        hr = asm_import->lpVtbl->GetAssemblyProps(asm_import, tk, &key, &keylen, &alg, name, ARRAY_SIZE(name), &len,
                                                      &amd, &flags);
        ok(hr == S_OK && alg == 0x8004 && amd.usMajorVersion == 1 && amd.usRevisionNumber == 4 && !keylen,
           "Got hr %#lx, alg %#lx.\n", hr, alg);
        CHECK_STR(name, "Asm");
        e = NULL;
        hr = asm_import->lpVtbl->EnumAssemblyRefs(asm_import, &e, tokens, ARRAY_SIZE(tokens), &count);
        ok(hr == S_OK && count == 1 && tokens[0] == 0x23000001, "Got hr %#lx, count %lu.\n", hr, count);
        asm_import->lpVtbl->CloseEnum(asm_import, e);
        memset(&amd, 0, sizeof(amd));
        hr = asm_import->lpVtbl->GetAssemblyRefProps(asm_import, 0x23000001, &key, &keylen, name, ARRAY_SIZE(name),
                                                         &len, &amd, NULL, NULL, &flags);
        ok(hr == S_OK && amd.usMajorVersion == 5 && amd.usRevisionNumber == 8, "Got hr %#lx.\n", hr);
        CHECK_STR(name, "mscorlib");
    }

    asm_import->lpVtbl->Release(asm_import);
    import->lpVtbl->Release(import);
    dispenser->lpVtbl->Release(dispenser);
    free(md.p);
    (void)td2; (void)md1;
}

static void test_real_file(void)
{
    WCHAR path[MAX_PATH];
    IMetaDataDispenser *dispenser;
    IMetaDataImport2 *import;
    mdTypeDef tds[64];
    HCORENUM e = NULL;
    ULONG count, total = 0;
    WCHAR name[256];
    IUnknown *unk;
    HRESULT hr;

    if (!GetEnvironmentVariableW(L"SG_METADATA_FILE", path, ARRAY_SIZE(path)))
    {
        printf("SG_METADATA_FILE not set.\n");
        return;
    }
    hr = pMetaDataGetDispenser(&CLSID_CorMetaDataDispenser, &IID_IMetaDataDispenser, (void **)&dispenser);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = dispenser->lpVtbl->OpenScope(dispenser, path, 0, &IID_IMetaDataImport2, &unk);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    if (hr == S_OK)
    {
        import = (IMetaDataImport2 *)unk;
        while ((hr = import->lpVtbl->EnumTypeDefs(import, &e, tds, ARRAY_SIZE(tds), &count)) == S_OK)
        {
            ULONG i;
            for (i = 0; i < count; i++)
            {
                mdMethodDef methods[256];
                mdToken extends;
                HCORENUM me = NULL;
                ULONG mcount;
                DWORD flags;

                hr = import->lpVtbl->GetTypeDefProps(import, tds[i], name, ARRAY_SIZE(name), NULL, &flags, &extends);
                ok(SUCCEEDED(hr), "Got hr %#lx for %#x.\n", hr, tds[i]);
                hr = import->lpVtbl->EnumMethods(import, &me, tds[i], methods, ARRAY_SIZE(methods), &mcount);
                ok(SUCCEEDED(hr), "Got hr %#lx.\n", hr);
                import->lpVtbl->CloseEnum(import, me);
            }
            total += count;
        }
        (void)("%s: %lu types\n", "file", total);
        import->lpVtbl->CloseEnum(import, e);
        {
            mdTypeDef point, stringable;
            mdFieldDef field;
            mdMethodDef method;
            const BYTE *sig;
            const void *blob;
            ULONG size;

            hr = import->lpVtbl->FindTypeDefByName(import, L"Windows.Foundation.Point", 0, &point);
            if (hr == S_OK)
            {
                hr = import->lpVtbl->FindField(import, point, L"X", NULL, 0, &field);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);
                hr = import->lpVtbl->GetFieldProps(import, field, NULL, NULL, 0, NULL, NULL, &sig, &size, NULL, NULL, NULL);
                ok(hr == S_OK && size == 2 && sig[0] == 0x06 && sig[1] == ELEMENT_TYPE_R4, "Got hr %#lx.\n", hr);
            }
            hr = import->lpVtbl->FindTypeDefByName(import, L"Windows.Foundation.IStringable", 0, &stringable);
            if (hr == S_OK)
            {
                hr = import->lpVtbl->FindMethod(import, stringable, L"ToString", NULL, 0, &method);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);
                hr = import->lpVtbl->GetCustomAttributeByName(import, stringable,
                                                               L"Windows.Foundation.Metadata.GuidAttribute", &blob, &size);
                ok(hr == S_OK && size == 20 && *(const unsigned int *)((const BYTE *)blob + 2) == 0x96369f54,
                   "Got hr %#lx, size %lu.\n", hr, size);
            }
        }
        hr = import->lpVtbl->GetVersionString(import, name, ARRAY_SIZE(name), NULL);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        (void)("version %s\n", "v");
        import->lpVtbl->Release(import);
    }
    dispenser->lpVtbl->Release(dispenser);
}


int main(void)
{
    HMODULE m = LoadLibraryA("rometadata.dll");
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    (void)hr;
    if (!m || !(pMetaDataGetDispenser = (void *)GetProcAddress(m, "MetaDataGetDispenser")))
    {
        printf("FAIL: no rometadata.dll\n");
        return 1;
    }
    test_import();
    {
        WCHAR p[MAX_PATH];
        if (GetEnvironmentVariableW(L"SG_METADATA_FILE", p, MAX_PATH)) test_real_file();
    }
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
