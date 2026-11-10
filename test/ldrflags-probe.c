/* ldrflags gate probe (wine-sg 7725): LdrLoadDll called the Windows way.
 * The first parameter is a search path or, below 64K, LOAD_LIBRARY_SEARCH_*
 * flags (bit 0 a tag); the second points to DllCharacteristics. Programs that
 * hook LoadLibrary call the original like that (Parsec).
 * Needs sgldrflags.dll, sgldrflags2.dll and sgldrflags3.dll next to the probe (built by the
 * gate; they export "attached", set by DllMain); they are not in System32. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

typedef NTSTATUS (WINAPI *ldr_load_fn)(PWSTR, ULONG *, UNICODE_STRING *, HMODULE *);
static ldr_load_fn pLdrLoadDll;
static int failures;

static void check(int ok, const char *what, NTSTATUS st)
{
    printf("%s %s (status %#lx)\n", ok ? "ok  " : "FAIL", what, (unsigned long)st);
    if (!ok) failures++;
}

static NTSTATUS load(ULONG_PTR path, ULONG *chars, const WCHAR *name, HMODULE *mod)
{
    UNICODE_STRING str;
    RtlInitUnicodeString(&str, name);
    *mod = NULL;
    return pLdrLoadDll((PWSTR)path, chars, &str, mod);
}

static ULONG_PTR hook_arg2 = 1;
static ldr_load_fn orig_ldr;

static NTSTATUS WINAPI hook_ldr(PWSTR path, ULONG *chars, UNICODE_STRING *name, HMODULE *mod)
{
    ULONG_PTR v = (ULONG_PTR)chars;
    hook_arg2 = v;
    if (v >> 16) (void)*(volatile ULONG *)chars;  /* a hook reads it, as on Windows */
    return orig_ldr(path, chars, name, mod);
}

/* point kernelbase's import of ntdll!LdrLoadDll at hook_ldr, as hooking programs do */
static int hook_kernelbase(void)
{
    BYTE *base = (BYTE *)GetModuleHandleW(L"kernelbase.dll");
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;

    if (!base) return 0;
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name; imp++)
    {
        IMAGE_THUNK_DATA *names, *iat;
        if (_stricmp((char *)base + imp->Name, "ntdll.dll")) continue;
        names = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        iat = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++)
        {
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            if (strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name, "LdrLoadDll")) continue;
            VirtualProtect(&iat->u1.Function, sizeof(void *), PAGE_READWRITE, &old);
            orig_ldr = (ldr_load_fn)iat->u1.Function;
            iat->u1.Function = (ULONG_PTR)hook_ldr;
            VirtualProtect(&iat->u1.Function, sizeof(void *), old, &old);
            return 1;
        }
    }
    return 0;
}

static int attached(HMODULE mod)
{
    int *p = (int *)GetProcAddress(mod, "attached");
    return p ? *p : -1;
}

static int in_system32(HMODULE mod)
{
    WCHAR path[MAX_PATH], sys[MAX_PATH];
    GetModuleFileNameW(mod, path, MAX_PATH);
    GetSystemDirectoryW(sys, MAX_PATH);
    return !_wcsnicmp(path, sys, wcslen(sys));
}

int main(void)
{
    HMODULE mod;
    NTSTATUS st;
    ULONG chars = 0;
    WCHAR sys[MAX_PATH];

    pLdrLoadDll = (ldr_load_fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrLoadDll");
    if (!pLdrLoadDll) { printf("FAIL no LdrLoadDll\nRESULT: FAIL\n"); return 1; }

    st = load(LOAD_LIBRARY_SEARCH_SYSTEM32, &chars, L"msimg32.dll", &mod);
    check(!st && mod && in_system32(mod), "SEARCH_SYSTEM32 as the path, characteristics pointer: msimg32 from System32", st);

    st = load(LOAD_LIBRARY_SEARCH_SYSTEM32 | 1, NULL, L"mfplat.dll", &mod);
    check(!st && mod && in_system32(mod), "SEARCH_SYSTEM32|1 (tagged), no characteristics: mfplat from System32", st);

    st = load(LOAD_LIBRARY_SEARCH_SYSTEM32, &chars, L"sgldrflags.dll", &mod);
    check(st == STATUS_DLL_NOT_FOUND && !mod, "SEARCH_SYSTEM32 does not look beside the program", st);

    st = load(LOAD_LIBRARY_SEARCH_APPLICATION_DIR, &chars, L"sgldrflags.dll", &mod);
    check(!st && mod && !in_system32(mod), "SEARCH_APPLICATION_DIR finds the DLL beside the program", st);

    GetSystemDirectoryW(sys, MAX_PATH);
    st = load((ULONG_PTR)sys, NULL, L"usp10.dll", &mod);
    check(!st && mod && in_system32(mod), "a string path still works", st);

    st = load(0, &chars, L"version.dll", &mod);
    check(!st && mod, "no path: the default search", st);

    chars = IMAGE_FILE_EXECUTABLE_IMAGE;
    st = load(LOAD_LIBRARY_SEARCH_APPLICATION_DIR, &chars, L"sgldrflags3.dll", &mod);
    check(!st && mod && attached(mod) == 0, "DllCharacteristics IMAGE_FILE_EXECUTABLE_IMAGE: loaded, DllMain not run", st);
    chars = 0;

    if (!hook_kernelbase()) { printf("FAIL kernelbase has no LdrLoadDll import to hook\n"); failures++; }
    else
    {
        mod = LoadLibraryW(L"sgldrflags2.dll");
        check(mod && attached(mod) == 1, "LoadLibrary through a hooked LdrLoadDll loads and runs DllMain", mod ? 0 : GetLastError());
        check((hook_arg2 >> 16) != 0, "kernelbase passes LdrLoadDll a DllCharacteristics pointer", (NTSTATUS)hook_arg2);
        mod = LoadLibraryExW(L"sgldrflags2.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
        check(mod != NULL, "LoadLibraryEx DONT_RESOLVE_DLL_REFERENCES through the hook", mod ? 0 : GetLastError());
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
