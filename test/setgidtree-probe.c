/* A folder a user makes in ProgramData's tree keeps the tree's setgid (6109), so what is made
 * in it later is the users' group's and SYSTEM can replace it, as on Windows (Epic's launcher:
 * its update data made by a user, replaced by its elevated copy, EC-PRM).
 *   probe make DIR     (the user) makes DIR\Update, DIR\Update\Sub and DIR\Update\Sub\data.bin
 *   probe replace DIR  (SYSTEM) writes over DIR\Update\Sub\data.bin and makes DIR\Update\Sub\new.bin */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int write_file(const char *path, DWORD disposition)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    if (h == INVALID_HANDLE_VALUE) return GetLastError();
    WriteFile(h, "data", 4, &written, NULL);
    CloseHandle(h);
    return 0;
}

int main(int argc, char **argv)
{
    char p[MAX_PATH];
    if (argc < 3) return 2;
    if (!strcmp(argv[1], "make"))
    {
        sprintf(p, "%s\\Update", argv[2]);
        printf("MKDIR1 %lu\n", CreateDirectoryA(p, NULL) ? 0 : GetLastError());
        sprintf(p, "%s\\Update\\Sub", argv[2]);
        printf("MKDIR2 %lu\n", CreateDirectoryA(p, NULL) ? 0 : GetLastError());
        sprintf(p, "%s\\Update\\Sub\\data.bin", argv[2]);
        printf("FILE %d\n", write_file(p, CREATE_NEW));
        return 0;
    }
    sprintf(p, "%s\\Update\\Sub\\data.bin", argv[2]);
    printf("REPLACE %d\n", write_file(p, CREATE_ALWAYS));
    sprintf(p, "%s\\Update\\Sub\\new.bin", argv[2]);
    printf("NEW %d\n", write_file(p, CREATE_NEW));
    return 0;
}
