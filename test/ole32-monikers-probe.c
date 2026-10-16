/* File moniker ParseDisplayName and the display name of an anti-moniker with a moniker to its left.
 * A file moniker reads the rest of a name like "file!item" by binding to the file's object and asking
 * its IParseDisplayName (here an object registered in the running object table); it used to answer
 * E_NOTIMPL, so such names could not be parsed.  An anti-moniker's name does not depend on the left. */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* the running object: IParseDisplayName that eats the whole name and makes an item moniker of it */
struct parser { IParseDisplayName IParseDisplayName_iface; LONG parse_calls; WCHAR last[64]; };
static struct parser *impl(IParseDisplayName *i) { return CONTAINING_RECORD(i, struct parser, IParseDisplayName_iface); }
static HRESULT WINAPI p_QI(IParseDisplayName *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IParseDisplayName)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI p_AddRef(IParseDisplayName *i) { return 2; }
static ULONG WINAPI p_Release(IParseDisplayName *i) { return 1; }
static HRESULT WINAPI p_Parse(IParseDisplayName *i, IBindCtx *pbc, LPOLESTR name, ULONG *eaten, IMoniker **out)
{
    struct parser *p = impl(i);
    p->parse_calls++;
    lstrcpynW(p->last, name, 64);
    *eaten = lstrlenW(name);
    return CreateItemMoniker(L"!", name + 1, out);
}
static const IParseDisplayNameVtbl p_vtbl = { p_QI, p_AddRef, p_Release, p_Parse };

int main(void)
{
    IBindCtx *bc;
    IRunningObjectTable *rot;
    IMoniker *file, *other, *out, *anti, *left;
    struct parser parser;
    DWORD cookie;
    ULONG eaten;
    HRESULT hr;
    WCHAR *name;
    char text[160];

    OleInitialize(NULL);
    parser.IParseDisplayName_iface.lpVtbl = &p_vtbl;
    parser.parse_calls = 0;
    CreateBindCtx(0, &bc);
    IBindCtx_GetRunningObjectTable(bc, &rot);

    hr = CreateFileMoniker(L"C:\\sg_monikers_test_file.doc", &file);
    check(hr == S_OK, "CreateFileMoniker");
    hr = IRunningObjectTable_Register(rot, 0, (IUnknown *)&parser.IParseDisplayName_iface, file, &cookie);
    check(hr == S_OK, "register the object in the running object table");

    eaten = 0xdeadbeef;
    out = (IMoniker *)0xdeadbeef;
    hr = IMoniker_ParseDisplayName(file, bc, NULL, (LPOLESTR)L"!Sheet1", &eaten, &out);
    sprintf(text, "ParseDisplayName hands the rest of the name to the object (hr %#lx)", hr);
    check(hr == S_OK && parser.parse_calls == 1 && !lstrcmpW(parser.last, L"!Sheet1"), text);
    sprintf(text, "it reports the characters eaten (%lu) and the moniker made (%p)", eaten, out);
    check(eaten == 7 && out && out != (IMoniker *)0xdeadbeef, text);
    if (out && out != (IMoniker *)0xdeadbeef)
    {
        DWORD kind = 0;
        IMoniker_IsSystemMoniker(out, &kind);
        check(kind == MKSYS_ITEMMONIKER, "the result is the object's moniker");
        IMoniker_Release(out);
    }

    /* through MkParseDisplayName: "file!item" is a file moniker composed with what the object parses */
    parser.parse_calls = 0;
    eaten = 0;
    out = NULL;
    hr = MkParseDisplayName(bc, L"C:\\sg_monikers_test_file.doc!Sheet1", &eaten, &out);
    sprintf(text, "MkParseDisplayName(\"file!Sheet1\") (hr %#lx, %lu eaten)", hr, eaten);
    check(hr == S_OK && parser.parse_calls == 1 && eaten == lstrlenW(L"C:\\sg_monikers_test_file.doc!Sheet1"), text);
    if (out) IMoniker_Release(out);

    IRunningObjectTable_Revoke(rot, cookie);

    /* a file that is not running and not a document: the error of binding, not E_NOTIMPL */
    parser.parse_calls = 0;
    eaten = 0xdeadbeef;
    out = (IMoniker *)0xdeadbeef;
    hr = IMoniker_ParseDisplayName(file, bc, NULL, (LPOLESTR)L"!Sheet1", &eaten, &out);
    sprintf(text, "no object to bind to: a failure other than E_NOTIMPL (hr %#lx)", hr);
    check(FAILED(hr) && hr != E_NOTIMPL && !out && !eaten && !parser.parse_calls, text);

    hr = IMoniker_ParseDisplayName(file, bc, NULL, (LPOLESTR)L"!x", NULL, &out);
    check(hr == E_POINTER, "a NULL eaten pointer is E_POINTER");

    /* the anti-moniker */
    hr = CreateAntiMoniker(&anti);
    check(hr == S_OK, "CreateAntiMoniker");
    hr = IMoniker_GetDisplayName(anti, bc, NULL, &name);
    check(hr == S_OK && !lstrcmpW(name, L"\\.."), "anti-moniker name without a left moniker");
    CoTaskMemFree(name);
    CreateItemMoniker(L"!", L"left", &left);
    name = NULL;
    hr = IMoniker_GetDisplayName(anti, bc, left, &name);
    sprintf(text, "anti-moniker name with a left moniker (hr %#lx)", hr);
    check(hr == S_OK && name && !lstrcmpW(name, L"\\.."), text);
    CoTaskMemFree(name);
    IMoniker_Release(left);
    other = NULL;
    hr = IMoniker_GetDisplayName(anti, bc, file, NULL);
    check(hr == E_POINTER, "a NULL name pointer is still E_POINTER");
    IMoniker_Release(anti);
    IMoniker_Release(file);
    IRunningObjectTable_Release(rot);
    IBindCtx_Release(bc);
    OleUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
