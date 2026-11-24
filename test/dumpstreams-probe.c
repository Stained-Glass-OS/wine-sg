/* Minidump streams a crash reporter asks MiniDumpWriteDump for (patches/sg/9550):
 * MiniDumpWithThreadInfo -> ThreadInfoListStream, MiniDumpWithFullMemoryInfo ->
 * MemoryInfoListStream, MiniDumpWithUnloadedModules -> UnloadedModuleListStream.
 * Written for the probe's own process and for a child process; each stream is
 * parsed by hand from the file (the layouts Windows' dbghelp writes), not
 * through dbghelp's reader.
 *
 *   dumpstreams-probe.exe          parent: runs every check
 *   dumpstreams-probe.exe child E  child: unloads a DLL, starts a worker, sets event E's name, waits
 */
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static DWORD WINAPI worker(void *arg)
{
    WaitForSingleObject((HANDLE)arg, INFINITE);
    return 0;
}

struct unloaded { ULONG64 base; DWORD size, timestamp, checksum; WCHAR name[32]; };

/* load and unload a DLL nothing else has loaded; what the trace should say */
static BOOL unload_one(struct unloaded *u)
{
    static const char *const names[] = { "msimg32.dll", "dciman32.dll", "sfc_os.dll" };
    IMAGE_NT_HEADERS *nt;
    HMODULE mod;
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(names); i++)
    {
        if (GetModuleHandleA(names[i])) continue;
        if (!(mod = LoadLibraryA(names[i]))) continue;
        nt = (IMAGE_NT_HEADERS *)((char *)mod + ((IMAGE_DOS_HEADER *)mod)->e_lfanew);
        u->base = (ULONG_PTR)mod;
        u->size = nt->OptionalHeader.SizeOfImage;
        u->timestamp = nt->FileHeader.TimeDateStamp;
        u->checksum = nt->OptionalHeader.CheckSum;
        MultiByteToWideChar(CP_ACP, 0, names[i], -1, u->name, ARRAY_SIZE(u->name));
        FreeLibrary(mod);
        if (GetModuleHandleA(names[i])) return FALSE;
        return TRUE;
    }
    return FALSE;
}

static int child(const char *evname)
{
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, evname), stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    struct unloaded u;
    HANDLE th;
    BOOL ok = unload_one(&u);
    void *rw = VirtualAlloc(NULL, 0x10000, MEM_COMMIT, PAGE_READWRITE);
    void *res = VirtualAlloc(NULL, 0x100000, MEM_RESERVE, PAGE_NOACCESS);

    th = CreateThread(NULL, 0, worker, stop, 0, NULL);
    /* the parent reads these from the child's stdout */
    printf("%d %I64x %lx %lx %lx %p %p %p %p %ls\n", ok, u.base, u.size, u.timestamp, u.checksum, worker,
           rw, res, GetModuleHandleW(NULL), ok ? u.name : L"-");
    fflush(stdout);
    SetEvent(ready);
    Sleep(60000);
    return th ? 0 : 1;
}

static BYTE *dump_of(HANDLE process, DWORD pid, MINIDUMP_TYPE type, DWORD *len)
{
    WCHAR path[MAX_PATH];
    HANDLE file;
    BYTE *data;
    BOOL ret;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"dumpstreams.mdmp");
    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    ret = MiniDumpWriteDump(process, pid, file, type, NULL, NULL, NULL);
    if (!ret) { printf("info  MiniDumpWriteDump(%#x) failed %#lx\n", type, GetLastError()); CloseHandle(file); return NULL; }
    *len = GetFileSize(file, NULL);
    data = HeapAlloc(GetProcessHeap(), 0, *len);
    SetFilePointer(file, 0, NULL, FILE_BEGIN);
    ReadFile(file, data, *len, len, NULL);
    CloseHandle(file);
    DeleteFileW(path);
    return data;
}

/* the stream's bytes, found in the directory by hand */
static BYTE *stream(BYTE *dump, DWORD len, ULONG type, ULONG *size)
{
    MINIDUMP_HEADER *hdr = (MINIDUMP_HEADER *)dump;
    MINIDUMP_DIRECTORY *dir;
    ULONG i;

    if (len < sizeof(*hdr) || hdr->Signature != 0x504d444d /* MDMP */) return NULL;
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

static ULONG32 u32(const BYTE *p) { ULONG32 v; memcpy(&v, p, 4); return v; }
static ULONG64 u64(const BYTE *p) { ULONG64 v; memcpy(&v, p, 8); return v; }

static void check_thread_info(const char *who, BYTE *dump, DWORD len, DWORD worker_tid, ULONG64 worker_start)
{
    BYTE *s, *tl, *e;
    ULONG size, tsize, n, i, j, nthreads, found = 0, matched = 0;
    FILETIME now;
    ULONG64 now64;

    s = stream(dump, len, ThreadInfoListStream, &size);
    CHECK(s != NULL, "%s: MiniDumpWithThreadInfo writes a ThreadInfoListStream", who);
    if (!s) return;
    CHECK(size >= 12 && u32(s) == 12 && u32(s + 4) == 64, "%s: thread info header 12, entry 64 (%lu %lu)", who,
          size >= 12 ? u32(s) : 0, size >= 12 ? u32(s + 4) : 0);
    n = u32(s + 8);
    CHECK(size == 12 + n * 64, "%s: stream size %lu is header + %lu entries", who, size, n);
    tl = stream(dump, len, ThreadListStream, &tsize);
    nthreads = tl ? u32(tl) : 0;
    CHECK(n == nthreads && n > 0, "%s: an entry for each of the %lu listed threads (%lu)", who, nthreads, n);
    GetSystemTimeAsFileTime(&now);
    now64 = ((ULONG64)now.dwHighDateTime << 32) | now.dwLowDateTime;
    for (i = 0; i < n && 12 + (i + 1) * 64 <= size; i++)
    {
        e = s + 12 + i * 64;
        for (j = 0; j < nthreads; j++)
            if (u32(tl + 4 + j * sizeof(MINIDUMP_THREAD)) == u32(e)) matched++;
        if (u32(e) == worker_tid)
        {
            found = 1;
            CHECK(u64(e + 48) == worker_start, "%s: worker's StartAddress %I64x is its thread routine %I64x",
                  who, u64(e + 48), worker_start);
            CHECK(u64(e + 16) && u64(e + 16) <= now64 && now64 - u64(e + 16) < 600ull * 10000000,
                  "%s: worker's CreateTime is a time of the last minutes (%I64x)", who, u64(e + 16));
            CHECK(u32(e + 12) == STILL_ACTIVE && !u64(e + 24), "%s: worker still active, no ExitTime (%#lx)", who, u32(e + 12));
            CHECK(u64(e + 56) != 0, "%s: worker's Affinity set (%I64x)", who, u64(e + 56));
            CHECK(u32(e + 4) == 0, "%s: worker's DumpFlags 0 (%#lx)", who, u32(e + 4));
        }
    }
    CHECK(found, "%s: the worker thread %lu is in the thread info list", who, worker_tid);
    CHECK(matched == n, "%s: every entry names a thread of the thread list (%lu of %lu)", who, matched, n);
}

static void check_memory_info(const char *who, HANDLE process, BYTE *dump, DWORD len, void *rw, void *reserved, void *image)
{
    BYTE *s, *e;
    ULONG size, i;
    ULONG64 n, prev_end = 0;
    BOOL contiguous = TRUE, seen_rw = FALSE, seen_res = FALSE, seen_img = FALSE, seen_free = FALSE;

    s = stream(dump, len, MemoryInfoListStream, &size);
    CHECK(s != NULL, "%s: MiniDumpWithFullMemoryInfo writes a MemoryInfoListStream", who);
    if (!s) return;
    CHECK(size >= 16 && u32(s) == 16 && u32(s + 4) == 48, "%s: memory info header 16, entry 48 (%lu %lu)", who,
          size >= 16 ? u32(s) : 0, size >= 16 ? u32(s + 4) : 0);
    n = u64(s + 8);
    CHECK(size == 16 + n * 48 && n > 10, "%s: stream size %lu is header + %I64u entries", who, size, n);
    for (i = 0; i < n && 16 + (i + 1) * 48 <= size; i++)
    {
        ULONG64 base, rsize;
        ULONG32 state, protect, type;

        e = s + 16 + i * 48;
        base = u64(e); rsize = u64(e + 24); state = u32(e + 32); protect = u32(e + 36); type = u32(e + 40);
        if (i && base != prev_end) contiguous = FALSE;
        prev_end = base + rsize;
        if (state == MEM_FREE) seen_free = TRUE;
        if ((ULONG_PTR)rw >= base && (ULONG_PTR)rw < base + rsize)
            seen_rw = state == MEM_COMMIT && protect == PAGE_READWRITE && type == MEM_PRIVATE &&
                      u64(e + 8) == (ULONG_PTR)rw && u32(e + 16) == PAGE_READWRITE;
        if ((ULONG_PTR)reserved >= base && (ULONG_PTR)reserved < base + rsize)
            seen_res = state == MEM_RESERVE && type == MEM_PRIVATE;
        if ((ULONG_PTR)image >= base && (ULONG_PTR)image < base + rsize)
            seen_img = state == MEM_COMMIT && type == MEM_IMAGE && u64(e + 8) == (ULONG_PTR)image;
    }
    CHECK(contiguous, "%s: the regions follow each other with no gap", who);
    CHECK(seen_free, "%s: free regions are listed too", who);
    CHECK(seen_rw, "%s: a committed read-write private region, with its allocation base and protection", who);
    CHECK(seen_res, "%s: a reserved region is listed as reserved", who);
    CHECK(seen_img, "%s: the exe's image region is MEM_IMAGE at its base", who);
}

static void check_unloaded(const char *who, BYTE *dump, DWORD len, const struct unloaded *u, const WCHAR *name)
{
    BYTE *s, *e;
    ULONG size, n, i, found = 0;

    s = stream(dump, len, UnloadedModuleListStream, &size);
    CHECK(s != NULL, "%s: MiniDumpWithUnloadedModules writes an UnloadedModuleListStream", who);
    if (!s) return;
    CHECK(size >= 12 && u32(s) == 12 && u32(s + 4) == 24, "%s: unloaded module header 12, entry 24 (%lu %lu)", who,
          size >= 12 ? u32(s) : 0, size >= 12 ? u32(s + 4) : 0);
    n = u32(s + 8);
    CHECK(size == 12 + n * 24, "%s: stream size %lu is header + %lu entries", who, size, n);
    for (i = 0; i < n && 12 + (i + 1) * 24 <= size; i++)
    {
        ULONG rva, slen;

        e = s + 12 + i * 24;
        if (u64(e) != u->base) continue;
        found = 1;
        CHECK(u32(e + 8) == u->size && u32(e + 16) == u->timestamp && u32(e + 12) == u->checksum,
              "%s: unloaded module's size, timestamp and checksum (%lx %lx %lx)", who, u32(e + 8), u32(e + 16), u32(e + 12));
        rva = u32(e + 20);
        slen = rva && rva + 4 <= len ? u32(dump + rva) : 0;
        CHECK(rva && rva + 4 + slen <= len && slen == wcslen(name) * sizeof(WCHAR) &&
              !_wcsnicmp((WCHAR *)(dump + rva + 4), name, slen / sizeof(WCHAR)),
              "%s: unloaded module's name is %ls (%.*ls)", who, name, (int)(slen / 2), slen ? (WCHAR *)(dump + rva + 4) : L"");
    }
    CHECK(found, "%s: the unloaded DLL at %I64x is listed", who, u->base);
}

static void check_absent(const char *who, BYTE *dump, DWORD len)
{
    ULONG size;
    CHECK(!stream(dump, len, ThreadInfoListStream, &size) && !stream(dump, len, MemoryInfoListStream, &size) &&
          !stream(dump, len, UnloadedModuleListStream, &size),
          "%s: a MiniDumpNormal dump has none of the three streams", who);
}

int main(int argc, char **argv)
{
    static const MINIDUMP_TYPE all = MiniDumpWithThreadInfo | MiniDumpWithFullMemoryInfo | MiniDumpWithUnloadedModules;
    struct unloaded u;
    HANDLE stop, th, ready;
    DWORD tid, len;
    BYTE *dump;
    void *rw, *res;
    char cmd[MAX_PATH + 64], line[256], evname[64];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rd, wr;
    DWORD got;
    int ok;
    ULONG64 cstart;
    void *cworker, *crw, *cres, *cimg;

    if (argc == 3 && !strcmp(argv[1], "child")) return child(argv[2]);
    printf("info  %d-bit\n", (int)sizeof(void *) * 8);

    /* this process */
    ok = unload_one(&u);
    CHECK(ok, "self: a DLL was loaded and unloaded (%I64x)", u.base);
    stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, worker, stop, 0, &tid);
    rw = VirtualAlloc(NULL, 0x10000, MEM_COMMIT, PAGE_READWRITE);
    res = VirtualAlloc(NULL, 0x100000, MEM_RESERVE, PAGE_NOACCESS);
    Sleep(50);

    dump = dump_of(GetCurrentProcess(), GetCurrentProcessId(), MiniDumpNormal, &len);
    CHECK(dump != NULL, "self: MiniDumpNormal dump written");
    if (dump) { check_absent("self", dump, len); HeapFree(GetProcessHeap(), 0, dump); }

    dump = dump_of(GetCurrentProcess(), GetCurrentProcessId(), all, &len);
    CHECK(dump != NULL, "self: dump with the three flags written");
    if (dump)
    {
        check_thread_info("self", dump, len, tid, (ULONG_PTR)worker);
        check_memory_info("self", GetCurrentProcess(), dump, len, rw, res, GetModuleHandleW(NULL));
        if (ok) check_unloaded("self", dump, len, &u, u.name);
        HeapFree(GetProcessHeap(), 0, dump);
    }
    SetEvent(stop);
    WaitForSingleObject(th, 5000);

    /* a child process */
    sprintf(evname, "dumpstreams-%lu", GetCurrentProcessId());
    ready = CreateEventA(NULL, TRUE, FALSE, evname);
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    cmd[0] = '"';
    GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
    sprintf(cmd + strlen(cmd), "\" child %s", evname);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CHECK(0, "child: started (%lu)", GetLastError());
    }
    else
    {
        CloseHandle(wr);
        CHECK(WaitForSingleObject(ready, 30000) == WAIT_OBJECT_0, "child: ready");
        memset(line, 0, sizeof(line));
        ReadFile(rd, line, sizeof(line) - 1, &got, NULL);
        ok = 0;
        sscanf(line, "%d %I64x %lx %lx %lx %p %p %p %p %31ls", &ok, &u.base, &u.size, &u.timestamp, &u.checksum,
               &cworker, &crw, &cres, &cimg, u.name);
        cstart = (ULONG_PTR)cworker;
        CHECK(ok, "child: a DLL was loaded and unloaded (%I64x)", u.base);
        Sleep(100);
        dump = dump_of(pi.hProcess, pi.dwProcessId, all, &len);
        CHECK(dump != NULL, "child: dump with the three flags written");
        if (dump)
        {
            BYTE *s, *e;
            ULONG size, i, n, wtid = 0;

            /* the child's worker: the thread info entry whose start is the child's routine */
            if ((s = stream(dump, len, ThreadInfoListStream, &size)) && size >= 12)
                for (n = u32(s + 8), i = 0; i < n && 12 + (i + 1) * 64 <= size; i++)
                {
                    e = s + 12 + i * 64;
                    if (u64(e + 48) == cstart) wtid = u32(e);
                }
            check_thread_info("child", dump, len, wtid, cstart);
            check_memory_info("child", pi.hProcess, dump, len, crw, cres, cimg);
            if (ok) check_unloaded("child", dump, len, &u, u.name);
            HeapFree(GetProcessHeap(), 0, dump);
        }
        TerminateProcess(pi.hProcess, 0);
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
