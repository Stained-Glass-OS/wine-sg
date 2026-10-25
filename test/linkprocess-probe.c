/* linkprocess-probe.c (wine-sg 6102): ShellExecuteEx on a shortcut gives the process the shortcut started.
 *   linkprocess-probe.exe child          -- sleeps 1.5 s and ends with exit code 7
 *   linkprocess-probe.exe make LNK        -- writes a shortcut LNK to "this exe child"
 *   linkprocess-probe.exe run LNK         -- ShellExecuteEx(LNK) with and without SEE_MASK_NOCLOSEPROCESS
 * KEY=VALUE lines on stdout. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    WCHAR self[MAX_PATH], lnk[MAX_PATH];

    if (argc > 1 && !strcmp(argv[1], "child")) { Sleep(1500); return 7; }
    if (argc < 3) return 2;
    GetModuleFileNameW(NULL, self, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, lnk, MAX_PATH);
    CoInitialize(NULL);
    if (!strcmp(argv[1], "make"))
    {
        IShellLinkW *link;
        IPersistFile *file;
        HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link);
        if (FAILED(hr)) { printf("made=%#lx\n", hr); return 1; }
        IShellLinkW_SetPath(link, self);
        IShellLinkW_SetArguments(link, L"child");
        IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&file);
        hr = IPersistFile_Save(file, lnk, TRUE);
        printf("made=%#lx\n", hr);
        return 0;
    }
    if (!strcmp(argv[1], "run"))
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        DWORD start, code = 0, waited;

        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpFile = lnk;
        sei.nShow = SW_SHOWNORMAL;
        start = GetTickCount();
        printf("executed=%d\n", ShellExecuteExW(&sei));
        printf("process=%d\n", sei.hProcess != NULL);
        if (sei.hProcess)
        {
            WaitForSingleObject(sei.hProcess, 10000);
            waited = GetTickCount() - start;
            GetExitCodeProcess(sei.hProcess, &code);
            printf("exit=%lu\nwaited=%d\n", code, waited >= 1000);
            CloseHandle(sei.hProcess);
        }
        memset(&sei, 0, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        sei.lpFile = lnk;
        /* not asked for: none */
        printf("unasked_executed=%d\nunasked_process=%d\n", ShellExecuteExW(&sei), sei.hProcess != NULL);
        fflush(stdout);
        Sleep(2000);
        printf("done=1\n");
        return 0;
    }
    return 2;
}
