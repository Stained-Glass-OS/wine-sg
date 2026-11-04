/* shlwapi's documented ordinals (6252): SHCreateThreadWithHandle is 615. Wine numbered its named
 * exports from 552, so a program importing 615 by ordinal (OneDrive's FileSyncShell64.dll) called
 * PathIsLFNFileSpecA with its thread procedure, no thread started and the overlay handler waited
 * for it for good.  Also the other ordinals OneDrive and Dropbox import: 12, 16, 176, 219, 487. */
#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } else printf("ok   %s\n", #c); } while (0)

static HANDLE ran;
static LONG called_back;
static DWORD WINAPI proc(void *data) { SetEvent(ran); return 0; }
static DWORD WINAPI callback(void *data) { InterlockedIncrement(&called_back); return 0; }

int main(void)
{
    static const struct { int ord; const char *name; } table[] =
    {
        { 12, "SHCreateMemStream" }, { 16, "SHCreateThread" }, { 176, "IUnknown_QueryService" },
        { 219, "QISearch" }, { 487, "SHLoadIndirectString" }, { 615, "SHCreateThreadWithHandle" },
    };
    HMODULE shlwapi = LoadLibraryA("shlwapi.dll"), shcore = LoadLibraryA("shcore.dll");
    BOOL (WINAPI *create)(LPTHREAD_START_ROUTINE, void *, DWORD, LPTHREAD_START_ROUTINE, HANDLE *);
    HANDLE thread = NULL;
    unsigned int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 0; i < ARRAYSIZE(table); i++)
    {
        FARPROC by_ord = GetProcAddress(shlwapi, MAKEINTRESOURCEA(table[i].ord));
        FARPROC by_name = GetProcAddress(shlwapi, table[i].name);
        FARPROC core = GetProcAddress(shcore, table[i].name);
        if (!by_name && !core) { printf("FAIL %s has no export\n", table[i].name); fails++; continue; }
        if (by_ord != by_name && by_ord != core) { printf("FAIL ordinal %d is not %s\n", table[i].ord, table[i].name); fails++; }
        else printf("ok   ordinal %d is %s\n", table[i].ord, table[i].name);
    }
    create = (void *)GetProcAddress(shlwapi, MAKEINTRESOURCEA(615));
    ran = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(create && create(proc, NULL, 0, callback, &thread));
    CHECK(WaitForSingleObject(ran, 5000) == WAIT_OBJECT_0);
    CHECK(called_back == 1);
    CHECK(thread != NULL && WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
