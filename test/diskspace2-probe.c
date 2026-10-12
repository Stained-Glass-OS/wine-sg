/* setupapi batch (patches/sg/2054), run by test/diskspace2-gate.sh: the disk
 * space list filled from the sections of an inf (copy and delete sections,
 * an install section with CopyFiles / DelFiles, the "@file" form), removed
 * again, and adjusted by an amount.
 *
 *   diskspace2-probe.exe */
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static LONGLONG required(HDSKSPC list)
{
    LONGLONG space = -12345;
    if (!SetupQuerySpaceRequiredOnDriveA(list, "C:", &space, NULL, 0)) return -99999999;
    return space;
}

int main(void)
{
    static const char inf_text[] =
        "[Version]\r\nSignature=\"$CHICAGO$\"\r\n"
        "[DestinationDirs]\r\nDefaultDestDir=-1,C:\\sgdisk\r\nFiles.Copy=-1,C:\\sgdisk\\sub\r\n"
        "[SourceDisksFiles]\r\na.dat=1,,10000\r\nb_src.dat=1,,3000\r\nc.dat=1,,500\r\n"
        "[DefaultInstall]\r\nCopyFiles=Files.Copy,@c.dat\r\nDelFiles=Files.Del\r\n"
        "[Files.Copy]\r\na.dat\r\nb.dat,b_src.dat\r\n"
        "[Files.Del]\r\nold.dat\r\n";
    char tmp[MAX_PATH], path[MAX_PATH], buf[8192];
    DWORD sectors, bytes, free_clusters, clusters, written, cluster;
    HANDLE file;
    HINF inf;
    HDSKSPC list;
    LONGLONG copy_space, base;

    GetTempPathA(sizeof(tmp), tmp);
    sprintf(path, "%sdiskspace2.inf", tmp);
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, inf_text, strlen(inf_text), &written, NULL);
    CloseHandle(file);

    CreateDirectoryA("C:\\sgdisk", NULL);
    file = CreateFileA("C:\\sgdisk\\old.dat", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    memset(buf, 'x', sizeof(buf));
    WriteFile(file, buf, 5000, &written, NULL);
    CloseHandle(file);

    GetDiskFreeSpaceA("C:\\", &sectors, &bytes, &free_clusters, &clusters);
    cluster = sectors * bytes;
    copy_space = (10000 + cluster - 1) / cluster * cluster + (3000 + cluster - 1) / cluster * cluster;

    inf = SetupOpenInfFileA(path, NULL, INF_STYLE_WIN4, NULL);
    CHECK(inf != INVALID_HANDLE_VALUE);
    list = SetupCreateDiskSpaceListA(NULL, 0, 0);
    CHECK(list != NULL);
    base = required(list);
    CHECK(base == 0);

    /* a copy section: the sizes come from SourceDisksFiles, by source name, in whole clusters */
    CHECK(SetupAddSectionToDiskSpaceListA(list, inf, NULL, "Files.Copy", FILEOP_COPY, NULL, 0));
    CHECK(required(list) == copy_space);
    CHECK(SetupAddSectionToDiskSpaceListA(list, inf, NULL, "Files.Copy", FILEOP_COPY, NULL, 0));
    CHECK(required(list) == copy_space);                       /* again: no more space */
    CHECK(SetupRemoveSectionFromDiskSpaceListA(list, inf, NULL, "Files.Copy", FILEOP_COPY, NULL, 0));
    CHECK(required(list) == 0);

    /* a delete section gives back what the file takes */
    CHECK(SetupAddSectionToDiskSpaceListA(list, inf, NULL, "Files.Del", FILEOP_DELETE, NULL, 0));
    CHECK(required(list) == -((5000 + (LONGLONG)cluster - 1) / cluster * cluster));
    CHECK(SetupRemoveSectionFromDiskSpaceListA(list, inf, NULL, "Files.Del", FILEOP_DELETE, NULL, 0));
    CHECK(required(list) == 0);

    /* an install section: CopyFiles (a section and an "@file") and DelFiles */
    CHECK(SetupAddInstallSectionToDiskSpaceListA(list, inf, NULL, "DefaultInstall", NULL, 0));
    CHECK(required(list) == copy_space + (500 + (LONGLONG)cluster - 1) / cluster * cluster -
                            (5000 + (LONGLONG)cluster - 1) / cluster * cluster);
    CHECK(SetupRemoveInstallSectionFromDiskSpaceListA(list, inf, NULL, "DefaultInstall", NULL, 0));
    CHECK(required(list) == 0);

    /* an amount for the drive */
    CHECK(SetupAdjustDiskSpaceListA(list, "C:", 7777, NULL, 0));
    CHECK(required(list) == 7777);
    CHECK(SetupAdjustDiskSpaceListA(list, "C:\\", -7000, NULL, 0));
    CHECK(required(list) == 777);

    /* the size of a source file, and of a section's files */
    {
        DWORD size = 99;
        CHECK(SetupGetSourceFileSizeA(inf, NULL, "a.dat", NULL, &size, 0) && size == 10000);
        CHECK(SetupGetSourceFileSizeA(inf, NULL, "a.dat", NULL, &size, 4096) && size == 12288);
        CHECK(SetupGetSourceFileSizeA(inf, NULL, "c.dat", NULL, &size, 0) && size == 500);
        CHECK(SetupGetSourceFileSizeA(inf, NULL, "unknown.dat", NULL, &size, 0) && size == 0);
        CHECK(SetupGetSourceFileSizeA(inf, NULL, NULL, "Files.Copy", &size, 0) && size == 13000);
        CHECK(SetupGetSourceFileSizeA(inf, NULL, NULL, "Files.Copy", &size, 4096) && size == 12288 + 4096);
        CHECK(!SetupGetSourceFileSizeA(inf, NULL, NULL, "No.Such.Section", &size, 0));
        SetLastError(0xdeadbeef);
        CHECK(!SetupGetSourceFileSizeA(inf, NULL, "a.dat", NULL, NULL, 0) && GetLastError() == ERROR_INVALID_PARAMETER);
    }

    /* the lists of places to install from */
    {
        PCSTR *entries = NULL;
        UINT count = 0;
        CHECK(SetupAddToSourceListA(SRCLIST_TEMPORARY, "C:\\src one"));
        CHECK(SetupAddToSourceListA(SRCLIST_TEMPORARY, "C:\\src two"));
        CHECK(SetupRemoveFromSourceListA(SRCLIST_TEMPORARY, "c:\\SRC ONE"));
        CHECK(SetupQuerySourceListA(SRCLIST_TEMPORARY, &entries, &count) && count == 1 && !strcmp(entries[0], "C:\\src two"));
        if (entries) SetupFreeSourceListA(&entries, count);
        CHECK(SetupRemoveFromSourceListA(SRCLIST_TEMPORARY, "C:\\not there"));
        SetLastError(0xdeadbeef);
        CHECK(!SetupRemoveFromSourceListA(0, "C:\\src two") && GetLastError() == ERROR_INVALID_PARAMETER);
        CHECK(SetupRemoveFromSourceListA(SRCLIST_TEMPORARY, "C:\\src two"));
    }

    /* directory ids */
    {
        CHECK(SetupSetDirectoryIdExA(inf, 32800, "C:\\sgdisk\\ids", 0, 0, NULL));
        CHECK(SetupSetDirectoryIdExA(inf, 32801, "relative\\dir", SETDIRID_NOT_FULL_PATH, 0, NULL));
        SetLastError(0xdeadbeef);
        CHECK(!SetupSetDirectoryIdExA(inf, 32802, "C:\\x", 0x10, 0, NULL) && GetLastError() == ERROR_INVALID_PARAMETER);
        SetLastError(0xdeadbeef);
        CHECK(!SetupSetDirectoryIdExA(inf, 5, "C:\\x", 0, 0, NULL) && GetLastError() == ERROR_INVALID_PARAMETER);
        CHECK(SetupSetDirectoryIdExA(inf, 0, NULL, 0, 0, NULL));          /* clears them */
    }

    /* errors */
    SetLastError(0xdeadbeef);
    CHECK(!SetupAddSectionToDiskSpaceListA(NULL, inf, NULL, "Files.Copy", FILEOP_COPY, NULL, 0) && GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0xdeadbeef);
    CHECK(!SetupAddSectionToDiskSpaceListA(list, inf, NULL, "Files.Copy", 7, NULL, 0) && GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(!SetupAddSectionToDiskSpaceListA(list, inf, NULL, "No.Such.Section", FILEOP_COPY, NULL, 0));
    CHECK(!SetupAddInstallSectionToDiskSpaceListA(NULL, inf, NULL, "DefaultInstall", NULL, 0));
    SetLastError(0xdeadbeef);
    CHECK(!SetupAdjustDiskSpaceListA(list, "9:", 5, NULL, 0) && GetLastError() == ERROR_INVALID_DRIVE);
    CHECK(!SetupAdjustDiskSpaceListA(NULL, "C:", 5, NULL, 0));
    CHECK(!SetupAdjustDiskSpaceListA(list, NULL, 5, NULL, 0));

    SetupDestroyDiskSpaceList(list);
    SetupCloseInfFile(inf);
    DeleteFileA("C:\\sgdisk\\old.dat");
    RemoveDirectoryA("C:\\sgdisk");
    DeleteFileA(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
