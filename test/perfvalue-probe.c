/* PerfSet/Increment/Decrement{ULong,ULongLong}CounterValue (patches/sg/7858): advapi32 exports
 * them (Visual Studio's C++ project system called PerfSetULongCounterValue and Wine aborted the
 * process: unimplemented function). Values land in the instance's counter data; a
 * PERF_ATTRIB_BY_REFERENCE counter's go to the variable PerfSetCounterRefValue named. */
#include <windows.h>
#include <perflib.h>
#include <stdio.h>
#ifndef PERF_COUNTERSET_SINGLE_INSTANCE
#define PERF_COUNTERSET_SINGLE_INSTANCE 0
#endif
#ifndef PERF_ATTRIB_BY_REFERENCE
#define PERF_ATTRIB_BY_REFERENCE 1
#endif

typedef ULONG (WINAPI *set32_fn)(HANDLE, PERF_COUNTERSET_INSTANCE *, ULONG, ULONG);
typedef ULONG (WINAPI *set64_fn)(HANDLE, PERF_COUNTERSET_INSTANCE *, ULONG, ULONGLONG);
typedef ULONG (WINAPI *start_fn)(GUID *, PERFLIBREQUEST, HANDLE *);
typedef ULONG (WINAPI *info_fn)(HANDLE, PERF_COUNTERSET_INFO *, ULONG);
typedef PERF_COUNTERSET_INSTANCE *(WINAPI *inst_fn)(HANDLE, const GUID *, const WCHAR *, ULONG);
typedef ULONG (WINAPI *ref_fn)(HANDLE, PERF_COUNTERSET_INSTANCE *, ULONG, void *);
typedef ULONG (WINAPI *stop_fn)(HANDLE);

static ULONG WINAPI cb(ULONG code, void *buf, ULONG size) { return 0; }

int main(void)
{
    static GUID prov_guid = {0x7f8a1c2d,0x1111,0x4a4a,{1,2,3,4,5,6,7,8}};
    static GUID set_guid = {0x7f8a1c2d,0x2222,0x4a4a,{1,2,3,4,5,6,7,8}};
    HMODULE adv = GetModuleHandleA("advapi32.dll") ? GetModuleHandleA("advapi32.dll") : LoadLibraryA("advapi32.dll");
    set32_fn set32 = (set32_fn)GetProcAddress(adv, "PerfSetULongCounterValue");
    set64_fn set64 = (set64_fn)GetProcAddress(adv, "PerfSetULongLongCounterValue");
    set32_fn inc32 = (set32_fn)GetProcAddress(adv, "PerfIncrementULongCounterValue");
    set64_fn inc64 = (set64_fn)GetProcAddress(adv, "PerfIncrementULongLongCounterValue");
    set32_fn dec32 = (set32_fn)GetProcAddress(adv, "PerfDecrementULongCounterValue");
    set64_fn dec64 = (set64_fn)GetProcAddress(adv, "PerfDecrementULongLongCounterValue");
    start_fn start = (start_fn)GetProcAddress(adv, "PerfStartProvider");
    info_fn info = (info_fn)GetProcAddress(adv, "PerfSetCounterSetInfo");
    inst_fn create = (inst_fn)GetProcAddress(adv, "PerfCreateInstance");
    ref_fn setref = (ref_fn)GetProcAddress(adv, "PerfSetCounterRefValue");
    stop_fn stop = (stop_fn)GetProcAddress(adv, "PerfStopProvider");
    struct { PERF_COUNTERSET_INFO set; PERF_COUNTER_INFO c[3]; } t;
    PERF_COUNTERSET_INSTANCE *inst;
    HANDLE prov = NULL;
    ULONG r1, r2, r3;
    ULONGLONG *data;
    LONGLONG refvar = 5;

    printf("exports=%d\n", set32 && set64 && inc32 && inc64 && dec32 && dec64);
    if (!(set32 && set64 && inc32 && inc64 && dec32 && dec64)) { printf("done=1\n"); return 0; }

    start(&prov_guid, cb, &prov);
    memset(&t, 0, sizeof(t));
    t.set.CounterSetGuid = set_guid; t.set.ProviderGuid = prov_guid;
    t.set.NumCounters = 3; t.set.InstanceType = PERF_COUNTERSET_SINGLE_INSTANCE;
    t.c[0].CounterId = 1; t.c[0].Type = PERF_COUNTER_RAWCOUNT; t.c[0].Size = 4;
    t.c[1].CounterId = 2; t.c[1].Type = PERF_COUNTER_LARGE_RAWCOUNT; t.c[1].Size = 8;
    t.c[2].CounterId = 3; t.c[2].Type = PERF_COUNTER_LARGE_RAWCOUNT; t.c[2].Size = 8;
    t.c[2].Attrib = PERF_ATTRIB_BY_REFERENCE;
    printf("info=%lu\n", info(prov, &t.set, sizeof(t)));
    inst = create(prov, &set_guid, L"probe", 0);
    if (!inst) { printf("noinstance done=1\n"); return 0; }
    data = (ULONGLONG *)(inst + 1);
    setref(prov, inst, 3, &refvar);

    r1 = set32(prov, inst, 1, 40); r2 = inc32(prov, inst, 1, 5); r3 = dec32(prov, inst, 1, 3);
    printf("ulong=%d\n", !r1 && !r2 && !r3 && *(ULONG *)&data[0] == 42);
    r1 = set64(prov, inst, 2, 0x100000000ull); r2 = inc64(prov, inst, 2, 10); r3 = dec64(prov, inst, 2, 1);
    printf("ulonglong=%d\n", !r1 && !r2 && !r3 && data[1] == 0x100000009ull);
    r1 = inc64(prov, inst, 3, 37);
    printf("byref=%d\n", !r1 && refvar == 42);
    printf("notfound=%d\n", set32(prov, inst, 99, 1) == ERROR_NOT_FOUND);
    printf("nullinst=%d\n", set32(prov, NULL, 1, 1) == ERROR_INVALID_PARAMETER);
    stop(prov);
    printf("done=1\n");
    return 0;
}
