/* LockFileEx on files opened for overlapped I/O (patches/sg/1643).
 *
 * A lock that had to wait returned ERROR_IO_PENDING and then never
 * completed ("Async I/O lock wait not implemented, might deadlock"); a lock
 * granted at once posted nothing to the file's completion port ("I/O
 * completion on lock not implemented yet"), and LockFileEx did not use the
 * OVERLAPPED as its status block. Now the lock is an asynchronous request:
 * granted when the conflicting lock goes (unlocked, its handle closed, its
 * process gone), cancelled by CancelIoEx, reported through the event, the
 * OVERLAPPED and the completion port.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static char path[MAX_PATH];

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE open_file(BOOL overlapped)
{
    return CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                       overlapped ? FILE_FLAG_OVERLAPPED : 0, NULL);
}

static BOOL lock(HANDLE h, OVERLAPPED *ov, DWORD offset, DWORD len, DWORD flags)
{
    memset(ov, 0, sizeof(*ov) - sizeof(HANDLE));
    ov->Offset = offset;
    return LockFileEx(h, flags | LOCKFILE_EXCLUSIVE_LOCK, 0, len, 0, ov);
}

static DWORD WINAPI sync_locker(void *arg)
{
    HANDLE h = open_file(FALSE);
    OVERLAPPED ov = { 0 };
    BOOL ok = LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &ov);   /* blocks */
    if (ok) UnlockFile(h, 0, 0, 10, 0);
    CloseHandle(h);
    return ok;
}

int main(int argc, char **argv)
{
    HANDLE h1, h2, h3, h4, port, thread;
    OVERLAPPED ov1, ov2, ov3, ov4, *got_ov;
    DWORD bytes, err;
    ULONG_PTR key;
    BOOL ok;
    char cmd[MAX_PATH * 2];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (argc > 2 && !strcmp(argv[1], "hold"))
    {
        /* another process holds a lock, then ends */
        HANDLE h;
        strcpy(path, argv[2]);
        h = open_file(FALSE);
        LockFile(h, 0, 0, 10, 0);
        Sleep(1500);
        ExitProcess(0);   /* the lock goes with the process */
    }

    GetTempPathA(MAX_PATH, path);
    strcat(path, "sg-asynclock.dat");
    DeleteFileA(path);

    h1 = open_file(TRUE);
    h2 = open_file(TRUE);
    ov1.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ov2.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);

    ok = lock(h1, &ov1, 0, 100, 0);
    check(ok && ov1.Internal == 0, "an uncontended lock is granted at once (OVERLAPPED.Internal 0)");

    SetLastError(0xdeadbeef);
    ok = lock(h2, &ov2, 50, 10, 0);
    err = GetLastError();
    printf("contended: ok %d err %lu internal %#lx\n", ok, err, (DWORD)ov2.Internal);
    check(!ok && err == ERROR_IO_PENDING && ov2.Internal == STATUS_PENDING, "a contended lock is pending (ERROR_IO_PENDING)");
    check(WaitForSingleObject(ov2.hEvent, 300) == WAIT_TIMEOUT, "... and waits while the other lock is held");

    UnlockFile(h1, 0, 0, 100, 0);
    check(WaitForSingleObject(ov2.hEvent, 3000) == WAIT_OBJECT_0, "unlocking grants it: the event is signalled");
    ok = GetOverlappedResult(h2, &ov2, &bytes, FALSE);
    check(ok && ov2.Internal == 0, "... and GetOverlappedResult reports success");

    SetLastError(0xdeadbeef);
    ok = lock(h1, &ov1, 55, 1, LOCKFILE_FAIL_IMMEDIATELY);
    check(!ok && GetLastError() == ERROR_LOCK_VIOLATION, "LOCKFILE_FAIL_IMMEDIATELY fails at once (ERROR_LOCK_VIOLATION)");

    /* a completion port */
    h3 = open_file(TRUE);
    port = CreateIoCompletionPort(h3, NULL, 0x5347, 0);
    memset(&ov3, 0, sizeof(ov3));
    ok = lock(h3, &ov3, 50, 5, 0);
    check(!ok && GetLastError() == ERROR_IO_PENDING, "a lock on a handle with a completion port is pending");
    UnlockFile(h2, 50, 0, 10, 0);
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &got_ov, 3000);
    printf("port: ok %d key %#lx ov %p (want %p)\n", ok, (DWORD)key, got_ov, &ov3);
    check(ok && key == 0x5347 && got_ov == &ov3, "when granted, a packet goes to the completion port");
    UnlockFile(h3, 50, 0, 5, 0);
    ok = lock(h3, &ov3, 200, 5, 0);
    check(ok, "an uncontended lock on it is granted at once");
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &got_ov, 1000);
    check(ok && got_ov == &ov3, "... and posts a packet too, as on Windows");
    UnlockFile(h3, 200, 0, 5, 0);

    /* cancelling */
    lock(h1, &ov1, 0, 10, 0);
    h4 = open_file(TRUE);
    ov4.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok = lock(h4, &ov4, 0, 10, 0);
    check(!ok && GetLastError() == ERROR_IO_PENDING, "another contended lock is pending");
    ok = CancelIoEx(h4, &ov4);
    ok = ok && WaitForSingleObject(ov4.hEvent, 3000) == WAIT_OBJECT_0;
    SetLastError(0xdeadbeef);
    ok = ok && !GetOverlappedResult(h4, &ov4, &bytes, FALSE);
    check(ok && GetLastError() == ERROR_OPERATION_ABORTED, "CancelIoEx cancels it (ERROR_OPERATION_ABORTED)");
    UnlockFile(h1, 0, 0, 10, 0);
    Sleep(300);
    ok = lock(h2, &ov2, 0, 10, LOCKFILE_FAIL_IMMEDIATELY);
    check(ok, "... and a cancelled lock is not granted later");
    UnlockFile(h2, 0, 0, 10, 0);

    /* the holder's handle closed */
    lock(h1, &ov1, 0, 10, 0);
    ResetEvent(ov4.hEvent);
    ok = lock(h4, &ov4, 0, 10, 0);
    CloseHandle(h1);
    check(WaitForSingleObject(ov4.hEvent, 3000) == WAIT_OBJECT_0, "closing the holder's handle grants a waiting lock");
    UnlockFile(h4, 0, 0, 10, 0);

    /* the holder process ends */
    sprintf(cmd, "\"%s\" hold \"%s\"", argv[0], path);
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    Sleep(700);
    ResetEvent(ov4.hEvent);
    ok = lock(h4, &ov4, 0, 10, 0);
    err = GetLastError();
    check(!ok && err == ERROR_IO_PENDING, "a lock another process holds: pending");
    check(WaitForSingleObject(ov4.hEvent, 5000) == WAIT_OBJECT_0, "granted when that process ends");
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    /* a handle without FILE_FLAG_OVERLAPPED still waits in the call */
    thread = CreateThread(NULL, 0, sync_locker, NULL, 0, NULL);
    check(WaitForSingleObject(thread, 500) == WAIT_TIMEOUT, "a synchronous LockFileEx waits in the call");
    UnlockFile(h4, 0, 0, 10, 0);
    check(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0 && GetExitCodeThread(thread, &bytes) && bytes,
          "... and returns once the lock is free");

    CloseHandle(h2);
    CloseHandle(h3);
    CloseHandle(h4);
    CloseHandle(port);
    DeleteFileA(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
