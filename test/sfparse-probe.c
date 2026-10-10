/* shell32 special folder batch (patches/sg/2017), run by test/sfparse-gate.sh.
 * Families (all from Wine's todo_wine blocks, which record Windows):
 * ParseDisplayName leaves pchEaten alone and refuses names that are not file
 * names ("." / "..", doubled or leading backslashes, forward slashes); the
 * attributes of shell extensions with nothing in the registry; the attributes
 * of My Computer asked directly and through the desktop; an empty item id
 * list.
 *
 *   sfparse-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

struct pcase { const WCHAR *name; HRESULT hr; };

int main(void)
{
    static const struct pcase cases[] =
    {
        { L"c:\\", S_OK },
        { L"c:\\\\", E_INVALIDARG },
        { L"c:\\windows", S_OK },
        { L"c:\\windows\\", S_OK },
        { L"c:\\windows\\.", E_INVALIDARG },
        { L"c:\\windows\\..", E_INVALIDARG },
        { L"c:\\windows\\.\\system32", E_INVALIDARG },
        { L"c:\\windows\\system32\\..\\", E_INVALIDARG },
        { L"c:\\windows\\\\system32", E_INVALIDARG },
        { L"c:\\windows/system32", E_INVALIDARG },
        { L"c:/", E_INVALIDARG },
        { L"c:fake", E_INVALIDARG },
        { L"c:\\fake", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) },
        { L".", E_INVALIDARG },
        { L"..", E_INVALIDARG },
        { L"..\\x", E_INVALIDARG },
        { L"sub/dir", E_INVALIDARG },
        { L"test", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) },
        { L"test\\", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) },
        { L"sub\\dir", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) },
        { L".hidden", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) },
        { L"a.b.c", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) },
    };
    static WCHAR network[] = L"::{208D2C60-3AEA-1069-A2D7-08002B30309D}";
    static WCHAR mycomp[] = L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";
    static WCHAR drive[] = L"C:\\";
    IShellFolder *desktop = NULL, *mc = NULL;
    SHITEMID empty = { 0, { 0 } };
    LPCITEMIDLIST pidl_empty = (LPCITEMIDLIST)&empty;
    LPITEMIDLIST pidl, pidl_mc;
    DWORD eaten, attr;
    HRESULT hr;
    char buf[200];
    unsigned i;

    CoInitialize(NULL);
    SHGetDesktopFolder(&desktop);

    for (i = 0; i < (sizeof(cases)/sizeof(cases[0])); i++)
    {
        pidl = NULL;
        hr = IShellFolder_ParseDisplayName(desktop, NULL, NULL, (WCHAR *)cases[i].name, NULL, &pidl, NULL);
        snprintf(buf, sizeof(buf), "parse %ls (hr %08lx, want %08lx)", cases[i].name, (unsigned long)hr, (unsigned long)cases[i].hr);
        check(hr == cases[i].hr, buf);
        if (SUCCEEDED(hr)) CoTaskMemFree(pidl);
    }

    /* pchEaten is not touched */
    eaten = 0xdeadbeef;
    hr = IShellFolder_ParseDisplayName(desktop, NULL, NULL, drive, &eaten, &pidl, NULL);
    check(hr == S_OK && eaten == 0xdeadbeef, "a drive: eaten untouched");
    CoTaskMemFree(pidl);
    eaten = 0xdeadbeef;
    hr = IShellFolder_ParseDisplayName(desktop, NULL, NULL, L"c:\\windows", &eaten, &pidl, NULL);
    check(hr == S_OK && eaten == 0xdeadbeef, "a folder path: eaten untouched");
    CoTaskMemFree(pidl);
    eaten = 0xdeadbeef;
    hr = IShellFolder_ParseDisplayName(desktop, NULL, NULL, (WCHAR *)L"c:\\\\", &eaten, &pidl, NULL);
    check(hr == E_INVALIDARG && eaten == 0xdeadbeef, "a refused name: eaten untouched");
    eaten = 0xdeadbeef;
    attr = ~0u;
    hr = IShellFolder_ParseDisplayName(desktop, NULL, NULL, network, &eaten, &pidl, &attr);
    check(hr == S_OK && eaten == 0xdeadbeef, "My Network Places: eaten untouched");
    check(attr == 0xb0000154 || attr == (0xb0000154 | SFGAO_STREAM) || attr == (0xb0000154 | SFGAO_CANDELETE) ||
          attr == (0xb0000154 | SFGAO_CANDELETE | SFGAO_NONENUMERATED), "My Network Places attributes");
    CoTaskMemFree(pidl);

    /* My Computer, through the desktop and directly */
    hr = IShellFolder_ParseDisplayName(desktop, NULL, NULL, mycomp, NULL, &pidl_mc, NULL);
    check(hr == S_OK, "parse My Computer");
    attr = ~0u;
    hr = IShellFolder_GetAttributesOf(desktop, 1, (LPCITEMIDLIST *)&pidl_mc, &attr);
    check(hr == S_OK && attr == 0xb0000174, "My Computer through the desktop includes CANLINK");
    hr = IShellFolder_BindToObject(desktop, pidl_mc, NULL, &IID_IShellFolder, (void **)&mc);
    check(hr == S_OK && mc, "bind to My Computer");
    if (mc)
    {
        attr = ~0u;
        hr = IShellFolder_GetAttributesOf(mc, 0, NULL, &attr);
        check(hr == S_OK && attr == 0xb0000170, "My Computer asked directly has no CANLINK");
        attr = ~0u;
        hr = IShellFolder_GetAttributesOf(mc, 1, &pidl_empty, &attr);
        snprintf(buf, sizeof(buf), "an empty item id list is invalid (hr %08lx)", (unsigned long)hr);
        check(hr == E_INVALIDARG, buf);
        eaten = 0xdeadbeef;
        hr = IShellFolder_ParseDisplayName(mc, NULL, NULL, drive, &eaten, &pidl, NULL);
        check(hr == S_OK && eaten == 0xdeadbeef, "My Computer parsing a drive: eaten untouched");
        CoTaskMemFree(pidl);
        IShellFolder_Release(mc);
    }
    CoTaskMemFree(pidl_mc);
    IShellFolder_Release(desktop);

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
