/* A user's Software\Classes as on Windows 7 and later (6248): CLSID, DirectShow, Interface,
 * Media Type and MediaFoundation have keys of their own for 32-bit programs (below
 * HKCU\Software\Classes\Wow6432Node), everything else is one key for both views. OneDrive's
 * 32-bit registration of its overlay handlers wrote the i386 FileSyncShell.dll over the 64-bit key.
 *   probe w32 | w64 | r32 | r64 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define G "{5B1D0E11-2B6D-4C11-9A01-0E57A1106248}"
#define OLD "{5B1D0E11-2B6D-4C11-9A01-0E57A11062AA}"

static void put(HKEY root, const char *path, const char *data, REGSAM extra)
{
    HKEY key;
    LONG r = RegCreateKeyExA(root, path, 0, NULL, 0, KEY_ALL_ACCESS | extra, NULL, &key, NULL);
    if (!r) { RegSetValueExA(key, NULL, 0, REG_SZ, (const BYTE *)data, strlen(data) + 1); RegCloseKey(key); }
    printf("PUT %s %ld\n", path, r);
}

static void get(const char *tag, HKEY root, const char *path, REGSAM extra)
{
    char buf[128] = "";
    DWORD size = sizeof(buf);
    HKEY key;
    LONG r = RegOpenKeyExA(root, path, 0, KEY_READ | extra, &key);
    if (!r) { r = RegQueryValueExA(key, NULL, NULL, NULL, (BYTE *)buf, &size); RegCloseKey(key); }
    printf("%s %s\n", tag, r ? "none" : buf);
}

static int has(HKEY root, const char *path, REGSAM extra)
{
    HKEY key;
    if (RegOpenKeyExA(root, path, 0, KEY_READ | extra, &key)) return 0;
    RegCloseKey(key);
    return 1;
}

int main(int argc, char **argv)
{
    static const char *kids[] = { "CLSID", "DirectShow", "Interface", "Media Type", "MediaFoundation" };
    char path[128];
    int i;

    if (argc < 2) return 2;
    if (!strcmp(argv[1], "w32"))
    {
        put(HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\" G "\\InprocServer32", "x86", 0);
        put(HKEY_CURRENT_USER, "Software\\Classes\\.sgwowshared", "shared32", 0);
    }
    else if (!strcmp(argv[1], "w64"))
    {
        put(HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\" G "\\InprocServer32", "x64", 0);
        put(HKEY_CURRENT_USER, "Software\\Classes\\Interface\\" G, "if64", 0);
        put(HKEY_CURRENT_USER, "Software\\Classes\\Interface\\" G, "if32", KEY_WOW64_32KEY);
    }
    else if (!strcmp(argv[1], "r64"))
    {
        get("CLSID64", HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\" G "\\InprocServer32", 0);
        get("CLSID32KEY", HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\" G "\\InprocServer32", KEY_WOW64_32KEY);
        get("NODE", HKEY_CURRENT_USER, "Software\\Classes\\Wow6432Node\\CLSID\\" G "\\InprocServer32", 0);
        get("HKCR64", HKEY_CLASSES_ROOT, "CLSID\\" G "\\InprocServer32", 0);
        get("SHARED", HKEY_CURRENT_USER, "Software\\Classes\\.sgwowshared", 0);
        get("IF64", HKEY_CURRENT_USER, "Software\\Classes\\Interface\\" G, 0);
        get("IF32", HKEY_CURRENT_USER, "Software\\Classes\\Wow6432Node\\Interface\\" G, 0);
        get("OLD32", HKEY_CURRENT_USER, "Software\\Classes\\Wow6432Node\\CLSID\\" OLD "\\InprocServer32", 0);
        get("OLD64", HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\" OLD "\\InprocServer32", 0);
        printf("KIDS ");
        for (i = 0; i < 5; i++)
        {
            snprintf(path, sizeof(path), "Software\\Classes\\Wow6432Node\\%s", kids[i]);
            printf("%d", has(HKEY_CURRENT_USER, path, 0));
        }
        printf(" %d\n", has(HKEY_CURRENT_USER, "Software\\Classes\\Wow6432Node\\.sgwowshared", 0));
    }
    else if (!strcmp(argv[1], "r32"))
    {
        get("CLSID", HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\" G "\\InprocServer32", 0);
        get("HKCR", HKEY_CLASSES_ROOT, "CLSID\\" G "\\InprocServer32", 0);
        get("SHARED", HKEY_CURRENT_USER, "Software\\Classes\\.sgwowshared", 0);
        get("SHAREDCR", HKEY_CLASSES_ROOT, ".sgwowshared", 0);
        get("IF", HKEY_CURRENT_USER, "Software\\Classes\\Interface\\" G, 0);
        get("OLD", HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\" OLD "\\InprocServer32", 0);
    }
    return 0;
}
