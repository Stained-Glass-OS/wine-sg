/* Probe driver for patches/sg/5090: IoGetConfigurationInformation.
 * - the counts are the numbered device objects of each kind in \Device (Harddisk0.., Serial0..,
 *   CdRom, Floppy, Tape, ScsiPort, Parallel, Changer) that exist, counted here by the driver;
 * - the structure is one object whose counts a driver raises when it creates a device. */
#include "sg-driver.h"

typedef NTSTATUS (NTAPI *open_dir_t)(HANDLE *handle, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr);
typedef NTSTATUS (NTAPI *query_dir_t)(HANDLE dir, void *buffer, ULONG size, BOOLEAN single, BOOLEAN restart, ULONG *context, ULONG *ret);

static void *routine(const WCHAR *name)
{
    UNICODE_STRING str;
    RtlInitUnicodeString(&str, name);
    return MmGetSystemRoutineAddress(&str);
}

static void run_probe(DRIVER_OBJECT *driver)
{
    static const struct { const WCHAR *prefix; size_t offset; const char *name; } kinds[] =
    {
        { L"Harddisk", FIELD_OFFSET(CONFIGURATION_INFORMATION, DiskCount), "disks" },
        { L"Floppy", FIELD_OFFSET(CONFIGURATION_INFORMATION, FloppyCount), "floppies" },
        { L"CdRom", FIELD_OFFSET(CONFIGURATION_INFORMATION, CdRomCount), "cdroms" },
        { L"Tape", FIELD_OFFSET(CONFIGURATION_INFORMATION, TapeCount), "tapes" },
        { L"ScsiPort", FIELD_OFFSET(CONFIGURATION_INFORMATION, ScsiPortCount), "scsi ports" },
        { L"Serial", FIELD_OFFSET(CONFIGURATION_INFORMATION, SerialCount), "serial ports" },
        { L"Parallel", FIELD_OFFSET(CONFIGURATION_INFORMATION, ParallelCount), "parallel ports" },
        { L"Changer", FIELD_OFFSET(CONFIGURATION_INFORMATION, MediumChangerCount), "changers" },
    };
    open_dir_t open_dir = routine(L"ZwOpenDirectoryObject");
    query_dir_t query_dir = routine(L"ZwQueryDirectoryObject");
    CONFIGURATION_INFORMATION *info = IoGetConfigurationInformation(), *again;
    UNICODE_STRING dirname;
    OBJECT_ATTRIBUTES attr;
    ULONG expect[8] = {0}, context = 0, ret, i;
    UCHAR buffer[1024];
    HANDLE dir;

    check(open_dir && query_dir, "the directory routines are exported");
    if (!open_dir || !query_dir) return;
    RtlInitUnicodeString(&dirname, L"\\Device");
    InitializeObjectAttributes(&attr, &dirname, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    check(!open_dir(&dir, 1, &attr), "\\Device opens");
    while (!query_dir(dir, buffer, sizeof(buffer), TRUE, FALSE, &context, &ret))
    {
        UNICODE_STRING *name = (UNICODE_STRING *)buffer;
        for (i = 0; i < 8; i++)
        {
            size_t plen = wcslen(kinds[i].prefix), j, len = name->Length / sizeof(WCHAR);
            if (len <= plen || _wcsnicmp(name->Buffer, kinds[i].prefix, plen)) continue;
            for (j = plen; j < len; j++) if (name->Buffer[j] < '0' || name->Buffer[j] > '9') break;
            if (j == len) expect[i]++;
        }
    }
    ZwClose(dir);

    check(info != NULL, "a structure is returned");
    for (i = 0; i < 8; i++)
    {
        ULONG got = *(ULONG *)((char *)info + kinds[i].offset);
        check(got == expect[i], "%s: %lu (devices %lu)", kinds[i].name, got, expect[i]);
    }
    check(info->DiskCount > 0 && info->SerialCount > 0, "this system has disks and serial ports: %lu %lu", info->DiskCount, info->SerialCount);

    info->TapeCount++;
    again = IoGetConfigurationInformation();
    check(again == info && again->TapeCount == expect[3] + 1, "one structure, a driver's increment stays: %lu", again->TapeCount);
    info->TapeCount--;
}
