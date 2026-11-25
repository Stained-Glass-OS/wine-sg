/* shellexkeys-probe: the shell extension handler keys Windows has in every installation, for
 * test/shellexkeys-gate.sh (Stained Glass OS wine-sg 9861). A shell extension's DllRegisterServer
 * opens these keys (it does not create them) to add itself; one missing fails the registration and
 * the installer that runs regsvr32 rolls back. Prints "KEY ok|missing name" for each, then END. */
#include <windows.h>
#include <stdio.h>

static const WCHAR *keys[] =
{
    L"*\\shellex\\ContextMenuHandlers",
    L"*\\shellex\\PropertySheetHandlers",
    L"AllFilesystemObjects\\shellex\\ContextMenuHandlers",
    L"AllFilesystemObjects\\shellex\\PropertySheetHandlers",
    L"Directory\\shellex\\ContextMenuHandlers",
    L"Directory\\shellex\\CopyHookHandlers",
    L"Directory\\shellex\\DragDropHandlers",
    L"Directory\\shellex\\PropertySheetHandlers",
    L"Directory\\Background\\shellex\\ContextMenuHandlers",
    L"Drive\\shellex\\ContextMenuHandlers",
    L"Drive\\shellex\\DragDropHandlers",
    L"Drive\\shellex\\PropertySheetHandlers",
    L"Folder\\shellex\\ContextMenuHandlers",
    L"Folder\\shellex\\DragDropHandlers",
    L"Folder\\shellex\\PropertySheetHandlers",
};

int main(void)
{
    unsigned int i;
    for (i = 0; i < ARRAYSIZE(keys); i++)
    {
        HKEY key;
        LONG r = RegOpenKeyExW(HKEY_CLASSES_ROOT, keys[i], 0, KEY_READ, &key);
        if (!r) RegCloseKey(key);
        printf("KEY %s %ls\n", r ? "missing" : "ok", keys[i]);
    }
    printf("END\n");
    return 0;
}
