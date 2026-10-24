/* shelllibrary-probe.c: IShellLibrary (CLSID_ShellLibrary), wine-sg 6081.
 * Prints KEY=VALUE lines that test/shelllibrary-gate.sh checks. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <stdio.h>

/* not in every SDK's headers */
DEFINE_GUID(SG_FOLDERTYPEID_Music, 0x94d6ddcc, 0x4a68, 0x4175, 0xa3, 0x74, 0xbd, 0x58, 0x4a, 0x51, 0x0b, 0x78);
DEFINE_GUID(SG_FOLDERTYPEID_Pictures, 0xb3690e58, 0xe961, 0x423b, 0xb6, 0x87, 0x38, 0x6e, 0xbf, 0xd8, 0x32, 0x39);

static WCHAR work[MAX_PATH];

static void say(const char *key, const char *fmt, ...)
{
    va_list args;
    printf("%s=", key);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

static IShellLibrary *create(void)
{
    IShellLibrary *lib = NULL;
    CoCreateInstance(&CLSID_ShellLibrary, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLibrary, (void **)&lib);
    return lib;
}

static IShellItem *item(const WCHAR *path)
{
    IShellItem *item = NULL;
    SHCreateItemFromParsingName(path, NULL, &IID_IShellItem, (void **)&item);
    return item;
}

/* the folders' leaf names joined with '|' */
static void folders(IShellLibrary *lib, LIBRARYFOLDERFILTER filter, char *out, size_t size)
{
    IShellItemArray *array;
    IShellItem *it;
    DWORD count = 0, i;
    WCHAR *path;
    HRESULT hr;

    out[0] = 0;
    if (FAILED(hr = IShellLibrary_GetFolders(lib, filter, &IID_IShellItemArray, (void **)&array)))
    {
        snprintf(out, size, "error %#lx", hr);
        return;
    }
    IShellItemArray_GetCount(array, &count);
    for (i = 0; i < count; i++)
    {
        if (FAILED(IShellItemArray_GetItemAt(array, i, &it))) continue;
        if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &path)))
        {
            WCHAR *leaf = wcsrchr(path, '\\');
            char buf[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, leaf ? leaf + 1 : path, -1, buf, sizeof(buf), NULL, NULL);
            if (out[0]) strncat(out, "|", size - strlen(out) - 1);
            strncat(out, buf, size - strlen(out) - 1);
            CoTaskMemFree(path);
        }
        IShellItem_Release(it);
    }
    if (!count) snprintf(out, size, "(none)");
    IShellItemArray_Release(array);
}

static void save_folder(IShellLibrary *lib, DEFAULTSAVEFOLDERTYPE type, char *out, size_t size)
{
    IShellItem *it;
    WCHAR *path, *leaf;
    HRESULT hr;

    if (FAILED(hr = IShellLibrary_GetDefaultSaveFolder(lib, type, &IID_IShellItem, (void **)&it)))
    {
        snprintf(out, size, "error %#lx", hr);
        return;
    }
    IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &path);
    leaf = wcsrchr(path, '\\');
    WideCharToMultiByte(CP_UTF8, 0, leaf ? leaf + 1 : path, -1, out, size, NULL, NULL);
    CoTaskMemFree(path);
    IShellItem_Release(it);
}

static WCHAR *sub(const WCHAR *name)
{
    static WCHAR buf[4][MAX_PATH];
    static int n;
    WCHAR *p = buf[n++ & 3];
    swprintf(p, MAX_PATH, L"%ls\\%ls", work, name);
    return p;
}

int main(void)
{
    IShellLibrary *lib, *lib2;
    IShellItem *a, *b, *c, *saved, *dir;
    LIBRARYOPTIONFLAGS opts;
    FOLDERTYPEID type;
    WCHAR *path, *music, *icon;
    char buf[1024];
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    GetTempPathW(MAX_PATH, work);
    wcscat(work, L"shlibprobe");
    CreateDirectoryW(work, NULL);
    CreateDirectoryW(sub(L"alpha"), NULL);
    CreateDirectoryW(sub(L"beta &amp; co"), NULL);
    CreateDirectoryW(sub(L"gamma"), NULL);

    lib = create();
    say("created", "%d", lib != NULL);
    if (!lib) { say("done", "0"); return 1; }

    /* a standard library: synthesised from its known folders */
    hr = IShellLibrary_LoadLibraryFromKnownFolder(lib, &FOLDERID_MusicLibrary, STGM_READ);
    say("music_load", "%#lx", hr);
    SHGetKnownFolderPath(&FOLDERID_Music, 0, NULL, &music);
    folders(lib, LFF_FORCEFILESYSTEM, buf, sizeof(buf));
    say("music_first", "%d", !strncmp(buf, "Music", 5));
    save_folder(lib, DSFT_DETECT, buf, sizeof(buf));
    say("music_save", "%s", buf);
    IShellLibrary_GetFolderType(lib, &type);
    say("music_type", "%d", IsEqualGUID(&type, &SG_FOLDERTYPEID_Music));
    IShellLibrary_GetOptions(lib, &opts);
    say("music_pinned", "%d", opts == LOF_PINNEDTONAVPANE);
    say("music_commit_ro", "%#lx", IShellLibrary_Commit(lib));
    CoTaskMemFree(music);
    IShellLibrary_Release(lib);

    /* an unknown known folder is no library */
    lib = create();
    say("docs_folder_load", "%#lx", IShellLibrary_LoadLibraryFromKnownFolder(lib, &FOLDERID_Documents, STGM_READ));
    IShellLibrary_Release(lib);

    /* a new library */
    lib = create();
    folders(lib, LFF_ALLITEMS, buf, sizeof(buf));
    say("empty_folders", "%s", buf);
    say("empty_save", "%#lx", IShellLibrary_GetDefaultSaveFolder(lib, DSFT_DETECT, &IID_IShellItem, (void **)&saved));
    say("empty_commit", "%#lx", IShellLibrary_Commit(lib));
    a = item(sub(L"alpha"));
    b = item(sub(L"beta &amp; co"));
    c = item(sub(L"gamma"));
    say("add_a", "%#lx", IShellLibrary_AddFolder(lib, a));
    say("add_b", "%#lx", IShellLibrary_AddFolder(lib, b));
    say("add_a_again", "%#lx", IShellLibrary_AddFolder(lib, a));
    folders(lib, LFF_FORCEFILESYSTEM, buf, sizeof(buf));
    say("new_folders", "%s", buf);
    save_folder(lib, DSFT_DETECT, buf, sizeof(buf));
    say("new_save", "%s", buf);
    say("set_save_b", "%#lx", IShellLibrary_SetDefaultSaveFolder(lib, DSFT_PRIVATE, b));
    save_folder(lib, DSFT_PRIVATE, buf, sizeof(buf));
    say("new_save_b", "%s", buf);
    say("set_options_bad", "%#lx", IShellLibrary_SetOptions(lib, 0x10, 0x10));
    say("set_options", "%#lx", IShellLibrary_SetOptions(lib, LOF_MASK_ALL, LOF_PINNEDTONAVPANE));
    IShellLibrary_SetIcon(lib, L"imageres.dll,-1001");
    IShellLibrary_SetFolderType(lib, &SG_FOLDERTYPEID_Pictures);
    say("remove_c", "%#lx", IShellLibrary_RemoveFolder(lib, c));

    dir = item(work);
    hr = IShellLibrary_Save(lib, dir, L"Probe", LSF_FAILIFTHERE, &saved);
    say("save", "%#lx", hr);
    if (SUCCEEDED(hr))
    {
        IShellItem_GetDisplayName(saved, SIGDN_FILESYSPATH, &path);
        say("save_name", "%d", !wcscmp(wcsrchr(path, '\\') + 1, L"Probe.library-ms"));
        CoTaskMemFree(path);
        IShellItem_Release(saved);
    }
    say("save_again", "%#lx", IShellLibrary_Save(lib, dir, L"Probe", LSF_FAILIFTHERE, &saved));
    hr = IShellLibrary_Save(lib, dir, L"Probe", LSF_MAKEUNIQUENAME, &saved);
    if (SUCCEEDED(hr))
    {
        IShellItem_GetDisplayName(saved, SIGDN_FILESYSPATH, &path);
        WideCharToMultiByte(CP_UTF8, 0, wcsrchr(path, '\\') + 1, -1, buf, sizeof(buf), NULL, NULL);
        say("save_unique", "%s", buf);
        CoTaskMemFree(path);
        IShellItem_Release(saved);
    }
    else say("save_unique", "error %#lx", hr);
    IShellLibrary_Release(lib);

    /* load it back */
    lib2 = create();
    saved = item(sub(L"Probe.library-ms"));
    say("reload", "%#lx", IShellLibrary_LoadLibraryFromItem(lib2, saved, STGM_READWRITE));
    folders(lib2, LFF_FORCEFILESYSTEM, buf, sizeof(buf));
    say("reload_folders", "%s", buf);
    save_folder(lib2, DSFT_PRIVATE, buf, sizeof(buf));
    say("reload_save", "%s", buf);
    IShellLibrary_GetOptions(lib2, &opts);
    say("reload_pinned", "%d", opts == LOF_PINNEDTONAVPANE);
    icon = NULL;
    IShellLibrary_GetIcon(lib2, &icon);
    WideCharToMultiByte(CP_UTF8, 0, icon ? icon : L"(null)", -1, buf, sizeof(buf), NULL, NULL);
    say("reload_icon", "%s", buf);
    CoTaskMemFree(icon);
    IShellLibrary_GetFolderType(lib2, &type);
    say("reload_type", "%d", IsEqualGUID(&type, &SG_FOLDERTYPEID_Pictures));

    /* change and commit */
    say("remove_b", "%#lx", IShellLibrary_RemoveFolder(lib2, b));
    save_folder(lib2, DSFT_PRIVATE, buf, sizeof(buf));
    say("after_remove_save", "%s", buf);
    say("add_c", "%#lx", IShellLibrary_AddFolder(lib2, c));
    say("commit", "%#lx", IShellLibrary_Commit(lib2));
    IShellLibrary_Release(lib2);

    lib2 = create();
    say("reload2", "%#lx", IShellLibrary_LoadLibraryFromItem(lib2, saved, STGM_READ));
    folders(lib2, LFF_ALLITEMS, buf, sizeof(buf));
    say("reload2_folders", "%s", buf);
    say("resolve_a", "%#lx", IShellLibrary_ResolveFolder(lib2, a, 1000, &IID_IShellItem, (void **)&dir));
    RemoveDirectoryW(sub(L"gamma"));
    say("resolve_gone", "%#lx", IShellLibrary_ResolveFolder(lib2, c, 1000, &IID_IShellItem, (void **)&dir));
    say("commit_ro", "%#lx", IShellLibrary_Commit(lib2));
    IShellLibrary_Release(lib2);
    IShellItem_Release(saved);

    /* not a library file */
    lib2 = create();
    {
        HANDLE f = CreateFileW(sub(L"junk.library-ms"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD w;
        WriteFile(f, "<foo/>", 6, &w, NULL);
        CloseHandle(f);
    }
    saved = item(sub(L"junk.library-ms"));
    say("junk", "%d", FAILED(IShellLibrary_LoadLibraryFromItem(lib2, saved, STGM_READ)));
    IShellLibrary_Release(lib2);

    /* a description as Windows writes it: a known folder URL, the default on the second location */
    {
        static const char xml[] =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
            "<libraryDescription xmlns=\"http://schemas.microsoft.com/windows/2009/library\">\r\n"
            "  <name>@shell32.dll,-34575</name>\r\n  <version>6</version>\r\n"
            "  <isLibraryPinned>false</isLibraryPinned>\r\n"
            "  <iconReference>imageres.dll,-1004</iconReference>\r\n"
            "  <templateInfo><folderType>{94d6ddcc-4a68-4175-a374-bd584a510b78}</folderType></templateInfo>\r\n"
            "  <searchConnectorDescriptionList>\r\n"
            "    <searchConnectorDescription publisher=\"Microsoft\" product=\"Windows\">\r\n"
            "      <description>@shell32.dll,-34577</description>\r\n"
            "      <simpleLocation><url>knownfolder:{4BD8D571-6D19-48D3-BE97-422220080E43}</url>"
            "<serialized>MBAAAEAFCAAA</serialized></simpleLocation>\r\n"
            "    </searchConnectorDescription>\r\n"
            "    <searchConnectorDescription>\r\n"
            "      <isDefaultSaveLocation>true</isDefaultSaveLocation>\r\n"
            "      <simpleLocation><url>%s</url></simpleLocation>\r\n"
            "    </searchConnectorDescription>\r\n"
            "  </searchConnectorDescriptionList>\r\n"
            "</libraryDescription>\r\n";
        char text[4096], alpha[MAX_PATH];
        HANDLE f;
        DWORD w;

        WideCharToMultiByte(CP_UTF8, 0, sub(L"alpha"), -1, alpha, sizeof(alpha), NULL, NULL);
        snprintf(text, sizeof(text), xml, alpha);
        f = CreateFileW(sub(L"Windows.library-ms"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(f, text, strlen(text), &w, NULL);
        CloseHandle(f);
        lib2 = create();
        saved = item(sub(L"Windows.library-ms"));
        say("win_load", "%#lx", IShellLibrary_LoadLibraryFromItem(lib2, saved, STGM_READ));
        folders(lib2, LFF_FORCEFILESYSTEM, buf, sizeof(buf));
        say("win_folders", "%s", buf);
        save_folder(lib2, DSFT_DETECT, buf, sizeof(buf));
        say("win_save", "%s", buf);
        IShellLibrary_GetFolderType(lib2, &type);
        say("win_type", "%d", IsEqualGUID(&type, &SG_FOLDERTYPEID_Music));
        IShellLibrary_GetOptions(lib2, &opts);
        say("win_pinned", "%d", opts == LOF_PINNEDTONAVPANE);
        IShellLibrary_Release(lib2);
        IShellItem_Release(saved);
    }

    say("done", "1");
    return 0;
}
