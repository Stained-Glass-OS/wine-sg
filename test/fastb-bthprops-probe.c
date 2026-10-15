/* bthprops.cpl (patch 3587): the entry points bluetoothapis implements are imported, not stubs. A stub raises an
 * "unimplemented function" exception, so merely returning proves it. */
#include <windows.h>
#include <stdio.h>

typedef ULONG_PTR (WINAPI *fn)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);

int main(void)
{
    static const struct { const char *name; int nargs; } tests[] = {
        { "BluetoothIsConnectable", 1 }, { "BluetoothIsDiscoverable", 1 }, { "BluetoothIsVersionAvailable", 2 },
        { "BluetoothEnableDiscovery", 2 }, { "BluetoothRemoveDevice", 1 }, { "BluetoothUpdateDeviceRecord", 1 },
        { "BluetoothGetDeviceInfo", 2 }, { "BluetoothSetServiceState", 4 }, { "BluetoothEnumerateInstalledServices", 4 },
    };
    HMODULE bthprops = LoadLibraryA("bthprops.cpl"), api = LoadLibraryA("bluetoothapis.dll");
    int i, fails = 0;

    if (!bthprops || !api) { printf("FAIL  load\nRESULT: FAIL\n"); return 1; }
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        fn a = (fn)GetProcAddress(bthprops, tests[i].name), b = (fn)GetProcAddress(api, tests[i].name);
        ULONG_PTR ra, rb;

        if (!a || !b) { printf("FAIL  %s export missing\n", tests[i].name); fails++; continue; }
        SetLastError(0); ra = a(0, 0, 0, 0); DWORD ea = GetLastError();
        SetLastError(0); rb = b(0, 0, 0, 0); DWORD eb = GetLastError();
        printf("%s  %s: %#Ix/%lu vs %#Ix/%lu\n", ra == rb && ea == eb ? "PASS" : "FAIL", tests[i].name, ra, ea, rb, eb);
        if (ra != rb || ea != eb) fails++;
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
