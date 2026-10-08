/* CPU sets, NUMA nodes, ideal processors and priority boost
 * (patches/sg/1606).
 *
 *  - SetThreadSelectedCpuSets applies the selection (the thread's affinity)
 *    and GetThreadSelectedCpuSets reads it back; an unknown id is refused;
 *    SetProcessDefaultCpuSets / GetProcessDefaultCpuSets likewise;
 *  - GetNumaHighestNodeNumber, GetNumaNodeProcessorMask(Ex),
 *    GetNumaProcessorNode(Ex), GetNumaAvailableMemoryNode(Ex) and
 *    GetNumaProximityNode(Ex) answer from the processor topology;
 *  - SetThreadIdealProcessor(Ex) keeps the ideal processor and gives the
 *    previous one; GetThreadIdealProcessorEx reads it;
 *  - Set/GetProcessPriorityBoost and Set/GetThreadPriorityBoost keep the
 *    setting; a thread follows the process's until it sets its own.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef BOOL (WINAPI *get_sets_t)(HANDLE, ULONG *, ULONG, ULONG *);
typedef BOOL (WINAPI *set_sets_t)(HANDLE, const ULONG *, ULONG);
typedef BOOL (WINAPI *get_cpu_info_t)(void *, ULONG, ULONG *, HANDLE, ULONG);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

/* SYSTEM_CPU_SET_INFORMATION, as far as needed */
typedef struct { DWORD Size; DWORD Type; DWORD Id; WORD Group; BYTE LogicalProcessorIndex; } cpu_set_head;

int main(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    get_sets_t pGetThreadSelectedCpuSets = (void *)GetProcAddress(k32, "GetThreadSelectedCpuSets");
    set_sets_t pSetThreadSelectedCpuSets = (void *)GetProcAddress(k32, "SetThreadSelectedCpuSets");
    get_sets_t pGetProcessDefaultCpuSets = (void *)GetProcAddress(k32, "GetProcessDefaultCpuSets");
    set_sets_t pSetProcessDefaultCpuSets = (void *)GetProcAddress(k32, "SetProcessDefaultCpuSets");
    get_cpu_info_t pGetSystemCpuSetInformation = (void *)GetProcAddress(k32, "GetSystemCpuSetInformation");
    BOOL (WINAPI *pGetNumaHighestNodeNumber)(ULONG *) = (void *)GetProcAddress(k32, "GetNumaHighestNodeNumber");
    BYTE info[8192];
    ULONG len = 0, ids[64], n_ids = 0, required, got[64], highest = 99, offset;
    DWORD_PTR proc_mask, sys_mask, prev_mask;
    ULONGLONG mask, avail;
    GROUP_AFFINITY ga;
    PROCESSOR_NUMBER pn, prev;
    USHORT node;
    UCHAR cnode;
    BOOL ok, disable;
    DWORD r;

    if (!pGetThreadSelectedCpuSets || !pGetProcessDefaultCpuSets)
    {
        printf("FAIL  GetThreadSelectedCpuSets/GetProcessDefaultCpuSets are not exported\n");
        failures++;
    }

    /* CPU sets */
    ok = pGetSystemCpuSetInformation(info, sizeof(info), &len, GetCurrentProcess(), 0);
    for (offset = 0; ok && offset < len && n_ids < 64; offset += ((cpu_set_head *)(info + offset))->Size)
    {
        cpu_set_head *h = (cpu_set_head *)(info + offset);
        if (!h->Size) break;
        ids[n_ids++] = h->Id;
    }
    printf("%lu CPU sets, first id %#lx\n", n_ids, n_ids ? ids[0] : 0);
    check(n_ids >= 1, "the system has CPU sets");

    ok = pSetThreadSelectedCpuSets(GetCurrentThread(), ids, 1);
    printf("SetThreadSelectedCpuSets: ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    prev_mask = SetThreadAffinityMask(GetCurrentThread(), ~(DWORD_PTR)0);
    check(ok && prev_mask == 1, "selecting the first CPU set puts the thread on the first processor");
    pSetThreadSelectedCpuSets(GetCurrentThread(), ids, 1);
    required = 0;
    ok = pGetThreadSelectedCpuSets && pGetThreadSelectedCpuSets(GetCurrentThread(), got, 64, &required);
    check(ok && required == 1 && got[0] == ids[0], "GetThreadSelectedCpuSets reads it back");
    SetLastError(0xdeadbeef);
    ok = pGetThreadSelectedCpuSets && pGetThreadSelectedCpuSets(GetCurrentThread(), got, 0, &required);
    check(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER && required == 1, "... with the count needed when the buffer is small");
    ids[63] = 0xbad;
    SetLastError(0xdeadbeef);
    ok = pSetThreadSelectedCpuSets(GetCurrentThread(), &ids[63], 1);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown CPU set id is refused");
    ok = pSetThreadSelectedCpuSets(GetCurrentThread(), NULL, 0);
    required = 5;
    if (pGetThreadSelectedCpuSets) pGetThreadSelectedCpuSets(GetCurrentThread(), got, 64, &required);
    GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask);
    prev_mask = SetThreadAffinityMask(GetCurrentThread(), proc_mask);
    check(ok && required == 0 && prev_mask == proc_mask, "an empty selection goes back to the process's processors");

    ok = pSetProcessDefaultCpuSets(GetCurrentProcess(), ids, 1);
    required = 0;
    ok = ok && pGetProcessDefaultCpuSets && pGetProcessDefaultCpuSets(GetCurrentProcess(), got, 64, &required);
    GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask);
    check(ok && required == 1 && got[0] == ids[0] && proc_mask == 1, "the process's default CPU sets are kept and applied");
    ok = pSetProcessDefaultCpuSets(GetCurrentProcess(), NULL, 0);
    GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask);
    check(ok && proc_mask == sys_mask, "... and cleared");

    /* NUMA */
    ok = pGetNumaHighestNodeNumber(&highest);
    printf("highest node %lu\n", highest);
    check(ok && highest < 64, "GetNumaHighestNodeNumber");
    ok = GetNumaNodeProcessorMaskEx(0, &ga);
    printf("node 0: ok %d mask %#llx group %u\n", ok, (ULONGLONG)ga.Mask, ga.Group);
    check(ok && ga.Mask, "GetNumaNodeProcessorMaskEx(0) gives node 0's processors");
    ok = GetNumaNodeProcessorMask(0, &mask);
    check(ok && mask == ga.Mask, "GetNumaNodeProcessorMask(0) agrees");
    SetLastError(0xdeadbeef);
    ok = GetNumaNodeProcessorMaskEx(highest + 1, &ga);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "a node past the highest is refused");
    pn.Group = 0; pn.Number = 0; pn.Reserved = 0;
    ok = GetNumaProcessorNodeEx(&pn, &node);
    check(ok && node <= highest, "GetNumaProcessorNodeEx(processor 0)");
    ok = GetNumaProcessorNode(0, &cnode);
    check(ok && cnode == node, "GetNumaProcessorNode(0) agrees");
    ok = GetNumaAvailableMemoryNodeEx(0, &avail);
    printf("node 0 available %llu MB\n", avail >> 20);
    check(ok && avail > 0, "GetNumaAvailableMemoryNodeEx(0) gives free memory");
    ok = GetNumaProximityNodeEx(0, &node);
    check(ok && node == 0, "GetNumaProximityNodeEx(0)");

    /* ideal processor */
    r = SetThreadIdealProcessor(GetCurrentThread(), 1 % GetActiveProcessorCount(0));
    r = SetThreadIdealProcessor(GetCurrentThread(), MAXIMUM_PROCESSORS);
    printf("ideal processor now %lu\n", r);
    check(r == 1 % GetActiveProcessorCount(0), "SetThreadIdealProcessor keeps it (MAXIMUM_PROCESSORS reads it)");
    ok = GetThreadIdealProcessorEx(GetCurrentThread(), &pn);
    check(ok && pn.Number == 1 % GetActiveProcessorCount(0), "GetThreadIdealProcessorEx reads it");
    pn.Group = 0; pn.Number = 0;
    memset(&prev, 0xcc, sizeof(prev));
    ok = SetThreadIdealProcessorEx(GetCurrentThread(), &pn, &prev);
    check(ok && prev.Number == 1 % GetActiveProcessorCount(0) && prev.Group == 0, "SetThreadIdealProcessorEx gives the previous one");
    SetLastError(0xdeadbeef);
    r = SetThreadIdealProcessor(GetCurrentThread(), 1000);
    check(r == ~0u && GetLastError() == ERROR_INVALID_PARAMETER, "a processor that does not exist is refused");

    /* priority boost */
    ok = GetProcessPriorityBoost(GetCurrentProcess(), &disable);
    check(ok && !disable, "the priority boost is on by default");
    ok = SetProcessPriorityBoost(GetCurrentProcess(), TRUE);
    disable = FALSE;
    ok = ok && GetProcessPriorityBoost(GetCurrentProcess(), &disable);
    check(ok && disable, "SetProcessPriorityBoost(TRUE) is kept");
    disable = FALSE;
    GetThreadPriorityBoost(GetCurrentThread(), &disable);
    check(disable, "a thread follows the process's setting");
    ok = SetThreadPriorityBoost(GetCurrentThread(), FALSE);
    disable = TRUE;
    GetThreadPriorityBoost(GetCurrentThread(), &disable);
    check(ok && !disable, "until it sets its own");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
