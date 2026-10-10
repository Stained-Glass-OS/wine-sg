/* shell32 shell link stub batch (patches/sg/2009), run by test/shlinkstub-gate.sh.
 * Families: IShellLinkDataList AddDataBlock / CopyDataBlock / RemoveDataBlock
 * (the extra data blocks, kept and saved), GetFlags / SetFlags (the flags that
 * follow the members and the ones a caller sets, saved in the header),
 * IPersistFile::SaveCompleted and IContextMenu::GetCommandString.
 *
 *   shlinkstub-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <propsys.h>
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
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

#define SIG_A 0xa0000005  /* special folder */
#define SIG_B 0xa0000002  /* console properties */

typedef struct { DWORD size, sig, v[4]; } blk;

static IShellLinkW *newlink(void)
{
    IShellLinkW *sl = NULL;
    CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl);
    return sl;
}

static IShellLinkDataList *dl_of(IShellLinkW *sl)
{
    IShellLinkDataList *dl = NULL;
    IShellLinkW_QueryInterface(sl, &IID_IShellLinkDataList, (void **)&dl);
    return dl;
}

static HRESULT save(IShellLinkW *sl, const WCHAR *path)
{
    IPersistFile *pf = NULL;
    HRESULT hr;
    IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
    hr = IPersistFile_Save(pf, path, TRUE);
    IPersistFile_Release(pf);
    return hr;
}

static HRESULT load(IShellLinkW *sl, const WCHAR *path)
{
    IPersistFile *pf = NULL;
    HRESULT hr;
    IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
    hr = IPersistFile_Load(pf, path, STGM_READ);
    IPersistFile_Release(pf);
    return hr;
}

static int copy_is(IShellLinkDataList *dl, DWORD sig, const void *want, DWORD size)
{
    void *p = NULL;
    HRESULT hr = IShellLinkDataList_CopyDataBlock(dl, sig, &p);
    int ok = hr == S_OK && p && !memcmp(p, want, size);
    if (p) LocalFree(p);
    return ok;
}

static HRESULT copy_hr(IShellLinkDataList *dl, DWORD sig, int *null)
{
    void *p = (void *)-1;
    HRESULT hr = IShellLinkDataList_CopyDataBlock(dl, sig, &p);
    *null = p == NULL;
    if (p) LocalFree(p);
    return hr;
}

static DWORD file_dword(const WCHAR *path, DWORD off)
{
    DWORD v = 0, n;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0xdeadbeef;
    SetFilePointer(h, off, NULL, FILE_BEGIN);
    ReadFile(h, &v, 4, &n, NULL);
    CloseHandle(h);
    return v;
}

static DWORD file_last_dword(const WCHAR *path)
{
    DWORD v = 0xdeadbeef, n, size;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return v;
    size = GetFileSize(h, NULL);
    SetFilePointer(h, size - 4, NULL, FILE_BEGIN);
    ReadFile(h, &v, 4, &n, NULL);
    CloseHandle(h);
    return v;
}

static void test_blocks(const WCHAR *path)
{
    IShellLinkW *sl = newlink(), *sl2;
    IShellLinkDataList *dl = dl_of(sl), *dl2;
    blk a = { sizeof(blk), SIG_A, { 1, 2, 3, 4 } }, a2 = { sizeof(blk), SIG_A, { 9, 8, 7, 6 } };
    blk b = { sizeof(blk), SIG_B, { 5, 5, 5, 5 } };
    int null;
    HRESULT hr;
    DWORD flags;

    check(sl && dl, "link and data list interface");
    hr = copy_hr(dl, SIG_A, &null);
    checkhr(hr, E_FAIL, "no block yet");
    check(null, "no block yet gives NULL");
    checkhr(IShellLinkDataList_AddDataBlock(dl, NULL), E_INVALIDARG, "AddDataBlock(NULL)");
    { DWORD small[2] = { 4, SIG_A }; checkhr(IShellLinkDataList_AddDataBlock(dl, small), E_INVALIDARG, "AddDataBlock(size 4)"); }

    checkhr(IShellLinkDataList_AddDataBlock(dl, &a), S_OK, "AddDataBlock A");
    check(copy_is(dl, SIG_A, &a, sizeof(a)), "A copies back");
    checkhr(IShellLinkDataList_AddDataBlock(dl, &b), S_OK, "AddDataBlock B");
    check(copy_is(dl, SIG_B, &b, sizeof(b)), "B copies back");
    checkhr(IShellLinkDataList_AddDataBlock(dl, &a2), S_OK, "AddDataBlock A again");
    check(copy_is(dl, SIG_A, &a2, sizeof(a2)), "A replaced by the second");
    check(copy_is(dl, SIG_B, &b, sizeof(b)), "B untouched by replacing A");

    /* saved, with the terminal zero last */
    checkhr(save(sl, path), S_OK, "save with blocks");
    check(file_last_dword(path) == 0, "terminal zero dword last");
    sl2 = newlink(); dl2 = dl_of(sl2);
    checkhr(load(sl2, path), S_OK, "load with blocks");
    check(copy_is(dl2, SIG_A, &a2, sizeof(a2)), "A survives save and load");
    check(copy_is(dl2, SIG_B, &b, sizeof(b)), "B survives save and load");

    /* save the loaded link again: the blocks stay */
    checkhr(save(sl2, path), S_OK, "save the loaded link");
    IShellLinkDataList_Release(dl2); IShellLinkW_Release(sl2);
    sl2 = newlink(); dl2 = dl_of(sl2);
    checkhr(load(sl2, path), S_OK, "load again");
    check(copy_is(dl2, SIG_A, &a2, sizeof(a2)) && copy_is(dl2, SIG_B, &b, sizeof(b)), "blocks survive a second round");

    checkhr(IShellLinkDataList_RemoveDataBlock(dl2, SIG_A), S_OK, "RemoveDataBlock A");
    hr = copy_hr(dl2, SIG_A, &null);
    checkhr(hr, E_FAIL, "A gone after remove");
    check(copy_is(dl2, SIG_B, &b, sizeof(b)), "B kept after removing A");
    checkhr(IShellLinkDataList_RemoveDataBlock(dl2, SIG_A), E_FAIL, "RemoveDataBlock A twice");
    checkhr(save(sl2, path), S_OK, "save after remove");
    IShellLinkDataList_Release(dl2); IShellLinkW_Release(sl2);
    sl2 = newlink(); dl2 = dl_of(sl2);
    checkhr(load(sl2, path), S_OK, "load after remove");
    hr = copy_hr(dl2, SIG_A, &null);
    checkhr(hr, E_FAIL, "removed block stays removed");
    check(copy_is(dl2, SIG_B, &b, sizeof(b)), "the other block still there");

    /* a load drops the previous blocks */
    { IShellLinkW *plain = newlink(); IShellLinkDataList *pdl = dl_of(plain);
      WCHAR p2[MAX_PATH]; wcscpy(p2, path); wcscat(p2, L".2.lnk");
      checkhr(save(plain, p2), S_OK, "save a plain link");
      checkhr(load(sl2, p2), S_OK, "load the plain link over one with blocks");
      hr = copy_hr(dl2, SIG_B, &null);
      checkhr(hr, E_FAIL, "the load replaced the blocks");
      DeleteFileW(p2);
      IShellLinkDataList_Release(pdl); IShellLinkW_Release(plain); }

    flags = 1;
    IShellLinkDataList_GetFlags(dl2, &flags);
    check(flags == 0, "no flags on an empty link");
    IShellLinkDataList_Release(dl2); IShellLinkW_Release(sl2);
    IShellLinkDataList_Release(dl); IShellLinkW_Release(sl);
}

static void test_darwin_and_props(void)
{
    IShellLinkW *sl = newlink();
    IShellLinkDataList *dl = dl_of(sl);
    IPropertyStore *ps = NULL;
    EXP_DARWIN_LINK dar, *copy = NULL;
    DWORD flags = 0;
    int null;
    PROPERTYKEY key = { { 0x11223344, 0x5566, 0x7788, { 1, 2, 3, 4, 5, 6, 7, 8 } }, 5 };
    PROPVARIANT pv, got;
    void *pblock = NULL;
    HRESULT hr;

    memset(&dar, 0, sizeof(dar));
    dar.cbSize = sizeof(dar);
    dar.dwSignature = EXP_DARWIN_ID_SIG;
    wcscpy(dar.szwDarwinID, L"darwin-descriptor-1");
    strcpy(dar.szDarwinID, "darwin-descriptor-1");
    checkhr(IShellLinkDataList_AddDataBlock(dl, &dar), S_OK, "AddDataBlock darwin");
    IShellLinkDataList_GetFlags(dl, &flags);
    check(flags & SLDF_HAS_DARWINID, "darwin block sets the DARWINID flag");
    hr = IShellLinkDataList_CopyDataBlock(dl, EXP_DARWIN_ID_SIG, (void **)&copy);
    check(hr == S_OK && copy && !wcscmp(copy->szwDarwinID, L"darwin-descriptor-1"), "darwin block copies back");
    if (copy) LocalFree(copy);
    checkhr(IShellLinkDataList_RemoveDataBlock(dl, EXP_DARWIN_ID_SIG), S_OK, "RemoveDataBlock darwin");
    IShellLinkDataList_GetFlags(dl, &flags);
    check(!(flags & SLDF_HAS_DARWINID), "darwin flag gone after remove");
    hr = copy_hr(dl, EXP_DARWIN_ID_SIG, &null);
    checkhr(hr, E_FAIL, "darwin block gone after remove");

    /* the property storage block carries the link's properties */
    IShellLinkW_QueryInterface(sl, &IID_IPropertyStore, (void **)&ps);
    PropVariantInit(&pv);
    pv.vt = VT_UI4; pv.ulVal = 4242;
    checkhr(IPropertyStore_SetValue(ps, &key, &pv), S_OK, "set a property");
    hr = IShellLinkDataList_CopyDataBlock(dl, EXP_PROPERTYSTORAGE_SIG, &pblock);
    check(hr == S_OK && pblock && ((DWORD *)pblock)[1] == EXP_PROPERTYSTORAGE_SIG && ((DWORD *)pblock)[0] > 8,
          "property storage block copies");
    checkhr(IShellLinkDataList_RemoveDataBlock(dl, EXP_PROPERTYSTORAGE_SIG), S_OK, "RemoveDataBlock property storage");
    PropVariantInit(&got);
    IPropertyStore_GetValue(ps, &key, &got);
    check(got.vt == VT_EMPTY, "property gone after removing its block");
    hr = copy_hr(dl, EXP_PROPERTYSTORAGE_SIG, &null);
    checkhr(hr, E_FAIL, "no property block without properties");
    if (pblock)
    {
        checkhr(IShellLinkDataList_AddDataBlock(dl, pblock), S_OK, "AddDataBlock property storage");
        PropVariantInit(&got);
        IPropertyStore_GetValue(ps, &key, &got);
        check(got.vt == VT_UI4 && got.ulVal == 4242, "the property is back from its block");
        LocalFree(pblock);
    }
    IPropertyStore_Release(ps);
    IShellLinkDataList_Release(dl); IShellLinkW_Release(sl);
}

static void test_flags(const WCHAR *path)
{
    IShellLinkW *sl = newlink(), *sl2;
    IShellLinkDataList *dl = dl_of(sl), *dl2;
    DWORD flags = 0, user = SLDF_RUNAS_USER | SLDF_RUN_IN_SEPARATE | SLDF_FORCE_NO_LINKTRACK;
    HRESULT hr;

    checkhr(IShellLinkDataList_SetFlags(dl, user), S_OK, "SetFlags");
    IShellLinkDataList_GetFlags(dl, &flags);
    check(flags == user, "GetFlags returns what was set");
    IShellLinkW_SetArguments(sl, L"-x");
    IShellLinkW_SetDescription(sl, L"desc");
    IShellLinkW_SetWorkingDirectory(sl, L"C:\\");
    IShellLinkW_SetIconLocation(sl, L"C:\\x.ico", 1);
    IShellLinkDataList_GetFlags(dl, &flags);
    check(flags == (user | SLDF_HAS_ARGS | SLDF_HAS_NAME | SLDF_HAS_WORKINGDIR | SLDF_HAS_ICONLOCATION),
          "the member flags join the set ones");

    /* presence flags are not settable: they follow the members */
    IShellLinkDataList_SetFlags(dl, user | SLDF_HAS_RELPATH | SLDF_HAS_LINK_INFO);
    IShellLinkDataList_GetFlags(dl, &flags);
    check(!(flags & SLDF_HAS_RELPATH), "RELPATH not set without a relative path");
    check(flags & user, "settable flags kept");

    IShellLinkDataList_SetFlags(dl, user);
    checkhr(save(sl, path), S_OK, "save with flags");
    check((file_dword(path, 0x14) & user) == user, "flags written to the header");
    sl2 = newlink(); dl2 = dl_of(sl2);
    checkhr(load(sl2, path), S_OK, "load with flags");
    IShellLinkDataList_GetFlags(dl2, &flags);
    check((flags & user) == user, "flags survive save and load");
    IShellLinkDataList_SetFlags(dl2, 0);
    IShellLinkDataList_GetFlags(dl2, &flags);
    check(!(flags & user), "SetFlags(0) clears the settable flags");
    check(flags & SLDF_HAS_ARGS, "SetFlags(0) keeps the member flags");
    checkhr(save(sl2, path), S_OK, "save with flags cleared");
    check((file_dword(path, 0x14) & user) == 0, "header flags cleared");
    IShellLinkDataList_Release(dl2); IShellLinkW_Release(sl2);

    /* SaveCompleted */
    {
        IPersistFile *pf = NULL;
        IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
        hr = IPersistFile_SaveCompleted(pf, path);
        checkhr(hr, S_OK, "SaveCompleted");
        hr = IPersistFile_SaveCompleted(pf, NULL);
        checkhr(hr, S_OK, "SaveCompleted(NULL)");
        IPersistFile_Release(pf);
    }
    IShellLinkDataList_Release(dl); IShellLinkW_Release(sl);
}

static void test_command_string(void)
{
    IShellLinkW *sl = newlink();
    IContextMenu *cm = NULL;
    char a[32];
    WCHAR w[32];
    HRESULT hr;

    IShellLinkW_QueryInterface(sl, &IID_IContextMenu, (void **)&cm);
    check(cm != NULL, "context menu interface");
    memset(a, 'x', sizeof(a));
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VERBA, NULL, a, sizeof(a));
    check(hr == S_OK && !strcmp(a, "open"), "verb of the command, ANSI");
    memset(w, 'x', sizeof(w));
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VERBW, NULL, (char *)w, ARRAYSIZE(w));
    check(hr == S_OK && !wcscmp(w, L"open"), "verb of the command, wide");
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VERBA, NULL, a, 3);
    check(hr == S_OK && !strcmp(a, "op"), "verb truncated to the buffer");
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VALIDATEA, NULL, NULL, 0);
    checkhr(hr, S_OK, "validate the command");
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VALIDATEW, NULL, NULL, 0);
    checkhr(hr, S_OK, "validate the command, wide");
    hr = IContextMenu_GetCommandString(cm, 7, GCS_VALIDATEA, NULL, NULL, 0);
    checkhr(hr, S_FALSE, "validate a command that is not there");
    hr = IContextMenu_GetCommandString(cm, 7, GCS_VERBA, NULL, a, sizeof(a));
    checkhr(hr, E_INVALIDARG, "verb of a command that is not there");
    hr = IContextMenu_GetCommandString(cm, 0, GCS_VERBA, NULL, NULL, 0);
    checkhr(hr, E_INVALIDARG, "verb without a buffer");
    hr = IContextMenu_GetCommandString(cm, 0, 0x33, NULL, a, sizeof(a));
    checkhr(hr, E_INVALIDARG, "unknown request");
    IContextMenu_Release(cm);
    IShellLinkW_Release(sl);
}

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%ssg-shlinkstub-%lu.lnk", dir, GetCurrentProcessId());

    test_blocks(path);
    test_darwin_and_props();
    test_flags(path);
    test_command_string();

    DeleteFileW(path);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
