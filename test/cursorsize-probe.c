/* cursorsize-probe: SystemParametersInfo(SPI_GETCURSORSIZE = 0x2028), for test/cursorsize-gate.sh
 * (Stained Glass OS wine-sg 9811). Prints "SIZE ok value lasterror". */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    DWORD size = 0xdead;
    BOOL ok;
    SetLastError(0);
    ok = SystemParametersInfoW(0x2028, 0, &size, 0);
    printf("SIZE %d %lu %lu\n", ok, size, ok ? 0 : GetLastError());
    return 0;
}
