/* Moniker edge cases (patches/sg/2247): Enum(NULL), MonikerCommonPrefixWith,
 * IPersist not exposed, composite QI/size/comparison data, MkParseDisplayName
 * syntax errors, pointer moniker keys. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static const GUID CLSID_CompositeMoniker = { 0x309, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static BOOL has_iface(IUnknown *u, REFIID iid)
{
    void *p = NULL;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (SUCCEEDED(hr)) IUnknown_Release((IUnknown *)p);
    return hr == S_OK;
}

static BOOL same_display(IMoniker *m, const WCHAR *expect)
{
    IBindCtx *bc;
    WCHAR *name = NULL;
    BOOL ok = FALSE;
    CreateBindCtx(0, &bc);
    if (IMoniker_GetDisplayName(m, bc, NULL, &name) == S_OK)
    {
        ok = !lstrcmpW(name, expect);
        if (!ok) printf("      display name %ls\n", name);
        CoTaskMemFree(name);
    }
    IBindCtx_Release(bc);
    return ok;
}

int main(void)
{
    IMoniker *item, *item2, *file1, *file2, *anti, *cls, *ptr, *ptr_null, *comp, *comp2, *comp_p, *m;
    IEnumMoniker *en;
    IBindCtx *bc;
    ULARGE_INTEGER size;
    IROTData *rd;
    IRunningObjectTable *rot;
    DWORD cookie, eaten;
    BYTE buf[256];
    ULONG len;
    HRESULT hr;
    IUnknown *unk;

    CoInitialize(NULL);
    CreateBindCtx(0, &bc);
    CreateItemMoniker(L"!", L"Item", &item);
    CreateItemMoniker(L"!", L"Item2", &item2);
    CreateFileMoniker(L"C:\\test.txt", &file1);
    CreateFileMoniker(L"C:\\a\\test.txt", &file2);
    CreateAntiMoniker(&anti);
    CreateClassMoniker(&CLSID_StdGlobalInterfaceTable, &cls);
    CreatePointerMoniker((IUnknown *)bc, &ptr);
    CreatePointerMoniker(NULL, &ptr_null);

    /* Enum with no output pointer */
    hr = IMoniker_Enum(item, FALSE, NULL);   CHECK(hr == E_INVALIDARG, "item enum %#lx", hr);
    hr = IMoniker_Enum(file1, FALSE, NULL);  CHECK(hr == E_INVALIDARG, "file enum %#lx", hr);
    hr = IMoniker_Enum(cls, FALSE, NULL);    CHECK(hr == E_INVALIDARG, "class enum %#lx", hr);
    en = (void *)1;
    hr = IMoniker_Enum(item, TRUE, &en);     CHECK(hr == S_OK && !en, "item enum ok %#lx %p", hr, en);

    /* MonikerCommonPrefixWith: simple x simple, NULLs */
    m = (void *)1;
    hr = MonikerCommonPrefixWith(NULL, NULL, &m);  CHECK(hr == MK_E_NOPREFIX && !m, "null,null %#lx %p", hr, m);
    m = (void *)1;
    hr = MonikerCommonPrefixWith(item, NULL, &m);  CHECK(hr == MK_E_NOPREFIX && !m, "item,null %#lx", hr);
    hr = MonikerCommonPrefixWith(item, item, &m);  CHECK(hr == MK_E_NOPREFIX, "item,item %#lx", hr);
    hr = MonikerCommonPrefixWith(file1, file2, &m);CHECK(hr == MK_E_NOPREFIX, "file,file %#lx", hr);
    m = (void *)1;
    hr = IMoniker_CommonPrefixWith(item, item2, &m);
    CHECK(hr == MK_E_NOPREFIX && !m, "item vs item2 %#lx %p", hr, m);

    /* simple x composite */
    CreateGenericComposite(file1, item, &comp);
    CreateGenericComposite(file2, item, &comp2);
    hr = MonikerCommonPrefixWith(file1, comp, &m);
    CHECK(hr == MK_S_ME && m == file1, "F x (F,I) %#lx %p", hr, m);
    if (hr == MK_S_ME) IMoniker_Release(m);
    hr = MonikerCommonPrefixWith(comp, file1, &m);
    CHECK(hr == MK_S_HIM && m == file1, "(F,I) x F %#lx", hr);
    if (hr == MK_S_HIM) IMoniker_Release(m);
    m = NULL;
    hr = MonikerCommonPrefixWith(file1, comp2, &m);
    CHECK(hr == S_OK && m && same_display(m, L"C:\\"), "F1 x (F2,I) %#lx", hr);
    if (m) IMoniker_Release(m);
    m = NULL;
    hr = MonikerCommonPrefixWith(comp2, file1, &m);
    CHECK(hr == S_OK && m && same_display(m, L"C:\\"), "(F2,I) x F1 %#lx", hr);
    if (m) IMoniker_Release(m);

    /* composite x composite */
    hr = MonikerCommonPrefixWith(comp, comp, &m);
    CHECK(hr == MK_S_US && m && m != comp && same_display(m, L"C:\\test.txt!Item"), "comp x comp %#lx", hr);
    if (m) IMoniker_Release(m);
    m = (void *)1;
    hr = MonikerCommonPrefixWith(comp, comp2, &m);
    CHECK(hr == S_OK && !m, "(F1,I) x (F2,I) %#lx %p", hr, m);
    {
        IMoniker *i1b, *i1, *i2, *i3, *i4, *c1, *c2, *c3, *c4;
        CreateItemMoniker(L"!", L"I1", &i1); CreateItemMoniker(L"!", L"I2", &i2);
        CreateItemMoniker(L"!", L"I3", &i3); CreateItemMoniker(L"!", L"I4", &i4);
        CreateGenericComposite(i2, i3, &c1); CreateGenericComposite(i1, c1, &c2);   /* (I1,(I2,I3)) */
        CreateGenericComposite(i1, i2, &c3); CreateGenericComposite(c3, i4, &c4);   /* ((I1,I2),I4) */
        hr = MonikerCommonPrefixWith(c2, c4, &m);
        CHECK(hr == S_OK && m && same_display(m, L"!I1!I2"), "flattened %#lx", hr);
        if (m) IMoniker_Release(m);
        hr = MonikerCommonPrefixWith(c3, c2, &m);   /* (I1,I2) is a prefix of (I1,I2,I3) */
        CHECK(hr == MK_S_ME, "(I1,I2) x (I1,I2,I3) %#lx", hr);
        if (SUCCEEDED(hr) && m) IMoniker_Release(m);
        hr = MonikerCommonPrefixWith(c2, c3, &m);
        CHECK(hr == MK_S_HIM, "(I1,I2,I3) x (I1,I2) %#lx", hr);
        if (SUCCEEDED(hr) && m) IMoniker_Release(m);
        /* a simple item against the composite's first component */
        CreateItemMoniker(L"!", L"I1", &i1b);
        hr = IMoniker_CommonPrefixWith(i1b, c2, &m);
        CHECK(hr == MK_S_ME && m && m != i1b && m == i1, "I1 x (I1,..) %#lx", hr);
        IMoniker_Release(i1b);
        if (SUCCEEDED(hr) && m) IMoniker_Release(m);
        IMoniker_Release(c4); IMoniker_Release(c3); IMoniker_Release(c2); IMoniker_Release(c1);
        IMoniker_Release(i4); IMoniker_Release(i3); IMoniker_Release(i2); IMoniker_Release(i1);
    }

    /* IPersist is not exposed by these, is by the class moniker */
    CHECK(!has_iface((IUnknown *)file1, &IID_IPersist), "file IPersist");
    CHECK(!has_iface((IUnknown *)anti, &IID_IPersist), "anti IPersist");
    CHECK(!has_iface((IUnknown *)comp, &IID_IPersist), "composite IPersist");
    CHECK(!has_iface((IUnknown *)ptr, &IID_IPersist), "pointer IPersist");
    CHECK(has_iface((IUnknown *)cls, &IID_IPersist), "class IPersist");
    CHECK(has_iface((IUnknown *)file1, &IID_IPersistStream), "file IPersistStream");
    CHECK(has_iface((IUnknown *)comp, &IID_IPersistStream), "composite IPersistStream");
    CHECK(has_iface((IUnknown *)comp, &CLSID_CompositeMoniker), "composite QI by CLSID");
    unk = NULL;
    hr = IMoniker_QueryInterface(comp, &CLSID_CompositeMoniker, (void **)&unk);
    CHECK(hr == S_OK && unk == (IUnknown *)comp, "composite QI is itself");
    if (unk) IUnknown_Release(unk);

    /* composite size includes its own CLSID: 4 + 2 * (item size + 16) + 16 */
    {
        IMoniker *a, *b, *c;
        ULARGE_INTEGER s1, s2;
        CreateItemMoniker(L"!", L"Test", &a); CreateItemMoniker(L"!", L"Wine", &b);
        CreateGenericComposite(a, b, &c);
        IMoniker_GetSizeMax(a, &s1); IMoniker_GetSizeMax(b, &s2);
        hr = IMoniker_GetSizeMax(c, &size);
        CHECK(hr == S_OK && size.QuadPart == 4 + s1.QuadPart + s2.QuadPart + 3 * 16, "composite size %I64u (items %I64u+%I64u)",
              size.QuadPart, s1.QuadPart, s2.QuadPart);
        IMoniker_Release(c); IMoniker_Release(b); IMoniker_Release(a);
    }

    /* comparison data of a composite with a pointer component */
    CreateGenericComposite(ptr, item, &comp_p);
    hr = IMoniker_QueryInterface(comp_p, &IID_IROTData, (void **)&rd);
    CHECK(hr == S_OK, "composite IROTData %#lx", hr);
    if (hr == S_OK)
    {
        len = 123;
        hr = IROTData_GetComparisonData(rd, buf, sizeof(buf), &len);
        CHECK(hr == E_NOTIMPL && len == 0, "composite(P,I) comparison data %#lx len %lu", hr, len);
        IROTData_Release(rd);
    }
    CreateGenericComposite(item, comp, &m);   /* all-IROTData components still work */
    hr = IMoniker_QueryInterface(m, &IID_IROTData, (void **)&rd);
    len = 0;
    hr = IROTData_GetComparisonData(rd, buf, sizeof(buf), &len);
    CHECK(hr == S_OK && len > 16, "composite(I,F,I) comparison data %#lx len %lu", hr, len);
    IROTData_Release(rd);
    IMoniker_Release(m);

    /* ROT keys: a pointer moniker holding nothing is invalid */
    GetRunningObjectTable(0, &rot);
    hr = IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, (IUnknown *)bc, ptr_null, &cookie);
    CHECK(hr == E_INVALIDARG, "register NULL pointer moniker %#lx", hr);
    hr = IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, (IUnknown *)bc, ptr, &cookie);
    CHECK(hr == E_NOTIMPL, "register pointer moniker %#lx", hr);
    IRunningObjectTable_Release(rot);

    /* MkParseDisplayName */
    eaten = 77; m = (void *)1;
    hr = MkParseDisplayName(bc, L"NonExistentProgId:", &eaten, &m);
    CHECK(hr == MK_E_SYNTAX && eaten == 0 && !m, "unknown progid %#lx", hr);
    eaten = 77; m = (void *)1;
    hr = MkParseDisplayName(bc, L"clsid:", &eaten, &m);
    CHECK(hr == MK_E_SYNTAX && eaten == 0 && !m, "clsid: %#lx", hr);
    eaten = 77; m = NULL;
    hr = MkParseDisplayName(bc, L"clsid:11111111-0000-0000-2222-444444444444", &eaten, &m);
    CHECK(hr == S_OK && eaten == 42, "clsid ok %#lx %lu", hr, eaten);
    if (m) IMoniker_Release(m);

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    CoUninitialize();
    return fails != 0;
}
