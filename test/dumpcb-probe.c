/* MiniDumpWriteDump's callbacks the way Windows makes them (patches/sg/9551):
 * the callback types and their preset outputs (as Wine's own dbghelp
 * conformance test records Windows' behaviour), IncludeThreadCallback and
 * IncludeModuleCallback leaving a thread or module out, MemoryCallback adding
 * memory, RemoveMemoryCallback taking it out again, CancelCallback cancelling,
 * IoStartCallback taking the writes (IoWriteAllCallback, IoFinishCallback),
 * ThreadCallback's thread handle. .NET's createdump and Breakpad drive their
 * dumps through these.
 */
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define MASK(t) (1ull << (t))

/* the callback types and structures as the Windows SDK has them (mingw's
 * dbghelp.h has only the first few) */
enum { CB_MODULE, CB_THREAD, CB_THREAD_EX, CB_INCLUDE_THREAD, CB_INCLUDE_MODULE, CB_MEMORY, CB_CANCEL,
       CB_WRITE_KERNEL_MINIDUMP, CB_KERNEL_MINIDUMP_STATUS, CB_REMOVE_MEMORY, CB_INCLUDE_VM_REGION, CB_IO_START,
       CB_IO_WRITE_ALL, CB_IO_FINISH, CB_READ_MEMORY_FAILURE, CB_SECONDARY_FLAGS, CB_IS_PROCESS_SNAPSHOT,
       CB_VM_START };
#include <pshpack4.h>
typedef struct
{
    ULONG ProcessId;
    HANDLE ProcessHandle;
    ULONG CallbackType;
    union
    {
        HRESULT Status;
        MINIDUMP_THREAD_CALLBACK Thread;
        struct { ULONG ThreadId; } IncludeThread;
        struct { ULONG64 BaseOfImage; } IncludeModule;
        struct { HANDLE Handle; ULONG64 Offset; PVOID Buffer; ULONG BufferBytes; } Io;
        ULONG SecondaryFlags;
    };
} CB_INPUT;
typedef struct
{
    union
    {
        ULONG ModuleWriteFlags;
        ULONG ThreadWriteFlags;
        ULONG SecondaryFlags;
        struct { ULONG64 MemoryBase; ULONG MemorySize; };
        struct { BOOL CheckCancel; BOOL Cancel; };
        HANDLE Handle;
        struct { MINIDUMP_MEMORY_INFO VmRegion; BOOL Continue; };
        HRESULT Status;
    };
} CB_OUTPUT;
#include <poppack.h>

struct cbstate
{
    /* what to do */
    DWORD    exclude_tid;
    ULONG64  exclude_module;
    BYTE    *add;            /* CB_MEMORY: this block */
    ULONG    add_size;
    int      add_done;
    ULONG64  remove_base;    /* CB_REMOVE_MEMORY: this range */
    ULONG    remove_size;
    int      remove_done;
    int      cancel_at;      /* CB_CANCEL: cancel on this call */
    int      take_io;        /* CB_IO_START: S_OK */
    /* what happened */
    ULONG64  types;
    int      bad_process, bad_preset;
    int      threads, include_threads, modules, include_modules, cancels;
    int      bad_handle;
    BYTE    *io;             /* the dump CB_IO_WRITE_ALL was given */
    ULONG64  io_size;
    int      io_finish, io_handle_ok;
    HANDLE   file;
};

static BOOL CALLBACK cb(void *param, CB_INPUT *in, CB_OUTPUT *out)
{
    struct cbstate *s = param;

    s->types |= MASK(in->CallbackType);
    switch (in->CallbackType)
    {
    case CB_IO_START: case CB_IO_WRITE_ALL: case CB_IO_FINISH: case CB_WRITE_KERNEL_MINIDUMP:
        if (in->ProcessId || in->ProcessHandle) s->bad_process++;
        break;
    case CB_IS_PROCESS_SNAPSHOT: case CB_VM_START:
        if (in->ProcessId || in->ProcessHandle != GetCurrentProcess()) s->bad_process++;
        break;
    default:
        if (in->ProcessId != GetCurrentProcessId() || in->ProcessHandle != GetCurrentProcess()) s->bad_process++;
        break;
    }
    switch (in->CallbackType)
    {
    case CB_INCLUDE_THREAD:
        s->include_threads++;
        return in->IncludeThread.ThreadId != s->exclude_tid;
    case CB_INCLUDE_MODULE:
        s->include_modules++;
        return in->IncludeModule.BaseOfImage != s->exclude_module;
    case CB_THREAD:
        s->threads++;
        if (!in->Thread.ThreadHandle || GetThreadId(in->Thread.ThreadHandle) != in->Thread.ThreadId) s->bad_handle++;
        return TRUE;
    case CB_MODULE:
        s->modules++;
        return TRUE;
    case CB_MEMORY:
        if (out->MemoryBase || out->MemorySize) s->bad_preset++;
        if (s->add && !s->add_done++)
        {
            out->MemoryBase = (ULONG_PTR)s->add;
            out->MemorySize = s->add_size;
        }
        return TRUE;
    case CB_REMOVE_MEMORY:
        if (out->MemoryBase || out->MemorySize) s->bad_preset++;
        if (s->remove_size && !s->remove_done++)
        {
            out->MemoryBase = s->remove_base;
            out->MemorySize = s->remove_size;
        }
        return TRUE;
    case CB_CANCEL:
        if (out->Cancel || out->CheckCancel) s->bad_preset++;
        s->cancels++;
        if (s->cancel_at)
        {
            out->CheckCancel = TRUE;
            out->Cancel = s->cancels >= s->cancel_at;
        }
        return TRUE;
    case CB_IO_START:
        if (out->Status != E_NOTIMPL) s->bad_preset++;
        if (in->Io.Handle == s->file) s->io_handle_ok++;
        if (s->take_io) out->Status = S_OK;
        return TRUE;
    case CB_IO_WRITE_ALL:
        if (in->Io.Offset + in->Io.BufferBytes > s->io_size)
        {
            s->io = s->io ? HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, s->io, in->Io.Offset + in->Io.BufferBytes)
                          : HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, in->Io.Offset + in->Io.BufferBytes);
            s->io_size = in->Io.Offset + in->Io.BufferBytes;
        }
        memcpy(s->io + in->Io.Offset, in->Io.Buffer, in->Io.BufferBytes);
        out->Status = S_OK;
        return TRUE;
    case CB_IO_FINISH:
        s->io_finish++;
        out->Status = S_OK;
        return TRUE;
    case CB_SECONDARY_FLAGS:
        if (in->SecondaryFlags || out->SecondaryFlags) s->bad_preset++;
        return TRUE;
    case CB_WRITE_KERNEL_MINIDUMP:
        if (out->Handle) s->bad_preset++;
        return TRUE;
    default:
        return TRUE;
    }
}

static DWORD WINAPI worker(void *arg)
{
    WaitForSingleObject((HANDLE)arg, INFINITE);
    return 0;
}

static BYTE *write_dump(struct cbstate *s, MINIDUMP_TYPE type, BOOL *ret, DWORD *err, DWORD *len)
{
    WCHAR path[MAX_PATH];
    MINIDUMP_CALLBACK_INFORMATION cbi = { (MINIDUMP_CALLBACK_ROUTINE)cb, s };
    BYTE *data = NULL;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"dumpcb.mdmp");
    s->file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    SetLastError(0xdeadbeef);
    *ret = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), s->file, type, NULL, NULL, &cbi);
    *err = GetLastError();
    *len = GetFileSize(s->file, NULL);
    if (*len)
    {
        data = HeapAlloc(GetProcessHeap(), 0, *len);
        SetFilePointer(s->file, 0, NULL, FILE_BEGIN);
        ReadFile(s->file, data, *len, len, NULL);
    }
    CloseHandle(s->file);
    DeleteFileW(path);
    return data;
}

static BYTE *stream(BYTE *dump, ULONG64 len, ULONG type, ULONG *size)
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

static BOOL has_thread(BYTE *dump, ULONG64 len, DWORD tid)
{
    ULONG size, i;
    MINIDUMP_THREAD_LIST *l = (MINIDUMP_THREAD_LIST *)stream(dump, len, ThreadListStream, &size);
    for (i = 0; l && i < l->NumberOfThreads; i++) if (l->Threads[i].ThreadId == tid) return TRUE;
    return FALSE;
}

static int thread_count(BYTE *dump, ULONG64 len)
{
    ULONG size;
    MINIDUMP_THREAD_LIST *l = (MINIDUMP_THREAD_LIST *)stream(dump, len, ThreadListStream, &size);
    return l ? l->NumberOfThreads : -1;
}

static BOOL has_module(BYTE *dump, ULONG64 len, ULONG64 base)
{
    ULONG size, i;
    MINIDUMP_MODULE_LIST *l = (MINIDUMP_MODULE_LIST *)stream(dump, len, ModuleListStream, &size);
    for (i = 0; l && i < l->NumberOfModules; i++) if (l->Modules[i].BaseOfImage == base) return TRUE;
    return FALSE;
}

/* the bytes of [addr, addr+size) the memory list holds; -1 if the list does not cover it all */
static int memory_has(BYTE *dump, ULONG64 len, ULONG64 addr, ULONG size, const BYTE *expect)
{
    ULONG ssize, i, done = 0;
    MINIDUMP_MEMORY_LIST *l = (MINIDUMP_MEMORY_LIST *)stream(dump, len, MemoryListStream, &ssize);

    for (i = 0; l && i < l->NumberOfMemoryRanges; i++)
    {
        MINIDUMP_MEMORY_DESCRIPTOR *d = &l->MemoryRanges[i];
        ULONG64 b = max(d->StartOfMemoryRange, addr), e = min(d->StartOfMemoryRange + d->Memory.DataSize, addr + size);
        if (b >= e) continue;
        if (d->Memory.Rva + (b - d->StartOfMemoryRange) + (e - b) > len) return -1;
        if (expect && memcmp(dump + d->Memory.Rva + (b - d->StartOfMemoryRange), expect + (b - addr), e - b)) return -2;
        done += e - b;
    }
    return done;
}

int main(void)
{
    static const ULONG64 windows_types =
        MASK(CB_MODULE) | MASK(CB_THREAD) | MASK(CB_INCLUDE_THREAD) | MASK(CB_INCLUDE_MODULE) |
        MASK(CB_MEMORY) | MASK(CB_CANCEL) | MASK(CB_WRITE_KERNEL_MINIDUMP) | MASK(CB_REMOVE_MEMORY) |
        MASK(CB_IO_START) | MASK(CB_SECONDARY_FLAGS) | MASK(CB_IS_PROCESS_SNAPSHOT) | MASK(CB_VM_START);
    struct cbstate s;
    HANDLE stop, th;
    DWORD tid, err, len;
    BYTE *dump, *block;
    BOOL ret;
    ULONG64 dbghelp = (ULONG_PTR)GetModuleHandleA("dbghelp.dll");
    ULONG i;

    printf("info  %d-bit\n", (int)sizeof(void *) * 8);
    stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, worker, stop, 0, &tid);
    block = VirtualAlloc(NULL, 0x3000, MEM_COMMIT, PAGE_READWRITE);
    for (i = 0; i < 0x3000; i++) block[i] = (BYTE)(i * 7 + 3);

    /* 1. plain dump: the callback types and their presets */
    memset(&s, 0, sizeof(s));
    dump = write_dump(&s, MiniDumpNormal, &ret, &err, &len);
    CHECK(ret, "plain dump written (%#lx)", ret ? 0 : err);
    CHECK(s.types == windows_types, "the callback types Windows makes (%I64x, Windows %I64x)", s.types, windows_types);
    CHECK(!s.bad_process, "each callback names the process as Windows does (%d wrong)", s.bad_process);
    CHECK(!s.bad_preset, "outputs preset as on Windows (%d wrong)", s.bad_preset);
    CHECK(s.include_threads == s.threads && s.threads > 0, "IncludeThreadCallback for each thread (%d, %d)", s.include_threads, s.threads);
    CHECK(s.include_modules == s.modules && s.modules > 5, "IncludeModuleCallback for each module (%d, %d)", s.include_modules, s.modules);
    CHECK(!s.bad_handle, "ThreadCallback gives each thread's handle (%d wrong)", s.bad_handle);
    CHECK(has_thread(dump, len, tid) && has_module(dump, len, dbghelp), "the worker and dbghelp are in a plain dump");
    HeapFree(GetProcessHeap(), 0, dump);

    /* 2. a thread and a module left out */
    memset(&s, 0, sizeof(s));
    s.exclude_tid = tid;
    s.exclude_module = dbghelp;
    dump = write_dump(&s, MiniDumpNormal, &ret, &err, &len);
    CHECK(ret && dump, "dump written");
    CHECK(!has_thread(dump, len, tid), "IncludeThreadCallback FALSE leaves the thread out");
    CHECK(thread_count(dump, len) >= 1, "the other threads stay (%d)", thread_count(dump, len));
    CHECK(!has_module(dump, len, dbghelp), "IncludeModuleCallback FALSE leaves the module out");
    CHECK(has_module(dump, len, (ULONG_PTR)GetModuleHandleA("kernel32.dll")), "the other modules stay");
    HeapFree(GetProcessHeap(), 0, dump);

    /* 3. memory added, then a part of it removed */
    memset(&s, 0, sizeof(s));
    s.add = block;
    s.add_size = 0x3000;
    dump = write_dump(&s, MiniDumpNormal, &ret, &err, &len);
    CHECK(ret && memory_has(dump, len, (ULONG_PTR)block, 0x3000, block) == 0x3000,
          "MemoryCallback's block is in the memory list with its bytes (%d)", memory_has(dump, len, (ULONG_PTR)block, 0x3000, block));
    HeapFree(GetProcessHeap(), 0, dump);
    memset(&s, 0, sizeof(s));
    s.add = block;
    s.add_size = 0x3000;
    s.remove_base = (ULONG_PTR)block + 0x1000;
    s.remove_size = 0x1000;
    dump = write_dump(&s, MiniDumpNormal, &ret, &err, &len);
    CHECK(ret && memory_has(dump, len, (ULONG_PTR)block, 0x1000, block) == 0x1000 &&
          memory_has(dump, len, (ULONG_PTR)block + 0x2000, 0x1000, block + 0x2000) == 0x1000,
          "RemoveMemoryCallback keeps the rest of the block");
    CHECK(memory_has(dump, len, (ULONG_PTR)block + 0x1000, 0x1000, NULL) == 0,
          "RemoveMemoryCallback's range is not in the dump (%d)", memory_has(dump, len, (ULONG_PTR)block + 0x1000, 0x1000, NULL));
    HeapFree(GetProcessHeap(), 0, dump);

    /* 4. cancelled */
    memset(&s, 0, sizeof(s));
    s.cancel_at = 3;
    dump = write_dump(&s, MiniDumpNormal, &ret, &err, &len);
    CHECK(!ret, "CancelCallback's Cancel makes MiniDumpWriteDump fail");
    CHECK(err == HRESULT_FROM_WIN32(ERROR_CANCELLED), "the last error is HRESULT_FROM_WIN32(ERROR_CANCELLED) (%#lx)", err);
    CHECK(s.cancels >= 3, "CheckCancel keeps CancelCallback coming (%d)", s.cancels);
    HeapFree(GetProcessHeap(), 0, dump);

    /* 5. the writes taken by the callback */
    memset(&s, 0, sizeof(s));
    s.take_io = 1;
    dump = write_dump(&s, MiniDumpNormal, &ret, &err, &len);
    CHECK(ret, "dump through IoWriteAllCallback written (%#lx)", ret ? 0 : err);
    CHECK(s.io_handle_ok, "IoStartCallback is given the file handle");
    CHECK(s.types & MASK(CB_IO_WRITE_ALL), "IoWriteAllCallback gets the writes");
    CHECK(s.io_finish == 1, "IoFinishCallback once (%d)", s.io_finish);
    CHECK(len == 0, "nothing written to the file itself (%lu bytes)", len);
    CHECK(s.io && has_thread(s.io, s.io_size, tid) && has_module(s.io, s.io_size, dbghelp),
          "what IoWriteAllCallback got is the dump (%I64u bytes)", s.io_size);
    HeapFree(GetProcessHeap(), 0, dump);
    HeapFree(GetProcessHeap(), 0, s.io);

    SetEvent(stop);
    WaitForSingleObject(th, 5000);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
