/* Small stubs (patches/sg/1695), run by test/smallstubs3-gate.sh on Xvfb:
 * ntdll's DbgPrompt, propsys's VariantCompare, msvcp140's _Throw_Cpp_error
 * (a std::system_error), ucrtbase's _invoke_watson (a fail-fast, in a child
 * process) and SHGetFileInfo's SHGFI_SELECTED (the icon blended with the
 * highlight). These were "@ stub" exports and a FIXME.
 *
 *   smallstubs3-probe.exe [watson] */
#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static volatile LONG cpp_code;
static volatile char cpp_msg[128];

static LONG CALLBACK veh(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == 0xe06d7363 && ep->ExceptionRecord->NumberParameters >= 3)
    {
        /* the thrown object: a runtime_error (vtable, message, ...) */
        char **object = (char **)ep->ExceptionRecord->ExceptionInformation[1];
        cpp_code = 1;
        if (object && object[1]) lstrcpynA((char *)cpp_msg, object[1], sizeof(cpp_msg));
        ExitThread(0);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI throw_thread(void *arg)
{
    void (__cdecl *throw_cpp_error)(int) = arg;
    throw_cpp_error(0);   /* busy */
    return 1;
}

static void pixel_sum(HICON icon, int *sum)
{
    ICONINFO ii;
    BITMAP bm;
    HDC dc = CreateCompatibleDC(NULL);
    int x, y;

    *sum = 0;
    if (!GetIconInfo(icon, &ii)) return;
    GetObjectW(ii.hbmColor, sizeof(bm), &bm);
    SelectObject(dc, ii.hbmColor);
    for (y = 0; y < bm.bmHeight; y++)
        for (x = 0; x < bm.bmWidth; x++) *sum += GetPixel(dc, x, y) & 0xffffff;
    DeleteDC(dc);
    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
}

int main(int argc, char **argv)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    ULONG (WINAPI *pDbgPrompt)(const char *, char *, ULONG) = (void *)GetProcAddress(ntdll, "DbgPrompt");
    HMODULE propsys = LoadLibraryA("propsys.dll");
    int (WINAPI *pVariantCompare)(const VARIANT *, const VARIANT *) = (void *)GetProcAddress(propsys, "VariantCompare");
    HMODULE msvcp = LoadLibraryA("msvcp140.dll");
    void *throw_cpp = GetProcAddress(msvcp, "?_Throw_Cpp_error@std@@YAXH@Z");
    char response[16] = "x";
    VARIANT a, b;
    HANDLE thread;
    SHFILEINFOW plain = { 0 }, selected = { 0 };
    WCHAR path[MAX_PATH];
    int sum1, sum2;

    if (argc > 1 && !strcmp(argv[1], "watson"))
    {
        HMODULE ucrt = LoadLibraryA("ucrtbase.dll");
        void (__cdecl *invoke_watson)(const WCHAR *, const WCHAR *, const WCHAR *, unsigned int, UINT_PTR) =
            (void *)GetProcAddress(ucrt, "_invoke_watson");
        if (invoke_watson) invoke_watson(NULL, NULL, NULL, 0, 0);
        return 7;
    }

    check(pDbgPrompt && pDbgPrompt("SG prompt> ", response, sizeof(response)) == 0 && !response[0],
          "DbgPrompt: nothing comes back with no kernel debugger");

    check(pVariantCompare != NULL, "VariantCompare is there");
    if (pVariantCompare)
    {
        V_VT(&a) = VT_I4; V_I4(&a) = 3;
        V_VT(&b) = VT_I4; V_I4(&b) = 5;
        check(pVariantCompare(&a, &b) < 0 && pVariantCompare(&b, &a) > 0, "3 < 5");
        V_I4(&b) = 3;
        check(pVariantCompare(&a, &b) == 0, "3 == 3");
        V_VT(&a) = VT_BSTR; V_BSTR(&a) = SysAllocString(L"apple");
        V_VT(&b) = VT_BSTR; V_BSTR(&b) = SysAllocString(L"banana");
        check(pVariantCompare(&a, &b) < 0, "\"apple\" < \"banana\"");
        VariantClear(&a);
        VariantClear(&b);
    }

    check(throw_cpp != NULL, "_Throw_Cpp_error is there");
    if (throw_cpp)
    {
        AddVectoredExceptionHandler(1, veh);
        thread = CreateThread(NULL, 0, throw_thread, throw_cpp, 0, NULL);
        WaitForSingleObject(thread, 5000);
        check(cpp_code == 1 && cpp_msg[0], "_Throw_Cpp_error(0) throws a C++ exception with a message");
        CloseHandle(thread);
    }

    /* _invoke_watson ends a child process as a fail-fast */
    {
        char cmd[MAX_PATH + 16];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 0;

        cmd[0] = '"';
        GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
        strcat(cmd, "\" watson");
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 10000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        check(code == 0xc0000409, "_invoke_watson: the process ends as a fail-fast (STATUS_STACK_BUFFER_OVERRUN)");
        if (code != 0xc0000409) printf("      exit code %#lx\n", code);
    }

    /* SHGFI_SELECTED */
    GetSystemDirectoryW(path, MAX_PATH);
    lstrcatW(path, L"\\notepad.exe");
    SHGetFileInfoW(path, 0, &plain, sizeof(plain), SHGFI_ICON);
    SHGetFileInfoW(path, 0, &selected, sizeof(selected), SHGFI_ICON | SHGFI_SELECTED);
    check(plain.hIcon && selected.hIcon, "two icons");
    pixel_sum(plain.hIcon, &sum1);
    pixel_sum(selected.hIcon, &sum2);
    check(sum1 != sum2, "SHGFI_SELECTED: the icon is drawn selected");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
