/* syncroot-probe: Windows.Storage.Provider.StorageProviderSyncRootManager (patches/sg/6222).
 *
 * The class was missing: OneDrive registers its folder as a sync root through it (then connects
 * with cldapi's CfConnectSyncRoot) and could not. A root registered here is kept where cldapi
 * keeps its own and reads back by id, by folder (a folder under it too) and in the current list;
 * CfConnectSyncRoot accepts it; Unregister removes it.
 *
 * Prints name=value lines; see test/syncroot-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_Statics,  0x3e99fbbf, 0x8fe3, 0x4b40, 0xab, 0xc7, 0xf6, 0xfc, 0x3d, 0x74, 0xc9, 0x8e);
DEFINE_GUID(IID_Statics2, 0xefb6cfee, 0x1374, 0x544e, 0x9d, 0xf1, 0x55, 0x98, 0xd2, 0xe9, 0xcf, 0xdd);
DEFINE_GUID(IID_Info,     0x7c1305c4, 0x99f9, 0x41ac, 0x89, 0x04, 0xab, 0x05, 0x5d, 0x65, 0x49, 0x26);
DEFINE_GUID(IID_Info2,    0xcf51b023, 0x7cf1, 0x5166, 0xbd, 0xba, 0xef, 0xd9, 0x5f, 0x52, 0x9e, 0x31);
DEFINE_GUID(IID_PropDef,  0xc5b383bb, 0xff1f, 0x4298, 0x83, 0x1e, 0xff, 0x1c, 0x08, 0x08, 0x96, 0x90);
DEFINE_GUID(IID_FolderStatics, 0x08f327ff, 0x85d5, 0x48b9, 0xae, 0xe9, 0x28, 0x51, 0x1e, 0x33, 0x9f, 0x9f);
DEFINE_GUID(IID_BufferFactory, 0x71af914d, 0xc10f, 0x484b, 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27);
DEFINE_GUID(IID_ByteAccess, 0x905a0fef, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(IID_Item,     0x4207a996, 0xca2f, 0x42f7, 0xbd, 0xe8, 0x8b, 0x10, 0x45, 0x7a, 0x7f, 0x30);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *putstr_fn)(obj *, HSTRING);
typedef HRESULT (WINAPI *getstr_fn)(obj *, HSTRING *);
typedef HRESULT (WINAPI *u32_fn)(obj *, UINT32 *);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *activate(const WCHAR *name)
{
    IInspectable *o = NULL;
    HRESULT hr = RoActivateInstance(hs(name), &o);
    if (FAILED(hr)) printf("activate %ls=%08lx\n", name, hr);
    return (obj *)o;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static const WCHAR *txt(HSTRING s) { const WCHAR *p = WindowsGetStringRawBuffer(s, NULL); return p ? p : L""; }

static obj *folder(obj *fs, const WCHAR *path)
{
    obj *op = NULL, *f = NULL;
    CALL(fs, 6, HRESULT (WINAPI *)(obj *, HSTRING, obj **))(fs, hs(path), &op);
    if (op) CALL(op, 8, get_fn)(op, &f);   /* GetResults: completed at once */
    return f;
}

typedef struct { DWORD type; void *callback; } cb_reg;

int main(void)
{
    obj *st, *st2, *fs, *bf, *info, *info2, *def, *defs = NULL, *buf = NULL, *ba, *got = NULL, *view = NULL, *sub;
    static const GUID provider = { 0x12345678, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    WCHAR value[MAX_PATH];
    HSTRING s = NULL;
    boolean supported = 0;
    BYTE *bytes = NULL;
    UINT32 n = 0;
    DWORD size;
    HRESULT hr;
    HMODULE cld;
    LONGLONG key = 0;
    cb_reg callbacks[] = { { 0xffffffff, NULL } };
    HRESULT (WINAPI *pCfConnectSyncRoot)(const WCHAR *, const void *, const void *, DWORD, LONGLONG *);
    HRESULT (WINAPI *pCfDisconnectSyncRoot)(LONGLONG);

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_MULTITHREADED);
    CreateDirectoryW(L"C:\\sync", NULL);
    CreateDirectoryW(L"C:\\sync\\sub", NULL);
    if (!(st = factory(L"Windows.Storage.Provider.StorageProviderSyncRootManager", &IID_Statics))) return 1;
    st2 = as(st, &IID_Statics2);
    if (st2) CALL(st2, 6, HRESULT (WINAPI *)(obj *, boolean *))(st2, &supported);
    printf("supported=%d\n", supported);

    fs = factory(L"Windows.Storage.StorageFolder", &IID_FolderStatics);
    bf = factory(L"Windows.Storage.Streams.Buffer", &IID_BufferFactory);
    info = as(activate(L"Windows.Storage.Provider.StorageProviderSyncRootInfo"), &IID_Info);
    info2 = as(info, &IID_Info2);
    def = as(activate(L"Windows.Storage.Provider.StorageProviderItemPropertyDefinition"), &IID_PropDef);
    if (!info || !info2 || !def || !fs || !bf) { printf("objects=0\n"); return 1; }
    CALL(info, 7, putstr_fn)(info, hs(L"Probe!S-1-5-21!Account1"));
    CALL(info, 13, putstr_fn)(info, hs(L"Probe Cloud"));
    CALL(info, 29, putstr_fn)(info, hs(L"1.2.3"));
    CALL(info, 11, putobj_fn)(info, folder(fs, L"C:\\sync"));
    CALL(bf, 6, HRESULT (WINAPI *)(obj *, UINT32, obj **))(bf, 4, &buf);
    ba = as(buf, &IID_ByteAccess);
    CALL(ba, 3, HRESULT (WINAPI *)(obj *, BYTE **))(ba, &bytes);
    memcpy(bytes, "ctx!", 4);
    CALL(buf, 8, HRESULT (WINAPI *)(obj *, UINT32))(buf, 4);
    CALL(info, 9, putobj_fn)(info, buf);
    CALL(info2, 7, HRESULT (WINAPI *)(obj *, GUID))(info2, provider);
    CALL(def, 7, HRESULT (WINAPI *)(obj *, INT32))(def, 7);
    CALL(def, 9, putstr_fn)(def, hs(L"Status"));
    CALL(info, 34, get_fn)(info, &defs);
    CALL(defs, 13, putobj_fn)(defs, def);   /* Append */
    hr = CALL(st, 6, putobj_fn)(st, info);   /* Register */
    printf("register=%08lx\n", hr);

    /* read back by id */
    hr = CALL(st, 9, HRESULT (WINAPI *)(obj *, HSTRING, obj **))(st, hs(L"Probe!S-1-5-21!Account1"), &got);
    if (SUCCEEDED(hr) && got)
    {
        obj *gdefs = NULL, *gdef = NULL, *ctx = NULL, *path = NULL, *item;
        INT32 id = -1;
        UINT32 len = 0;
        CALL(got, 12, getstr_fn)(got, &s);
        printf("byid=%ls", txt(s));
        CALL(got, 34, get_fn)(got, &gdefs);
        if (gdefs) CALL(gdefs, 7, u32_fn)(gdefs, &n);
        if (gdefs && n) CALL(gdefs, 6, HRESULT (WINAPI *)(obj *, UINT32, obj **))(gdefs, 0, &gdef);
        if (gdef) CALL(gdef, 6, HRESULT (WINAPI *)(obj *, INT32 *))(gdef, &id);
        CALL(got, 8, get_fn)(got, &ctx);
        if (ctx) CALL(ctx, 7, u32_fn)(ctx, &len);
        printf(",defs=%u:%d,context=%u", n, id, len);
        CALL(got, 10, get_fn)(got, &path);
        if (path && (item = as(path, &IID_Item))) { CALL(item, 12, getstr_fn)(item, &s); printf(",path=%ls", txt(s)); }
        printf("\n");
    }
    else printf("byid=%08lx\n", hr);

    /* by a folder under it */
    got = NULL;
    sub = folder(fs, L"C:\\sync\\sub");
    hr = CALL(st, 8, HRESULT (WINAPI *)(obj *, obj *, obj **))(st, sub, &got);
    s = NULL;
    if (got) CALL(got, 6, getstr_fn)(got, &s);
    printf("byfolder=%08lx,%ls\n", hr, txt(s));
    got = NULL;
    hr = CALL(st, 8, HRESULT (WINAPI *)(obj *, obj *, obj **))(st, folder(fs, L"C:\\windows"), &got);
    printf("outside=%08lx\n", hr);

    CALL(st, 10, get_fn)(st, &view);
    n = 0;
    if (view) CALL(view, 7, u32_fn)(view, &n);
    printf("current=%u\n", n);

    /* where cldapi reads it, and cldapi connects to it */
    size = sizeof(value);
    value[0] = 0;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SyncRootManager\\Probe!S-1-5-21!Account1",
                 L"Path", RRF_RT_REG_SZ, NULL, value, &size);
    printf("regpath=%ls\n", value);
    cld = LoadLibraryW(L"cldapi.dll");
    pCfConnectSyncRoot = (void *)GetProcAddress(cld, "CfConnectSyncRoot");
    pCfDisconnectSyncRoot = (void *)GetProcAddress(cld, "CfDisconnectSyncRoot");
    hr = pCfConnectSyncRoot ? pCfConnectSyncRoot(L"C:\\sync", callbacks, NULL, 0, &key) : E_FAIL;
    printf("connect=%08lx\n", hr);
    if (SUCCEEDED(hr)) pCfDisconnectSyncRoot(key);

    hr = CALL(st, 7, putstr_fn)(st, hs(L"Probe!S-1-5-21!Account1"));
    got = NULL;
    printf("unregister=%08lx,%08lx\n", hr, CALL(st, 9, HRESULT (WINAPI *)(obj *, HSTRING, obj **))(st, hs(L"Probe!S-1-5-21!Account1"), &got));
    printf("done=1\n");
    return 0;
}
