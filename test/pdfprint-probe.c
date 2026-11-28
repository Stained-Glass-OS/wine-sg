/* pdfprint-probe: the Print to PDF printer and Print to file (wine-sg 9814).
 *
 *   pdfprint-probe.exe PDFQUEUE PSQUEUE DIR
 * PDFQUEUE is a CUPS queue on cups-pdf, PSQUEUE a PostScript printer; files go to DIR.
 * Prints "PASS ..." / "FAIL ..." lines and "RESULT: PASS" when all held. */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <wchar.h>

static int failures;
#define CHECK(c, ...) do { int ok_ = !!(c); printf(ok_ ? "PASS  " : "FAIL  "); printf(__VA_ARGS__); printf("\n"); \
                           failures += !ok_; fflush(stdout); } while (0)

/* what the dialog thread does when "Save Print Output As" appears */
static struct
{
    const WCHAR *answer;    /* the file to save as, NULL: Cancel */
    WCHAR seen_name[MAX_PATH], seen_filter[128];
    BOOL seen;
} g_dlg;

static BOOL CALLBACK find_child(HWND child, LPARAM lparam)
{
    HWND *found = (HWND *)lparam;
    WCHAR cls[32], text[128];

    GetClassNameW(child, cls, 32);
    if (!IsWindowVisible(child)) return TRUE;
    if (!wcsicmp(cls, L"Edit") && !found[0]) found[0] = child;
    else if (!wcsicmp(cls, L"Button"))
    {
        GetWindowTextW(child, text, 128);
        if (!wcscmp(text, L"&Save") || !wcscmp(text, L"Save")) found[1] = child;
        if (!wcscmp(text, L"Cancel")) found[2] = child;
    }
    else if (!wcsicmp(cls, L"ComboBox") && !found[3])
    {
        /* the file type list: the one whose text names a file type */
        GetWindowTextW(child, text, 128);
        if (wcschr(text, '(')) found[3] = child;
    }
    return TRUE;
}

static DWORD WINAPI dialog_thread(void *arg)
{
    HWND dlg = NULL, found[4];
    int i;

    for (i = 0; i < 200 && !(dlg = FindWindowW(NULL, L"Save Print Output As")); i++) Sleep(50);
    if (!dlg) return 1;
    Sleep(500);
    memset(found, 0, sizeof(found));
    EnumChildWindows(dlg, find_child, (LPARAM)found);
    g_dlg.seen = TRUE;
    if (found[0]) GetWindowTextW(found[0], g_dlg.seen_name, MAX_PATH);
    if (found[3]) GetWindowTextW(found[3], g_dlg.seen_filter, 128);
    if (g_dlg.answer && found[0] && found[1])
    {
        SendMessageW(found[0], WM_SETTEXT, 0, (LPARAM)g_dlg.answer);
        SendMessageW(found[1], BM_CLICK, 0, 0);
    }
    else if (found[2]) SendMessageW(found[2], BM_CLICK, 0, 0);
    else PostMessageW(dlg, WM_CLOSE, 0, 0);
    return 0;
}

/* prints a page through the printer; output: the DOCINFO's lpszOutput */
static int print_page(const WCHAR *printer, const WCHAR *output, const WCHAR *answer, DWORD *error)
{
    DOCINFOW di = { sizeof(di), L"Quarterly report.txt", output };
    HANDLE thread;
    HDC dc;
    int ret;

    memset(&g_dlg, 0, sizeof(g_dlg));
    g_dlg.answer = answer;
    if (!(dc = CreateDCW(L"WINSPOOL", printer, NULL, NULL))) { *error = GetLastError(); return -2; }
    thread = CreateThread(NULL, 0, dialog_thread, NULL, 0, NULL);
    ret = StartDocW(dc, &di);
    *error = GetLastError();
    if (ret > 0)
    {
        StartPage(dc);
        TextOutW(dc, 300, 300, L"Quarterly report", 16);
        EndPage(dc);
        EndDoc(dc);
    }
    DeleteDC(dc);
    WaitForSingleObject(thread, 3000);
    CloseHandle(thread);
    return ret;
}

static BOOL file_starts(const WCHAR *path, const char *magic)
{
    char head[8] = { 0 };
    DWORD got = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(f, head, strlen(magic), &got, NULL);
    CloseHandle(f);
    return got == strlen(magic) && !memcmp(head, magic, got);
}

static DWORD queue_jobs(const WCHAR *printer)
{
    DWORD needed = 0, count = 0;
    HANDLE h;

    if (!OpenPrinterW((WCHAR *)printer, &h, NULL)) return ~0u;
    EnumJobsW(h, 0, 100, 1, NULL, 0, &needed, &count);
    ClosePrinter(h);
    return count;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR port[300], file[MAX_PATH];
    DWORD error;
    int ret;

    if (argc < 4) return 2;
    swprintf(port, 300, L"CUPS:%ls", argv[1]);

    /* the PDF printer: asked where, named after the document; a PDF there */
    swprintf(file, MAX_PATH, L"%ls\\asked.pdf", argv[3]);
    ret = print_page(argv[1], NULL, file, &error);
    CHECK(g_dlg.seen, "the PDF printer asks: Save Print Output As");
    CHECK(!wcscmp(g_dlg.seen_name, L"Quarterly report"), "the file is named after the document (%ls)", g_dlg.seen_name);
    CHECK(wcsstr(g_dlg.seen_filter, L"PDF") != NULL, "as a PDF document (%ls)", g_dlg.seen_filter);
    CHECK(ret > 0, "StartDoc (%d, %lu)", ret, error);
    CHECK(file_starts(file, "%PDF"), "the file saved is a PDF");
    CHECK(queue_jobs(argv[1]) == 0, "nothing went to the CUPS queue (%lu)", queue_jobs(argv[1]));

    /* the same with the printer's port as the output (a program's DEVNAMES) */
    swprintf(file, MAX_PATH, L"%ls\\port.pdf", argv[3]);
    ret = print_page(argv[1], port, file, &error);
    CHECK(g_dlg.seen && ret > 0 && file_starts(file, "%PDF"), "the printer's port as output: asked, a PDF (%d)", ret);

    /* Cancel: nothing printed, StartDoc fails with ERROR_CANCELLED */
    ret = print_page(argv[1], NULL, NULL, &error);
    CHECK(g_dlg.seen, "asked again");
    CHECK(ret <= 0 && error == ERROR_CANCELLED, "Cancel: StartDoc fails with ERROR_CANCELLED (%d, %lu)", ret, error);
    Sleep(500);
    CHECK(queue_jobs(argv[1]) == 0, "and nothing was printed (%lu)", queue_jobs(argv[1]));

    /* a file the program names: not asked, a PDF */
    swprintf(file, MAX_PATH, L"%ls\\given.pdf", argv[3]);
    ret = print_page(argv[1], file, NULL, &error);
    CHECK(!g_dlg.seen && ret > 0, "an output file the program gives is not asked for (%d)", ret);
    CHECK(file_starts(file, "%PDF"), "and is a PDF");

    /* Print to file on a PostScript printer: Save Print Output As, Printer Files; the printer's data */
    swprintf(file, MAX_PATH, L"%ls\\out.prn", argv[3]);
    ret = print_page(argv[2], L"FILE:", file, &error);
    CHECK(g_dlg.seen && wcsstr(g_dlg.seen_filter, L"Printer Files") != NULL,
          "Print to file asks: Save Print Output As, Printer Files (%ls)", g_dlg.seen_filter);
    CHECK(ret > 0 && file_starts(file, "%!"), "the file has the printer's PostScript (%d)", ret);

    /* the PostScript printer without a file prints as before: no question */
    ret = print_page(argv[2], NULL, NULL, &error);
    CHECK(!g_dlg.seen && ret > 0, "a printer prints without asking (%d)", ret);

    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
