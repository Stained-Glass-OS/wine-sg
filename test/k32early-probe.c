/* k32early-probe: kernel32 works as soon as a hook can see it (Stained Glass OS wine-sg 8291;
 * test/k32early-gate.sh).  Firefox's launcher hooks NtMapViewOfSection in its suspended browser process; the
 * hook looks for kernel32 in the PEB's module list and calls its GetModuleHandleW.  Wine mapped kernel32
 * before kernelbase, so when kernelbase was mapped the hook found kernel32 with its imports not yet bound
 * and its GetModuleHandleW (a stub that jumps through its import table to kernelbase) crashed.  The probe
 * does what the launcher does: it starts itself suspended, hooks the child's NtMapViewOfSection with code of
 * this executable, and the child reports what the hook saw.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

typedef NTSTATUS (WINAPI *map_fn)( HANDLE, HANDLE, PVOID *, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, ULONG, ULONG, ULONG );
typedef HMODULE (WINAPI *gmh_fn)( const WCHAR * );

/* written by the parent into the child (same image, same address) */
volatile map_fn g_tramp;
volatile LONG g_hook_calls, g_k32_calls, g_k32_bad;
volatile void *g_k32_result;

static void *find_export( BYTE *base, const char *name )
{
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_EXPORT_DIRECTORY *exp = (IMAGE_EXPORT_DIRECTORY *)(base + nt->OptionalHeader.DataDirectory[0].VirtualAddress);
    DWORD *names = (DWORD *)(base + exp->AddressOfNames), *funcs = (DWORD *)(base + exp->AddressOfFunctions);
    WORD *ords = (WORD *)(base + exp->AddressOfNameOrdinals);
    DWORD i, j;

    for (i = 0; i < exp->NumberOfNames; i++)
    {
        const char *n = (const char *)(base + names[i]);
        for (j = 0; n[j] && n[j] == name[j]; j++) ;
        if (!n[j] && !name[j]) return base + funcs[ords[i]];
    }
    return NULL;
}

/* runs in the child before its imports are bound: no imports, no CRT */
static NTSTATUS WINAPI hook( HANDLE section, HANDLE process, PVOID *addr, ULONG_PTR zero, SIZE_T commit,
                             PLARGE_INTEGER offset, PSIZE_T size, ULONG inherit, ULONG type, ULONG prot )
{
    static const WCHAR k32[] = L"kernel32.dll";
    PEB *peb = (PEB *)__readgsqword( 0x60 );
    LIST_ENTRY *head = &peb->Ldr->InMemoryOrderModuleList, *p;

    g_hook_calls++;
    for (p = head->Flink; p != head; p = p->Flink)
    {
        BYTE *entry = (BYTE *)p - 0x10;                       /* InMemoryOrderLinks */
        UNICODE_STRING *base_name = (UNICODE_STRING *)(entry + 0x58);
        BYTE *dll_base = *(BYTE **)(entry + 0x30);
        unsigned int i, len = base_name->Length / sizeof(WCHAR);

        if (len != 12) continue;
        for (i = 0; i < len; i++)
        {
            WCHAR c = base_name->Buffer[i];
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c != k32[i]) break;
        }
        if (i < len) continue;
        {
            gmh_fn gmh = (gmh_fn)find_export( dll_base, "GetModuleHandleW" );
            void *ret;
            if (!gmh) break;
            ret = gmh( NULL );                                /* what Firefox's launcher does */
            g_k32_calls++;
            if (ret != ((void **)peb)[2]) g_k32_bad++;   /* PEB ImageBaseAddress */
            g_k32_result = ret;
        }
        break;
    }
    return g_tramp( section, process, addr, zero, commit, offset, size, inherit, type, prot );
}

int main( int argc, char **argv )
{
    char cmd[MAX_PATH + 16];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BYTE *stub = (BYTE *)GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "NtMapViewOfSection" );
    BYTE tramp[16 + 14], patch[12];
    void *remote_tramp, *hook_addr = (void *)hook;
    DWORD code = 0, old;
    ULONG_PTR back;

    if (argc > 1)
    {
        PEB *peb = (PEB *)__readgsqword( 0x60 );
        int pos[2][2] = { { -1, -1 }, { -1, -1 } }, l, n;

        /* the module lists keep Windows' order: kernel32 before kernelbase */
        for (l = 0; l < 2; l++)
        {
            LIST_ENTRY *head = l ? &peb->Ldr->InMemoryOrderModuleList : (LIST_ENTRY *)((BYTE *)peb->Ldr + 0x10), *p;
            for (p = head->Flink, n = 0; p != head; p = p->Flink, n++)
            {
                UNICODE_STRING *name = (UNICODE_STRING *)((BYTE *)p - (l ? 0x10 : 0) + 0x58);
                if (name->Length == 24 && !_wcsnicmp( name->Buffer, L"kernel32.dll", 12 )) pos[l][0] = n;
                if (name->Length == 28 && !_wcsnicmp( name->Buffer, L"kernelbase.dll", 14 )) pos[l][1] = n;
            }
        }
        printf( "child hookcalls=%ld k32calls=%ld k32bad=%ld order=%d\n", g_hook_calls, g_k32_calls, g_k32_bad,
                pos[0][0] >= 0 && pos[0][0] < pos[0][1] && pos[1][0] >= 0 && pos[1][0] < pos[1][1] );
        fflush( stdout );
        return 42;
    }
    setvbuf( stdout, NULL, _IONBF, 0 );
    /* the syscall stub as Windows has it: mov r10,rcx; mov eax,id; test byte [7ffe0308],1 */
    if (stub[0] != 0x4c || stub[1] != 0x8b || stub[2] != 0xd1 || stub[3] != 0xb8 || stub[8] != 0xf6)
    {
        printf( "FAIL  NtMapViewOfSection is not a syscall stub\nRESULT: FAIL\n" );
        return 1;
    }
    sprintf( cmd, "\"%s\" child", argv[0] );
    if (!CreateProcessA( NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi ))
    {
        printf( "FAIL  no child (%lu)\nRESULT: FAIL\n", GetLastError() );
        return 1;
    }
    /* the trampoline: the first 16 bytes of the stub, then jmp [rip] back to the stub's 17th byte */
    memcpy( tramp, stub, 16 );
    tramp[16] = 0xff; tramp[17] = 0x25; memset( tramp + 18, 0, 4 );
    back = (ULONG_PTR)stub + 16;
    memcpy( tramp + 22, &back, 8 );
    remote_tramp = VirtualAllocEx( pi.hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
    WriteProcessMemory( pi.hProcess, remote_tramp, tramp, sizeof(tramp), NULL );
    WriteProcessMemory( pi.hProcess, (void *)&g_tramp, &remote_tramp, sizeof(remote_tramp), NULL );
    /* the hook: mov rax, hook; jmp rax over the stub's first 12 bytes */
    patch[0] = 0x48; patch[1] = 0xb8; memcpy( patch + 2, &hook_addr, 8 ); patch[10] = 0xff; patch[11] = 0xe0;
    VirtualProtectEx( pi.hProcess, stub, sizeof(patch), PAGE_EXECUTE_READWRITE, &old );
    WriteProcessMemory( pi.hProcess, stub, patch, sizeof(patch), NULL );
    VirtualProtectEx( pi.hProcess, stub, sizeof(patch), old, &old );
    FlushInstructionCache( pi.hProcess, stub, sizeof(patch) );

    ResumeThread( pi.hThread );
    if (WaitForSingleObject( pi.hProcess, 30000 )) TerminateProcess( pi.hProcess, 1 );
    GetExitCodeProcess( pi.hProcess, &code );
    printf( "%s  the hooked child started and ran its main (exit %#lx)\n", code == 42 ? "PASS" : "FAIL", code );
    printf( "RESULT: %s\n", code == 42 ? "PASS" : "FAIL" );
    return code != 42;
}
