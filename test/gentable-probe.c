/* Non-AVL generic tables (patches/sg/2200): RtlInsertElementGenericTable,
 * RtlDeleteElementGenericTable, RtlLookupElementGenericTable,
 * RtlEnumerateGenericTable, RtlEnumerateGenericTableWithoutSplaying,
 * RtlGetElementGenericTable and RtlIsGenericTableEmpty were stubs or always
 * returned NULL/FALSE: a table never held anything. RTL_SPLAY_LINKS is a
 * real splay tree keyed by the caller's compare routine; insertion order is
 * kept separately (InsertOrderList) for the two functions that must not
 * disturb the tree. This probe builds a table of ints, checks lookup,
 * sorted ("with splaying") enumeration, insertion-order enumeration and
 * by-index access, deletes from it, and confirms emptiness at the end. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef enum { GenericLessThan, GenericGreaterThan, GenericEqual } COMPARE_RESULTS;

typedef struct _SPLAY_LINKS
{
    struct _SPLAY_LINKS *Parent, *LeftChild, *RightChild;
} SPLAY_LINKS;

typedef struct _GENERIC_TABLE
{
    SPLAY_LINKS *TableRoot;
    LIST_ENTRY InsertOrderList;
    LIST_ENTRY *OrderedPointer;
    ULONG WhichOrderedElement;
    ULONG NumberGenericTableElements;
    void *CompareRoutine;
    void *AllocateRoutine;
    void *FreeRoutine;
    void *TableContext;
} GENERIC_TABLE;

static COMPARE_RESULTS WINAPI compare_ints( GENERIC_TABLE *table, void *a, void *b )
{
    int x = *(int *)a, y = *(int *)b;
    if (x < y) return GenericLessThan;
    if (x > y) return GenericGreaterThan;
    return GenericEqual;
}

static void * WINAPI alloc_node( GENERIC_TABLE *table, LONG size )
{
    return HeapAlloc( GetProcessHeap(), 0, size );
}

static void WINAPI free_node( GENERIC_TABLE *table, void *buffer )
{
    HeapFree( GetProcessHeap(), 0, buffer );
}

static void (WINAPI *pRtlInitializeGenericTable)(GENERIC_TABLE *, void *, void *, void *, void *);
static void * (WINAPI *pRtlInsertElementGenericTable)(GENERIC_TABLE *, void *, ULONG, BOOLEAN *);
static BOOLEAN (WINAPI *pRtlDeleteElementGenericTable)(GENERIC_TABLE *, void *);
static void * (WINAPI *pRtlLookupElementGenericTable)(GENERIC_TABLE *, void *);
static void * (WINAPI *pRtlEnumerateGenericTable)(GENERIC_TABLE *, BOOLEAN);
static void * (WINAPI *pRtlEnumerateGenericTableWithoutSplaying)(GENERIC_TABLE *, void **);
static void * (WINAPI *pRtlGetElementGenericTable)(GENERIC_TABLE *, ULONG);
static ULONG (WINAPI *pRtlNumberGenericTableElements)(GENERIC_TABLE *);
static BOOLEAN (WINAPI *pRtlIsGenericTableEmpty)(GENERIC_TABLE *);

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    GENERIC_TABLE table;
    int values[] = { 50, 10, 40, 20, 30 };
    int sorted[]  = { 10, 20, 30, 40, 50 };
    int i, v, *p;
    BOOLEAN is_new, b;
    void *prev;

#define RESOLVE(n) p##n = (void *)GetProcAddress(ntdll, #n); \
    if (!p##n) { printf("FAIL  %s missing from ntdll\n", #n); return 1; }
    RESOLVE(RtlInitializeGenericTable);
    RESOLVE(RtlInsertElementGenericTable);
    RESOLVE(RtlDeleteElementGenericTable);
    RESOLVE(RtlLookupElementGenericTable);
    RESOLVE(RtlEnumerateGenericTable);
    RESOLVE(RtlEnumerateGenericTableWithoutSplaying);
    RESOLVE(RtlGetElementGenericTable);
    RESOLVE(RtlNumberGenericTableElements);
    RESOLVE(RtlIsGenericTableEmpty);
#undef RESOLVE

    pRtlInitializeGenericTable( &table, (void *)compare_ints, (void *)alloc_node, (void *)free_node, NULL );
    check( pRtlIsGenericTableEmpty( &table ), "new table is empty" );
    check( pRtlNumberGenericTableElements( &table ) == 0, "new table has 0 elements" );
    check( pRtlEnumerateGenericTable( &table, TRUE ) == NULL, "enumerate of an empty table is NULL" );

    for (i = 0; i < 5; i++)
    {
        p = pRtlInsertElementGenericTable( &table, &values[i], sizeof(int), &is_new );
        check( p != NULL && is_new && *p == values[i], "insert is new and holds the value" );
    }
    check( pRtlNumberGenericTableElements( &table ) == 5, "5 elements after 5 inserts" );
    check( !pRtlIsGenericTableEmpty( &table ), "table is not empty" );

    /* inserting an existing key returns the same element, not a new one */
    v = values[2];
    p = pRtlInsertElementGenericTable( &table, &v, sizeof(int), &is_new );
    check( p != NULL && !is_new, "re-inserting an existing key is not new" );
    check( pRtlNumberGenericTableElements( &table ) == 5, "still 5 elements after a duplicate insert" );

    /* lookup finds an existing key and nothing for a missing one */
    v = 30;
    p = pRtlLookupElementGenericTable( &table, &v );
    check( p != NULL && *p == 30, "lookup finds an existing key" );
    v = 999;
    check( pRtlLookupElementGenericTable( &table, &v ) == NULL, "lookup of a missing key is NULL" );

    /* RtlEnumerateGenericTable walks in sorted (CompareRoutine) order */
    b = TRUE;
    for (i = 0; i < 5; i++)
    {
        p = pRtlEnumerateGenericTable( &table, b );
        b = FALSE;
        check( p != NULL && *p == sorted[i], "sorted enumeration step is in order" );
    }
    check( pRtlEnumerateGenericTable( &table, FALSE ) == NULL, "sorted enumeration ends after the last element" );

    /* RtlEnumerateGenericTableWithoutSplaying and RtlGetElementGenericTable
     * walk in insertion order and must agree with each other */
    prev = NULL;
    for (i = 0; i < 5; i++)
    {
        p = pRtlEnumerateGenericTableWithoutSplaying( &table, &prev );
        check( p != NULL && *p == values[i], "insertion-order enumeration step is in order" );
        check( pRtlGetElementGenericTable( &table, i ) == p, "GetElementGenericTable(i) matches enumeration step i" );
    }
    check( pRtlEnumerateGenericTableWithoutSplaying( &table, &prev ) == NULL,
           "insertion-order enumeration ends after the last element" );
    check( pRtlGetElementGenericTable( &table, 5 ) == NULL, "GetElementGenericTable past the end is NULL" );

    /* deleting the splayed-to-root element (the last one looked up, 30) and
     * a leaf, then confirming what is left */
    v = 30;
    check( pRtlDeleteElementGenericTable( &table, &v ), "delete of the current root succeeds" );
    check( pRtlLookupElementGenericTable( &table, &v ) == NULL, "deleted key is gone" );
    check( pRtlNumberGenericTableElements( &table ) == 4, "4 elements after one delete" );
    v = 999;
    check( !pRtlDeleteElementGenericTable( &table, &v ), "delete of a missing key fails" );

    b = TRUE;
    for (i = 0; i < 5; i++)
    {
        if (sorted[i] == 30) continue;
        p = pRtlEnumerateGenericTable( &table, b );
        b = FALSE;
        check( p != NULL && *p == sorted[i], "sorted enumeration after a delete skips the deleted key" );
    }

    while (!pRtlIsGenericTableEmpty( &table ))
    {
        p = pRtlEnumerateGenericTable( &table, TRUE );
        pRtlDeleteElementGenericTable( &table, p );
    }
    check( pRtlNumberGenericTableElements( &table ) == 0, "table is empty after deleting everything" );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures ? 1 : 0;
}
