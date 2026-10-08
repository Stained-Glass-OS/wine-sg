/* EnumJobs and the page calls of a raw print job (patches/sg/1615).
 * EnumJobsW failed with no error set, StartPagePrinter/EndPagePrinter did
 * nothing. A printer is made on Wine's PostScript driver with a FILE: port
 * printing to a file, then a raw job is started and looked at. */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <wchar.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static const char ppd[] =
    "*PPD-Adobe: \"4.3\"\n*FormatVersion: \"4.3\"\n*FileVersion: \"1.0\"\n*LanguageVersion: English\n"
    "*LanguageEncoding: ISOLatin1\n*PCFileName: \"SGJOBS.PPD\"\n*Manufacturer: \"Stained Glass OS\"\n"
    "*ModelName: \"SG Jobs PS\"\n*NickName: \"SG Jobs PS\"\n*ShortNickName: \"SG Jobs PS\"\n"
    "*PSVersion: \"(3010.000) 0\"\n*LanguageLevel: \"3\"\n*ColorDevice: True\n*DefaultColorSpace: RGB\n"
    "*OpenUI *PageSize/Media Size: PickOne\n*DefaultPageSize: Letter\n"
    "*PageSize Letter/Letter: \"<</PageSize[612 792]>>setpagedevice\"\n*CloseUI: *PageSize\n"
    "*DefaultImageableArea: Letter\n*ImageableArea Letter/Letter: \"18 18 594 774\"\n"
    "*DefaultPaperDimension: Letter\n*PaperDimension Letter/Letter: \"612 792\"\n*DefaultResolution: 300dpi\n";

int wmain(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], sys[MAX_PATH], out[MAX_PATH];
    DRIVER_INFO_3W di = { 3 };
    PRINTER_INFO_2W pi = { 0 };
    DOC_INFO_1W doc = { L"sg enumjobs", NULL, L"RAW" };
    BYTE buf[8192];
    JOB_INFO_1W *j1 = (JOB_INFO_1W *)buf;
    JOB_INFO_1A *ja = (JOB_INFO_1A *)buf;
    JOB_INFO_3 *j3 = (JOB_INFO_3 *)buf;
    DWORD size, needed, count, written, job;
    HANDLE printer, f;
    BOOL ok;

    GetPrinterDriverDirectoryW(NULL, NULL, 1, (BYTE *)dir, sizeof(dir), &size);
    CreateDirectoryW(dir, NULL);
    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(path, MAX_PATH, L"%ls\\wineps.drv", sys);
    swprintf(out, MAX_PATH, L"%ls\\wineps.drv", dir);
    CopyFileW(path, out, FALSE);
    swprintf(out, MAX_PATH, L"%ls\\sgjobs.ppd", dir);
    f = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, ppd, sizeof(ppd) - 1, &written, NULL);
    CloseHandle(f);
    di.pName = L"SG Jobs PS";
    di.pDriverPath = L"wineps.drv";
    di.pDataFile = L"sgjobs.ppd";
    di.pConfigFile = L"wineps.drv";
    ok = AddPrinterDriverW(NULL, 3, (BYTE *)&di);
    printf("AddPrinterDriver: %d (%lu)\n", ok, ok ? 0 : GetLastError());
    pi.pPrinterName = L"SG Jobs";
    pi.pDriverName = L"SG Jobs PS";
    pi.pPortName = L"FILE:";
    pi.pPrintProcessor = L"WinPrint";
    pi.pDatatype = L"RAW";
    printer = AddPrinterW(NULL, 2, (BYTE *)&pi);
    printf("AddPrinter: %p (%lu)\n", printer, printer ? 0 : GetLastError());
    if (!printer) { printf("FAIL  no printer to test with\nRESULT: FAIL\n"); return 1; }

    SetLastError(0xdeadbeef);
    ok = StartPagePrinter(printer);
    check(!ok && GetLastError() == ERROR_SPL_NO_STARTDOC, "StartPagePrinter without a document: ERROR_SPL_NO_STARTDOC");

    count = 99;
    ok = EnumJobsW(printer, 0, 10, 1, buf, sizeof(buf), &needed, &count);
    printf("empty queue: ok %d count %lu needed %lu err %lu\n", ok, count, needed, ok ? 0 : GetLastError());
    check(ok && count == 0, "EnumJobsW of an empty queue: none");

    GetTempPathW(MAX_PATH, out);
    wcscat(out, L"sg-enumjobs.prn");
    doc.pOutputFile = out;
    job = StartDocPrinterW(printer, 1, (BYTE *)&doc);
    printf("job %lu\n", job);
    ok = StartPagePrinter(printer);
    WritePrinter(printer, "%!PS\n", 5, &written);
    ok = ok && EndPagePrinter(printer);
    ok = ok && StartPagePrinter(printer) && EndPagePrinter(printer);
    check(job && ok, "StartPagePrinter/EndPagePrinter in a document");

    needed = 0;
    ok = EnumJobsW(printer, 0, 10, 1, NULL, 0, &needed, &count);
    check(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed > sizeof(JOB_INFO_1W), "EnumJobsW gives the size needed");
    ok = EnumJobsW(printer, 0, 10, 1, buf, sizeof(buf), &needed, &count);
    printf("queue: ok %d count %lu id %lu doc %ls pages %lu pos %lu\n", ok, count, ok && count ? j1->JobId : 0,
           ok && count && j1->pDocument ? j1->pDocument : L"", ok && count ? j1->TotalPages : 0, ok && count ? j1->Position : 0);
    check(ok && count == 1 && j1->JobId == job && j1->pDocument && !wcscmp(j1->pDocument, L"sg enumjobs"),
          "EnumJobsW lists the job, with its document name");
    check(ok && count == 1 && j1->TotalPages == 2 && j1->Position == 1, "... its two pages and its place in the queue");
    ok = EnumJobsW(printer, 1, 10, 1, buf, sizeof(buf), &needed, &count);
    check(ok && count == 0, "from the second job on: none");
    ok = EnumJobsW(printer, 0, 10, 3, buf, sizeof(buf), &needed, &count);
    check(ok && count == 1 && j3->JobId == job && j3->NextJobId == 0, "level 3: the job and no next one");
    ok = EnumJobsA(printer, 0, 10, 1, buf, sizeof(buf), &needed, &count);
    check(ok && count == 1 && ja->pDocument && !strcmp(ja->pDocument, "sg enumjobs"), "EnumJobsA gives narrow strings");

    EndDocPrinter(printer);
    DeletePrinter(printer);
    ClosePrinter(printer);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
