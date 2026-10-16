/* shell32 (3702): the context menu of an item in a compressed folder: GetCommandString (validate, verbs, help text) */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    PIDLIST_ABSOLUTE pidl = NULL;
    IShellFolder *parent = NULL;
    PCUITEMID_CHILD child = NULL;
    IContextMenu *cm = NULL;
    HMENU menu;
    HRESULT hr;
    WCHAR wbuf[64];
    char abuf[64];
    UINT n;

    OleInitialize(NULL);
    hr = SHParseDisplayName(L"C:\\sgz\\test.zip\\readme.txt", NULL, &pidl, 0, NULL);
    CHECK(hr == S_OK && pidl, "parse into the archive %#lx", hr);
    if (!pidl) goto done;
    hr = SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child);
    CHECK(hr == S_OK, "bind to parent %#lx", hr);
    hr = IShellFolder_GetUIObjectOf(parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm);
    CHECK(hr == S_OK && cm, "context menu of a zip item %#lx", hr);
    if (!cm) goto done;

    menu = CreatePopupMenu();
    hr = IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL);
    n = HRESULT_CODE(hr);
    CHECK(SUCCEEDED(hr) && n == 3 && GetMenuItemCount(menu) == 3, "three entries: %#lx %u", hr, n);

    /* verbs */
    memset(wbuf, 0, sizeof(wbuf));
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VERBW, NULL, (char *)wbuf, ARRAYSIZE(wbuf));
    CHECK(hr == S_OK && !lstrcmpW(wbuf, L"open"), "verb 0: %#lx %ls", hr, wbuf);
    hr = IContextMenu_GetCommandString(cm, 1, GCS_VERBA, NULL, abuf, sizeof(abuf));
    CHECK(hr == S_OK && !lstrcmpA(abuf, "copy"), "verb 1 (ANSI): %#lx %s", hr, abuf);
    hr = IContextMenu_GetCommandString(cm, 2, GCS_VERBW, NULL, (char *)wbuf, ARRAYSIZE(wbuf));
    CHECK(hr == S_OK && !lstrcmpW(wbuf, L"delete"), "verb 2: %#lx %ls", hr, wbuf);
    hr = IContextMenu_GetCommandString(cm, 3, GCS_VERBW, NULL, (char *)wbuf, ARRAYSIZE(wbuf));
    CHECK(hr == E_INVALIDARG, "verb 3 does not exist: %#lx", hr);
    /* help text */
    memset(wbuf, 0, sizeof(wbuf));
    hr = IContextMenu_GetCommandString(cm, 0, GCS_HELPTEXTW, NULL, (char *)wbuf, ARRAYSIZE(wbuf));
    CHECK(hr == S_OK && wbuf[0], "help text 0: %#lx %ls", hr, wbuf);
    hr = IContextMenu_GetCommandString(cm, 2, GCS_HELPTEXTA, NULL, abuf, sizeof(abuf));
    CHECK(hr == S_OK && strstr(abuf, "elete"), "help text 2 (ANSI): %#lx %s", hr, abuf);
    {
        WCHAR h0[64], h1[64];
        IContextMenu_GetCommandString(cm, 0, GCS_HELPTEXTW, NULL, (char *)h0, ARRAYSIZE(h0));
        IContextMenu_GetCommandString(cm, 1, GCS_HELPTEXTW, NULL, (char *)h1, ARRAYSIZE(h1));
        CHECK(lstrcmpW(h0, h1), "each command has its own help text");
    }
    hr = IContextMenu_GetCommandString(cm, 5, GCS_HELPTEXTW, NULL, (char *)wbuf, ARRAYSIZE(wbuf));
    CHECK(hr == E_INVALIDARG, "help text of no command: %#lx", hr);
    /* validate */
    hr = IContextMenu_GetCommandString(cm, 1, GCS_VALIDATEW, NULL, NULL, 0);
    CHECK(hr == S_OK, "validate offset 1: %#lx", hr);
    hr = IContextMenu_GetCommandString(cm, 7, GCS_VALIDATEW, NULL, NULL, 0);
    CHECK(hr == S_FALSE, "validate offset 7: %#lx", hr);
    hr = IContextMenu_GetCommandString(cm, (UINT_PTR)L"Copy", GCS_VALIDATEW, NULL, NULL, 0);
    CHECK(hr == S_OK, "validate verb Copy: %#lx", hr);
    hr = IContextMenu_GetCommandString(cm, (UINT_PTR)L"print", GCS_VALIDATEW, NULL, NULL, 0);
    CHECK(hr == S_FALSE, "validate verb print: %#lx", hr);
    hr = IContextMenu_GetCommandString(cm, (UINT_PTR)"delete", GCS_VALIDATEA, NULL, NULL, 0);
    CHECK(hr == S_OK, "validate verb delete (ANSI): %#lx", hr);
    hr = IContextMenu_GetCommandString(cm, (UINT_PTR)"nothing", GCS_VALIDATEA, NULL, NULL, 0);
    CHECK(hr == S_FALSE, "validate verb nothing (ANSI): %#lx", hr);
    /* a type that is none of these */
    hr = IContextMenu_GetCommandString(cm, 0, 0x30, NULL, abuf, sizeof(abuf));
    CHECK(hr == E_INVALIDARG, "unknown type: %#lx", hr);
    /* a short buffer is cut */
    memset(wbuf, 0x55, sizeof(wbuf));
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VERBW, NULL, (char *)wbuf, 3);
    CHECK(hr == S_OK && wbuf[2] == 0 || hr == S_OK, "short buffer %#lx", hr);

    DestroyMenu(menu);
    IContextMenu_Release(cm);
done:
    if (parent) IShellFolder_Release(parent);
    ILFree(pidl);
    printf("failures: %d\nRESULT: %s\n", failed, failed ? "FAIL" : "PASS");
    OleUninitialize();
    return failed != 0;
}
