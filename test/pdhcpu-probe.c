/* pdhcpu-probe: pdh counters with real values (Stained Glass OS wine-sg 8236; test/pdhcpu-gate.sh):
 * \Processor(*)\% Processor Time from the processors' idle times over two samples (it was a constant), per
 * processor and _Total; Memory and System counters; PdhGetFormattedCounterArray / PdhGetRawCounterArray of a
 * wildcard counter, PdhComputeCounterStatistics, PdhFormatFromRawValue and the H functions (spec stubs).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check( int ok, const char *what, double got )
{
    printf( "%s  %s (%g)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

static volatile LONG burn;
static DWORD WINAPI burner( void *arg )
{
    volatile unsigned int x = 0;
    while (burn) x++;
    return 0;
}

static double cpu_over( PDH_HQUERY q, PDH_HCOUNTER c, DWORD ms, BOOL busy, unsigned int cpus )
{
    PDH_FMT_COUNTERVALUE v;
    HANDLE threads[64];
    unsigned int i, n = min( cpus, 64 );

    if (busy)
    {
        burn = 1;
        for (i = 0; i < n; i++) threads[i] = CreateThread( NULL, 0, burner, NULL, 0, NULL );
        Sleep( 50 );
    }
    PdhCollectQueryData( q );
    Sleep( ms );
    PdhCollectQueryData( q );
    if (busy)
    {
        burn = 0;
        WaitForMultipleObjects( n, threads, TRUE, 5000 );
        for (i = 0; i < n; i++) CloseHandle( threads[i] );
    }
    v.doubleValue = -1;
    PdhGetFormattedCounterValue( c, PDH_FMT_DOUBLE, NULL, &v );
    return v.doubleValue;
}

int main( void )
{
    PDH_HQUERY q = NULL, qh = NULL;
    PDH_HCOUNTER total, all, mem, procs, commit, uptime;
    PDH_FMT_COUNTERVALUE v;
    PDH_RAW_COUNTER raw[3];
    PDH_STATISTICS stats;
    SYSTEM_INFO si;
    MEMORYSTATUSEX ms = { sizeof(ms) };
    DWORD size, count, type;
    char objs[2048];
    double busy, idle;
    PDH_STATUS r;
    BYTE buf[8192];
    int i;

    setvbuf( stdout, NULL, _IONBF, 0 );
    GetSystemInfo( &si );
    r = PdhOpenQueryW( NULL, 0, &q );
    check( r == 0, "PdhOpenQuery", r );
    check( PdhAddCounterW( q, L"\\Processor(_Total)\\% Processor Time", 0, &total ) == 0, "add _Total", 0 );
    r = PdhAddCounterW( q, L"\\Processor(*)\\% Processor Time", 0, &all );
    check( r == 0, "add a wildcard counter, Processor(*)", r );
    check( PdhAddCounterW( q, L"\\Memory\\Available MBytes", 0, &mem ) == 0, "add Memory\\Available MBytes", 0 );
    check( PdhAddCounterW( q, L"\\System\\Processes", 0, &procs ) == 0, "add System\\Processes", 0 );
    check( PdhAddCounterW( q, L"\\Memory\\% Committed Bytes In Use", 0, &commit ) == 0, "add Memory\\% Committed Bytes In Use", 0 );
    check( PdhAddCounterW( q, L"\\System\\System Up Time", 0, &uptime ) == 0, "add System Up Time", 0 );

    PdhCollectQueryData( q );
    r = PdhGetFormattedCounterValue( total, PDH_FMT_DOUBLE, &type, &v );
    check( r == PDH_INVALID_DATA, "one sample of a rate counter: PDH_INVALID_DATA", r );

    busy = cpu_over( q, total, 600, TRUE, si.dwNumberOfProcessors );
    check( busy > 50.0 && busy <= 100.0, "% Processor Time with every processor busy: high", busy );
    idle = cpu_over( q, total, 600, FALSE, si.dwNumberOfProcessors );
    check( idle >= 0.0 && idle < busy, "% Processor Time idle: lower than busy", idle );

    /* the wildcard counter as an array: _Total and one per processor */
    size = 0; count = 0;
    r = PdhGetFormattedCounterArrayW( all, PDH_FMT_DOUBLE, &size, &count, NULL );
    check( r == PDH_MORE_DATA && count == si.dwNumberOfProcessors + 1, "PdhGetFormattedCounterArray: size and count", count );
    if (size <= sizeof(buf))
    {
        PDH_FMT_COUNTERVALUE_ITEM_W *items = (void *)buf;
        BOOL ok = TRUE;
        r = PdhGetFormattedCounterArrayW( all, PDH_FMT_DOUBLE, &size, &count, items );
        for (i = 0; i < (int)count; i++)
            if (items[i].FmtValue.CStatus || items[i].FmtValue.doubleValue < 0 || items[i].FmtValue.doubleValue > 100) ok = FALSE;
        printf( "      %ls %g, %ls %g\n", items[0].szName, items[0].FmtValue.doubleValue,
                count > 1 ? items[1].szName : L"-", count > 1 ? items[1].FmtValue.doubleValue : 0 );
        check( r == 0 && !lstrcmpW( items[0].szName, L"_Total" ) && count > 1 && !lstrcmpW( items[1].szName, L"0" ) && ok,
               "the items: _Total, 0, ...; each 0-100", r );
    }
    size = sizeof(buf);
    r = PdhGetRawCounterArrayA( all, &size, &count, (void *)buf );
    check( r == 0 && !strcmp( ((PDH_RAW_COUNTER_ITEM_A *)buf)[0].szName, "_Total" ) &&
           ((PDH_RAW_COUNTER_ITEM_A *)buf)[0].RawValue.SecondValue > 0, "PdhGetRawCounterArrayA", r );

    GlobalMemoryStatusEx( &ms );
    PdhGetFormattedCounterValue( mem, PDH_FMT_LARGE, NULL, &v );
    check( v.largeValue > 0 && llabs( v.largeValue - (LONGLONG)(ms.ullAvailPhys >> 20) ) < 256, "Available MBytes", (double)v.largeValue );
    PdhGetFormattedCounterValue( procs, PDH_FMT_LONG, NULL, &v );
    check( v.longValue >= 1, "System\\Processes", v.longValue );
    PdhGetFormattedCounterValue( commit, PDH_FMT_DOUBLE, NULL, &v );
    check( v.doubleValue > 0 && v.doubleValue <= 100, "% Committed Bytes In Use", v.doubleValue );
    r = PdhGetFormattedCounterValue( uptime, PDH_FMT_LARGE, NULL, &v );
    check( r == 0 && v.largeValue > 0, "System Up Time in seconds", (double)v.largeValue );

    /* statistics over three raw samples: two values */
    for (i = 0; i < 3; i++)
    {
        PdhCollectQueryData( q );
        PdhGetRawCounterValue( total, &type, &raw[i] );
        Sleep( 200 );
    }
    r = PdhComputeCounterStatistics( total, PDH_FMT_DOUBLE, 0, 3, raw, &stats );
    check( r == 0 && stats.count == 2 && stats.min.doubleValue <= stats.mean.doubleValue &&
           stats.mean.doubleValue <= stats.max.doubleValue && stats.max.doubleValue <= 100, "PdhComputeCounterStatistics", stats.count );
    r = PdhFormatFromRawValue( type, PDH_FMT_DOUBLE, NULL, &raw[2], &raw[1], &v );
    {
        PDH_FMT_COUNTERVALUE v2;
        PdhCalculateCounterFromRawValue( total, PDH_FMT_DOUBLE, &raw[2], &raw[1], &v2 );
        check( r == 0 && v.doubleValue == v2.doubleValue, "PdhFormatFromRawValue: as PdhCalculateCounterFromRawValue", v.doubleValue );
    }

    /* the H functions, real-time */
    r = PdhOpenQueryH( NULL, 0, &qh );
    check( r == 0 && qh, "PdhOpenQueryH (real time)", r );
    if (qh) PdhCloseQuery( qh );
    size = sizeof(objs);
    r = PdhEnumObjectsHA( NULL, NULL, objs, &size, PERF_DETAIL_WIZARD, FALSE );
    {
        const char *p; int found = 0;
        for (p = objs; r == 0 && *p; p += strlen( p ) + 1) if (!strcmp( p, "Memory" )) found = 1;
        check( r == 0 && found, "PdhEnumObjectsHA lists Memory", r );
    }
    check( PdhOpenQueryH( (PDH_HLOG)&size, 0, &qh ) == PDH_INVALID_HANDLE, "PdhOpenQueryH: a bad handle", 0 );
    PdhCloseQuery( q );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
