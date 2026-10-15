/* SetJob in the local spooler (patch 3297), run by test/fastsetjob-gate.sh: job control commands (pause,
 * resume, cancel/delete, retain/release), job order (level 3) and argument errors. A fake driver and printer
 * are made through the spooler API; documents go to files, so no printer is involved. */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef JOB_CONTROL_RETAIN
#define JOB_CONTROL_RETAIN 8
#define JOB_CONTROL_RELEASE 9
#endif
#define STATUS_RETAINED 0x2000

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE hw, hc;           /* writer handle, control handle */

static int jobs(DWORD *ids, DWORD *status, WCHAR titles[][40])
{
    DWORD need = 0, n = 0, i;
    BYTE *buf;
    EnumJobsW(hc, 0, 100, 1, NULL, 0, &need, &n);
    if (!need || !(buf = malloc(need))) return 0;
    if (!EnumJobsW(hc, 0, 100, 1, buf, need, &need, &n)) n = 0;
    for (i = 0; i < n; i++)
    {
        JOB_INFO_1W *j = (JOB_INFO_1W *)buf + i;
        ids[i] = j->JobId;
        status[i] = j->Status;
        if (titles) { wcsncpy(titles[i], j->pDocument ? j->pDocument : L"", 39); titles[i][39] = 0; }
    }
    free(buf);
    return n;
}
static BOOL job_status(DWORD id, DWORD *st)
{
    DWORD ids[100], status[100];
    int n = jobs(ids, status, NULL), i;
    for (i = 0; i < n; i++) if (ids[i] == id) { *st = status[i]; return TRUE; }
    return FALSE;
}
static BOOL in_queue(DWORD id) { DWORD st; return job_status(id, &st); }

static DWORD start_doc(const WCHAR *title, const WCHAR *out, const char *text)
{
    DOC_INFO_1W di = { (WCHAR *)title, (WCHAR *)out, (WCHAR *)L"RAW" };
    DWORD id = StartDocPrinterW(hw, 1, (BYTE *)&di), w;
    if (id) { StartPagePrinter(hw); WritePrinter(hw, (void *)text, strlen(text), &w); EndPagePrinter(hw); }
    return id;
}
static BOOL file_has(const WCHAR *path, const char *text)
{
    char buf[100] = {0};
    DWORD got = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(f, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(f);
    return strstr(buf, text) != NULL;
}
static BOOL file_exists(const WCHAR *path) { return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES; }
static BOOL set_job(DWORD id, DWORD level, void *data, DWORD cmd) { return SetJobW(hc, id, level, data, cmd); }

int main(void)
{
    DRIVER_INFO_3W di = {0};
    PRINTER_INFO_2W pi = {0};
    PRINTER_DEFAULTSW def = { NULL, NULL, PRINTER_ALL_ACCESS };
    HANDLE h;
    DWORD a, b, c, d, e, f2, g, st, ids[100], status[100];
    JOB_INFO_3 i3;
    JOB_INFO_1W i1;
    WCHAR titles[100][40];
    int n;

    di.cVersion = 3; di.pName = (WCHAR *)L"SgFakeDriver"; di.pDriverPath = (WCHAR *)L"wineps.drv";
    di.pDataFile = (WCHAR *)L"generic.ppd"; di.pConfigFile = (WCHAR *)L"wineps.drv";
    AddPrinterDriverExW(NULL, 3, (BYTE *)&di, APD_COPY_NEW_FILES);
    pi.pPrinterName = (WCHAR *)L"SgJobPrinter"; pi.pPortName = (WCHAR *)L"FILE:"; pi.pDriverName = (WCHAR *)L"SgFakeDriver";
    pi.pPrintProcessor = (WCHAR *)L"winprint"; pi.pDatatype = (WCHAR *)L"RAW";
    if ((h = AddPrinterW(NULL, 2, (BYTE *)&pi))) ClosePrinter(h);
    check(OpenPrinterW((WCHAR *)L"SgJobPrinter", &hw, &def) && OpenPrinterW((WCHAR *)L"SgJobPrinter", &hc, &def), "printer opened twice");

    /* T1: a plain job prints at once */
    DeleteFileW(L"C:\\sgout-a.prn");
    a = start_doc(L"doc A", L"C:\\sgout-a.prn", "AAA-data");
    check(a && EndPagePrinter(hw) | 1, "document A started");
    check(EndDocPrinter(hw), "document A ended");
    check(file_has(L"C:\\sgout-a.prn", "AAA-data") && !in_queue(a), "A printed to its file and left the queue");

    /* T2: pause, end the document, resume */
    DeleteFileW(L"C:\\sgout-b.prn");
    b = start_doc(L"doc B", L"C:\\sgout-b.prn", "BBB-data");
    check(in_queue(b), "B is queued while being written");
    check(set_job(b, 0, NULL, JOB_CONTROL_PAUSE), "SetJob pause");
    check(job_status(b, &st) && (st & JOB_STATUS_PAUSED), "B shows JOB_STATUS_PAUSED");
    check(EndDocPrinter(hw), "B ended while paused");
    check(in_queue(b) && !file_exists(L"C:\\sgout-b.prn"), "a paused job waits in the queue and is not printed");
    check(set_job(b, 0, NULL, JOB_CONTROL_RESUME), "SetJob resume");
    check(file_has(L"C:\\sgout-b.prn", "BBB-data") && !in_queue(b), "resumed B printed and left the queue");

    /* T3: cancel a document that is being written */
    DeleteFileW(L"C:\\sgout-c.prn");
    c = start_doc(L"doc C", L"C:\\sgout-c.prn", "CCC-data");
    check(set_job(c, 0, NULL, JOB_CONTROL_CANCEL), "SetJob cancel of an open document");
    check(job_status(c, &st) && (st & JOB_STATUS_DELETING), "C shows JOB_STATUS_DELETING");
    EndDocPrinter(hw);
    check(!in_queue(c) && !file_exists(L"C:\\sgout-c.prn"), "the cancelled job is gone and never printed");

    /* T4: delete a spooled, paused job */
    DeleteFileW(L"C:\\sgout-d.prn");
    d = start_doc(L"doc D", L"C:\\sgout-d.prn", "DDD-data");
    set_job(d, 0, NULL, JOB_CONTROL_PAUSE);
    EndDocPrinter(hw);
    check(in_queue(d), "D waits paused");
    check(set_job(d, 0, NULL, JOB_CONTROL_DELETE), "SetJob delete");
    check(!in_queue(d) && !file_exists(L"C:\\sgout-d.prn"), "deleted D left the queue unprinted");

    /* T5: order */
    e = start_doc(L"doc E", L"C:\\sgout-e.prn", "E"); set_job(e, 0, NULL, JOB_CONTROL_PAUSE); EndDocPrinter(hw);
    f2 = start_doc(L"doc F", L"C:\\sgout-f.prn", "F"); set_job(f2, 0, NULL, JOB_CONTROL_PAUSE); EndDocPrinter(hw);
    g = start_doc(L"doc G", L"C:\\sgout-g.prn", "G"); set_job(g, 0, NULL, JOB_CONTROL_PAUSE); EndDocPrinter(hw);
    n = jobs(ids, status, NULL);
    check(n == 3 && ids[0] == e && ids[1] == f2 && ids[2] == g, "queue order E F G");
    i3.JobId = e; i3.NextJobId = g;
    check(set_job(e, 3, &i3, 0), "SetJob level 3: E before G");
    n = jobs(ids, status, NULL);
    check(n == 3 && ids[0] == f2 && ids[1] == e && ids[2] == g, "queue order is F E G");
    i3.NextJobId = 0;
    check(set_job(e, 3, &i3, 0), "SetJob level 3 with NextJobId 0");
    n = jobs(ids, status, NULL);
    check(n == 3 && ids[0] == f2 && ids[1] == g && ids[2] == e, "queue order is F G E");
    i3.NextJobId = 99999;
    SetLastError(0);
    check(!set_job(e, 3, &i3, 0) && GetLastError() == ERROR_INVALID_PARAMETER, "NextJobId of no job is refused");
    i3.NextJobId = e;
    check(!set_job(e, 3, &i3, 0), "a job cannot be its own next job");
    n = jobs(ids, status, NULL);
    check(n == 3 && ids[2] == e, "refused calls leave the order");

    /* level 1 still renames */
    memset(&i1, 0, sizeof(i1));
    i1.pDocument = (WCHAR *)L"renamed F";
    check(set_job(f2, 1, &i1, 0), "SetJob level 1");
    n = jobs(ids, status, titles);
    check(n == 3 && !wcscmp(titles[0], L"renamed F"), "the new document title shows");
    set_job(e, 0, NULL, JOB_CONTROL_DELETE); set_job(f2, 0, NULL, JOB_CONTROL_DELETE); set_job(g, 0, NULL, JOB_CONTROL_DELETE);
    check(jobs(ids, status, NULL) == 0, "all deleted");

    /* T6: errors */
    SetLastError(0);
    check(!set_job(424242, 0, NULL, JOB_CONTROL_PAUSE) && GetLastError() == ERROR_INVALID_PARAMETER, "SetJob of an unknown job");
    a = start_doc(L"doc X", L"C:\\sgout-x.prn", "X");
    SetLastError(0);
    check(!set_job(a, 0, NULL, 99) && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown command is refused");
    check(in_queue(a) && (job_status(a, &st), !(st & JOB_STATUS_PAUSED)), "and changes nothing");
    SetLastError(0);
    check(!set_job(a, 9, NULL, 0) && GetLastError() == ERROR_INVALID_LEVEL, "an unknown level is refused");
    EndDocPrinter(hw);

    /* T7: retain / release */
    DeleteFileW(L"C:\\sgout-h.prn");
    a = start_doc(L"doc H", L"C:\\sgout-h.prn", "HHH-data");
    check(set_job(a, 0, NULL, JOB_CONTROL_RETAIN), "SetJob retain");
    EndDocPrinter(hw);
    check(file_has(L"C:\\sgout-h.prn", "HHH-data"), "retained H was printed");
    check(job_status(a, &st) && (st & STATUS_RETAINED) && (st & JOB_STATUS_PRINTED), "and stays queued as retained + printed");
    check(set_job(a, 0, NULL, JOB_CONTROL_RELEASE) && !in_queue(a), "release removes the printed job");

    ClosePrinter(hw); ClosePrinter(hc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
