/* Probe for patches/sg/2446: DnsQueryEx (synchronous, with a callback, cancelled) and DnsCancelQuery. */
#include <winsock2.h>
#include <windows.h>
#include <windns.h>
#include <stdio.h>
#include <string.h>

#ifndef DNS_REQUEST_PENDING
#define DNS_REQUEST_PENDING 9506
#endif

/* the structures of the documented API, under our own names (older SDK headers lack some of them) */
typedef struct { ULONG Version; DNS_STATUS QueryStatus; ULONG64 QueryOptions; DNS_RECORD *pQueryRecords; void *Reserved; } QRESULT;
typedef void (WINAPI *QCALLBACK)(void *, QRESULT *);
typedef struct { char MaxSa[32]; DWORD Data[8]; } QADDR;
#pragma pack(push, 1)
typedef struct { DWORD MaxCount, AddrCount, Tag; WORD Family, WordReserved; DWORD Flags, MatchFlag, Reserved1, Reserved2; QADDR AddrArray[1]; } QADDRARRAY;
#pragma pack(pop)
typedef struct { ULONG Version; const WCHAR *QueryName; WORD QueryType; ULONG64 QueryOptions; QADDRARRAY *pDnsServerList; ULONG InterfaceIndex; QCALLBACK pQueryCompletionCallback; void *pQueryContext; } QREQUEST;
typedef struct { char Reserved[32]; } QCANCEL;
#ifndef DNS_QUERY_REQUEST_VERSION1
#define DNS_QUERY_REQUEST_VERSION1 1
#endif
#define QRESULTS_VERSION1 1

static DNS_STATUS (WINAPI *pDnsQueryEx)(QREQUEST *, QRESULT *, QCANCEL *);
static DNS_STATUS (WINAPI *pDnsCancelQuery)(QCANCEL *);
#define DnsQueryEx pDnsQueryEx
#define DnsCancelQuery pDnsCancelQuery

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static HANDLE done_event;
static volatile LONG callbacks;
static void *seen_context;
static QRESULT *seen_result;
static DNS_STATUS seen_status;
static DNS_RECORD *seen_records;
static DWORD seen_thread;

static void WINAPI complete(void *ctx, QRESULT *res)
{
    seen_context = ctx;
    seen_result = res;
    seen_status = res->QueryStatus;
    seen_records = res->pQueryRecords;
    seen_thread = GetCurrentThreadId();
    InterlockedIncrement(&callbacks);
    SetEvent(done_event);
}

int main(void)
{
    QREQUEST req;
    QRESULT res;
    QCANCEL cancel;
    DNS_STATUS st;
    QADDRARRAY *servers;
    SOCKADDR_IN *sin;
    int marker = 7;

    HMODULE dns = LoadLibraryA("dnsapi.dll");
    pDnsQueryEx = (void *)GetProcAddress(dns, "DnsQueryEx");
    pDnsCancelQuery = (void *)GetProcAddress(dns, "DnsCancelQuery");
    if (!pDnsQueryEx || !pDnsCancelQuery) { puts("      FAIL  DnsQueryEx or DnsCancelQuery not exported"); puts("RESULT: FAIL"); return 1; }
    done_event = CreateEventW(NULL, FALSE, FALSE, NULL);

    /* synchronous */
    memset(&req, 0, sizeof(req));
    memset(&res, 0, sizeof(res));
    req.Version = DNS_QUERY_REQUEST_VERSION1;
    req.QueryName = L"10.1.2.3";
    req.QueryType = DNS_TYPE_A;
    req.QueryOptions = DNS_QUERY_BYPASS_CACHE;
    res.Version = QRESULTS_VERSION1;
    st = DnsQueryEx(&req, &res, NULL);
    check("synchronous query succeeds", st == ERROR_SUCCESS);
    check("QueryStatus is the status", res.QueryStatus == ERROR_SUCCESS);
    check("an A record for the address", res.pQueryRecords && res.pQueryRecords->wType == DNS_TYPE_A &&
          res.pQueryRecords->Data.A.IpAddress == htonl(0x0a010203));
    if (res.pQueryRecords) DnsRecordListFree(res.pQueryRecords, DnsFreeRecordList);

    /* bad requests */
    res.Version = QRESULTS_VERSION1;
    check("no request", DnsQueryEx(NULL, &res, NULL) == ERROR_INVALID_PARAMETER);
    check("no result", DnsQueryEx(&req, NULL, NULL) == ERROR_INVALID_PARAMETER);
    req.Version = 99;
    check("bad request version", DnsQueryEx(&req, &res, NULL) == ERROR_INVALID_PARAMETER);
    req.Version = DNS_QUERY_REQUEST_VERSION1;
    res.Version = 99;
    check("bad result version", DnsQueryEx(&req, &res, NULL) == ERROR_INVALID_PARAMETER);
    res.Version = QRESULTS_VERSION1;
    req.QueryName = NULL;
    check("no name", DnsQueryEx(&req, &res, NULL) == ERROR_INVALID_PARAMETER);
    req.QueryName = L"10.1.2.3";

    /* with a callback: pending, then the callback gets the result */
    req.pQueryCompletionCallback = complete;
    req.pQueryContext = &marker;
    memset(&res, 0, sizeof(res));
    res.Version = QRESULTS_VERSION1;
    memset(&cancel, 0xcc, sizeof(cancel));
    st = DnsQueryEx(&req, &res, &cancel);
    check("with a callback: pending", st == DNS_REQUEST_PENDING);
    check("the callback runs", WaitForSingleObject(done_event, 10000) == WAIT_OBJECT_0);
    check("it gets the context and the very result structure", seen_context == &marker && seen_result == &res);
    check("with the answer", seen_status == ERROR_SUCCESS && seen_records && seen_records->wType == DNS_TYPE_A);
    check("called once", callbacks == 1);
    if (seen_records) DnsRecordListFree(seen_records, DnsFreeRecordList);
    check("cancelling a finished query is harmless", DnsCancelQuery(&cancel) == ERROR_SUCCESS && callbacks == 1);
    check("DnsCancelQuery(NULL)", DnsCancelQuery(NULL) == ERROR_INVALID_PARAMETER);

    /* a query that never gets an answer, cancelled */
    servers = calloc(1, sizeof(*servers));
    servers->MaxCount = servers->AddrCount = 1;
    sin = (SOCKADDR_IN *)servers->AddrArray[0].MaxSa;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(53);
    sin->sin_addr.s_addr = inet_addr("192.0.2.1");     /* TEST-NET-1: nothing answers */
    req.QueryName = L"nowhere.example.invalid";
    req.QueryOptions = DNS_QUERY_BYPASS_CACHE | DNS_QUERY_NO_NETBT;
    req.pDnsServerList = servers;
    memset(&res, 0, sizeof(res));
    res.Version = QRESULTS_VERSION1;
    callbacks = 0;
    st = DnsQueryEx(&req, &res, &cancel);
    check("a slow query is pending", st == DNS_REQUEST_PENDING);
    Sleep(200);
    check("DnsCancelQuery succeeds", DnsCancelQuery(&cancel) == ERROR_SUCCESS);
    check("the callback ran at once", WaitForSingleObject(done_event, 3000) == WAIT_OBJECT_0 && callbacks == 1);
    check("with ERROR_CANCELLED and no records", seen_status == ERROR_CANCELLED && seen_records == NULL);
    check("the result says so too", res.QueryStatus == ERROR_CANCELLED);
    Sleep(300);
    check("and not again", callbacks == 1);
    check("a second cancel does nothing", DnsCancelQuery(&cancel) == ERROR_SUCCESS && callbacks == 1);

    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    fflush(stdout);
    ExitProcess(fails != 0);   /* the resolver thread may still be waiting for the dead server */
}
