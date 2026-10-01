/* gdi32's GPU scheduling priority class (patches/sg/0633). OBS Studio imports
 * D3DKMTSetProcessSchedulingPriorityClass from gdi32; it was not exported,
 * and OBS aborted as it started its renderer. Prints:
 *   exports=<n of the two found>
 *   set=<status of HIGH for this process> get=<status> class=<n>
 *   invalid=<status of class 6> null=<status of Get without an output>
 *   badhandle=<status for a handle that is no process>
 */
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *set_fn)(HANDLE, int);
typedef LONG (WINAPI *get_fn)(HANDLE, int *);

int main(void)
{
    HMODULE gdi32 = LoadLibraryA("gdi32.dll");
    set_fn pset = (set_fn)GetProcAddress(gdi32, "D3DKMTSetProcessSchedulingPriorityClass");
    get_fn pget = (get_fn)GetProcAddress(gdi32, "D3DKMTGetProcessSchedulingPriorityClass");
    int cls = -1;
    LONG set, get;

    printf("exports=%d\n", !!pset + !!pget);
    if (!pset || !pget) return 1;
    set = pset(GetCurrentProcess(), 4 /* HIGH */);
    get = pget(GetCurrentProcess(), &cls);
    printf("set=%08lx get=%08lx class=%d\n", set, get, cls);
    printf("invalid=%08lx null=%08lx\n", pset(GetCurrentProcess(), 6), pget(GetCurrentProcess(), NULL));
    printf("badhandle=%08lx\n", pset((HANDLE)(ULONG_PTR)0x1234, 2));
    return 0;
}
