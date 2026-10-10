/* NtQuerySystemInformation SystemDeviceInformation and SystemPageFileInformation (patches/sg/2244). */
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define SystemDeviceInformation 7
#define SystemPageFileInformation 18
#define LENGTH_MISMATCH ((LONG)0xC0000004)

struct dev { ULONG disks, floppies, cdroms, tapes, serial, parallel; };

static int pages, in_use;
static BOOL CALLBACK count_cb(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION info, LPCWSTR name)
{
    (*(int *)ctx)++;
    pages = info->TotalSize;
    in_use = info->TotalInUse;
    return TRUE;
}

int main(void)
{
    LONG (WINAPI *q)(ULONG, void *, ULONG, ULONG *) = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    struct dev dev;
    ULONG len = 0xdead;
    LONG st;
    int count = 0;
    BYTE *buf;

    memset(&dev, 0xcc, sizeof(dev));
    st = q(SystemDeviceInformation, &dev, sizeof(dev), &len);
    CHECK(!st && len == sizeof(dev), "device info st %#lx len %lu", st, len);
    CHECK(dev.disks < 4096 && dev.floppies < 64 && dev.cdroms < 256 && dev.tapes < 256 && dev.serial < 4096 && dev.parallel < 64,
          "plausible counts %lu %lu %lu %lu %lu %lu", dev.disks, dev.floppies, dev.cdroms, dev.tapes, dev.serial, dev.parallel);
    st = q(SystemDeviceInformation, &dev, sizeof(dev) - 1, &len);
    CHECK(st == LENGTH_MISMATCH && len == sizeof(dev), "short buffer st %#lx len %lu", st, len);

    EnumPageFilesW(count_cb, &count);
    len = 0xdead;
    st = q(SystemPageFileInformation, NULL, 0, &len);
    if (!count)
    {
        CHECK(!st && len == 0, "no swap: success and nothing listed (st %#lx len %lu)", st, len);
    }
    else
    {
        struct { ULONG next, total, used, peak; UNICODE_STRING name; } *info;
        CHECK(st == LENGTH_MISMATCH && len > sizeof(*info), "size query st %#lx len %lu", st, len);
        buf = calloc(1, len + 64);
        st = q(SystemPageFileInformation, buf, len, &len);
        info = (void *)buf;
        CHECK(!st, "query st %#lx", st);
        CHECK(info->next == 0, "single entry: next %lu", info->next);
        CHECK((LONG)info->total - pages <= 1 && pages - (LONG)info->total <= 1, "total %lu pages / psapi %d", info->total, pages);
        CHECK(info->used <= info->total && info->peak >= info->used, "used %lu peak %lu total %lu", info->used, info->peak, info->total);
        CHECK(info->name.Length >= 10 && (BYTE *)info->name.Buffer >= buf && (BYTE *)info->name.Buffer + info->name.Length <= buf + len &&
              !_wcsnicmp(info->name.Buffer, L"\\??\\", 4), "name %.*ls", info->name.Length / 2, info->name.Buffer);
        free(buf);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
