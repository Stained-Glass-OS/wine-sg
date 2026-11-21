/* wine-sg 8413: ShellExecute of a shortcut (.lnk) from a thread in the multithreaded apartment.
 * SG Store calls RoInitialize(RO_INIT_MULTITHREADED); its Open on an app whose Start entry is a
 * shortcut (OpenRA's "Red Alert") started nothing: shell32's handler refused to run there, the
 * .lnk was then executed as a program (error 193, SE_ERR_NOASSOC). The shortcut points back at
 * this probe with "child MARKER"; started = the child wrote its marker. */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static WCHAR self[MAX_PATH], dir[MAX_PATH];

static int run(const WCHAR *tag, DWORD coinit)
{
    WCHAR lnk[MAX_PATH], marker[MAX_PATH], args[MAX_PATH * 2];
    IShellLinkW *link; IPersistFile *file;
    HINSTANCE ret;
    int i, started = 0;

    CoInitializeEx(NULL, coinit);
    swprintf(lnk, MAX_PATH, L"%ls\\%ls.lnk", dir, tag);
    swprintf(marker, MAX_PATH, L"%ls\\%ls.marker", dir, tag);
    swprintf(args, ARRAYSIZE(args), L"child \"%ls\"", marker);
    DeleteFileW(marker);
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link))) return -1;
    link->lpVtbl->SetPath(link, self);
    link->lpVtbl->SetArguments(link, args);
    link->lpVtbl->SetWorkingDirectory(link, dir);
    link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void **)&file);
    file->lpVtbl->Save(file, lnk, TRUE);
    file->lpVtbl->Release(file); link->lpVtbl->Release(link);
    ret = ShellExecuteW(NULL, NULL, lnk, NULL, dir, SW_SHOWNORMAL);
    for (i = 0; i < 100 && !started; i++) { Sleep(100); started = GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES; }
    printf("%ls ret=%d started=%d\n", tag, (int)(INT_PTR)ret, started);
    fflush(stdout);
    CoUninitialize();
    return (INT_PTR)ret > 32 && started;
}

static DWORD WINAPI sta_thread(void *arg) { return run(L"sta", COINIT_APARTMENTTHREADED); }

int wmain(int argc, WCHAR **argv)
{
    HANDLE t; DWORD sta = 0;
    WCHAR *p;
    int mta;

    if (argc >= 3 && !wcscmp(argv[1], L"child"))
    {
        HANDLE h = CreateFileW(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(h);
        return 0;
    }
    GetModuleFileNameW(NULL, self, MAX_PATH);
    lstrcpyW(dir, self);
    if ((p = wcsrchr(dir, '\\'))) *p = 0;
    mta = run(L"mta", COINIT_MULTITHREADED);
    t = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(t, 30000); GetExitCodeThread(t, &sta);
    printf("RESULT: %s\n", mta == 1 && sta == 1 ? "PASS" : "FAIL");
    return 0;
}
