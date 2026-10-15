#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HRESULT (WINAPI *pRegSaveRestoreA)(HWND, LPCSTR, HKEY, LPCSTR, LPCSTR, LPCSTR, DWORD);
static HRESULT (WINAPI *pRegRestoreAllA)(HWND, LPSTR, HKEY);
static HRESULT (WINAPI *pRegSaveRestoreOnINFA)(HWND, LPCSTR, LPCSTR, LPCSTR, HKEY, HKEY, DWORD);
static HRESULT (WINAPI *pFileSaveRestoreA)(HWND, LPSTR, LPSTR, LPSTR, DWORD);
static HRESULT (WINAPI *pFileSaveRestoreOnINFA)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, DWORD);
typedef struct { CHAR szGUID[59], szDispName[128], szLocale[10], szStub[MAX_PATH * 4], szVersion[32], szCompID[128]; DWORD dwIsInstalled; BOOL bRollback; } PERUSER;
static HRESULT (WINAPI *pSetPerUserSecValuesA)(PERUSER *);
static DWORD (WINAPI *pNeedRebootInit)(void);
static BOOL (WINAPI *pNeedReboot)(DWORD);
static HRESULT (WINAPI *pRebootCheckOnInstallA)(HWND, LPCSTR, LPCSTR, DWORD);

static void write_file(const char *path, const char *data)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n;
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, data, strlen(data), &n, NULL);
    CloseHandle(h);
}

static int file_has(const char *path, const char *text)
{
    char buf[256] = "";
    DWORD n = 0;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    ReadFile(h, buf, sizeof(buf) - 1, &n, NULL);
    CloseHandle(h);
    buf[n] = 0;
    return !strcmp(buf, text);
}

static int exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }

int main(void)
{
    HMODULE adv = LoadLibraryA("advpack.dll");
    HKEY back, hklm_back, hkcu_back, key;
    char dir[MAX_PATH], a[MAX_PATH], b[MAX_PATH], c[MAX_PATH], list[MAX_PATH * 4], inf[MAX_PATH], buf[64];
    DWORD size, st, disp;
    HRESULT hr;
    HANDLE lock;

    CHECK(adv != NULL, "advpack");
#define GET(n) p##n = (void *)GetProcAddress(adv, #n); CHECK(p##n != NULL, "no " #n)
    GET(RegSaveRestoreA); GET(RegRestoreAllA); GET(RegSaveRestoreOnINFA); GET(FileSaveRestoreA);
    GET(FileSaveRestoreOnINFA); GET(NeedRebootInit); GET(NeedReboot); GET(RebootCheckOnInstallA);

    /* NeedReboot: nothing changed since NeedRebootInit */
    st = pNeedRebootInit();
    CHECK(st != 0, "NeedRebootInit returned 0");
    CHECK(!pNeedReboot(st), "a reboot needed without a change");
    if (!RegCreateKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Session Manager", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, &disp))
    {
        RegSetValueExA(key, "PendingFileRenameOperations", 0, REG_MULTI_SZ, (const BYTE *)"\\??\\C:\\fastadv-a\0\0\\??\\C:\\fastadv-b\0\0\0", 40);
        CHECK(pNeedReboot(st), "no reboot needed after a file was queued for replacement");
        RegDeleteValueA(key, "PendingFileRenameOperations");
        RegCloseKey(key);
    }
    else CHECK(0, "session manager key");

    GetTempPathA(sizeof(dir), dir);
    strcat(dir, "fastadvpack");
    CreateDirectoryA(dir, NULL);

    /* registry: save a value, change it, restore it; a value that did not exist is deleted */
    RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\FastAdv", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, &disp);
    RegSetValueExA(key, "One", 0, REG_SZ, (const BYTE *)"first", 6);
    RegCloseKey(key);
    RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\FastAdvBackup", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &back, &disp);

    hr = pRegSaveRestoreA(NULL, "t", back, "HKCU", "Software\\FastAdv", "One", 0);
    CHECK(hr == S_OK, "save One: %08lx", hr);
    hr = pRegSaveRestoreA(NULL, "t", back, "HKCU", "Software\\FastAdv", "Two", 0);
    CHECK(hr == S_OK, "save Two (absent): %08lx", hr);
    RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\FastAdv", 0, KEY_ALL_ACCESS, &key);
    RegSetValueExA(key, "One", 0, REG_SZ, (const BYTE *)"changed", 8);
    RegSetValueExA(key, "Two", 0, REG_DWORD, (const BYTE *)"\x07\0\0\0", 4);
    hr = pRegSaveRestoreA(NULL, "t", back, "HKCU", "Software\\FastAdv", "One", 1);
    CHECK(hr == S_OK, "restore One: %08lx", hr);
    size = sizeof(buf);
    CHECK(!RegQueryValueExA(key, "One", NULL, NULL, (BYTE *)buf, &size) && !strcmp(buf, "first"), "One restored: %s", buf);
    hr = pRegRestoreAllA(NULL, NULL, back);
    CHECK(hr == S_OK, "restore all: %08lx", hr);
    CHECK(RegQueryValueExA(key, "Two", NULL, NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND, "Two deleted again");
    /* the first saved state wins over a second save */
    RegSetValueExA(key, "One", 0, REG_SZ, (const BYTE *)"later", 6);
    pRegSaveRestoreA(NULL, "t", back, "HKCU", "Software\\FastAdv", "One", 0);
    pRegSaveRestoreA(NULL, "t", back, "HKCU", "Software\\FastAdv", "One", 1);
    size = sizeof(buf);
    RegQueryValueExA(key, "One", NULL, NULL, (BYTE *)buf, &size);
    CHECK(!strcmp(buf, "first"), "first state kept: %s", buf);

    /* the registry sections of an INF */
    sprintf(inf, "%s\\reg.inf", dir);
    write_file(inf, "[Version]\r\nSignature=\"$Chicago$\"\r\n[DefaultInstall]\r\nAddReg=Add\r\nDelReg=Del\r\n"
                    "[Add]\r\nHKCU,Software\\FastAdv,Three,0,\"new\"\r\n[Del]\r\nHKCU,Software\\FastAdv,One\r\n");
    RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\FastAdvBackup2", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkcu_back, &disp);
    RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\FastAdvBackup3", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hklm_back, &disp);
    RegSetValueExA(key, "One", 0, REG_SZ, (const BYTE *)"first", 6);
    hr = pRegSaveRestoreOnINFA(NULL, "t", inf, NULL, hklm_back, hkcu_back, 0);
    CHECK(hr == S_OK, "save on INF: %08lx", hr);
    RegSetValueExA(key, "Three", 0, REG_SZ, (const BYTE *)"zzz", 4);
    RegDeleteValueA(key, "One");
    hr = pRegSaveRestoreOnINFA(NULL, "t", inf, NULL, hklm_back, hkcu_back, 1);
    CHECK(hr == S_OK, "restore on INF: %08lx", hr);
    size = sizeof(buf);
    CHECK(!RegQueryValueExA(key, "One", NULL, NULL, (BYTE *)buf, &size) && !strcmp(buf, "first"), "One back after INF restore");
    CHECK(RegQueryValueExA(key, "Three", NULL, NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND, "Three removed after INF restore");
    RegCloseKey(key);
    RegCloseKey(hkcu_back);
    RegCloseKey(hklm_back);
    RegCloseKey(back);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\FastAdv");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\FastAdvBackup");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\FastAdvBackup2");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\FastAdvBackup3");

    /* files */
    sprintf(a, "%s\\a.txt", dir); sprintf(b, "%s\\b.txt", dir); sprintf(c, "%s\\c.txt", dir);
    write_file(a, "AAA"); write_file(b, "BBB"); DeleteFileA(c);
    memset(list, 0, sizeof(list));
    strcpy(list, a); strcpy(list + strlen(a) + 1, c);
    hr = pFileSaveRestoreA(NULL, list, dir, "bk", 0);
    CHECK(hr == S_OK, "file save: %08lx", hr);
    write_file(a, "changed"); write_file(c, "new file");
    hr = pFileSaveRestoreA(NULL, list, dir, "bk", 1);
    CHECK(hr == S_OK, "file restore: %08lx", hr);
    CHECK(file_has(a, "AAA"), "a.txt restored");
    CHECK(!exists(c), "c.txt (absent at save time) deleted");
    write_file(c, "keep me");
    hr = pFileSaveRestoreA(NULL, NULL, dir, "bk", 1 | 4);
    CHECK(hr == S_OK && exists(c), "NODELETENEW keeps c.txt");

    /* the files of an INF section */
    sprintf(inf, "%s\\files.inf", dir);
    {
        char text[1024];
        sprintf(text, "[Version]\r\nSignature=\"$Chicago$\"\r\n[DefaultInstall]\r\nCopyFiles=Cp\r\n[DestinationDirs]\r\nCp=-1,%s\r\n[Cp]\r\nd.txt\r\n", dir);
        write_file(inf, text);
    }
    sprintf(a, "%s\\d.txt", dir);
    write_file(a, "DDD");
    hr = pFileSaveRestoreOnINFA(NULL, "t", inf, NULL, dir, "bk2", 0);
    CHECK(hr == S_OK, "file save on INF: %08lx", hr);
    write_file(a, "xxx");
    hr = pFileSaveRestoreOnINFA(NULL, "t", inf, NULL, dir, "bk2", 1);
    CHECK(hr == S_OK && file_has(a, "DDD"), "file restore on INF: %08lx", hr);

    /* reboot check: a locked destination needs a reboot */
    hr = pRebootCheckOnInstallA(NULL, inf, "DefaultInstall", 0);
    CHECK(hr == S_FALSE, "free file: %08lx", hr);
    lock = CreateFileA(a, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    hr = pRebootCheckOnInstallA(NULL, inf, "DefaultInstall", 0);
    CHECK(hr == S_OK, "file held open by us (exclusive): %08lx", hr);
    CloseHandle(lock);
    hr = pRebootCheckOnInstallA(NULL, inf, "DefaultInstall", 0);
    CHECK(hr == S_FALSE, "released: %08lx", hr);

    /* a per-user component installed again with rollback keeps the old stub and runs the new one through the wrapper */
    pSetPerUserSecValuesA = (void *)GetProcAddress(adv, "SetPerUserSecValues");
    CHECK(pSetPerUserSecValuesA != NULL, "no SetPerUserSecValues");
    if (pSetPerUserSecValuesA)
    {
        static const char *guid = "{FA571A5B-0000-4000-8000-0000000A0001}";
        PERUSER pu;
        HKEY comp;
        char path[200], v[260];
        DWORD vs;

        memset(&pu, 0, sizeof(pu));
        strcpy(pu.szGUID, guid); strcpy(pu.szDispName, "First"); strcpy(pu.szStub, "first.exe"); strcpy(pu.szVersion, "1,0");
        hr = pSetPerUserSecValuesA(&pu);
        CHECK(hr == S_OK, "per-user first: %08lx", hr);
        strcpy(pu.szDispName, "Second"); strcpy(pu.szStub, "second.exe"); pu.bRollback = TRUE;
        hr = pSetPerUserSecValuesA(&pu);
        CHECK(hr == S_OK, "per-user second: %08lx", hr);
        sprintf(path, "Software\\Microsoft\\Active Setup\\Installed Components\\%s", guid);
        if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_ALL_ACCESS, &comp))
        {
            vs = sizeof(v); v[0] = 0;
            RegQueryValueExA(comp, "OldStubPath", NULL, NULL, (BYTE *)v, &vs);
            CHECK(!strcmp(v, "first.exe"), "OldStubPath '%s'", v);
            vs = sizeof(v); v[0] = 0;
            RegQueryValueExA(comp, "RealStubPath", NULL, NULL, (BYTE *)v, &vs);
            CHECK(!strcmp(v, "second.exe"), "RealStubPath '%s'", v);
            vs = sizeof(v); v[0] = 0;
            RegQueryValueExA(comp, "StubPath", NULL, NULL, (BYTE *)v, &vs);
            CHECK(strstr(v, "UserInstStubWrapper") != NULL, "StubPath '%s'", v);
            RegCloseKey(comp);
            RegDeleteKeyA(HKEY_LOCAL_MACHINE, path);
        }
        else CHECK(0, "component key missing");
    }

    printf("checks %d fails %d\n", checks, fails);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
