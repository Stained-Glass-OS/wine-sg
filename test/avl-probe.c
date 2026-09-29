/* avl-gate.sh's probe (0498, 0499): ntdll's AVL generic tables,
 * RtlIsNameInExpression, and FindFirstFileNameW / FindNextFileNameW.
 * Prints "NAME VALUE" lines. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _RTL_BALANCED_LINKS { struct _RTL_BALANCED_LINKS *p, *l, *r; CHAR b; UCHAR res[3]; } LINKS;
typedef struct _TABLE
{
    LINKS root; void *ordered; ULONG which; ULONG count; ULONG depth; LINKS *restart; ULONG deletes;
    int (WINAPI *compare)(struct _TABLE *, void *, void *);
    void *(WINAPI *alloc)(struct _TABLE *, LONG);
    void (WINAPI *free)(struct _TABLE *, void *);
    void *context;
} TABLE;

void WINAPI RtlInitializeGenericTableAvl(TABLE *, void *, void *, void *, void *);
void *WINAPI RtlInsertElementGenericTableAvl(TABLE *, void *, ULONG, BOOLEAN *);
void *WINAPI RtlLookupElementGenericTableAvl(TABLE *, void *);
BOOLEAN WINAPI RtlDeleteElementGenericTableAvl(TABLE *, void *);
void *WINAPI RtlEnumerateGenericTableAvl(TABLE *, BOOLEAN);
void *WINAPI RtlEnumerateGenericTableWithoutSplayingAvl(TABLE *, void **);
void *WINAPI RtlGetElementGenericTableAvl(TABLE *, ULONG);
ULONG WINAPI RtlNumberGenericTableElementsAvl(TABLE *);
BOOLEAN WINAPI RtlIsGenericTableEmptyAvl(TABLE *);
BOOLEAN WINAPI RtlIsNameInExpression(UNICODE_STRING *, UNICODE_STRING *, BOOLEAN, WCHAR *);
HANDLE WINAPI FindFirstFileNameW(const WCHAR *, DWORD, DWORD *, WCHAR *);
BOOL WINAPI FindNextFileNameW(HANDLE, DWORD *, WCHAR *);

static int allocs, frees;
static int WINAPI cmp(TABLE *t, void *a, void *b) { int x = *(int *)a, y = *(int *)b; return x < y ? 0 : x > y ? 1 : 2; }
static void *WINAPI al(TABLE *t, LONG n) { allocs++; return malloc(n); }
static void WINAPI fr(TABLE *t, void *p) { frees++; free(p); }

static int match(const WCHAR *exp, const WCHAR *name, BOOLEAN ic)
{
    UNICODE_STRING e, n;
    RtlInitUnicodeString(&e, exp); RtlInitUnicodeString(&n, name);
    return RtlIsNameInExpression(&e, &n, ic, NULL);
}

int main(void)
{
    TABLE t;
    BOOLEAN isnew;
    int i, v, *p, prev = -1, ordered = 1, n = 0;
    void *restart = NULL;
    WCHAR path[MAX_PATH], name[MAX_PATH];
    DWORD len;
    HANDLE h;

    RtlInitializeGenericTableAvl(&t, cmp, al, fr, NULL);
    printf("empty %d\n", RtlIsGenericTableEmptyAvl(&t));
    for (i = 0; i < 1000; i++) { v = (i * 7919) % 1000; RtlInsertElementGenericTableAvl(&t, &v, sizeof(v), &isnew); }
    v = 5; p = RtlInsertElementGenericTableAvl(&t, &v, sizeof(v), &isnew);
    printf("again %d %d\n", isnew, p && *p == 5);
    printf("count %lu\n", RtlNumberGenericTableElementsAvl(&t));
    v = 777; p = RtlLookupElementGenericTableAvl(&t, &v); printf("lookup %d\n", p ? *p : -1);
    v = 5000; printf("missing %d\n", RtlLookupElementGenericTableAvl(&t, &v) == NULL);
    for (p = RtlEnumerateGenericTableAvl(&t, TRUE); p; p = RtlEnumerateGenericTableAvl(&t, FALSE)) { if (*p <= prev) ordered = 0; prev = *p; n++; }
    printf("enum %d %d\n", n, ordered);
    n = 0; prev = -1; ordered = 1;
    while ((p = RtlEnumerateGenericTableWithoutSplayingAvl(&t, &restart))) { if (*p <= prev) ordered = 0; prev = *p; n++; }
    printf("nosplay %d %d\n", n, ordered);
    p = RtlGetElementGenericTableAvl(&t, 10); printf("element10 %d\n", p ? *p : -1);
    for (i = 0; i < 1000; i += 2) { v = i; RtlDeleteElementGenericTableAvl(&t, &v); }
    v = 4; printf("deleted %d count %lu\n", RtlLookupElementGenericTableAvl(&t, &v) == NULL, RtlNumberGenericTableElementsAvl(&t));
    v = 4; printf("delete-again %d\n", RtlDeleteElementGenericTableAvl(&t, &v));
    printf("frees %d\n", frees);
    printf("allocs %d\n", allocs);

    printf("m1 %d\n", match(L"*.TXT", L"notes.txt", TRUE));
    printf("m2 %d\n", match(L"*.TXT", L"notes.txt", FALSE));
    printf("m3 %d\n", match(L"A?C", L"ABC", FALSE));
    printf("m4 %d\n", match(L"A?C", L"AC", FALSE));
    printf("m5 %d\n", match(L"\\\\DEVICE\\\\*\\\\OFFICE16\\\\*", L"\\\\DEVICE\\\\HARDDISK\\\\OFFICE16\\\\WINWORD.EXE", FALSE));
    printf("m6 %d\n", match(L"<.DLL", L"A.B.DLL", FALSE));
    printf("m8 %d\n", match(L"NAME\"*", L"NAME", FALSE));
    printf("m9 %d\n", match(L"AB>>", L"AB", FALSE));
    printf("m10 %d\n", match(L"*", L"", FALSE));

    GetWindowsDirectoryW(path, MAX_PATH); lstrcatW(path, L"\\notepad.exe");
    len = 2;
    h = FindFirstFileNameW(path, 0, &len, name);
    i = GetLastError();
    printf("small %d %d %lu\n", h == INVALID_HANDLE_VALUE, i, len);
    len = MAX_PATH;
    h = FindFirstFileNameW(path, 0, &len, name);
    printf("first %d %ls\n", h != INVALID_HANDLE_VALUE, name);
    SetLastError(0);
    i = FindNextFileNameW(h, &len, name);
    printf("next %d %lu\n", i, GetLastError());
    printf("close %d\n", FindClose(h));
    len = MAX_PATH;
    h = FindFirstFileNameW(L"C:\\no\\such\\file", 0, &len, name);
    i = GetLastError();
    printf("nofile %d %d\n", h == INVALID_HANDLE_VALUE, i);
    return 0;
}
