/* printprops-probe: a printer's properties open on the General page (wine-sg 9816).
 *
 *   printprops-probe.exe PRINTER
 * PRINTER is a stopped CUPS queue (its test page waits there). Prints "PASS ..." / "FAIL ..."
 * lines and "RESULT: PASS" when all held. */
#include <windows.h>
#include <winspool.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static int failures;
#define CHECK(c, ...) do { int ok_ = !!(c); printf(ok_ ? "PASS  " : "FAIL  "); printf(__VA_ARGS__); printf("\n"); \
                           failures += !ok_; fflush(stdout); } while (0)

#define IDC_PG_NAME   301
#define IDC_PG_MODEL  304
#define IDC_PG_COLOR  305
#define IDC_PG_DUPLEX 306
#define IDC_PG_PAPERS 310
#define IDC_PG_PREFS  311
#define IDC_PG_TEST   312

static const WCHAR *g_printer;

static DWORD WINAPI props_thread(void *arg)
{
    HANDLE h;

    if (!OpenPrinterW((WCHAR *)g_printer, &h, NULL)) return 1;
    PrinterProperties(NULL, h);
    ClosePrinter(h);
    return 0;
}

struct find { const WCHAR *title; HWND found; };
static BOOL CALLBACK find_top(HWND hwnd, LPARAM lparam)
{
    struct find *f = (struct find *)lparam;
    WCHAR text[300];
    DWORD pid;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
    GetWindowTextW(hwnd, text, 300);
    if (wcsstr(text, f->title)) { f->found = hwnd; return FALSE; }
    return TRUE;
}

static HWND wait_window(const WCHAR *title, int tenths)
{
    struct find f = { title, NULL };

    while (tenths-- > 0)
    {
        f.found = NULL;
        EnumWindows(find_top, (LPARAM)&f);
        if (f.found) return f.found;
        Sleep(100);
    }
    return NULL;
}

/* the page that has control id inside the sheet */
static HWND find_control(HWND sheet, int id)
{
    HWND page = NULL, ctl;

    while ((page = FindWindowExW(sheet, page, NULL, NULL)))
        if ((ctl = GetDlgItem(page, id))) return ctl;
    return NULL;
}

static DWORD queue_jobs(const WCHAR *printer)
{
    DWORD needed = 0, count = 0;
    HANDLE h;

    BYTE *buf;

    if (!OpenPrinterW((WCHAR *)printer, &h, NULL)) return ~0u;
    /* sized first: without room EnumJobs returns no jobs, only the size */
    EnumJobsW(h, 0, 100, 1, NULL, 0, &needed, &count);
    count = 0;
    if (needed && (buf = malloc(needed)))
    {
        if (!EnumJobsW(h, 0, 100, 1, buf, needed, &needed, &count)) count = 0;
        free(buf);
    }
    ClosePrinter(h);
    return count;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR title[300], text[300], tab[64];
    HWND sheet, ctl, box, tabs;
    HANDLE thread;
    int i, n;
    TCITEMW item;

    if (argc < 2) return 2;
    g_printer = argv[1];
    swprintf(title, 300, L"%ls Properties", g_printer);
    thread = CreateThread(NULL, 0, props_thread, NULL, 0, NULL);
    sheet = wait_window(title, 150);
    CHECK(sheet != NULL, "PrinterProperties opens \"%ls\"", title);
    if (!sheet) goto done;
    Sleep(500);
    tabs = FindWindowExW(sheet, NULL, L"SysTabControl32", NULL);
    tab[0] = 0;
    memset(&item, 0, sizeof(item));
    item.mask = TCIF_TEXT;
    item.pszText = tab;
    item.cchTextMax = 64;
    if (tabs) SendMessageW(tabs, TCM_GETITEMW, 0, (LPARAM)&item);
    CHECK(!wcscmp(tab, L"General"), "its first page is General (%ls)", tab);
    ctl = find_control(sheet, IDC_PG_NAME);
    text[0] = 0;
    if (ctl) GetWindowTextW(ctl, text, 300);
    CHECK(!wcscmp(text, g_printer), "with the printer's name (%ls)", text);
    ctl = find_control(sheet, IDC_PG_MODEL);
    text[0] = 0;
    if (ctl) GetWindowTextW(ctl, text, 300);
    CHECK(!wcscmp(text, L"SG Office Laser"), "its model (%ls)", text);
    ctl = find_control(sheet, IDC_PG_COLOR);
    text[0] = 0;
    if (ctl) GetWindowTextW(ctl, text, 300);
    CHECK(!wcscmp(text, L"Yes"), "colour: Yes (%ls)", text);
    ctl = find_control(sheet, IDC_PG_DUPLEX);
    text[0] = 0;
    if (ctl) GetWindowTextW(ctl, text, 300);
    CHECK(!wcscmp(text, L"Yes"), "double-sided: Yes (%ls)", text);
    ctl = find_control(sheet, IDC_PG_PAPERS);
    n = ctl ? SendMessageW(ctl, LB_GETCOUNT, 0, 0) : 0;
    text[0] = 0;
    if (n > 0) SendMessageW(ctl, LB_GETTEXT, 0, (LPARAM)text);
    CHECK(n == 2 && !wcscmp(text, L"Letter"), "the paper it takes (%d: %ls...)", n, text);
    CHECK(find_control(sheet, IDC_PG_PREFS) != NULL, "a Preferences... button");

    /* Print Test Page: a document waits in the printer's queue, and the person is told */
    ctl = find_control(sheet, IDC_PG_TEST);
    CHECK(ctl != NULL, "a Print Test Page button");
    if (ctl)
    {
        PostMessageW(GetParent(ctl), WM_COMMAND, MAKEWPARAM(IDC_PG_TEST, BN_CLICKED), (LPARAM)ctl);
        for (i = 0; i < 100; i++)
        {
            struct find f = { g_printer, NULL };
            HWND w = NULL;
            /* the message box is titled with the printer's name, not "... Properties" */
            while ((w = FindWindowExW(NULL, w, L"#32770", NULL)))
            {
                GetWindowTextW(w, text, 300);
                if (!wcscmp(text, g_printer) && IsWindowVisible(w)) { f.found = w; break; }
            }
            if ((box = f.found)) break;
            Sleep(100);
        }
        CHECK(box != NULL, "the person is told the test page was sent");
        for (i = 0; i < 50 && queue_jobs(g_printer) != 1; i++) Sleep(100);
        CHECK(queue_jobs(g_printer) == 1, "the test page waits in the printer's queue (%lu)", queue_jobs(g_printer));
        if (box) PostMessageW(box, WM_COMMAND, IDOK, 0);
    }
    Sleep(500);
    PostMessageW(sheet, WM_COMMAND, IDCANCEL, 0);
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0, "the sheet closes");
done:
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
