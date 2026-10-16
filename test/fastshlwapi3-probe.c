/* fastshlwapi3-probe: SHGetViewStatePropertyBag (flag checks, per folder / all folders / inherit bags,
 * defaults, per user and all users) */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define PB_IID IID_IPropertyBag

static HRESULT (WINAPI *pGet)(PCIDLIST_ABSOLUTE, PCWSTR, DWORD, REFIID, void **);

static HRESULT get(PCIDLIST_ABSOLUTE pidl, const WCHAR *name, DWORD flags, IPropertyBag **bag)
{
    *bag = NULL;
    return pGet(pidl, name, flags, &IID_IPropertyBag, (void **)bag);
}

static HRESULT put_long(IPropertyBag *bag, const WCHAR *name, LONG v)
{
    VARIANT var;
    VariantInit(&var);
    V_VT(&var) = VT_I4;
    V_I4(&var) = v;
    return IPropertyBag_Write(bag, name, &var);
}

static HRESULT put_str(IPropertyBag *bag, const WCHAR *name, const WCHAR *s)
{
    VARIANT var;
    HRESULT hr;
    VariantInit(&var);
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString(s);
    hr = IPropertyBag_Write(bag, name, &var);
    VariantClear(&var);
    return hr;
}

static HRESULT get_long(IPropertyBag *bag, const WCHAR *name, LONG *v)
{
    VARIANT var;
    HRESULT hr;
    VariantInit(&var);
    V_VT(&var) = VT_I4;
    hr = IPropertyBag_Read(bag, name, &var, NULL);
    if (SUCCEEDED(hr)) *v = V_I4(&var);
    VariantClear(&var);
    return hr;
}

static LONG read_long(PCIDLIST_ABSOLUTE pidl, const WCHAR *bagname, DWORD flags, const WCHAR *name, HRESULT *hr)
{
    IPropertyBag *bag;
    LONG v = -999;
    HRESULT h = get(pidl, bagname, flags, &bag);
    if (SUCCEEDED(h)) { h = get_long(bag, name, &v); IPropertyBag_Release(bag); }
    if (hr) *hr = h;
    return v;
}

static void wipe(void)
{
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\Shell\\ViewState");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\Shell\\ViewState");
}

int main(void)
{
    HMODULE shlwapi = LoadLibraryA("shlwapi.dll");
    PIDLIST_ABSOLUTE win = NULL, sys = NULL, sys2 = NULL;
    WCHAR path[MAX_PATH];
    IPropertyBag *bag, *bag2;
    IUnknown *unk;
    HRESULT hr;
    LONG v;
    BOOL made = FALSE;
    VARIANT var;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    pGet = (void *)GetProcAddress(shlwapi, (LPCSTR)515);
    CHECK(pGet != NULL, "export");
    if (!pGet) goto done;
    wipe();

    GetWindowsDirectoryW(path, MAX_PATH);
    SHParseDisplayName(path, NULL, &win, 0, NULL);
    wcscat(path, L"\\system32");
    SHParseDisplayName(path, NULL, &sys, 0, NULL);
    GetWindowsDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\system");
    made = CreateDirectoryW(path, NULL);
    SHParseDisplayName(path, NULL, &sys2, 0, NULL);
    CHECK(win && sys && sys2, "pidls %p %p %p", win, sys, sys2);
    if (!win || !sys || !sys2) goto done;

    /* argument checks */
    hr = pGet(win, L"Shell", SHGVSPB_FOLDER, &IID_IPropertyBag, NULL);
    CHECK(hr == E_POINTER, "ppv NULL %08lx", hr);
    hr = pGet(win, L"Shell", SHGVSPB_FOLDER, &IID_IShellFolder, (void **)&unk);
    CHECK(hr == E_NOINTERFACE && !unk, "unsupported interface %08lx", hr);
    hr = get(win, L"Shell", SHGVSPB_PERFOLDER, &bag);
    CHECK(hr == E_INVALIDARG, "no user flag %08lx", hr);
    hr = get(win, L"Shell", SHGVSPB_PERUSER | SHGVSPB_ALLUSERS | SHGVSPB_PERFOLDER, &bag);
    CHECK(hr == E_INVALIDARG, "both user flags %08lx", hr);
    hr = get(win, L"Shell", SHGVSPB_PERUSER, &bag);
    CHECK(hr == E_INVALIDARG, "no folder scope %08lx", hr);
    hr = get(win, L"Shell", SHGVSPB_USERDEFAULTS, &bag);
    CHECK(hr == E_INVALIDARG, "ALLFOLDERS with a pidl %08lx", hr);
    hr = get(NULL, L"Shell", SHGVSPB_USERDEFAULTS | SHGVSPB_ROAM, &bag);
    CHECK(hr == E_INVALIDARG, "ALLFOLDERS with ROAM %08lx", hr);
    hr = get(NULL, L"Shell", SHGVSPB_FOLDER, &bag);
    CHECK(hr == E_INVALIDARG, "PERFOLDER without a pidl %08lx", hr);

    /* per folder values */
    hr = get(win, L"Shell", SHGVSPB_FOLDER, &bag);
    CHECK(hr == S_OK && bag, "folder bag %08lx", hr);
    if (bag)
    {
        CHECK(put_long(bag, L"Mode", 3) == S_OK, "write long");
        CHECK(put_str(bag, L"Title", L"Windows") == S_OK, "write string");
        CHECK(get_long(bag, L"Mode", &v) == S_OK && v == 3, "read long %ld", v);
        VariantInit(&var);
        V_VT(&var) = VT_BSTR;
        hr = IPropertyBag_Read(bag, L"Title", &var, NULL);
        CHECK(hr == S_OK && !wcscmp(V_BSTR(&var), L"Windows"), "read string");
        VariantClear(&var);
        VariantInit(&var);
        V_VT(&var) = VT_I4;
        hr = IPropertyBag_Read(bag, L"Missing", &var, NULL);
        CHECK(FAILED(hr), "missing value %08lx", hr);
        IPropertyBag_Release(bag);
    }
    v = read_long(win, L"Shell", SHGVSPB_FOLDER, L"Mode", &hr);
    CHECK(hr == S_OK && v == 3, "reopened bag %08lx %ld", hr, v);
    v = read_long(sys, L"Shell", SHGVSPB_FOLDER, L"Mode", &hr);
    CHECK(FAILED(hr), "another folder has its own bag %08lx %ld", hr, v);
    v = read_long(win, L"Other", SHGVSPB_FOLDER, L"Mode", &hr);
    CHECK(FAILED(hr), "another bag name %08lx %ld", hr, v);
    v = read_long(win, L"Shell", SHGVSPB_ALLUSERS | SHGVSPB_PERFOLDER, L"Mode", &hr);
    CHECK(FAILED(hr), "the all users bag is another one %08lx %ld", hr, v);

    /* a bag name with a path */
    hr = get(win, L"Shell\\{00000000-0000-0000-0000-000000000001}", SHGVSPB_FOLDER, &bag);
    CHECK(hr == S_OK && bag, "nested name %08lx", hr);
    if (bag) { put_long(bag, L"N", 11); IPropertyBag_Release(bag); }
    v = read_long(win, L"Shell\\{00000000-0000-0000-0000-000000000001}", SHGVSPB_FOLDER, L"N", &hr);
    CHECK(hr == S_OK && v == 11, "nested name read %ld", v);

    /* user defaults */
    hr = get(NULL, L"Shell", SHGVSPB_USERDEFAULTS, &bag);
    CHECK(hr == S_OK && bag, "user defaults bag %08lx", hr);
    if (bag) { put_long(bag, L"Zoom", 7); put_long(bag, L"Mode", 1); IPropertyBag_Release(bag); }
    v = read_long(sys, L"Shell", SHGVSPB_FOLDER, L"Zoom", &hr);
    CHECK(hr == S_OK && v == 7, "folder bag falls back on the user defaults %08lx %ld", hr, v);
    v = read_long(sys, L"Shell", SHGVSPB_FOLDERNODEFAULTS, L"Zoom", &hr);
    CHECK(FAILED(hr), "NOAUTODEFAULTS: no fallback %08lx %ld", hr, v);
    v = read_long(win, L"Shell", SHGVSPB_FOLDER, L"Mode", &hr);
    CHECK(hr == S_OK && v == 3, "the folder's own value wins %ld", v);

    /* global defaults */
    hr = get(NULL, L"Shell", SHGVSPB_GLOBALDEFAULTS, &bag);
    CHECK(hr == S_OK && bag, "global defaults bag %08lx", hr);
    if (bag) { put_long(bag, L"Glob", 5); put_long(bag, L"Zoom", 99); IPropertyBag_Release(bag); }
    v = read_long(sys, L"Shell", SHGVSPB_FOLDER, L"Glob", &hr);
    CHECK(hr == S_OK && v == 5, "folder bag falls back on the global defaults %08lx %ld", hr, v);
    v = read_long(sys, L"Shell", SHGVSPB_FOLDER, L"Zoom", &hr);
    CHECK(hr == S_OK && v == 7, "user defaults before global ones %ld", v);
    v = read_long(NULL, L"Shell", SHGVSPB_USERDEFAULTS, L"Glob", &hr);
    CHECK(FAILED(hr), "the user defaults bag has no fallback %08lx", hr);

    /* inherit: the bag of the parent for its subfolders */
    hr = get(win, L"Shell", SHGVSPB_PERUSER | SHGVSPB_INHERIT, &bag);
    CHECK(hr == S_OK && bag, "inherit bag %08lx", hr);
    if (bag) { put_long(bag, L"Sort", 4); put_long(bag, L"Zoom", 8); IPropertyBag_Release(bag); }
    v = read_long(sys, L"Shell", SHGVSPB_FOLDER, L"Sort", &hr);
    CHECK(hr == S_OK && v == 4, "subfolder inherits %08lx %ld", hr, v);
    v = read_long(sys, L"Shell", SHGVSPB_FOLDER, L"Zoom", &hr);
    CHECK(hr == S_OK && v == 8, "inherited before the user defaults %ld", v);
    v = read_long(sys2, L"Shell", SHGVSPB_FOLDER, L"Sort", &hr);
    CHECK(hr == S_OK && v == 4, "a sibling inherits too %ld", v);
    v = read_long(win, L"Shell", SHGVSPB_FOLDER, L"Sort", &hr);
    CHECK(FAILED(hr), "the folder itself does not inherit its own bag %08lx %ld", hr, v);

    /* the two bags of the same request are different objects with the same data */
    hr = get(sys, L"Shell", SHGVSPB_FOLDER, &bag);
    hr = get(sys, L"Shell", SHGVSPB_FOLDER, &bag2);
    CHECK(bag && bag2 && bag != bag2, "separate objects");
    if (bag && bag2)
    {
        put_long(bag, L"Shared", 21);
        CHECK(get_long(bag2, L"Shared", &v) == S_OK && v == 21, "same data %ld", v);
    }
    if (bag) IPropertyBag_Release(bag);
    if (bag2) IPropertyBag_Release(bag2);

    wipe();
    if (made) RemoveDirectoryW(path);
done:
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    CoUninitialize();
    return fails != 0;
}
