/* wine-sg 9922: SHOpenPropSheetW / SHOpenPropSheetA, run by test/propsheet-gate.sh with
 * test/propsheet-handler.c built as C:\propsheet-handler.dll (pages "SG Page A", "SG Page B").
 * SHOpenPropSheetW is imported by name (Shell32.lib names it; Wine did not export it, so this probe did
 * not start). The sheet's pages come from the keys' PropertySheetHandlers and the default CLSID, each
 * handler gets the data object and its key, the start page is honoured, no pages is FALSE. */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>

static const CLSID clsid = { 0x5e6a1c2b, 0x9d3f, 0x4c71, { 0x8a, 0x10, 0x2f, 0x33, 0x51, 0x7c, 0x0e, 0x91 } };
static const WCHAR clsid_str[] = L"{5E6A1C2B-9D3F-4C71-8A10-2F33517C0E91}";
#ifdef _WIN64
#define HANDLER_PATH L"C:\\propsheet-handler.dll"
#else
#define HANDLER_PATH L"C:\\propsheet-handler32.dll"
#endif
typedef BOOL (WINAPI *open_a_t)(LPCSTR, HKEY *, UINT, const CLSID *, IDataObject *, IShellBrowser *, LPCSTR);

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static DWORD main_tid;
static volatile LONG stop;
static struct { int found, pages, cursel; WCHAR title[200], tabs[200]; } seen;

static BOOL CALLBACK find_sheet(HWND hwnd, LPARAM lp)
{
    WCHAR title[200];
    if (!IsWindowVisible(hwnd)) return TRUE;
    GetWindowTextW(hwnd, title, 200);
    if (!wcsstr(title, L"SG Probe")) return TRUE;
    *(HWND *)lp = hwnd;
    return FALSE;
}

static DWORD WINAPI closer(void *arg)
{
    int i, j;
    for (i = 0; i < 200 && !stop; i++)
    {
        HWND sheet = NULL, tab;
        EnumThreadWindows(main_tid, find_sheet, (LPARAM)&sheet);
        if (sheet)
        {
            seen.found++;
            GetWindowTextW(sheet, seen.title, 200);
            tab = (HWND)SendMessageW(sheet, PSM_GETTABCONTROL, 0, 0);
            seen.pages = SendMessageW(tab, TCM_GETITEMCOUNT, 0, 0);
            seen.cursel = SendMessageW(tab, TCM_GETCURSEL, 0, 0);
            seen.tabs[0] = 0;
            for (j = 0; j < seen.pages; j++)
            {
                WCHAR text[64] = L"";
                TCITEMW item = { TCIF_TEXT };
                item.pszText = text; item.cchTextMax = 64;
                SendMessageW(tab, TCM_GETITEMW, j, (LPARAM)&item);
                if (j) wcscat(seen.tabs, L"|");
                wcsncat(seen.tabs, text, 60);
            }
            PostMessageW(sheet, PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
            return 0;
        }
        Sleep(50);
    }
    return 1;
}

static HANDLE start_closer(void)
{
    memset(&seen, 0, sizeof(seen));
    stop = 0;
    return CreateThread(NULL, 0, closer, NULL, 0, NULL);
}

static void wait_closer(HANDLE t, DWORD ms)
{
    if (WaitForSingleObject(t, ms))
    {
        stop = 1;
        WaitForSingleObject(t, INFINITE);
    }
    CloseHandle(t);
}

static void setstr(HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *value)
{
    HKEY k;
    if (RegCreateKeyExW(root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL)) return;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW(value) + 1) * sizeof(WCHAR));
    RegCloseKey(k);
}

int main(void)
{
    WCHAR path[300];
    HMODULE handler;
    int *init_count;
    IDataObject **init_dobj, *dobj = NULL;
    HKEY *init_key, key = NULL, none = NULL;
    open_a_t pSHOpenPropSheetA;
    HANDLE t;
    BOOL ret;

    main_tid = GetCurrentThreadId();
    CoInitialize(NULL);
    swprintf(path, 300, L"Software\\Classes\\CLSID\\%ls\\InprocServer32", clsid_str);
    setstr(HKEY_LOCAL_MACHINE, path, NULL, HANDLER_PATH);
    setstr(HKEY_LOCAL_MACHINE, path, L"ThreadingModel", L"Apartment");
    setstr(HKEY_LOCAL_MACHINE, L"Software\\Classes\\sgtest.file\\shellex\\PropertySheetHandlers\\SGTest", NULL, clsid_str);
    setstr(HKEY_LOCAL_MACHINE, L"Software\\Classes\\sgtest.none", NULL, L"no handlers");
    RegOpenKeyExW(HKEY_CLASSES_ROOT, L"sgtest.file", 0, KEY_READ, &key);
    RegOpenKeyExW(HKEY_CLASSES_ROOT, L"sgtest.none", 0, KEY_READ, &none);
    CHECK(key && none, "test classes registered");

    handler = LoadLibraryW(HANDLER_PATH);
    init_count = (int *)GetProcAddress(handler, "sg_init_count");
    init_dobj = (IDataObject **)GetProcAddress(handler, "sg_init_dobj");
    init_key = (HKEY *)GetProcAddress(handler, "sg_init_key");
    CHECK(init_count && init_dobj && init_key, "the handler DLL loads");
    SHCreateDataObject(NULL, 0, NULL, NULL, &IID_IDataObject, (void **)&dobj);
    CHECK(dobj != NULL, "a data object");
    if (!init_count || !dobj || !key) goto end;

    /* the key's handlers */
    t = start_closer();
    ret = SHOpenPropSheetW(L"SG Probe", &key, 1, NULL, dobj, NULL, NULL);
    wait_closer(t, 15000);
    CHECK(ret, "SHOpenPropSheetW with a key whose handler adds two pages (ret %d)", ret);
    CHECK(seen.found == 1 && seen.pages == 2 && !wcscmp(seen.tabs, L"SG Page A|SG Page B"), "the sheet \"%ls\" has the handler's pages (%d: %ls)", seen.title, seen.pages, seen.tabs);
    CHECK(seen.cursel == 0, "no start page: the first page (%d)", seen.cursel);
    CHECK(*init_count == 1 && *init_dobj == dobj && *init_key == key, "the handler was initialized with the data object and its key (%d, %p, %p)", *init_count, *init_dobj, *init_key);

    /* the start page, by name */
    t = start_closer();
    ret = SHOpenPropSheetW(L"SG Probe", &key, 1, NULL, dobj, NULL, L"SG Page B");
    wait_closer(t, 15000);
    CHECK(ret && seen.found == 1 && seen.cursel == 1, "start page \"SG Page B\" is the one shown (ret %d, page %d)", ret, seen.cursel);

    /* the default CLSID alone, then with the key */
    t = start_closer();
    ret = SHOpenPropSheetW(L"SG Probe", NULL, 0, &clsid, dobj, NULL, NULL);
    wait_closer(t, 15000);
    CHECK(ret && seen.found == 1 && seen.pages == 2, "the default CLSID's pages without keys (ret %d, %d pages)", ret, seen.pages);
    t = start_closer();
    ret = SHOpenPropSheetW(L"SG Probe", &key, 1, &clsid, dobj, NULL, NULL);
    wait_closer(t, 15000);
    CHECK(ret && seen.found == 1 && seen.pages == 4, "the default CLSID's pages and the key's (ret %d, %d pages)", ret, seen.pages);

    /* no pages: FALSE and no sheet */
    t = start_closer();
    ret = SHOpenPropSheetW(L"SG Probe", &none, 1, NULL, dobj, NULL, NULL);
    wait_closer(t, 2000);
    CHECK(!ret && !seen.found, "a key without handlers and no default: FALSE, no sheet (ret %d)", ret);

    /* the ANSI one, ordinal 707 */
    pSHOpenPropSheetA = (open_a_t)GetProcAddress(GetModuleHandleA("shell32.dll"), (LPCSTR)707);
    CHECK(pSHOpenPropSheetA != NULL, "ordinal 707 (SHOpenPropSheetA) is exported");
    if (pSHOpenPropSheetA)
    {
        t = start_closer();
        ret = pSHOpenPropSheetA("SG Probe A", &key, 1, NULL, dobj, NULL, "SG Page B");
        wait_closer(t, 15000);
        CHECK(ret && seen.found == 1 && wcsstr(seen.title, L"SG Probe A") && seen.cursel == 1, "SHOpenPropSheetA (ret %d, \"%ls\", page %d)", ret, seen.title, seen.cursel);
    }

end:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(failures ? 1 : 0);
}
