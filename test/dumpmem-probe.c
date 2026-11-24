/* Memory a minidump's type asks for (patches/sg/9552), 64- and 32-bit:
 * MiniDumpWithProcessThreadData (the PEB, the process parameters with the
 * command line and environment, the TEBs), MiniDumpWithPrivateReadWriteMemory
 * (private read-write regions: what .NET's heap dumps are made of),
 * MiniDumpWithIndirectlyReferencedMemory (what the stacks point to),
 * MiniDumpWithCodeSegs (modules' code sections), MiniDumpWithModuleHeaders
 * (PE headers, debug and x86-64 exception directories), and full-memory dumps
 * without no-access pages. No dump's memory list has overlapping ranges.
 */
#include <windows.h>
#include <winternl.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static BYTE *dump_of(MINIDUMP_TYPE type, DWORD *len)
{
    WCHAR path[MAX_PATH];
    HANDLE file;
    BYTE *data;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"dumpmem.mdmp");
    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, NULL, NULL, NULL))
    {
        printf("info  MiniDumpWriteDump(%#x) failed %#lx\n", type, GetLastError());
        CloseHandle(file);
        DeleteFileW(path);
        return NULL;
    }
    *len = GetFileSize(file, NULL);
    data = HeapAlloc(GetProcessHeap(), 0, *len);
    SetFilePointer(file, 0, NULL, FILE_BEGIN);
    ReadFile(file, data, *len, len, NULL);
    CloseHandle(file);
    DeleteFileW(path);
    return data;
}

static BYTE *stream(BYTE *dump, DWORD len, ULONG type, ULONG *size)
{
    MINIDUMP_HEADER *hdr = (MINIDUMP_HEADER *)dump;
    MINIDUMP_DIRECTORY *dir;
    ULONG i;

    if (!dump || len < sizeof(*hdr) || hdr->Signature != 0x504d444d /* MDMP */) return NULL;
    dir = (MINIDUMP_DIRECTORY *)(dump + hdr->StreamDirectoryRva);
    for (i = 0; i < hdr->NumberOfStreams; i++)
        if (dir[i].StreamType == type)
        {
            if (dir[i].Location.Rva + (ULONG64)dir[i].Location.DataSize > len) return NULL;
            *size = dir[i].Location.DataSize;
            return dump + dir[i].Location.Rva;
        }
    return NULL;
}

/* how many bytes of [addr, addr+size) the memory list (or the full-memory
 * list) holds, and whether they are the process's bytes */
static ULONG64 covered(BYTE *dump, DWORD len, ULONG64 addr, ULONG64 size, BOOL *same)
{
    ULONG ssize, i;
    ULONG64 done = 0, b, e, rva;
    MINIDUMP_MEMORY_LIST *l = (MINIDUMP_MEMORY_LIST *)stream(dump, len, MemoryListStream, &ssize);
    MINIDUMP_MEMORY64_LIST *l64 = (MINIDUMP_MEMORY64_LIST *)stream(dump, len, Memory64ListStream, &ssize);

    if (same) *same = TRUE;
    for (i = 0; l && i < l->NumberOfMemoryRanges; i++)
    {
        MINIDUMP_MEMORY_DESCRIPTOR *d = &l->MemoryRanges[i];
        b = max(d->StartOfMemoryRange, addr);
        e = min(d->StartOfMemoryRange + d->Memory.DataSize, addr + size);
        if (b >= e) continue;
        rva = d->Memory.Rva + (b - d->StartOfMemoryRange);
        if (rva + (e - b) > len) { if (same) *same = FALSE; continue; }
        if (same && memcmp(dump + rva, (void *)(ULONG_PTR)b, e - b)) *same = FALSE;
        done += e - b;
    }
    if (l64)
    {
        rva = l64->BaseRva;
        for (i = 0; i < l64->NumberOfMemoryRanges; rva += l64->MemoryRanges[i].DataSize, i++)
        {
            MINIDUMP_MEMORY_DESCRIPTOR64 *d = &l64->MemoryRanges[i];
            b = max(d->StartOfMemoryRange, addr);
            e = min(d->StartOfMemoryRange + d->DataSize, addr + size);
            if (b >= e) continue;
            if (same && (rva + (b - d->StartOfMemoryRange) + (e - b) > len ||
                         memcmp(dump + rva + (b - d->StartOfMemoryRange), (void *)(ULONG_PTR)b, e - b)))
                *same = FALSE;
            done += e - b;
        }
    }
    return done;
}

static BOOL no_overlap(BYTE *dump, DWORD len)
{
    ULONG ssize, i, j;
    MINIDUMP_MEMORY_LIST *l = (MINIDUMP_MEMORY_LIST *)stream(dump, len, MemoryListStream, &ssize);

    for (i = 0; l && i < l->NumberOfMemoryRanges; i++)
        for (j = i + 1; j < l->NumberOfMemoryRanges; j++)
        {
            ULONG64 b1 = l->MemoryRanges[i].StartOfMemoryRange, e1 = b1 + l->MemoryRanges[i].Memory.DataSize;
            ULONG64 b2 = l->MemoryRanges[j].StartOfMemoryRange, e2 = b2 + l->MemoryRanges[j].Memory.DataSize;
            if (b1 < e2 && b2 < e1)
            {
                printf("info  overlap %I64x-%I64x %I64x-%I64x\n", b1, e1, b2, e2);
                return FALSE;
            }
        }
    return TRUE;
}

static IMAGE_SECTION_HEADER *code_section(HMODULE mod)
{
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)mod + ((IMAGE_DOS_HEADER *)mod)->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    unsigned i;

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) return &sec[i];
    return NULL;
}

/* what the stacks point to comes in with MiniDumpWithIndirectlyReferencedMemory:
 * there, what is not asked for may still be in */
#define EXPECT(in, want, ...) do { if ((want) || !indirect) CHECK((in) == (want), __VA_ARGS__); } while (0)

static void check(const char *what, MINIDUMP_TYPE type, BYTE *rw, BYTE *ro, BYTE *noaccess, BYTE *heap)
{
    TEB *teb = NtCurrentTeb();
    PEB *peb = teb->ProcessEnvironmentBlock;
    RTL_USER_PROCESS_PARAMETERS *params = peb->ProcessParameters;
    void *env = *(void **)((BYTE *)params + (sizeof(void *) == 8 ? 0x80 : 0x48));
    HMODULE kb = GetModuleHandleA("kernelbase.dll");
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)kb + ((IMAGE_DOS_HEADER *)kb)->e_lfanew);
    IMAGE_SECTION_HEADER *text = code_section(kb);
    DWORD len;
    BYTE *dump = dump_of(type, &len);
    BOOL same, want, indirect = (type & MiniDumpWithIndirectlyReferencedMemory) != 0;

    CHECK(dump != NULL, "%s: dump written", what);
    if (!dump) return;
    CHECK(no_overlap(dump, len), "%s: no two memory list ranges overlap", what);

    want = (type & (MiniDumpWithProcessThreadData | MiniDumpWithPrivateReadWriteMemory | MiniDumpWithFullMemory)) != 0;
    EXPECT((covered(dump, len, (ULONG_PTR)peb, 0x100, &same) == 0x100), want, "%s: the PEB %s", what, want ? "is in" : "is not in");
    EXPECT((covered(dump, len, (ULONG_PTR)teb, 0x100, NULL) == 0x100), want, "%s: this thread's TEB %s", what, want ? "is in" : "is not in");
    EXPECT((covered(dump, len, (ULONG_PTR)params, 0x40, NULL) == 0x40), want, "%s: the process parameters %s", what, want ? "are in" : "are not in");
    EXPECT((covered(dump, len, (ULONG_PTR)params->CommandLine.Buffer, params->CommandLine.Length, &same) == params->CommandLine.Length && same), want,
          "%s: the command line %s", what, want ? "is in" : "is not in");
    EXPECT((env && covered(dump, len, (ULONG_PTR)env, 0x40, NULL) == 0x40), want, "%s: the environment %s", what, want ? "is in" : "is not in");

    want = (type & (MiniDumpWithPrivateReadWriteMemory | MiniDumpWithFullMemory)) != 0;
    EXPECT((covered(dump, len, (ULONG_PTR)rw, 0x20000, &same) == 0x20000 && same), want,
          "%s: a private read-write region %s", what, want ? "is in, with its bytes" : "is not in");
    EXPECT(covered(dump, len, (ULONG_PTR)ro, 0x1000, NULL) != 0, !!(type & MiniDumpWithFullMemory),
          "%s: a private read-only region %s", what, (type & MiniDumpWithFullMemory) ? "is in" : "is not in");

    want = (type & (MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithPrivateReadWriteMemory | MiniDumpWithFullMemory)) != 0;
    CHECK((covered(dump, len, (ULONG_PTR)heap, 0x100, &same) == 0x100 && same) == want,
          "%s: a heap block a stack variable points to %s", what, want ? "is in" : "is not in");

    want = (type & (MiniDumpWithCodeSegs | MiniDumpWithFullMemory)) != 0;
    EXPECT(text && (covered(dump, len, (ULONG_PTR)kb + text->VirtualAddress, 0x1000, &same) == 0x1000 && same), want,
          "%s: kernelbase's code %s", what, want ? "is in" : "is not in");

    want = (type & (MiniDumpWithModuleHeaders | MiniDumpWithFullMemory)) != 0;
    EXPECT((covered(dump, len, (ULONG_PTR)kb, 0x200, &same) == 0x200 && same), want,
          "%s: kernelbase's PE headers %s", what, want ? "are in" : "are not in");
    if (sizeof(void *) == 8)
    {
        IMAGE_DATA_DIRECTORY *ex = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        EXPECT(ex->VirtualAddress && (covered(dump, len, (ULONG_PTR)kb + ex->VirtualAddress, ex->Size, NULL) == ex->Size), want,
              "%s: kernelbase's unwind data %s", what, want ? "is in" : "is not in");
    }

    if (type & MiniDumpWithFullMemory)
        CHECK(covered(dump, len, (ULONG_PTR)noaccess, 0x1000, NULL) == 0, "%s: a no-access region is not in", what);
    HeapFree(GetProcessHeap(), 0, dump);
}

int main(void)
{
    static const struct { const char *name; MINIDUMP_TYPE type; } types[] =
    {
        { "normal", MiniDumpNormal },
        { "process-thread-data", MiniDumpWithProcessThreadData },
        { "private-rw", MiniDumpWithPrivateReadWriteMemory },
        { "indirect", MiniDumpWithIndirectlyReferencedMemory },
        { "code-segs", MiniDumpWithCodeSegs },
        { "module-headers", MiniDumpWithModuleHeaders },
        { "full", MiniDumpWithFullMemory },
        { "all", MiniDumpWithProcessThreadData | MiniDumpWithPrivateReadWriteMemory | MiniDumpWithIndirectlyReferencedMemory |
                 MiniDumpWithCodeSegs | MiniDumpWithModuleHeaders | MiniDumpWithDataSegs | MiniDumpWithFullMemoryInfo },
    };
    BYTE *rw, *ro, *noaccess;
    BYTE *volatile heap;
    DWORD old;
    unsigned i;

    printf("info  %d-bit\n", (int)sizeof(void *) * 8);
    rw = VirtualAlloc(NULL, 0x20000, MEM_COMMIT, PAGE_READWRITE);
    for (i = 0; i < 0x20000; i++) rw[i] = (BYTE)(i * 13 + 1);
    ro = VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_READWRITE);
    memset(ro, 0x5a, 0x1000);
    VirtualProtect(ro, 0x1000, PAGE_READONLY, &old);
    noaccess = VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_NOACCESS);
    /* a heap block far from anything else on the heap the stack points to */
    heap = HeapAlloc(GetProcessHeap(), 0, 0x200);
    for (i = 0; i < 0x200; i++) heap[i] = (BYTE)(i ^ 0xa5);

    for (i = 0; i < ARRAYSIZE(types); i++) check(types[i].name, types[i].type, rw, ro, noaccess, heap);
    heap[0] ^= 1;  /* the pointer stays live on the stack */
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
