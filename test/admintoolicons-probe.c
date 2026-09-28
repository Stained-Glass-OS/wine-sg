/* admintoolicons-probe: the icons the administrative tools' launchers carry,
 * and whether a .msc file's icon is mmc.exe's */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int main(void)
{
    static const WCHAR *progs[] = { L"mmc.exe", L"eventvwr.exe", L"resmon.exe", L"cleanmgr.exe" };
    WCHAR path[MAX_PATH], sys[MAX_PATH];
    SHFILEINFOW msc = {0}, mmc = {0};
    int i;

    GetSystemDirectoryW(sys, MAX_PATH);
    for (i = 0; i < 4; i++)
    {
        swprintf(path, MAX_PATH, L"%ls\\%ls", sys, progs[i]);
        printf("%ls %u\n", progs[i], ExtractIconExW(path, -1, NULL, NULL, 0));
    }
    swprintf(path, MAX_PATH, L"%ls\\compmgmt.msc", sys);
    SHGetFileInfoW(path, 0, &msc, sizeof(msc), SHGFI_SYSICONINDEX);
    swprintf(path, MAX_PATH, L"%ls\\mmc.exe", sys);
    SHGetFileInfoW(path, 0, &mmc, sizeof(mmc), SHGFI_SYSICONINDEX);
    printf("msc-is-mmc %d\n", msc.iIcon == mmc.iIcon && mmc.iIcon > 0);
    return 0;
}
