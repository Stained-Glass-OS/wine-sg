/* mscoree metadata entry points: GetAssemblyMDImport and the CorMetaDataDispenser class, over a
 * fixture built with Wine Mono's mcs (test/net/sgmeta.cs). Usage: probe FIXTURE.dll */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <objbase.h>
#include <cor.h>

static int failures, checks;
#define ok(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); } } while (0)
#define V(p) ((p)->lpVtbl)

static const GUID IID_IMetaDataImport_ = {0x7dac8207,0xd3ae,0x4c75,{0x9b,0x67,0x92,0x80,0x1a,0x49,0x7d,0x44}};
static const GUID IID_IMetaDataAssemblyImport_ = {0xee62470b,0xe94b,0x424e,{0x9b,0x7c,0x2f,0x00,0xc9,0x24,0x9f,0x93}};
static const GUID CLSID_Disp = {0xe5cb7a31,0x7512,0x11d2,{0x89,0xce,0x00,0x80,0xc7,0x92,0xe5,0xd8}};
static const GUID IID_Disp = {0x809c652e,0x7396,0x11d2,{0x97,0x71,0x00,0xa0,0xc9,0xb4,0xd5,0x0c}};

static HRESULT (WINAPI *pGetAssemblyMDImport)(LPCWSTR, REFIID, IUnknown **);

static void check_scope(IMetaDataImport *imp, const char *how)
{
    HCORENUM e = 0;
    mdTypeDef td[16], circle = 0, ishape = 0;
    ULONG n = 0, i, len;
    WCHAR name[128];
    DWORD flags;
    mdToken ext;
    HRESULT hr;

    hr = V(imp)->EnumTypeDefs(imp, &e, td, 16, &n);
    ok(hr == S_OK, "%s: EnumTypeDefs %lx\n", how, hr);
    V(imp)->CloseEnum(imp, e);
    ok(n == 8, "%s: %lu typedefs\n", how, n);
    for (i = 0; i < n; i++)
    {
        hr = V(imp)->GetTypeDefProps(imp, td[i], name, 128, &len, &flags, &ext);
        ok(hr == S_OK, "%s: GetTypeDefProps %lx\n", how, hr);
        if (!wcscmp(name, L"SgMeta.Circle")) circle = td[i];
        if (!wcscmp(name, L"SgMeta.IShape")) { ishape = td[i]; ok(flags & tdInterface, "%s: IShape flags %lx\n", how, flags); }
    }
    ok(circle && ishape, "%s: types found\n", how);
    hr = V(imp)->FindTypeDefByName(imp, L"SgMeta.Circle", 0, &td[0]);
    ok(hr == S_OK && td[0] == circle, "%s: FindTypeDefByName %lx\n", how, hr);
    e = 0;
    hr = V(imp)->EnumMethods(imp, &e, circle, (mdMethodDef *)td, 16, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n >= 8, "%s: Circle has %lu methods\n", how, n);
    hr = V(imp)->FindMethod(imp, circle, L"Area", NULL, 0, &td[0]);
    ok(hr == S_OK, "%s: FindMethod Area %lx\n", how, hr);
    e = 0;
    hr = V(imp)->EnumInterfaceImpls(imp, &e, circle, (mdInterfaceImpl *)td, 4, &n);
    V(imp)->CloseEnum(imp, e);
    ok(hr == S_OK && n == 1, "%s: interface impls %lu\n", how, n);
}

int main(int argc, char **argv)
{
    WCHAR path[MAX_PATH];
    IMetaDataImport *imp = NULL;
    IMetaDataAssemblyImport *asmimp = NULL;
    IMetaDataDispenserEx *disp = NULL;
    IUnknown *unk = NULL;
    HMODULE mod;
    HRESULT hr;
    ULONG ver[4];
    DWORD flags;
    HANDLE file;
    BYTE *buf;
    DWORD size, read;
    mdAssembly as;
    WCHAR name[64];
    ULONG len;
    ASSEMBLYMETADATA md;
    WCHAR loc[8];

    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, path, MAX_PATH);

    mod = LoadLibraryA("mscoree.dll");
    pGetAssemblyMDImport = (void *)GetProcAddress(mod, "GetAssemblyMDImport");
    ok(pGetAssemblyMDImport != NULL, "no GetAssemblyMDImport\n");
    if (!pGetAssemblyMDImport) goto done;

    hr = pGetAssemblyMDImport(path, &IID_IMetaDataImport_, &unk);
    ok(hr == S_OK && unk, "GetAssemblyMDImport %lx\n", hr);
    if (unk)
    {
        check_scope((IMetaDataImport *)unk, "GetAssemblyMDImport");
        hr = (unk)->lpVtbl->QueryInterface(unk, &IID_IMetaDataAssemblyImport_, (void **)&asmimp);
        ok(hr == S_OK, "QI assembly import %lx\n", hr);
        if (asmimp)
        {
            hr = V(asmimp)->GetAssemblyFromScope(asmimp, &as);
            ok(hr == S_OK && as == 0x20000001, "GetAssemblyFromScope %lx %lx\n", hr, as);
            memset(&md, 0, sizeof(md));
            md.szLocale = loc; md.cbLocale = 8;
            hr = V(asmimp)->GetAssemblyProps(asmimp, as, NULL, NULL, NULL, name, 64, &len, &md, &flags);
            ok(hr == S_OK && !wcscmp(name, L"sgmeta"), "assembly name %ls %lx\n", name, hr);
            ok(md.usMajorVersion == 1 && md.usMinorVersion == 2 && md.usBuildNumber == 3 && md.usRevisionNumber == 4,
               "version %u.%u.%u.%u\n", md.usMajorVersion, md.usMinorVersion, md.usBuildNumber, md.usRevisionNumber);
            V(asmimp)->Release(asmimp);
        }
        (unk)->lpVtbl->Release(unk);
    }

    unk = (IUnknown *)(ULONG_PTR)1;
    hr = pGetAssemblyMDImport(L"C:\\nonexistent\\nothing.dll", &IID_IMetaDataImport_, &unk);
    ok(FAILED(hr) && !unk, "missing file: %lx %p\n", hr, unk);
    unk = (IUnknown *)(ULONG_PTR)1;
    hr = pGetAssemblyMDImport(path, &IID_IStream, &unk);
    ok(hr == E_NOINTERFACE && !unk, "bad iid: %lx %p\n", hr, unk);

    /* through the class */
    hr = CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_Disp, NULL, CLSCTX_INPROC_SERVER, &IID_Disp, (void **)&unk);
    ok(hr == S_OK, "CoCreateInstance dispenser %lx\n", hr);
    if (unk)
    {
        hr = (unk)->lpVtbl->QueryInterface(unk, &IID_IMetaDataDispenserEx, (void **)&disp);
        ok(hr == S_OK, "QI Ex %lx\n", hr);
        (unk)->lpVtbl->Release(unk);
    }
    if (disp)
    {
        IUnknown *scope = NULL;

        hr = V(disp)->OpenScope(disp, path, 0, &IID_IMetaDataImport_, &scope);
        ok(hr == S_OK, "OpenScope %lx\n", hr);
        if (scope) { check_scope((IMetaDataImport *)scope, "OpenScope"); (scope)->lpVtbl->Release(scope); }

        file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        size = GetFileSize(file, NULL);
        buf = malloc(size);
        ReadFile(file, buf, size, &read, NULL);
        CloseHandle(file);
        scope = NULL;
        hr = V(disp)->OpenScopeOnMemory(disp, buf, size, 0, &IID_IMetaDataImport_, &scope);
        ok(hr == S_OK, "OpenScopeOnMemory %lx\n", hr);
        if (scope) { check_scope((IMetaDataImport *)scope, "OpenScopeOnMemory"); (scope)->lpVtbl->Release(scope); }
        free(buf);
        V(disp)->Release(disp);
    }
    (void)ver;
done:
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
