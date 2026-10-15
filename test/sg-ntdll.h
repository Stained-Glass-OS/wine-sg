/* Probe drivers must not import ntdll (Wine then loads the driver as an ordinary DLL):
 * sg_ntdll_proc() finds an ntdll export by name through the PEB's module list.
 * 64-bit only (the driver probes are skipped for WoW64 hosts). */
#include <ntddk.h>

#ifdef _WIN64
static void *sg_ntdll_proc(const char *name)
{
    char *peb = (char *)__readgsqword(0x60);
    LIST_ENTRY *head = (LIST_ENTRY *)(*(char **)(peb + 0x18) + 0x10), *e;
    char *base = NULL;
    char *nt, *exp;
    ULONG i, *names, *funcs;
    USHORT *ords;

    for (e = head->Flink; e != head; e = e->Flink)
    {
        UNICODE_STRING *dll = (UNICODE_STRING *)((char *)e + 0x58);
        if (dll->Length == 9 * sizeof(WCHAR) && !_wcsnicmp(dll->Buffer, L"ntdll.dll", 9)) { base = *(char **)((char *)e + 0x30); break; }
    }
    if (!base) return NULL;
    nt = base + *(ULONG *)(base + 0x3c);                      /* e_lfanew */
    exp = base + *(ULONG *)(nt + 0x88);                       /* export directory RVA (PE32+) */
    names = (ULONG *)(base + *(ULONG *)(exp + 0x20));
    ords = (USHORT *)(base + *(ULONG *)(exp + 0x24));
    funcs = (ULONG *)(base + *(ULONG *)(exp + 0x1c));
    for (i = 0; i < *(ULONG *)(exp + 0x18); i++)
        if (!strcmp(base + names[i], name)) return base + funcs[ords[i]];
    return NULL;
}
#else
static void *sg_ntdll_proc(const char *name) { return NULL; }
#endif
