/* Probe driver for patches/sg/5089: IoCreateFile options.
 * - IO_IGNORE_SHARE_ACCESS_CHECK is accepted and leaves the other opens' share modes in force;
 * - IO_OPEN_TARGET_DIRECTORY opens the file's parent directory;
 * - IO_STOP_ON_SYMLINK on a symbolic link is STATUS_STOPPED_ON_SYMLINK with a REPARSE_DATA_BUFFER
 *   in IoStatus.Information (the caller frees it), on any other file it opens it. */
#include "sg-driver.h"

#define OPT_IGNORE_SHARE 0x0800
#define OPT_TARGET_DIR   0x0004
#define OPT_STOP_SYMLINK 0x0008

static NTSTATUS open_file(const WCHAR *path, ACCESS_MASK access, ULONG share, ULONG create_options, ULONG options, HANDLE *handle, IO_STATUS_BLOCK *io)
{
    UNICODE_STRING name;
    OBJECT_ATTRIBUTES attr;

    RtlInitUnicodeString(&name, path);
    InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    *handle = NULL;
    return IoCreateFile(handle, access, &attr, io, NULL, FILE_ATTRIBUTE_NORMAL, share, FILE_OPEN,
                        create_options | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0, CreateFileTypeNone, NULL, options);
}

static void run_probe(DRIVER_OBJECT *driver)
{
    static const WCHAR regular[] = L"\\??\\C:\\sgio\\regular.txt", link[] = L"\\??\\C:\\sgio\\link.txt";
    HANDLE a, b, dir;
    IO_STATUS_BLOCK io;
    NTSTATUS status;

    status = open_file(regular, GENERIC_READ | SYNCHRONIZE, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE, 0, &a, &io);
    check(!status && a, "plain open: %#lx", status);
    /* IO_IGNORE_SHARE_ACCESS_CHECK only skips the I/O manager's check: the share modes of other
     * opens still apply (the file system checks them) */
    status = open_file(regular, GENERIC_WRITE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_NON_DIRECTORY_FILE, 0, &b, &io);
    check(status == STATUS_SHARING_VIOLATION, "sharing violation while another open denies write sharing: %#lx", status);
    if (!status) ZwClose(b);
    status = open_file(regular, GENERIC_WRITE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_NON_DIRECTORY_FILE, OPT_IGNORE_SHARE, &b, &io);
    check(status == STATUS_SHARING_VIOLATION, "and with IO_IGNORE_SHARE_ACCESS_CHECK: %#lx", status);
    if (!status) ZwClose(b);
    if (a) ZwClose(a);
    status = open_file(regular, GENERIC_READ | SYNCHRONIZE, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE, OPT_IGNORE_SHARE | 1, &a, &io);
    check(!status && a, "the option is accepted (with IO_FORCE_ACCESS_CHECK): %#lx", status);
    if (!status) ZwClose(a);

    /* the parent directory */
    status = open_file(regular, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPT_TARGET_DIR, &dir, &io);
    check(!status && dir, "IO_OPEN_TARGET_DIRECTORY: %#lx", status);
    if (!status)
    {
        FILE_BASIC_INFORMATION basic;
        union { FILE_NAME_INFORMATION info; char buffer[512]; } name;
        ULONG len;

        status = ZwQueryInformationFile(dir, &io, &basic, sizeof(basic), FileBasicInformation);
        check(!status && (basic.FileAttributes & FILE_ATTRIBUTE_DIRECTORY), "the handle is a directory: %#lx %#lx", status, basic.FileAttributes);
        status = ZwQueryInformationFile(dir, &io, &name, sizeof(name), FileNameInformation);
        len = name.info.FileNameLength / sizeof(WCHAR);
        check(!status && len == 5 && !_wcsnicmp(name.info.FileName, L"\\sgio", 5),
              "it is the sgio directory: %#lx %.*ws", status, (int)len, name.info.FileName);
        ZwClose(dir);
    }

    /* a symbolic link */
    status = open_file(link, GENERIC_READ | SYNCHRONIZE, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE, OPT_STOP_SYMLINK, &a, &io);
    check(status == STATUS_STOPPED_ON_SYMLINK && io.Information, "IO_STOP_ON_SYMLINK on a link: %#lx", status);
    if (status == STATUS_STOPPED_ON_SYMLINK && io.Information)
    {
        ULONG tag = *(ULONG *)io.Information;
        check(tag == 0xA000000C, "the reparse tag is a symbolic link: %#lx", tag);
        ExFreePool((void *)io.Information);
    }
    else if (!status && a) ZwClose(a);
    status = open_file(regular, GENERIC_READ | SYNCHRONIZE, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE, OPT_STOP_SYMLINK, &a, &io);
    check(!status && a, "IO_STOP_ON_SYMLINK on a plain file opens it: %#lx", status);
    if (!status) ZwClose(a);
    status = open_file(link, GENERIC_READ | SYNCHRONIZE, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE, 0, &a, &io);
    check(!status && a, "without the option the link is followed: %#lx", status);
    if (!status) ZwClose(a);
}
