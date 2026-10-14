/* shell32 smoke: SHSet/Get/RemoveLocalizedName, IQueryAssociations::GetKey */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    WCHAR dir[MAX_PATH], tmp[MAX_PATH], mod[MAX_PATH], ini[MAX_PATH];
    int id = 0; HRESULT hr; IQueryAssociations *qa = NULL; HKEY key = NULL, base = NULL;
    HRESULT (WINAPI *pGet)(LPCWSTR, LPWSTR, UINT, int *);
    HRESULT (WINAPI *pSet)(LPWSTR, LPCWSTR, int);
    HRESULT (WINAPI *pRem)(LPCWSTR);
    HMODULE sh = LoadLibraryA("shell32.dll");

    CoInitialize(NULL);
    pGet = (void *)GetProcAddress(sh, "SHGetLocalizedName");
    pSet = (void *)GetProcAddress(sh, "SHSetLocalizedName");
    pRem = (void *)GetProcAddress(sh, "SHRemoveLocalizedName");

    GetTempPathW(MAX_PATH, tmp);
    wsprintfW(dir, L"%ssg_gapshell_%lu", tmp, GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    hr = pGet(dir, mod, MAX_PATH, &id);
    CHECK(FAILED(hr), "no ini yet, got %#lx", hr);
    hr = pSet(dir, L"%SystemRoot%\\system32\\shell32.dll", 21770);
    CHECK(hr == S_OK, "SHSetLocalizedName %#lx", hr);
    hr = pGet(dir, mod, MAX_PATH, &id);
    CHECK(hr == S_OK, "SHGetLocalizedName %#lx", hr);
    CHECK(id == 21770, "id %d", id);
    CHECK(!lstrcmpiW(mod, L"C:\\windows\\system32\\shell32.dll") || StrStrIW(mod, L"shell32.dll"), "module %ls", mod);
    hr = pGet(dir, mod, 4, &id);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "small buffer %#lx", hr);
    hr = pRem(dir);
    CHECK(hr == S_OK, "SHRemoveLocalizedName %#lx", hr);
    hr = pGet(dir, mod, MAX_PATH, &id);
    CHECK(FAILED(hr), "after remove %#lx", hr);
    wsprintfW(ini, L"%s\\desktop.ini", dir);
    SetFileAttributesW(ini, FILE_ATTRIBUTE_NORMAL); DeleteFileW(ini); RemoveDirectoryW(dir);

    hr = CoCreateInstance(&CLSID_QueryAssociations, NULL, CLSCTX_INPROC_SERVER, &IID_IQueryAssociations, (void **)&qa);
    CHECK(hr == S_OK, "QueryAssociations %#lx", hr);
    if (qa)
    {
        hr = qa->lpVtbl->Init(qa, 0, L".txt", NULL, NULL);
        CHECK(hr == S_OK, "Init %#lx", hr);
        hr = qa->lpVtbl->GetKey(qa, 0, ASSOCKEY_CLASS, NULL, &key);
        CHECK(hr == S_OK && key, "GetKey(CLASS) %#lx", hr);
        if (key) { DWORD n = 0; CHECK(RegQueryInfoKeyW(key, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL) == 0, "key not valid"); RegCloseKey(key); key = NULL; }
        hr = qa->lpVtbl->GetKey(qa, 0, ASSOCKEY_SHELLEXECCLASS, NULL, &key);
        CHECK(hr == S_OK && key, "GetKey(SHELLEXECCLASS) %#lx", hr);
        if (key) RegCloseKey(key);
        hr = qa->lpVtbl->GetKey(qa, 0, ASSOCKEY_BASECLASS, NULL, &base);
        CHECK(hr == S_OK || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "GetKey(BASECLASS) %#lx", hr);
        if (hr == S_OK) RegCloseKey(base);
        key = NULL;
        hr = qa->lpVtbl->GetKey(qa, 0, ASSOCKEY_APP, NULL, &key);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION), "GetKey(APP) %#lx", hr);
        hr = qa->lpVtbl->GetKey(qa, 0, 99, NULL, &key);
        CHECK(hr == E_INVALIDARG, "GetKey(99) %#lx", hr);
        qa->lpVtbl->Release(qa);
    }
    printf("RESULT: %s\n", failed ? "FAIL" : "PASS");
    return failed != 0;
}
