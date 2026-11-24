/* The process and system figures Windows writes in every minidump
 * (patches/sg/9553), 64- and 32-bit: MiscInfoStream as MINIDUMP_MISC_INFO_5
 * (process times, processor power, integrity level, time zone, build strings,
 * extended processor state layout), ProcessVmCountersStream and
 * SystemMemoryInfoStream; SecondaryFlagsCallback's MiniSecondaryWithoutPowerInfo
 * leaves the power figures out. Parsed by hand at the SDK's offsets.
 */
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#include <pshpack4.h>
typedef struct { ULONG ProcessId; HANDLE ProcessHandle; ULONG CallbackType; union { ULONG SecondaryFlags; }; } CB_INPUT;
typedef struct { union { ULONG SecondaryFlags; HRESULT Status; BYTE pad[64]; }; } CB_OUTPUT;
#include <poppack.h>

static BOOL CALLBACK nopower(void *param, CB_INPUT *in, CB_OUTPUT *out)
{
    if (in->CallbackType == 15 /* SecondaryFlagsCallback */) out->SecondaryFlags = 1 /* MiniSecondaryWithoutPowerInfo */;
    return TRUE;
}

static BYTE *dump_of(MINIDUMP_TYPE type, MINIDUMP_CALLBACK_INFORMATION *cbi, DWORD *len)
{
    WCHAR path[MAX_PATH];
    HANDLE file;
    BYTE *data;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"dumpmisc.mdmp");
    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, NULL, NULL, cbi))
    {
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

static ULONG32 u32(const BYTE *p) { ULONG32 v; memcpy(&v, p, 4); return v; }
static ULONG64 u64(const BYTE *p) { ULONG64 v; memcpy(&v, p, 8); return v; }

int main(void)
{
    MINIDUMP_CALLBACK_INFORMATION cbi = { (MINIDUMP_CALLBACK_ROUTINE)nopower, NULL };
    TIME_ZONE_INFORMATION tz;
    FILETIME create, ex, kernel, user;
    ULONG64 ct;
    SYSTEM_INFO si;
    BYTE *dump, *m, *v, *s;
    DWORD len, tzid;
    ULONG size;
    void *big;
    DWORD64 (WINAPI *pGetEnabledXStateFeatures)(void) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetEnabledXStateFeatures");
    HANDLE token;
    char buf[256];
    DWORD rid = 0, l;
    TOKEN_MANDATORY_LABEL *label = (TOKEN_MANDATORY_LABEL *)buf;

    printf("info  %d-bit\n", (int)sizeof(void *) * 8);
    big = VirtualAlloc(NULL, 64 << 20, MEM_COMMIT, PAGE_READWRITE);
    GetSystemInfo(&si);
    tzid = GetTimeZoneInformation(&tz);
    GetProcessTimes(GetCurrentProcess(), &create, &ex, &kernel, &user);
    ct = ((((ULONG64)create.dwHighDateTime << 32) | create.dwLowDateTime) - 116444736000000000ull) / 10000000;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        if (GetTokenInformation(token, TokenIntegrityLevel, buf, sizeof(buf), &l))
            rid = *GetSidSubAuthority(label->Label.Sid, *GetSidSubAuthorityCount(label->Label.Sid) - 1);
        CloseHandle(token);
    }

    dump = dump_of(MiniDumpNormal, NULL, &len);
    CHECK(dump != NULL, "dump written");
    if (!dump) goto done;

    m = stream(dump, len, MiscInfoStream, &size);
    CHECK(m && size == 1364 && u32(m) == 1364, "MiscInfoStream is a MINIDUMP_MISC_INFO_5 (%lu)", m ? size : 0);
    if (m && size >= 1364)
    {
        ULONG32 flags = u32(m + 4);
        WCHAR *bld = (WCHAR *)(m + 232), *dbg = (WCHAR *)(m + 752);

        printf("info  flags %#lx build %.40ls dbg %.40ls\n", flags, bld, dbg);
        CHECK(u32(m + 8) == GetCurrentProcessId() && (flags & 1), "process id");
        CHECK((flags & 2) && u32(m + 12) == (ULONG32)ct, "process create time %lu (GetProcessTimes %I64u)", u32(m + 12), ct);
        CHECK((flags & 4) && u32(m + 24) != 0, "processor power info (max %lu MHz)", u32(m + 24));
        CHECK((flags & 0x10) && u32(m + 44) == rid, "process integrity level %#lx (token %#lx)", u32(m + 44), rid);
        CHECK((flags & 0x40) && u32(m + 56) == tzid && !memcmp(m + 60, &tz, sizeof(tz)), "time zone as GetTimeZoneInformation");
        CHECK((flags & 0x100) && bld[0], "a build string");
        CHECK(!wcsncmp(dbg, L"dbghelp.", 8) && wcschr(dbg, ','), "DbgBldStr names dbghelp and its version");
        if (pGetEnabledXStateFeatures && pGetEnabledXStateFeatures())
            CHECK(u32(m + 832) == 528 && u64(m + 840) == pGetEnabledXStateFeatures() && u32(m + 836) > 0x400 &&
                  u32(m + 848 + 2 * 8 + 4) /* AVX size */ == ((pGetEnabledXStateFeatures() & 4) ? 256 : 0),
                  "XStateData: the enabled features %I64x and their layout (size %lu, context %lu)",
                  u64(m + 840), u32(m + 832), u32(m + 836));
    }

    v = stream(dump, len, ProcessVmCountersStream, &size);
    CHECK(v && size >= 8 && *(USHORT *)v == 2 && (*(USHORT *)(v + 2) & 1), "ProcessVmCountersStream revision 2 with VM counters (%lu)", v ? size : 0);
    if (v && size >= 0x60)
    {
        CHECK(u64(v + 16) > 0, "working set %I64u", u64(v + 16));
        CHECK(u64(v + 0x50) >= (64 << 20), "virtual size %I64u counts a 64 MB allocation", u64(v + 0x50));
    }

    s = stream(dump, len, SystemMemoryInfoStream, &size);
    CHECK(s && size >= 4 && *(USHORT *)s == 1, "SystemMemoryInfoStream revision 1 (%lu)", s ? size : 0);
    if (s && size >= 0x40)
    {
        /* MINIDUMP_SYSTEM_BASIC_INFORMATION at 4 */
        CHECK(u32(s + 8) == si.dwPageSize, "page size %lu", u32(s + 8));
        CHECK(u64(s + 36) == (ULONG_PTR)si.lpMaximumApplicationAddress, "highest user address %I64x", u64(s + 36));
        CHECK(u32(s + 52) == si.dwNumberOfProcessors, "processors %lu", u32(s + 52));
        CHECK(u32(s + 12) > 0, "physical pages %lu", u32(s + 12));
    }
    HeapFree(GetProcessHeap(), 0, dump);

    dump = dump_of(MiniDumpNormal, &cbi, &len);
    m = stream(dump, len, MiscInfoStream, &size);
    CHECK(m && size >= 1364 && !(u32(m + 4) & 4) && !u32(m + 24), "MiniSecondaryWithoutPowerInfo leaves the power info out");
    HeapFree(GetProcessHeap(), 0, dump);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
