/* test/tryframe-gate.sh probe (wine-sg 6270): user32 leaves no exception frame behind on the thread.
 * An icon of one of our own modules is loaded (LoadImage from shell32, ExtractIconEx on its file: user32
 * checks whether the module is ours inside a __TRY); then the thread's exception list (TEB) must be what
 * it was, and a C++ exception thrown from deep in the stack must still be caught -- with a frame left
 * behind, the dispatcher called a handler from a dead stack slot and the program died (Teams). */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static void *exception_list(void)
{
    return ((NT_TIB *)NtCurrentTeb())->ExceptionList;   /* the TEB starts with its NT_TIB */
}

/* scribbles over the stack the icon loading used */
static int __attribute__((noinline)) deep(int n)
{
    volatile char pad[4096];
    memset((char *)pad, 0x5a, sizeof(pad));
    if (n) return deep(n - 1) + pad[n % 4096];
    throw 42;
}

int main(void)
{
    HMODULE shell32 = LoadLibraryW(L"shell32.dll");
    WCHAR path[MAX_PATH];
    void *before, *after;
    HICON big = NULL, small_icon = NULL, icon;
    int caught = 0, i;

    setvbuf(stdout, NULL, _IONBF, 0);
    GetModuleFileNameW(shell32, path, MAX_PATH);
    before = exception_list();
    for (i = 0; i < 3; i++)
    {
        icon = (HICON)LoadImageW(shell32, MAKEINTRESOURCEW(4), IMAGE_ICON, 32, 32, 0);
        if (icon) DestroyIcon(icon);
    }
    after = exception_list();
    check(before == after, "LoadImage of our own module's icon leaves the exception list as it was");
    ExtractIconExW(path, 3, &big, &small_icon, 1);
    check(big != NULL, "ExtractIconEx gives shell32's folder");
    check(exception_list() == before, "and so does ExtractIconEx");
    try
    {
        deep(40);
    }
    catch (int v)
    {
        caught = v;
    }
    check(caught == 42, "a C++ exception thrown deep in the stack afterwards is caught");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return 0;
}
