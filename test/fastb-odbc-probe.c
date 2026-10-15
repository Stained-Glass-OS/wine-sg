/* odbccp32 dialog gate: drives SQLManageDataSources, SQLCreateDataSource and
 * SQLGetTranslator through their real dialogs (a helper thread clicks the controls). */
#include <windows.h>
#include <commctrl.h>
#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>
#include <stdio.h>
#include <wchar.h>

#define IDC_TAB 200
#define IDC_LIST 201
#define IDC_ADD 202
#define IDC_REMOVE 203
#define IDC_CONFIGURE 204
#define IDC_TRACE_ON 206
#define IDC_TRACE_FILE 207
#define IDC_DRIVERLIST 210
#define IDC_TITLE 220
#define IDC_TYPE_USER 221
#define IDC_TYPE_SYSTEM 222
#define IDC_TYPE_FILE 223
#define IDC_WDRIVERS 224
#define IDC_ADVANCED 225
#define IDC_FILENAME 226
#define IDC_REVIEW 228
#define IDC_BACK 229
#define IDC_NEXT 230
#define IDC_FINISH 231
#define IDC_ADV_TEXT 240
#define IDC_VERIFY 241
#define IDC_TRANSLIST 250

static int failures;
static DWORD main_tid;
static HWND owner;
static volatile LONG script_done, call_returned;

#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct find { const WCHAR *cap; int want, not; HWND found; };

static BOOL CALLBACK find_proc(HWND h, LPARAM lp)
{
    struct find *f = (struct find *)lp;
    WCHAR t[200];

    if (!IsWindowVisible(h)) return TRUE;
    GetWindowTextW(h, t, 200);
    if (wcscmp(t, f->cap)) return TRUE;
    if (f->want && !GetDlgItem(h, f->want)) return TRUE;
    if (f->not && GetDlgItem(h, f->not)) return TRUE;
    f->found = h;
    return FALSE;
}

static HWND wait_dlg(const WCHAR *cap, int want, int not)
{
    int i;
    struct find f = { cap, want, not, NULL };
    for (i = 0; i < 300; i++)
    {
        EnumThreadWindows(main_tid, find_proc, (LPARAM)&f);
        if (f.found) { Sleep(30); return f.found; }
        Sleep(30);
    }
    return NULL;
}

static BOOL CALLBACK kill_proc(HWND h, LPARAM lp)
{
    if (h != owner) { SendMessageW(h, WM_COMMAND, IDCANCEL, 0); SendMessageW(h, WM_COMMAND, IDNO, 0); }
    return TRUE;
}

static void abort_dialogs(void)
{
    int i;
    for (i = 0; i < 4; i++) { EnumThreadWindows(main_tid, kill_proc, 0); Sleep(100); }
}

static void click(HWND dlg, int id)
{
    /* posted: the handler may run a nested modal loop */
    PostMessageW(dlg, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(dlg, id));
    Sleep(120);
}

static BOOL enabled(HWND dlg, int id) { return IsWindowEnabled(GetDlgItem(dlg, id)); }
static BOOL shown(HWND dlg, int id) { return IsWindowVisible(GetDlgItem(dlg, id)); }

static BOOL text_has(HWND dlg, int id, const WCHAR *sub)
{
    WCHAR buf[2048];
    GetDlgItemTextW(dlg, id, buf, 2048);
    return wcsstr(buf, sub) != NULL;
}

static void select_driver(HWND dlg, int id, const WCHAR *name)
{
    HWND lb = GetDlgItem(dlg, id);
    LRESULT i = SendMessageW(lb, LB_FINDSTRINGEXACT, -1, (LPARAM)name);
    SendMessageW(lb, LB_SETCURSEL, i, 0);
    SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(id, LBN_SELCHANGE), (LPARAM)lb);
}

typedef void (*script_fn)(void);
static DWORD WINAPI thread_proc(void *p)
{
    int i;
    ((script_fn)p)();
    InterlockedExchange(&script_done, 1);
    for (i = 0; i < 100 && !call_returned; i++) Sleep(100);
    if (!call_returned) { printf("FAIL  dialog still open after its script finished\n"); failures++; abort_dialogs(); }
    return 0;
}

/* run fn in a helper thread while the caller makes the blocking dialog call */
#define RUN(fn, call) do { HANDLE th; script_done = 0; call_returned = 0; th = CreateThread(NULL, 0, thread_proc, (void *)fn, 0, NULL); call; InterlockedExchange(&call_returned, 1); \
    if (WaitForSingleObject(th, 20000) == WAIT_TIMEOUT) { printf("FAIL  script %s hung\n", #fn); failures++; abort_dialogs(); WaitForSingleObject(th, 5000); } \
    CloseHandle(th); } while (0)

static WCHAR filedsn_dir[MAX_PATH];

static void read_file(const WCHAR *path, char *buf, int n)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD got = 0;
    buf[0] = 0;
    if (h == INVALID_HANDLE_VALUE) return;
    ReadFile(h, buf, n - 1, &got, NULL);
    buf[got] = 0;
    CloseHandle(h);
}

static DWORD last_error_code(void)
{
    DWORD code = 0;
    if (SQLInstallerErrorW(1, &code, NULL, 0, NULL) == SQL_NO_DATA) return 0;
    return code;
}

static void get_key(const WCHAR *file, const WCHAR *key, WCHAR *out)
{
    GetPrivateProfileStringW(L"ODBC", key, L"<none>", out, 256, file);
}

/* ---- wizard scripts ---- */

static void script_cancel_first(void)
{
    HWND w = wait_dlg(L"Create New Data Source", IDC_TITLE, 0);
    CHECK(w != NULL, "no wizard");
    if (!w) return;
    CHECK(IsDlgButtonChecked(w, IDC_TYPE_FILE) == BST_CHECKED, "File Data Source should be the default");
    CHECK(!enabled(w, IDC_BACK), "Back enabled on first page");
    click(w, IDCANCEL);
}

static void script_file_dsn(void)
{
    HWND w = wait_dlg(L"Create New Data Source", IDC_TITLE, 0), a, m;
    if (!w) { failures++; printf("FAIL  no wizard\n"); return; }
    click(w, IDC_NEXT);
    CHECK(text_has(w, IDC_TITLE, L"Select a driver"), "page 2");
    CHECK(!enabled(w, IDC_NEXT), "Next enabled without a driver");
    CHECK(shown(w, IDC_ADVANCED), "Advanced hidden for a file DSN");
    select_driver(w, IDC_WDRIVERS, L"SG Test Driver");
    CHECK(enabled(w, IDC_NEXT), "Next disabled after selecting a driver");
    click(w, IDC_ADVANCED);
    a = wait_dlg(L"Advanced File DSN Creation Settings", IDC_ADV_TEXT, 0);
    CHECK(a != NULL && IsDlgButtonChecked(a, IDC_VERIFY) == BST_CHECKED, "verify default");
    if (a)
    {
        SetDlgItemTextW(a, IDC_ADV_TEXT, L"UID=bob\r\nDSN=ignored\r\nPWD=pw\r\nnokeyword");
        CheckDlgButton(a, IDC_VERIFY, BST_UNCHECKED);
        click(a, IDOK);
    }
    Sleep(100);
    click(w, IDC_NEXT);
    CHECK(text_has(w, IDC_TITLE, L"name of the file"), "file page");
    SetDlgItemTextW(w, IDC_FILENAME, L"a?b");
    click(w, IDC_NEXT);
    m = wait_dlg(L"Create New Data Source", IDOK, IDC_TITLE);
    CHECK(m != NULL, "no invalid file name message");
    if (m) click(m, IDOK);
    Sleep(150);
    CHECK(text_has(w, IDC_TITLE, L"name of the file"), "left the file page after an invalid name");
    SetDlgItemTextW(w, IDC_FILENAME, L"mine");
    click(w, IDC_NEXT);
    CHECK(shown(w, IDC_FINISH) && !shown(w, IDC_NEXT), "Finish not shown");
    CHECK(text_has(w, IDC_REVIEW, L"SG Test Driver") && text_has(w, IDC_REVIEW, L"mine.dsn") && text_has(w, IDC_REVIEW, L"File Data Source"), "review text");
    click(w, IDC_BACK);
    CHECK(text_has(w, IDC_TITLE, L"name of the file"), "Back from review");
    click(w, IDC_NEXT);
    click(w, IDC_FINISH);
}

static void script_verify(void)
{
    HWND w = wait_dlg(L"Create New Data Source", IDC_TITLE, 0), a;
    if (!w) { failures++; printf("FAIL  no wizard\n"); return; }
    click(w, IDC_NEXT);
    select_driver(w, IDC_WDRIVERS, L"SG Test Driver");
    click(w, IDC_ADVANCED);
    a = wait_dlg(L"Advanced File DSN Creation Settings", IDC_ADV_TEXT, 0);
    if (a) { SetDlgItemTextW(a, IDC_ADV_TEXT, L"OK=1\r\nUID=u\r\nPWD=secret"); click(a, IDOK); }
    Sleep(100);
    click(w, IDC_NEXT);
    SetDlgItemTextW(w, IDC_FILENAME, L"C:\\sgtest\\verified");
    click(w, IDC_NEXT);
    click(w, IDC_FINISH);
}

static void script_verify_fail(void)
{
    HWND w = wait_dlg(L"Create New Data Source", IDC_TITLE, 0), m;
    int no = failures; /* unused */
    (void)no;
    if (!w) { failures++; printf("FAIL  no wizard\n"); return; }
    click(w, IDC_NEXT);
    select_driver(w, IDC_WDRIVERS, L"SG Test Driver");
    click(w, IDC_NEXT);
    SetDlgItemTextW(w, IDC_FILENAME, L"C:\\sgtest\\failing");
    click(w, IDC_NEXT);
    click(w, IDC_FINISH);
    m = wait_dlg(L"Create New Data Source", IDYES, IDC_TITLE);
    CHECK(m != NULL, "no connection warning");
    if (m) click(m, IDNO);
}

static void script_verify_fail_continue(void)
{
    HWND w = wait_dlg(L"Create New Data Source", IDC_TITLE, 0), m;
    if (!w) { failures++; printf("FAIL  no wizard\n"); return; }
    click(w, IDC_NEXT);
    select_driver(w, IDC_WDRIVERS, L"SG Test Driver");
    click(w, IDC_NEXT);
    SetDlgItemTextW(w, IDC_FILENAME, L"C:\\sgtest\\forced.dsn");
    click(w, IDC_NEXT);
    click(w, IDC_FINISH);
    m = wait_dlg(L"Create New Data Source", IDYES, IDC_TITLE);
    CHECK(m != NULL, "no connection warning (continue)");
    if (m) click(m, IDYES);
}

static void script_user_dsn(void)
{
    HWND w = wait_dlg(L"Create New Data Source", IDC_TITLE, 0);
    if (!w) { failures++; printf("FAIL  no wizard\n"); return; }
    CheckRadioButton(w, IDC_TYPE_USER, IDC_TYPE_FILE, IDC_TYPE_USER);
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_TYPE_USER, BN_CLICKED), (LPARAM)GetDlgItem(w, IDC_TYPE_USER));
    click(w, IDC_NEXT);
    CHECK(!shown(w, IDC_ADVANCED), "Advanced visible for a user DSN");
    select_driver(w, IDC_WDRIVERS, L"SG Test Driver");
    click(w, IDC_NEXT);
    CHECK(shown(w, IDC_FINISH) && text_has(w, IDC_REVIEW, L"User Data Source"), "user review page");
    click(w, IDC_FINISH);
}

/* ---- manage dialog ---- */

static int list_count(HWND d) { return (int)SendMessageW(GetDlgItem(d, IDC_LIST), LVM_GETITEMCOUNT, 0, 0); }

static void list_select(HWND d, int i)
{
    LVITEMW it = { 0 };
    it.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    it.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(GetDlgItem(d, IDC_LIST), LVM_SETITEMSTATE, i, (LPARAM)&it);
}

static void goto_tab(HWND d, int t)
{
    NMHDR nm = { GetDlgItem(d, IDC_TAB), IDC_TAB, TCN_SELCHANGE };
    SendMessageW(GetDlgItem(d, IDC_TAB), TCM_SETCURSEL, t, 0);
    SendMessageW(d, WM_NOTIFY, IDC_TAB, (LPARAM)&nm);
}

static void script_manage(void)
{
    HWND d = wait_dlg(L"ODBC Data Source Administrator", IDC_TAB, 0), p, m;
    if (!d) { failures++; printf("FAIL  no manage dialog\n"); return; }
    CHECK(list_count(d) == 1, "user DSN count %d", list_count(d));
    CHECK(!enabled(d, IDC_REMOVE) && !enabled(d, IDC_CONFIGURE) && enabled(d, IDC_ADD), "buttons without selection");
    list_select(d, 0);
    CHECK(enabled(d, IDC_REMOVE) && enabled(d, IDC_CONFIGURE), "buttons with selection");
    click(d, IDC_CONFIGURE);
    click(d, IDC_ADD);
    p = wait_dlg(L"Create New Data Source", IDC_DRIVERLIST, 0);
    CHECK(p != NULL, "no driver picker");
    if (p) { select_driver(p, IDC_DRIVERLIST, L"SG Test Driver"); click(p, IDOK); }
    Sleep(150);
    CHECK(list_count(d) == 2, "after add %d", list_count(d));
    list_select(d, 0);
    click(d, IDC_REMOVE);
    m = wait_dlg(L"ODBC Data Source Administrator", IDYES, IDC_TAB);
    CHECK(m != NULL, "no remove confirmation");
    if (m) click(m, IDYES);
    Sleep(150);
    CHECK(list_count(d) == 1, "after remove %d", list_count(d));
    goto_tab(d, 1);
    CHECK(list_count(d) == 1, "system DSN count %d", list_count(d));
    goto_tab(d, 2);
    CHECK(list_count(d) >= 1, "file DSN count %d", list_count(d));
    list_select(d, 0);
    CHECK(enabled(d, IDC_REMOVE) && !enabled(d, IDC_CONFIGURE), "file tab buttons");
    click(d, IDC_REMOVE);
    m = wait_dlg(L"ODBC Data Source Administrator", IDYES, IDC_TAB);
    if (m) click(m, IDYES);
    Sleep(150);
    goto_tab(d, 3);
    CHECK(list_count(d) == 1 && !enabled(d, IDC_ADD), "drivers tab count %d", list_count(d));
    goto_tab(d, 4);
    CHECK(shown(d, IDC_TRACE_FILE) && !shown(d, IDC_LIST), "tracing tab");
    SetDlgItemTextW(d, IDC_TRACE_FILE, L"C:\\sg.trc");
    CheckDlgButton(d, IDC_TRACE_ON, BST_CHECKED);
    click(d, IDOK);
}

/* ---- translator ---- */

static void script_xlat_cancel(void)
{
    HWND d = wait_dlg(L"Select Translator", IDC_TRANSLIST, 0);
    CHECK(d != NULL, "no translator dialog");
    if (!d) return;
    CHECK(SendMessageW(GetDlgItem(d, IDC_TRANSLIST), LB_GETCOUNT, 0, 0) == 2, "translator count");
    CHECK(SendMessageW(GetDlgItem(d, IDC_TRANSLIST), LB_GETCURSEL, 0, 0) == 0, "<No Translator> default");
    click(d, IDCANCEL);
}

static void script_xlat_none(void)
{
    HWND d = wait_dlg(L"Select Translator", IDC_TRANSLIST, 0);
    if (!d) { failures++; return; }
    click(d, IDOK);
}

static void script_xlat_pick(void)
{
    HWND d = wait_dlg(L"Select Translator", IDC_TRANSLIST, 0);
    if (!d) { failures++; return; }
    CHECK(SendMessageW(GetDlgItem(d, IDC_TRANSLIST), LB_GETCURSEL, 0, 0) == 1, "preselected translator");
    click(d, IDOK);
}

static void script_xlat_refuse(void)
{
    HWND d = wait_dlg(L"Select Translator", IDC_TRANSLIST, 0);
    if (!d) { failures++; return; }
    click(d, IDOK);         /* ConfigTranslator returns FALSE: back to the dialog */
    Sleep(300);
    CHECK(IsWindow(d) && IsWindowVisible(d), "dialog closed after a refused ConfigTranslator");
    click(d, IDCANCEL);
}

static void regset(HKEY root, const WCHAR *key, const WCHAR *name, const WCHAR *val)
{
    HKEY k;
    if (!RegCreateKeyExW(root, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL))
    {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (lstrlenW(val) + 1) * sizeof(WCHAR));
        RegCloseKey(k);
    }
}

static void get_reg_string(HKEY root, const WCHAR *key, const WCHAR *name, WCHAR *out, DWORD n)
{
    DWORD sz = n * sizeof(WCHAR);
    out[0] = 0;
    RegGetValueW(root, key, name, RRF_RT_REG_SZ, NULL, out, &sz);
}

static void dump_sources(const char *tag)
{
    HKEY roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }, k;
    int r;
    for (r = 0; r < 2; r++)
    {
        DWORD i, nl, dl;
        WCHAR n[100], d[100];
        if (RegOpenKeyExW(roots[r], L"Software\\ODBC\\ODBC.INI\\ODBC Data Sources", 0, KEY_READ, &k)) continue;
        for (i = 0; nl = 100, dl = 200, !RegEnumValueW(k, i, n, &nl, NULL, NULL, (BYTE *)d, &dl); i++)
            printf("  [%s] %s: %ls = %ls\n", tag, r ? "HKLM" : "HKCU", n, d);
        RegCloseKey(k);
    }
}

int main(void)
{
    WCHAR buf[300], path[MAX_PATH], name[100], dir[MAX_PATH];
    char text[2048];
    BOOL ret;
    WORD nlen, plen;
    DWORD opt;
    WNDCLASSW wc = { 0 };

    setvbuf(stdout, NULL, _IONBF, 0);
    main_tid = GetCurrentThreadId();
    InitCommonControls();
    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"SgOdbcOwner";
    wc.hInstance = GetModuleHandleW(NULL);
    RegisterClassW(&wc);
    owner = CreateWindowW(L"SgOdbcOwner", L"owner", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 100, NULL, NULL, wc.hInstance, NULL);

    /* registry fixtures: the prefix's ODBCINST.INI only */
    regset(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBCINST.INI\\ODBC Drivers", L"SG Test Driver", L"Installed");
    regset(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBCINST.INI\\SG Test Driver", L"Driver", L"C:\\sgodbc.dll");
    regset(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBCINST.INI\\SG Test Driver", L"Setup", L"C:\\sgodbc.dll");
    regset(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBCINST.INI\\ODBC Translators", L"SG Xlat", L"Installed");
    regset(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBCINST.INI\\SG Xlat", L"Translator", L"C:\\sgxlat.dll");
    regset(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBCINST.INI\\SG Xlat", L"Setup", L"C:\\sgodbc.dll");
    DeleteFileW(L"C:\\sgodbc.log");
    DeleteFileW(L"C:\\sgodbc.cancel");

    get_reg_string(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion", L"CommonFileDir", dir, MAX_PATH);
    if (!dir[0]) GetEnvironmentVariableW(L"CommonProgramFiles", dir, MAX_PATH);
    wcscpy(filedsn_dir, dir);
    wcscat(filedsn_dir, L"\\ODBC\\Data Sources");

    /* argument errors */
    ret = SQLManageDataSources(NULL);
    CHECK(!ret && last_error_code() == ODBC_ERROR_INVALID_HWND, "SQLManageDataSources(NULL) -> %d, err %lu", ret, last_error_code());
    ret = SQLCreateDataSourceW(NULL, NULL);
    CHECK(!ret && last_error_code() == ODBC_ERROR_INVALID_HWND, "SQLCreateDataSourceW(NULL) err %lu", last_error_code());
    ret = SQLCreateDataSourceW(owner, L"bad[name");
    CHECK(!ret && last_error_code() == ODBC_ERROR_INVALID_DSN, "invalid DSN err %lu", last_error_code());
    ret = SQLInstallODBCW(owner, L"C:\\odbc.inf", NULL, NULL);
    CHECK(!ret, "SQLInstallODBC succeeded");
    ret = SQLGetTranslatorW(NULL, name, 100, &nlen, path, MAX_PATH, &plen, &opt);
    CHECK(!ret && last_error_code() == ODBC_ERROR_INVALID_HWND, "SQLGetTranslatorW(NULL) err %lu", last_error_code());
    ret = SQLGetTranslatorW(owner, name, 0, &nlen, path, MAX_PATH, &plen, &opt);
    CHECK(!ret && last_error_code() == ODBC_ERROR_INVALID_BUFF_LEN, "cbNameMax 0 err %lu", last_error_code());
    ret = SQLGetTranslatorW(owner, name, 100, &nlen, path, 0, &plen, &opt);
    CHECK(!ret && last_error_code() == ODBC_ERROR_INVALID_BUFF_LEN, "cbPathMax 0 err %lu", last_error_code());
    ret = SQLGetTranslatorW(owner, name, 100, &nlen, path, MAX_PATH, &plen, NULL);
    CHECK(!ret, "NULL option succeeded");

    /* wizard: cancel */
    RUN(script_cancel_first, ret = SQLCreateDataSourceW(owner, NULL));
    CHECK(!ret && last_error_code() == ODBC_ERROR_USER_CANCELED, "cancel -> %d, err %lu", ret, last_error_code());

    /* wizard: file data source in the default folder */
    DeleteFileW(L"C:\\sgodbc.log");
    RUN(script_file_dsn, ret = SQLCreateDataSource(owner, ""));
    CHECK(ret, "file wizard failed, err %lu", last_error_code());
    swprintf(path, MAX_PATH, L"%ls\\mine.dsn", filedsn_dir);
    CHECK(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES, "%ls missing", path);
    get_key(path, L"DRIVER", buf); CHECK(!wcscmp(buf, L"SG Test Driver"), "DRIVER=%ls", buf);
    get_key(path, L"UID", buf); CHECK(!wcscmp(buf, L"bob"), "UID=%ls", buf);
    get_key(path, L"PWD", buf); CHECK(!wcscmp(buf, L"pw"), "PWD=%ls", buf);
    get_key(path, L"DSN", buf); CHECK(!wcscmp(buf, L"<none>"), "DSN written: %ls", buf);
    read_file(L"C:\\sgodbc.log", text, sizeof(text));
    CHECK(!strstr(text, "CONNECT"), "driver was asked to connect although verify was off");

    /* wizard: verified connection (driver reports keywords) */
    RUN(script_verify, ret = SQLCreateDataSourceW(owner, NULL));
    CHECK(ret, "verified wizard failed, err %lu", last_error_code());
    get_key(L"C:\\sgtest\\verified.dsn", L"DRIVER", buf); CHECK(!wcscmp(buf, L"SG Test Driver"), "verified DRIVER=%ls", buf);
    get_key(L"C:\\sgtest\\verified.dsn", L"OUT", buf); CHECK(!wcscmp(buf, L"1"), "driver output keyword OUT=%ls", buf);
    get_key(L"C:\\sgtest\\verified.dsn", L"UID", buf); CHECK(!wcscmp(buf, L"u"), "verified UID=%ls", buf);
    get_key(L"C:\\sgtest\\verified.dsn", L"PWD", buf); CHECK(!wcscmp(buf, L"<none>"), "password saved: %ls", buf);

    /* wizard: failing connection, user declines / continues */
    RUN(script_verify_fail, ret = SQLCreateDataSourceW(owner, NULL));
    CHECK(!ret && last_error_code() == ODBC_ERROR_CREATE_DSN_FAILED, "declined -> %d, err %lu", ret, last_error_code());
    CHECK(GetFileAttributesW(L"C:\\sgtest\\failing.dsn") == INVALID_FILE_ATTRIBUTES, "file created although declined");
    RUN(script_verify_fail_continue, ret = SQLCreateDataSourceW(owner, NULL));
    CHECK(ret && GetFileAttributesW(L"C:\\sgtest\\forced.dsn") != INVALID_FILE_ATTRIBUTES, "continue failed, err %lu", last_error_code());
    get_key(L"C:\\sgtest\\forced.dsn", L"DRIVER", buf); CHECK(!wcscmp(buf, L"SG Test Driver"), "forced DRIVER=%ls", buf);

    /* wizard: user data source -> ConfigDSN(ODBC_ADD_DSN) with the preset name */
    DeleteFileW(L"C:\\sgodbc.log");
    RUN(script_user_dsn, ret = SQLCreateDataSourceW(owner, L"Preset"));
    CHECK(ret, "user wizard failed, err %lu", last_error_code());
    read_file(L"C:\\sgodbc.log", text, sizeof(text));
    CHECK(strstr(text, "1|SG Test Driver|DSN=Preset"), "ADD log: %s", text);
    get_reg_string(HKEY_CURRENT_USER, L"Software\\ODBC\\ODBC.INI\\ODBC Data Sources", L"Preset", buf, 300);
    CHECK(!wcscmp(buf, L"SG Test Driver"), "Preset DSN not registered: %ls", buf);
    SQLRemoveDSNFromIni("Preset");

    /* manage dialog */
    SQLWriteDSNToIni("MyDSN", "SG Test Driver");
    SQLSetConfigMode(ODBC_SYSTEM_DSN);
    SQLWriteDSNToIni("SysDSN", "SG Test Driver");
    SQLSetConfigMode(ODBC_BOTH_DSN);
    DeleteFileW(L"C:\\sgodbc.log");
    RUN(script_manage, ret = SQLManageDataSources(owner));
    CHECK(ret, "SQLManageDataSources failed");
    read_file(L"C:\\sgodbc.log", text, sizeof(text));
    CHECK(strstr(text, "2|SG Test Driver|DSN=MyDSN"), "CONFIG log: %s", text);
    CHECK(strstr(text, "1|SG Test Driver|"), "ADD log: %s", text);
    CHECK(strstr(text, "3|SG Test Driver|DSN=MyDSN"), "REMOVE log: %s", text);
    get_reg_string(HKEY_CURRENT_USER, L"Software\\ODBC\\ODBC.INI\\ODBC Data Sources", L"MyDSN", buf, 300);
    CHECK(buf[0] == 0, "MyDSN still registered");
    get_reg_string(HKEY_CURRENT_USER, L"Software\\ODBC\\ODBC.INI\\ODBC Data Sources", L"NewDSN", buf, 300);
    CHECK(!wcscmp(buf, L"SG Test Driver"), "NewDSN missing");
    get_reg_string(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBC.INI\\ODBC", L"Trace", buf, 300);
    CHECK(!wcscmp(buf, L"1"), "Trace=%ls", buf);
    get_reg_string(HKEY_LOCAL_MACHINE, L"Software\\ODBC\\ODBC.INI\\ODBC", L"TraceFile", buf, 300);
    CHECK(!wcscmp(buf, L"C:\\sg.trc"), "TraceFile=%ls", buf);
    swprintf(path, MAX_PATH, L"%ls\\mine.dsn", filedsn_dir);
    CHECK(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES, "mine.dsn not removed by the File DSN tab");
    SQLRemoveDSNFromIni("NewDSN");
    SQLSetConfigMode(ODBC_SYSTEM_DSN);
    SQLRemoveDSNFromIni("SysDSN");
    SQLSetConfigMode(ODBC_BOTH_DSN);

    /* translators */
    RUN(script_xlat_cancel, ret = SQLGetTranslatorW(owner, name, 100, &nlen, path, MAX_PATH, &plen, &opt));
    CHECK(!ret, "cancelled translator dialog returned TRUE");

    wcscpy(name, L"keep"); wcscpy(path, L"keep"); opt = 77; nlen = plen = 99;
    RUN(script_xlat_none, ret = SQLGetTranslatorW(owner, name, 100, &nlen, path, MAX_PATH, &plen, &opt));
    CHECK(ret && nlen == 0 && plen == 0 && !wcscmp(name, L"keep") && !wcscmp(path, L"keep") && opt == 77,
          "<No Translator>: ret %d lens %u/%u name %ls path %ls opt %lu", ret, nlen, plen, name, path, opt);

    wcscpy(name, L"SG Xlat");
    RUN(script_xlat_pick, ret = SQLGetTranslatorW(owner, name, 100, &nlen, path, MAX_PATH, &plen, &opt));
    CHECK(ret && !wcscmp(name, L"SG Xlat") && nlen == 7 && !wcscmp(path, L"C:\\sgxlat.dll") && plen == 13 && opt == 0x1234,
          "pick: ret %d name %ls/%u path %ls/%u opt %#lx", ret, name, nlen, path, plen, opt);

    {
        char an[100] = "SG Xlat", ap[MAX_PATH];
        RUN(script_xlat_pick, ret = SQLGetTranslator(owner, an, 4, &nlen, ap, MAX_PATH, &plen, &opt));
        CHECK(ret && !strcmp(an, "SG ") && nlen == 7 && !strcmp(ap, "C:\\sgxlat.dll"), "ANSI truncation: %d '%s' %u", ret, an, nlen);
    }

    CloseHandle(CreateFileW(L"C:\\sgodbc.cancel", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    wcscpy(name, L"SG Xlat");
    RUN(script_xlat_refuse, ret = SQLGetTranslatorW(owner, name, 100, &nlen, path, MAX_PATH, &plen, &opt));
    CHECK(!ret, "refused translator returned TRUE");
    DeleteFileW(L"C:\\sgodbc.cancel");

    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
