/* winpaths-probe PATH (patches/sg/9900, test/winpaths-gate.sh): what PATH (a rooted path with
 * forward slashes, as a Windows port of Unix software uses) means to this program:
 *   FULL <GetFullPathName> EXISTS <0|1> */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    char full[MAX_PATH];
    if (argc < 2) return 2;
    if (!GetFullPathNameA(argv[1], sizeof(full), full, NULL)) strcpy(full, "-");
    printf("FULL %s EXISTS %d\n", full, GetFileAttributesA(argv[1]) != INVALID_FILE_ATTRIBUTES);
    fflush(stdout);
    return 0;
}
