/* cupsjobs-probe: a CUPS printer's queue (wine-sg 9812, 9813).
 *
 *   cupsjobs-probe.exe jobs QUEUE      print a document to the stopped queue QUEUE, then list,
 *                                      pause, resume and cancel it as a Windows program does
 *   cupsjobs-probe.exe printer QUEUE   pause and resume the printer; its documents print
 *   cupsjobs-probe.exe window QUEUE    the queue window (printui /o) lists a document and
 *                                      pauses it from its Document menu
 * Prints "PASS ..." / "FAIL ..." lines and "RESULT: PASS" when all held. */
#include <windows.h>
#include <winspool.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

static int failures;
#define CHECK(c, ...) do { int ok_ = !!(c); printf(ok_ ? "PASS  " : "FAIL  "); printf(__VA_ARGS__); printf("\n"); \
                           failures += !ok_; fflush(stdout); } while (0)

/* a title with accented and CJK characters: CUPS got it cut short before 9812 */
static const WCHAR title[] = L"R\x00e9sum\x00e9 \x5c65\x6b74\x66f8 report.txt";

static DWORD print_doc(HANDLE h, const WCHAR *name)
{
    DOC_INFO_1W di = { (WCHAR *)name, NULL, (WCHAR *)L"RAW" };
    char data[3000];
    DWORD job, written = 0;

    memset(data, 'x', sizeof(data));
    memcpy(data, "plain text for the queue\n", 25);
    if (!(job = StartDocPrinterW(h, 1, (BYTE *)&di))) return 0;
    StartPagePrinter(h);
    WritePrinter(h, data, sizeof(data), &written);
    EndPagePrinter(h);
    EndDocPrinter(h);
    return job;
}

static JOB_INFO_2W *jobs(HANDLE h, DWORD *count)
{
    DWORD needed = 0;
    JOB_INFO_2W *list;

    *count = 0;
    EnumJobsW(h, 0, 100, 2, NULL, 0, &needed, count);
    if (!needed) return NULL;
    list = malloc(needed);
    if (!EnumJobsW(h, 0, 100, 2, (BYTE *)list, needed, &needed, count))
    {
        free(list);
        *count = 0;
        return NULL;
    }
    return list;
}

static JOB_INFO_2W *wait_jobs(HANDLE h, DWORD *count, DWORD want, int tenths)
{
    JOB_INFO_2W *list = NULL;

    while (tenths-- > 0)
    {
        free(list);
        list = jobs(h, count);
        if (*count == want) break;
        Sleep(100);
    }
    return list;
}

static DWORD printer_status(HANDLE h, DWORD *cjobs)
{
    PRINTER_INFO_2W *pi;
    DWORD needed = 0, status = 0;

    GetPrinterW(h, 2, NULL, 0, &needed);
    if (!needed || !(pi = malloc(needed))) return ~0u;
    if (GetPrinterW(h, 2, (BYTE *)pi, needed, &needed))
    {
        status = pi->Status;
        if (cjobs) *cjobs = pi->cJobs;
    }
    free(pi);
    return status;
}

static DWORD job_status(HANDLE h, DWORD id)
{
    JOB_INFO_2W *list;
    DWORD count, i, status = ~0u;

    list = jobs(h, &count);
    for (i = 0; i < count; i++) if (list[i].JobId == id) status = list[i].Status;
    free(list);
    return status;
}

static int jobs_test(const WCHAR *queue)
{
    HANDLE h;
    JOB_INFO_2W *list;
    JOB_INFO_1W *j1;
    DWORD count = 0, id, needed = 0, cjobs = ~0u, i;

    if (!OpenPrinterW((WCHAR *)queue, &h, NULL)) { CHECK(0, "OpenPrinter %ls: %lu", queue, GetLastError()); return 1; }
    list = jobs(h, &count);
    CHECK(count == 0, "the stopped queue is empty at first (%lu)", count);
    free(list);
    print_doc(h, title);
    list = wait_jobs(h, &count, 1, 50);
    CHECK(count == 1, "the spooled document is in the printer's queue, waiting in CUPS (%lu jobs)", count);
    if (!count) return 1;
    id = list[0].JobId;
    CHECK(list[0].pDocument && !wcscmp(list[0].pDocument, title),
          "its name is the document's whole name (%u of %u characters)",
          list[0].pDocument ? (unsigned)wcslen(list[0].pDocument) : 0, (unsigned)wcslen(title));
    CHECK(list[0].Size >= 3000, "its size is the document's (%lu)", list[0].Size);
    CHECK(list[0].pUserName && list[0].pUserName[0], "it has its owner (%ls)", list[0].pUserName ? list[0].pUserName : L"(null)");
    CHECK(list[0].Submitted.wYear >= 2024, "it has the time it was submitted (%u-%02u-%02u)",
          list[0].Submitted.wYear, list[0].Submitted.wMonth, list[0].Submitted.wDay);
    CHECK(list[0].pPrinterName && !wcscmp(list[0].pPrinterName, queue), "it is the printer's (%ls)",
          list[0].pPrinterName ? list[0].pPrinterName : L"(null)");
    free(list);

    GetJobW(h, id, 1, NULL, 0, &needed);
    j1 = needed ? malloc(needed) : NULL;
    CHECK(j1 && GetJobW(h, id, 1, (BYTE *)j1, needed, &needed) && j1->JobId == id && j1->pDocument &&
          !wcscmp(j1->pDocument, title), "GetJob finds it");
    free(j1);

    Sleep(2200);    /* winspool asks CUPS at most every two seconds */
    printer_status(h, &cjobs);
    CHECK(cjobs == 1, "the printer counts the document waiting (cJobs %lu)", cjobs);

    CHECK(SetJobW(h, id, 0, NULL, JOB_CONTROL_PAUSE), "Pause the document (%lu)", GetLastError());
    for (i = 0; i < 30 && !(job_status(h, id) & JOB_STATUS_PAUSED); i++) Sleep(100);
    CHECK(job_status(h, id) != ~0u && (job_status(h, id) & JOB_STATUS_PAUSED), "it is paused (status %#lx)", job_status(h, id));
    CHECK(SetJobW(h, id, 0, NULL, JOB_CONTROL_RESUME), "Resume it (%lu)", GetLastError());
    for (i = 0; i < 30 && (job_status(h, id) & JOB_STATUS_PAUSED); i++) Sleep(100);
    CHECK(job_status(h, id) != ~0u && !(job_status(h, id) & JOB_STATUS_PAUSED), "it waits again (status %#lx)", job_status(h, id));
    CHECK(SetJobW(h, id, 0, NULL, JOB_CONTROL_RESTART), "Restart it (%lu)", GetLastError());
    CHECK(SetJobW(h, id, 0, NULL, JOB_CONTROL_DELETE), "Cancel it (%lu)", GetLastError());
    list = wait_jobs(h, &count, 0, 50);
    CHECK(count == 0, "it has left the queue (%lu jobs)", count);
    free(list);
    SetLastError(0xdeadbeef);
    CHECK(!SetJobW(h, 999999, 0, NULL, JOB_CONTROL_DELETE) && GetLastError() == ERROR_INVALID_PARAMETER,
          "a job that is not in the queue: ERROR_INVALID_PARAMETER (%lu)", GetLastError());
    ClosePrinter(h);
    return 0;
}

static int printer_test(const WCHAR *queue)
{
    PRINTER_DEFAULTSW defaults = { NULL, NULL, PRINTER_ALL_ACCESS };
    HANDLE h;
    JOB_INFO_2W *list;
    DWORD count, status;

    if (!OpenPrinterW((WCHAR *)queue, &h, &defaults)) { CHECK(0, "OpenPrinter %ls: %lu", queue, GetLastError()); return 1; }
    Sleep(2200);
    status = printer_status(h, NULL);
    CHECK(status & PRINTER_STATUS_PAUSED, "the stopped queue's printer is paused (%#lx)", status);
    print_doc(h, L"second document");
    list = wait_jobs(h, &count, 1, 50);
    CHECK(count == 1, "a document waits (%lu)", count);
    free(list);
    CHECK(SetPrinterW(h, 0, NULL, PRINTER_CONTROL_RESUME), "Resume printing (%lu)", GetLastError());
    list = wait_jobs(h, &count, 0, 150);
    CHECK(count == 0, "the document printed once the printer was resumed (%lu left)", count);
    free(list);
    Sleep(2200);
    status = printer_status(h, NULL);
    CHECK(!(status & PRINTER_STATUS_PAUSED), "the printer is no longer paused (%#lx)", status);
    CHECK(SetPrinterW(h, 0, NULL, PRINTER_CONTROL_PAUSE), "Pause printing (%lu)", GetLastError());
    Sleep(2200);
    status = printer_status(h, NULL);
    CHECK(status & PRINTER_STATUS_PAUSED, "the printer is paused (%#lx)", status);
    print_doc(h, L"third document");
    print_doc(h, L"fourth document");
    list = wait_jobs(h, &count, 2, 50);
    CHECK(count == 2, "two documents wait at the paused printer (%lu)", count);
    free(list);
    CHECK(SetPrinterW(h, 0, NULL, PRINTER_CONTROL_PURGE), "Cancel all documents (%lu)", GetLastError());
    list = wait_jobs(h, &count, 0, 50);
    CHECK(count == 0, "the queue is empty (%lu)", count);
    free(list);
    ClosePrinter(h);
    return 0;
}

/* the queue window (wine-sg 9813), in this process so its list can be read */
#define IDM_DOC_PAUSE 106
#define IDM_DOC_CANCEL 109

static const WCHAR *g_queue;
static DWORD WINAPI window_thread(void *arg)
{
    void (WINAPI *entry)(HWND, HINSTANCE, const WCHAR *, DWORD);
    WCHAR line[300];
    HMODULE mod = LoadLibraryW(L"printui.dll");

    entry = mod ? (void *)GetProcAddress(mod, "PrintUIEntryW") : NULL;
    if (!entry) return 1;
    swprintf(line, 300, L"/o /n \"%ls\"", g_queue);
    entry(NULL, NULL, line, SW_SHOWNORMAL);
    return 0;
}

static void item_text(HWND list, int item, int sub, WCHAR *out, int count)
{
    LVITEMW lvi;

    memset(&lvi, 0, sizeof(lvi));
    lvi.iSubItem = sub;
    lvi.pszText = out;
    lvi.cchTextMax = count;
    out[0] = 0;
    SendMessageW(list, LVM_GETITEMTEXTW, item, (LPARAM)&lvi);
}

static int window_test(const WCHAR *queue)
{
    HANDLE h, thread;
    HWND wnd = NULL, list = NULL, status = NULL;
    WCHAR text[300], name[300];
    JOB_INFO_2W *jl;
    DWORD count, id;
    int i, n = 0;
    LVITEMW lvi;

    if (!OpenPrinterW((WCHAR *)queue, &h, NULL)) { CHECK(0, "OpenPrinter %ls: %lu", queue, GetLastError()); return 1; }
    print_doc(h, title);
    jl = wait_jobs(h, &count, 1, 50);
    id = count ? jl[0].JobId : 0;
    free(jl);
    g_queue = queue;
    thread = CreateThread(NULL, 0, window_thread, NULL, 0, NULL);
    /* the window, once it has made its list and status bar */
    for (i = 0; i < 100; i++)
    {
        if ((wnd = FindWindowW(L"PrintUI_PrinterQueue", NULL)) && IsWindowVisible(wnd) &&
            (list = FindWindowExW(wnd, NULL, WC_LISTVIEWW, NULL)) &&
            (status = FindWindowExW(wnd, NULL, STATUSCLASSNAMEW, NULL))) break;
        Sleep(100);
    }
    CHECK(wnd && list && status, "printui /o opens the queue window (%p %p %p)", wnd, list, status);
    if (!wnd || !list) return 1;
    GetWindowTextW(wnd, name, 300);
    CHECK(!wcsncmp(name, queue, wcslen(queue)), "it is titled with the printer's name (%ls)", name);
    for (i = 0; i < 50 && list && (n = SendMessageW(list, LVM_GETITEMCOUNT, 0, 0)) != 1; i++) Sleep(100);
    CHECK(n == 1, "it lists the document waiting (%d)", n);
    if (n >= 1)
    {
        item_text(list, 0, 0, text, 300);
        CHECK(!wcscmp(text, title), "by its name (%u characters)", (unsigned)wcslen(text));
        item_text(list, 0, 6, text, 300);
        CHECK(!wcsncmp(text, L"CUPS:", 5), "with its port (%ls)", text);
    }
    text[0] = 0;
    if (status) SendMessageW(status, SB_GETTEXTW, 0, (LPARAM)text);
    CHECK(wcsstr(text, L"1 document") != NULL, "the status bar counts it (%ls)", text);
    memset(&lvi, 0, sizeof(lvi));
    lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(list, LVM_SETITEMSTATE, 0, (LPARAM)&lvi);
    SendMessageW(wnd, WM_COMMAND, IDM_DOC_PAUSE, 0);
    for (i = 0; i < 30 && !(job_status(h, id) & JOB_STATUS_PAUSED); i++) Sleep(100);
    CHECK(job_status(h, id) != ~0u && (job_status(h, id) & JOB_STATUS_PAUSED),
          "Document > Pause pauses it (status %#lx)", job_status(h, id));
    for (i = 0; i < 30; i++)
    {
        item_text(list, 0, 1, text, 300);
        if (!wcscmp(text, L"Paused")) break;
        Sleep(100);
    }
    CHECK(!wcscmp(text, L"Paused"), "its status reads Paused (%ls)", text);
    SendMessageW(wnd, WM_CLOSE, 0, 0);
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0, "the window closes and PrintUIEntry returns");

    /* the shell's Open of a printer (the Printers folder, SHInvokePrinterCommand) is its queue window */
    CHECK(SHInvokePrinterCommandW(NULL, PRINTACTION_OPEN, queue, NULL, TRUE), "PRINTACTION_OPEN (%lu)", GetLastError());
    for (i = 0, wnd = NULL; i < 150 && !(wnd = FindWindowW(L"PrintUI_PrinterQueue", NULL)); i++) Sleep(100);
    name[0] = 0;
    for (i = 0; wnd && i < 50 && !name[0]; i++) { GetWindowTextW(wnd, name, 300); if (!name[0]) Sleep(100); }
    CHECK(wnd && !wcsncmp(name, queue, wcslen(queue)), "it opens the printer's queue window (%ls)", name);
    if (wnd) PostMessageW(wnd, WM_CLOSE, 0, 0);
    SetJobW(h, id, 0, NULL, JOB_CONTROL_DELETE);
    ClosePrinter(h);
    return 0;
}

/* 9212: what GetPrinter says follows this process's own changes at once (the
 * CUPS state is kept for 2 s; a pause, a new document, a purge showed late) */
static DWORD status_soon(HANDLE h, DWORD *cjobs, DWORD mask, DWORD want, DWORD jobs)
{
    DWORD status = 0, count = 0;
    int i;

    for (i = 0; i < 8; i++)
    {
        status = printer_status(h, &count);
        if ((status & mask) == want && (jobs == ~0u || count == jobs)) break;
        Sleep(100);
    }
    if (cjobs) *cjobs = count;
    return status;
}

static int status_test(const WCHAR *queue)
{
    PRINTER_DEFAULTSW defaults = { NULL, NULL, PRINTER_ALL_ACCESS };
    HANDLE h;
    DWORD count, status;

    if (!OpenPrinterW((WCHAR *)queue, &h, &defaults)) { CHECK(0, "OpenPrinter %ls: %lu", queue, GetLastError()); return 1; }
    status = printer_status(h, &count);     /* what CUPS says now is kept */
    CHECK(SetPrinterW(h, 0, NULL, PRINTER_CONTROL_RESUME), "Resume printing (%lu)", GetLastError());
    status = status_soon(h, NULL, PRINTER_STATUS_PAUSED, 0, ~0u);
    CHECK(!(status & PRINTER_STATUS_PAUSED), "resumed, the printer is not paused at once (%#lx)", status);
    CHECK(SetPrinterW(h, 0, NULL, PRINTER_CONTROL_PAUSE), "Pause printing (%lu)", GetLastError());
    status = status_soon(h, NULL, PRINTER_STATUS_PAUSED, PRINTER_STATUS_PAUSED, ~0u);
    CHECK(status & PRINTER_STATUS_PAUSED, "paused, the printer is paused at once (%#lx)", status);
    print_doc(h, L"status document");
    status_soon(h, &count, 0, 0, 1);
    CHECK(count == 1, "a document just printed is counted at once (cJobs %lu)", count);
    CHECK(SetPrinterW(h, 0, NULL, PRINTER_CONTROL_PURGE), "Cancel all documents (%lu)", GetLastError());
    status_soon(h, &count, 0, 0, 0);
    CHECK(count == 0, "purged, no document is counted at once (cJobs %lu)", count);
    ClosePrinter(h);
    return 0;
}

/* 9213: a user with printers but no default printer of their own is given one
 * -- the CUPS default -- as Windows gives every user a default printer; the
 * classic Print dialog then lists the printers (WordPad's listed none) */
static int default_test(const WCHAR *queue)
{
    WCHAR name[256];
    DWORD size = ARRAYSIZE(name);
    PRINTDLGW pd = { sizeof(pd) };
    HKEY key;

    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, KEY_SET_VALUE, &key))
    {
        RegDeleteValueW(key, L"Device");
        RegCloseKey(key);
    }
    CHECK(GetDefaultPrinterW(name, &size), "a default printer though the user had none (%lu)", GetLastError());
    CHECK(!wcscmp(name, queue), "it is the CUPS default (%ls)", name);
    pd.Flags = PD_RETURNDEFAULT;
    CHECK(PrintDlgW(&pd) && pd.hDevNames, "the Print dialog has a printer (%#lx)", CommDlgExtendedError());
    if (pd.hDevNames)
    {
        DEVNAMES *dn = GlobalLock(pd.hDevNames);
        CHECK(!wcscmp((WCHAR *)dn + dn->wDeviceOffset, queue), "the dialog's printer is it (%ls)", (WCHAR *)dn + dn->wDeviceOffset);
        GlobalUnlock(pd.hDevNames);
    }
    return 0;
}

/* 9214: a printer's location and description are what CUPS says now */
static int location_test(const WCHAR *queue, const WCHAR *want)
{
    PRINTER_INFO_2W *pi;
    DWORD needed = 0;
    HANDLE h;

    if (!OpenPrinterW((WCHAR *)queue, &h, NULL)) { CHECK(0, "OpenPrinter %ls: %lu", queue, GetLastError()); return 1; }
    GetPrinterW(h, 2, NULL, 0, &needed);
    if (needed && (pi = malloc(needed)) && GetPrinterW(h, 2, (BYTE *)pi, needed, &needed))
    {
        CHECK(pi->pLocation && !wcscmp(pi->pLocation, want), "the location is CUPS's new one (%ls)", pi->pLocation);
        free(pi);
    }
    else CHECK(0, "GetPrinter %lu", GetLastError());
    ClosePrinter(h);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    if (argc < 3) return 2;
    if (!wcscmp(argv[1], L"jobs")) jobs_test(argv[2]);
    else if (!wcscmp(argv[1], L"printer")) printer_test(argv[2]);
    else if (!wcscmp(argv[1], L"window")) window_test(argv[2]);
    else if (!wcscmp(argv[1], L"status")) status_test(argv[2]);
    else if (!wcscmp(argv[1], L"default")) default_test(argv[2]);
    else if (!wcscmp(argv[1], L"location") && argc > 3) location_test(argv[2], argv[3]);
    else return 2;
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
