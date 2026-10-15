/* dbghelp 32-bit (patch 3683): MapDebugInformation / UnmapDebugInformation. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef struct
{
    LIST_ENTRY List;
    DWORD Size;
    PVOID MappedBase;
    USHORT Machine, Characteristics;
    DWORD CheckSum, ImageBase, SizeOfImage, NumberOfSections;
    void *Sections;
    DWORD ExportedNamesSize;
    char *ExportedNames;
    DWORD NumFunc;
    void *Func;
    DWORD Low, High, NumFpo;
    void *Fpo;
    DWORD SizeOfCoffSymbols;
    struct { DWORD NumberOfSymbols, LvaToFirstSymbol, NumberOfLinenumbers, LvaToFirstLinenumber, RvaToFirstByteOfCode, RvaToLastByteOfCode, RvaToFirstByteOfData, RvaToLastByteOfData; } *CoffSymbols;
    DWORD SizeOfCv;
    void *Cv;
    char *ImageFilePath, *ImageFileName, *DebugFilePath;
    DWORD TimeDateStamp;
    BOOL RomImage;
    void *DebugDirectory;
    DWORD NumberOfDebugDirectories;
    DWORD Reserved[3];
} DBGINFO;

int main(void)
{
    HMODULE dbg = LoadLibraryA("dbghelp.dll");
    DBGINFO *(WINAPI *map)(HANDLE, const char *, const char *, ULONG);
    BOOL (WINAPI *unmap)(DBGINFO *);
    char path[MAX_PATH];
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)((BYTE *)dos + dos->e_lfanew);
    DBGINFO *info;
    HANDLE file;

    if (!dbg) { check(0, "dbghelp loaded"); return 1; }
    map = (void *)GetProcAddress(dbg, "MapDebugInformation");
    unmap = (void *)GetProcAddress(dbg, "UnmapDebugInformation");
    check(map && unmap, "MapDebugInformation and UnmapDebugInformation are exported");
    if (!map || !unmap) return 1;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    SetLastError(0xdeadbeef);
    info = map(NULL, "C:\\no\\such\\file.exe", NULL, 0);
    check(info == NULL && GetLastError() == ERROR_PATH_NOT_FOUND, "a missing file fails");

    info = map(NULL, path, NULL, 0);
    check(info != NULL, "map by name");
    if (info)
    {
        const char *base = strrchr(path, '\\');
        check(info->ImageBase == nt->OptionalHeader.ImageBase, "ImageBase");
        check(info->SizeOfImage == nt->OptionalHeader.SizeOfImage, "SizeOfImage");
        check(info->TimeDateStamp == nt->FileHeader.TimeDateStamp, "time stamp");
        check(info->Machine == nt->FileHeader.Machine && info->Characteristics == nt->FileHeader.Characteristics, "machine and characteristics");
        check(info->CheckSum == nt->OptionalHeader.CheckSum, "checksum");
        check(info->NumberOfSections == nt->FileHeader.NumberOfSections && info->Sections != NULL, "sections");
        check(info->ImageFilePath && !strcmp(info->ImageFilePath, path), "ImageFilePath is the name given");
        check(info->ImageFileName && base && !strcmp(info->ImageFileName, base + 1), "ImageFileName is the last component");
        check(info->MappedBase && ((IMAGE_DOS_HEADER *)info->MappedBase)->e_magic == IMAGE_DOS_SIGNATURE, "the file is mapped");
        check(info->Size >= 1024, "file size");
        if (nt->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG)
            check(info->NumberOfDebugDirectories == nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size / sizeof(IMAGE_DEBUG_DIRECTORY), "debug directory count");
        if (nt->FileHeader.NumberOfSymbols)
        {
            check(info->CoffSymbols && info->CoffSymbols->NumberOfSymbols == nt->FileHeader.NumberOfSymbols
                    && info->SizeOfCoffSymbols == nt->FileHeader.NumberOfSymbols * 18, "COFF symbols are described");
            check(info->CoffSymbols && info->CoffSymbols->LvaToFirstSymbol == nt->FileHeader.PointerToSymbolTable, "COFF symbol table position");
        }
        else printf("SKIP  the probe has no COFF symbols\n");
        check(unmap(info), "UnmapDebugInformation");
    }

    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    info = map(file, "named.exe", NULL, 0x12340000);
    check(info && info->ImageBase == 0x12340000 && !strcmp(info->ImageFileName, "named.exe"), "map by handle with a given base and name");
    if (info) unmap(info);
    check(GetFileSize(file, NULL) != INVALID_FILE_SIZE, "the caller's handle is left open");
    CloseHandle(file);

    SetLastError(0);
    check(!unmap(NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "Unmap(NULL) fails");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
