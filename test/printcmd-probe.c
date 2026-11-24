/* wine-sg 9923: SHInvokePrinterCommandW/A, run by test/printcmd-gate.sh with the printer "SG Cmd Printer"
 * (a PostScript driver package, LPT1: to a file). Imported by name (Shell32.lib names both; they were spec
 * stubs, so a caller crashed). Unknown printers and actions fail with the spooler's / parameter errors; a
 * test page prints (modal and not); PRINTACTION_NETINSTALLLINK makes a shortcut to the printer's item;
 * PRINTACTION_PROPERTIES and PRINTACTION_DOCUMENTDEFAULTS show their dialogs (the probe cancels them);
 * PRINTACTION_SERVERPROPERTIES is ERROR_NOT_SUPPORTED. */
#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>

#ifndef PRINTACTION_OPEN
#define PRINTACTION_PROPERTIES 1
#define PRINTACTION_NETINSTALLLINK 3
#define PRINTACTION_TESTPAGE 4
#define PRINTACTION_DOCUMENTDEFAULTS 6
#define PRINTACTION_SERVERPROPERTIES 7
#endif

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static DWORD main_tid;
static int seen;
static WCHAR seen_title[200];

static BOOL CALLBACK find_window(HWND hwnd, LPARAM lp)
{
    if (!IsWindowVisible(hwnd)) return TRUE;
    *(HWND *)lp = hwnd;
    return FALSE;
}

static DWORD WINAPI closer(void *arg)
{
    int i;
    for (i = 0; i < 300; i++)
    {
        HWND w = NULL;
        EnumThreadWindows(main_tid, find_window, (LPARAM)&w);
        if (w)
        {
            seen++;
            GetWindowTextW(w, seen_title, 200);
            Sleep(300);
            PostMessageW(w, WM_COMMAND, IDCANCEL, 0);
            PostMessageW(w, WM_CLOSE, 0, 0);
            return 0;
        }
        Sleep(50);
    }
    return 1;
}

static BOOL with_closer(UINT action, const WCHAR *name)
{
    HANDLE t;
    BOOL ret;
    seen = 0; seen_title[0] = 0;
    t = CreateThread(NULL, 0, closer, NULL, 0, NULL);
    ret = SHInvokePrinterCommandW(NULL, action, name, NULL, TRUE);
    WaitForSingleObject(t, 20000);
    CloseHandle(t);
    return ret;
}

int main(int argc, char **argv)
{
    static const WCHAR printer[] = L"SG Cmd Printer";
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    LPITEMIDLIST pidl = NULL;
    WCHAR *display = NULL;
    BOOL ret;

    main_tid = GetCurrentThreadId();
    CoInitialize(NULL);

    SetLastError(0xdead);
    ret = SHInvokePrinterCommandW(NULL, PRINTACTION_TESTPAGE, L"No Such Printer", NULL, TRUE);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PRINTER_NAME, "an unknown printer: ERROR_INVALID_PRINTER_NAME (ret %d, error %lu)", ret, GetLastError());
    SetLastError(0xdead);
    ret = SHInvokePrinterCommandA(NULL, PRINTACTION_TESTPAGE, "No Such Printer", NULL, FALSE);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PRINTER_NAME, "A, not modal, an unknown printer: ERROR_INVALID_PRINTER_NAME (ret %d, error %lu)", ret, GetLastError());
    SetLastError(0xdead);
    ret = SHInvokePrinterCommandW(NULL, 99, printer, NULL, TRUE);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown action: ERROR_INVALID_PARAMETER (ret %d, error %lu)", ret, GetLastError());
    SetLastError(0xdead);
    ret = SHInvokePrinterCommandW(NULL, PRINTACTION_SERVERPROPERTIES, L"\\\\sgserver", NULL, TRUE);
    CHECK(!ret && GetLastError() == ERROR_NOT_SUPPORTED, "server properties: ERROR_NOT_SUPPORTED (ret %d, error %lu)", ret, GetLastError());

    if (argc > 1 && !strcmp(argv[1], "page"))
    {
        ret = SHInvokePrinterCommandW(NULL, PRINTACTION_TESTPAGE, printer, NULL, TRUE);
        CHECK(ret, "a test page, modal (error %lu)", GetLastError());
        goto end;
    }
    ret = SHInvokePrinterCommandA(NULL, PRINTACTION_TESTPAGE, "SG Cmd Printer", NULL, FALSE);
    CHECK(ret, "a test page, A, not modal (error %lu)", GetLastError());
    Sleep(8000);   /* it prints on its own thread */

    CreateDirectoryW(L"C:\\links", NULL);
    ret = SHInvokePrinterCommandW(NULL, PRINTACTION_NETINSTALLLINK, printer, L"C:\\links", TRUE);
    CHECK(ret && GetFileAttributesW(L"C:\\links\\SG Cmd Printer.lnk") != INVALID_FILE_ATTRIBUTES, "a shortcut to the printer in C:\\links (ret %d, error %lu)", ret, GetLastError());
    {
        HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl);
        if (SUCCEEDED(hr)) hr = IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
        if (SUCCEEDED(hr)) hr = IPersistFile_Load(pf, L"C:\\links\\SG Cmd Printer.lnk", STGM_READ);
        if (SUCCEEDED(hr)) hr = IShellLinkW_GetIDList(sl, &pidl);
        if (SUCCEEDED(hr)) hr = SHGetNameFromIDList(pidl, SIGDN_NORMALDISPLAY, &display);
        CHECK(display && !wcscmp(display, printer), "the shortcut is the printer's item (\"%ls\", hr %08lx)", display ? display : L"", hr);
    }
    ret = SHInvokePrinterCommandW(NULL, PRINTACTION_NETINSTALLLINK, printer, NULL, TRUE);
    CHECK(!ret, "a shortcut with no folder fails (ret %d)", ret);

    ret = with_closer(PRINTACTION_PROPERTIES, printer);
    CHECK(seen == 1, "the printer's properties opened (\"%ls\"; ret %d)", seen_title, ret);
    ret = with_closer(PRINTACTION_DOCUMENTDEFAULTS, printer);
    CHECK(seen == 1, "its printing preferences opened (\"%ls\")", seen_title);
    CHECK(ret, "cancelling the preferences is not a failure (ret %d, error %lu)", ret, GetLastError());

end:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(failures ? 1 : 0);
}
