/* Probe for patches/sg/1492: combase's RoTransformError, RoTransformErrorW and
 * RoOriginateErrorW return instead of ending the program.  Word called
 * RoTransformError a minute after opening a document (a Windows.Web.Http
 * request answered E_NOTIMPL); the export was an unimplemented stub, so Word
 * ended with exception 0xe0000002.
 *
 * Prints name=1 / name=0 lines; the gate checks them.  A stub ends the
 * probe early, which the gate reports as missing lines.
 *
 *   x86_64-w64-mingw32-gcc -O2 -o rotransformerror-probe.exe rotransformerror-probe.c
 */
#include <windows.h>
#include <stdio.h>

typedef BOOL (WINAPI *transform_fn)(HRESULT, HRESULT, void *);
typedef BOOL (WINAPI *transformw_fn)(HRESULT, HRESULT, UINT, const WCHAR *);
typedef BOOL (WINAPI *originatew_fn)(HRESULT, UINT, const WCHAR *);

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    transform_fn transform = (transform_fn)GetProcAddress(combase, "RoTransformError");
    transformw_fn transformw = (transformw_fn)GetProcAddress(combase, "RoTransformErrorW");
    originatew_fn originatew = (originatew_fn)GetProcAddress(combase, "RoOriginateErrorW");
    BOOL ret;

    printf("exports=%d\n", transform && transformw && originatew ? 1 : 0);
    fflush(stdout);
    if (!transform || !transformw || !originatew) { printf("done=0\n"); return 1; }

    ret = transform(E_NOTIMPL, S_OK, NULL);
    printf("transform=1 %d\n", ret);
    fflush(stdout);
    ret = transformw(E_NOTIMPL, E_FAIL, 5, L"error");
    printf("transformw=1 %d\n", ret);
    fflush(stdout);
    ret = originatew(E_FAIL, 0, L"an error");
    printf("originatew=1 %d\n", ret);
    fflush(stdout);
    printf("done=1\n");
    return 0;
}
