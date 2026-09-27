/* battery-probe: GetSystemPowerStatus as a program sees it (patches/sg/0445).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
int main(void)
{
    SYSTEM_POWER_STATUS ps;
    if (!GetSystemPowerStatus(&ps)) { printf("STATUS failed\n"); return 1; }
    printf("AC %u FLAG %u PERCENT %u LIFE %ld\n", ps.ACLineStatus, ps.BatteryFlag, ps.BatteryLifePercent,
           (long)(int)ps.BatteryLifeTime);
    return 0;
}
