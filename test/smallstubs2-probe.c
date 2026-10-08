/* Small stubs that answered wrongly, second set (patches/sg/1642).
 *
 *  - NtQueryInformationFile: FileStatLxInformation (the Linux owner, group
 *    and mode), FileCaseSensitiveInformation and
 *    FileStorageReserveIdInformation were STATUS_NOT_IMPLEMENTED;
 *  - GetLongPathNameW gave \\?\ and UNC paths back unexpanded and did not
 *    check they exist;
 *  - NetGetJoinInformation said "Workgroup", not the workgroup (Samba's, or
 *    WORKGROUP as on Windows);
 *  - CertControlStore(CERT_STORE_CTRL_NOTIFY_CHANGE) never signalled.
 */
#include <windows.h>
#include <winternl.h>
#include <wincrypt.h>
#include <lm.h>
#include <stdio.h>
#include <string.h>

typedef NTSTATUS (WINAPI *query_t)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

struct lx_info
{
    LARGE_INTEGER FileId, CreationTime, LastAccessTime, LastWriteTime, ChangeTime, AllocationSize, EndOfFile;
    ULONG FileAttributes, ReparseTag, NumberOfLinks, EffectiveAccess, LxFlags, LxUid, LxGid, LxMode, LxMajor, LxMinor;
};

int main(int argc, char **argv)
{
    query_t pNtQueryInformationFile = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationFile");
    WCHAR dir[MAX_PATH], longdir[MAX_PATH], shortdir[MAX_PATH], in[MAX_PATH + 8], out[MAX_PATH + 8], file[MAX_PATH];
    struct lx_info lx;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    HANDLE h;
    DWORD len, value;
    LPWSTR name = NULL;
    NETSETUP_JOIN_STATUS type;

    /* file information classes */
    GetTempPathW(MAX_PATH, dir);
    swprintf(longdir, MAX_PATH, L"%lsA Rather Long Directory Name", dir);
    CreateDirectoryW(longdir, NULL);
    swprintf(file, MAX_PATH, L"%ls\\some file.txt", longdir);
    h = CreateFileW(file, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "hello", 5, &len, NULL);
    memset(&lx, 0xcc, sizeof(lx));
    status = pNtQueryInformationFile(h, &io, &lx, sizeof(lx), 70 /* FileStatLxInformation */);
    printf("lx: status %#lx flags %#lx mode %o uid %lu size %lu\n", status, lx.LxFlags, lx.LxMode, lx.LxUid,
           (ULONG)lx.EndOfFile.QuadPart);
    check(!status && io.Information == sizeof(lx) && (lx.LxFlags & 7) == 7 && (lx.LxMode & 0170000) == 0100000 &&
          lx.EndOfFile.QuadPart == 5, "FileStatLxInformation: a regular file's Linux mode, owner and size");
    value = 0xdeadbeef;
    status = pNtQueryInformationFile(h, &io, &value, sizeof(value), 74 /* FileStorageReserveIdInformation */);
    check(!status && value == 0, "FileStorageReserveIdInformation: StorageReserveIdNone");
    CloseHandle(h);
    h = CreateFileW(longdir, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    value = 0xdeadbeef;
    status = pNtQueryInformationFile(h, &io, &value, sizeof(value), 71 /* FileCaseSensitiveInformation */);
    check(!status && value == 0, "FileCaseSensitiveInformation: a directory is not case sensitive");
    memset(&lx, 0xcc, sizeof(lx));
    status = pNtQueryInformationFile(h, &io, &lx, sizeof(lx), 70);
    check(!status && (lx.LxMode & 0170000) == 0040000, "FileStatLxInformation: a directory's mode");
    CloseHandle(h);

    /* GetLongPathNameW on \\?\ paths */
    len = GetShortPathNameW(longdir, shortdir, MAX_PATH);
    printf("short %ls\n", shortdir);
    swprintf(in, ARRAYSIZE(in), L"\\\\?\\%ls", shortdir);
    len = GetLongPathNameW(in, out, ARRAYSIZE(out));
    printf("long of %ls: %lu %ls\n", in, len, out);
    swprintf(in, ARRAYSIZE(in), L"\\\\?\\%ls", longdir);
    check(len && !wcsicmp(out, in), "GetLongPathNameW expands a \\\\?\\ path's short names");
    swprintf(in, ARRAYSIZE(in), L"\\\\?\\%ls\\no such file", longdir);
    SetLastError(0xdeadbeef);
    len = GetLongPathNameW(in, out, ARRAYSIZE(out));
    check(!len && GetLastError() == ERROR_FILE_NOT_FOUND, "... and fails for a file that is not there");
    len = GetLongPathNameW(L"\\\\.\\C:", out, ARRAYSIZE(out));
    check(len == 6 && !wcscmp(out, L"\\\\.\\C:"), "a device path comes back as it is");
    len = GetLongPathNameW(L"\\\\?\\C:\\", out, ARRAYSIZE(out));
    check(len == 7 && !wcscmp(out, L"\\\\?\\C:\\"), "\\\\?\\C:\\ comes back as it is");
    DeleteFileW(file);
    RemoveDirectoryW(longdir);

    /* NetGetJoinInformation */
    if (!NetGetJoinInformation(NULL, &name, &type))
    {
        char expect[64] = "WORKGROUP";
        if (argc > 1) lstrcpynA(expect, argv[1], sizeof(expect));
        printf("join: %ls type %u (want %s)\n", name, type, expect);
        WCHAR expectW[64];
        MultiByteToWideChar(CP_ACP, 0, expect, -1, expectW, 64);
        check(type == NetSetupWorkgroupName && !wcscmp(name, expectW), "NetGetJoinInformation gives the workgroup");
        NetApiBufferFree(name);
    }
    else check(0, "NetGetJoinInformation");

    /* CertControlStore(CERT_STORE_CTRL_NOTIFY_CHANGE) */
    {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGNotifyTest");
        HCERTSTORE other = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGNotifyTest");
        HANDLE event = CreateEventW(NULL, FALSE, FALSE, NULL);
        CERT_NAME_BLOB subject;
        BYTE encoded[256];
        DWORD size = sizeof(encoded);
        PCCERT_CONTEXT cert;
        BOOL ok;

        ok = CertControlStore(store, 0, CERT_STORE_CTRL_NOTIFY_CHANGE, &event);
        check(ok, "CertControlStore(CERT_STORE_CTRL_NOTIFY_CHANGE) succeeds");
        check(WaitForSingleObject(event, 300) == WAIT_TIMEOUT, "... and does not signal before a change");
        CertStrToNameW(X509_ASN_ENCODING, L"CN=SG notify test", CERT_X500_NAME_STR, NULL, encoded, &size, NULL);
        subject.cbData = size;
        subject.pbData = encoded;
        cert = CertCreateSelfSignCertificate(0, &subject, 0, NULL, NULL, NULL, NULL, NULL);
        printf("self-signed: %p (%lu)\n", cert, cert ? 0 : GetLastError());
        if (cert)
        {
            ok = CertAddCertificateContextToStore(other, cert, CERT_STORE_ADD_ALWAYS, NULL);
            /* the registry store writes on commit (or close) */
            ok = ok && CertControlStore(other, 0, CERT_STORE_CTRL_COMMIT, NULL);
            printf("added through another handle: %d\n", ok);
            check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "a certificate added through another handle signals the event");
            ok = CertControlStore(store, 0, CERT_STORE_CTRL_RESYNC, &event);
            check(ok, "RESYNC with the event re-arms it");
            {
                PCCERT_CONTEXT found = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_NAME, &subject, NULL);
                check(found != NULL, "... and the resynced store has the certificate");
                if (found) CertFreeCertificateContext(found);
            }
            {
                PCCERT_CONTEXT found = CertFindCertificateInStore(other, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_NAME, &subject, NULL);
                if (found) CertDeleteCertificateFromStore(found);
                CertControlStore(other, 0, CERT_STORE_CTRL_COMMIT, NULL);
            }
            check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "deleting it signals again");
            CertFreeCertificateContext(cert);
        }
        else check(0, "CertCreateSelfSignCertificate");
        CertCloseStore(other, 0);
        CertCloseStore(store, 0);
        CertUnregisterSystemStore(L"SGNotifyTest", CERT_SYSTEM_STORE_CURRENT_USER);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
