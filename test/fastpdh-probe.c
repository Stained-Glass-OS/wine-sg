#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <pdh.h>
#include <pdhmsg.h>

static PDH_STATUS (WINAPI *pPdhGetLogFileTypeA)(const char *, DWORD *);
static PDH_STATUS (WINAPI *pPdhVbAddCounter)(PDH_HQUERY, const char *, PDH_HCOUNTER *);
static PDH_STATUS (WINAPI *pPdhVbOpenQuery)(PDH_HQUERY *);
static LONG (WINAPI *pPdhVbIsGoodStatus)(LONG);
#define PdhGetLogFileTypeA pPdhGetLogFileTypeA
#define PdhVbAddCounter pPdhVbAddCounter
#define PdhVbOpenQuery pPdhVbOpenQuery
#define PdhVbIsGoodStatus pPdhVbIsGoodStatus
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    PDH_STATUS r;
    DWORD size, n, idx, type;
    char buf[2048], b2[256], b3[256];
    WCHAR wbuf[2048];
    PDH_COUNTER_PATH_ELEMENTS_A *e = (void *)buf;
    HANDLE h, log;
    char file[MAX_PATH], dir[MAX_PATH];

    {
        HMODULE m = LoadLibraryA("pdh.dll");
        pPdhGetLogFileTypeA = (void *)GetProcAddress(m, "PdhGetLogFileTypeA");
        pPdhVbAddCounter = (void *)GetProcAddress(m, "PdhVbAddCounter");
        pPdhVbOpenQuery = (void *)GetProcAddress(m, "PdhVbOpenQuery");
        pPdhVbIsGoodStatus = (void *)GetProcAddress(m, "PdhVbIsGoodStatus");
    }
    /* ParseCounterPath */
    size = 0;
    r = PdhParseCounterPathA("\\\\mach\\Process(par/svc#2)\\ID Process", NULL, &size, 0);
    CHECK(r == PDH_MORE_DATA && size > sizeof(*e), "parse size r=%lx size=%lu", r, size);
    size = sizeof(buf);
    r = PdhParseCounterPathA("\\\\mach\\Process(par/svc#2)\\ID Process", e, &size, 0);
    CHECK(r == 0, "parse r=%lx", r);
    if (!r)
    {
        CHECK(!strcmp(e->szMachineName, "mach"), "machine %s", e->szMachineName);
        CHECK(!strcmp(e->szObjectName, "Process"), "object %s", e->szObjectName);
        CHECK(!strcmp(e->szParentInstance, "par"), "parent");
        CHECK(!strcmp(e->szInstanceName, "svc"), "instance %s", e->szInstanceName);
        CHECK(e->dwInstanceIndex == 2, "index %lu", e->dwInstanceIndex);
        CHECK(!strcmp(e->szCounterName, "ID Process"), "counter");
    }
    size = sizeof(buf);
    r = PdhParseCounterPathA("\\Processor(_Total)\\% Processor Time", e, &size, 0);
    CHECK(r == 0 && !e->szMachineName && !e->szParentInstance && !strcmp(e->szInstanceName, "_Total"), "parse2 r=%lx", r);
    size = sizeof(buf);
    CHECK(PdhParseCounterPathA("nonsense", e, &size, 0) == PDH_CSTATUS_BAD_COUNTERNAME, "bad path");
    CHECK(PdhParseCounterPathA(NULL, e, &size, 0) == PDH_INVALID_ARGUMENT, "null path");

    /* ParseInstanceName */
    n = sizeof(b2); size = sizeof(b3);
    r = PdhParseInstanceNameA("p/name#3", b2, &n, b3, &size, &idx);
    CHECK(r == 0 && !strcmp(b2, "name") && !strcmp(b3, "p") && idx == 3, "inst r=%lx %s %s %lu", r, b2, b3, idx);
    n = 2; size = sizeof(b3);
    r = PdhParseInstanceNameA("p/name#3", b2, &n, b3, &size, &idx);
    CHECK(r == PDH_MORE_DATA && n == 5, "inst more r=%lx n=%lu", r, n);

    /* MakeCounterPath round trip, index 0 omitted */
    {
        PDH_COUNTER_PATH_ELEMENTS_A m = { NULL, (char *)"Process", (char *)"svc", NULL, 0, (char *)"Cnt" };
        size = sizeof(b2);
        r = PdhMakeCounterPathA(&m, b2, &size, 0);
        CHECK(r == 0 && !strcmp(b2, "\\Process(svc)\\Cnt"), "make %s", b2);
    }

    /* wildcard expansion */
    size = 0;
    r = PdhExpandWildCardPathA(NULL, "\\Processor(*)\\*", NULL, &size, 0);
    CHECK(r == PDH_MORE_DATA && size > 2, "expand size r=%lx", r);
    size = sizeof(buf);
    r = PdhExpandWildCardPathA(NULL, "\\Processor(*)\\*", buf, &size, 0);
    CHECK(r == 0 && !strcmp(buf, "\\Processor(_Total)\\% Processor Time") && buf[strlen(buf) + 1] == 0, "expand r=%lx %s", r, buf);
    size = sizeof(buf);
    CHECK(PdhExpandWildCardPathA(NULL, "\\NoSuchObject(*)\\*", buf, &size, 0) == PDH_CSTATUS_NO_OBJECT, "no object");
    size = sizeof(buf);
    CHECK(PdhExpandWildCardPathA(NULL, "\\System\\Nothing*", buf, &size, 0) == PDH_CSTATUS_NO_COUNTER, "no counter");
    size = sizeof(buf);
    r = PdhExpandCounterPathA("\\System\\System Up*", buf, &size);
    CHECK(r == 0 && !strcmp(buf, "\\System\\System Up Time"), "expand counter r=%lx", r);
    size = sizeof(wbuf) / sizeof(WCHAR);
    r = PdhExpandWildCardPathW(NULL, L"\\\\nosuchhost\\System\\*", wbuf, &size, 0);
    CHECK(r == PDH_CANNOT_CONNECT_MACHINE, "remote expand r=%lx", r);

    /* object items */
    {
        DWORD cl = 0, il = 0;
        char c[512], i[512];
        r = PdhEnumObjectItemsA(NULL, NULL, "Processor", NULL, &cl, NULL, &il, PERF_DETAIL_WIZARD, 0);
        CHECK(r == PDH_MORE_DATA && cl > 2 && il > 2, "enum probe r=%lx cl=%lu il=%lu", r, cl, il);
        cl = sizeof(c); il = sizeof(i);
        r = PdhEnumObjectItemsA(NULL, NULL, "Processor", c, &cl, i, &il, PERF_DETAIL_WIZARD, 0);
        CHECK(r == 0 && !strcmp(c, "% Processor Time") && !strcmp(i, "_Total"), "enum r=%lx %s %s", r, c, i);
        cl = sizeof(c); il = sizeof(i);
        CHECK(PdhEnumObjectItemsA(NULL, NULL, "Bogus", c, &cl, i, &il, PERF_DETAIL_WIZARD, 0) == PDH_CSTATUS_NO_OBJECT, "enum bogus");
        CHECK(PdhEnumObjectItemsA(NULL, NULL, "System", c, &cl, i, &il, 7, 0) == PDH_INVALID_ARGUMENT, "enum detail");
        size = sizeof(c);
        r = PdhEnumObjectsA(NULL, NULL, c, &size, PERF_DETAIL_WIZARD, FALSE);
        CHECK(r == 0 && !strcmp(c, "Processor") && !strcmp(c + 10, "System"), "objects r=%lx", r);
        size = sizeof(c);
        r = PdhGetDefaultPerfObjectA(NULL, NULL, c, &size);
        CHECK(r == 0 && !strcmp(c, "Processor"), "defobj");
        size = sizeof(c);
        r = PdhGetDefaultPerfCounterA(NULL, NULL, "System", c, &size);
        CHECK(r == 0 && !strcmp(c, "System Up Time"), "defctr r=%lx", r);
    }

    /* vb helpers, connect, source */
    CHECK(PdhVbOpenQuery(&h) == 0 && PdhIsRealTimeQuery(h), "vbopen");
    {
        PDH_HCOUNTER c;
        CHECK(PdhVbAddCounter(h, "\\System\\System Up Time", &c) == 0, "vbadd");
        CHECK(PdhVbAddCounter(h, NULL, &c) == PDH_INVALID_ARGUMENT, "vbadd null");
    }
    PdhCloseQuery(h);
    CHECK(!PdhIsRealTimeQuery(NULL), "isrt null");
    CHECK(PdhVbIsGoodStatus(0) && !PdhVbIsGoodStatus(PDH_CSTATUS_NO_COUNTER), "goodstatus");
    CHECK(PdhConnectMachineA(NULL) == 0, "connect local");
    CHECK(PdhConnectMachineA("no-such-host-xyz") == PDH_CANNOT_CONNECT_MACHINE, "connect remote");
    CHECK(PdhSetDefaultRealTimeDataSource(DATA_SOURCE_WBEM) == 0 && PdhSetDefaultRealTimeDataSource(DATA_SOURCE_LOGFILE) == PDH_INVALID_ARGUMENT, "defsrc");
    PdhSetDefaultRealTimeDataSource(DATA_SOURCE_REGISTRY);

    GetTempPathA(sizeof(dir), dir);
    snprintf(file, sizeof(file), "%sfastpdh.csv", dir);
    {
        DWORD w;
        HANDLE f = CreateFileA(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
        const char *txt = "\"(PDH-CSV 4.0)\",\"\\\\M\\System\\System Up Time\"\r\n";
        WriteFile(f, txt, strlen(txt), &w, NULL);
        CloseHandle(f);
    }
    r = PdhGetLogFileTypeA(file, &type);
    CHECK(r == 0 && type == PDH_LOG_TYPE_CSV, "logtype r=%lx type=%lu", r, type);
    CHECK(PdhGetLogFileTypeA("C:\\no\\such\\file.csv", &type) == PDH_LOG_FILE_OPEN_ERROR, "logtype missing");
    r = PdhBindInputDataSourceA(&log, file);
    CHECK(r == 0 && log, "bind r=%lx", r);
    CHECK(PdhValidatePathExA(log, "\\System\\System Up Time") == 0, "validate ex log");
    CHECK(PdhCloseLog(log, 0) == 0, "closelog");
    CHECK(PdhCloseLog(NULL, 0) == PDH_INVALID_HANDLE, "closelog null");
    CHECK(PdhBindInputDataSourceA(&log, "C:\\no\\such\\file.csv") == PDH_LOG_FILE_OPEN_ERROR, "bind missing");
    DeleteFileA(file);

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
