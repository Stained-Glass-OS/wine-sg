/* Minidump contents that were missing (patches/sg/1625):
 *
 *  - modules had no CodeView record (the PDB name and GUID a debugger needs
 *    to find symbols);
 *  - MiniDumpWithDataSegs left out the modules' data sections;
 *  - MiniDumpWithHandleData wrote no handle stream;
 *  - MiniDumpScanMemory never marked modules referenced from the stacks;
 *  - MiniDumpFilterMemory left the stacks as they were.
 */
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

static int failures;
static volatile ULONG data_marker = 0x5a5a1234;
static ULONG scan_flags;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BOOL CALLBACK scan_cb(void *param, MINIDUMP_CALLBACK_INPUT *in, MINIDUMP_CALLBACK_OUTPUT *out)
{
    if (in->CallbackType == ModuleCallback && in->Module.BaseOfImage == (ULONG_PTR)GetModuleHandleA(NULL))
        scan_flags = out->ModuleWriteFlags;
    return TRUE;
}

static void *write_dump(MINIDUMP_TYPE type, MINIDUMP_CALLBACK_INFORMATION *cb, HANDLE *map_out, HANDLE *file_out)
{
    HANDLE file = CreateFileA("probe.mdmp", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    HANDLE map;
    void *view;

    if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, NULL, NULL, cb))
    {
        CloseHandle(file);
        return NULL;
    }
    map = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    *map_out = map;
    *file_out = file;
    return view;
}

static void close_dump(void *view, HANDLE map, HANDLE file)
{
    UnmapViewOfFile(view);
    CloseHandle(map);
    CloseHandle(file);
}

/* the dump's memory for an address, or NULL */
static const void *dump_memory(void *dump, ULONG64 addr, ULONG size)
{
    MINIDUMP_MEMORY_LIST *list;
    ULONG i, len;

    if (!MiniDumpReadDumpStream(dump, MemoryListStream, NULL, (void **)&list, &len)) return NULL;
    for (i = 0; i < list->NumberOfMemoryRanges; i++)
    {
        MINIDUMP_MEMORY_DESCRIPTOR *d = list->MemoryRanges + i;
        if (addr >= d->StartOfMemoryRange && addr + size <= d->StartOfMemoryRange + d->Memory.DataSize)
            return (char *)dump + d->Memory.Rva + (addr - d->StartOfMemoryRange);
    }
    return NULL;
}

static BOOL stacks_contain(void *dump, ULONG64 value)
{
    MINIDUMP_THREAD_LIST *threads;
    ULONG i, j, len;

    if (!MiniDumpReadDumpStream(dump, ThreadListStream, NULL, (void **)&threads, &len)) return FALSE;
    for (i = 0; i < threads->NumberOfThreads; i++)
    {
        MINIDUMP_MEMORY_DESCRIPTOR *st = &threads->Threads[i].Stack;
        const char *mem = (char *)dump + st->Memory.Rva;
        for (j = 0; j + 8 <= st->Memory.DataSize; j += 4)
            if (!memcmp(mem + j, &value, 8)) return TRUE;
    }
    return FALSE;
}

int main(void)
{
    volatile ULONG64 secret[4] = { 0x1122334455667788ull, 0x1122334455667788ull, 0, 0 };
    HANDLE event = CreateEventA(NULL, TRUE, FALSE, "sg_minidump_event"), map, file;
    MINIDUMP_CALLBACK_INFORMATION cbi = { scan_cb, NULL };
    void *dump;
    ULONG len, i;

    /* CodeView records */
    if ((dump = write_dump(MiniDumpNormal, NULL, &map, &file)))
    {
        MINIDUMP_MODULE_LIST *mods;
        BOOL found = FALSE;

        if (MiniDumpReadDumpStream(dump, ModuleListStream, NULL, (void **)&mods, &len))
            for (i = 0; i < mods->NumberOfModules; i++)
                if (mods->Modules[i].BaseOfImage == (ULONG_PTR)GetModuleHandleA(NULL))
                {
                    MINIDUMP_LOCATION_DESCRIPTOR *cv = &mods->Modules[i].CvRecord;
                    printf("exe CodeView record: %lu bytes\n", cv->DataSize);
                    found = cv->DataSize >= 24 && !memcmp((char *)dump + cv->Rva, "RSDS", 4);
                }
        check(found, "the module's CodeView record (RSDS) is in the dump");
        check(!dump_memory(dump, (ULONG_PTR)&data_marker, sizeof(data_marker)),
              "a normal dump leaves the data sections out");
        check(stacks_contain(dump, secret[0]), "a normal dump keeps the stack as it is");
        close_dump(dump, map, file);
    }
    else check(0, "MiniDumpWriteDump(MiniDumpNormal)");

    /* data segments */
    if ((dump = write_dump(MiniDumpWithDataSegs, NULL, &map, &file)))
    {
        const ULONG *v = dump_memory(dump, (ULONG_PTR)&data_marker, sizeof(data_marker));
        printf("data marker in dump: %p %#lx\n", v, v ? *v : 0);
        check(v && *v == 0x5a5a1234, "MiniDumpWithDataSegs: the module's data section is in the memory list");
        close_dump(dump, map, file);
    }
    else check(0, "MiniDumpWriteDump(MiniDumpWithDataSegs)");

    /* handles */
    if ((dump = write_dump(MiniDumpWithHandleData, NULL, &map, &file)))
    {
        MINIDUMP_HANDLE_DATA_STREAM *hd;
        BOOL found = FALSE, typed = FALSE;

        if (MiniDumpReadDumpStream(dump, HandleDataStream, NULL, (void **)&hd, &len))
        {
            printf("handle stream: %lu descriptors of %lu bytes\n", hd->NumberOfDescriptors, hd->SizeOfDescriptor);
            for (i = 0; i < hd->NumberOfDescriptors; i++)
            {
                MINIDUMP_HANDLE_DESCRIPTOR *d = (void *)((char *)hd + hd->SizeOfHeader + i * hd->SizeOfDescriptor);
                if (d->Handle != (ULONG_PTR)event) continue;
                found = TRUE;
                if (d->TypeNameRva)
                {
                    MINIDUMP_STRING *t = (void *)((char *)dump + d->TypeNameRva);
                    printf("event handle type %.*ls access %#lx\n", (int)(t->Length / 2), t->Buffer, d->GrantedAccess);
                    typed = !wcsncmp(t->Buffer, L"Event", 5);
                }
                if (d->ObjectNameRva)
                {
                    MINIDUMP_STRING *n = (void *)((char *)dump + d->ObjectNameRva);
                    printf("event handle name %.*ls\n", (int)(n->Length / 2), n->Buffer);
                }
            }
        }
        check(found, "MiniDumpWithHandleData: the handle stream lists the event");
        check(typed, "... with its type");
        close_dump(dump, map, file);
    }
    else check(0, "MiniDumpWriteDump(MiniDumpWithHandleData)");

    /* scan */
    if ((dump = write_dump(MiniDumpScanMemory, &cbi, &map, &file)))
    {
        printf("exe module flags with ScanMemory: %#lx\n", scan_flags);
        check(scan_flags & ModuleReferencedByMemory, "MiniDumpScanMemory marks the exe referenced from the stack");
        close_dump(dump, map, file);
    }
    else check(0, "MiniDumpWriteDump(MiniDumpScanMemory)");

    /* filter */
    if ((dump = write_dump(MiniDumpFilterMemory, NULL, &map, &file)))
    {
        check(!stacks_contain(dump, secret[0]), "MiniDumpFilterMemory takes values that are not addresses out of the stacks");
        close_dump(dump, map, file);
    }
    else check(0, "MiniDumpWriteDump(MiniDumpFilterMemory)");

    DeleteFileA("probe.mdmp");
    printf("secret %llx\n", (unsigned long long)secret[1]);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
