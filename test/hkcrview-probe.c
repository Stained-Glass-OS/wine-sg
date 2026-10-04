/* hkcrview-probe -- 32-bit: a class registered in the 64-bit view through
 * HKEY_CLASSES_ROOT, as a 32-bit installer of a 64-bit program does (7-Zip's
 * x64 setup): the key created with KEY_WOW64_64KEY, its value set through the
 * handle, and a subkey created below it (no flag: it inherits the view)
 * with its own value (test/hkcrview-gate.sh, patches/sg/0780). */
#include <windows.h>
#include <stdio.h>
static void reg(REGSAM wow, const WCHAR *name, const WCHAR *dll, LONG *r)
{
    HKEY key, sub;
    const WCHAR *path = L"CLSID\\{6A2B9C30-8F1E-4D7A-B3C5-0E9F11A2D7E4}";
    r[0] = RegCreateKeyExW(HKEY_CLASSES_ROOT, path, 0, NULL, 0, KEY_ALL_ACCESS | wow, NULL, &key, NULL);
    r[1] = RegSetValueExW(key, NULL, 0, REG_SZ, (const BYTE *)name, (lstrlenW(name) + 1) * sizeof(WCHAR));
    r[2] = RegCreateKeyExW(key, L"InprocServer32", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sub, NULL);
    r[3] = RegSetValueExW(sub, NULL, 0, REG_SZ, (const BYTE *)dll, (lstrlenW(dll) + 1) * sizeof(WCHAR));
    RegCloseKey(sub);
    RegCloseKey(key);
}

int main(void)
{
    LONG a[4], b[4];
    /* as 7-Zip's setup: the 32-bit registration first, then the 64-bit one */
    reg(0, L"SG view test 32", L"C:\\sgview32.dll", a);
    reg(KEY_WOW64_64KEY, L"SG view test", L"C:\\sgview64.dll", b);
    printf("32: %ld %ld %ld %ld 64: %ld %ld %ld %ld\n", a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
    return 0;
}
