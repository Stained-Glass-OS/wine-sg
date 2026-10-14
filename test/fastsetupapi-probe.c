/* setupapi gap-A batch (patch 3212), run by test/fastsetupapi-gate.sh.
 *   fastsetupapi-probe.exe WORKDIR (a directory in the prefix, e.g. C:\sgtest) */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static WCHAR work[MAX_PATH];

static BOOL SgStoreLocation(const WCHAR *name, const void *plat, const WCHAR *locale, WCHAR *out, DWORD size, DWORD *req)
{
    BOOL (WINAPI *fn)(const WCHAR *, const void *, const WCHAR *, WCHAR *, DWORD, DWORD *);
    fn = (void *)GetProcAddress(GetModuleHandleW(L"setupapi.dll"), "SetupGetInfDriverStoreLocationW");
    if (!fn) fn = (void *)GetProcAddress(LoadLibraryW(L"setupapi.dll"), "SetupGetInfDriverStoreLocationW");
    return fn(name, plat, locale, out, size, req);
}
#define SetupGetInfDriverStoreLocationW SgStoreLocation

static void write_file(const WCHAR *name, const char *text)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    DWORD n;
    swprintf(path, MAX_PATH, L"%ls\\%ls", work, name);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, text, strlen(text), &n, NULL);
    CloseHandle(h);
}

static void ini_get(const WCHAR *section, const WCHAR *key, WCHAR *out, DWORD size)
{
    WCHAR path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\test.ini", work);
    GetPrivateProfileStringW(section, key, L"<none>", out, size, path);
}

static BOOL reg_string(HKEY root, const WCHAR *key, const WCHAR *name, WCHAR *out, DWORD size)
{
    HKEY k;
    DWORD type, sz = size * sizeof(WCHAR);
    BOOL ok = FALSE;
    if (!RegOpenKeyExW(root, key, 0, KEY_READ, &k))
    {
        ok = !RegQueryValueExW(k, name, NULL, &type, (BYTE *)out, &sz) && type == REG_SZ;
        RegCloseKey(k);
    }
    return ok;
}

/* ---- INF directives */
static void test_directives(void)
{
    WCHAR inf[MAX_PATH], buf[256];
    HKEY key;
    BYTE bits[3] = { 0xf0, 0x00, 0x03 };
    BYTE got[3];
    DWORD size = sizeof(got), type;
    WCHAR oem_before[MAX_PATH], pattern[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE find;
    int before = 0, after = 0;
    BOOL ret;

    write_file(L"test.ini", "[Sec5]\r\nk=new\r\n[Sec]\r\nLine=a old b\r\nAdd=x y\r\nDel=p gone q\r\nWild=abc zzz\r\nComma=m\r\nSame=s added\r\n"
                            "[Sec2]\r\nk1=v1\r\nk2=v2\r\n[Sec3]\r\na=1\r\nb=2\r\n[Sec4]\r\nk=fresh\r\n");
    write_file(L"other.inf", "[Version]\r\nSignature=\"$Windows NT$\"\r\nClass=SgOther\r\nProvider=Sg\r\n[DefaultInstall]\r\n");
    swprintf(pattern, MAX_PATH, L"%ls\\test.ini", work);
    {
        char text[2048];
        char ini[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, pattern, -1, ini, MAX_PATH, NULL, NULL);
        snprintf(text, sizeof(text),
            "[Version]\r\nSignature=\"$Windows NT$\"\r\nClass=SgTest\r\n"
            "[DefaultInstall]\r\nUpdateIniFields=uif\r\nIni2Reg=i2r\r\nBitReg=br\r\nCopyINF=other.inf\r\n"
            "[uif]\r\n%%ini%%,Sec,Line,old,new,0\r\n%%ini%%,Sec,Add,,added,0\r\n%%ini%%,Sec,Del,gone,,0\r\n"
            "%%ini%%,Sec,Wild,a*c,X,1\r\n%%ini%%,Sec,Comma,,cc,2\r\n%%ini%%,Sec,Same,,added,0\r\n"
            "[i2r]\r\n%%ini%%,Sec2,k1,HKCU,Software\\SgTest\\One,1\r\n%%ini%%,Sec3,,HKCU,Software\\SgTest\\Two\r\n"
            "%%ini%%,Sec4,k,HKCU,Software\\SgTest\\Keep,0\r\n%%ini%%,Sec5,k,HKCU,Software\\SgTest\\Over,2\r\n"
            "[br]\r\nHKCU,Software\\SgTest\\Bits,Val,0x1,0x0f,1\r\nHKCU,Software\\SgTest\\Bits,Val,,0xf0,0\r\n"
            "[Strings]\r\nini=\"%s\"\r\n", ini);
        write_file(L"main.inf", text);
    }
    swprintf(inf, MAX_PATH, L"%ls\\main.inf", work);

    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SgTest\\Bits", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"Val", 0, REG_BINARY, bits, sizeof(bits));
    RegCloseKey(key);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SgTest\\Over", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"k", 0, REG_SZ, (BYTE *)L"old", 8);
    RegCloseKey(key);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SgTest\\Keep", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"k", 0, REG_SZ, (BYTE *)L"old", 8);
    RegCloseKey(key);

    GetWindowsDirectoryW(oem_before, MAX_PATH);
    swprintf(pattern, MAX_PATH, L"%ls\\inf\\oem*.inf", oem_before);
    if ((find = FindFirstFileW(pattern, &fd)) != INVALID_HANDLE_VALUE) { do before++; while (FindNextFileW(find, &fd)); FindClose(find); }

    {
        HINF hinf = SetupOpenInfFileW(inf, L"SgTest", INF_STYLE_WIN4, NULL);
        check(hinf != INVALID_HANDLE_VALUE, "INF opens with its own class");
        ret = hinf != INVALID_HANDLE_VALUE && SetupInstallFromInfSectionW(NULL, hinf, L"DefaultInstall",
                SPINST_INIFILES | SPINST_INI2REG | SPINST_BITREG | SPINST_COPYINF, HKEY_CURRENT_USER, NULL, 0, NULL, NULL, NULL, NULL);
        check(ret, "SetupInstallFromInfSection with the INI, Ini2Reg, BitReg and CopyINF directives");
        if (hinf != INVALID_HANDLE_VALUE) SetupCloseInfFile(hinf);
    }

    ini_get(L"Sec", L"Line", buf, 256);   check(!wcscmp(buf, L"a new b"), "UpdateIniFields replaces a field");
    ini_get(L"Sec", L"Add", buf, 256);    check(!wcscmp(buf, L"x y added"), "UpdateIniFields appends a field with a space");
    ini_get(L"Sec", L"Del", buf, 256);    check(!wcscmp(buf, L"p q"), "UpdateIniFields deletes a field");
    ini_get(L"Sec", L"Wild", buf, 256);   check(!wcscmp(buf, L"X zzz"), "UpdateIniFields wildcard match");
    ini_get(L"Sec", L"Comma", buf, 256);  check(!wcscmp(buf, L"m,cc"), "UpdateIniFields appends with a comma");
    ini_get(L"Sec", L"Same", buf, 256);   check(!wcscmp(buf, L"s added"), "UpdateIniFields does not add a field twice");

    check(reg_string(HKEY_CURRENT_USER, L"Software\\SgTest\\One", L"k1", buf, 256) && !wcscmp(buf, L"v1"), "Ini2Reg copies one INI key");
    ini_get(L"Sec2", L"k1", buf, 256);    check(!wcscmp(buf, L"<none>"), "Ini2Reg flag 1 removes the key from the INI");
    ini_get(L"Sec2", L"k2", buf, 256);    check(!wcscmp(buf, L"v2"), "other keys of the INI section stay");
    check(reg_string(HKEY_CURRENT_USER, L"Software\\SgTest\\Two", L"a", buf, 256) && !wcscmp(buf, L"1") &&
          reg_string(HKEY_CURRENT_USER, L"Software\\SgTest\\Two", L"b", buf, 256) && !wcscmp(buf, L"2"), "Ini2Reg copies a whole section");
    check(reg_string(HKEY_CURRENT_USER, L"Software\\SgTest\\Over", L"k", buf, 256) && !wcscmp(buf, L"new"), "Ini2Reg flag 2 replaces an existing value");
    check(reg_string(HKEY_CURRENT_USER, L"Software\\SgTest\\Keep", L"k", buf, 256) && !wcscmp(buf, L"old"), "Ini2Reg leaves an existing subkey alone");

    RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SgTest\\Bits", 0, KEY_READ, &key);
    memset(got, 0xee, sizeof(got));
    RegQueryValueExW(key, L"Val", NULL, &type, got, &size);
    RegCloseKey(key);
    check(got[1] == 0x0f && got[0] == 0x00 && got[2] == 0x03, "BitReg sets and clears bits of the indexed byte");

    {
        WCHAR copied[MAX_PATH];
        GetWindowsDirectoryW(copied, MAX_PATH);
        wcscat(copied, L"\\inf\\other.inf");
        after = GetFileAttributesW(copied) != INVALID_FILE_ATTRIBUTES;
        check(after, "CopyINF copies the INF into the INF directory");
        DeleteFileW(copied);
        (void)before;
    }
    {
        HINF hinf = SetupOpenInfFileW(inf, L"WrongClass", INF_STYLE_WIN4, NULL);
        check(hinf == INVALID_HANDLE_VALUE && GetLastError() == ERROR_CLASS_MISMATCH, "SetupOpenInfFile with another class fails with ERROR_CLASS_MISMATCH");
        hinf = SetupOpenInfFileW(inf, NULL, INF_STYLE_WIN4, NULL);
        check(hinf != INVALID_HANDLE_VALUE, "no class: any INF opens");
        if (hinf != INVALID_HANDLE_VALUE) SetupCloseInfFile(hinf);
    }
}

/* ---- copy OEM INF: original name and driver store location */
static void test_original(void)
{
    WCHAR src[MAX_PATH], dest[MAX_PATH], store[MAX_PATH], buf[MAX_PATH];
    SP_ORIGINAL_FILE_INFO_W orig = { sizeof(orig) };
    SP_INF_INFORMATION *info;
    DWORD size, req = 0;
    char big[4096];
    SP_ALTPLATFORM_INFO_V2 plat = { sizeof(plat) };

    write_file(L"myorig.inf", "[Version]\r\nSignature=\"$Windows NT$\"\r\nClass=SgOrig\r\nClassGUID={11111111-2222-3333-4444-555555555555}\r\nProvider=Sg\r\n[DefaultInstall]\r\n");
    swprintf(src, MAX_PATH, L"%ls\\myorig.inf", work);
    check(SetupCopyOEMInfW(src, NULL, SPOST_PATH, 0, dest, MAX_PATH, NULL, NULL), "SetupCopyOEMInf");
    { char shown[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, dest, -1, shown, MAX_PATH, NULL, NULL); printf("dest=%s\n", shown); }

    info = (SP_INF_INFORMATION *)big;
    size = sizeof(big);
    check(SetupGetInfInformationW(dest, INFINFO_INF_NAME_IS_ABSOLUTE, info, size, &size), "SetupGetInfInformation");
    check(SetupQueryInfOriginalFileInformationW(info, 0, NULL, &orig), "SetupQueryInfOriginalFileInformation");
    check(!wcscmp(orig.OriginalInfName, L"myorig.inf"), "the original INF name is that of the file copied");
    plat.cbSize = 5;
    check(!SetupQueryInfOriginalFileInformationW(info, 0, (SP_ALTPLATFORM_INFO *)&plat, &orig) && GetLastError() == ERROR_INVALID_PARAMETER, "a bad platform size is refused");
    check(!SetupQueryInfOriginalFileInformationW(info, 5, NULL, &orig) && GetLastError() == ERROR_INVALID_PARAMETER, "an INF index out of range is refused");

    {
        /* an INF whose .pnf names another file it was copied from */
        WCHAR pnf[MAX_PATH], renamed[MAX_PATH], realstore[MAX_PATH], from[MAX_PATH], text[MAX_PATH * 2];
        char pnftext[MAX_PATH * 2];
        HANDLE h;
        DWORD n;

        swprintf(realstore, MAX_PATH, L"%ls\\realstore", work);
        CreateDirectoryW(realstore, NULL);
        swprintf(from, MAX_PATH, L"%ls\\realname.inf", realstore);
        write_file(L"realstore\\realname.inf", "[Version]\r\nSignature=\"$Windows NT$\"\r\n");
        write_file(L"renamed.inf", "[Version]\r\nSignature=\"$Windows NT$\"\r\nClass=SgOrig\r\n");
        swprintf(pnf, MAX_PATH, L"%ls\\renamed.pnf", work);
        snprintf(pnftext, sizeof(pnftext), "Wine PNF header\n%ls\n", from);
        (void)text;
        h = CreateFileW(pnf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        {
            char fromA[MAX_PATH * 2];
            WideCharToMultiByte(CP_ACP, 0, from, -1, fromA, sizeof(fromA), NULL, NULL);
            snprintf(pnftext, sizeof(pnftext), "Wine PNF header\n%s\n", fromA);
        }
        WriteFile(h, pnftext, strlen(pnftext), &n, NULL);
        CloseHandle(h);
        swprintf(renamed, MAX_PATH, L"%ls\\renamed.inf", work);
        size = sizeof(big);
        check(SetupGetInfInformationW(renamed, INFINFO_INF_NAME_IS_ABSOLUTE, info, size, &size), "SetupGetInfInformation of an INF with a .pnf");
        check(SetupQueryInfOriginalFileInformationW(info, 0, NULL, &orig) && !wcscmp(orig.OriginalInfName, L"realname.inf"),
              "the original name comes from the .pnf");
        check(SetupGetInfDriverStoreLocationW(renamed, NULL, NULL, store, MAX_PATH, &req) && !wcscmp(store, from),
              "the store location comes from the .pnf");
    }

    wcscpy(buf, wcsrchr(dest, '\\') + 1);
    check(SetupGetInfDriverStoreLocationW(buf, NULL, NULL, store, MAX_PATH, &req), "SetupGetInfDriverStoreLocation by name");
    check(req == wcslen(store) + 1 && wcsstr(store, L"myorig.inf"), "it is the store copy under the original name");
    check(GetFileAttributesW(store) != INVALID_FILE_ATTRIBUTES, "the store copy exists");
    check(SetupGetInfDriverStoreLocationW(dest, NULL, NULL, NULL, 0, &req) && req == wcslen(store) + 1, "size query with a full path");
    check(!SetupGetInfDriverStoreLocationW(buf, NULL, NULL, store, 3, &req) && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "small buffer");
    check(!SetupGetInfDriverStoreLocationW(L"nosuchfile.inf", NULL, NULL, store, MAX_PATH, &req) && GetLastError() == ERROR_FILE_NOT_FOUND, "missing INF");
    check(!SetupGetInfDriverStoreLocationW(NULL, NULL, NULL, store, MAX_PATH, &req) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL name");
}

/* ---- class of an INF found through the INF directory */
static void test_inf_class(void)
{
    WCHAR windir[MAX_PATH], path[MAX_PATH], name[64];
    GUID guid;
    char text[] = "[Version]\r\nSignature=\"$Windows NT$\"\r\nClass=SgSearch\r\nClassGUID={22222222-3333-4444-5555-666666666666}\r\n";
    HANDLE h;
    DWORD n;

    GetWindowsDirectoryW(windir, MAX_PATH);
    swprintf(path, MAX_PATH, L"%ls\\inf\\sgsearch.inf", windir);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, text, strlen(text), &n, NULL);
    CloseHandle(h);
    memset(name, 0, sizeof(name));
    check(SetupDiGetINFClassW(L"sgsearch.inf", &guid, name, 64, NULL), "SetupDiGetINFClass finds a bare INF name in the INF directory");
    check(!wcscmp(name, L"SgSearch") && guid.Data1 == 0x22222222, "with its class");
    check(!SetupDiGetINFClassW(L"nosuchfile.inf", &guid, name, 64, NULL) && GetLastError() == ERROR_FILE_NOT_FOUND, "a missing bare name still fails");
    DeleteFileW(path);
}

/* ---- file queue alternate platform */
static void test_alt_platform(void)
{
    HSPFILEQ q = SetupOpenFileQueue();
    SP_ALTPLATFORM_INFO_V2 v2 = { sizeof(v2), VER_PLATFORM_WIN32_NT, 6, 1, PROCESSOR_ARCHITECTURE_AMD64 };
    SP_ALTPLATFORM_INFO_V1 v1 = { sizeof(v1), VER_PLATFORM_WIN32_NT, 5, 1, PROCESSOR_ARCHITECTURE_INTEL };
    DWORD bad = 7;

    check(q != INVALID_HANDLE_VALUE && q != NULL, "queue");
    check(SetupSetFileQueueAlternatePlatformW(q, (SP_ALTPLATFORM_INFO *)&v2, L"x.cat"), "set platform V2 with a catalog");
    check(SetupSetFileQueueAlternatePlatformW(q, (SP_ALTPLATFORM_INFO *)&v1, NULL), "set platform V1");
    check(SetupSetFileQueueAlternatePlatformA(q, (SP_ALTPLATFORM_INFO *)&v2, "y.cat"), "ANSI variant");
    check(!SetupSetFileQueueAlternatePlatformW(q, (SP_ALTPLATFORM_INFO *)&bad, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "bad cbSize refused");
    check(!SetupSetFileQueueAlternatePlatformW(q, NULL, L"z.cat") && GetLastError() == ERROR_INVALID_PARAMETER, "a catalog without a platform is refused");
    check(SetupSetFileQueueAlternatePlatformW(q, NULL, NULL), "NULL clears the platform");
    check(!SetupSetFileQueueAlternatePlatformW((HSPFILEQ)INVALID_HANDLE_VALUE, NULL, NULL) && GetLastError() == ERROR_INVALID_HANDLE, "bad queue handle");
    SetupCloseFileQueue(q);
}

/* ---- error dialogs: a helper thread presses a button */
static int press_id;
static DWORD main_thread;
static BOOL CALLBACK find_dialog(HWND hwnd, LPARAM lp)
{
    WCHAR cls[32];
    GetClassNameW(hwnd, cls, 32);
    if (!wcscmp(cls, L"#32770")) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}
static DWORD WINAPI presser(void *arg)
{
    HWND dlg = NULL;
    int i;
    for (i = 0; i < 100 && !dlg; i++) { Sleep(100); EnumThreadWindows(main_thread, find_dialog, (LPARAM)&dlg); }
    if (dlg)
    {
        /* record whether the dialog has the Ignore (skip) button, then press */
        if (press_id == -1) { press_id = GetDlgItem(dlg, IDIGNORE) ? 1 : 0; PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0); PostMessageW(dlg, WM_COMMAND, IDABORT, 0); }
        else PostMessageW(dlg, WM_COMMAND, press_id, 0);
    }
    return 0;
}
static UINT with_button(int id, UINT (*call)(void))
{
    HANDLE t;
    UINT r;
    press_id = id;
    main_thread = GetCurrentThreadId();
    t = CreateThread(NULL, 0, presser, NULL, 0, NULL);
    r = call();
    WaitForSingleObject(t, 12000);
    CloseHandle(t);
    return r;
}
static DWORD style_arg;
static UINT call_delete(void) { return SetupDeleteErrorW(NULL, L"T", L"C:\\x\\file.txt", ERROR_ACCESS_DENIED, style_arg); }
static UINT call_rename(void) { return SetupRenameErrorW(NULL, L"T", L"C:\\a", L"C:\\b", ERROR_SHARING_VIOLATION, style_arg); }
static WCHAR pathbuf[64];
static DWORD pathreq;
static UINT call_copy(void) { return SetupCopyErrorW(NULL, L"T", L"Disk", L"D:\\src", L"f.dll", L"C:\\dst", ERROR_FILE_NOT_FOUND, style_arg, pathbuf, 64, &pathreq); }
static UINT call_copy_a(void)
{
    char buf[64];
    UINT r = SetupCopyErrorA(NULL, "T", "Disk", "D:\\src", "f.dll", "C:\\dst", ERROR_FILE_NOT_FOUND, style_arg, buf, 64, &pathreq);
    if (r == DPROMPT_SUCCESS) MultiByteToWideChar(CP_ACP, 0, buf, -1, pathbuf, 64);
    return r;
}
static UINT call_delete_a(void) { return SetupDeleteErrorA(NULL, "T", "C:\\x\\file.txt", ERROR_ACCESS_DENIED, style_arg); }
static UINT call_rename_a(void) { return SetupRenameErrorA(NULL, "T", "C:\\a", "C:\\b", ERROR_SHARING_VIOLATION, style_arg); }

static void test_errors(void)
{
    UINT r;
    style_arg = 0;
    check(with_button(IDRETRY, call_delete) == DPROMPT_SUCCESS, "SetupDeleteError: Retry");
    check(with_button(IDIGNORE, call_delete) == DPROMPT_SKIPFILE, "SetupDeleteError: Skip");
    check(with_button(IDABORT, call_delete) == DPROMPT_CANCEL, "SetupDeleteError: Cancel");
    check(with_button(IDIGNORE, call_delete_a) == DPROMPT_SKIPFILE, "SetupDeleteErrorA: Skip");
    check(with_button(IDRETRY, call_rename) == DPROMPT_SUCCESS, "SetupRenameError: Retry");
    check(with_button(IDIGNORE, call_rename_a) == DPROMPT_SKIPFILE, "SetupRenameErrorA: Skip");
    pathbuf[0] = 0;
    r = with_button(IDRETRY, call_copy);
    check(r == DPROMPT_SUCCESS && !wcscmp(pathbuf, L"D:\\src") && pathreq == 7, "SetupCopyError: Retry returns the source path");
    pathbuf[0] = 0;
    r = with_button(IDRETRY, call_copy_a);
    check(r == DPROMPT_SUCCESS && !wcscmp(pathbuf, L"D:\\src"), "SetupCopyErrorA: Retry returns the source path");
    style_arg = IDF_NOSKIP;
    check(with_button(-1, call_delete) == DPROMPT_CANCEL, "IDF_NOSKIP: the dialog has no Skip button");
    check(press_id == 0, "(no Ignore control found)");
    style_arg = 0;
    with_button(-1, call_delete);
    check(press_id == 1, "without IDF_NOSKIP there is a Skip button");
    {
        UINT (*f)(void) = call_copy;
        WCHAR small[2];
        pathreq = 0;
        r = SetupCopyErrorW(NULL, L"T", L"Disk", L"D:\\src", L"f.dll", L"C:\\dst", 2, 0, small, 2, &pathreq);
        check(r == DPROMPT_BUFFERTOOSMALL && pathreq == 7, "SetupCopyError: a small buffer is reported before any dialog");
        (void)f;
    }
}

/* ---- devnodes and co-installers */
static void test_devnodes(void)
{
    DEVINST dn = 0, dn2 = 0, child = 0, parent = 0, root = 0;
    CONFIGRET cr;
    WCHAR id[MAX_DEVICE_ID_LEN];

    CM_Locate_DevNodeW(&root, NULL, 0);
    cr = CM_Create_DevNodeW(&dn, L"SGTEST\\DEV\\0001", root, 0);
    printf("create cr=%#lx dn=%lu root=%lu\n", cr, dn, root);
    check(cr == CR_SUCCESS , "CM_Create_DevNode");
    cr = CM_Locate_DevNodeW(&dn2, L"SGTEST\\DEV\\0001", 0);
    check(cr == CR_SUCCESS && dn2 == dn, "the new devnode is found");
    check(CM_Get_Device_IDW(dn, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS && !wcsicmp(id, L"SGTEST\\DEV\\0001"), "with its instance id");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\DEV\\0001", root, 0) == CR_ALREADY_SUCH_DEVNODE, "creating it again fails");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\DEV\\0002", root, 0x10) == CR_INVALID_FLAG, "unknown flag");
    check(CM_Create_DevNodeW(NULL, L"SGTEST\\DEV\\0003", root, 0) == CR_INVALID_POINTER, "NULL devnode pointer");
    check(CM_Create_DevNodeW(&dn2, NULL, 0, 0) == CR_INVALID_POINTER, "NULL id");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\ONLYTWO", root, 0) == CR_INVALID_DEVICE_ID, "an id needs three parts");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\GEN\\0005", root, CM_CREATE_DEVNODE_GENERATE_ID) == CR_INVALID_DEVICE_ID, "GENERATE_ID takes two parts");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\GEN", root, CM_CREATE_DEVNODE_GENERATE_ID) == CR_SUCCESS, "GENERATE_ID creates an instance id");
    check(CM_Get_Device_IDW(dn2, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS && !wcsicmp(id, L"SGTEST\\GEN\\0000"), "numbered 0000");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\GEN", root, CM_CREATE_DEVNODE_GENERATE_ID) == CR_SUCCESS &&
          CM_Get_Device_IDW(dn2, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS && !wcsicmp(id, L"SGTEST\\GEN\\0001"), "then 0001");
    check(CM_Create_DevNodeW(&child, L"SGTEST\\CHILD\\0001", dn, 0) == CR_SUCCESS, "create with a parent");
    check(CM_Get_Parent(&parent, child, 0) == CR_SUCCESS && parent == dn, "the parent is recorded");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\ORPHAN\\0001", 0xffff, 0) == CR_INVALID_DEVNODE, "an invalid parent is refused");
    check(CM_Create_DevNodeA(&dn2, "SGTEST\\ANSI\\0001", root, 0) == CR_SUCCESS, "ANSI variant");
    check(CM_Create_DevNodeW(&dn2, L"SGTEST\\PH\\0001", root, CM_CREATE_DEVNODE_PHANTOM) == CR_SUCCESS, "phantom devnode");
    check(CM_Locate_DevNodeW(&dn2, L"SGTEST\\PH\\0001", 0) == CR_NO_SUCH_DEVNODE, "a phantom is not present");
    check(CM_Locate_DevNodeW(&dn2, L"SGTEST\\PH\\0001", CM_LOCATE_DEVNODE_PHANTOM) == CR_SUCCESS, "but is found as a phantom");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Enum\\SGTEST");
}

static DWORD reg_dword(const char *name)
{
    HKEY key;
    DWORD v = 0, size = sizeof(v), type;
    if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\SgCoinst", 0, KEY_QUERY_VALUE, &key))
    {
        if (RegQueryValueExA(key, name, NULL, &type, (BYTE *)&v, &size)) v = 0;
        RegCloseKey(key);
    }
    return v;
}

static void test_coinstaller(const WCHAR *dll)
{
    static const GUID cls = { 0x33333333, 0x4444, 0x4444, { 0x55, 0x55, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66 } };
    WCHAR sys[MAX_PATH], dest[MAX_PATH], guidstr[64];
    HKEY key;
    HDEVINFO set;
    SP_DEVINFO_DATA data = { sizeof(data) };
    DWORD post_return = ERROR_ACCESS_DENIED;

    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(dest, MAX_PATH, L"%ls\\sgcoinst.dll", sys);
    check(CopyFileW(dll, dest, FALSE), "co-installer DLL copied to system32");
    StringFromGUID2(&cls, guidstr, 64);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\CoDeviceInstallers", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, guidstr, 0, REG_MULTI_SZ, (BYTE *)L"sgcoinst.dll\0", 14 * sizeof(WCHAR));
    RegCloseKey(key);

    set = SetupDiCreateDeviceInfoList(&cls, NULL);
    check(SetupDiCreateDeviceInfoW(set, L"SGCOI", &cls, NULL, NULL, DICD_GENERATE_ID, &data), "device info created");

    check(SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, &data), "default install function runs with a co-installer");
    check(reg_dword("Pre") == 1, "co-installer pre-processing ran once");
    check(reg_dword("Post") == 1, "co-installer post-processing ran once");
    printf("install result seen: %lu\n", reg_dword("PostInstallResult"));
    check(reg_dword("PostInstallResult") == 1000, "post-processing saw a successful result");
    check(reg_dword("PrivateOK") == 1, "post-processing kept the private data");

    RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\SgCoinst", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExA(key, "PostReturn", 0, REG_DWORD, (BYTE *)&post_return, 4);
    RegCloseKey(key);
    SetLastError(0);
    check(!SetupDiCallClassInstaller(DIF_INSTALLINTERFACES, set, &data) && GetLastError() == ERROR_ACCESS_DENIED,
          "post-processing can fail the call");
    check(reg_dword("Post") == 2, "post-processing ran again");
    SetupDiDestroyDeviceInfoList(set);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\SgCoinst");
    DeleteFileW(dest);
}

int main(int argc, char **argv)
{
    WCHAR dll[MAX_PATH];

    if (argc < 3) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, work, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, dll, MAX_PATH);
    CreateDirectoryW(work, NULL);
    test_directives();
    test_original();
    test_inf_class();
    test_alt_platform();
    test_errors();
    test_devnodes();
    test_coinstaller(dll);
    printf("RESULT: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures != 0;
}
