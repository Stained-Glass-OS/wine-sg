/* GlobalXxx/LocalXxx on bad pointers and handles (patches/sg/2215): an
 * unreadable address is ERROR_NOACCESS (Local) / ERROR_INVALID_HANDLE
 * (Global flags/lock/handle), GlobalUnlock of a freed or invalid handle sets
 * ERROR_INVALID_HANDLE but answers TRUE, LocalFlags on a bad pointer is
 * LMEM_INVALID_HANDLE.  The expectations are the ones Wine's conformance test
 * kernel32/tests/heap.c records from Windows. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void err(DWORD want, const char *what)
{
    char b[160];
    DWORD got = GetLastError();
    snprintf(b, sizeof(b), "%s (error %lu, want %lu)", what, (unsigned long)got, (unsigned long)want);
    check(got == want, b);
}

int main(void)
{
    void *const invalid_ptr = (void *)(ULONG_PTR)0xdeadbee0;
    const HGLOBAL invalid_mem = (HGLOBAL)(ULONG_PTR)(0xdeadbee0 + sizeof(void *));
    HGLOBAL mem, tmp;
    UINT flags;
    BOOL ret;
    void *ptr;

    /* --- Global, invalid pointer --- */
    SetLastError(0xdeadbeef);
    tmp = GlobalFree(invalid_ptr);
    check(tmp == invalid_ptr, "GlobalFree(invalid pointer) fails");
    err(ERROR_NOACCESS, "...with ERROR_NOACCESS");
    SetLastError(0xdeadbeef);
    flags = GlobalFlags(invalid_ptr);
    check(flags == GMEM_INVALID_HANDLE, "GlobalFlags(invalid pointer) is GMEM_INVALID_HANDLE");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    ptr = GlobalLock(invalid_ptr);
    check(!ptr, "GlobalLock(invalid pointer) fails");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    ret = GlobalUnlock(invalid_ptr);
    check(ret && GetLastError() == 0xdeadbeef, "GlobalUnlock(invalid pointer) is TRUE and sets no error");
    SetLastError(0xdeadbeef);
    tmp = GlobalReAlloc(invalid_ptr, 0, GMEM_MOVEABLE);
    check(!tmp, "GlobalReAlloc(invalid pointer) fails");
    err(ERROR_NOACCESS, "...with ERROR_NOACCESS");
    SetLastError(0xdeadbeef);
    tmp = GlobalHandle(invalid_ptr);
    check(!tmp, "GlobalHandle(invalid pointer) fails");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");

    /* --- Local, invalid pointer --- */
    SetLastError(0xdeadbeef);
    tmp = LocalFree(invalid_ptr);
    check(tmp == invalid_ptr, "LocalFree(invalid pointer) fails");
    err(ERROR_NOACCESS, "...with ERROR_NOACCESS");
    SetLastError(0xdeadbeef);
    flags = LocalFlags(invalid_ptr);
    check(flags == LMEM_INVALID_HANDLE, "LocalFlags(invalid pointer) is LMEM_INVALID_HANDLE");
    err(ERROR_NOACCESS, "...with ERROR_NOACCESS");
    SetLastError(0xdeadbeef);
    tmp = LocalReAlloc(invalid_ptr, 0, LMEM_MOVEABLE);
    check(!tmp, "LocalReAlloc(invalid pointer) fails");
    err(ERROR_NOACCESS, "...with ERROR_NOACCESS");
    SetLastError(0xdeadbeef);
    tmp = LocalHandle(invalid_ptr);
    check(!tmp, "LocalHandle(invalid pointer) fails");
    err(ERROR_NOACCESS, "...with ERROR_NOACCESS");

    /* --- handles: freed and invalid --- */
    mem = GlobalAlloc(GMEM_MOVEABLE, 256);
    GlobalFree(mem);
    SetLastError(0xdeadbeef);
    ret = GlobalUnlock(mem);
    check(ret, "GlobalUnlock(freed handle) is TRUE");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    ret = GlobalUnlock(invalid_mem);
    check(ret, "GlobalUnlock(invalid handle) is TRUE");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    tmp = GlobalFree(mem);
    check(tmp == mem, "GlobalFree(freed handle) fails");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    ret = LocalUnlock(invalid_mem);
    check(!ret, "LocalUnlock(invalid handle) is FALSE (unlike GlobalUnlock)");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    flags = LocalFlags(invalid_mem);
    check(flags == LMEM_INVALID_HANDLE, "LocalFlags(invalid handle) is LMEM_INVALID_HANDLE");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    tmp = LocalReAlloc(invalid_mem, 0, LMEM_MOVEABLE);
    check(!tmp, "LocalReAlloc(invalid handle) fails");
    err(ERROR_INVALID_HANDLE, "...with ERROR_INVALID_HANDLE");

    /* --- ordinary use keeps working --- */
    mem = GlobalAlloc(GMEM_MOVEABLE, 100);
    ptr = GlobalLock(mem);
    check(ptr != NULL, "a movable block locks");
    check(GlobalFlags(mem) == 1, "...with a lock count of 1");
    SetLastError(0xdeadbeef);
    check(!GlobalUnlock(mem) && GetLastError() == NO_ERROR, "...and unlocks (FALSE, NO_ERROR when the count reaches 0)");
    check(GlobalFree(mem) == NULL, "...and frees");
    mem = GlobalAlloc(GMEM_FIXED, 100);
    check(mem && GlobalFlags(mem) == 0, "a fixed block has flags 0");
    check(GlobalLock(mem) == mem, "...locks to itself");
    check(GlobalUnlock(mem), "...and GlobalUnlock says TRUE");
    check(GlobalHandle(mem) == mem, "GlobalHandle of a fixed block is the block");
    check(LocalFlags(mem) == 0 && LocalHandle(mem) == mem, "LocalFlags/LocalHandle of a fixed block");
    check(GlobalFree(mem) == NULL, "...and frees");
    mem = LocalAlloc(LMEM_FIXED, 40);
    SetLastError(0xdeadbeef);
    check(LocalLock(mem) == mem && GetLastError() == 0xdeadbeef, "LocalLock of a fixed block");
    check(LocalFree(mem) == NULL, "...and frees");
    SetLastError(0xdeadbeef);
    check(GlobalLock(NULL) == NULL, "GlobalLock(NULL) fails");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
