/* SHAddToRecentDocs makes a Recent shortcut that points at the document (6112). Since 1664 every
 * path reaches the shortcut step as SHARD_PATHW, and the step gave the wide path to the ANSI
 * IShellLinkA::SetPath: every Recent\*.lnk pointed at "C:\C", and Quick access's Recent files, which
 * leaves out links to nothing, was empty. Checked for SHARD_PATHW, SHARD_PATHA and SHARD_PIDL. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void target_of(const WCHAR *name, WCHAR *target)
{
    WCHAR recent[MAX_PATH], lnk[MAX_PATH];
    IShellLinkW *link;
    IPersistFile *pf;
    int i;

    target[0] = 0;
    SHGetFolderPathW(NULL, CSIDL_RECENT, NULL, 0, recent);
    swprintf(lnk, MAX_PATH, L"%ls\\%ls.lnk", recent, name);
    for (i = 0; i < 30 && GetFileAttributesW(lnk) == INVALID_FILE_ATTRIBUTES; i++) Sleep(100);
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link))) return;
    IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&pf);
    if (SUCCEEDED(IPersistFile_Load(pf, lnk, STGM_READ)))
        IShellLinkW_GetPath(link, target, MAX_PATH, NULL, SLGP_RAWPATH);
    IPersistFile_Release(pf);
    IShellLinkW_Release(link);
}

static void make(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
}

int main(void)
{
    WCHAR dir[MAX_PATH], w[MAX_PATH], a_w[MAX_PATH], p[MAX_PATH], target[MAX_PATH];
    char a[MAX_PATH], buf[600];
    ITEMIDLIST *pidl;

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, dir);
    swprintf(w, MAX_PATH, L"%lssg-recent-wide.txt", dir);
    swprintf(a_w, MAX_PATH, L"%lssg-recent-ansi.txt", dir);
    swprintf(p, MAX_PATH, L"%lssg-recent-pidl.txt", dir);
    make(w); make(a_w); make(p);
    WideCharToMultiByte(CP_ACP, 0, a_w, -1, a, MAX_PATH, NULL, NULL);

    SHAddToRecentDocs(SHARD_PATHW, w);
    target_of(L"sg-recent-wide.txt", target);
    sprintf(buf, "SHARD_PATHW: the Recent shortcut points at the document (%ls)", target);
    check(!lstrcmpiW(target, w), buf);

    SHAddToRecentDocs(SHARD_PATHA, a);
    target_of(L"sg-recent-ansi.txt", target);
    sprintf(buf, "SHARD_PATHA: the Recent shortcut points at the document (%ls)", target);
    check(!lstrcmpiW(target, a_w), buf);

    pidl = ILCreateFromPathW(p);
    SHAddToRecentDocs(SHARD_PIDL, pidl);
    ILFree(pidl);
    target_of(L"sg-recent-pidl.txt", target);
    sprintf(buf, "SHARD_PIDL: the Recent shortcut points at the document (%ls)", target);
    check(!lstrcmpiW(target, p), buf);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
