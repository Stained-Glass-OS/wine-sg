/* metricsfont-probe: the face names SystemParametersInfo gives for the
 * interface fonts */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    LOGFONTW icon;

    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) return 1;
    printf("menu %ls\ncaption %ls\n", ncm.lfMenuFont.lfFaceName, ncm.lfCaptionFont.lfFaceName);
    if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(icon), &icon, 0)) printf("icon %ls\n", icon.lfFaceName);
    return 0;
}
