/* prefixparse-probe (patches/sg/9653, test/prefixparse-gate.sh): the shell's parsing of Win32
 * names with the \\?\ and \\.\ prefixes. Each name is parsed by SHParseDisplayName on a thread of
 * its own; a parse that has not returned in 20 s (a dialog asking for a name and password, as
 * Transmission's start showed) is reported as "hung". Prints one line per name:
 *   PARSE <tag> <hr> <path of the item, or ->   |   PARSE <tag> hung */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static const WCHAR *names[][2] = {
    { L"drive", L"\\\\?\\C:\\windows\\system32" },
    { L"plain", L"C:\\windows\\system32" },
    { L"device", L"\\\\.\\pipe\\sg-prefixparse" },
    { L"qmark", L"\\\\?\\GLOBALROOT\\Device" },
};
static HRESULT result;
static WCHAR path[MAX_PATH];

static DWORD WINAPI parse(void *arg)
{
    const WCHAR *name = arg;
    PIDLIST_ABSOLUTE pidl = NULL;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    path[0] = 0;
    result = SHParseDisplayName(name, NULL, &pidl, 0, NULL);
    if (SUCCEEDED(result) && !SHGetPathFromIDListW(pidl, path)) lstrcpyW(path, L"(nopath)");
    CoTaskMemFree(pidl);
    CoUninitialize();
    return 0;
}

int main(void)
{
    unsigned int i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        HANDLE t = CreateThread(NULL, 0, parse, (void *)names[i][1], 0, NULL);
        if (WaitForSingleObject(t, 20000) != WAIT_OBJECT_0)
        {
            printf("PARSE %ls hung\n", names[i][0]);
            fflush(stdout);
            ExitProcess(3);
        }
        printf("PARSE %ls %08lx %ls\n", names[i][0], result, path[0] ? path : L"-");
        CloseHandle(t);
    }
    fflush(stdout);
    return 0;
}
