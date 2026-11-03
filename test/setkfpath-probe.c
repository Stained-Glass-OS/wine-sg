/* SHSetKnownFolderPath: moving known folders (patch 6236) */
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <stdio.h>

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); f++; } } while (0)
static int f;
static HRESULT (WINAPI *pSet)(REFKNOWNFOLDERID, DWORD, HANDLE, const WCHAR *);

static BOOL get_is(REFKNOWNFOLDERID id, const WCHAR *want)
{
    WCHAR *p = NULL;
    BOOL ok = SUCCEEDED(SHGetKnownFolderPath(id, 0, NULL, &p)) && !lstrcmpiW(p, want);
    if (!ok) printf("  path %ls, want %ls\n", p ? p : L"(none)", want);
    CoTaskMemFree(p);
    return ok;
}

static BOOL reg_is(const WCHAR *key, const WCHAR *name, const WCHAR *want, DWORD want_type)
{
    WCHAR buf[MAX_PATH] = {0};
    DWORD size = sizeof(buf), type = 0;
    LONG r = RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_ANY | RRF_NOEXPAND, &type, buf, &size);
    if (r || lstrcmpiW(buf, want) || (want_type && type != want_type))
        printf("  %ls %ls = %ls (type %lu, %ld)\n", key, name, buf, type, r);
    return !r && !lstrcmpiW(buf, want) && (!want_type || type == want_type);
}

int main(void)
{
    static const WCHAR usf[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders";
    static const WCHAR sf[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
    static const GUID unknown = {0x12345678, 0x1234, 0x1234, {1, 2, 3, 4, 5, 6, 7, 8}};
    WCHAR *docs = NULL, *dl = NULL, path[MAX_PATH], exp[MAX_PATH];
    HANDLE tok;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    *(FARPROC *)&pSet = GetProcAddress(GetModuleHandleA("shell32.dll"), "SHSetKnownFolderPath");
    CHECK(pSet != NULL);
    if (!pSet) goto done;
    CHECK(SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &docs)));
    CHECK(SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Downloads, 0, NULL, &dl)));

    /* a folder with a legacy name: User Shell Folders and Shell Folders "Personal" */
    hr = pSet(&FOLDERID_Documents, KF_FLAG_CREATE, NULL, L"C:\\users\\Public\\MovedDocs");
    CHECK(hr == S_OK);
    CHECK(GetFileAttributesW(L"C:\\users\\Public\\MovedDocs") & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(get_is(&FOLDERID_Documents, L"C:\\users\\Public\\MovedDocs"));
    CHECK(SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path)) &&
          !lstrcmpiW(path, L"C:\\users\\Public\\MovedDocs"));
    CHECK(reg_is(usf, L"Personal", L"C:\\users\\Public\\MovedDocs", REG_SZ));
    CHECK(reg_is(sf, L"Personal", L"C:\\users\\Public\\MovedDocs", REG_SZ));

    /* environment strings stay unexpanded in User Shell Folders */
    hr = pSet(&FOLDERID_Documents, KF_FLAG_CREATE, NULL, L"%USERPROFILE%\\Docs3");
    CHECK(hr == S_OK);
    ExpandEnvironmentStringsW(L"%USERPROFILE%\\Docs3", exp, MAX_PATH);
    CHECK(get_is(&FOLDERID_Documents, exp));
    CHECK(reg_is(usf, L"Personal", L"%USERPROFILE%\\Docs3", REG_EXPAND_SZ));
    CHECK(reg_is(sf, L"Personal", exp, REG_SZ));

    /* a folder known by its id only; the process's own token is the current user */
    CreateDirectoryW(L"C:\\dl2", NULL); /* without KF_FLAG_CREATE the folder must exist to be found */
    CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok));
    hr = pSet(&FOLDERID_Downloads, 0, tok, L"C:\\dl2");
    CloseHandle(tok);
    CHECK(hr == S_OK);
    CHECK(get_is(&FOLDERID_Downloads, L"C:\\dl2"));
    CHECK(reg_is(usf, L"{374DE290-123F-4565-9164-39C4925E467B}", L"C:\\dl2", 0));

    /* fixed and unknown folders, no path */
    CHECK(pSet(&FOLDERID_Windows, 0, NULL, L"C:\\elsewhere") == E_INVALIDARG);
    CHECK(pSet(&FOLDERID_System, 0, NULL, L"C:\\elsewhere") == E_INVALIDARG);
    CHECK(pSet(&unknown, 0, NULL, L"C:\\elsewhere") == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    CHECK(pSet(&FOLDERID_Documents, 0, NULL, NULL) == E_INVALIDARG);
    CHECK(get_is(&FOLDERID_Documents, exp));

    /* back */
    CHECK(pSet(&FOLDERID_Documents, 0, NULL, docs) == S_OK && get_is(&FOLDERID_Documents, docs));
    CHECK(pSet(&FOLDERID_Downloads, 0, NULL, dl) == S_OK && get_is(&FOLDERID_Downloads, dl));
    CoTaskMemFree(docs);
    CoTaskMemFree(dl);
done:
    printf("RESULT: %s\n", f ? "FAIL" : "PASS");
    return f != 0;
}
