/* advpack gate: UserUnInstStubWrapper (the "<guid>.Restore" Active Setup key) next to UserInstStubWrapper */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef HRESULT (WINAPI *stub_fn)(HWND, HINSTANCE, LPWSTR, INT);
static const WCHAR *base = L"SOFTWARE\\Microsoft\\Active Setup\\Installed Components";

static void set_stub(const WCHAR *name, const WCHAR *cmd)
{
    WCHAR key[300];
    HKEY k;
    swprintf(key, 300, L"%ls\\%ls", base, name);
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL))
    {
        RegSetValueExW(k, L"RealStubPath", 0, REG_SZ, (const BYTE *)cmd, (lstrlenW(cmd) + 1) * sizeof(WCHAR));
        RegCloseKey(k);
    }
}

static BOOL exists(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

int main(void)
{
    HMODULE mod = LoadLibraryW(L"advpack.dll");
    stub_fn inst = (stub_fn)GetProcAddress(mod, "UserInstStubWrapperW");
    stub_fn uninst = (stub_fn)GetProcAddress(mod, "UserUnInstStubWrapperW");
    WCHAR guid[] = L"{6B3DA7C0-0001-4000-8000-000000000001}", missing[] = L"{6B3DA7C0-0001-4000-8000-0000000000FF}", empty[] = L"";
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(inst && uninst, "exports");
    DeleteFileW(L"C:\\sgadv-inst.txt");
    DeleteFileW(L"C:\\sgadv-uninst.txt");
    set_stub(guid, L"C:\\windows\\system32\\cmd.exe /c echo x> C:\\sgadv-inst.txt");
    set_stub(L"{6B3DA7C0-0001-4000-8000-000000000001}.Restore", L"C:\\windows\\system32\\cmd.exe /c echo x> C:\\sgadv-uninst.txt");

    hr = uninst(NULL, NULL, guid, 0);
    CHECK(hr == S_OK, "UserUnInstStubWrapper %#lx", hr);
    CHECK(exists(L"C:\\sgadv-uninst.txt"), "the .Restore stub did not run");
    CHECK(!exists(L"C:\\sgadv-inst.txt"), "the install stub ran for the uninstall");
    hr = inst(NULL, NULL, guid, 0);
    CHECK(hr == S_OK && exists(L"C:\\sgadv-inst.txt"), "UserInstStubWrapper still runs RealStubPath: %#lx", hr);
    hr = uninst(NULL, NULL, missing, 0);
    CHECK(hr == E_FAIL, "missing .Restore key: %#lx", hr);
    hr = uninst(NULL, NULL, NULL, 0);
    CHECK(hr == E_INVALIDARG, "NULL parameters: %#lx", hr);
    hr = uninst(NULL, NULL, empty, 0);
    CHECK(hr == E_INVALIDARG, "empty parameters: %#lx", hr);
    {
        HRESULT (WINAPI *uninstA)(HWND, HINSTANCE, LPSTR, INT) = (void *)GetProcAddress(mod, "UserUnInstStubWrapperA");
        DeleteFileW(L"C:\\sgadv-uninst.txt");
        hr = uninstA(NULL, NULL, "{6B3DA7C0-0001-4000-8000-000000000001}", 0);
        CHECK(hr == S_OK && exists(L"C:\\sgadv-uninst.txt"), "ANSI variant: %#lx", hr);
    }
    DeleteFileW(L"C:\\sgadv-inst.txt");
    DeleteFileW(L"C:\\sgadv-uninst.txt");
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
