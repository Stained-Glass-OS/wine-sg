/* Probe for patches/sg/5005: HeapCompact/RtlCompactHeap return the size of the
 * largest committed free block (it returned 0), and msvcrt's _heapmin, which
 * relies on it, succeeds. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    HANDLE heap = HeapCreate(0, 0x10000, 0);
    void *blocks[8];
    SIZE_T largest, after;
    int (__cdecl *heapmin)(void) = (void *)GetProcAddress(LoadLibraryA("msvcrt.dll"), "_heapmin");
    unsigned int i;

    for (i = 0; i < 8; i++) blocks[i] = HeapAlloc(heap, 0, 0x400);
    largest = HeapCompact(heap, 0);
    check(largest > 0 && largest < 0x10000, "a new heap has a free block: %Iu", largest);
    for (i = 2; i < 6; i++) HeapFree(heap, 0, blocks[i]);
    after = HeapCompact(heap, 0);
    check(after >= largest, "freeing blocks keeps or grows the largest free block: %Iu -> %Iu", largest, after);
    check(HeapCompact(GetProcessHeap(), 0) > 0, "the process heap has a free block");
    SetLastError(0xdeadbeef);
    check(HeapCompact(heap, HEAP_NO_SERIALIZE) == after, "HEAP_NO_SERIALIZE gives the same answer");
    for (i = 0; i < 8; i++) if (i < 2 || i >= 6) HeapFree(heap, 0, blocks[i]);
    HeapDestroy(heap);
    check(heapmin && heapmin() == 0, "msvcrt _heapmin succeeds");

    /* a heap that cannot grow, filled to the last block: nothing free is 0
     * with no error, not a failure */
    heap = HeapCreate(0, 0x2000, 0x2000);
    {
        static const SIZE_T sizes[] = { 0x200, 0x40, 0x10, 1 };
        void *p;
        int n = 0, s;
        for (s = 0; s < 4; s++) while ((p = HeapAlloc(heap, 0, sizes[s]))) n++;
        check(n > 0, "filled a fixed heap with %d blocks", n);
        SetLastError(0xdeadbeef);
        largest = HeapCompact(heap, 0);
        check(largest == 0 ? GetLastError() == NO_ERROR : largest < 0x2000,
              "a full heap: HeapCompact %Iu, error %lu", largest, GetLastError());
    }
    HeapDestroy(heap);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
