/* kbdsg-probe: a program that loads the keyboard layout DLL as GTK 3.24 does (patches/sg/8703).
 * GetKeyboardLayoutNameA -> HKLM\System\CurrentControlSet\Control\Keyboard Layouts\<KLID>\Layout File
 * -> system directory\<file> -> KbdLayerDescriptor -> KBDTABLES (64-bit pointers in a 32-bit
 * process on a 64-bit system, as GTK's WOW64 code reads them). Prints what it finds. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define KLP(type) type *
#define KP(x) (x)
#else
#define KLP(type) union { type *ptr; UINT64 align; }
#define KP(x) ((x).ptr)
#endif
typedef struct { BYTE Vk; BYTE ModBits; } VK_TO_BIT_;
typedef struct { KLP(VK_TO_BIT_) pVkToBit; WORD wMaxModBits; BYTE ModNumber[1]; } MODIFIERS_;
typedef struct { BYTE VirtualKey; BYTE Attributes; WCHAR wch[1]; } VK_TO_WCHARS_;
typedef struct { KLP(VK_TO_WCHARS_) pVkToWchars; BYTE nModifications; BYTE cbSize; } VK_TO_WCHAR_TABLE_;
typedef struct
{
    KLP(MODIFIERS_) pCharModifiers;
    KLP(VK_TO_WCHAR_TABLE_) pVkToWcharTable;
    KLP(void) pDeadKey;
    KLP(void) pKeyNames;
    KLP(void) pKeyNamesExt;
    KLP(void) pKeyNamesDead;
    KLP(USHORT) pusVSCtoVK;
    BYTE bMaxVSCtoVK;
} KBDTABLES_;

static const VK_TO_WCHARS_ *find( const KBDTABLES_ *t, BYTE vk, int *levels )
{
    const VK_TO_WCHAR_TABLE_ *tab;
    for (tab = KP(t->pVkToWcharTable); KP(tab->pVkToWchars); tab++)
    {
        const BYTE *e = (const BYTE *)KP(tab->pVkToWchars);
        for (; ((const VK_TO_WCHARS_ *)e)->VirtualKey; e += tab->cbSize)
            if (((const VK_TO_WCHARS_ *)e)->VirtualKey == vk) { *levels = tab->nModifications; return (const VK_TO_WCHARS_ *)e; }
    }
    return NULL;
}

int main( void )
{
    char name[KL_NAMELENGTH], key[128], file[MAX_PATH], path[MAX_PATH];
    DWORD size = sizeof(file), type;
    const VK_TO_WCHARS_ *e;
    const KBDTABLES_ *t;
    const MODIFIERS_ *m;
    void *(*desc)(void);
    HMODULE lib;
    HKEY hkey;
    int levels, shift_level;

    GetKeyboardLayoutNameA( name );
    printf( "layout %s\n", name );
    snprintf( key, sizeof(key), "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\%s", name );
    if (RegOpenKeyExA( HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE, &hkey ) ||
        RegQueryValueExA( hkey, "Layout File", NULL, &type, (BYTE *)file, &size ))
    { printf( "FAIL no Layout File for %s\n", name ); return 1; }
    RegCloseKey( hkey );
    printf( "layout file %s\n", file );
    GetSystemDirectoryA( path, MAX_PATH );
    strcat( path, "\\" ); strcat( path, file );
    if (!(lib = LoadLibraryA( path ))) { printf( "FAIL LoadLibrary %s %lu\n", path, GetLastError() ); return 1; }
    if (!(desc = (void *)GetProcAddress( lib, "KbdLayerDescriptor" ))) { printf( "FAIL no KbdLayerDescriptor\n" ); return 1; }
    if (!(t = desc())) { printf( "FAIL KbdLayerDescriptor gave nothing\n" ); return 1; }
    m = KP(t->pCharModifiers);
    shift_level = m->ModNumber[1];
    printf( "maxmodbits %u shiftlevel %d altgrlevel %d bits %u/%u\n", m->wMaxModBits, shift_level,
            m->wMaxModBits >= 6 ? m->ModNumber[6] : -1, KP(m->pVkToBit)[0].Vk, KP(m->pVkToBit)[0].ModBits );
    if ((e = find( t, 'A', &levels ))) printf( "A %04x %04x attr %u levels %d\n", e->wch[0], e->wch[shift_level], e->Attributes, levels );
    else printf( "A missing\n" );
    if ((e = find( t, '1', &levels ))) printf( "1 %04x %04x\n", e->wch[0], e->wch[shift_level] );
    if ((e = find( t, VK_BACK, &levels ))) printf( "BACK %04x\n", e->wch[0] );
    if ((e = find( t, VK_RETURN, &levels ))) printf( "RETURN %04x\n", e->wch[0] );
    if ((e = find( t, 'C', &levels ))) printf( "C ctrl %04x\n", e->wch[m->ModNumber[2]] );
    printf( "vsc1e %04x\n", KP(t->pusVSCtoVK)[0x1e] );
    return 0;
}
