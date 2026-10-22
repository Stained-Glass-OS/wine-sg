/* noalias probe (patch 5955): SHGetKnownFolderIDList with KF_FLAG_NO_ALIAS gives the Desktop and Documents
 * folders on disk, not their aliases in the namespace (the root; the Documents item). ImgBurn's shell
 * component (VirtualShellTools) resolves the root through the "physical" Desktop and recursed forever when
 * that was the root again: stack overflow at start (qa4/qa5 2026-10-22). */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <stdio.h>

static int fails;
#define OK(c, ...) do { if (c) { printf("PASS  "); } else { printf("FAIL  "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void check(const KNOWNFOLDERID *id, const char *name)
{
    PIDLIST_ABSOLUTE pidl = NULL, alias = NULL;
    WCHAR path[MAX_PATH] = {0}, *want = NULL;
    HRESULT hr;

    SHGetKnownFolderPath(id, 0, NULL, &want);
    hr = SHGetKnownFolderIDList(id, KF_FLAG_NO_ALIAS, NULL, &pidl);
    OK(hr == S_OK && pidl, "%s with KF_FLAG_NO_ALIAS: %#lx", name, hr);
    if (pidl)
    {
        OK(!ILIsEmpty(pidl), "%s with KF_FLAG_NO_ALIAS is not the namespace root", name);
        SHGetPathFromIDListW(pidl, path);
        OK(want && !lstrcmpiW(path, want), "%s with KF_FLAG_NO_ALIAS is the folder on disk: '%ls' (want '%ls')", name, path, want ? want : L"?");
    }
    hr = SHGetKnownFolderIDList(id, 0, NULL, &alias);
    OK(hr == S_OK && alias, "%s without flags: %#lx", name, hr);
    if (alias && id == &FOLDERID_Desktop)
        OK(ILIsEmpty(alias), "the Desktop without flags is still the namespace root");
    if (alias && pidl) OK(!ILIsEqual(alias, pidl), "%s: the alias and the folder on disk differ", name);
    CoTaskMemFree(pidl); CoTaskMemFree(alias); CoTaskMemFree(want);
}

int main(void)
{
    CoInitialize(NULL);
    check(&FOLDERID_Desktop, "the Desktop");
    check(&FOLDERID_Documents, "Documents");
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
