/* workingset-probe (patches/sg/9758, test/workingset-gate.sh): QueryWorkingSet (NtQueryVirtualMemory
 * MemoryWorkingSetInformation). Prints:
 *   SMALL <ok|fail gle> <entries>   a 16-byte buffer, as Firefox asks first (Windows: ERROR_BAD_LENGTH, the count set)
 *   FULL <ok|fail gle> <entries>    a buffer for them all
 *   DATA <prot|absent>              the page of a written global: protection code (4 = read/write)
 *   CODE <prot|absent> <shared>     the page of main(): 3 = execute/read, shared (an image page)
 *   UNTOUCHED <absent|present>      a committed page never touched
 *   TOUCHED <prot|absent>           the same page after a write */
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>

static volatile char data_page[8192] = { 1 };

static PSAPI_WORKING_SET_INFORMATION *query(void)
{
    PSAPI_WORKING_SET_INFORMATION small, *ws;
    ULONG_PTR n;

    memset(&small, 0, sizeof(small));
    if (QueryWorkingSet(GetCurrentProcess(), &small, sizeof(small))) printf("SMALL ok %lu\n", (unsigned long)small.NumberOfEntries);
    else printf("SMALL fail %lu %lu\n", GetLastError(), (unsigned long)small.NumberOfEntries);
    n = small.NumberOfEntries + 4096;
    ws = calloc(1, sizeof(ULONG_PTR) + n * sizeof(PSAPI_WORKING_SET_BLOCK));
    if (QueryWorkingSet(GetCurrentProcess(), ws, sizeof(ULONG_PTR) + n * sizeof(PSAPI_WORKING_SET_BLOCK)))
        printf("FULL ok %lu\n", (unsigned long)ws->NumberOfEntries);
    else printf("FULL fail %lu\n", GetLastError());
    return ws;
}

static PSAPI_WORKING_SET_BLOCK *find(PSAPI_WORKING_SET_INFORMATION *ws, const void *p)
{
    ULONG_PTR page = (ULONG_PTR)p >> 12, i;
    for (i = 0; i < ws->NumberOfEntries; i++)
        if (ws->WorkingSetInfo[i].VirtualPage == page) return &ws->WorkingSetInfo[i];
    return NULL;
}

int main(void)
{
    PSAPI_WORKING_SET_INFORMATION *ws;
    PSAPI_WORKING_SET_BLOCK *b;
    char *mem = VirtualAlloc(NULL, 65536, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    data_page[4096] = 2;
    ws = query();
    b = find(ws, (const void *)&data_page[4096]);
    if (b) printf("DATA %u\n", (unsigned)b->Protection); else printf("DATA absent\n");
    b = find(ws, (const void *)main);
    if (b) printf("CODE %u %u\n", (unsigned)b->Protection, (unsigned)b->Shared); else printf("CODE absent 0\n");
    printf("UNTOUCHED %s\n", find(ws, mem + 32768) ? "present" : "absent");
    free(ws);
    mem[32768] = 1;
    ws = query();
    b = find(ws, mem + 32768);
    if (b) printf("TOUCHED %u\n", (unsigned)b->Protection); else printf("TOUCHED absent\n");
    return 0;
}
