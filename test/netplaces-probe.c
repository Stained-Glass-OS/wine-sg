/* netplaces-probe: File Explorer's Network folder -- the computers, a
 * computer's shares, their names to show and to parse, and the Comments
 * column (GetDetailsOf column 1; wine-sg 1471) */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
static void list(const WCHAR *what)
{
    ITEMIDLIST *pidl; IShellFolder *desk, *f; IShellFolder2 *f2 = NULL; SHELLDETAILS sd; IEnumIDList *e; ITEMIDLIST *c; STRRET s; WCHAR name[MAX_PATH]; HRESULT hr;
    SHGetDesktopFolder(&desk);
    hr = SHParseDisplayName(what, NULL, &pidl, 0, NULL);
    printf("parse %ls: %08lx\n", what, hr);
    if (FAILED(hr)) return;
    hr = IShellFolder_BindToObject(desk, pidl, NULL, &IID_IShellFolder, (void **)&f);
    printf("  bind %08lx\n", hr);
    if (FAILED(hr)) return;
    IShellFolder_QueryInterface(f, &IID_IShellFolder2, (void **)&f2);
    hr = IShellFolder_EnumObjects(f, NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &e);
    printf("  enum %08lx\n", hr);
    if (hr != S_OK) return;
    while (IEnumIDList_Next(e, 1, &c, NULL) == S_OK)
    {
        IShellFolder_GetDisplayNameOf(f, c, SHGDN_NORMAL, &s); StrRetToBufW(&s, c, name, MAX_PATH);
        printf("    %ls", name);
        IShellFolder_GetDisplayNameOf(f, c, SHGDN_FORPARSING, &s); StrRetToBufW(&s, c, name, MAX_PATH);
        printf("  [%ls]", name);
        name[0] = 0;
        if (f2 && SUCCEEDED(IShellFolder2_GetDetailsOf(f2, c, 1, &sd))) StrRetToBufW(&sd.str, c, name, MAX_PATH);
        printf("  {%ls}\n", name);
    }
}
int main(void)
{
    CoInitialize(NULL);
    list(L"::{208D2C60-3AEA-1069-A2D7-08002B30309D}");
    list(L"\\\\SERVER1");
    return 0;
}
