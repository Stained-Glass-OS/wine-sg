/* relocheap probe (patch 5954): where a 64-bit exe's allocations go. An exe without relocations is not
 * ASLR-compatible whatever its DYNAMIC_BASE / HIGH_ENTROPY_VA flags say, and its heap stays low, as on
 * Windows; NSIS's 64-bit installers are such exes and their scripts' 32-bit pointer arithmetic (IntOp)
 * crashed with a heap above 2 GB (foobar2000's uninstaller, qa5 2026-10-22). An exe with relocations
 * keeps its high-entropy (high) allocations.  Prints "heap=0x... global=0x... valloc=0x..." */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    void *h = HeapAlloc(GetProcessHeap(), 0, 64);
    void *g = GlobalAlloc(GMEM_FIXED, 0x4000);
    void *v = VirtualAlloc(NULL, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    printf("heap=%p global=%p valloc=%p\n", h, g, v);
    return 0;
}
