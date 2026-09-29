/* sortreenter-gate.sh's probe (0513): a registry hook that maps strings while
 * kernelbase looks up a locale's sort -- as App-V's virtual registry does in
 * Click-to-Run Office -- must not recurse. The probe hooks kernelbase's
 * import of ntdll's NtOpenKeyEx; the hook lower-cases a string in the same,
 * not yet used locale before forwarding. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

typedef NTSTATUS (WINAPI *open_fn)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *, ULONG);
static open_fn real_open;
static LONG depth, max_depth, hook_calls;

static NTSTATUS WINAPI hook_open(HANDLE *key, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr, ULONG options)
{
    WCHAR buf[32];
    NTSTATUS status;

    hook_calls++;
    if (++depth > max_depth) max_depth = depth;
    if (depth > 40)   /* it would recurse until the stack ran out */
    {
        depth--;
        return ((NTSTATUS)0xC0000022);
    }
    LCMapStringEx(L"fr-FR", LCMAP_LOWERCASE, L"SORTING", -1, buf, ARRAYSIZE(buf), NULL, NULL, 0);
    status = real_open(key, access, attr, options);
    depth--;
    return status;
}

static void **find_import(HMODULE module, const char *dll, const char *name)
{
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)module + ((IMAGE_DOS_HEADER *)module)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)((BYTE *)module + dir->VirtualAddress);

    for (; imp->Name; imp++)
    {
        IMAGE_THUNK_DATA *names, *funcs;
        if (_stricmp((char *)module + imp->Name, dll)) continue;
        names = (IMAGE_THUNK_DATA *)((BYTE *)module + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        funcs = (IMAGE_THUNK_DATA *)((BYTE *)module + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, funcs++)
        {
            IMAGE_IMPORT_BY_NAME *by_name;
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            by_name = (IMAGE_IMPORT_BY_NAME *)((BYTE *)module + names->u1.AddressOfData);
            if (!strcmp((char *)by_name->Name, name)) return (void **)&funcs->u1.Function;
        }
    }
    return NULL;
}

int main(void)
{
    HMODULE kb = GetModuleHandleA("kernelbase.dll");
    void **slot = find_import(kb, "ntdll.dll", "NtOpenKeyEx");
    WCHAR out[32];
    DWORD old;
    int len;

    printf("import %d\n", slot != NULL);
    if (!slot) return 0;
    real_open = *slot;
    VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old);
    *slot = hook_open;

    /* the first lower-casing in fr-FR looks its sort up */
    len = LCMapStringEx(L"fr-FR", LCMAP_LOWERCASE, L"HELLO", -1, out, ARRAYSIZE(out), NULL, NULL, 0);
    *slot = real_open;
    VirtualProtect(slot, sizeof(*slot), old, &old);

    printf("hooked %d\n", hook_calls > 0);
    printf("maxdepth %ld\n", max_depth);
    printf("result %d %ls\n", len, len ? out : L"-");
    /* and the sort was still found and kept: a second call is plain */
    len = LCMapStringEx(L"fr-FR", LCMAP_UPPERCASE, L"hello", -1, out, ARRAYSIZE(out), NULL, NULL, 0);
    printf("again %d %ls\n", len, len ? out : L"-");
    return 0;
}
