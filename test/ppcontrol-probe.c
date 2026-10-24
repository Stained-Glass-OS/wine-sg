/* ppcontrol-probe: SetJob pauses, resumes and cancels a job while its print processor prints it
 * (ControlPrintProcessor, Stained Glass OS wine-sg 6079), run by test/ppcontrol-gate.sh. The printer is a
 * raw TCP printer on 127.0.0.1 that the probe itself is, reading slowly, so the job is still printing while the
 * probe looks at it. */
#include <winsock2.h>
#include <windows.h>
#include <winspool.h>
#include <tcpxcv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JOB_SIZE (8 * 1024 * 1024)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE hw, hc;
static DWORD job_id;
static BOOL end_ret;
static DWORD end_err;

static BOOL job_status(DWORD id, DWORD *st)
{
    DWORD need = 0, n = 0, i;
    BOOL found = FALSE;
    BYTE *buf;
    EnumJobsW(hc, 0, 100, 1, NULL, 0, &need, &n);
    if (!need || !(buf = malloc(need))) return FALSE;
    if (EnumJobsW(hc, 0, 100, 1, buf, need, &need, &n))
        for (i = 0; i < n; i++)
            if (((JOB_INFO_1W *)buf)[i].JobId == id) { *st = ((JOB_INFO_1W *)buf)[i].Status; found = TRUE; }
    free(buf);
    return found;
}

/* writes the document and ends it: printing happens inside EndDocPrinter, on this thread */
static DWORD WINAPI writer(void *arg)
{
    DOC_INFO_1W di = { (WCHAR *)L"control job", NULL, (WCHAR *)L"RAW" };
    char *data = malloc(JOB_SIZE);
    DWORD w;

    memset(data, 'J', JOB_SIZE);
    job_id = StartDocPrinterW(hw, 1, (BYTE *)&di);
    StartPagePrinter(hw);
    WritePrinter(hw, data, JOB_SIZE, &w);
    EndPagePrinter(hw);
    end_ret = EndDocPrinter(hw);
    end_err = GetLastError();
    free(data);
    return 0;
}

/* the printer: accepts one connection per job and counts what arrives, a little at a time */
#define PRINTER_PORT 39123
static SOCKET listener;
static volatile LONG received, reader_done;
static DWORD WINAPI reader(void *arg)
{
    SOCKET c = accept(listener, NULL, NULL);
    char buf[4096];
    int got;

    if (c == INVALID_SOCKET) { reader_done = 1; return 1; }
    while ((got = recv(c, buf, sizeof(buf), 0)) > 0)
    {
        InterlockedExchangeAdd(&received, got);
        Sleep(2);
    }
    closesocket(c);
    reader_done = 1;
    return 0;
}

static void make_port(void)
{
    PRINTER_DEFAULTSW defaults = { NULL, NULL, SERVER_ACCESS_ADMINISTER };
    DWORD needed = 0, status = 0;
    PORT_DATA_1 data;
    HANDLE h;

    memset(&data, 0, sizeof(data));
    lstrcpynW(data.sztPortName, L"SG_CONTROL", MAX_PORTNAME_LEN);
    data.dwVersion = 1;
    data.dwProtocol = PROTOCOL_RAWTCP_TYPE;
    data.cbSize = sizeof(data);
    lstrcpynW(data.sztHostAddress, L"127.0.0.1", MAX_NETWORKNAME_LEN);
    data.dwPortNumber = PRINTER_PORT;
    if (OpenPrinterW((WCHAR *)L",XcvMonitor Standard TCP/IP Port", &h, &defaults))
    {
        check(XcvDataW(h, L"AddPort", (BYTE *)&data, sizeof(data), NULL, 0, &needed, &status) && !status,
              "a raw TCP port to the probe's printer");
        ClosePrinter(h);
    }
    else check(0, "the TCP/IP port monitor opens");
}

static LONG settle(DWORD ms)
{
    LONG before = received;
    Sleep(ms);
    return received - before;
}

int main(void)
{
    DRIVER_INFO_3W di = {0};
    PRINTER_INFO_2W pi = {0};
    PRINTER_DEFAULTSW def = { NULL, NULL, PRINTER_ALL_ACCESS };
    HANDLE h, thread, rthread;
    DWORD st, i;
    LONG during;
    BOOL printing = FALSE;

    {
        WSADATA wsa;
        struct sockaddr_in addr = { AF_INET };
        int small = 8192;

        WSAStartup(MAKEWORD(2, 2), &wsa);
        listener = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(listener, SOL_SOCKET, SO_RCVBUF, (char *)&small, sizeof(small));
        addr.sin_port = htons(PRINTER_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        check(!bind(listener, (struct sockaddr *)&addr, sizeof(addr)) && !listen(listener, 2), "the printer listens");
    }
    make_port();
    di.cVersion = 3; di.pName = (WCHAR *)L"SgFakeDriver"; di.pDriverPath = (WCHAR *)L"wineps.drv";
    di.pDataFile = (WCHAR *)L"generic.ppd"; di.pConfigFile = (WCHAR *)L"wineps.drv";
    AddPrinterDriverExW(NULL, 3, (BYTE *)&di, APD_COPY_NEW_FILES);
    pi.pPrinterName = (WCHAR *)L"SgControlPrinter"; pi.pPortName = (WCHAR *)L"SG_CONTROL"; pi.pDriverName = (WCHAR *)L"SgFakeDriver";
    pi.pPrintProcessor = (WCHAR *)L"winprint"; pi.pDatatype = (WCHAR *)L"RAW";
    if ((h = AddPrinterW(NULL, 2, (BYTE *)&pi))) ClosePrinter(h);
    check(OpenPrinterW((WCHAR *)L"SgControlPrinter", &hw, &def) && OpenPrinterW((WCHAR *)L"SgControlPrinter", &hc, &def),
          "printer opened twice");

    /* pause and resume while printing */
    received = reader_done = 0;
    rthread = CreateThread(NULL, 0, reader, NULL, 0, NULL);
    thread = CreateThread(NULL, 0, writer, NULL, 0, NULL);
    for (i = 0; i < 500 && !printing; i++)
    {
        if (received && job_status(job_id, &st) && (st & JOB_STATUS_PRINTING)) printing = TRUE;
        else Sleep(10);
    }
    check(printing, "the job shows JOB_STATUS_PRINTING while it prints (the queue is readable meanwhile)");
    check(SetJobW(hc, job_id, 0, NULL, JOB_CONTROL_PAUSE), "SetJob pause while printing");
    check(job_status(job_id, &st) && (st & JOB_STATUS_PAUSED), "the job shows JOB_STATUS_PAUSED");
    settle(600);                      /* what was on its way */
    during = settle(800);
    printf("      paused: %ld bytes in 0.8 s, %ld so far\n", during, received);
    check(during == 0 && !reader_done, "nothing more is printed while paused");
    check(WaitForSingleObject(thread, 0) == WAIT_TIMEOUT, "the print is still going on");
    check(SetJobW(hc, job_id, 0, NULL, JOB_CONTROL_RESUME), "SetJob resume");
    WaitForSingleObject(thread, 60000);
    WaitForSingleObject(rthread, 60000);
    CloseHandle(thread);
    CloseHandle(rthread);
    printf("      %ld bytes of %u\n", received, JOB_SIZE);
    check(received == JOB_SIZE, "after resuming the whole job is printed");
    check(end_ret, "EndDocPrinter succeeded");
    check(!job_status(job_id, &st), "the printed job left the queue");

    /* cancel while printing */
    received = reader_done = 0;
    printing = FALSE;
    rthread = CreateThread(NULL, 0, reader, NULL, 0, NULL);
    thread = CreateThread(NULL, 0, writer, NULL, 0, NULL);
    for (i = 0; i < 500 && !printing; i++)
    {
        if (received && job_status(job_id, &st) && (st & JOB_STATUS_PRINTING)) printing = TRUE;
        else Sleep(10);
    }
    check(SetJobW(hc, job_id, 0, NULL, JOB_CONTROL_CANCEL), "SetJob cancel while printing");
    WaitForSingleObject(thread, 60000);
    WaitForSingleObject(rthread, 60000);
    CloseHandle(thread);
    CloseHandle(rthread);
    printf("      %ld bytes of %u\n", received, JOB_SIZE);
    check(received < JOB_SIZE, "a cancelled job stops printing");
    check(!job_status(job_id, &st), "and leaves the queue");

    ClosePrinter(hw);
    ClosePrinter(hc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
