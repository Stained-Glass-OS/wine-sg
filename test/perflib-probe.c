/* The Perflib provider API (patches/sg/2201): PerfStartProviderEx,
 * PerfSetCounterSetInfo, PerfCreateInstance and PerfSetCounterRefValue were
 * all "semi-stub" FIXMEs even though they already worked (noise on every
 * call, score 120 each: observed from real programs). Checks the basic
 * provider/counterset/instance/counter-value lifecycle, that a single-
 * instance counter set refuses a second instance, and that a provider's own
 * MemAllocRoutine/MemFreeRoutine (PERF_PROVIDER_CONTEXT) is used for the
 * instance blocks it hands PerfCreateInstance/PerfDeleteInstance. */
#include <windows.h>
#include <perflib.h>
#include <stdio.h>

#ifndef PERF_COUNTERSET_SINGLE_INSTANCE
#define PERF_COUNTERSET_SINGLE_INSTANCE 0
#endif
#ifndef PERF_COUNTERSET_MULTI_INSTANCES
#define PERF_COUNTERSET_MULTI_INSTANCES 2
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int custom_allocs, custom_frees;
static void * WINAPI custom_alloc(SIZE_T size, void *context)
{
    custom_allocs++;
    return HeapAlloc(GetProcessHeap(), 0, size);
}
static void WINAPI custom_free(void *ptr, void *context)
{
    custom_frees++;
    HeapFree(GetProcessHeap(), 0, ptr);
}

int main(void)
{
    GUID provider_guid = { 0x11111111, 0x1111, 0x1111, { 1,2,3,4,5,6,7,8 } };
    GUID counterset_guid = { 0x22222222, 0x2222, 0x2222, { 1,2,3,4,5,6,7,8 } };
    GUID counterset2_guid = { 0x33333333, 0x3333, 0x3333, { 1,2,3,4,5,6,7,8 } };
    PERF_PROVIDER_CONTEXT ctx;
    PERF_COUNTERSET_INFO *info;
    PERF_COUNTER_INFO *counter;
    PERF_COUNTERSET_INSTANCE *inst1, *inst2;
    HANDLE provider;
    UINT64 value = 0;
    ULONG err;
    char buf[sizeof(PERF_COUNTERSET_INFO) + sizeof(PERF_COUNTER_INFO)];

    memset( &ctx, 0, sizeof(ctx) );
    ctx.ContextSize = sizeof(ctx);
    ctx.MemAllocRoutine = custom_alloc;
    ctx.MemFreeRoutine = custom_free;
    err = PerfStartProviderEx( &provider_guid, &ctx, &provider );
    check( err == ERROR_SUCCESS && provider != NULL, "PerfStartProviderEx with a custom allocator succeeds" );

    info = (PERF_COUNTERSET_INFO *)buf;
    counter = (PERF_COUNTER_INFO *)(info + 1);
    info->CounterSetGuid = counterset_guid;
    info->ProviderGuid = provider_guid;
    info->NumCounters = 1;
    info->InstanceType = PERF_COUNTERSET_SINGLE_INSTANCE;
    memset( counter, 0, sizeof(*counter) );
    counter->CounterId = 1;
    counter->Type = 0x40020000; /* PERF_COUNTER_RAWCOUNT-ish, value is opaque to us */
    counter->Size = sizeof(UINT64);
    err = PerfSetCounterSetInfo( provider, info, sizeof(buf) );
    check( err == ERROR_SUCCESS, "PerfSetCounterSetInfo registers a single-instance counter set" );
    err = PerfSetCounterSetInfo( provider, info, sizeof(buf) );
    check( err == ERROR_ALREADY_EXISTS, "registering the same counter set guid twice fails" );

    inst1 = PerfCreateInstance( provider, &counterset_guid, L"instance-one", 0 );
    check( inst1 != NULL, "PerfCreateInstance makes the first instance" );
    check( custom_allocs >= 1, "the provider's own MemAllocRoutine was used" );

    inst2 = PerfCreateInstance( provider, &counterset_guid, L"instance-two", 1 );
    check( inst2 == NULL && GetLastError() == ERROR_ALREADY_EXISTS,
           "a single-instance counter set refuses a second instance" );

    err = PerfSetCounterRefValue( provider, inst1, 1, &value );
    check( err == ERROR_SUCCESS, "PerfSetCounterRefValue accepts a known counter id" );
    value = 42;
    check( *(UINT64 **)((BYTE *)inst1 + sizeof(PERF_COUNTERSET_INSTANCE)) == &value &&
           **(UINT64 **)((BYTE *)inst1 + sizeof(PERF_COUNTERSET_INSTANCE)) == 42,
           "the counter slot holds the referenced address, which reads back the live value" );

    err = PerfSetCounterRefValue( provider, inst1, 99, &value );
    check( err == ERROR_NOT_FOUND, "PerfSetCounterRefValue rejects an unknown counter id" );

    err = PerfDeleteInstance( provider, inst1 );
    check( err == ERROR_SUCCESS, "PerfDeleteInstance succeeds" );
    check( custom_frees >= 1, "the provider's own MemFreeRoutine was used" );

    /* a multi-instance counter set may have more than one instance */
    info->CounterSetGuid = counterset2_guid;
    info->InstanceType = PERF_COUNTERSET_MULTI_INSTANCES;
    err = PerfSetCounterSetInfo( provider, info, sizeof(buf) );
    check( err == ERROR_SUCCESS, "PerfSetCounterSetInfo registers a multi-instance counter set" );
    inst1 = PerfCreateInstance( provider, &counterset2_guid, L"a", 0 );
    inst2 = PerfCreateInstance( provider, &counterset2_guid, L"b", 1 );
    check( inst1 != NULL && inst2 != NULL, "a multi-instance counter set accepts two instances" );
    PerfDeleteInstance( provider, inst1 );
    PerfDeleteInstance( provider, inst2 );

    err = PerfStopProvider( provider );
    check( err == ERROR_SUCCESS, "PerfStopProvider succeeds" );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures ? 1 : 0;
}
