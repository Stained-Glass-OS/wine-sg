/* The shell's environment after a change (patches/sg/0749).
 *   envreload-probe.exe set NAME VALUE   HKCU\Environment, then the broadcast
 *   envreload-probe.exe del NAME         removed, then the broadcast
 *   envreload-probe.exe get NAME         prints "NAME=<value in the shell's
 *                                        environment>" or "NAME=(none)": read
 *                                        out of explorer's process (the one
 *                                        with the taskbar)
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <wchar.h>

static void broadcast(void)
{
    DWORD_PTR r;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, &r);
}

static int get(const WCHAR *name)
{
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    DWORD pid = 0;
    HANDLE proc;
    PROCESS_BASIC_INFORMATION pbi;
    BYTE *params = NULL, *envp = NULL;
    static WCHAR env[1 << 16];
    SIZE_T got = 0, total = 0;
    WCHAR *p;
    size_t n = wcslen(name);

    if (!tray) { printf("notray\n"); return 1; }
    GetWindowThreadProcessId(tray, &pid);
    if (!(proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid))) { printf("noopen %lu\n", GetLastError()); return 1; }
    if (NtQueryInformationProcess(proc, ProcessBasicInformation, &pbi, sizeof(pbi), NULL)) return 1;
    ReadProcessMemory(proc, (BYTE *)pbi.PebBaseAddress + FIELD_OFFSET(PEB, ProcessParameters), &params, sizeof(params), NULL);
    /* RTL_USER_PROCESS_PARAMETERS.Environment (x86_64) */
    ReadProcessMemory(proc, params + 0x80, &envp, sizeof(envp), NULL);
    while (total < sizeof(env) - 4096 && ReadProcessMemory(proc, envp + total, (BYTE *)env + total, 4096, &got) && got)
    {
        total += got;
        for (p = env; (BYTE *)(p + 1) < (BYTE *)env + total; p++) if (!p[0] && !p[1]) goto done;
    }
done:
    for (p = env; *p; p += wcslen(p) + 1)
        if (!_wcsnicmp(p, name, n) && p[n] == '=') { printf("%ls\n", p); return 0; }
    printf("%ls=(none)\n", name);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    HKEY k;
    if (argc >= 4 && !wcscmp(argv[1], L"set"))
    {
        RegCreateKeyExW(HKEY_CURRENT_USER, L"Environment", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL);
        RegSetValueExW(k, argv[2], 0, REG_SZ, (const BYTE *)argv[3], (wcslen(argv[3]) + 1) * sizeof(WCHAR));
        RegCloseKey(k);
        broadcast();
        return 0;
    }
    if (argc >= 3 && !wcscmp(argv[1], L"del"))
    {
        if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &k)) { RegDeleteValueW(k, argv[2]); RegCloseKey(k); }
        broadcast();
        return 0;
    }
    if (argc >= 3 && !wcscmp(argv[1], L"get")) return get(argv[2]);
    return 2;
}
