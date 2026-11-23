/* wine-sg 9700: the position of a synchronous file object with no unix fd (a pipe) is kept like Windows'
 * I/O manager keeps CurrentByteOffset: FilePositionInformation queries and sets succeed and the pipe's data
 * is unaffected. Python's tell()/lseek() on a subprocess pipe (calibre, Cura) got "not implemented". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef NTSTATUS (WINAPI *qif_t)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int);
typedef NTSTATUS (WINAPI *sif_t)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int);

int main(void)
{
    HANDLE r, w, dup;
    LARGE_INTEGER pos, d;
    IO_STATUS_BLOCK io;
    LONGLONG q = -1;
    char buf[16];
    DWORD n;
    NTSTATUS st;
    qif_t qif = (qif_t)GetProcAddress(GetModuleHandleA("ntdll"), "NtQueryInformationFile");
    sif_t sif = (sif_t)GetProcAddress(GetModuleHandleA("ntdll"), "NtSetInformationFile");
    int fd;
    __int64 off;

    CHECK(CreatePipe(&r, &w, NULL, 0), "CreatePipe");
    d.QuadPart = 0; pos.QuadPart = -1;
    CHECK(SetFilePointerEx(r, d, &pos, FILE_CURRENT) && pos.QuadPart == 0,
          "SetFilePointerEx(read end, 0, FILE_CURRENT) succeeds at 0 (got %lld, error %lu)", pos.QuadPart, GetLastError());
    CHECK(WriteFile(w, "abc", 3, &n, NULL) && n == 3, "write 3 bytes");
    memset(&io, 0xcc, sizeof(io));
    st = qif(r, &io, &q, sizeof(q), 14 /* FilePositionInformation */);
    CHECK(st == 0 && q == 0 && io.Information == sizeof(q),
          "NtQueryInformationFile(FilePositionInformation) on the read end: status %#lx, offset %lld, info %lu (want 0, 0, 8)",
          st, q, (unsigned long)io.Information);
    d.QuadPart = 100;
    CHECK(SetFilePointerEx(w, d, &pos, FILE_BEGIN) && pos.QuadPart == 100, "SetFilePointerEx(write end, 100, FILE_BEGIN)");
    CHECK(DuplicateHandle(GetCurrentProcess(), w, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS), "duplicate the write end");
    q = -1; st = qif(dup, &io, &q, sizeof(q), 14);
    CHECK(st == 0 && q == 100, "the duplicate shares the file object's position: status %#lx, offset %lld (want 100)", st, q);
    q = 7; st = sif(dup, &io, &q, sizeof(q), 14);
    CHECK(st == 0, "NtSetInformationFile(FilePositionInformation, 7): status %#lx", st);
    q = -1; qif(w, &io, &q, sizeof(q), 14);
    CHECK(q == 7, "the original handle sees 7 (got %lld)", q);
    q = -1; qif(r, &io, &q, sizeof(q), 14);
    CHECK(q == 0, "the read end (its own file object) still at 0 (got %lld)", q);
    st = qif(r, &io, buf, 4, 14);
    CHECK(st == (NTSTATUS)0xC0000004, "short buffer: status %#lx (want STATUS_INFO_LENGTH_MISMATCH)", st);
    memset(buf, 0, sizeof(buf));
    CHECK(ReadFile(r, buf, sizeof(buf), &n, NULL) && n == 3 && !memcmp(buf, "abc", 3), "the pipe's data is unaffected (read %lu: %.3s)", n, buf);
    q = -1; qif(r, &io, &q, sizeof(q), 14);
    CHECK(q == 0, "a read does not move a pipe's position (got %lld)", q);

    /* the C runtime path Python takes: lseek(fd, 0, SEEK_CUR) */
    fd = _open_osfhandle((intptr_t)r, _O_RDONLY);
    off = _lseeki64(fd, 0, SEEK_CUR);
    CHECK(off == 0, "_lseeki64(pipe fd, 0, SEEK_CUR) = %lld (want 0, as on Windows)", off);

    /* an overlapped (asynchronous) pipe handle is not the I/O manager's: unchanged */
    {
        HANDLE s = CreateNamedPipeA("\\\\.\\pipe\\sg-pipepos-probe", PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                    PIPE_TYPE_BYTE, 1, 512, 512, 0, NULL);
        q = 5; st = sif(s, &io, &q, sizeof(q), 14);
        CHECK(st != 0, "set on an overlapped pipe handle still fails: status %#lx", st);
        CloseHandle(s);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
