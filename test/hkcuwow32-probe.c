/* HKCU\Software is shared between the 32- and 64-bit views (6110), as on Windows since Windows 7:
 * a 64-bit program writing it with KEY_WOW64_32KEY writes the key a 32-bit program reads, even when
 * an HKCU\Software\Wow6432Node exists. Chromium's setup (Edge WebView2's, user level) opens its
 * EdgeUpdate keys that way and wrote them under HKCU\Software\Wow6432Node; the 32-bit updater then
 * found none ("Installer did not create key", 0x80040905).
 *   probe write   (64-bit) makes HKCU\Software\Wow6432Node, then HKCU\Software\SGTestWow\Clients with KEY_WOW64_32KEY
 *   probe read    (32-bit) reads HKCU\Software\SGTestWow\Clients */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    HKEY key;
    LONG r;
    DWORD value = 0, size = sizeof(value);

    if (argc < 2) return 2;
    if (!strcmp(argv[1], "write"))
    {
        RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Wow6432Node\\SGTestOther", 0, NULL, 0,
                        KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL);
        RegCloseKey(key);
        r = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SGTestWow\\Clients", 0, NULL, 0,
                            KEY_ALL_ACCESS | KEY_WOW64_32KEY, NULL, &key, NULL);
        printf("CREATE %ld\n", r);
        value = 155;
        if (!r) { RegSetValueExA(key, "pv", 0, REG_DWORD, (BYTE *)&value, sizeof(value)); RegCloseKey(key); }
        r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Wow6432Node\\SGTestWow", 0, KEY_READ | KEY_WOW64_64KEY, &key);
        printf("UNDERWOW %ld\n", r);
        if (!r) RegCloseKey(key);
        r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SGTestWow\\Clients", 0, KEY_READ | KEY_WOW64_32KEY, &key);
        printf("REOPEN32 %ld\n", r);
        if (!r) RegCloseKey(key);
        /* HKLM\Software stays redirected */
        r = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\SGTestWowLM", 0, NULL, 0,
                            KEY_ALL_ACCESS | KEY_WOW64_32KEY, NULL, &key, NULL);
        if (!r) RegCloseKey(key);
        r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wow6432Node\\SGTestWowLM", 0, KEY_READ | KEY_WOW64_64KEY, &key);
        printf("HKLMWOW %ld\n", r);
        if (!r) RegCloseKey(key);
        return 0;
    }
    r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SGTestWow\\Clients", 0, KEY_READ, &key);
    if (!r) { r = RegQueryValueExA(key, "pv", NULL, NULL, (BYTE *)&value, &size); RegCloseKey(key); }
    printf("READ %ld %lu\n", r, value);
    return 0;
}
