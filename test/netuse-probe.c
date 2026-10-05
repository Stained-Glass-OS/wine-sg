/* The connections a user has made, as programs see them (patches/sg/0826,
 * 0838): NetUseEnum's list (use=LOCAL REMOTE) and the names File Explorer
 * shows for the drive letters given as arguments (name[X:]=...). */
#include <windows.h>
#include <lm.h>
#include <shellapi.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    USE_INFO_2 *uses = NULL;
    DWORD read = 0, total = 0, resume = 0, rc, i;
    SHFILEINFOW info;

    rc = NetUseEnum(NULL, 2, (BYTE **)&uses, MAX_PREFERRED_LENGTH, &read, &total, &resume);
    printf("netuseenum=%lu count=%lu\n", rc, read);
    for (i = 0; i < read; i++) printf("use=%ls %ls\n", uses[i].ui2_local, uses[i].ui2_remote);
    if (uses) NetApiBufferFree(uses);
    for (i = 1; i < (DWORD)argc; i++)
    {
        memset(&info, 0, sizeof(info));
        SHGetFileInfoW(argv[i], 0, &info, sizeof(info), SHGFI_DISPLAYNAME);
        printf("name[%ls]=%ls type=%u\n", argv[i], info.szDisplayName, GetDriveTypeW(argv[i]));
    }
    return 0;
}
