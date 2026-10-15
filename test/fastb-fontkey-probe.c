#include <windows.h>
#include <stdio.h>
int main(void)
{
    HDC dc = GetDC(0);
    char face[64] = "";
    HKEY key;
    char val[64] = "";
    DWORD size = sizeof(val);
    int ok_key = 0;

    SelectObject(dc, GetStockObject(SYSTEM_FONT));
    GetTextFaceA(dc, sizeof(face), face);
    if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Hardware Profiles\\Current\\Software\\Fonts",
                       0, KEY_READ, &key))
    {
        ok_key = !RegQueryValueExA(key, "FONTS.FON", NULL, NULL, (BYTE *)val, &size);
        RegCloseKey(key);
    }
    printf("fontlist=%d\n", ok_key);
    printf("system_font=%s\n", face);
    return 0;
}
