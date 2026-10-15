/* the metadata writer: IMetaDataEmit/Emit2/AssemblyEmit build a scope, the reader (import interfaces) sees it
 * live and after Save, and a saved scope can be opened for writing again. Usage: probe [OUTFILE] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <objbase.h>
#include <cor.h>

static int failures, checks;
#define ok(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); } } while (0)
#define V(p) ((p)->lpVtbl)

static const GUID CLSID_Disp = {0xe5cb7a31,0x7512,0x11d2,{0x89,0xce,0x00,0x80,0xc7,0x92,0xe5,0xd8}};
static const GUID IID_Disp = {0x31bcfce2,0xdafb,0x11d2,{0x9f,0x81,0x00,0xc0,0x4f,0x79,0xa0,0xa3}};
static const GUID IID_Emit_ = {0xba3fee4c,0xecb9,0x4e41,{0x83,0xb7,0x18,0x3f,0xa4,0x1c,0xd8,0x59}};
static const GUID IID_Emit2_ = {0xf5dd9950,0xf693,0x42e6,{0x83,0x0e,0x7b,0x83,0x3e,0x81,0x46,0xa9}};
static const GUID IID_AsmEmit_ = {0x211ef15b,0x5317,0x4438,{0xb1,0x96,0xde,0xc8,0x7b,0x88,0x76,0x93}};
static const GUID IID_Import_ = {0x7dac8207,0xd3ae,0x4c75,{0x9b,0x67,0x92,0x80,0x1a,0x49,0x7d,0x44}};
static const GUID IID_Import2_ = {0xfce5efa0,0x8bba,0x4f8e,{0xa0,0x36,0x8f,0x20,0x22,0xb0,0x84,0x66}};
static const GUID IID_AsmImport_ = {0xee62470b,0xe94b,0x424e,{0x9b,0x7c,0x2f,0x00,0xc9,0x24,0x9f,0x93}};

#define tdPublic_ 1
#define tdInterface_ 0x20
#define tdAbstract_ 0x80
#define tdSealed_ 0x100
#define tdNestedPublic_ 2
#define mdPublic_ 6
#define mdVirtual_ 0x40
#define mdAbstract_ 0x400
#define mdStatic_ 0x10
#define mdHideBySig_ 0x80
#define mdSpecialName_ 0x800
#define mdRTSpecialName_ 0x1000
#define fdPublic_ 6
#define fdStatic_ 0x10
#define fdLiteral_ 0x40
#define fdHasDefault_ 0x8000

/* an IMapToken that records the tokens Save renumbers */
static struct { mdToken from, to; } maps[256];
static int nmaps;
static IMapToken map_obj;
static HRESULT WINAPI map_qi(IMapToken *This, REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IMapToken)) { *obj = This; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI map_addref(IMapToken *This) { return 2; }
static ULONG WINAPI map_release(IMapToken *This) { return 1; }
static HRESULT WINAPI map_map(IMapToken *This, mdToken from, mdToken to)
{
    if (nmaps < 256) { maps[nmaps].from = from; maps[nmaps].to = to; nmaps++; }
    return S_OK;
}
static IMapTokenVtbl map_vtbl = { map_qi, map_addref, map_release, map_map };
static mdToken mapped(mdToken tk)
{
    int i;
    for (i = 0; i < nmaps; i++) if (maps[i].from == tk) return maps[i].to;
    return tk;
}

typedef struct { IMetaDataEmit2 *emit; IMetaDataAssemblyEmit *aemit; IMetaDataImport2 *imp; IMetaDataAssemblyImport *aimp; } scope_t;

static const BYTE sig_void_i4[] = { 0x20, 1, 1, 8 };      /* instance void (int32) */
static const BYTE sig_void[] = { 0x20, 0, 1 };           /* instance void () */
static const BYTE sig_get[] = { 0x20, 0, 8 };            /* instance int32 () */
static const BYTE sig_f_i4[] = { 6, 8 };                 /* field int32 */
static const BYTE sig_prop[] = { 0x28, 0, 8 };           /* property int32 */

typedef struct { mdTypeDef ifoo, foo, bar, inner, sval; mdMethodDef foo_ctor, foo_a, foo_b, bar_n, area, getid, setid, pinv, add, rem; mdFieldDef f_count, f_const, f_s, sv_x, sv_y; mdProperty prop; mdEvent ev; mdTypeRef object_ref, vt_ref; mdParamDef p1; mdAssemblyRef mscorlib; mdModuleRef kernel32; mdCustomAttribute ca, ca2; mdMemberRef mr_ctor; mdString us; } tokens_t;

static HRESULT build(scope_t *s, tokens_t *t)
{
    HRESULT hr;
    ASSEMBLYMETADATA am = { 1, 2, 3, 4, NULL, 0, NULL, 0, NULL, 0 };
    ASSEMBLYMETADATA mscor = { 4, 0, 0, 0, NULL, 0, NULL, 0, NULL, 0 };
    BYTE token[8] = { 0xb7, 0x7a, 0x5c, 0x56, 0x19, 0x34, 0xe0, 0x89 };
    mdToken impls[2];
    COR_FIELD_OFFSET offs[3];
    static const WCHAR hello[] = L"Hello";
    int constant = 42;
    BYTE ca_blob[] = { 1, 0, 3, 'a', 'b', 'c', 0, 0 };
    mdMethodDef dummy;

    hr = V(s->emit)->SetModuleProps(s->emit, L"emitted.dll");
    ok(hr == S_OK, "SetModuleProps %lx\n", hr);
    hr = V(s->aemit)->DefineAssembly(s->aemit, NULL, 0, 0x8004, L"Emitted", &am, 0, &dummy);
    ok(hr == S_OK && dummy == 0x20000001, "DefineAssembly %lx %x\n", hr, dummy);
    hr = V(s->aemit)->DefineAssembly(s->aemit, NULL, 0, 0x8004, L"Again", &am, 0, &dummy);
    ok(FAILED(hr), "second DefineAssembly succeeded\n");
    hr = V(s->aemit)->DefineAssemblyRef(s->aemit, token, 8, L"mscorlib", &mscor, NULL, 0, 0, &t->mscorlib);
    ok(hr == S_OK && t->mscorlib == 0x23000001, "DefineAssemblyRef %lx %x\n", hr, t->mscorlib);
    hr = V(s->aemit)->DefineAssemblyRef(s->aemit, token, 8, L"mscorlib", &mscor, NULL, 0, 0, &dummy);
    ok(hr == S_OK && dummy == t->mscorlib, "DefineAssemblyRef twice gave %x\n", dummy);
    hr = V(s->emit)->DefineTypeRefByName(s->emit, t->mscorlib, L"System.Object", &t->object_ref);
    ok(hr == S_OK && t->object_ref == 0x01000001, "TypeRef object %lx %x\n", hr, t->object_ref);
    hr = V(s->emit)->DefineTypeRefByName(s->emit, t->mscorlib, L"System.ValueType", &t->vt_ref);
    ok(hr == S_OK, "TypeRef valuetype %lx\n", hr);
    hr = V(s->emit)->DefineTypeRefByName(s->emit, t->mscorlib, L"System.Object", &dummy);
    ok(hr == S_OK && dummy == t->object_ref, "TypeRef twice gave %x\n", dummy);

    hr = V(s->emit)->DefineTypeDef(s->emit, L"Emit.IFoo", tdPublic_ | tdInterface_ | tdAbstract_, 0, NULL, &t->ifoo);
    ok(hr == S_OK && t->ifoo == 0x02000002, "DefineTypeDef IFoo %lx %x\n", hr, t->ifoo);
    impls[0] = t->ifoo; impls[1] = 0;
    hr = V(s->emit)->DefineTypeDef(s->emit, L"Emit.Foo", tdPublic_, t->object_ref, impls, &t->foo);
    ok(hr == S_OK && t->foo == 0x02000003, "DefineTypeDef Foo %lx %x\n", hr, t->foo);
    hr = V(s->emit)->DefineTypeDef(s->emit, L"Emit.Foo", tdPublic_, t->object_ref, NULL, &dummy);
    ok(hr == CLDB_E_RECORD_DUPLICATE, "duplicate type: %lx\n", hr);
    hr = V(s->emit)->DefineTypeDef(s->emit, L"Emit.Bar", tdPublic_, t->object_ref, impls, &t->bar);
    ok(hr == S_OK && t->bar == 0x02000004, "DefineTypeDef Bar %lx %x\n", hr, t->bar);
    hr = V(s->emit)->DefineNestedType(s->emit, L"Inner", tdNestedPublic_, t->object_ref, NULL, t->foo, &t->inner);
    ok(hr == S_OK && t->inner == 0x02000005, "DefineNestedType %lx %x\n", hr, t->inner);
    hr = V(s->emit)->DefineTypeDef(s->emit, L"Emit.Point", tdPublic_ | tdSealed_, t->vt_ref, NULL, &t->sval);
    ok(hr == S_OK && t->sval == 0x02000006, "DefineTypeDef Point %lx %x\n", hr, t->sval);
    /* Foo's interface rows are replaced: they now follow Bar's, so the table is out of order until Save sorts it */
    hr = V(s->emit)->SetTypeDefProps(s->emit, t->foo, 0xffffffff, 0xffffffff, impls);
    ok(hr == S_OK, "SetTypeDefProps %lx\n", hr);

    /* members of Foo and Bar defined in interleaved order: each type still owns its own */
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L".ctor", mdPublic_ | mdHideBySig_ | mdSpecialName_ | mdRTSpecialName_,
                                  sig_void, sizeof(sig_void), 0, 0, &t->foo_ctor);
    ok(hr == S_OK && t->foo_ctor == 0x06000001, "DefineMethod ctor %lx %x\n", hr, t->foo_ctor);
    hr = V(s->emit)->DefineMethod(s->emit, t->bar, L"N", mdPublic_, sig_void_i4, sizeof(sig_void_i4), 0x2050, 0, &t->bar_n);
    ok(hr == S_OK && t->bar_n == 0x06000002, "DefineMethod N %lx %x\n", hr, t->bar_n);
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L"A", mdPublic_ | mdVirtual_, sig_void_i4, sizeof(sig_void_i4), 0x2060, 0, &t->foo_a);
    ok(hr == S_OK, "DefineMethod A %lx\n", hr);
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L"B", mdPublic_ | mdStatic_, sig_get, sizeof(sig_get), 0, 0, &t->foo_b);
    ok(hr == S_OK, "DefineMethod B %lx\n", hr);
    hr = V(s->emit)->DefineMethod(s->emit, t->ifoo, L"Area", mdPublic_ | mdVirtual_ | mdAbstract_, sig_get, sizeof(sig_get), 0, 0, &t->area);
    ok(hr == S_OK, "DefineMethod Area %lx\n", hr);
    hr = V(s->emit)->DefineMethod(s->emit, 0, L"GlobalFn", mdPublic_ | mdStatic_, sig_get, sizeof(sig_get), 0, 0, &dummy);
    ok(hr == S_OK, "DefineMethod global %lx\n", hr);
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L"get_Id", mdPublic_ | mdSpecialName_, sig_get, sizeof(sig_get), 0, 0, &t->getid);
    ok(hr == S_OK, "DefineMethod get_Id %lx\n", hr);
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L"set_Id", mdPublic_ | mdSpecialName_, sig_void_i4, sizeof(sig_void_i4), 0, 0, &t->setid);
    ok(hr == S_OK, "DefineMethod set_Id %lx\n", hr);
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L"add_Changed", mdPublic_ | mdSpecialName_, sig_void_i4, sizeof(sig_void_i4), 0, 0, &t->add);
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L"remove_Changed", mdPublic_ | mdSpecialName_, sig_void_i4, sizeof(sig_void_i4), 0, 0, &t->rem);
    hr = V(s->emit)->DefineMethod(s->emit, t->foo, L"Tick", mdPublic_ | mdStatic_, sig_get, sizeof(sig_get), 0, 0, &t->pinv);
    ok(hr == S_OK, "DefineMethod Tick %lx\n", hr);

    hr = V(s->emit)->DefineParam(s->emit, t->foo_a, 1, L"x", 0, ELEMENT_TYPE_END, NULL, 0, &t->p1);
    ok(hr == S_OK, "DefineParam %lx\n", hr);
    hr = V(s->emit)->DefineParam(s->emit, t->bar_n, 1, L"count", 0x1000, ELEMENT_TYPE_I4, &constant, 0, &dummy);
    ok(hr == S_OK, "DefineParam default %lx\n", hr);

    hr = V(s->emit)->DefineField(s->emit, t->foo, L"count", fdPublic_, sig_f_i4, sizeof(sig_f_i4), ELEMENT_TYPE_END, NULL, 0, &t->f_count);
    ok(hr == S_OK && t->f_count == 0x04000001, "DefineField count %lx %x\n", hr, t->f_count);
    hr = V(s->emit)->DefineField(s->emit, t->foo, L"Const", fdPublic_ | fdStatic_ | fdLiteral_, sig_f_i4, sizeof(sig_f_i4), ELEMENT_TYPE_I4, &constant, 0, &t->f_const);
    ok(hr == S_OK, "DefineField Const %lx\n", hr);
    hr = V(s->emit)->DefineField(s->emit, t->bar, L"S", fdPublic_ | fdStatic_ | fdLiteral_, (BYTE[]){ 6, 0x0e }, 2, ELEMENT_TYPE_STRING, hello, 5, &t->f_s);
    ok(hr == S_OK, "DefineField S %lx\n", hr);
    hr = V(s->emit)->DefineField(s->emit, t->sval, L"X", fdPublic_, sig_f_i4, sizeof(sig_f_i4), ELEMENT_TYPE_END, NULL, 0, &t->sv_x);
    hr = V(s->emit)->DefineField(s->emit, t->sval, L"Y", fdPublic_, sig_f_i4, sizeof(sig_f_i4), ELEMENT_TYPE_END, NULL, 0, &t->sv_y);
    ok(hr == S_OK, "DefineField Y %lx\n", hr);
    hr = V(s->emit)->DefineField(s->emit, t->foo, L"late", fdPublic_, sig_f_i4, sizeof(sig_f_i4), ELEMENT_TYPE_END, NULL, 0, &dummy);
    ok(hr == S_OK, "DefineField late %lx\n", hr);

    offs[0].ridOfField = t->sv_y; offs[0].ulOffset = 4;
    offs[1].ridOfField = t->sv_x; offs[1].ulOffset = 0;
    offs[2].ridOfField = 0; offs[2].ulOffset = 0;
    hr = V(s->emit)->SetClassLayout(s->emit, t->sval, 4, offs, 8);
    ok(hr == S_OK, "SetClassLayout %lx\n", hr);

    hr = V(s->emit)->DefineProperty(s->emit, t->foo, L"Id", 0, sig_prop, sizeof(sig_prop), ELEMENT_TYPE_END, NULL, 0, t->setid, t->getid, NULL, &t->prop);
    ok(hr == S_OK, "DefineProperty %lx\n", hr);
    hr = V(s->emit)->DefineEvent(s->emit, t->foo, L"Changed", 0, t->object_ref, t->add, t->rem, 0, NULL, &t->ev);
    ok(hr == S_OK, "DefineEvent %lx\n", hr);

    hr = V(s->emit)->DefineModuleRef(s->emit, L"kernel32.dll", &t->kernel32);
    ok(hr == S_OK && t->kernel32 == 0x1a000001, "DefineModuleRef %lx %x\n", hr, t->kernel32);
    hr = V(s->emit)->DefinePinvokeMap(s->emit, t->pinv, 0x100 /* cc winapi */, L"GetTickCount", t->kernel32);
    ok(hr == S_OK, "DefinePinvokeMap %lx\n", hr);

    hr = V(s->emit)->DefineMemberRef(s->emit, t->object_ref, L".ctor", sig_void, sizeof(sig_void), &t->mr_ctor);
    ok(hr == S_OK && t->mr_ctor == 0x0a000001, "DefineMemberRef %lx %x\n", hr, t->mr_ctor);
    hr = V(s->emit)->DefineCustomAttribute(s->emit, t->foo, t->mr_ctor, ca_blob, sizeof(ca_blob), &t->ca);
    ok(hr == S_OK && t->ca == 0x0c000001, "DefineCustomAttribute %lx %x\n", hr, t->ca);
    hr = V(s->emit)->DefineCustomAttribute(s->emit, t->ifoo, t->mr_ctor, ca_blob, sizeof(ca_blob), &t->ca2);
    ok(hr == S_OK && t->ca2 == 0x0c000002, "second DefineCustomAttribute %lx %x\n", hr, t->ca2);
    hr = V(s->emit)->DefineMethodImpl(s->emit, t->foo, t->foo_a, t->area);
    ok(hr == S_OK, "DefineMethodImpl %lx\n", hr);
    hr = V(s->emit)->DefineUserString(s->emit, hello, 5, &t->us);
    ok(hr == S_OK && (t->us >> 24) == 0x70, "DefineUserString %lx %x\n", hr, t->us);
    return S_OK;
}

/* the saved bytes: no pointer tables, and every sorted table sorted by its key (all indexes are two bytes wide here) */
static unsigned int le16(const BYTE *p) { return p[0] | p[1] << 8; }

static void check_saved_layout(const BYTE *root, ULONG size)
{
    static const BYTE rowsize[0x2d] = { 10, 6, 14, 2, 6, 2, 14, 2, 6, 4, 6, 6, 6, 4, 6, 8, 6, 2, 4, 2, 6, 4, 2, 6, 6, 6, 2, 2, 8, 6, 8, 4,
                                        22, 4, 12, 20, 6, 14, 8, 14, 12, 4, 8, 4, 4 };
    /* table, column offset of the key, name */
    static const struct { int t, off; const char *name; } keys[] =
    {
        { 9, 0, "InterfaceImpl" }, { 0xb, 2, "Constant" }, { 0xc, 0, "CustomAttribute" }, { 0xd, 0, "FieldMarshal" },
        { 0xf, 6, "ClassLayout" }, { 0x10, 4, "FieldLayout" }, { 0x18, 4, "MethodSemantics" }, { 0x19, 0, "MethodImpl" },
        { 0x1c, 2, "ImplMap" }, { 0x29, 0, "NestedClass" },
    };
    ULONG vlen = root[12] | root[13] << 8, p, nstreams, i, off = 0, tsize = 0, rows[0x2d] = {0}, pos;
    const BYTE *tb;
    int t;
    unsigned long long valid, sorted;
    static const char *names[] = { "" };

    (void)names;
    p = 16 + vlen + 2;
    nstreams = le16(root + p);
    p += 2;
    for (i = 0; i < nstreams; i++)
    {
        ULONG o = root[p] | root[p + 1] << 8 | root[p + 2] << 16 | (ULONG)root[p + 3] << 24;
        const char *n = (const char *)root + p + 8;

        if (!strcmp(n, "#~")) { off = o; tsize = root[p + 4] | root[p + 5] << 8 | root[p + 6] << 16; }
        p += 8 + ((strlen(n) + 4) & ~3u);
    }
    ok(off && off + tsize <= size, "no #~ stream\n");
    if (!off) return;
    tb = root + off;
    ok(tb[6] == 0, "wide heaps\n");
    valid = *(const unsigned long long *)(tb + 8);
    sorted = *(const unsigned long long *)(tb + 16);
    ok(!(valid & ((1ull << 3) | (1ull << 5) | (1ull << 7) | (1ull << 0x13) | (1ull << 0x16))), "pointer tables in a saved file: %llx\n", valid);
    ok((sorted & (1ull << 0xc)) && (sorted & (1ull << 9)) && !(sorted & (1ull << 6)), "sorted mask %llx\n", sorted);
    pos = 24;
    for (t = 0; t < 0x2d; t++)
        if (valid & (1ull << t)) { rows[t] = tb[pos] | tb[pos + 1] << 8 | tb[pos + 2] << 16; pos += 4; }
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        ULONG start = pos, r, prev = 0;

        for (t = 0; t < keys[i].t; t++) start += rows[t] * rowsize[t];
        for (r = 0; r < rows[keys[i].t]; r++)
        {
            ULONG v = le16(tb + start + r * rowsize[keys[i].t] + keys[i].off);
            ok(v >= prev, "%s is not sorted at row %lu (%lu after %lu)\n", keys[i].name, r + 1, v, prev);
            prev = v;
        }
    }
}

static void get_name(IMetaDataImport2 *imp, mdTypeDef td, WCHAR *name, DWORD *flags, mdToken *ext)
{
    ULONG len;
    HRESULT hr = V(imp)->GetTypeDefProps(imp, td, name, 128, &len, flags, ext);
    ok(hr == S_OK, "GetTypeDefProps %x: %lx\n", td, hr);
}

static void verify(IMetaDataImport2 *imp, IMetaDataAssemblyImport *aimp, const tokens_t *t, const char *how)
{
    WCHAR name[128];
    DWORD flags, attr, impl, ctype, flags2;
    mdToken ext, tk[16], tk2[16], parent;
    ULONG n, i, len, cb, rva, cch, seq;
    HCORENUM e = 0;
    const BYTE *sig;
    const void *val;
    HRESULT hr;
    mdTypeDef enc, cls;
    BYTE *buf;
    COR_FIELD_OFFSET offs[4];
    DWORD pack;
    ULONG size;
    mdMethodDef setter, getter, add, rem, fire, others[4];
    DWORD semflags;

    hr = V(imp)->EnumTypeDefs(imp, &e, tk, 16, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 5, "%s: %lu typedefs\n", how, n);       /* the global type is not enumerated */
    get_name(imp, t->foo, name, &flags, &ext);
    ok(!wcscmp(name, L"Emit.Foo") && flags == tdPublic_ && ext == t->object_ref, "%s: Foo %ls %lx %x\n", how, name, flags, ext);
    get_name(imp, t->ifoo, name, &flags, &ext);
    ok(!wcscmp(name, L"Emit.IFoo") && (flags & tdInterface_) && ext == 0x02000000, "%s: IFoo %ls %lx %x\n", how, name, flags, ext);
    get_name(imp, t->inner, name, &flags, &ext);
    ok(!wcscmp(name, L"Inner"), "%s: Inner %ls\n", how, name);
    hr = V(imp)->GetNestedClassProps(imp, t->inner, &enc);
    ok(hr == S_OK && enc == t->foo, "%s: nested class %lx %x\n", how, hr, enc);

    hr = V(imp)->FindTypeDefByName(imp, L"Emit.Bar", 0, &cls);
    ok(hr == S_OK && cls == t->bar, "%s: FindTypeDefByName %lx %x\n", how, hr, cls);

    /* interface impl */
    e = 0;
    hr = V(imp)->EnumInterfaceImpls(imp, &e, t->foo, tk, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1, "%s: interface impls %lu\n", how, n);
    if (n == 1)
    {
        mdToken iface;
        hr = V(imp)->GetInterfaceImplProps(imp, tk[0], &cls, &iface);
        ok(hr == S_OK && cls == t->foo && iface == t->ifoo, "%s: iface impl %lx %x %x\n", how, hr, cls, iface);
    }

    e = 0;
    hr = V(imp)->EnumInterfaceImpls(imp, &e, t->bar, tk, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1, "%s: Bar has %lu interface impls\n", how, n);
    {
        mdToken owner, type;
        const void *blob;
        ULONG blen;

        hr = V(imp)->GetCustomAttributeProps(imp, t->ca2, &owner, &type, &blob, &blen);
        ok(hr == S_OK && owner == t->ifoo && type == t->mr_ctor, "%s: second custom attribute %lx owner %x\n", how, hr, owner);
    }

    /* methods: each type has its own, in definition order */
    e = 0;
    hr = V(imp)->EnumMethods(imp, &e, t->foo, tk, 16, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 8 && tk[0] == t->foo_ctor && tk[1] == t->foo_a && tk[2] == t->foo_b, "%s: Foo has %lu methods, first %x\n", how, n, tk[0]);
    e = 0;
    hr = V(imp)->EnumMethods(imp, &e, t->bar, tk, 16, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1 && tk[0] == t->bar_n, "%s: Bar has %lu methods\n", how, n);
    e = 0;
    hr = V(imp)->EnumMethods(imp, &e, 0x02000001, tk, 16, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1, "%s: global type has %lu methods\n", how, n);
    hr = V(imp)->GetMethodProps(imp, t->foo_a, &cls, name, 128, &len, &attr, &sig, &cb, &rva, &impl);
    ok(hr == S_OK && cls == t->foo && !wcscmp(name, L"A") && attr == (mdPublic_ | mdVirtual_) && cb == sizeof(sig_void_i4) &&
       !memcmp(sig, sig_void_i4, cb) && rva == 0x2060, "%s: GetMethodProps A: %lx %ls %lx rva %lx\n", how, hr, name, attr, rva);
    hr = V(imp)->GetMethodProps(imp, t->bar_n, &cls, name, 128, &len, &attr, &sig, &cb, &rva, &impl);
    ok(hr == S_OK && cls == t->bar && !wcscmp(name, L"N") && rva == 0x2050, "%s: GetMethodProps N: %lx\n", how, hr);
    hr = V(imp)->FindMethod(imp, t->foo, L"B", sig_get, sizeof(sig_get), &tk[0]);
    ok(hr == S_OK && tk[0] == t->foo_b, "%s: FindMethod B %lx %x\n", how, hr, tk[0]);

    /* params and their default */
    e = 0;
    hr = V(imp)->EnumParams(imp, &e, t->foo_a, tk, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1, "%s: Foo.A has %lu params\n", how, n);
    e = 0;
    hr = V(imp)->EnumParams(imp, &e, t->bar_n, tk, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1, "%s: Bar.N has %lu params\n", how, n);
    if (n == 1)
    {
        mdMethodDef pm;
        hr = V(imp)->GetParamProps(imp, tk[0], &pm, &seq, name, 128, &len, &attr, &ctype, &val, &cch);
        ok(hr == S_OK && pm == t->bar_n && seq == 1 && !wcscmp(name, L"count") && ctype == ELEMENT_TYPE_I4 &&
           val && *(const int *)val == 42 && (attr & 0x1000), "%s: GetParamProps %lx %ls ctype %lx attr %lx\n", how, hr, name, ctype, attr);
    }

    /* fields */
    e = 0;
    hr = V(imp)->EnumFields(imp, &e, t->foo, tk, 16, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 3 && tk[0] == t->f_count && tk[1] == t->f_const, "%s: Foo has %lu fields\n", how, n);
    hr = V(imp)->GetFieldProps(imp, t->f_const, &cls, name, 128, &len, &attr, &sig, &cb, &ctype, &val, &cch);
    ok(hr == S_OK && cls == t->foo && !wcscmp(name, L"Const") && (attr & fdHasDefault_) && ctype == ELEMENT_TYPE_I4 &&
       val && *(const int *)val == 42 && cb == 2 && !memcmp(sig, sig_f_i4, 2), "%s: GetFieldProps Const %lx %ls %lx ctype %lx\n", how, hr, name, attr, ctype);
    hr = V(imp)->GetFieldProps(imp, t->f_s, &cls, name, 128, &len, &attr, &sig, &cb, &ctype, &val, &cch);
    ok(hr == S_OK && cls == t->bar && ctype == ELEMENT_TYPE_STRING && cch == 5 && val && !memcmp(val, L"Hello", 10), "%s: string constant %lx ctype %lx cch %lu\n", how, hr, ctype, cch);
    hr = V(imp)->GetFieldProps(imp, t->f_count, &cls, name, 128, &len, &attr, &sig, &cb, &ctype, &val, &cch);
    ok(hr == S_OK && ctype == ELEMENT_TYPE_END && !(attr & fdHasDefault_), "%s: field without constant: ctype %lx\n", how, ctype);

    /* class layout */
    hr = V(imp)->GetClassLayout(imp, t->sval, &pack, offs, 4, &n, &size);
    ok(hr == S_OK && pack == 4 && size == 8 && n == 2, "%s: GetClassLayout %lx pack %lu size %lu n %lu\n", how, hr, pack, size, n);
    if (n == 2)
    {
        int xi = offs[0].ridOfField == t->sv_x ? 0 : 1;      /* order is not specified */
        ok(offs[xi].ridOfField == t->sv_x && offs[xi].ulOffset == 0 && offs[1 - xi].ridOfField == t->sv_y && offs[1 - xi].ulOffset == 4, "%s: field offsets\n", how);
    }

    /* property and event with their accessors */
    e = 0;
    hr = V(imp)->EnumProperties(imp, &e, t->foo, tk, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1 && tk[0] == t->prop, "%s: properties %lu\n", how, n);
    {
        ULONG ct;
        const BYTE *psig;
        ULONG pcb, nother;
        DWORD pf, pct;
        UVCP_CONSTANT pv;
        ULONG pcch;

        hr = V(imp)->GetPropertyProps(imp, t->prop, &cls, name, 128, &len, &pf, &psig, &pcb, &pct, &pv, &pcch, &setter, &getter, others, 4, &nother);
        ok(hr == S_OK && cls == t->foo && !wcscmp(name, L"Id") && setter == t->setid && getter == t->getid && nother == 0 && pcb == sizeof(sig_prop),
           "%s: GetPropertyProps %lx %ls setter %x getter %x\n", how, hr, name, setter, getter);
        (void)ct;
    }
    e = 0;
    hr = V(imp)->EnumEvents(imp, &e, t->foo, tk, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1 && tk[0] == t->ev, "%s: events %lu\n", how, n);
    {
        mdToken etype;
        ULONG nother;

        hr = V(imp)->GetEventProps(imp, t->ev, &cls, name, 128, &len, &flags2, &etype, &add, &rem, &fire, others, 4, &nother);
        ok(hr == S_OK && !wcscmp(name, L"Changed") && etype == t->object_ref && add == t->add && rem == t->rem && !fire,
           "%s: GetEventProps %lx %ls add %x rem %x\n", how, hr, name, add, rem);
    }
    hr = V(imp)->GetMethodSemantics(imp, t->getid, t->prop, &semflags);
    ok(hr == S_OK && semflags == 2, "%s: getter semantics %lx %lx\n", how, hr, semflags);

    /* pinvoke */
    {
        mdModuleRef mod;
        hr = V(imp)->GetPinvokeMap(imp, t->pinv, &flags2, name, 128, &len, &mod);
        ok(hr == S_OK && flags2 == 0x100 && !wcscmp(name, L"GetTickCount") && mod == t->kernel32, "%s: GetPinvokeMap %lx %ls\n", how, hr, name);
        hr = V(imp)->GetModuleRefProps(imp, mod, name, 128, &len);
        ok(hr == S_OK && !wcscmp(name, L"kernel32.dll"), "%s: module ref %ls\n", how, name);
    }
    hr = V(imp)->GetMethodProps(imp, t->pinv, &cls, name, 128, &len, &attr, &sig, &cb, &rva, &impl);
    ok(hr == S_OK && (attr & 0x2000), "%s: pinvoke flag %lx\n", how, attr);

    /* member ref, custom attribute, method impl, user string */
    hr = V(imp)->GetMemberRefProps(imp, t->mr_ctor, &parent, name, 128, &len, &sig, &cb);
    ok(hr == S_OK && parent == t->object_ref && !wcscmp(name, L".ctor") && cb == sizeof(sig_void), "%s: GetMemberRefProps %lx %ls\n", how, hr, name);
    {
        mdToken owner, type;
        const void *blob;
        ULONG blen;

        hr = V(imp)->GetCustomAttributeProps(imp, t->ca, &owner, &type, &blob, &blen);
        ok(hr == S_OK && owner == t->foo && type == t->mr_ctor && blen == 8 && !memcmp(blob, "\x01\x00\x03" "abc\0\0", 8), "%s: GetCustomAttributeProps %lx owner %x\n", how, hr, owner);
    }
    e = 0;
    hr = V(imp)->EnumMethodImpls(imp, &e, t->foo, tk, tk2, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1 && tk[0] == t->foo_a && tk2[0] == t->area, "%s: method impls %lu\n", how, n);
    hr = V(imp)->GetUserString(imp, t->us, name, 128, &len);
    ok(hr == S_OK && len == 5 && !wcscmp(name, L"Hello"), "%s: user string %lx %ls len %lu\n", how, hr, name, len);

    /* scope and assembly */
    {
        GUID mvid;
        mdAssembly as;
        WCHAR an[64];
        ASSEMBLYMETADATA am;
        WCHAR loc[8];
        ULONG alen;

        hr = V(imp)->GetScopeProps(imp, name, 128, &len, &mvid);
        ok(hr == S_OK && !wcscmp(name, L"emitted.dll") && !IsEqualGUID(&mvid, &GUID_NULL), "%s: GetScopeProps %lx %ls\n", how, hr, name);
        hr = V(aimp)->GetAssemblyFromScope(aimp, &as);
        ok(hr == S_OK && as == 0x20000001, "%s: GetAssemblyFromScope %lx\n", how, hr);
        memset(&am, 0, sizeof(am)); am.szLocale = loc; am.cbLocale = 8;
        hr = V(aimp)->GetAssemblyProps(aimp, as, NULL, NULL, NULL, an, 64, &alen, &am, &flags2);
        ok(hr == S_OK && !wcscmp(an, L"Emitted") && am.usMajorVersion == 1 && am.usRevisionNumber == 4, "%s: GetAssemblyProps %lx %ls\n", how, hr, an);
        {
            const void *pk;
            ULONG pkl;
            hr = V(aimp)->GetAssemblyRefProps(aimp, t->mscorlib, &pk, &pkl, an, 64, &alen, &am, NULL, NULL, &flags2);
            ok(hr == S_OK && !wcscmp(an, L"mscorlib") && pkl == 8 && am.usMajorVersion == 4, "%s: GetAssemblyRefProps %lx %ls\n", how, hr, an);
        }
    }
    (void)i; (void)buf;
}

int main(int argc, char **argv)
{
    IMetaDataDispenserEx *disp = NULL;
    IUnknown *unk = NULL;
    scope_t s = {0}, s2 = {0};
    tokens_t t, t2;
    HRESULT hr;
    DWORD size = 0;
    BYTE *buf;
    IMetaDataImport2 *saved = NULL;
    IMetaDataAssemblyImport *saved_asm = NULL;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_Disp, NULL, CLSCTX_INPROC_SERVER, &IID_Disp, (void **)&disp);
    ok(hr == S_OK, "CoCreateInstance %lx\n", hr);
    if (!disp) goto done;

    hr = V(disp)->DefineScope(disp, &CLSID_Disp, 0, &IID_Emit2_, &unk);
    ok(hr == S_OK && unk, "DefineScope %lx\n", hr);
    if (!unk) goto done;
    s.emit = (IMetaDataEmit2 *)unk;
    hr = V(unk)->QueryInterface(unk, &IID_AsmEmit_, (void **)&s.aemit);
    ok(hr == S_OK, "QI AssemblyEmit %lx\n", hr);
    hr = V(unk)->QueryInterface(unk, &IID_Import2_, (void **)&s.imp);
    ok(hr == S_OK, "QI Import2 %lx\n", hr);
    hr = V(unk)->QueryInterface(unk, &IID_AsmImport_, (void **)&s.aimp);
    ok(hr == S_OK, "QI AssemblyImport %lx\n", hr);
    {
        IMetaDataEmit *e1 = NULL;
        hr = V(unk)->QueryInterface(unk, &IID_Emit_, (void **)&e1);
        ok(hr == S_OK && e1, "QI Emit %lx\n", hr);
        if (e1) V(e1)->Release(e1);
    }
    if (!s.aemit || !s.imp || !s.aimp) goto done;

    memset(&t, 0, sizeof(t));
    map_obj.lpVtbl = &map_vtbl;
    hr = V(s.emit)->SetHandler(s.emit, (IUnknown *)&map_obj);
    ok(hr == S_OK, "SetHandler %lx\n", hr);
    build(&s, &t);
    verify(s.imp, s.aimp, &t, "live");

    /* save, reopen read-only, and compare */
    hr = V(s.emit)->GetSaveSize(s.emit, 0, &size);
    ok(hr == S_OK && size > 200, "GetSaveSize %lx %lu\n", hr, size);
    buf = malloc(size);
    hr = V(s.emit)->SaveToMemory(s.emit, buf, size);
    ok(hr == S_OK, "SaveToMemory %lx\n", hr);
    ok(nmaps > 0, "Save renumbered nothing although the members were defined out of order\n");
    {
        mdToken *src = (mdToken *)&t, *dst = (mdToken *)&t2;
        size_t k;
        for (k = 0; k < sizeof(t) / sizeof(mdToken); k++) dst[k] = mapped(src[k]);
    }
    hr = V(s.emit)->SaveToMemory(s.emit, buf, size - 1);
    ok(FAILED(hr), "SaveToMemory into a short buffer succeeded\n");
    ok(!memcmp(buf, "BSJB", 4), "no metadata root\n");
    check_saved_layout(buf, size);
    if (argc > 1)
    {
        HANDLE f = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD w;
        WriteFile(f, buf, size, &w, NULL);
        CloseHandle(f);
    }
    hr = V(disp)->OpenScopeOnMemory(disp, buf, size, 0, &IID_Import2_, &unk);
    ok(hr == S_OK, "OpenScopeOnMemory(saved) %lx\n", hr);
    if (unk)
    {
        saved = (IMetaDataImport2 *)unk;
        V(unk)->QueryInterface(unk, &IID_AsmImport_, (void **)&saved_asm);
        verify(saved, saved_asm, &t2, "saved");
        {
            IMetaDataEmit *e = NULL;
            hr = V(unk)->QueryInterface(unk, &IID_Emit_, (void **)&e);
            ok(FAILED(hr) && !e, "a read-only scope gave an emit interface\n");
        }
        V(saved_asm)->Release(saved_asm);
        V(saved)->Release(saved);
    }

    /* open the saved metadata for writing, add a type, check both old and new */
    hr = V(disp)->OpenScopeOnMemory(disp, buf, size, 1 /* ofWrite */, &IID_Emit2_, &unk);
    ok(hr == S_OK, "OpenScopeOnMemory(write) %lx\n", hr);
    if (unk)
    {
        mdTypeDef extra;
        mdMethodDef em;

        s2.emit = (IMetaDataEmit2 *)unk;
        V(unk)->QueryInterface(unk, &IID_Import2_, (void **)&s2.imp);
        V(unk)->QueryInterface(unk, &IID_AsmImport_, (void **)&s2.aimp);
        hr = V(s2.emit)->DefineTypeDef(s2.emit, L"Emit.Extra", tdPublic_, t2.object_ref, NULL, &extra);
        ok(hr == S_OK && extra == 0x02000007, "DefineTypeDef on a reopened scope %lx %x\n", hr, extra);
        /* a method for an earlier type shifts the later methods but keeps each type's members together */
        hr = V(s2.emit)->DefineMethod(s2.emit, t2.bar, L"M2", mdPublic_, sig_void, sizeof(sig_void), 0, 0, &em);
        ok(hr == S_OK && em == 0x0600000c, "late method for Bar: %lx %x\n", hr, em);
        {
            HCORENUM e = 0;
            mdMethodDef tk[16];
            ULONG n;
            WCHAR name[64];
            ULONG len;
            mdTypeDef cls;

            V(s2.imp)->EnumMethods(s2.imp, &e, t2.bar, tk, 16, &n);
            V(s2.imp)->CloseEnum(s2.imp, e);
            ok(n == 2 && tk[0] == t2.bar_n && tk[1] == em, "Bar now has %lu methods\n", n);
            hr = V(s2.imp)->GetMethodProps(s2.imp, t2.foo_a, &cls, name, 64, &len, NULL, NULL, NULL, NULL, NULL);
            ok(hr == S_OK && !wcscmp(name, L"A") && cls == t2.foo, "tokens of the saved scope moved: %ls\n", name);
            e = 0;
            V(s2.imp)->EnumMethods(s2.imp, &e, t2.foo, tk, 16, &n);
            V(s2.imp)->CloseEnum(s2.imp, e);
            ok(n == 8, "Foo still has %lu methods\n", n);
        }
        hr = V(s2.emit)->SetModuleProps(s2.emit, L"again.dll");
        ok(hr == S_OK, "SetModuleProps %lx\n", hr);
        {
            WCHAR name[64];
            ULONG len;
            V(s2.imp)->GetScopeProps(s2.imp, name, 64, &len, NULL);
            ok(!wcscmp(name, L"again.dll"), "module name %ls\n", name);
        }
        V(s2.aimp)->Release(s2.aimp);
        V(s2.imp)->Release(s2.imp);
        V(s2.emit)->Release(s2.emit);
    }
    free(buf);

    V(s.aimp)->Release(s.aimp);
    V(s.imp)->Release(s.imp);
    V(s.aemit)->Release(s.aemit);
    V(s.emit)->Release(s.emit);
    V(disp)->Release(disp);
done:
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
