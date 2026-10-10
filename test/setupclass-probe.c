/* setupapi/cfgmgr32 batch (patches/sg/2035), run by test/setupclass-gate.sh.
 * Class keys (CM_Open_Class_Key, CM_Delete_Class_Key, CM_Get_Class_Name, A and
 * W, through cfgmgr32), the global state, the version and the docking
 * station. Everything is looked up with GetProcAddress in cfgmgr32.dll, as
 * the exports are what is under test.
 *
 *   setupclass-probe.exe */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define CR_SUCCESS 0x00
#define CR_INVALID_POINTER 0x03
#define CR_INVALID_FLAG 0x04
#define CR_BUFFER_SMALL 0x1a
#define CR_REGISTRY_ERROR 0x1d
#define CR_INVALID_DATA 0x1f
#define CR_NO_SUCH_REGISTRY_KEY 0x2e

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

typedef DWORD (WINAPI *open_w_t)(GUID *, const WCHAR *, REGSAM, ULONG, HKEY *, ULONG);
typedef DWORD (WINAPI *open_a_t)(GUID *, const char *, REGSAM, ULONG, HKEY *, ULONG);
typedef DWORD (WINAPI *delete_t)(GUID *, ULONG);
typedef DWORD (WINAPI *name_w_t)(GUID *, WCHAR *, ULONG *, ULONG);
typedef DWORD (WINAPI *name_a_t)(GUID *, char *, ULONG *, ULONG);
typedef DWORD (WINAPI *state_t)(ULONG *, ULONG);
typedef WORD (WINAPI *version_ex_t)(void *);
typedef DWORD (WINAPI *dock_t)(BOOL *);

static const GUID class_guid = { 0x5b1a4c90, 0x2f4c, 0x4f3a, { 0x9e, 0x11, 0x53, 0x67, 0x47, 0x6c, 0x61, 0x73 } };
static const GUID missing_guid = { 0x5b1a4c91, 0x2f4c, 0x4f3a, { 0x9e, 0x11, 0x53, 0x67, 0x47, 0x6c, 0x61, 0x73 } };

int main(void)
{
    HMODULE cm = LoadLibraryA("cfgmgr32.dll");
    open_w_t open_w = (open_w_t)GetProcAddress(cm, "CM_Open_Class_KeyW");
    open_a_t open_a = (open_a_t)GetProcAddress(cm, "CM_Open_Class_KeyA");
    delete_t del = (delete_t)GetProcAddress(cm, "CM_Delete_Class_Key");
    name_w_t name_w = (name_w_t)GetProcAddress(cm, "CM_Get_Class_NameW");
    name_a_t name_a = (name_a_t)GetProcAddress(cm, "CM_Get_Class_NameA");
    state_t state = (state_t)GetProcAddress(cm, "CM_Get_Global_State");
    version_ex_t version_ex = (version_ex_t)GetProcAddress(cm, "CM_Get_Version_Ex");
    dock_t dock = (dock_t)GetProcAddress(cm, "CM_Is_Dock_Station_Present");
    GUID g = class_guid, miss = missing_guid;
    HKEY key = (HKEY)1, sub;
    WCHAR wname[64];
    char aname[64];
    ULONG len, st;
    BOOL present = TRUE;
    DWORD cr;
    DWORD type, size;

    CHECK(open_w && open_a && del && name_w && name_a && state && version_ex && dock);
    if (!(open_w && open_a && del && name_w && name_a && state && version_ex && dock))
    {
        printf("RESULT: FAIL\n");
        return 1;
    }

    /* open */
    cr = open_w(&miss, NULL, KEY_READ, 1, &key, 0);
    check(cr == CR_NO_SUCH_REGISTRY_KEY && key == NULL, "an unknown class is not found when opened existing");
    cr = open_w(&g, L"SgProbeClass", KEY_ALL_ACCESS, 0, &key, 0);
    check(cr == CR_SUCCESS && key != NULL, "open always creates the class key");
    size = sizeof(wname);
    type = 0;
    cr = key ? RegQueryValueExW(key, L"Class", NULL, &type, (BYTE *)wname, &size) : 1;
    check(cr == 0 && type == REG_SZ && !wcscmp(wname, L"SgProbeClass"), "a new class key carries its class name");
    if (key) RegCloseKey(key);
    cr = open_w(&g, NULL, KEY_READ, 1, &key, 0);
    check(cr == CR_SUCCESS && key != NULL, "open existing finds it");
    if (key) RegCloseKey(key);
    cr = open_a(&g, "Other", KEY_READ, 0, &key, 0);
    check(cr == CR_SUCCESS, "open always on an existing key (ANSI)");
    if (!cr) RegCloseKey(key);
    cr = open_w(&g, NULL, KEY_READ, 1, NULL, 0);
    check(cr == CR_INVALID_POINTER, "open without a key pointer");
    cr = open_w(&g, NULL, KEY_READ, 1, &key, 0x10);
    check(cr == CR_INVALID_FLAG && key == NULL, "open with a bad flag");
    cr = open_w(&g, NULL, KEY_READ, 7, &key, 0);
    check(cr == CR_INVALID_DATA, "open with a bad disposition");
    cr = open_w(&g, NULL, KEY_ALL_ACCESS, 0, &key, 1);
    check(cr == CR_SUCCESS && key != NULL, "the interface key can be made");
    if (key)
    {
        size = sizeof(wname);
        check(RegQueryValueExW(key, L"Class", NULL, NULL, (BYTE *)wname, &size) != 0, "and has no class name");
        RegCloseKey(key);
    }
    cr = open_w(NULL, NULL, KEY_READ, 1, &key, 0);
    check(cr == CR_SUCCESS && key != NULL, "no GUID opens the root of the classes");
    if (!cr) RegCloseKey(key);

    /* name */
    len = 64;
    cr = name_w(&g, wname, &len, 0);
    check(cr == CR_SUCCESS && !wcscmp(wname, L"SgProbeClass") && len == 13, "class name (W) and its length with the NUL");
    len = 5;
    cr = name_w(&g, wname, &len, 0);
    check(cr == CR_BUFFER_SMALL && len == 13, "a small buffer reports the size needed");
    len = 64;
    cr = name_a(&g, aname, &len, 0);
    check(cr == CR_SUCCESS && !strcmp(aname, "SgProbeClass") && len == 13, "class name (A)");
    len = 64;
    check(name_w(&miss, wname, &len, 0) == CR_NO_SUCH_REGISTRY_KEY, "the name of an unknown class");
    len = 64;
    check(name_w(NULL, wname, &len, 0) == CR_INVALID_POINTER, "no GUID");
    len = 64;
    check(name_w(&g, wname, &len, 1) == CR_INVALID_FLAG, "a flag for the name");
    /* a class key without a name */
    len = 64;
    cr = open_w(&miss, NULL, KEY_ALL_ACCESS, 0, &key, 0);
    if (!cr) RegCloseKey(key);
    cr = name_w(&miss, wname, &len, 0);
    check(cr == CR_REGISTRY_ERROR, "a class key without a name");

    /* delete */
    check(del(&miss, 0) == CR_SUCCESS, "delete a class key");
    check(del(&miss, 0) == CR_NO_SUCH_REGISTRY_KEY, "deleting it again");
    check(del(NULL, 0) == CR_INVALID_POINTER, "delete without a GUID");
    check(del(&g, 2) == CR_INVALID_FLAG, "delete with a bad flag");
    cr = open_w(&g, NULL, KEY_ALL_ACCESS, 1, &key, 0);
    if (!cr)
    {
        RegCreateKeyExW(key, L"0000", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sub, NULL);
        RegCloseKey(sub);
        RegCloseKey(key);
    }
    check(del(&g, 0) == CR_REGISTRY_ERROR, "a class with subkeys is not deleted by itself");
    cr = open_w(&g, NULL, KEY_READ, 1, &key, 0);
    check(cr == CR_SUCCESS, "and is still there");
    if (!cr) RegCloseKey(key);
    check(del(&g, 1) == CR_SUCCESS, "deleted with its subkeys");
    cr = open_w(&g, NULL, KEY_READ, 1, &key, 0);
    check(cr == CR_NO_SUCH_REGISTRY_KEY, "and gone");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\{5b1a4c90-2f4c-4f3a-9e11-536747" L"6c6173}");

    /* the rest */
    st = 0;
    check(state(&st, 0) == CR_SUCCESS && st == 5, "global state: UI and services");
    check(state(NULL, 0) == CR_INVALID_POINTER, "global state without a pointer");
    check(state(&st, 1) == CR_INVALID_FLAG, "global state with a flag");
    check(version_ex(NULL) == 0x0400, "version");
    check(dock(&present) == CR_SUCCESS && present == FALSE, "no docking station");
    check(dock(NULL) == CR_INVALID_POINTER, "docking station without a pointer");

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
