/* NtQueryVirtualMemory(MemoryRegionInformation) on ANOTHER process (it answered
 * STATUS_NOT_IMPLEMENTED): the region of the child's image, of a private allocation made in
 * it, and of a free address; the answers must agree with VirtualQueryEx on the same address. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

#define MemoryRegionInformation 3

typedef struct
{
    PVOID AllocationBase;
    ULONG AllocationProtect;
    ULONG RegionType;
    SIZE_T RegionSize;
    SIZE_T CommitSize;
    ULONG_PTR PartitionId;
    ULONG_PTR NodePreference;
} REGION_INFO;

typedef NTSTATUS (WINAPI *query_fn)(HANDLE, const void *, int, void *, SIZE_T, SIZE_T *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(int argc, char **argv)
{
    query_fn NtQueryVirtualMemory = (query_fn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryVirtualMemory");
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = {sizeof(si)};
    char cmd[MAX_PATH + 32];
    HANDLE ev;
    unsigned int i;
    REGION_INFO ri;
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T len;
    NTSTATUS status;
    void *priv, *image;

    if (argc > 1 && !strcmp(argv[1], "child"))
    {
        ev = OpenEventA(SYNCHRONIZE, FALSE, argv[2]);
        VirtualAlloc((void *)0x20000000, 0x8000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        WaitForSingleObject(ev, 60000);
        return 0;
    }

    ev = CreateEventA(NULL, TRUE, FALSE, "sg_regioninfo_probe");
    sprintf(cmd, "\"%s\" child sg_regioninfo_probe", argv[0]);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("FAIL  cannot start the child\nRESULT: FAIL\n");
        return 1;
    }
    Sleep(1500);  /* the child has made its allocation */

    {
        PROCESS_BASIC_INFORMATION pbi;
        PEB peb;
        NtQueryInformationProcess(pi.hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);
        ReadProcessMemory(pi.hProcess, pbi.PebBaseAddress, &peb, sizeof(peb), NULL);
        image = peb.Reserved3[1];  /* ImageBaseAddress */
    }
    priv = (void *)0x20000000;
    printf("      child image at %p\n", image);

    {
        static const struct { int use_private; const char *name; } cases[] =
        {
            {0, "the child's image"},
            {1, "a private allocation in the child"},
        };
        for (i = 0; i < 2; ++i)
        {
            void *addr = cases[i].use_private ? priv : image;
            memset(&ri, 0xcc, sizeof(ri));
            memset(&mbi, 0, sizeof(mbi));
            len = 0;
            status = NtQueryVirtualMemory(pi.hProcess, addr, MemoryRegionInformation, &ri, sizeof(ri), &len);
            check(status == 0, cases[i].name);
            if (status) { printf("      status %#lx\n", status); continue; }
            check(VirtualQueryEx(pi.hProcess, addr, &mbi, sizeof(mbi)) != 0, "  VirtualQueryEx agrees it is there");
            check(ri.AllocationBase == mbi.AllocationBase, "  allocation base equals VirtualQueryEx's");
            check(ri.AllocationProtect == mbi.AllocationProtect, "  allocation protection equals VirtualQueryEx's");
            check(ri.RegionSize == mbi.RegionSize && ri.RegionSize, "  region size equals VirtualQueryEx's");
            check(ri.CommitSize == (mbi.State == MEM_COMMIT ? mbi.RegionSize : 0), "  commit size is the committed part");
            check(len == sizeof(ri), "  returned length");
        }
    }
    check(ri.RegionSize == 0x8000 && ri.CommitSize == 0x8000 && ri.AllocationBase == priv, "the private allocation is 0x8000 bytes, committed");

    /* length checks as for the current process */
    status = NtQueryVirtualMemory(pi.hProcess, priv, MemoryRegionInformation, &ri, 8, &len);
    check(status == 0xC0000004, "too short a buffer is STATUS_INFO_LENGTH_MISMATCH");

    SetEvent(ev);
    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
