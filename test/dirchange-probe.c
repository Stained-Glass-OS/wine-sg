/* A new folder or file does not raise a second, attribute change notification
 * (patches/sg/2239): the mode a client gives what it created is not a change. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define ALL (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | \
             FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY)

/* collect what is reported for 'ms' milliseconds; returns the number of records and the actions in 'acts' */
static int collect(HANDLE dir, DWORD ms, DWORD *acts, WCHAR names[][32], int max)
{
    BYTE buf[4096];
    OVERLAPPED ov = {0};
    DWORD n, start = GetTickCount();
    int count = 0;

    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    while (GetTickCount() - start < ms && count < max)
    {
        FILE_NOTIFY_INFORMATION *fni = (void *)buf;
        ResetEvent(ov.hEvent);
        if (!ReadDirectoryChangesW(dir, buf, sizeof(buf), FALSE, ALL, NULL, &ov, NULL)) break;
        if (WaitForSingleObject(ov.hEvent, ms - (GetTickCount() - start) > ms ? 0 : ms - (GetTickCount() - start)) != WAIT_OBJECT_0)
        {
            CancelIo(dir);
            GetOverlappedResult(dir, &ov, &n, TRUE);
            break;
        }
        if (!GetOverlappedResult(dir, &ov, &n, FALSE)) break;
        for (;;)
        {
            if (count < max)
            {
                acts[count] = fni->Action;
                memset(names[count], 0, sizeof(names[count]));
                memcpy(names[count], fni->FileName, min(fni->FileNameLength, 30));
                count++;
            }
            if (!fni->NextEntryOffset) break;
            fni = (void *)((BYTE *)fni + fni->NextEntryOffset);
        }
    }
    CloseHandle(ov.hEvent);
    return count;
}

int main(void)
{
    WCHAR base[MAX_PATH], path[MAX_PATH];
    DWORD acts[16];
    WCHAR names[16][32];
    HANDLE dir, file;
    int n, i;

    GetTempPathW(MAX_PATH, base);
    wcscat(base, L"sg-dirchange");
    CreateDirectoryW(base, NULL);
    dir = CreateFileW(base, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    CHECK(dir != INVALID_HANDLE_VALUE, "open watched folder %lu", GetLastError());

    for (i = 0; i < 3; i++)
    {
        /* a new folder: one record */
        wsprintfW(path, L"%s\\dir%d", base, i);
        {
            OVERLAPPED ov = {0};
            BYTE buf[1024];
            DWORD got;
            ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            ReadDirectoryChangesW(dir, buf, sizeof(buf), FALSE, ALL, NULL, &ov, NULL);
            CreateDirectoryW(path, NULL);
            CHECK(WaitForSingleObject(ov.hEvent, 3000) == WAIT_OBJECT_0, "folder notification arrives");
            GetOverlappedResult(dir, &ov, &got, FALSE);
            CloseHandle(ov.hEvent);
        }
        /* nothing more may follow it */
        n = collect(dir, 600, acts, names, 16);
        CHECK(n == 0, "round %d folder: %d extra records, first action %lu name %ls", i, n, n ? acts[0] : 0, n ? names[0] : L"");
    }

    /* a new file, written and closed: no attribute notification beyond Windows' own (writes) -
     * but a plain empty file creates nothing after the ADDED */
    {
        OVERLAPPED ov = {0};
        BYTE buf[1024];
        DWORD got;
        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        ReadDirectoryChangesW(dir, buf, sizeof(buf), FALSE, ALL, NULL, &ov, NULL);
        wsprintfW(path, L"%s\\empty.txt", base);
        file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        CloseHandle(file);
        CHECK(WaitForSingleObject(ov.hEvent, 3000) == WAIT_OBJECT_0, "file notification arrives");
        GetOverlappedResult(dir, &ov, &got, FALSE);
        CloseHandle(ov.hEvent);
    }
    n = collect(dir, 600, acts, names, 16);
    CHECK(n == 0, "empty file: %d extra records, first action %lu", n, n ? acts[0] : 0);

    /* a real attribute change is still reported */
    {
        OVERLAPPED ov = {0};
        BYTE buf[1024];
        DWORD got;
        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        ReadDirectoryChangesW(dir, buf, sizeof(buf), FALSE, ALL, NULL, &ov, NULL);
        SetFileAttributesW(path, FILE_ATTRIBUTE_READONLY);
        CHECK(WaitForSingleObject(ov.hEvent, 3000) == WAIT_OBJECT_0, "a later attribute change is reported");
        GetOverlappedResult(dir, &ov, &got, FALSE);
        CloseHandle(ov.hEvent);
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    }

    CloseHandle(dir);
    for (i = 0; i < 3; i++) { wsprintfW(path, L"%s\\dir%d", base, i); RemoveDirectoryW(path); }
    wsprintfW(path, L"%s\\empty.txt", base);
    DeleteFileW(path);
    RemoveDirectoryW(base);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
