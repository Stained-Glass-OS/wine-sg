/* wine-sg 9704: RtlCreateQueryDebugBuffer / RtlQueryProcessDebugInformation / RtlDestroyQueryDebugBuffer
 * (ntdll; CodeLite and WinMerge import them). The modules, heaps (with blocks) and locks of the process in
 * the layouts Windows uses; another process's modules. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { PVOID Section, MappedBase, ImageBase; ULONG ImageSize, Flags; USHORT LoadOrderIndex, InitOrderIndex, LoadCount, NameOffset; char Name[256]; } MODINFO;
typedef struct { ULONG Count; MODINFO Modules[1]; } MODLIST;
typedef struct { SIZE_T Size; USHORT Flags, BackTrace; union { struct { SIZE_T Settable; ULONG Tag; } s1; struct { SIZE_T Committed; PVOID First; } s2; } u; } HEAPENTRY;
typedef struct { PVOID Base; ULONG Flags; USHORT Overhead, BackTrace; SIZE_T Allocated, Committed; ULONG Tags, Entries, PseudoTags, Gran, Reserved[5]; PVOID TagList; HEAPENTRY *EntryList; } HEAPINFO;
typedef struct { ULONG Count; HEAPINFO Heaps[1]; } HEAPLIST;
typedef struct { HANDLE SectionHandle; PVOID Base, RemoteBase; ULONG_PTR Delta; HANDLE EventPair, EventPair2, Pid, Thread; ULONG Flags; SIZE_T OffsetFree, Commit, View;
                 MODLIST *Modules; PVOID BackTraces; HEAPLIST *Heaps; ULONG *Locks; } DBGINFO;

typedef DBGINFO *(WINAPI *create_t)(ULONG, BOOLEAN);
typedef NTSTATUS (WINAPI *query_t)(ULONG, ULONG, DBGINFO *);
typedef NTSTATUS (WINAPI *destroy_t)(DBGINFO *);

int main(int argc, char **argv)
{
    HMODULE ntdll = GetModuleHandleA("ntdll");
    create_t create = (create_t)GetProcAddress(ntdll, "RtlCreateQueryDebugBuffer");
    query_t query = (query_t)GetProcAddress(ntdll, "RtlQueryProcessDebugInformation");
    destroy_t destroy = (destroy_t)GetProcAddress(ntdll, "RtlDestroyQueryDebugBuffer");
    DBGINFO *buf;
    NTSTATUS st;
    HANDLE heap, heaps[64];
    ULONG i, j, found_ntdll = 0, found_k32 = 0, busy = 0;
    DWORD nheaps;
    HEAPINFO *mine = NULL;

    if (argc > 1) { Sleep(30000); return 0; }
    heap = HeapCreate(0, 0, 0);
    for (i = 0; i < 10; i++) HeapAlloc(heap, 0, 1000);
    buf = create(0, FALSE);
    CHECK(buf != NULL, "RtlCreateQueryDebugBuffer");
    st = query(GetCurrentProcessId(), 0x01 | 0x04 | 0x20, buf);
    CHECK(st == 0, "query modules, heaps and locks of this process: %#lx", st);
    CHECK(buf->Modules && buf->Modules->Count >= 3 && buf->Modules->Modules[0].ImageBase == GetModuleHandleA(NULL),
          "modules: %lu, the first is the program", buf->Modules ? buf->Modules->Count : 0);
    for (i = 0; buf->Modules && i < buf->Modules->Count; i++)
    {
        MODINFO *m = &buf->Modules->Modules[i];
        if (!lstrcmpiA(m->Name + m->NameOffset, "ntdll.dll") && m->ImageBase == ntdll) found_ntdll = 1;
        if (!lstrcmpiA(m->Name + m->NameOffset, "kernel32.dll") && m->ImageBase == GetModuleHandleA("kernel32") &&
            m->ImageSize > 0 && strchr(m->Name, '\\')) found_k32 = 1;
    }
    CHECK(found_ntdll && found_k32, "ntdll and kernel32 listed with their bases, sizes and full names");
    nheaps = GetProcessHeaps(64, heaps);
    CHECK(buf->Heaps && buf->Heaps->Count == nheaps && buf->Heaps->Heaps[0].Base == GetProcessHeap(),
          "heaps: %lu (GetProcessHeaps %lu), the first is the process heap", buf->Heaps ? buf->Heaps->Count : 0, nheaps);
    for (i = 0; buf->Heaps && i < buf->Heaps->Count; i++) if (buf->Heaps->Heaps[i].Base == heap) mine = &buf->Heaps->Heaps[i];
    CHECK(mine && mine->Allocated >= 10000 && mine->Committed >= mine->Allocated && mine->Entries >= 10 && !mine->EntryList,
          "a private heap with ten 1000-byte blocks: allocated %Iu, committed %Iu, entries %lu",
          mine ? mine->Allocated : 0, mine ? mine->Committed : 0, mine ? mine->Entries : 0);
    CHECK(buf->Locks != NULL, "a lock list");
    st = query(GetCurrentProcessId(), 0x04 | 0x10, buf);
    mine = NULL;
    for (i = 0; buf->Heaps && i < buf->Heaps->Count; i++) if (buf->Heaps->Heaps[i].Base == heap) mine = &buf->Heaps->Heaps[i];
    for (j = 0; mine && mine->EntryList && j < mine->Entries; j++)
        if ((mine->EntryList[j].Flags & 1) && mine->EntryList[j].Size == 1000) busy++;
    CHECK(st == 0 && mine && mine->EntryList && busy == 10, "PDI_HEAP_BLOCKS: the ten busy 1000-byte blocks listed (%lu)", busy);
    CHECK(!buf->Modules, "a query without PDI_MODULES leaves no module list");
    CHECK(destroy(buf) == 0, "RtlDestroyQueryDebugBuffer");

    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 16], self[MAX_PATH];
        GetModuleFileNameA(NULL, self, sizeof(self));
        sprintf(cmd, "\"%s\" child", self);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            Sleep(1500);
            buf = create(0, FALSE);
            st = query(pi.dwProcessId, 0x01, buf);
            CHECK(st == 0 && buf->Modules && buf->Modules->Count >= 3 &&
                  !lstrcmpiA(buf->Modules->Modules[0].Name + buf->Modules->Modules[0].NameOffset, strrchr(self, '\\') + 1),
                  "another process's modules: %#lx, %lu, first %s", st, buf->Modules ? buf->Modules->Count : 0,
                  buf->Modules ? buf->Modules->Modules[0].Name : "");
            destroy(buf);
            TerminateProcess(pi.hProcess, 0);
        }
        else CHECK(0, "start the child");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
