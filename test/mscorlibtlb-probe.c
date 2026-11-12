/* mscorlib.tlb (wine-sg 7743): the type library of mscorlib's COM-visible types is installed beside the
 * framework and registered, so LoadRegTypeLib finds it and its interfaces describe themselves.
 * Visual Studio 2022 refuses to start its shell without it ("mscorlib.tlb could not be loaded").
 * Prints ok/FAIL lines and RESULT: PASS|FAIL. */
#define COBJMACROS
#include <windows.h>
#include <oleauto.h>
#include <stdio.h>
#include <wchar.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const GUID LIBID_mscorlib = {0xbed7f4ea,0x1a96,0x11d2,{0x8f,0x08,0x00,0xa0,0xc9,0xa6,0x18,0x6d}};
static const GUID IID_IEnumerable_ = {0x496b0abe,0xcdee,0x11d3,{0x88,0xe8,0x00,0x90,0x27,0x54,0xc4,0x3a}};
static const GUID IID__Type_ = {0xbca8b44d,0xaad6,0x3a86,{0x8a,0xb7,0x03,0x34,0x9f,0x4f,0x2d,0xa2}};
static const GUID IID__AppDomain_ = {0x05f696dc,0x2b29,0x3663,{0xad,0x8b,0xc4,0x38,0x9c,0xf2,0xa7,0x13}};
static const GUID IID_IDisposable_ = {0x805d7a98,0xd4af,0x3f0f,{0x96,0x7f,0xe5,0xcf,0x45,0x31,0x2d,0x2c}};
static const GUID IID__Assembly_ = {0x17156360,0x2f1a,0x384a,{0xbc,0x52,0xfd,0xe9,0x3c,0x21,0x5c,0x5b}};

static ITypeInfo *info_of(ITypeLib *tl, const GUID *iid, const char *name, TYPEKIND kind, int min_funcs)
{
    ITypeInfo *ti = NULL;
    TYPEATTR *ta;
    BSTR n = NULL;
    HRESULT hr = ITypeLib_GetTypeInfoOfGuid(tl, iid, &ti);

    CHECK(hr == S_OK, "%s: typeinfo by its IID (hr %#lx)", name, hr);
    if (hr != S_OK) return NULL;
    ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &n, NULL, NULL, NULL);
    {
        WCHAR w[64];
        MultiByteToWideChar(CP_ACP, 0, name, -1, w, 64);
        CHECK(n && !wcscmp(n, w), "%s: named %ls", name, n ? n : L"(null)");
    }
    SysFreeString(n);
    if (ITypeInfo_GetTypeAttr(ti, &ta) == S_OK)
    {
        CHECK(ta->typekind == kind, "%s: kind %d (want %d)", name, ta->typekind, kind);
        CHECK(ta->cFuncs >= min_funcs, "%s: %u members (want >= %d)", name, ta->cFuncs, min_funcs);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    return ti;
}

int main(void)
{
    WCHAR fw[MAX_PATH], want[MAX_PATH];
    ITypeLib *tl = NULL;
    ITypeInfo *ti;
    TLIBATTR *la;
    BSTR path = NULL;
    HRESULT hr;
    DWORD attrs;

    CoInitialize(NULL);

    GetWindowsDirectoryW(fw, MAX_PATH);
    swprintf(want, MAX_PATH, L"%ls\\Microsoft.NET\\%ls\\v4.0.30319\\mscorlib.tlb", fw,
             sizeof(void *) == 8 ? L"Framework64" : L"Framework");
    attrs = GetFileAttributesW(want);
    CHECK(attrs != INVALID_FILE_ATTRIBUTES, "%ls exists", want);

    hr = QueryPathOfRegTypeLib(&LIBID_mscorlib, 2, 4, 0, &path);
    CHECK(hr == S_OK && path && !_wcsicmp(path, want), "registered 2.4 path (hr %#lx) %ls", hr, path ? path : L"(none)");
    SysFreeString(path);

    hr = LoadRegTypeLib(&LIBID_mscorlib, 2, 4, 0, &tl);
    CHECK(hr == S_OK, "LoadRegTypeLib mscorlib 2.4 (hr %#lx)", hr);
    if (hr != S_OK) goto done;

    if (ITypeLib_GetLibAttr(tl, &la) == S_OK)
    {
        CHECK(IsEqualGUID(&la->guid, &LIBID_mscorlib) && la->wMajorVerNum == 2 && la->wMinorVerNum == 4,
              "library id and version 2.4 (%u.%u)", la->wMajorVerNum, la->wMinorVerNum);
        ITypeLib_ReleaseTLibAttr(tl, la);
    }
    CHECK(ITypeLib_GetTypeInfoCount(tl) > 300, "%u typeinfos", ITypeLib_GetTypeInfoCount(tl));

    if ((ti = info_of(tl, &IID_IEnumerable_, "IEnumerable", TKIND_DISPATCH, 1)))
    {
        /* the dual interface's v-table half carries GetEnumerator as DISPID_NEWENUM */
        HREFTYPE ref;
        ITypeInfo *vt = NULL;
        FUNCDESC *fd;
        BSTR fn = NULL;
        if (ITypeInfo_GetRefTypeOfImplType(ti, -1, &ref) == S_OK) ITypeInfo_GetRefTypeInfo(ti, ref, &vt);
        CHECK(vt != NULL, "IEnumerable: dual (v-table half)");
        if (vt && ITypeInfo_GetFuncDesc(vt, 0, &fd) == S_OK)
        {
            ITypeInfo_GetDocumentation(vt, fd->memid, &fn, NULL, NULL, NULL);
            CHECK(fd->memid == DISPID_NEWENUM && fn && !wcscmp(fn, L"GetEnumerator"),
                  "IEnumerable.GetEnumerator is DISPID_NEWENUM (%ld %ls)", fd->memid, fn ? fn : L"");
            SysFreeString(fn);
            ITypeInfo_ReleaseFuncDesc(vt, fd);
        }
        if (vt) ITypeInfo_Release(vt);
        ITypeInfo_Release(ti);
    }
    if ((ti = info_of(tl, &IID__Type_, "_Type", TKIND_INTERFACE, 40)))
    {
        /* its base is stdole's IUnknown: the import of stdole2.tlb resolves */
        HREFTYPE ref;
        ITypeInfo *base = NULL;
        BSTR bn = NULL;
        if (ITypeInfo_GetRefTypeOfImplType(ti, 0, &ref) == S_OK && ITypeInfo_GetRefTypeInfo(ti, ref, &base) == S_OK)
            ITypeInfo_GetDocumentation(base, MEMBERID_NIL, &bn, NULL, NULL, NULL);
        CHECK(bn && !wcscmp(bn, L"IUnknown"), "_Type derives from IUnknown (%ls)", bn ? bn : L"none");
        SysFreeString(bn);
        if (base) ITypeInfo_Release(base);
        ITypeInfo_Release(ti);
    }
    if ((ti = info_of(tl, &IID__AppDomain_, "_AppDomain", TKIND_INTERFACE, 40))) ITypeInfo_Release(ti);
    if ((ti = info_of(tl, &IID_IDisposable_, "IDisposable", TKIND_DISPATCH, 1))) ITypeInfo_Release(ti);
    if ((ti = info_of(tl, &IID__Assembly_, "_Assembly", TKIND_DISPATCH, 10))) ITypeInfo_Release(ti);

    {
        /* names resolve case-insensitively, as a host's IsName/FindName lookups do */
        WCHAR name[] = L"ienumerator";
        ITypeInfo *found[2] = {0};
        MEMBERID ids[2];
        USHORT n = 2;
        hr = ITypeLib_FindName(tl, name, 0, found, ids, &n);
        CHECK(hr == S_OK && n >= 1, "FindName ienumerator (hr %#lx, %u found)", hr, n);
        for (int i = 0; i < n && i < 2; i++) if (found[i]) ITypeInfo_Release(found[i]);
    }
    ITypeLib_Release(tl);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
