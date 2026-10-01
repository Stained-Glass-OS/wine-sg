/* The Start menu and desktop folders a program gets (patches/sg/0617):
 * "name=path" for CSIDL_PROGRAMS, CSIDL_DESKTOPDIRECTORY, CSIDL_STARTUP and
 * FOLDERID_Programs, and the all-users Programs folder; and FOLDERID_
 * UserProgramFiles(Common), where per-user installs go (0618). */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <stdio.h>

int wmain(void)
{
    WCHAR p[MAX_PATH], *k = NULL;
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, p); wprintf(L"programs=%ls\n", p);
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, p); wprintf(L"desktop=%ls\n", p);
    SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, p); wprintf(L"startup=%ls\n", p);
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Programs, 0, NULL, &k))) { wprintf(L"known-programs=%ls\n", k); CoTaskMemFree(k); }
    SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, p); wprintf(L"common-programs=%ls\n", p);
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, p); wprintf(L"appdata=%ls\n", p);
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, p); wprintf(L"localappdata=%ls\n", p);
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_UserProgramFiles, KF_FLAG_CREATE, NULL, &k))) { wprintf(L"user-program-files=%ls\n", k); CoTaskMemFree(k); }
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_UserProgramFilesCommon, KF_FLAG_CREATE, NULL, &k))) { wprintf(L"user-program-files-common=%ls\n", k); CoTaskMemFree(k); }
    return 0;
}
