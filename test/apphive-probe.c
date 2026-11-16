/* application hives are written back as on Windows (wine-sg 7808):
 *   set FILE      load, set a value, exit with the hive open (VsRegEdit)
 *   setkill FILE  load, set a value, wait 4 s, be killed (Visual Studio)
 *   get FILE      load and print the value */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    HKEY hive, key;
    DWORD v = 0, size = sizeof(v);
    LONG ret;

    if (argc < 3) return 2;
    ret = RegLoadAppKeyA(argv[2], &hive, KEY_ALL_ACCESS, 0, 0);
    if (ret) { printf("load=%ld\n", ret); return 1; }
    if (!strcmp(argv[1], "get"))
    {
        ret = RegGetValueA(hive, "Software\\Sg\\Probe", "Value", RRF_RT_REG_DWORD, NULL, &v, &size);
        printf("get=%ld %lu\n", ret, v);
        RegCloseKey(hive);
        return 0;
    }
    v = !strcmp(argv[1], "set") ? 7 : 9;
    if (RegCreateKeyExA(hive, "Software\\Sg\\Probe", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)) return 1;
    RegSetValueExA(key, "Value", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    RegCloseKey(key);
    printf("set=%lu\n", v);
    fflush(stdout);
    if (!strcmp(argv[1], "setkill"))
    {
        Sleep(4000);
        TerminateProcess(GetCurrentProcess(), 3);   /* no DLL detach: only the lazy save can have written it */
    }
    ExitProcess(0);   /* the hive's handle never closed */
}
